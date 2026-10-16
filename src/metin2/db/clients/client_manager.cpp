
#include <metin2/db/clients/client_manager.h>

#include <metin2/db/app/main.h>
#include <metin2/db/caches/cache.h>
#include <metin2/db/clients/peer.h>
#include <metin2/db/empire/monarch.h>
#include <metin2/db/empire/priv_manager.h>
#include <metin2/db/guilds/guild_manager.h>
#include <metin2/db/items/item_award_manager.h>
#include <metin2/db/items/item_id_range_manager.h>
#include <metin2/db/login/login_data.h>
#include <metin2/db/logs/money_log.h>
#include <metin2/db/social/marriage.h>
#include <metin2/db/storage/db_manager.h>

#include <metin2/common/grid.h>
#include <metin2/common/peer_security.h>
#include <metin2/common/utils.h>
#include <metin2/config/config.h>
#include <metin2/core/main.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/net/connection.h>
#include <metin2/net/io.h>
#include <metin2/net/listener.h>
#include <metin2/proto/building.h>
#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <initializer_list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <strings.h>

namespace metin2::db::inline clients {

    namespace {
        struct payload_part {
            const void *data;
            size_t size;
        };

        void broadcast_to_game_peers(const client_manager::peer_list &peers, uint8_t header, const peer *requester,
            uint32_t requester_handle, uint32_t others_handle, std::initializer_list<payload_part> payload) {
            uint32_t size = 0;
            for (const auto &part : payload)
                size += part.size;

            for (peer *p : peers) {
                if (!p->channel())
                    continue;

                p->encode_header(header, p == requester ? requester_handle : others_handle, size);
                for (const auto &part : payload)
                    p->encode(part.data, part.size);
            }
        }
    }

    uint32_t usage_max = 0;
    uint32_t usage_avg = 0;

    int item_count = 0;
    int query_count[2];

    client_manager::client_manager() : auth_peer_(nullptr),
                                       player_id_start_(0),
                                       player_delete_level_limit_(0),
                                       player_delete_level_limit_lower_(0),
                                       shop_table_size_(0),
                                       shop_table_(nullptr),
                                       refine_table_size_(0),
                                       refine_table_(nullptr),
                                       shutdowned_(false),
                                       cache_flush_count_(0),
                                       cache_flush_count_limit_(200) {
        item_range_.min = 0;
        item_range_.max = 0;
        item_range_.usable_item_id_min = 0;

        memset(query_count, 0, sizeof(query_count));
    }

    client_manager::~client_manager() {
        destroy();
    }

    void client_manager::set_player_id_start(int id_start) {
        player_id_start_ = id_start;
    }

    void client_manager::destroy() {
        m_channel_status_.clear();
        for (peer *peer : peer_list_)
            delete peer;

        peer_list_.clear();

        if (listener_) {
            listener_->close();
            listener_.reset();
        }
    }

    auto client_manager::initialize() -> bool {

        if (!initialize_localization()) {
            metin2::log::error("Cannot load the localization settings, exiting");
            return false;
        }

        if (!initialize_now_item_id()) {
            metin2::log::error("Cannot initialize the item id range, exiting");
            return false;
        }

        if (!initialize_tables()) {
            metin2::log::error("Cannot load game tables");
            return false;
        }

        guild_manager::instance().boot_reserve_war();

        const metin2::config::db_settings &db = metin2::config::db();

        listener_ = net::listener::open(db.bind_ip, db.port, [this](net::tcp::socket socket) { add_peer(std::move(socket)); });

        if (!listener_)
            return false;

        metin2::log::info("Listening for game peers on {}:{:d}", db.bind_ip, db.port);

        player_delete_level_limit_ = db.player_delete_level_limit > 0 ? db.player_delete_level_limit : PLAYER_MAX_LEVEL_CONST + 1;
        player_delete_level_limit_lower_ = db.player_delete_level_limit_lower;

        metin2::log::info("Player deletion level limit {:d}", player_delete_level_limit_);
        metin2::log::info("Player deletion minimum level {:d}", player_delete_level_limit_lower_);

        load_event_flag();

        return true;
    }

    void client_manager::main_loop() {
        while (!shutdowned_) {
            while (auto msg = db_manager::instance().pop_result()) {
                if (msg->on_result)
                    msg->on_result(*msg);
            }

            if (!process())
                break;

            metin2::log::rotate();
        }

        metin2::log::info("Main loop exited, flushing caches");

        signal_timer_disable();

        for (auto &[pid, cache] : map_player_cache_)
            cache->flush();
        map_player_cache_.clear();

        for (auto &[id, cache] : map_item_cache_)
            cache->flush();
        map_item_cache_.clear();

        for (auto &[owner, cache] : item_price_list_cache_)
            cache->flush();
        item_price_list_cache_.clear();
    }

    void client_manager::query_boot(peer *peer, packet_gd_boot *p) {
        const uint8_t bPacketVersion = 6; // Bump whenever the BOOT packet layout changes.

        std::vector<t_admin_info> vAdmin;
        std::vector<std::string> vHost;

        host_info(vHost);
        admin_info(p->ip, vAdmin);

        metin2::log::info("Boot: sending gm list to game server {}", p->ip);

        uint32_t dwPacketSize =
            sizeof(uint32_t) +
            sizeof(uint8_t) +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(mob_table) * vec_mob_table_.size() +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(item_table) * vec_item_table_.size() +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(shop_table) * shop_table_size_ +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(skill_table) * vec_skill_table_.size() +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(refine_table) * refine_table_size_ +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(item_attr_table) * vec_item_attr_table_.size() +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(item_attr_table) * vec_item_rare_table_.size() +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(banword_table) * vec_banword_table_.size() +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(building::land_table) * vec_land_table_.size() +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(building::object_proto) * vec_object_proto_.size() +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(building::object_table) * map_object_table_.size() +
            sizeof(int32_t) +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(item_id_range_table) * 2 +
            sizeof(uint16_t) + sizeof(uint16_t) + 16 * vHost.size() +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(t_admin_info) * vAdmin.size() +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(monarch_info) +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(monarch_candidacy) * monarch::instance().monarch_candidacy_size() +
            sizeof(uint16_t);

        peer->encode_header(HEADER_DG_BOOT, 0, dwPacketSize);
        peer->encode(&dwPacketSize, sizeof(uint32_t));
        peer->encode(&bPacketVersion, sizeof(uint8_t));

        metin2::log::debug("Boot: packet size {:d}", dwPacketSize);
        metin2::log::debug("Boot: packet version {:d}", bPacketVersion);

        metin2::log::debug("Boot: mob table entry size {:d}", sizeof(mob_table));
        metin2::log::debug("Boot: item table entry size {:d}", sizeof(item_table));
        metin2::log::debug("Boot: shop table entry size {:d}", sizeof(shop_table));
        metin2::log::debug("Boot: skill table entry size {:d}", sizeof(skill_table));
        metin2::log::debug("Boot: refine table entry size {:d}", sizeof(refine_table));
        metin2::log::debug("Boot: item attribute table entry size {:d}", sizeof(item_attr_table));
        metin2::log::debug("Boot: rare item attribute table entry size {:d}", sizeof(item_attr_table));
        metin2::log::debug("Boot: banword table entry size {:d}", sizeof(banword_table));
        metin2::log::debug("Boot: land entry size {:d}", sizeof(building::land_table));
        metin2::log::debug("Boot: object proto entry size {:d}", sizeof(building::object_proto));
        metin2::log::debug("Boot: object entry size {:d}", sizeof(building::object_table));
        metin2::log::debug("Boot: gm list size {:d}", sizeof(t_admin_info) * vAdmin.size());
        metin2::log::debug("Boot: monarch info size {:d}", sizeof(monarch_info));

        peer->encode_word(sizeof(mob_table));
        peer->encode_word(vec_mob_table_.size());
        peer->encode(vec_mob_table_.data(), sizeof(mob_table) * vec_mob_table_.size());

        peer->encode_word(sizeof(item_table));
        peer->encode_word(vec_item_table_.size());
        peer->encode(vec_item_table_.data(), sizeof(item_table) * vec_item_table_.size());

        peer->encode_word(sizeof(shop_table));
        peer->encode_word(shop_table_size_);
        peer->encode(shop_table_.get(), sizeof(shop_table) * shop_table_size_);

        peer->encode_word(sizeof(skill_table));
        peer->encode_word(vec_skill_table_.size());
        peer->encode(vec_skill_table_.data(), sizeof(skill_table) * vec_skill_table_.size());

        peer->encode_word(sizeof(refine_table));
        peer->encode_word(refine_table_size_);
        peer->encode(refine_table_.get(), sizeof(refine_table) * refine_table_size_);

        peer->encode_word(sizeof(item_attr_table));
        peer->encode_word(vec_item_attr_table_.size());
        peer->encode(vec_item_attr_table_.data(), sizeof(item_attr_table) * vec_item_attr_table_.size());

        peer->encode_word(sizeof(item_attr_table));
        peer->encode_word(vec_item_rare_table_.size());
        peer->encode(vec_item_rare_table_.data(), sizeof(item_attr_table) * vec_item_rare_table_.size());

        peer->encode_word(sizeof(banword_table));
        peer->encode_word(vec_banword_table_.size());
        peer->encode(vec_banword_table_.data(), sizeof(banword_table) * vec_banword_table_.size());

        peer->encode_word(sizeof(building::land_table));
        peer->encode_word(vec_land_table_.size());
        peer->encode(vec_land_table_.data(), sizeof(building::land_table) * vec_land_table_.size());

        peer->encode_word(sizeof(building::object_proto));
        peer->encode_word(vec_object_proto_.size());
        peer->encode(vec_object_proto_.data(), sizeof(building::object_proto) * vec_object_proto_.size());

        peer->encode_word(sizeof(building::object_table));
        peer->encode_word(map_object_table_.size());

        auto it = map_object_table_.begin();

        while (it != map_object_table_.end())
            peer->encode((it++)->second.get(), sizeof(building::object_table));

        int32_t now = time(nullptr);
        peer->encode(&now, sizeof(int32_t));

        item_id_range_table itemRange = item_id_range_manager::instance().range();
        item_id_range_table itemRangeSpare = item_id_range_manager::instance().range();

        peer->encode_word(sizeof(item_id_range_table));
        peer->encode_word(1);
        peer->encode(&itemRange, sizeof(item_id_range_table));
        peer->encode(&itemRangeSpare, sizeof(item_id_range_table));

        peer->set_item_id_range(itemRange);
        peer->set_spare_item_id_range(itemRangeSpare);

        peer->encode_word(16);
        peer->encode_word(vHost.size());

        for (size_t n = 0; n < vHost.size(); ++n) {
            peer->encode(vHost[n].c_str(), 16);
            metin2::log::debug("Boot: gm host {}", vHost[n]);
        }

        peer->encode_word(sizeof(t_admin_info));
        peer->encode_word(vAdmin.size());

        for (size_t n = 0; n < vAdmin.size(); ++n) {
            peer->encode(&vAdmin[n], sizeof(t_admin_info));
            metin2::log::debug("Boot: gm {} (contact ip {})", vAdmin[n].name, vAdmin[n].contact_ip);
        }

        peer->encode_word(sizeof(monarch_info));
        peer->encode_word(1);
        peer->encode(monarch::instance().get_monarch(), sizeof(monarch_info));

        monarch::vec_monarchcandidacy &rVecMonarchCandidacy = monarch::instance().vec_monarch_candidacy();

        size_t num_monarch_candidacy = monarch::instance().monarch_candidacy_size();
        peer->encode_word(sizeof(monarch_candidacy));
        peer->encode_word(num_monarch_candidacy);
        if (num_monarch_candidacy != 0) {
            peer->encode(rVecMonarchCandidacy.data(), sizeof(monarch_candidacy) * num_monarch_candidacy);
        }

        if (metin2::config::test_server())
            metin2::log::debug("Boot: {:d} monarch candidates", monarch::instance().monarch_candidacy_size());

        peer->encode_word(0xffff);
    }

    void client_manager::send_party_on_setup(peer *peer_value) {
        party_map &pm = map_channel_party_[peer_value->channel()];

        for (auto it_party = pm.begin(); it_party != pm.end(); ++it_party) {
            metin2::log::debug("Party: sending party of leader pid {:d} to new peer", it_party->first);
            peer_value->encode_header(HEADER_DG_PARTY_CREATE, 0, sizeof(packet_party_create));
            peer_value->encode(&it_party->first, sizeof(uint32_t));

            for (auto it_member = it_party->second.begin(); it_member != it_party->second.end(); ++it_member) {
                metin2::log::debug("Party: sending party of leader pid {:d}, member pid {:d}", it_party->first, it_member->first);
                peer_value->encode_header(HEADER_DG_PARTY_ADD, 0, sizeof(packet_party_add));
                peer_value->encode(&it_party->first, sizeof(uint32_t));
                peer_value->encode(&it_member->first, sizeof(uint32_t));
                peer_value->encode(&it_member->second.role, sizeof(uint8_t));

                peer_value->encode_header(HEADER_DG_PARTY_SET_MEMBER_LEVEL, 0, sizeof(packet_party_set_member_level));
                peer_value->encode(&it_party->first, sizeof(uint32_t));
                peer_value->encode(&it_member->first, sizeof(uint32_t));
                peer_value->encode(&it_member->second.level, sizeof(uint8_t));
            }
        }
    }

    void client_manager::query_quest_save(peer *peer_value, quest_table *table, uint32_t len) {
        if (0 != (len % sizeof(quest_table))) {
            metin2::log::error("Quest save packet size {:d} is not a multiple of the quest entry size {:d}", len, sizeof(quest_table));
            return;
        }

        int iSize = len / sizeof(quest_table);

        std::string query;

        for (int i = 0; i < iSize; ++i, ++table) {
            if (table->value == 0) {
                query = sql::format(
                    "DELETE FROM quest{} WHERE dwPID={} AND szName={} AND szState={}",
                    sql::raw(get_table_postfix()), table->pid, table->name, table->state);
            } else {
                query = sql::format(
                    "REPLACE INTO quest{} (dwPID, szName, szState, lValue) VALUES({}, {}, {}, {})",
                    sql::raw(get_table_postfix()), table->pid, table->name, table->state, table->value);
            }

            db_manager::instance().return_query(query, [](sql_msg &) {});
        }
    }

    void client_manager::query_safebox_load(peer *peer_value, uint32_t handle_value, safebox_load_packet *packet, bool mall) {
        auto pi = std::make_unique<client_handle_info>(handle_value);
        strlcpy(pi->safebox_password, packet->password, sizeof(pi->safebox_password));
        pi->account_id = packet->id;
        pi->account_index = 0;
        pi->ip[0] = mall ? 1 : 0;
        strlcpy(pi->login, packet->login, sizeof(pi->login));

        const std::string query = sql::format(
            "SELECT account_id, size, password FROM safebox{} WHERE account_id={}",
            sql::raw(get_table_postfix()), packet->id);

        if (metin2::config::db().login_log)
            metin2::log::debug("Safebox: load request, handle {:d}, account {:d}, mall {:d}", handle_value, packet->id, mall ? 1 : 0);

        db_manager::instance().return_query(query, [this, handle = peer_value->handle(), pi = std::move(pi)](sql_msg &msg) mutable {
            if (auto *peer_value = get_peer(handle))
                result_safebox_load(peer_value, msg, std::move(pi));
        });
    }

    void client_manager::result_safebox_load(peer *peer_value, sql_msg &msg, std::unique_ptr<client_handle_info> pi) {
        metin2::log::debug("Safebox load query finished");

        uint32_t dwHandle = pi->handle;

        // account_index carries the query stage here: 0 checks the password, 1 loads the items.

        if (pi->account_index == 0) {
            std::string strSafeboxPassword = pi->safebox_password;

            auto pSafebox = std::make_unique<safebox_table>();
            memset(pSafebox.get(), 0, sizeof(safebox_table));

            sql_result *res = msg.get();

            if (res->num_rows == 0) {
                if (strSafeboxPassword != "000000") {
                    peer_value->encode_header(HEADER_DG_SAFEBOX_WRONG_PASSWORD, dwHandle, 0);
                    return;
                }
            } else {
                MYSQL_ROW row = mysql_fetch_row(res->res.get());

                if (((!row[2] || !*row[2]) && strSafeboxPassword != "000000") ||
                    ((row[2] && *row[2]) && strSafeboxPassword != row[2])) {
                    peer_value->encode_header(HEADER_DG_SAFEBOX_WRONG_PASSWORD, dwHandle, 0);
                    return;
                }

                if (!row[0])
                    pSafebox->id = 0;
                else
                    str_to_number(pSafebox->id, row[0]);

                if (!row[1])
                    pSafebox->size = 0;
                else
                    str_to_number(pSafebox->size, row[1]);
                if (pi->ip[0] == 1) {
                    pSafebox->size = 1;
                    metin2::log::debug("Safebox: loaded mall of account {:d}, size {:d}", pSafebox->id, pSafebox->size);
                } else
                    metin2::log::debug("Safebox: loaded safebox of account {:d}, size {:d}", pSafebox->id, pSafebox->size);
            }

            if (0 == pSafebox->id)
                pSafebox->id = pi->account_id;

            pi->safebox = std::move(pSafebox);

            const std::string query = sql::format("SELECT {} FROM item{} WHERE owner_id={} AND window={}",
                sql::raw(item_select_columns), sql::raw(get_table_postfix()), pi->account_id, pi->ip[0] == 0 ? "SAFEBOX" : "MALL");

            pi->account_index = 1;

            db_manager::instance().return_query(query, [this, handle = peer_value->handle(), pi = std::move(pi)](sql_msg &msg) mutable {
                if (auto *peer_value = get_peer(handle))
                    result_safebox_load(peer_value, msg, std::move(pi));
            });
        } else {

            if (!pi->safebox) {
                metin2::log::error("Safebox: load result has no safebox");
                return;
            }

            // On a query error the safebox stays closed; answering would show it empty.
            if (!msg.get()->res.get()) {
                metin2::log::error("Safebox: item query failed, safebox not opened");
                return;
            }

            std::vector<player_item> s_items;
            create_item_table_from_res(msg.get()->res.get(), &s_items, pi->account_id);

            std::set<db::items::item_award *> *pSet = item_award_manager::instance().by_login(pi->login);

            if (pSet && !vec_item_table_.empty()) {

                grid grid(5, std::max<int>(1, pi->safebox->size) * 9);
                bool bEscape = false;

                for (uint32_t i = 0; i < s_items.size(); ++i) {
                    player_item &r = s_items[i];

                    auto it = map_item_table_by_vnum_.find(r.vnum);

                    if (it == map_item_table_by_vnum_.end()) {
                        bEscape = true;
                        metin2::log::error("Safebox: unknown item vnum {:d} for account {}", r.vnum, pi->login);
                        break;
                    }

                    grid.put(r.pos, 1, it->second->size);
                }

                if (!bEscape) {
                    std::vector<std::pair<uint32_t, uint32_t>> vec_dwFinishedAwardID;

                    __typeof(pSet->begin()) it = pSet->begin();

                    std::string query;

                    while (it != pSet->end()) {
                        db::items::item_award *pItemAward = *(it++);
                        const uint32_t &dwItemVnum = pItemAward->vnum;

                        if (pItemAward->taken)
                            continue;

                        if (pi->ip[0] == 0 && pItemAward->mall)
                            continue;

                        if (pi->ip[0] == 1 && !pItemAward->mall)
                            continue;

                        auto it = map_item_table_by_vnum_.find(pItemAward->vnum);

                        if (it == map_item_table_by_vnum_.end()) {
                            metin2::log::error("Item award: unknown item vnum {:d} for account {}", pItemAward->vnum, pi->login);
                            continue;
                        }

                        item_table *pItemTable = it->second;

                        int iPos;

                        if ((iPos = grid.find_blank(1, it->second->size)) == -1)
                            break;

                        player_item item;
                        memset(&item, 0, sizeof(player_item));

                        uint32_t dwSocket2 = pItemAward->socket2;

                        if (pItemTable->type == ITEM_UNIQUE) {
                            if (pItemAward->socket2 != 0)
                                dwSocket2 = pItemAward->socket2;
                            else
                                dwSocket2 = pItemTable->values[0];
                        } else if ((dwItemVnum == 50300 || dwItemVnum == 70037) && pItemAward->socket0 == 0) {
                            std::vector<uint32_t> skill_vnums;
                            for (const skill_table &skill : vec_skill_table_) {
                                if (skill.vnum <= 120)
                                    skill_vnums.push_back(skill.vnum);
                            }

                            if (!skill_vnums.empty())
                                pItemAward->socket0 = skill_vnums[number(0, static_cast<int>(skill_vnums.size()) - 1)];
                        } else {
                            switch (dwItemVnum) {
                            case 72723:
                            case 72724:
                            case 72725:
                            case 72726:
                            case 72727:
                            case 72728:
                            case 72729:
                            case 72730:
                            case 76004:
                            case 76005:
                            case 76021:
                            case 76022:
                            case 79012:
                            case 79013:
                                if (pItemAward->socket2 == 0) {
                                    dwSocket2 = pItemTable->values[0];
                                } else {
                                    dwSocket2 = pItemAward->socket2;
                                }
                                break;
                            }
                        }

                        if (item_id() > item_range_.max) {
                            metin2::log::error("Item award: item id range exhausted");
                            break;
                        }

                        {
                            auto it = map_item_table_by_vnum_.find(dwItemVnum);
                            if (it == map_item_table_by_vnum_.end()) {
                                metin2::log::error("Item award: unknown item vnum {:d}", dwItemVnum);
                                continue;
                            }
                            item_table *item_table = it->second;
                            if (item_table == nullptr) {
                                metin2::log::error("Item award: item vnum {:d} has no item table entry", dwItemVnum);
                                continue;
                            }
                            if (0 == pItemAward->socket0) {
                                for (int i = 0; i < ITEM_LIMIT_MAX_NUM; i++) {
                                    if (LIMIT_REAL_TIME == item_table->limits[i].type) {
                                        if (0 == item_table->limits[i].value)
                                            pItemAward->socket0 = time(nullptr) + 60 * 60 * 24 * 7;
                                        else
                                            pItemAward->socket0 = time(nullptr) + item_table->limits[i].value;

                                        break;
                                    } else if (LIMIT_REAL_TIME_START_FIRST_USE == item_table->limits[i].type || LIMIT_TIMER_BASED_ON_WEAR == item_table->limits[i].type) {
                                        if (0 == item_table->limits[i].value)
                                            pItemAward->socket0 = 60 * 60 * 24 * 7;
                                        else
                                            pItemAward->socket0 = item_table->limits[i].value;

                                        break;
                                    }
                                }
                            }

                            query = sql::format(
                                "INSERT INTO item{} (id, owner_id, window, pos, vnum, count, socket0, socket1, socket2) "
                                "VALUES({}, {}, {}, {}, {}, {}, {}, {}, {})",
                                sql::raw(get_table_postfix()),
                                gain_item_id(),
                                pi->account_id,
                                pi->ip[0] == 0 ? "SAFEBOX" : "MALL",
                                iPos,
                                pItemAward->vnum, pItemAward->count, pItemAward->socket0, pItemAward->socket1, dwSocket2);
                        }

                        auto pmsg = db_manager::instance().direct_query(query);
                        sql_result *pRes = pmsg->get();
                        metin2::log::debug("Item award: {}", query);

                        if (pRes->affected_rows == 0 || pRes->insert_id == 0 || pRes->affected_rows == (uint32_t)-1)
                            break;

                        item.id = pmsg->get()->insert_id;
                        item.window = pi->ip[0] == 0 ? SAFEBOX : MALL,
                        item.pos = iPos;
                        item.count = pItemAward->count;
                        item.vnum = pItemAward->vnum;
                        item.sockets[0] = pItemAward->socket0;
                        item.sockets[1] = pItemAward->socket1;
                        item.sockets[2] = dwSocket2;
                        s_items.push_back(item);

                        vec_dwFinishedAwardID.push_back(std::make_pair(pItemAward->id, item.id));
                        grid.put(iPos, 1, it->second->size);
                    }

                    for (uint32_t i = 0; i < vec_dwFinishedAwardID.size(); ++i)
                        item_award_manager::instance().taken(vec_dwFinishedAwardID[i].first, vec_dwFinishedAwardID[i].second);
                }
            }

            pi->safebox->item_count = s_items.size();

            peer_value->encode_header(pi->ip[0] == 0 ? HEADER_DG_SAFEBOX_LOAD : HEADER_DG_MALL_LOAD, dwHandle, sizeof(safebox_table) + sizeof(player_item) * s_items.size());

            peer_value->encode(pi->safebox.get(), sizeof(safebox_table));

            if (!s_items.empty())
                peer_value->encode(s_items.data(), sizeof(player_item) * s_items.size());
        }
    }

    void client_manager::query_safebox_change_size(peer *peer_value, uint32_t handle_value, safebox_change_size_packet *p) {
        auto pi = std::make_unique<client_handle_info>(handle_value);
        pi->account_index = p->size; // account_index carries the safebox size here.

        const std::string query = p->size == 1
                                      ? sql::format("INSERT INTO safebox{} (account_id, size) VALUES({}, {})", sql::raw(get_table_postfix()), p->id, p->size)
                                      : sql::format("UPDATE safebox{} SET size={} WHERE account_id={}", sql::raw(get_table_postfix()), p->size, p->id);

        db_manager::instance().return_query(query, [this, handle = peer_value->handle(), pi = std::move(pi)](sql_msg &msg) {
            if (auto *peer_value = get_peer(handle))
                result_safebox_change_size(peer_value, msg, *pi);
        });
    }

    void client_manager::result_safebox_change_size(peer *peer_value, sql_msg &msg, client_handle_info &p) {
        metin2::log::debug("Safebox resize query finished");

        uint32_t dwHandle = p.handle;
        uint8_t bSize = p.account_index;

        if (msg.get()->num_rows > 0) {
            peer_value->encode_header(HEADER_DG_SAFEBOX_CHANGE_SIZE, dwHandle, sizeof(uint8_t));
            peer_value->encode_byte(bSize);
        }
    }

    void client_manager::query_safebox_change_password(peer *peer_value, uint32_t handle_value, safebox_change_password_packet *p) {
        auto pi = std::make_unique<client_handle_info>(handle_value);
        strlcpy(pi->safebox_password, p->new_password, sizeof(pi->safebox_password));
        strlcpy(pi->login, p->old_password, sizeof(pi->login));
        pi->account_id = p->id;

        const std::string query = sql::format("SELECT password FROM safebox{} WHERE account_id={}", sql::raw(get_table_postfix()), p->id);

        db_manager::instance().return_query(query, [this, handle = peer_value->handle(), pi = std::move(pi)](sql_msg &msg) mutable {
            if (auto *peer_value = get_peer(handle))
                result_safebox_change_password(peer_value, msg, std::move(pi));
        });
    }

    void client_manager::result_safebox_change_password(peer *peer_value, sql_msg &msg, std::unique_ptr<client_handle_info> p) {
        metin2::log::debug("Safebox password change query finished");

        uint32_t dwHandle = p->handle;

        if (msg.get()->num_rows > 0) {
            MYSQL_ROW row = mysql_fetch_row(msg.get()->res.get());

            if ((row[0] && *row[0] && !strcasecmp(row[0], p->login)) || ((!row[0] || !*row[0]) && !strcmp("000000", p->login))) {
                const std::string query = sql::format("UPDATE safebox{} SET password={} WHERE account_id={}", sql::raw(get_table_postfix()), p->safebox_password, p->account_id);

                db_manager::instance().return_query(query, [this, handle = peer_value->handle(), p = std::move(p)](sql_msg &msg) {
                    if (auto *peer_value = get_peer(handle))
                        result_safebox_change_password_second(peer_value, msg, *p);
                });
                return;
            }
        }

        peer_value->encode_header(HEADER_DG_SAFEBOX_CHANGE_PASSWORD_ANSWER, dwHandle, sizeof(uint8_t));
        peer_value->encode_byte(0);
    }

    void client_manager::result_safebox_change_password_second(peer *peer_value, sql_msg &msg, client_handle_info &p) {
        metin2::log::debug("Safebox password change query (second step) finished");

        uint32_t dwHandle = p.handle;

        peer_value->encode_header(HEADER_DG_SAFEBOX_CHANGE_PASSWORD_ANSWER, dwHandle, sizeof(uint8_t));
        peer_value->encode_byte(1);
    }

    void client_manager::result_pricelist_load(peer *peer, sql_msg &msg, const item_pricelist_req_info &req_info) {
        item_price_list_table table;
        table.owner_id = req_info.second;
        table.count = 0;

        MYSQL_ROW row;

        while (table.count < SHOP_PRICELIST_MAX_NUM && (row = mysql_fetch_row(msg.get()->res.get()))) {
            str_to_number(table.price_info[table.count].vnum, row[0]);
            str_to_number(table.price_info[table.count].price, row[1]);
            table.count++;
        }

        put_item_price_list_cache(&table);

        packet_myshop_pricelist_header header;

        header.owner_id = req_info.second;
        header.count = table.count;

        size_t sizePriceListSize = sizeof(item_price_info) * header.count;

        peer->encode_header(HEADER_DG_MYSHOP_PRICELIST_RES, req_info.first, sizeof(header) + sizePriceListSize);
        peer->encode(&header, sizeof(header));
        peer->encode(table.price_info, sizePriceListSize);

        metin2::log::debug("Price list: loaded for handle {:d}, pid {:d}, {:d} items", req_info.first, req_info.second, header.count);
    }

    void client_manager::result_pricelist_load_for_update(sql_msg &msg, item_price_list_table &update_table) {
        item_price_list_table table;
        table.owner_id = update_table.owner_id;
        table.count = 0;

        MYSQL_ROW row;

        while (table.count < SHOP_PRICELIST_MAX_NUM && (row = mysql_fetch_row(msg.get()->res.get()))) {
            str_to_number(table.price_info[table.count].vnum, row[0]);
            str_to_number(table.price_info[table.count].price, row[1]);
            table.count++;
        }

        put_item_price_list_cache(&table);

        item_price_list_cache(update_table.owner_id)->update_list(&update_table);
    }

    void client_manager::query_safebox_save(peer *peer_value, safebox_table *table) {
        const std::string query = sql::format(
            "UPDATE safebox{} SET gold='{}' WHERE account_id={}",
            sql::raw(get_table_postfix()), table->gold, table->id);

        db_manager::instance().return_query(query, [](sql_msg &) {});
    }

    void client_manager::query_empire_select(peer *peer_value, uint32_t handle, empire_select_packet *p) {
        if (p->empire == 0 || p->empire >= EMPIRE_MAX_NUM) {
            metin2::log::warn("Empire select: invalid empire {:d} for account {:d}", p->empire, p->account_id);
            return;
        }

        const std::string query = sql::format("UPDATE player_index{} SET empire={} WHERE id={}", sql::raw(get_table_postfix()), p->empire, p->account_id);
        db_manager::instance().direct_query(query);

        metin2::log::debug("Empire select: {}", query);
        {
            const std::string select_query = sql::format(
                "SELECT pid1, pid2, pid3, pid4 FROM player_index{} WHERE id={}", sql::raw(get_table_postfix()), p->account_id);

            auto pmsg = db_manager::instance().direct_query(select_query);

            sql_result *pRes = pmsg->get();

            if (pRes->num_rows) {
                metin2::log::debug("Empire select: {:d} player_index rows", pRes->num_rows);

                MYSQL_ROW row = mysql_fetch_row(pRes->res.get());
                uint32_t pids[PLAYER_PER_ACCOUNT];

                uint32_t g_start_map[4] =
                    {
                        0,
                        1,
                        21,
                        41};

                uint32_t g_start_position[4][2] =
                    {
                        {0,      0     },
                        {469300, 964200},
                        {55700,  157900},
                        {969600, 278400}
                };

                for (int i = 0; i < PLAYER_PER_ACCOUNT; ++i) {
                    str_to_number(pids[i], row[i]);
                    metin2::log::debug("Empire select: slot pid {:d}", pids[i]);

                    if (pids[i]) {
                        metin2::log::info("Empire select: moving pid {:d} to the village of empire {:d} (map {:d})",
                            pids[i], p->empire, g_start_map[p->empire]);

                        const std::string move_query = sql::format("UPDATE player{} SET map_index={},x={},y={} WHERE id={}",
                            sql::raw(get_table_postfix()),
                            g_start_map[p->empire],
                            g_start_position[p->empire][0],
                            g_start_position[p->empire][1],
                            pids[i]);

                        auto pmsg2 = db_manager::instance().direct_query(move_query);
                    }
                }
            }
        }

        peer_value->encode_header(HEADER_DG_EMPIRE_SELECT, handle, sizeof(uint8_t));
        peer_value->encode_byte(p->empire);
    }

    void client_manager::query_setup(peer *peer, uint32_t handle_value, const char *data) {
        packet_gd_setup *p = (packet_gd_setup *)data;
        data += sizeof(packet_gd_setup);

        if (p->auth_server) {
            metin2::log::info("Auth server peer connected ({})", static_cast<const void *>(peer));

            auth_peer_ = peer;
            return;
        }

        peer->set_public_ip(p->public_ip);
        peer->set_channel(p->channel);
        peer->set_listen_port(p->listen_port);
        peer->set_p2p_port(p->p2p_port);
        peer->set_maps(p->maps);

        const auto location_of = [](db::clients::peer *p) {
            map_location location{};
            strlcpy(location.host, p->public_ip(), sizeof(location.host));
            location.port = p->listen_port();
            std::memcpy(location.maps, p->maps(), sizeof(location.maps));
            return location;
        };

        const map_location kMapLocations = location_of(peer);
        const uint8_t channel = peer->channel();

        const auto is_visible_to_new_peer = [channel](uint8_t other) {
            return other == channel || other == GUILD_WARP_WAR_CHANNEL || (channel == GUILD_WARP_WAR_CHANNEL && other == 1);
        };
        const auto learns_new_maps = [channel](uint8_t other) {
            return other == channel || channel == GUILD_WARP_WAR_CHANNEL || (channel == 1 && other == GUILD_WARP_WAR_CHANNEL);
        };

        uint8_t bMapCount;

        std::vector<map_location> vec_kMapLocations;

        for (db::clients::peer *tmp : peer_list_) {
            if (tmp == peer || !tmp->channel())
                continue;

            if (is_visible_to_new_peer(tmp->channel()))
                vec_kMapLocations.push_back(location_of(tmp));

            if (learns_new_maps(tmp->channel())) {
                tmp->encode_header(HEADER_DG_MAP_LOCATIONS, 0, sizeof(uint8_t) + sizeof(map_location));
                tmp->encode_byte(1);
                tmp->encode(&kMapLocations, sizeof(map_location));
            }
        }

        vec_kMapLocations.push_back(kMapLocations);

        peer->encode_header(HEADER_DG_MAP_LOCATIONS, 0, sizeof(uint8_t) + sizeof(map_location) * vec_kMapLocations.size());
        bMapCount = vec_kMapLocations.size();
        peer->encode_byte(bMapCount);
        peer->encode(vec_kMapLocations.data(), sizeof(map_location) * vec_kMapLocations.size());

        metin2::log::info("Game server setup: channel {:d}, listen port {:d}, p2p port {:d}, {:d} maps", peer->channel(), p->listen_port, p->p2p_port, bMapCount);

        packet_dg_p2p p2pSetupPacket;
        p2pSetupPacket.port = peer->p2p_port();
        p2pSetupPacket.channel = peer->channel();
        strlcpy(p2pSetupPacket.host, peer->public_ip(), sizeof(p2pSetupPacket.host));

        for (auto i = peer_list_.begin(); i != peer_list_.end(); ++i) {
            db::clients::peer *tmp = *i;

            if (tmp == peer)
                continue;

            // Channel 0: auth, or a peer whose SETUP has not arrived yet.
            if (0 == tmp->channel())
                continue;

            tmp->encode_header(HEADER_DG_P2P, 0, sizeof(packet_dg_p2p));
            tmp->encode(&p2pSetupPacket, sizeof(packet_dg_p2p));
        }

        packet_login_on_setup *pck = (packet_login_on_setup *)data;

        for (uint32_t c = 0; c < p->login_count; ++c, ++pck) {
            login_data *pkLD = new login_data;

            pkLD->set_key(pck->login_key);
            pkLD->set_client_key(pck->client_key);

            account_table &r = pkLD->account_ref();

            r.id = pck->id;
            trim_and_lower(pck->login, r.login);
            strlcpy(r.social_id, pck->social_id, sizeof(r.social_id));
            strlcpy(r.passwd, "TEMP", sizeof(r.passwd));

            insert_login_data(pkLD);

            if (insert_logon_account(pck->login, peer->handle())) {
                metin2::log::info("Game server setup: restored login of account {:d} ({}), key {:d}, host {}", pck->id, pck->login, pck->login_key, pck->host);
                pkLD->set_play(true);
            } else
                metin2::log::warn("Game server setup: cannot restore login of account {:d} ({}), key {:d}, already logged in", pck->id, pck->login, pck->login_key);
        }

        send_party_on_setup(peer);
        guild_manager::instance().on_setup(peer);
        priv_manager::instance().send_priv_on_setup(peer);
        send_event_flags_on_setup(peer);
        marriage::manager::instance().on_setup(peer);
    }

    void client_manager::query_item_flush(peer *peer_value, const char *data) {
        uint32_t dwID = *(uint32_t *)data;

        if (metin2::config::db().login_log)
            metin2::log::info("Item cache: flush request for item {:d}", dwID);

        item_cache *c = get_item_cache(dwID);

        if (c)
            c->flush();
    }

    void client_manager::query_item_save(peer *peer_value, const char *data) {
        player_item *p = (player_item *)data;

        // Safebox and mall items are never cached; drop any that are.

        if (p->window == SAFEBOX || p->window == MALL) {
            item_cache *c = get_item_cache(p->id);

            if (c) {
                item_cache_set_ptr_map::iterator it = map_item_cache_set_ptr_.find(c->get()->owner);

                if (it != map_item_cache_set_ptr_.end()) {
                    if (metin2::config::test_server())
                        metin2::log::debug("Item cache: removing item {1:d} from owner {0:d}", c->get()->owner, c->get()->id);

                    it->second->erase(c);
                }

                map_item_cache_.erase(p->id);
            }
            const std::string query = create_item_save_query(*p);

            db_manager::instance().return_query(query, [](sql_msg &) {});
        } else {
            if (metin2::config::test_server())
                metin2::log::debug("Item cache: caching item {1:d} (vnum {2:d}) of owner {0:d}", p->owner, p->id, p->vnum);

            put_item_cache(p);
        }
    }

    auto client_manager::get_item_cache_set(uint32_t pid) -> client_manager::item_cache_set * {
        item_cache_set_ptr_map::iterator it = map_item_cache_set_ptr_.find(pid);

        if (it == map_item_cache_set_ptr_.end())
            return nullptr;

        return it->second.get();
    }

    void client_manager::create_item_cache_set(uint32_t pid) {
        if (map_item_cache_set_ptr_.find(pid) != map_item_cache_set_ptr_.end())
            return;

        map_item_cache_set_ptr_.insert(item_cache_set_ptr_map::value_type(pid, std::make_unique<item_cache_set>()));

        if (metin2::config::db().login_log)
            metin2::log::info("Item cache: created cache for pid {:d}", pid);
    }

    void client_manager::flush_item_cache_set(uint32_t pid) {
        item_cache_set_ptr_map::iterator it = map_item_cache_set_ptr_.find(pid);

        if (it == map_item_cache_set_ptr_.end()) {
            metin2::log::debug("Item cache: no cache to flush for pid {:d}", pid);
            return;
        }

        item_cache_set *pSet = it->second.get();
        item_cache_set::iterator it_set = pSet->begin();

        while (it_set != pSet->end()) {
            item_cache *c = *it_set++;
            c->flush();

            map_item_cache_.erase(c->get()->id);
        }

        pSet->clear();

        map_item_cache_set_ptr_.erase(it);

        if (metin2::config::db().login_log)
            metin2::log::info("Item cache: flushed and removed cache of pid {:d}", pid);
    }

    auto client_manager::get_item_cache(uint32_t id) -> item_cache * {
        item_cache_map::iterator it = map_item_cache_.find(id);

        if (it == map_item_cache_.end())
            return nullptr;

        return it->second.get();
    }

    void client_manager::put_item_cache(player_item *updated, bool skip_query) {
        item_cache *c;

        c = get_item_cache(updated->id);

        if (!c) {
            if (metin2::config::db().login_log)
                metin2::log::info("Item cache: new entry for item {:d} (vnum {:d}), owner {:d}", updated->id, updated->vnum, updated->owner);

            c = map_item_cache_.emplace(updated->id, std::make_unique<item_cache>()).first->second.get();
        } else {
            if (metin2::config::db().login_log)
                metin2::log::info("Item cache: updating item {:d}", updated->id);
            if (updated->owner != c->get()->owner) {
                item_cache_set_ptr_map::iterator it = map_item_cache_set_ptr_.find(c->get()->owner);

                if (it != map_item_cache_set_ptr_.end()) {
                    if (metin2::config::db().login_log)
                        metin2::log::info("Item cache: item {1:d} moved from owner {0:d} to {2:d}", c->get()->owner, c->get()->id, updated->owner);
                    it->second->erase(c);
                }
            }
        }

        c->put(updated, skip_query);

        item_cache_set_ptr_map::iterator it = map_item_cache_set_ptr_.find(c->get()->owner);

        if (it != map_item_cache_set_ptr_.end()) {
            if (metin2::config::db().login_log)
                metin2::log::info("Item cache: cached item {1:d} of owner {0:d}", c->get()->owner, c->get()->id);
            else
                metin2::log::debug("Item cache: cached item {1:d} of owner {0:d}", c->get()->owner, c->get()->id);
            it->second->insert(c);
        } else {
            if (metin2::config::db().login_log)
                metin2::log::info("Item cache: saving item {1:d} of owner {0:d} directly", c->get()->owner, c->get()->id);
            else
                metin2::log::debug("Item cache: saving item {1:d} of owner {0:d} directly", c->get()->owner, c->get()->id);

            c->on_flush();
        }
    }

    auto client_manager::delete_item_cache(uint32_t id) -> bool {
        item_cache *c = get_item_cache(id);

        if (!c)
            return false;

        c->remove();
        return true;
    }

    auto client_manager::item_price_list_cache(uint32_t id) -> item_price_list_table_cache * {
        item_price_list_cache_map::iterator it = item_price_list_cache_.find(id);

        if (it == item_price_list_cache_.end())
            return nullptr;

        return it->second.get();
    }

    void client_manager::put_item_price_list_cache(const item_price_list_table *item_price_list) {
        item_price_list_table_cache *pCache = item_price_list_cache(item_price_list->owner_id);

        if (!pCache) {
            pCache = item_price_list_cache_.emplace(item_price_list->owner_id, std::make_unique<item_price_list_table_cache>()).first->second.get();
        }

        pCache->put(const_cast<item_price_list_table *>(item_price_list), true);
    }

    void client_manager::update_player_cache() {
        player_table_cache_map::iterator it = map_player_cache_.begin();

        while (it != map_player_cache_.end()) {
            player_table_cache *c = (it++)->second.get();

            if (c->check_timeout()) {
                if (metin2::config::db().login_log)
                    metin2::log::info("Player cache: flushing timed-out player {1} (pid {0:d})", c->get(false)->id, c->get(false)->name);

                c->flush();

                update_item_cache_set(c->get()->id);
            } else if (c->check_flush_timeout())
                c->flush();
        }
    }

    void client_manager::set_cache_flush_count_limit(int limit) {
        cache_flush_count_limit_ = std::max(10, limit);
        metin2::log::info("Cache flush limit {:d} per second", cache_flush_count_limit_);
    }

    void client_manager::update_item_cache() {
        if (cache_flush_count_ >= cache_flush_count_limit_)
            return;

        item_cache_map::iterator it = map_item_cache_.begin();

        while (it != map_item_cache_.end()) {
            item_cache *c = (it++)->second.get();

            if (c->check_flush_timeout()) {
                if (metin2::config::test_server())
                    metin2::log::debug("Item cache: flushing item {1:d} (vnum {0:d}) of owner {2:d}", c->get()->vnum, c->get()->id, c->get()->owner);

                c->flush();

                if (++cache_flush_count_ >= cache_flush_count_limit_)
                    break;
            }
        }
    }

    void client_manager::update_item_price_list_cache() {
        item_price_list_cache_map::iterator it = item_price_list_cache_.begin();

        while (it != item_price_list_cache_.end()) {
            item_price_list_table_cache *pCache = it->second.get();

            if (pCache->check_flush_timeout()) {
                pCache->flush();
                item_price_list_cache_.erase(it++);
            } else
                ++it;
        }
    }

    void client_manager::query_item_destroy(peer *peer_value, const char *data) {
        uint32_t dwID = *(uint32_t *)data;
        data += sizeof(uint32_t);

        uint32_t dwPID = *(uint32_t *)data;

        if (!delete_item_cache(dwID)) {
            const std::string query = sql::format("DELETE FROM item{} WHERE id={}", sql::raw(get_table_postfix()), dwID);

            if (metin2::config::db().login_log)
                metin2::log::info("Item cache: deleting item {1:d} of pid {0:d}", dwPID, dwID);

            if (dwPID == 0)
                db_manager::instance().async_query(query);
            else
                db_manager::instance().return_query(query, [](sql_msg &) {});
        }
    }

    void client_manager::query_flush_cache(peer *peer_value, const char *data) {
        uint32_t dwPID = *(uint32_t *)data;

        player_table_cache *pkCache = player_cache(dwPID);

        if (!pkCache)
            return;

        metin2::log::info("Player cache: flushing pid {:d}", dwPID);

        pkCache->flush();
        flush_item_cache_set(dwPID);

        map_player_cache_.erase(dwPID);
    }

    void client_manager::query_sms(peer *peer_value, packet_gdsms *pack) {
        const std::string query = sql::format(
            "INSERT INTO sms_pool (server, sender, receiver, mobile, msg) VALUES({}, {}, {}, {}, {})",
            (player_id_start_ + 2) / 3, pack->from, pack->to, pack->mobile, pack->msg);

        db_manager::instance().async_query(query);
    }

    void client_manager::query_reload_proto() {
        if (!initialize_tables()) {
            metin2::log::error("Proto reload: cannot load tables");
            return;
        }

        for (peer_list::iterator i = peer_list_.begin(); i != peer_list_.end(); ++i) {
            peer *tmp = *i;

            if (!tmp->channel())
                continue;

            tmp->encode_header(HEADER_DG_RELOAD_PROTO, 0,
                sizeof(uint16_t) + sizeof(skill_table) * vec_skill_table_.size() +
                    sizeof(uint16_t) + sizeof(banword_table) * vec_banword_table_.size() +
                    sizeof(uint16_t) + sizeof(item_table) * vec_item_table_.size() +
                    sizeof(uint16_t) + sizeof(mob_table) * vec_mob_table_.size());

            tmp->encode_word(vec_skill_table_.size());
            tmp->encode(vec_skill_table_.data(), sizeof(skill_table) * vec_skill_table_.size());

            tmp->encode_word(vec_banword_table_.size());
            tmp->encode(vec_banword_table_.data(), sizeof(banword_table) * vec_banword_table_.size());

            tmp->encode_word(vec_item_table_.size());
            tmp->encode(vec_item_table_.data(), sizeof(item_table) * vec_item_table_.size());

            tmp->encode_word(vec_mob_table_.size());
            tmp->encode(vec_mob_table_.data(), sizeof(mob_table) * vec_mob_table_.size());
        }
    }

    void client_manager::add_guild_priv(packet_give_guild_priv *p) {
        priv_manager::instance().add_guild_priv(p->guild_id, p->type, p->value, p->duration_sec);
    }

    void client_manager::add_empire_priv(packet_give_empire_priv *p) {
        priv_manager::instance().add_empire_priv(p->empire, p->type, p->value, p->duration_sec);
    }

    void client_manager::add_character_priv(packet_give_character_priv *p) {
        priv_manager::instance().add_char_priv(p->pid, p->type, p->value);
    }

    void client_manager::money_log(packet_money_log *p) {
        db::logs::money_log::instance().add_log(p->type, p->vnum, p->gold);
    }

    auto client_manager::get_login_data(uint32_t key) -> login_data * {
        login_data_by_login_key::iterator it = map_login_data_.find(key);

        if (it == map_login_data_.end())
            return nullptr;

        return it->second;
    }

    auto client_manager::get_login_data_by_login(const char *login) -> login_data * {
        char szLogin[LOGIN_MAX_LEN + 1];
        trim_and_lower(login, szLogin);

        login_data_by_login::iterator it = map_login_data_by_login_.find(szLogin);

        if (it == map_login_data_by_login_.end())
            return nullptr;

        return it->second;
    }

    auto client_manager::get_login_data_by_aid(uint32_t aid) -> login_data * {
        login_data_by_aid::iterator it = map_login_data_by_aid_.find(aid);

        if (it == map_login_data_by_aid_.end())
            return nullptr;

        return it->second;
    }

    void client_manager::insert_login_data(login_data *ld) {
        char szLogin[LOGIN_MAX_LEN + 1];
        trim_and_lower(ld->account_ref().login, szLogin);

        map_login_data_.insert(std::make_pair(ld->key(), ld));
        map_login_data_by_login_.insert(std::make_pair(szLogin, ld));
        map_login_data_by_aid_.insert(std::make_pair(ld->account_ref().id, ld));
    }

    void client_manager::delete_login_data(login_data *ld) {
        map_login_data_.erase(ld->key());
        map_login_data_by_login_.erase(ld->account_ref().login);
        map_login_data_by_aid_.erase(ld->account_ref().id);

        if (map_logon_account_.find(ld->account_ref().login) == map_logon_account_.end())
            delete ld;
        else
            ld->set_deleted(true);
    }

    void client_manager::query_auth_login(peer *peer_value, uint32_t handle, packet_gd_auth_login *p) {
        if (metin2::config::test_server())
            metin2::log::debug("Auth login: account {:d}, key {:d}, login {}", p->id, p->login_key, p->login);
        login_data *pkLD = get_login_data_by_login(p->login);

        if (pkLD) {
            delete_login_data(pkLD);
        }

        uint8_t bResult;

        if (get_login_data(p->login_key)) {
            metin2::log::warn("Auth login: login key {:d} already in use (account {})", p->login_key, p->login);
            bResult = 0;

            peer_value->encode_header(HEADER_DG_AUTH_LOGIN, handle, sizeof(uint8_t));
            peer_value->encode_byte(bResult);
        } else {
            login_data *pkLD = new login_data;

            pkLD->set_key(p->login_key);
            pkLD->set_client_key(p->client_key);
            pkLD->set_premium(p->premium_times);

            account_table &r = pkLD->account_ref();

            r.id = p->id;
            trim_and_lower(p->login, r.login);
            strlcpy(r.social_id, p->social_id, sizeof(r.social_id));
            strlcpy(r.passwd, "TEMP", sizeof(r.passwd));

            metin2::log::info("Auth login: account {:d} ({}), social id {}, key {:d}, client key {:d} {:d} {:d} {:d}",
                p->id, p->login, p->social_id, p->login_key,
                p->client_key[0], p->client_key[1], p->client_key[2], p->client_key[3]);

            bResult = 1;

            insert_login_data(pkLD);

            peer_value->encode_header(HEADER_DG_AUTH_LOGIN, handle, sizeof(uint8_t));
            peer_value->encode_byte(bResult);
        }
    }

    void client_manager::guild_deposit_money(packet_gd_guild_money *p) {
        guild_manager::instance().deposit_money(p->guild, p->gold);
    }

    void client_manager::guild_withdraw_money(peer *peer, packet_gd_guild_money *p) {
        guild_manager::instance().withdraw_money(peer, p->guild, p->gold);
    }

    void client_manager::guild_withdraw_money_give_reply(packet_gd_guild_money_withdraw_give_reply *p) {
        guild_manager::instance().withdraw_money_reply(p->guild, p->give_success, p->change_gold);
    }

    void client_manager::guild_war_bet(packet_gd_guild_war_bet *p) {
        guild_manager::instance().bet(p->war_id, p->login, p->gold, p->guild);
    }

    void client_manager::create_object(packet_gd_create_object *p) {
        using namespace building;

        const std::string query = sql::format(
            "INSERT INTO object{} (land_id, vnum, map_index, x, y, x_rot, y_rot, z_rot) VALUES({}, {}, {}, {}, {}, {:f}, {:f}, {:f})",
            sql::raw(get_table_postfix()), p->land_id, p->vnum, p->map_index, p->x, p->y, p->x_rot, p->y_rot, p->z_rot);

        auto pmsg = db_manager::instance().direct_query(query);

        if (pmsg->get()->insert_id == 0) {
            metin2::log::error("Cannot insert building object (vnum {:d}, land {:d})", p->vnum, p->land_id);
            return;
        }

        auto pkObj = std::make_unique<object_table>();

        memset(pkObj.get(), 0, sizeof(object_table));

        pkObj->id = pmsg->get()->insert_id;
        pkObj->vnum = p->vnum;
        pkObj->land_id = p->land_id;
        pkObj->map_index = p->map_index;
        pkObj->x = p->x;
        pkObj->y = p->y;
        pkObj->x_rot = p->x_rot;
        pkObj->y_rot = p->y_rot;
        pkObj->z_rot = p->z_rot;
        pkObj->life = 0;

        forward_packet(HEADER_DG_CREATE_OBJECT, pkObj.get(), sizeof(object_table));

        map_object_table_.insert(std::make_pair(pkObj->id, std::move(pkObj)));
    }

    void client_manager::delete_object(uint32_t id) {
        const std::string query = sql::format("DELETE FROM object{} WHERE id={}", sql::raw(get_table_postfix()), id);

        auto pmsg = db_manager::instance().direct_query(query);

        if (pmsg->get()->affected_rows == 0 || pmsg->get()->affected_rows == (uint32_t)-1) {
            metin2::log::warn("Cannot delete building object {:d}, not found", id);
            return;
        }

        auto it = map_object_table_.find(id);

        if (it != map_object_table_.end()) {
            map_object_table_.erase(it);
        }

        forward_packet(HEADER_DG_DELETE_OBJECT, &id, sizeof(uint32_t));
    }

    void client_manager::update_land(uint32_t *pdw) {
        uint32_t dwID = pdw[0];
        uint32_t dwGuild = pdw[1];

        const auto p = std::ranges::find(vec_land_table_, dwID, &building::land_table::id);
        if (p == vec_land_table_.end())
            return;

        const std::string query = sql::format("UPDATE land{} SET guild_id={} WHERE id={}", sql::raw(get_table_postfix()), dwGuild, dwID);
        db_manager::instance().async_query(query);

        p->guild_id = dwGuild;
        forward_packet(HEADER_DG_UPDATE_LAND, &*p, sizeof(building::land_table));
    }

    void client_manager::block_chat(packet_block_chat *p) {
        const std::string query = sql::format("SELECT id FROM player{} WHERE name = {}", sql::raw(get_table_postfix()), p->name);
        auto pmsg = db_manager::instance().direct_query(query);
        sql_result *pRes = pmsg->get();

        if (pRes->num_rows) {
            MYSQL_ROW row = mysql_fetch_row(pRes->res.get());
            uint32_t pid = strtoul(row[0], nullptr, 10);

            packet_gd_add_affect pa;
            pa.pid = pid;
            pa.elem.type = 223;
            pa.elem.apply_on = 0;
            pa.elem.apply_value = 0;
            pa.elem.flag = 0;
            pa.elem.duration = p->duration;
            pa.elem.sp_cost = 0;
            query_add_affect(nullptr, &pa);
        }
    }

    void client_manager::marriage_add(packet_marriage_add *p) {
        metin2::log::info("Marriage: add pid {:d} and pid {:d} ({} and {})", p->pid1, p->pid2, p->name1, p->name2);
        marriage::manager::instance().add(p->pid1, p->pid2, p->name1, p->name2);
    }

    void client_manager::marriage_update(packet_marriage_update *p) {
        metin2::log::info("Marriage: update pid {:d} and pid {:d}, love points {:d}, married {:d}", p->pid1, p->pid2, p->love_point, p->married);
        marriage::manager::instance().update(p->pid1, p->pid2, p->love_point, p->married);
    }

    void client_manager::marriage_remove(packet_marriage_remove *p) {
        metin2::log::info("Marriage: remove pid {:d} and pid {:d}", p->pid1, p->pid2);
        marriage::manager::instance().remove(p->pid1, p->pid2);
    }

    void client_manager::wedding_request(packet_wedding_request *p) {
        metin2::log::info("Marriage: wedding requested by pid {:d} and pid {:d}", p->pid1, p->pid2);
        forward_packet(HEADER_DG_WEDDING_REQUEST, p, sizeof(packet_wedding_request));
    }

    void client_manager::wedding_ready(packet_wedding_ready *p) {
        metin2::log::info("Marriage: wedding of pid {:d} and pid {:d} is ready", p->pid1, p->pid2);
        forward_packet(HEADER_DG_WEDDING_READY, p, sizeof(packet_wedding_ready));
        marriage::manager::instance().ready_wedding(p->map_index, p->pid1, p->pid2);
    }

    void client_manager::wedding_end(packet_wedding_end *p) {
        metin2::log::info("Marriage: wedding of pid {:d} and pid {:d} ended", p->pid1, p->pid2);
        marriage::manager::instance().end_wedding(p->pid1, p->pid2);
    }

    void client_manager::myshop_pricelist_update(const item_price_list_table *packet) {
        if (packet->count > SHOP_PRICELIST_MAX_NUM) {
            metin2::log::warn("Price list update for owner pid {:d} has too many entries ({:d}), ignored", packet->owner_id, packet->count);
            return;
        }

        item_price_list_table_cache *pCache = item_price_list_cache(packet->owner_id);

        if (pCache) {
            item_price_list_table table;

            table.owner_id = packet->owner_id;
            table.count = packet->count;

            std::memcpy(table.price_info, packet->price_info, sizeof(item_price_info) * packet->count);

            pCache->update_list(&table);
        } else {
            auto pUpdateTable = std::make_unique<item_price_list_table>();

            pUpdateTable->owner_id = packet->owner_id;
            pUpdateTable->count = packet->count;

            std::memcpy(pUpdateTable->price_info, packet->price_info, sizeof(item_price_info) * packet->count);

            const std::string query = sql::format("SELECT item_vnum, price FROM myshop_pricelist{} WHERE owner_id={}", sql::raw(get_table_postfix()), packet->owner_id);
            db_manager::instance().return_query(query, [this, update_table = std::move(pUpdateTable)](sql_msg &msg) {
                result_pricelist_load_for_update(msg, *update_table);
            });
        }
    }

    void client_manager::myshop_pricelist_request(peer *peer, uint32_t handle_value, uint32_t player_id) {
        if (item_price_list_table_cache *pCache = item_price_list_cache(player_id)) {
            metin2::log::debug("Price list: sending cached list for handle {:d}, pid {:d}", handle_value, player_id);

            item_price_list_table *pTable = pCache->get(false);

            packet_myshop_pricelist_header header =
                {
                    pTable->owner_id,
                    pTable->count};

            size_t sizePriceListSize = sizeof(item_price_info) * pTable->count;

            peer->encode_header(HEADER_DG_MYSHOP_PRICELIST_RES, handle_value, sizeof(header) + sizePriceListSize);
            peer->encode(&header, sizeof(header));
            peer->encode(pTable->price_info, sizePriceListSize);

        } else {
            metin2::log::debug("Price list: loading list for handle {:d}, pid {:d}", handle_value, player_id);

            const std::string query = sql::format("SELECT item_vnum, price FROM myshop_pricelist{} WHERE owner_id={}", sql::raw(get_table_postfix()), player_id);
            db_manager::instance().return_query(query, [this, handle = peer->handle(), req_info = item_pricelist_req_info(handle_value, player_id)](sql_msg &msg) {
                if (auto *peer_value = get_peer(handle))
                    result_pricelist_load(peer_value, msg, req_info);
            });
        }
    }

    void client_manager::process_packets(peer *peer) {
        uint8_t header;
        uint32_t dwHandle;
        uint32_t dwLength;
        const char *data = nullptr;
        int i = 0;
        int iCount = 0;

        while (peer->peek_packet(i, header, dwHandle, dwLength, &data)) {
            ++iCount;

            if (metin2::config::test_server()) {
                if (header != HEADER_GD_PLAYER_COUNT)
                    metin2::log::debug("Packet header {:d}, handle {:d}, length {:d}, #{:d} in batch", header, dwHandle, dwLength, iCount);
            }

            switch (header) {
            case HEADER_GD_BOOT:
                query_boot(peer, (packet_gd_boot *)data);
                break;

            case HEADER_GD_LOGIN_BY_KEY:
                query_login_by_key(peer, dwHandle, (packet_gd_login_by_key *)data);
                break;

            case HEADER_GD_LOGOUT:
                query_logout(peer, dwHandle, data);
                break;

            case HEADER_GD_PLAYER_LOAD:
                metin2::log::debug("Player load request (handle {:d}, length {:d})", dwHandle, dwLength);
                query_player_load(peer, dwHandle, (player_load_packet *)data);
                break;

            case HEADER_GD_PLAYER_SAVE:
                metin2::log::debug("Player save request (handle {:d}, length {:d})", dwHandle, dwLength);
                query_player_save(peer, dwHandle, (player_table *)data);
                break;

            case HEADER_GD_PLAYER_CREATE:
                metin2::log::debug("Player create request (handle {:d}, length {:d})", dwHandle, dwLength);
                query_player_create(peer, dwHandle, (player_create_packet *)data);
                break;

            case HEADER_GD_PLAYER_DELETE:
                metin2::log::debug("Player delete request (handle {:d}, length {:d})", dwHandle, dwLength);
                query_player_delete(peer, dwHandle, (player_delete_packet *)data);
                break;

            case HEADER_GD_PLAYER_COUNT:
                break;

            case HEADER_GD_QUEST_SAVE:
                metin2::log::debug("Quest save request (handle {:d}, length {:d})", dwHandle, dwLength);
                query_quest_save(peer, (quest_table *)data, dwLength);
                break;

            case HEADER_GD_SAFEBOX_LOAD:
                query_safebox_load(peer, dwHandle, (safebox_load_packet *)data, 0);
                break;

            case HEADER_GD_SAFEBOX_SAVE:
                metin2::log::debug("Safebox save request (handle {:d}, length {:d})", dwHandle, dwLength);
                query_safebox_save(peer, (safebox_table *)data);
                break;

            case HEADER_GD_SAFEBOX_CHANGE_SIZE:
                query_safebox_change_size(peer, dwHandle, (safebox_change_size_packet *)data);
                break;

            case HEADER_GD_SAFEBOX_CHANGE_PASSWORD:
                query_safebox_change_password(peer, dwHandle, (safebox_change_password_packet *)data);
                break;

            case HEADER_GD_MALL_LOAD:
                query_safebox_load(peer, dwHandle, (safebox_load_packet *)data, 1);
                break;

            case HEADER_GD_EMPIRE_SELECT:
                query_empire_select(peer, dwHandle, (empire_select_packet *)data);
                break;

            case HEADER_GD_SETUP:
                query_setup(peer, dwHandle, data);
                break;

            case HEADER_GD_GUILD_CREATE:
                guild_create(peer, *(uint32_t *)data);
                break;

            case HEADER_GD_GUILD_SKILL_UPDATE:
                guild_skill_update(peer, (packet_guild_skill_update *)data);
                break;

            case HEADER_GD_GUILD_EXP_UPDATE:
                guild_exp_update(peer, (packet_guild_exp_update *)data);
                break;

            case HEADER_GD_GUILD_ADD_MEMBER:
                guild_add_member(peer, (packet_gd_guild_add_member *)data);
                break;

            case HEADER_GD_GUILD_REMOVE_MEMBER:
                guild_remove_member(peer, (packet_guild *)data);
                break;

            case HEADER_GD_GUILD_CHANGE_GRADE:
                guild_change_grade(peer, (packet_guild *)data);
                break;

            case HEADER_GD_GUILD_CHANGE_MEMBER_DATA:
                guild_change_member_data(peer, (packet_guild_change_member_data *)data);
                break;

            case HEADER_GD_GUILD_DISBAND:
                guild_disband(peer, (packet_guild *)data);
                break;

            case HEADER_GD_GUILD_WAR:
                guild_war(peer, (packet_guild_war *)data);
                break;

            case HEADER_GD_GUILD_WAR_SCORE:
                guild_war_score(peer, (packet_guild_war_score *)data);
                break;

            case HEADER_GD_GUILD_CHANGE_LADDER_POINT:
                guild_change_ladder_point((packet_guild_ladder_point *)data);
                break;

            case HEADER_GD_GUILD_USE_SKILL:
                guild_use_skill((packet_guild_use_skill *)data);
                break;

            case HEADER_GD_FLUSH_CACHE:
                query_flush_cache(peer, data);
                break;

            case HEADER_GD_ITEM_SAVE:
                query_item_save(peer, data);
                break;

            case HEADER_GD_ITEM_DESTROY:
                query_item_destroy(peer, data);
                break;

            case HEADER_GD_ITEM_FLUSH:
                query_item_flush(peer, data);
                break;

            case HEADER_GD_ADD_AFFECT:
                metin2::log::debug("Add affect request");
                query_add_affect(peer, (packet_gd_add_affect *)data);
                break;

            case HEADER_GD_REMOVE_AFFECT:
                metin2::log::debug("Remove affect request");
                query_remove_affect(peer, (packet_gd_remove_affect *)data);
                break;

            case HEADER_GD_HIGHSCORE_REGISTER:
                query_highscore_register(peer, (packet_gd_highscore *)data);
                break;

            case HEADER_GD_PARTY_CREATE:
                query_party_create(peer, (packet_party_create *)data);
                break;

            case HEADER_GD_PARTY_DELETE:
                query_party_delete(peer, (packet_party_delete *)data);
                break;

            case HEADER_GD_PARTY_ADD:
                query_party_add(peer, (packet_party_add *)data);
                break;

            case HEADER_GD_PARTY_REMOVE:
                query_party_remove(peer, (packet_party_remove *)data);
                break;

            case HEADER_GD_PARTY_STATE_CHANGE:
                query_party_state_change(peer, (packet_party_state_change *)data);
                break;

            case HEADER_GD_PARTY_SET_MEMBER_LEVEL:
                query_party_set_member_level(peer, (packet_party_set_member_level *)data);
                break;

            case HEADER_GD_RELOAD_PROTO:
                query_reload_proto();
                break;

            case HEADER_GD_CHANGE_NAME:
                query_change_name(peer, dwHandle, (packet_gd_change_name *)data);
                break;

            case HEADER_GD_SMS:
                query_sms(peer, (packet_gdsms *)data);
                break;

            case HEADER_GD_AUTH_LOGIN:
                query_auth_login(peer, dwHandle, (packet_gd_auth_login *)data);
                break;

            case HEADER_GD_REQUEST_GUILD_PRIV:
                add_guild_priv((packet_give_guild_priv *)data);
                break;

            case HEADER_GD_REQUEST_EMPIRE_PRIV:
                add_empire_priv((packet_give_empire_priv *)data);
                break;

            case HEADER_GD_REQUEST_CHARACTER_PRIV:
                add_character_priv((packet_give_character_priv *)data);
                break;

            case HEADER_GD_MONEY_LOG:
                money_log((packet_money_log *)data);
                break;

            case HEADER_GD_GUILD_DEPOSIT_MONEY:
                guild_deposit_money((packet_gd_guild_money *)data);
                break;

            case HEADER_GD_GUILD_WITHDRAW_MONEY:
                guild_withdraw_money(peer, (packet_gd_guild_money *)data);
                break;

            case HEADER_GD_GUILD_WITHDRAW_MONEY_GIVE_REPLY:
                guild_withdraw_money_give_reply((packet_gd_guild_money_withdraw_give_reply *)data);
                break;

            case HEADER_GD_GUILD_WAR_BET:
                guild_war_bet((packet_gd_guild_war_bet *)data);
                break;

            case HEADER_GD_SET_EVENT_FLAG:
                set_event_flag((packet_set_event_flag *)data);
                break;

            case HEADER_GD_CREATE_OBJECT:
                create_object((packet_gd_create_object *)data);
                break;

            case HEADER_GD_DELETE_OBJECT:
                delete_object(*(uint32_t *)data);
                break;

            case HEADER_GD_UPDATE_LAND:
                update_land((uint32_t *)data);
                break;

            case HEADER_GD_MARRIAGE_ADD:
                marriage_add((packet_marriage_add *)data);
                break;

            case HEADER_GD_MARRIAGE_UPDATE:
                marriage_update((packet_marriage_update *)data);
                break;

            case HEADER_GD_MARRIAGE_REMOVE:
                marriage_remove((packet_marriage_remove *)data);
                break;

            case HEADER_GD_WEDDING_REQUEST:
                wedding_request((packet_wedding_request *)data);
                break;

            case HEADER_GD_WEDDING_READY:
                wedding_ready((packet_wedding_ready *)data);
                break;

            case HEADER_GD_WEDDING_END:
                wedding_end((packet_wedding_end *)data);
                break;

            case HEADER_GD_BLOCK_CHAT:
                block_chat((packet_block_chat *)data);
                break;

            case HEADER_GD_MYSHOP_PRICELIST_UPDATE:
                myshop_pricelist_update((item_price_list_table *)data);
                break;

            case HEADER_GD_MYSHOP_PRICELIST_REQ:
                myshop_pricelist_request(peer, dwHandle, *(uint32_t *)data);
                break;

            case HEADER_GD_RELOAD_ADMIN:
                reload_admin(peer, (packet_reload_admin *)data);
                break;

            case HEADER_GD_BREAK_MARRIAGE:
                break_marriage(peer, data);
                break;

            case HEADER_GD_ELECT_MONARCH:
                election(peer, dwHandle, data);
                break;

            case HEADER_GD_CANDIDACY:
                candidacy(peer, dwHandle, data);
                break;

            case HEADER_GD_ADD_MONARCH_MONEY:
                add_monarch_money(peer, dwHandle, data);
                break;

            case HEADER_GD_DEC_MONARCH_MONEY:
                dec_monarch_money(peer, dwHandle, data);
                break;

            case HEADER_GD_TAKE_MONARCH_MONEY:
                take_monarch_money(peer, dwHandle, data);
                break;

            case HEADER_GD_COME_TO_VOTE:
                come_to_vote(peer, dwHandle, data);
                break;

            case HEADER_GD_RMCANDIDACY:
                rm_candidacy(peer, dwHandle, data);
                break;

            case HEADER_GD_SETMONARCH:
                set_monarch(peer, dwHandle, data);
                break;

            case HEADER_GD_RMMONARCH:
                rm_monarch(peer, dwHandle, data);
                break;

            case HEADER_GD_CHANGE_MONARCH_LORD:
                change_monarch_lord(peer, dwHandle, (packet_change_monarch_lord *)data);
                break;

            case HEADER_GD_REQ_SPARE_ITEM_ID_RANGE:
                send_spare_item_id_range(peer);
                break;

            case HEADER_GD_REQ_CHANGE_GUILD_MASTER:
                guild_change_master((packet_change_guild_master *)data);
                break;

            case HEADER_GD_UPDATE_HORSE_NAME:
                update_horse_name((packet_update_horse_name *)data, peer);
                break;

            case HEADER_GD_REQ_HORSE_NAME:
                ack_horse_name(*(uint32_t *)data, peer);
                break;

            case HEADER_GD_DC:
                delete_login_key((packet_dc *)data);
                break;

            case HEADER_GD_VALID_LOGOUT:
                reset_last_player_id((packet_need_login_log_info *)data);
                break;

            case HEADER_GD_REQUEST_CHARGE_CASH:
                charge_cash((request_charge_cash *)data);
                break;

            case HEADER_GD_DELETE_AWARDID:
                delete_award_id((packet_delete_award_id *)data);
                break;

            case HEADER_GD_UPDATE_CHANNELSTATUS:
                update_channel_status((channel_status *)data);
                break;
            case HEADER_GD_REQUEST_CHANNELSTATUS:
                request_channel_status(peer, dwHandle);
                break;
            default:
                metin2::log::error("Unknown packet header {:d} (handle {:d}, length {:d})", header, dwHandle, dwLength);
                break;
            }
        }

        peer->recv_end(i);
    }

    void client_manager::add_peer(net::tcp::socket socket) {
        constexpr std::size_t MAX_PEER_BUFFER = 256 * 1024 * 1024;

        auto connection = net::connection::create(std::move(socket), MAX_PEER_BUFFER, MAX_PEER_BUFFER);

        if (!is_allowed_peer(connection->remote_address().c_str(), metin2::config::db().allowed_peers, "127.0.0.1")) {
            metin2::log::info("Rejected connection from {}, not an allowed peer", connection->remote_address());
            connection->close();
            return;
        }

        peer_list_.push_front(new peer(std::move(connection)));
    }

    void client_manager::remove_peer(peer *peer_value) {
        if (auth_peer_ == peer_value) {
            auth_peer_ = nullptr;
        } else {
            logon_account_map::iterator it = map_logon_account_.begin();

            while (it != map_logon_account_.end()) {
                login_data *pkLD = it->second;

                if (pkLD->connected_peer_handle() == peer_value->handle()) {
                    pkLD->set_play(false);

                    if (pkLD->is_deleted()) {
                        metin2::log::debug("Deleting login data of account {}", pkLD->account_ref().login);
                        delete pkLD;
                    }

                    map_logon_account_.erase(it++);
                } else
                    ++it;
            }
        }

        peer_value->return_item_id_range();
        peer_list_.remove(peer_value);
        delete peer_value;
    }

    auto client_manager::get_peer(uint32_t ident) -> peer * {
        for (auto i = peer_list_.begin(); i != peer_list_.end(); ++i) {
            peer *tmp = *i;

            if (tmp->handle() == ident)
                return tmp;
        }

        return nullptr;
    }

    auto client_manager::any_peer() -> peer * {
        if (peer_list_.empty())
            return nullptr;

        return peer_list_.front();
    }

    void usage_log() {
        FILE *fp = nullptr;

        time_t ct;
        char *time_s;
        struct tm lt;

        int avg = usage_avg / 3600;

        fp = fopen("usage.txt", "a+");

        if (!fp)
            return;

        ct = time(nullptr);
        lt = *localtime(&ct);
        time_s = asctime(&lt);

        time_s[strlen(time_s) - 1] = '\0';

        fprintf(fp, "| %4d %-15.15s | %5d | %5u |", lt.tm_year + 1900, time_s + 4, avg, usage_max);

        fprintf(fp, "\n");
        fclose(fp);

        usage_max = usage_avg = 0;
    }

    auto client_manager::process() -> int {
        int pulses;

        if (!(pulses = thecore_idle()))
            return 0;

        while (pulses--) {
            const int pulse = thecore_next_pulse();
            const int per_sec = thecore_passes_per_sec();

            if (!(pulse % per_sec)) {
                const sql_stats ret_stats = db_manager::instance().return_stats(SQL_PLAYER);
                const sql_stats async_stats = db_manager::instance().async_stats(SQL_PLAYER);

                if (!metin2::config::test_server() || !(pulse % (per_sec * 10))) {
                    metin2::log::debug("Perf: pulse {:d}, return queries {:d}/{:d}/{:d}/{:d}, async queries {:d}/{:d}/{:d}/{:d}",
                        pulse,
                        ret_stats.queued,
                        ret_stats.results,
                        ret_stats.finished,
                        ret_stats.copied,
                        async_stats.queued,
                        async_stats.results,
                        async_stats.finished,
                        async_stats.copied);

                    if ((pulse % 50) == 0)
                        metin2::log::debug("Perf: pulse {:d}, return queries {:d}/{:d}/{:d}, async queries {:d}/{:d}/{:d}",
                            pulse,
                            ret_stats.queued,
                            ret_stats.results,
                            ret_stats.finished,
                            async_stats.queued,
                            async_stats.results,
                            async_stats.finished);
                }

                db_manager::instance().reset_counter();

                uint32_t dwCount = client_manager::instance().user_count();

                usage_avg += dwCount;
                usage_max = std::max<int>(usage_max, dwCount);

                thecore_idle_time = 0;

                if (!(pulse % (per_sec * 3600)))
                    usage_log();

                cache_flush_count_ = 0;

                update_player_cache();
                update_item_cache();
                update_logout_player();

                update_item_price_list_cache();

                guild_manager::instance().update();
                priv_manager::instance().update();
                marriage::manager::instance().update();
            }

            if (!(pulse % (per_sec * 5))) {
                item_award_manager::instance().request_load();
            }

            if (!(pulse % (per_sec * 10))) {
                metin2::log::debug("Perf: {:d} main queries, {:d} async queries in the last 10 s", query_count[0], query_count[1]);
                query_count[0] = 0;
                query_count[1] = 0;

                metin2::log::debug("Perf: {:d} item saves in the last 10 s", item_count);
                item_count = 0;
            }

            if (!(pulse % (per_sec * 60))) {
                client_manager::instance().send_time();
            }

            if (!(pulse % (per_sec * 3600))) {
                db::logs::money_log::instance().save();
            }
        }

        net::io().poll();

        // Closed peers are removed here, never inside their own connection's handlers.
        for (auto it = peer_list_.begin(); it != peer_list_.end();) {
            peer *peer = *it++;

            if (peer->is_closed())
                remove_peer(peer);
        }

        return 1;
    }

    auto client_manager::user_count() -> uint32_t {
        return map_logon_account_.size();
    }

    void client_manager::send_time() {
        int32_t now = current_time();
        forward_packet(HEADER_DG_TIME, &now, sizeof(int32_t));
    }

    void client_manager::forward_packet(uint8_t header, const void *data, int size, uint8_t channel_value, peer *except) {
        for (auto it = peer_list_.begin(); it != peer_list_.end(); ++it) {
            peer *peer = *it;

            if (peer == except)
                continue;

            if (!peer->channel())
                continue;

            if (channel_value && peer->channel() != channel_value)
                continue;

            peer->encode_header(header, 0, size);

            if (size > 0 && data)
                peer->encode(data, size);
        }
    }

    auto client_manager::current_time() -> time_t {
        return time(nullptr);
    }

    auto client_manager::initialize_now_item_id() -> bool {
        const uint32_t dwMin = metin2::config::db().item_id_min;
        const uint32_t dwMax = metin2::config::db().item_id_max;

        metin2::log::info("Item id range from config: {:d} to {:d}", dwMin, dwMax);

        if (item_id_range_manager::instance().build_range(dwMin, dwMax, item_range_) == false) {
            metin2::log::error("Cannot build item id range");
            return false;
        }

        metin2::log::info("Item id range {:d} to {:d}, next free id {:d}", item_range_.min, item_range_.max, item_range_.usable_item_id_min);

        return true;
    }

    auto client_manager::gain_item_id() -> uint32_t {
        return item_range_.usable_item_id_min++;
    }

    auto client_manager::item_id() -> uint32_t {
        return item_range_.usable_item_id_min;
    }

    auto client_manager::initialize_localization() -> bool {
        const std::string query = "SELECT mValue, mKey FROM locale";
        auto pMsg = db_manager::instance().direct_query(query, SQL_COMMON);

        if (pMsg->get()->num_rows == 0) {
            metin2::log::error("Locale table is empty or cannot be read ({})", query);
            return false;
        }

        metin2::log::info("Loaded {:d} locale settings", pMsg->get()->num_rows);

        MYSQL_ROW row = nullptr;

        while ((row = mysql_fetch_row(pMsg->get()->res.get())) != nullptr) {
            int col = 0;
            t_locale locale;

            strlcpy(locale.value, row[col++], sizeof(locale.value));
            strlcpy(locale.key, row[col++], sizeof(locale.key));

            if (strcmp(locale.key, "LOCALE") == 0) {
                if (strcmp(locale.value, "english") != 0) {
                    metin2::log::error("Locale {} is not supported, only english is", locale.value);
                    exit(0);
                }

                metin2::log::info("Locale: {}", locale.value);
                locale_name_column = "locale_name";
            } else if (strcmp(locale.key, "DB_NAME_COLUMN") == 0) {
                metin2::log::info("Locale: name column {}", locale.value);
                locale_name_column = locale.value;
            } else {
                metin2::log::debug("Locale: {} = {} (not used by the db)", locale.key, locale.value);
            }
        }

        return true;
    }

    auto client_manager::admin_info(const char *ip, std::vector<t_admin_info> &admin_vec) -> bool {
        const std::string query = sql::format(
            "SELECT mID,mAccount,mName,mContactIP,mServerIP,mAuthority FROM gmlist WHERE mServerIP='ALL' or mServerIP={}",
            ip ? ip : "ALL");

        auto pMsg = db_manager::instance().direct_query(query, SQL_COMMON);

        if (pMsg->get()->num_rows == 0) {
            metin2::log::info("No gm accounts found ({})", query);
            return false;
        }

        MYSQL_ROW row;
        admin_vec.reserve(pMsg->get()->num_rows);

        while ((row = mysql_fetch_row(pMsg->get()->res.get()))) {
            int idx = 0;
            t_admin_info Info;

            str_to_number(Info.id, row[idx++]);
            trim_and_lower(row[idx++], Info.account);
            strlcpy(Info.name, row[idx++], sizeof(Info.name));
            strlcpy(Info.contact_ip, row[idx++], sizeof(Info.contact_ip));
            strlcpy(Info.server_ip, row[idx++], sizeof(Info.server_ip));
            std::string stAuth = row[idx++];

            if (!stAuth.compare("IMPLEMENTOR"))
                Info.authority = GM_IMPLEMENTOR;
            else if (!stAuth.compare("GOD"))
                Info.authority = GM_GOD;
            else if (!stAuth.compare("HIGH_WIZARD"))
                Info.authority = GM_HIGH_WIZARD;
            else if (!stAuth.compare("LOW_WIZARD"))
                Info.authority = GM_LOW_WIZARD;
            else if (!stAuth.compare("WIZARD"))
                Info.authority = GM_WIZARD;
            else
                continue;

            admin_vec.push_back(Info);

            metin2::log::info("Gm: pid {:d}, account {}, character {}, contact ip {}, server ip {}, authority {:d} ({})",
                Info.id, Info.account, Info.name, Info.contact_ip, Info.server_ip, Info.authority, stAuth);
        }

        return true;
    }

    auto client_manager::host_info(std::vector<std::string> &ip_vec) -> bool {
        const std::string query = "SELECT mIP FROM gmhost";
        auto pMsg = db_manager::instance().direct_query(query, SQL_COMMON);

        if (pMsg->get()->num_rows == 0) {
            metin2::log::info("No gm hosts configured ({})", query);
            return false;
        }

        ip_vec.reserve(pMsg->get()->num_rows);

        MYSQL_ROW row;

        while ((row = mysql_fetch_row(pMsg->get()->res.get()))) {
            if (row[0] && *row[0]) {
                ip_vec.push_back(row[0]);
                metin2::log::info("Gm host: {}", row[0]);
            }
        }

        return true;
    }

    void client_manager::reload_admin(peer *, packet_reload_admin *p) {
        std::vector<t_admin_info> vAdmin;
        std::vector<std::string> vHost;

        host_info(vHost);
        admin_info(p->ip, vAdmin);

        uint32_t dwPacketSize = sizeof(uint16_t) + sizeof(uint16_t) + sizeof(t_admin_info) * vAdmin.size() +
                                sizeof(uint16_t) + sizeof(uint16_t) + 16 * vHost.size();

        for (auto it = peer_list_.begin(); it != peer_list_.end(); ++it) {
            peer *peer = *it;

            if (!peer->channel())
                continue;

            peer->encode_header(HEADER_DG_RELOAD_ADMIN, 0, dwPacketSize);

            peer->encode_word(16);
            peer->encode_word(vHost.size());

            for (size_t n = 0; n < vHost.size(); ++n)
                peer->encode(vHost[n].c_str(), 16);

            peer->encode_word(sizeof(t_admin_info));
            peer->encode_word(vAdmin.size());

            for (size_t n = 0; n < vAdmin.size(); ++n)
                peer->encode(&vAdmin[n], sizeof(t_admin_info));
        }

        metin2::log::info("Reloaded gm list for game server {}", p->ip);
    }

    void client_manager::break_marriage(peer *peer, const char *data) {
        uint32_t pid1, pid2;

        pid1 = *(int *)data;
        data += sizeof(int);

        pid2 = *(int *)data;
        data += sizeof(int);

        metin2::log::info("Marriage: breaking off engagement of pid {:d} and pid {:d}", pid1, pid2);
        marriage::manager::instance().remove(pid1, pid2);
    }

    void client_manager::update_item_cache_set(uint32_t pid) {
        auto it = map_item_cache_set_ptr_.find(pid);

        if (it == map_item_cache_set_ptr_.end()) {
            if (metin2::config::test_server())
                metin2::log::debug("Item cache: no cache to update for pid {:d}", pid);
            return;
        }

        item_cache_set *pSet = it->second.get();
        item_cache_set::iterator it_set = pSet->begin();

        while (it_set != pSet->end()) {
            item_cache *c = *it_set++;
            c->flush();
        }

        if (metin2::config::db().login_log)
            metin2::log::info("Item cache: flushed items of pid {:d}", pid);
    }

    void client_manager::election(peer *peer, uint32_t handle, const char *data) {
        uint32_t idx;
        uint32_t selectingpid;

        idx = *(uint32_t *)data;
        data += sizeof(uint32_t);

        selectingpid = *(uint32_t *)data;
        data += sizeof(uint32_t);

        const int Success = monarch::instance().vote_monarch(selectingpid, idx);

        if (metin2::config::test_server()) {
            if (Success)
                metin2::log::debug("Monarch election: vote {:d} for pid {:d} recorded", idx, selectingpid);
            else
                metin2::log::debug("Monarch election: vote {:d} for pid {:d} failed", idx, selectingpid);
        }

        peer->encode_header(HEADER_DG_ELECT_MONARCH, handle, sizeof(int));
        peer->encode(&Success, sizeof(int));
    }

    void client_manager::candidacy(peer *peer, uint32_t handle, const char *data) {
        uint32_t pid;

        pid = *(uint32_t *)data;
        data += sizeof(uint32_t);

        if (!monarch::instance().add_candidacy(pid, data)) {
            if (metin2::config::test_server())
                metin2::log::debug("Monarch election: candidacy of pid {:d} ({}) rejected", pid, data);

            const int rejected = 0;
            peer->encode_header(HEADER_DG_CANDIDACY, handle, sizeof(int) + 32);
            peer->encode(&rejected, sizeof(int));
            peer->encode(data, 32);
            return;
        } else {
            if (metin2::config::test_server())
                metin2::log::debug("Monarch election: pid {:d} ({}) is now a candidate", pid, data);

            broadcast_to_game_peers(peer_list_, HEADER_DG_CANDIDACY, peer, handle, 0, {
                                                                                          {&pid, sizeof(int)},
                                                                                          {data, 32         }
            });
        }
    }

    void client_manager::add_monarch_money(peer *peer, uint32_t handle, const char *data) {
        int Empire = *(int *)data;
        data += sizeof(int);

        int Money = *(int *)data;
        data += sizeof(int);

        if (metin2::config::test_server())
            metin2::log::debug("Monarch: add {1:d} money to empire {0:d}", Empire, Money);

        monarch::instance().add_money(Empire, Money);

        broadcast_to_game_peers(peer_list_, HEADER_DG_ADD_MONARCH_MONEY, peer, handle, 0, {
                                                                                              {&Empire, sizeof(int)},
                                                                                              {&Money,  sizeof(int)}
        });
    }
    void client_manager::dec_monarch_money(peer *peer, uint32_t handle, const char *data) {
        int Empire = *(int *)data;
        data += sizeof(int);

        int Money = *(int *)data;
        data += sizeof(int);

        if (metin2::config::test_server())
            metin2::log::debug("Monarch: remove {1:d} money from empire {0:d}", Empire, Money);

        monarch::instance().dec_money(Empire, Money);

        broadcast_to_game_peers(peer_list_, HEADER_DG_DEC_MONARCH_MONEY, peer, handle, 0, {
                                                                                              {&Empire, sizeof(int)},
                                                                                              {&Money,  sizeof(int)}
        });
    }

    void client_manager::take_monarch_money(peer *peer, uint32_t handle, const char *data) {
        int Empire = *(int *)data;
        data += sizeof(int);

        uint32_t pid = *(uint32_t *)data;
        data += sizeof(int);

        int Money = *(int *)data;
        data += sizeof(int);

        if (metin2::config::test_server())
            metin2::log::debug("Monarch: take {1:d} money from empire {0:d}", Empire, Money);

        if (!monarch::instance().take_money(Empire, pid, Money))
            Money = 0;

        peer->encode_header(HEADER_DG_TAKE_MONARCH_MONEY, handle, sizeof(int) + sizeof(int));
        peer->encode(&Empire, sizeof(int));
        peer->encode(&Money, sizeof(int));
    }

    void client_manager::come_to_vote(peer *peer, uint32_t handle, const char *data) {
        monarch::instance().elect_monarch();
    }

    void client_manager::rm_candidacy(peer *peer, uint32_t handle, const char *data) {
        char szName[32];

        strlcpy(szName, data, sizeof(szName));
        metin2::log::info("Monarch: gm removed candidacy of {}", szName);

        int iRet = monarch::instance().del_candidacy(szName) ? 1 : 0;

        if (1 == iRet) {
            broadcast_to_game_peers(peer_list_, HEADER_DG_RMCANDIDACY, peer, handle, handle, {
                                                                                                 {&iRet,  sizeof(int)   },
                                                                                                 {szName, sizeof(szName)}
            });
        } else {
            db::clients::peer *p = peer;
            p->encode_header(HEADER_DG_RMCANDIDACY, handle, sizeof(int) + sizeof(szName));
            p->encode(&iRet, sizeof(int));
            p->encode(szName, sizeof(szName));
        }
    }

    void client_manager::set_monarch(peer *peer, uint32_t handle, const char *data) {
        char szName[32];

        strlcpy(szName, data, sizeof(szName));

        if (metin2::config::test_server())
            metin2::log::debug("Monarch: gm set {} as monarch", szName);

        int iRet = monarch::instance().set_monarch(szName) ? 1 : 0;

        if (1 == iRet) {
            broadcast_to_game_peers(peer_list_, HEADER_DG_RMCANDIDACY, peer, handle, handle, {
                                                                                                 {&iRet,  sizeof(int)   },
                                                                                                 {szName, sizeof(szName)}
            });
        } else {
            db::clients::peer *p = peer;
            p->encode_header(HEADER_DG_RMCANDIDACY, handle, sizeof(int) + sizeof(szName));
            p->encode(&iRet, sizeof(int));
            p->encode(szName, sizeof(szName));
        }
    }

    void client_manager::rm_monarch(peer *peer, uint32_t handle, const char *data) {
        char szName[32];

        strlcpy(szName, data, sizeof(szName));

        if (metin2::config::test_server())
            metin2::log::debug("Monarch: gm removed monarch {}", szName);

        int iRet = monarch::instance().del_monarch(szName) ? 1 : 0;

        if (1 == iRet) {
            broadcast_to_game_peers(peer_list_, HEADER_DG_RMMONARCH, peer, handle, handle, {
                                                                                               {&iRet,  sizeof(int)   },
                                                                                               {szName, sizeof(szName)}
            });
        } else {
            db::clients::peer *p = peer;
            p->encode_header(HEADER_DG_RMCANDIDACY, handle, sizeof(int) + sizeof(szName));
            p->encode(&iRet, sizeof(int));
            p->encode(szName, sizeof(szName));
        }
    }

    void client_manager::change_monarch_lord(peer *peer, uint32_t handle, packet_change_monarch_lord *info) {
        const std::string query = sql::format(
            "SELECT a.name, NOW() FROM player{0} AS a, player_index{0} AS b WHERE (a.account_id=b.id AND a.id={1} AND b.empire={2}) AND "
            "(b.pid1={1} OR b.pid2={1} OR b.pid3={1} OR b.pid4={1})",
            sql::raw(get_table_postfix()), info->pid, info->empire);

        auto pMsg = db_manager::instance().direct_query(query, SQL_PLAYER);

        if (pMsg->get()->num_rows != 0) {
            packet_change_monarch_lord_ack ack;
            ack.empire = info->empire;
            ack.pid = info->pid;

            MYSQL_ROW row = mysql_fetch_row(pMsg->get()->res.get());
            strlcpy(ack.name, row[0], sizeof(ack.name));
            strlcpy(ack.date, row[1], sizeof(ack.date));

            const std::string update_query = sql::format("UPDATE monarch SET pid={}, windate=NOW() WHERE empire={}", ack.pid, ack.empire);
            auto pMsg2 = db_manager::instance().direct_query(update_query, SQL_PLAYER);

            if (pMsg2->get()->affected_rows > 0) {
                monarch::instance().load_monarch();

                monarch_info *newInfo = monarch::instance().get_monarch();

                for (auto it = peer_list_.begin(); it != peer_list_.end(); it++) {
                    db::clients::peer *client = *it;

                    client->encode_header(HEADER_DG_CHANGE_MONARCH_LORD_ACK, 0, sizeof(packet_change_monarch_lord_ack));
                    client->encode(&ack, sizeof(packet_change_monarch_lord_ack));

                    client->encode_header(HEADER_DG_UPDATE_MONARCH_INFO, 0, sizeof(monarch_info));
                    client->encode(newInfo, sizeof(monarch_info));
                }
            }
        }
    }

    void client_manager::send_spare_item_id_range(peer *peer) {
        peer->send_spare_item_id_range();
    }

    void client_manager::delete_login_key(packet_dc *data) {
        char login[LOGIN_MAX_LEN + 1] = {0};
        trim_and_lower(data->login, login);

        login_data *pkLD = get_login_data_by_login(login);

        if (pkLD) {
            login_data_by_login_key::iterator it = map_login_data_.find(pkLD->key());

            if (it != map_login_data_.end())
                map_login_data_.erase(it);
        }
    }

    void client_manager::delete_award_id(packet_delete_award_id *data) {
        if (item_award_manager::instance().remove(data->id))
            metin2::log::debug("Item award: removed award {:d} from cache", data->id);
        else
            metin2::log::warn("Item award: cannot delete award {:d}, not found", data->id);
    }

    void client_manager::update_channel_status(channel_status *data) {
        channel_status_map::iterator it = m_channel_status_.find(data->port);
        if (it != m_channel_status_.end()) {
            it->second = data->status;
        } else {
            m_channel_status_.insert(channel_status_map::value_type(data->port, data->status));
        }
    }

    void client_manager::request_channel_status(peer *peer, uint32_t handle) {
        const int nSize = m_channel_status_.size();
        peer->encode_header(HEADER_DG_RESPOND_CHANNELSTATUS, handle, sizeof(channel_status) * nSize + sizeof(int));
        peer->encode(&nSize, sizeof(int));
        for (channel_status_map::iterator it = m_channel_status_.begin(); it != m_channel_status_.end(); it++) {
            peer->encode(&it->first, sizeof(short));
            peer->encode(&it->second, sizeof(uint8_t));
        }
    }

    void client_manager::reset_last_player_id(const packet_need_login_log_info *data) {
        login_data *pkLD = get_login_data_by_aid(data->player_id);

        if (nullptr != pkLD) {
            pkLD->set_last_player_id(0);
        }
    }

    void client_manager::charge_cash(const request_charge_cash *packet) {
        std::string query;

        if (ERequestCharge_Cash == packet->charge_type)
            query = sql::format("update account set `cash` = `cash` + {} where id = {} limit 1", static_cast<int32_t>(packet->amount), packet->aid);
        else if (ERequestCharge_Mileage == packet->charge_type)
            query = sql::format("update account set `mileage` = `mileage` + {} where id = {} limit 1", static_cast<int32_t>(packet->amount), packet->aid);
        else {
            metin2::log::warn("Invalid charge type {:d} (amount {:d}, account {:d})", packet->charge_type, packet->amount, packet->aid);
            return;
        }

        metin2::log::info("Charging account: type {:d}, amount {:d}, account {:d}", packet->charge_type, packet->amount, packet->aid);

        db_manager::instance().async_query(query, SQL_ACCOUNT);
    }
}
