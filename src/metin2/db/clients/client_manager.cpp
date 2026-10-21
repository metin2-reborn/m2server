
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
#include <metin2/proto/character_types.h>
#include <metin2/proto/inventory.h>
#include <metin2/proto/item_types.h>
#include <metin2/proto/limits.h>
#include <metin2/proto/shop_types.h>
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
#include <unordered_set>
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
            for (const auto &part : payload) {
                size += part.size;
            }

            for (peer *p : peers) {
                if (!p->channel()) {
                    continue;
                }

                p->encode_header(header, p == requester ? requester_handle : others_handle, size);
                for (const auto &part : payload) {
                    p->encode(part.data, part.size);
                }
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
        channel_status_.clear();
        for (peer *peer : peer_list_) {
            delete peer;
        }

        peer_list_.clear();

        // A login_data lives in login_data_, or only in logon_accounts_ once it is marked deleted.
        std::unordered_set<login_data *> logins;
        for (const auto &[key, ld] : login_data_)
            logins.insert(ld);
        for (const auto &[login, ld] : logon_accounts_)
            logins.insert(ld);
        for (login_data *ld : logins)
            delete ld;

        login_data_.clear();
        login_data_by_login_.clear();
        login_data_by_aid_.clear();
        logon_accounts_.clear();

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

        get_guild_manager().boot_reserve_war();

        const auto &db = metin2::config::db();

        listener_ = net::listener::open(db.bind_ip, db.port, [this](net::tcp::socket socket) { add_peer(std::move(socket)); });
        if (!listener_) {
            return false;
        }

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
            while (auto msg = get_db_manager().pop_result()) {
                if (msg->on_result)
                    msg->on_result(*msg);
            }

            if (!process())
                break;

            metin2::log::rotate();
        }

        metin2::log::info("Main loop exited, flushing caches");

        signal_timer_disable();

        for (auto &[pid, cache] : player_caches_)
            cache->flush();
        player_caches_.clear();

        for (auto &[id, cache] : item_caches_)
            cache->flush();
        item_caches_.clear();

        for (auto &[owner, cache] : item_price_list_cache_)
            cache->flush();
        item_price_list_cache_.clear();
    }

    void client_manager::query_boot(peer *peer, packet_gd_boot *p) {
        const uint8_t packet_version = 6; // Bump whenever the BOOT packet layout changes.

        std::vector<t_admin_info> v_admin;
        std::vector<std::string> v_host;

        host_info(v_host);
        admin_info(p->ip, v_admin);

        metin2::log::info("Boot: sending gm list to game server {}", p->ip);

        uint32_t packet_size =
            sizeof(uint32_t) +
            sizeof(uint8_t) +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(mob_table) * mob_tables_.size() +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(item_table) * item_tables_.size() +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(shop_table) * shop_table_size_ +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(skill_table) * skill_tables_.size() +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(refine_table) * refine_table_size_ +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(item_attr_table) * item_attr_tables_.size() +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(item_attr_table) * item_rare_tables_.size() +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(banword_table) * banword_tables_.size() +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(building::land_table) * land_tables_.size() +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(building::object_proto) * object_protos_.size() +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(building::object_table) * object_tables_.size() +
            sizeof(int32_t) +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(item_id_range_table) * 2 +
            sizeof(uint16_t) + sizeof(uint16_t) + 16 * v_host.size() +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(t_admin_info) * v_admin.size() +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(monarch_info) +
            sizeof(uint16_t) + sizeof(uint16_t) + sizeof(monarch_candidacy) * get_monarch().monarch_candidacy_size() +
            sizeof(uint16_t);

        peer->encode_header(HEADER_DG_BOOT, 0, packet_size);
        peer->encode(&packet_size, sizeof(uint32_t));
        peer->encode(&packet_version, sizeof(uint8_t));

        metin2::log::debug("Boot: packet size {:d}", packet_size);
        metin2::log::debug("Boot: packet version {:d}", packet_version);

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
        metin2::log::debug("Boot: gm list size {:d}", sizeof(t_admin_info) * v_admin.size());
        metin2::log::debug("Boot: monarch info size {:d}", sizeof(monarch_info));

        peer->encode_word(sizeof(mob_table));
        peer->encode_word(mob_tables_.size());
        peer->encode(mob_tables_.data(), sizeof(mob_table) * mob_tables_.size());

        peer->encode_word(sizeof(item_table));
        peer->encode_word(item_tables_.size());
        peer->encode(item_tables_.data(), sizeof(item_table) * item_tables_.size());

        peer->encode_word(sizeof(shop_table));
        peer->encode_word(shop_table_size_);
        peer->encode(shop_table_.get(), sizeof(shop_table) * shop_table_size_);

        peer->encode_word(sizeof(skill_table));
        peer->encode_word(skill_tables_.size());
        peer->encode(skill_tables_.data(), sizeof(skill_table) * skill_tables_.size());

        peer->encode_word(sizeof(refine_table));
        peer->encode_word(refine_table_size_);
        peer->encode(refine_table_.get(), sizeof(refine_table) * refine_table_size_);

        peer->encode_word(sizeof(item_attr_table));
        peer->encode_word(item_attr_tables_.size());
        peer->encode(item_attr_tables_.data(), sizeof(item_attr_table) * item_attr_tables_.size());

        peer->encode_word(sizeof(item_attr_table));
        peer->encode_word(item_rare_tables_.size());
        peer->encode(item_rare_tables_.data(), sizeof(item_attr_table) * item_rare_tables_.size());

        peer->encode_word(sizeof(banword_table));
        peer->encode_word(banword_tables_.size());
        peer->encode(banword_tables_.data(), sizeof(banword_table) * banword_tables_.size());

        peer->encode_word(sizeof(building::land_table));
        peer->encode_word(land_tables_.size());
        peer->encode(land_tables_.data(), sizeof(building::land_table) * land_tables_.size());

        peer->encode_word(sizeof(building::object_proto));
        peer->encode_word(object_protos_.size());
        peer->encode(object_protos_.data(), sizeof(building::object_proto) * object_protos_.size());

        peer->encode_word(sizeof(building::object_table));
        peer->encode_word(object_tables_.size());

        auto it = object_tables_.begin();

        while (it != object_tables_.end())
            peer->encode((it++)->second.get(), sizeof(building::object_table));

        int32_t now = time(nullptr);
        peer->encode(&now, sizeof(int32_t));

        item_id_range_table item_range = get_item_id_range_manager().range();
        item_id_range_table item_range_spare = get_item_id_range_manager().range();

        peer->encode_word(sizeof(item_id_range_table));
        peer->encode_word(1);
        peer->encode(&item_range, sizeof(item_id_range_table));
        peer->encode(&item_range_spare, sizeof(item_id_range_table));

        peer->set_item_id_range(item_range);
        peer->set_spare_item_id_range(item_range_spare);

        peer->encode_word(16);
        peer->encode_word(v_host.size());

        for (size_t n = 0; n < v_host.size(); ++n) {
            peer->encode(v_host[n].c_str(), 16);
            metin2::log::debug("Boot: gm host {}", v_host[n]);
        }

        peer->encode_word(sizeof(t_admin_info));
        peer->encode_word(v_admin.size());

        for (size_t n = 0; n < v_admin.size(); ++n) {
            peer->encode(&v_admin[n], sizeof(t_admin_info));
            metin2::log::debug("Boot: gm {} (contact ip {})", v_admin[n].name, v_admin[n].contact_ip);
        }

        peer->encode_word(sizeof(monarch_info));
        peer->encode_word(1);
        peer->encode(get_monarch().info(), sizeof(monarch_info));

        monarch::candidacy_list &monarch_candidacies = get_monarch().candidacies();

        size_t num_monarch_candidacy = get_monarch().monarch_candidacy_size();
        peer->encode_word(sizeof(monarch_candidacy));
        peer->encode_word(num_monarch_candidacy);
        if (num_monarch_candidacy != 0) {
            peer->encode(monarch_candidacies.data(), sizeof(monarch_candidacy) * num_monarch_candidacy);
        }

        if (metin2::config::test_server())
            metin2::log::debug("Boot: {:d} monarch candidates", get_monarch().monarch_candidacy_size());

        peer->encode_word(0xffff);
    }

    void client_manager::send_party_on_setup(peer *peer_value) {
        party_map &pm = channel_parties_[peer_value->channel()];

        for (auto party_it = pm.begin(); party_it != pm.end(); ++party_it) {
            metin2::log::debug("Party: sending party of leader pid {:d} to new peer", party_it->first);
            peer_value->encode_header(HEADER_DG_PARTY_CREATE, 0, sizeof(packet_party_create));
            peer_value->encode(&party_it->first, sizeof(uint32_t));

            for (auto member_it = party_it->second.begin(); member_it != party_it->second.end(); ++member_it) {
                metin2::log::debug("Party: sending party of leader pid {:d}, member pid {:d}", party_it->first, member_it->first);
                peer_value->encode_header(HEADER_DG_PARTY_ADD, 0, sizeof(packet_party_add));
                peer_value->encode(&party_it->first, sizeof(uint32_t));
                peer_value->encode(&member_it->first, sizeof(uint32_t));
                peer_value->encode(&member_it->second.role, sizeof(uint8_t));

                peer_value->encode_header(HEADER_DG_PARTY_SET_MEMBER_LEVEL, 0, sizeof(packet_party_set_member_level));
                peer_value->encode(&party_it->first, sizeof(uint32_t));
                peer_value->encode(&member_it->first, sizeof(uint32_t));
                peer_value->encode(&member_it->second.level, sizeof(uint8_t));
            }
        }
    }

    void client_manager::query_quest_save(peer *peer_value, quest_table *table, uint32_t len) {
        if (0 != (len % sizeof(quest_table))) {
            metin2::log::error("Quest save packet size {:d} is not a multiple of the quest entry size {:d}", len, sizeof(quest_table));
            return;
        }

        int size = len / sizeof(quest_table);

        std::string query;

        for (int i = 0; i < size; ++i, ++table) {
            if (table->value == 0) {
                query = sql::format(
                    "DELETE FROM quest WHERE dwPID={} AND szName={} AND szState={}", table->pid, table->name, table->state);
            } else {
                query = sql::format(
                    "REPLACE INTO quest (dwPID, szName, szState, lValue) VALUES({}, {}, {}, {})", table->pid, table->name, table->state, table->value);
            }

            get_db_manager().return_query(query, [](sql_msg &) {});
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
            "SELECT account_id, size, password FROM safebox WHERE account_id={}", packet->id);

        if (metin2::config::db().login_log)
            metin2::log::debug("Safebox: load request, handle {:d}, account {:d}, mall {:d}", handle_value, packet->id, mall ? 1 : 0);

        get_db_manager().return_query(query, [this, handle = peer_value->handle(), pi = std::move(pi)](sql_msg &msg) mutable {
            if (auto *peer_value = get_peer(handle))
                result_safebox_load(peer_value, msg, std::move(pi));
        });
    }

    void client_manager::result_safebox_load(peer *peer_value, sql_msg &msg, std::unique_ptr<client_handle_info> pi) {
        metin2::log::debug("Safebox load query finished");

        uint32_t handle_value = pi->handle;

        // account_index carries the query stage here: 0 checks the password, 1 loads the items.

        if (pi->account_index == 0) {
            std::string safebox_password = pi->safebox_password;

            auto safebox_value = std::make_unique<safebox_table>();
            memset(safebox_value.get(), 0, sizeof(safebox_table));

            sql_result *res = msg.get();

            if (res->num_rows == 0) {
                if (safebox_password != "000000") {
                    peer_value->encode_header(HEADER_DG_SAFEBOX_WRONG_PASSWORD, handle_value, 0);
                    return;
                }
            } else {
                MYSQL_ROW row = mysql_fetch_row(res->res.get());

                if (((!row[2] || !*row[2]) && safebox_password != "000000") ||
                    ((row[2] && *row[2]) && safebox_password != row[2])) {
                    peer_value->encode_header(HEADER_DG_SAFEBOX_WRONG_PASSWORD, handle_value, 0);
                    return;
                }

                if (!row[0])
                    safebox_value->id = 0;
                else
                    str_to_number(safebox_value->id, row[0]);

                if (!row[1])
                    safebox_value->size = 0;
                else
                    str_to_number(safebox_value->size, row[1]);
                if (pi->ip[0] == 1) {
                    safebox_value->size = 1;
                    metin2::log::debug("Safebox: loaded mall of account {:d}, size {:d}", safebox_value->id, safebox_value->size);
                } else
                    metin2::log::debug("Safebox: loaded safebox of account {:d}, size {:d}", safebox_value->id, safebox_value->size);
            }

            if (0 == safebox_value->id)
                safebox_value->id = pi->account_id;

            pi->safebox = std::move(safebox_value);

            const std::string query = sql::format("SELECT {} FROM item WHERE owner_id={} AND window={}",
                sql::raw(item_select_columns), pi->account_id, pi->ip[0] == 0 ? "SAFEBOX" : "MALL");

            pi->account_index = 1;

            get_db_manager().return_query(query, [this, handle = peer_value->handle(), pi = std::move(pi)](sql_msg &msg) mutable {
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

            std::set<db::items::item_award *> *set = get_item_award_manager().by_login(pi->login);

            if (set && !item_tables_.empty()) {

                grid grid(5, std::max<int>(1, pi->safebox->size) * 9);
                bool escape = false;

                for (uint32_t i = 0; i < s_items.size(); ++i) {
                    player_item &r = s_items[i];

                    auto it = item_tables_by_vnum_.find(r.vnum);

                    if (it == item_tables_by_vnum_.end()) {
                        escape = true;
                        metin2::log::error("Safebox: unknown item vnum {:d} for account {}", r.vnum, pi->login);
                        break;
                    }

                    grid.put(r.pos, 1, it->second->size);
                }

                if (!escape) {
                    std::vector<std::pair<uint32_t, uint64_t>> finished_award_ids;

                    auto it = set->begin();

                    std::string query;

                    while (it != set->end()) {
                        db::items::item_award *item_award = *(it++);
                        const uint32_t &item_vnum = item_award->vnum;

                        if (item_award->taken)
                            continue;

                        if (pi->ip[0] == 0 && item_award->mall)
                            continue;

                        if (pi->ip[0] == 1 && !item_award->mall)
                            continue;

                        auto it = item_tables_by_vnum_.find(item_award->vnum);

                        if (it == item_tables_by_vnum_.end()) {
                            metin2::log::error("Item award: unknown item vnum {:d} for account {}", item_award->vnum, pi->login);
                            continue;
                        }

                        item_table *item_table_value = it->second;

                        int pos_value;

                        if ((pos_value = grid.find_blank(1, it->second->size)) == -1)
                            break;

                        player_item item;
                        memset(&item, 0, sizeof(player_item));

                        uint32_t socket2_value = item_award->socket2;

                        if (item_table_value->type == ITEM_UNIQUE) {
                            if (item_award->socket2 != 0)
                                socket2_value = item_award->socket2;
                            else
                                socket2_value = item_table_value->values[0];
                        } else if ((item_vnum == 50300 || item_vnum == 70037) && item_award->socket0 == 0) {
                            std::vector<uint32_t> skill_vnums;
                            for (const skill_table &skill : skill_tables_) {
                                if (skill.vnum <= 120)
                                    skill_vnums.push_back(skill.vnum);
                            }

                            if (!skill_vnums.empty())
                                item_award->socket0 = skill_vnums[number(0, static_cast<int>(skill_vnums.size()) - 1)];
                        } else {
                            switch (item_vnum) {
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
                                if (item_award->socket2 == 0) {
                                    socket2_value = item_table_value->values[0];
                                } else {
                                    socket2_value = item_award->socket2;
                                }
                                break;
                            }
                        }

                        if (item_id() > item_range_.max) {
                            metin2::log::error("Item award: item id range exhausted");
                            break;
                        }

                        {
                            auto it = item_tables_by_vnum_.find(item_vnum);
                            if (it == item_tables_by_vnum_.end()) {
                                metin2::log::error("Item award: unknown item vnum {:d}", item_vnum);
                                continue;
                            }
                            item_table *item_table = it->second;
                            if (item_table == nullptr) {
                                metin2::log::error("Item award: item vnum {:d} has no item table entry", item_vnum);
                                continue;
                            }
                            if (0 == item_award->socket0) {
                                for (int i = 0; i < ITEM_LIMIT_MAX_NUM; i++) {
                                    if (LIMIT_REAL_TIME == item_table->limits[i].type) {
                                        if (0 == item_table->limits[i].value)
                                            item_award->socket0 = time(nullptr) + 60 * 60 * 24 * 7;
                                        else
                                            item_award->socket0 = time(nullptr) + item_table->limits[i].value;

                                        break;
                                    } else if (LIMIT_REAL_TIME_START_FIRST_USE == item_table->limits[i].type || LIMIT_TIMER_BASED_ON_WEAR == item_table->limits[i].type) {
                                        if (0 == item_table->limits[i].value)
                                            item_award->socket0 = 60 * 60 * 24 * 7;
                                        else
                                            item_award->socket0 = item_table->limits[i].value;

                                        break;
                                    }
                                }
                            }

                            query = sql::format(
                                "INSERT INTO item (id, owner_id, window, pos, vnum, count, socket0, socket1, socket2) "
                                "VALUES({}, {}, {}, {}, {}, {}, {}, {}, {})",
                                gain_item_id(),
                                pi->account_id,
                                pi->ip[0] == 0 ? "SAFEBOX" : "MALL",
                                pos_value,
                                item_award->vnum, item_award->count, item_award->socket0, item_award->socket1, socket2_value);
                        }

                        auto pmsg = get_db_manager().direct_query(query);
                        sql_result *res = pmsg->get();
                        metin2::log::debug("Item award: {}", query);

                        if (res->affected_rows == 0 || res->insert_id == 0 || res->affected_rows == (uint32_t)-1)
                            break;

                        item.id = pmsg->get()->insert_id;
                        item.window = pi->ip[0] == 0 ? SAFEBOX : MALL,
                        item.pos = pos_value;
                        item.count = item_award->count;
                        item.vnum = item_award->vnum;
                        item.sockets[0] = item_award->socket0;
                        item.sockets[1] = item_award->socket1;
                        item.sockets[2] = socket2_value;
                        s_items.push_back(item);

                        finished_award_ids.push_back(std::make_pair(item_award->id, item.id));
                        grid.put(pos_value, 1, it->second->size);
                    }

                    for (uint32_t i = 0; i < finished_award_ids.size(); ++i)
                        get_item_award_manager().taken(finished_award_ids[i].first, finished_award_ids[i].second);
                }
            }

            pi->safebox->item_count = s_items.size();

            peer_value->encode_header(pi->ip[0] == 0 ? HEADER_DG_SAFEBOX_LOAD : HEADER_DG_MALL_LOAD, handle_value, sizeof(safebox_table) + sizeof(player_item) * s_items.size());

            peer_value->encode(pi->safebox.get(), sizeof(safebox_table));

            if (!s_items.empty())
                peer_value->encode(s_items.data(), sizeof(player_item) * s_items.size());
        }
    }

    void client_manager::query_safebox_change_size(peer *peer_value, uint32_t handle_value, safebox_change_size_packet *p) {
        auto pi = std::make_unique<client_handle_info>(handle_value);
        pi->account_index = p->size; // account_index carries the safebox size here.

        const std::string query = p->size == 1
                                      ? sql::format("INSERT INTO safebox (account_id, size) VALUES({}, {})", p->id, p->size)
                                      : sql::format("UPDATE safebox SET size={} WHERE account_id={}", p->size, p->id);

        get_db_manager().return_query(query, [this, handle = peer_value->handle(), pi = std::move(pi)](sql_msg &msg) {
            if (auto *peer_value = get_peer(handle))
                result_safebox_change_size(peer_value, msg, *pi);
        });
    }

    void client_manager::result_safebox_change_size(peer *peer_value, sql_msg &msg, client_handle_info &p) {
        metin2::log::debug("Safebox resize query finished");

        uint32_t handle = p.handle;
        uint8_t size = p.account_index;

        if (msg.get()->num_rows > 0) {
            peer_value->encode_header(HEADER_DG_SAFEBOX_CHANGE_SIZE, handle, sizeof(uint8_t));
            peer_value->encode_byte(size);
        }
    }

    void client_manager::query_safebox_change_password(peer *peer_value, uint32_t handle_value, safebox_change_password_packet *p) {
        auto pi = std::make_unique<client_handle_info>(handle_value);
        strlcpy(pi->safebox_password, p->new_password, sizeof(pi->safebox_password));
        strlcpy(pi->login, p->old_password, sizeof(pi->login));
        pi->account_id = p->id;

        const std::string query = sql::format("SELECT password FROM safebox WHERE account_id={}", p->id);

        get_db_manager().return_query(query, [this, handle = peer_value->handle(), pi = std::move(pi)](sql_msg &msg) mutable {
            if (auto *peer_value = get_peer(handle))
                result_safebox_change_password(peer_value, msg, std::move(pi));
        });
    }

    void client_manager::result_safebox_change_password(peer *peer_value, sql_msg &msg, std::unique_ptr<client_handle_info> p) {
        metin2::log::debug("Safebox password change query finished");

        uint32_t handle_value = p->handle;

        if (msg.get()->num_rows > 0) {
            MYSQL_ROW row = mysql_fetch_row(msg.get()->res.get());

            if ((row[0] && *row[0] && !strcasecmp(row[0], p->login)) || ((!row[0] || !*row[0]) && !strcmp("000000", p->login))) {
                const std::string query = sql::format("UPDATE safebox SET password={} WHERE account_id={}", p->safebox_password, p->account_id);

                get_db_manager().return_query(query, [this, handle = peer_value->handle(), p = std::move(p)](sql_msg &msg) {
                    if (auto *peer_value = get_peer(handle))
                        result_safebox_change_password_second(peer_value, msg, *p);
                });
                return;
            }
        }

        peer_value->encode_header(HEADER_DG_SAFEBOX_CHANGE_PASSWORD_ANSWER, handle_value, sizeof(uint8_t));
        peer_value->encode_byte(0);
    }

    void client_manager::result_safebox_change_password_second(peer *peer_value, sql_msg &msg, client_handle_info &p) {
        metin2::log::debug("Safebox password change query (second step) finished");

        uint32_t handle = p.handle;

        peer_value->encode_header(HEADER_DG_SAFEBOX_CHANGE_PASSWORD_ANSWER, handle, sizeof(uint8_t));
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

        size_t size_price_list_size = sizeof(item_price_info) * header.count;

        peer->encode_header(HEADER_DG_MYSHOP_PRICELIST_RES, req_info.first, sizeof(header) + size_price_list_size);
        peer->encode(&header, sizeof(header));
        peer->encode(table.price_info, size_price_list_size);

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
            "UPDATE safebox SET gold='{}' WHERE account_id={}", table->gold, table->id);

        get_db_manager().return_query(query, [](sql_msg &) {});
    }

    void client_manager::query_empire_select(peer *peer_value, uint32_t handle, empire_select_packet *p) {
        if (p->empire == 0 || p->empire >= EMPIRE_MAX_NUM) {
            metin2::log::warn("Empire select: invalid empire {:d} for account {:d}", p->empire, p->account_id);
            return;
        }

        const std::string query = sql::format("UPDATE player_index SET empire={} WHERE id={}", p->empire, p->account_id);
        get_db_manager().direct_query(query);

        metin2::log::debug("Empire select: {}", query);
        {
            const std::string select_query = sql::format(
                "SELECT pid1, pid2, pid3, pid4 FROM player_index WHERE id={}", p->account_id);

            auto pmsg = get_db_manager().direct_query(select_query);

            sql_result *res = pmsg->get();

            if (res->num_rows) {
                metin2::log::debug("Empire select: {:d} player_index rows", res->num_rows);

                MYSQL_ROW row = mysql_fetch_row(res->res.get());
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

                        const std::string move_query = sql::format("UPDATE player SET map_index={},x={},y={} WHERE id={}",
                            g_start_map[p->empire],
                            g_start_position[p->empire][0],
                            g_start_position[p->empire][1],
                            pids[i]);

                        auto pmsg2 = get_db_manager().direct_query(move_query);
                    }
                }
            }
        }

        peer_value->encode_header(HEADER_DG_EMPIRE_SELECT, handle, sizeof(uint8_t));
        peer_value->encode_byte(p->empire);
    }

    void client_manager::query_setup(peer *peer, uint32_t handle_value, const char *data) {
        auto *p = (packet_gd_setup *)data;
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

        const auto location_of = [](db::clients::peer *p) -> map_location {
            map_location location{};
            strlcpy(location.host, p->public_ip(), sizeof(location.host));
            location.port = p->listen_port();
            std::memcpy(location.maps, p->maps(), sizeof(location.maps));
            return location;
        };

        const map_location map_locations = location_of(peer);
        const uint8_t channel = peer->channel();

        const auto is_visible_to_new_peer = [channel](uint8_t other) -> bool {
            return other == channel || other == GUILD_WARP_WAR_CHANNEL || (channel == GUILD_WARP_WAR_CHANNEL && other == 1);
        };
        const auto learns_new_maps = [channel](uint8_t other) -> bool {
            return other == channel || channel == GUILD_WARP_WAR_CHANNEL || (channel == 1 && other == GUILD_WARP_WAR_CHANNEL);
        };

        uint8_t map_count;

        std::vector<map_location> locations;

        for (db::clients::peer *tmp : peer_list_) {
            if (tmp == peer || !tmp->channel())
                continue;

            if (is_visible_to_new_peer(tmp->channel()))
                locations.push_back(location_of(tmp));

            if (learns_new_maps(tmp->channel())) {
                tmp->encode_header(HEADER_DG_MAP_LOCATIONS, 0, sizeof(uint8_t) + sizeof(map_location));
                tmp->encode_byte(1);
                tmp->encode(&map_locations, sizeof(map_location));
            }
        }

        locations.push_back(map_locations);

        peer->encode_header(HEADER_DG_MAP_LOCATIONS, 0, sizeof(uint8_t) + sizeof(map_location) * locations.size());
        map_count = locations.size();
        peer->encode_byte(map_count);
        peer->encode(locations.data(), sizeof(map_location) * locations.size());

        metin2::log::info("Game server setup: channel {:d}, listen port {:d}, p2p port {:d}, {:d} maps", peer->channel(), p->listen_port, p->p2p_port, map_count);

        packet_dg_p2p p2_p_setup_packet;
        p2_p_setup_packet.port = peer->p2p_port();
        p2_p_setup_packet.channel = peer->channel();
        strlcpy(p2_p_setup_packet.host, peer->public_ip(), sizeof(p2_p_setup_packet.host));

        for (auto i = peer_list_.begin(); i != peer_list_.end(); ++i) {
            db::clients::peer *tmp = *i;

            if (tmp == peer)
                continue;

            // Channel 0: auth, or a peer whose SETUP has not arrived yet.
            if (0 == tmp->channel())
                continue;

            tmp->encode_header(HEADER_DG_P2P, 0, sizeof(packet_dg_p2p));
            tmp->encode(&p2_p_setup_packet, sizeof(packet_dg_p2p));
        }

        auto *pck = (packet_login_on_setup *)data;

        for (uint32_t c = 0; c < p->login_count; ++c, ++pck) {
            auto *ld = new login_data;

            ld->set_key(pck->login_key);
            ld->set_client_key(pck->client_key);

            account_table &r = ld->account_ref();

            r.id = pck->id;
            trim_and_lower(pck->login, r.login);
            strlcpy(r.social_id, pck->social_id, sizeof(r.social_id));
            strlcpy(r.passwd, "TEMP", sizeof(r.passwd));

            insert_login_data(ld);

            if (insert_logon_account(pck->login, peer->handle())) {
                metin2::log::info("Game server setup: restored login of account {:d} ({}), key {:d}, host {}", pck->id, pck->login, pck->login_key, pck->host);
                ld->set_play(true);
            } else
                metin2::log::warn("Game server setup: cannot restore login of account {:d} ({}), key {:d}, already logged in", pck->id, pck->login, pck->login_key);
        }

        send_party_on_setup(peer);
        get_guild_manager().on_setup(peer);
        get_priv_manager().send_priv_on_setup(peer);
        send_event_flags_on_setup(peer);
        marriage::get_manager().on_setup(peer);
    }

    void client_manager::query_item_flush(peer *peer_value, const char *data) {
        uint64_t id;
        std::memcpy(&id, data, sizeof(id));

        if (metin2::config::db().login_log)
            metin2::log::info("Item cache: flush request for item {:d}", id);

        item_cache *c = get_item_cache(id);

        if (c)
            c->flush();
    }

    void client_manager::query_item_save(peer *peer_value, const char *data) {
        auto *p = (player_item *)data;

        // Safebox and mall items are never cached; drop any that are.

        if (p->window == SAFEBOX || p->window == MALL) {
            item_cache *c = get_item_cache(p->id);

            if (c) {
                auto it = item_cache_set_ptrs_.find(c->get()->owner);

                if (it != item_cache_set_ptrs_.end()) {
                    if (metin2::config::test_server())
                        metin2::log::debug("Item cache: removing item {1:d} from owner {0:d}", c->get()->owner, c->get()->id);

                    it->second->erase(c);
                }

                item_caches_.erase(p->id);
            }
            const std::string query = create_item_save_query(*p);

            get_db_manager().return_query(query, [](sql_msg &) {});
        } else {
            if (metin2::config::test_server())
                metin2::log::debug("Item cache: caching item {1:d} (vnum {2:d}) of owner {0:d}", p->owner, p->id, p->vnum);

            put_item_cache(p);
        }
    }

    auto client_manager::get_item_cache_set(uint32_t pid) -> client_manager::item_cache_set * {
        auto it = item_cache_set_ptrs_.find(pid);

        if (it == item_cache_set_ptrs_.end())
            return nullptr;

        return it->second.get();
    }

    void client_manager::create_item_cache_set(uint32_t pid) {
        if (item_cache_set_ptrs_.find(pid) != item_cache_set_ptrs_.end())
            return;

        item_cache_set_ptrs_.insert(item_cache_set_ptr_map::value_type(pid, std::make_unique<item_cache_set>()));

        if (metin2::config::db().login_log)
            metin2::log::info("Item cache: created cache for pid {:d}", pid);
    }

    void client_manager::flush_item_cache_set(uint32_t pid) {
        auto it = item_cache_set_ptrs_.find(pid);

        if (it == item_cache_set_ptrs_.end()) {
            metin2::log::debug("Item cache: no cache to flush for pid {:d}", pid);
            return;
        }

        item_cache_set *set = it->second.get();
        auto it_set = set->begin();

        while (it_set != set->end()) {
            item_cache *c = *it_set++;
            c->flush();

            item_caches_.erase(c->get()->id);
        }

        set->clear();

        item_cache_set_ptrs_.erase(it);

        if (metin2::config::db().login_log)
            metin2::log::info("Item cache: flushed and removed cache of pid {:d}", pid);
    }

    auto client_manager::get_item_cache(uint64_t id) -> item_cache * {
        auto it = item_caches_.find(id);

        if (it == item_caches_.end())
            return nullptr;

        return it->second.get();
    }

    void client_manager::put_item_cache(player_item *updated, bool skip_query) {
        item_cache *c;

        c = get_item_cache(updated->id);

        if (!c) {
            if (metin2::config::db().login_log)
                metin2::log::info("Item cache: new entry for item {:d} (vnum {:d}), owner {:d}", updated->id, updated->vnum, updated->owner);

            c = item_caches_.emplace(updated->id, std::make_unique<item_cache>()).first->second.get();
        } else {
            if (metin2::config::db().login_log)
                metin2::log::info("Item cache: updating item {:d}", updated->id);
            if (updated->owner != c->get()->owner) {
                auto it = item_cache_set_ptrs_.find(c->get()->owner);

                if (it != item_cache_set_ptrs_.end()) {
                    if (metin2::config::db().login_log)
                        metin2::log::info("Item cache: item {1:d} moved from owner {0:d} to {2:d}", c->get()->owner, c->get()->id, updated->owner);
                    it->second->erase(c);
                }
            }
        }

        c->put(updated, skip_query);

        auto it = item_cache_set_ptrs_.find(c->get()->owner);

        if (it != item_cache_set_ptrs_.end()) {
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

    auto client_manager::delete_item_cache(uint64_t id) -> bool {
        item_cache *c = get_item_cache(id);

        if (!c)
            return false;

        c->remove();
        return true;
    }

    auto client_manager::item_price_list_cache(uint32_t id) -> item_price_list_table_cache * {
        auto it = item_price_list_cache_.find(id);

        if (it == item_price_list_cache_.end())
            return nullptr;

        return it->second.get();
    }

    void client_manager::put_item_price_list_cache(const item_price_list_table *item_price_list) {
        item_price_list_table_cache *cache = item_price_list_cache(item_price_list->owner_id);

        if (!cache) {
            cache = item_price_list_cache_.emplace(item_price_list->owner_id, std::make_unique<item_price_list_table_cache>()).first->second.get();
        }

        cache->put(const_cast<item_price_list_table *>(item_price_list), true);
    }

    void client_manager::update_player_cache() {
        auto it = player_caches_.begin();

        while (it != player_caches_.end()) {
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

        auto it = item_caches_.begin();

        while (it != item_caches_.end()) {
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
        auto it = item_price_list_cache_.begin();

        while (it != item_price_list_cache_.end()) {
            item_price_list_table_cache *cache = it->second.get();

            if (cache->check_flush_timeout()) {
                cache->flush();
                item_price_list_cache_.erase(it++);
            } else
                ++it;
        }
    }

    void client_manager::query_item_destroy(peer *peer_value, const char *data) {
        uint64_t id_value;
        uint32_t pid_value;
        std::memcpy(&id_value, data, sizeof(id_value));
        std::memcpy(&pid_value, data + sizeof(id_value), sizeof(pid_value));

        if (!delete_item_cache(id_value)) {
            const std::string query = sql::format("DELETE FROM item WHERE id={}", id_value);

            if (metin2::config::db().login_log)
                metin2::log::info("Item cache: deleting item {1:d} of pid {0:d}", pid_value, id_value);

            if (pid_value == 0)
                get_db_manager().async_query(query);
            else
                get_db_manager().return_query(query, [](sql_msg &) {});
        }
    }

    void client_manager::query_flush_cache(peer *peer_value, const char *data) {
        uint32_t pid_value = *(uint32_t *)data;

        player_table_cache *cache_value = player_cache(pid_value);

        if (!cache_value)
            return;

        metin2::log::info("Player cache: flushing pid {:d}", pid_value);

        cache_value->flush();
        flush_item_cache_set(pid_value);

        player_caches_.erase(pid_value);
    }

    void client_manager::query_sms(peer *peer_value, packet_gdsms *pack) {
        const std::string query = sql::format(
            "INSERT INTO sms_pool (server, sender, receiver, mobile, msg) VALUES({}, {}, {}, {}, {})",
            (player_id_start_ + 2) / 3, pack->from, pack->to, pack->mobile, pack->msg);

        get_db_manager().async_query(query);
    }

    void client_manager::query_reload_proto() {
        if (!initialize_tables()) {
            metin2::log::error("Proto reload: cannot load tables");
            return;
        }

        for (auto i = peer_list_.begin(); i != peer_list_.end(); ++i) {
            peer *tmp = *i;

            if (!tmp->channel())
                continue;

            tmp->encode_header(HEADER_DG_RELOAD_PROTO, 0,
                sizeof(uint16_t) + sizeof(skill_table) * skill_tables_.size() +
                    sizeof(uint16_t) + sizeof(banword_table) * banword_tables_.size() +
                    sizeof(uint16_t) + sizeof(item_table) * item_tables_.size() +
                    sizeof(uint16_t) + sizeof(mob_table) * mob_tables_.size());

            tmp->encode_word(skill_tables_.size());
            tmp->encode(skill_tables_.data(), sizeof(skill_table) * skill_tables_.size());

            tmp->encode_word(banword_tables_.size());
            tmp->encode(banword_tables_.data(), sizeof(banword_table) * banword_tables_.size());

            tmp->encode_word(item_tables_.size());
            tmp->encode(item_tables_.data(), sizeof(item_table) * item_tables_.size());

            tmp->encode_word(mob_tables_.size());
            tmp->encode(mob_tables_.data(), sizeof(mob_table) * mob_tables_.size());
        }
    }

    void client_manager::add_guild_priv(packet_give_guild_priv *p) {
        get_priv_manager().add_guild_priv(p->guild_id, p->type, p->value, p->duration_sec);
    }

    void client_manager::add_empire_priv(packet_give_empire_priv *p) {
        get_priv_manager().add_empire_priv(p->empire, p->type, p->value, p->duration_sec);
    }

    void client_manager::add_character_priv(packet_give_character_priv *p) {
        get_priv_manager().add_char_priv(p->pid, p->type, p->value);
    }

    void client_manager::money_log(packet_money_log *p) {
        db::logs::get_money_log().add_log(p->type, p->vnum, p->gold);
    }

    auto client_manager::get_login_data(uint32_t key) -> login_data * {
        auto it = login_data_.find(key);

        if (it == login_data_.end())
            return nullptr;

        return it->second;
    }

    auto client_manager::get_login_data_by_login(const char *login) -> login_data * {
        char login_value[LOGIN_MAX_LEN + 1];
        trim_and_lower(login, login_value);

        auto it = login_data_by_login_.find(login_value);

        if (it == login_data_by_login_.end())
            return nullptr;

        return it->second;
    }

    auto client_manager::get_login_data_by_aid(uint32_t aid) -> login_data * {
        auto it = login_data_by_aid_.find(aid);

        if (it == login_data_by_aid_.end())
            return nullptr;

        return it->second;
    }

    void client_manager::insert_login_data(login_data *ld) {
        char login[LOGIN_MAX_LEN + 1];
        trim_and_lower(ld->account_ref().login, login);

        login_data_.insert(std::make_pair(ld->key(), ld));
        login_data_by_login_.insert(std::make_pair(login, ld));
        login_data_by_aid_.insert(std::make_pair(ld->account_ref().id, ld));
    }

    void client_manager::delete_login_data(login_data *ld) {
        login_data_.erase(ld->key());
        login_data_by_login_.erase(ld->account_ref().login);
        login_data_by_aid_.erase(ld->account_ref().id);

        if (logon_accounts_.find(ld->account_ref().login) == logon_accounts_.end())
            delete ld;
        else
            ld->set_deleted(true);
    }

    void client_manager::query_auth_login(peer *peer_value, uint32_t handle, packet_gd_auth_login *p) {
        if (metin2::config::test_server())
            metin2::log::debug("Auth login: account {:d}, key {:d}, login {}", p->id, p->login_key, p->login);
        login_data *ld = get_login_data_by_login(p->login);

        if (ld) {
            delete_login_data(ld);
        }

        uint8_t result;

        if (get_login_data(p->login_key)) {
            metin2::log::warn("Auth login: login key {:d} already in use (account {})", p->login_key, p->login);
            result = 0;

            peer_value->encode_header(HEADER_DG_AUTH_LOGIN, handle, sizeof(uint8_t));
            peer_value->encode_byte(result);
        } else {
            auto *ld = new login_data;

            ld->set_key(p->login_key);
            ld->set_client_key(p->client_key);
            ld->set_premium(p->premium_times);

            account_table &r = ld->account_ref();

            r.id = p->id;
            trim_and_lower(p->login, r.login);
            strlcpy(r.social_id, p->social_id, sizeof(r.social_id));
            strlcpy(r.passwd, "TEMP", sizeof(r.passwd));

            metin2::log::info("Auth login: account {:d} ({}), social id {}, key {:d}, client key {:d} {:d} {:d} {:d}",
                p->id, p->login, p->social_id, p->login_key,
                p->client_key[0], p->client_key[1], p->client_key[2], p->client_key[3]);

            result = 1;

            insert_login_data(ld);

            peer_value->encode_header(HEADER_DG_AUTH_LOGIN, handle, sizeof(uint8_t));
            peer_value->encode_byte(result);
        }
    }

    void client_manager::guild_deposit_money(packet_gd_guild_money *p) {
        get_guild_manager().deposit_money(p->guild, p->gold);
    }

    void client_manager::guild_withdraw_money(peer *peer, packet_gd_guild_money *p) {
        get_guild_manager().withdraw_money(peer, p->guild, p->gold);
    }

    void client_manager::guild_withdraw_money_give_reply(packet_gd_guild_money_withdraw_give_reply *p) {
        get_guild_manager().withdraw_money_reply(p->guild, p->give_success, p->change_gold);
    }

    void client_manager::guild_war_bet(packet_gd_guild_war_bet *p) {
        get_guild_manager().bet(p->war_id, p->login, p->gold, p->guild);
    }

    void client_manager::create_object(packet_gd_create_object *p) {
        using namespace building;

        const std::string query = sql::format(
            "INSERT INTO object (land_id, vnum, map_index, x, y, x_rot, y_rot, z_rot) VALUES({}, {}, {}, {}, {}, {:f}, {:f}, {:f})", p->land_id, p->vnum, p->map_index, p->x, p->y, p->x_rot, p->y_rot, p->z_rot);

        auto pmsg = get_db_manager().direct_query(query);

        if (pmsg->get()->insert_id == 0) {
            metin2::log::error("Cannot insert building object (vnum {:d}, land {:d})", p->vnum, p->land_id);
            return;
        }

        auto obj = std::make_unique<object_table>();

        memset(obj.get(), 0, sizeof(object_table));

        obj->id = pmsg->get()->insert_id;
        obj->vnum = p->vnum;
        obj->land_id = p->land_id;
        obj->map_index = p->map_index;
        obj->x = p->x;
        obj->y = p->y;
        obj->x_rot = p->x_rot;
        obj->y_rot = p->y_rot;
        obj->z_rot = p->z_rot;
        obj->life = 0;

        forward_packet(HEADER_DG_CREATE_OBJECT, obj.get(), sizeof(object_table));

        object_tables_.insert(std::make_pair(obj->id, std::move(obj)));
    }

    void client_manager::delete_object(uint32_t id) {
        const std::string query = sql::format("DELETE FROM object WHERE id={}", id);

        auto pmsg = get_db_manager().direct_query(query);

        if (pmsg->get()->affected_rows == 0 || pmsg->get()->affected_rows == (uint32_t)-1) {
            metin2::log::warn("Cannot delete building object {:d}, not found", id);
            return;
        }

        auto it = object_tables_.find(id);

        if (it != object_tables_.end()) {
            object_tables_.erase(it);
        }

        forward_packet(HEADER_DG_DELETE_OBJECT, &id, sizeof(uint32_t));
    }

    void client_manager::update_land(uint32_t *pdw) {
        uint32_t id_value = pdw[0];
        uint32_t guild = pdw[1];

        const auto p = std::ranges::find(land_tables_, id_value, &building::land_table::id);
        if (p == land_tables_.end())
            return;

        const std::string query = sql::format("UPDATE land SET guild_id={} WHERE id={}", guild, id_value);
        get_db_manager().async_query(query);

        p->guild_id = guild;
        forward_packet(HEADER_DG_UPDATE_LAND, &*p, sizeof(building::land_table));
    }

    void client_manager::block_chat(packet_block_chat *p) {
        const std::string query = sql::format("SELECT id FROM player WHERE name = {}", p->name);
        auto pmsg = get_db_manager().direct_query(query);
        sql_result *res = pmsg->get();

        if (res->num_rows) {
            MYSQL_ROW row = mysql_fetch_row(res->res.get());
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
        marriage::get_manager().add(p->pid1, p->pid2, p->name1, p->name2);
    }

    void client_manager::marriage_update(packet_marriage_update *p) {
        metin2::log::info("Marriage: update pid {:d} and pid {:d}, love points {:d}, married {:d}", p->pid1, p->pid2, p->love_point, p->married);
        marriage::get_manager().update(p->pid1, p->pid2, p->love_point, p->married);
    }

    void client_manager::marriage_remove(packet_marriage_remove *p) {
        metin2::log::info("Marriage: remove pid {:d} and pid {:d}", p->pid1, p->pid2);
        marriage::get_manager().remove(p->pid1, p->pid2);
    }

    void client_manager::wedding_request(packet_wedding_request *p) {
        metin2::log::info("Marriage: wedding requested by pid {:d} and pid {:d}", p->pid1, p->pid2);
        forward_packet(HEADER_DG_WEDDING_REQUEST, p, sizeof(packet_wedding_request));
    }

    void client_manager::wedding_ready(packet_wedding_ready *p) {
        metin2::log::info("Marriage: wedding of pid {:d} and pid {:d} is ready", p->pid1, p->pid2);
        forward_packet(HEADER_DG_WEDDING_READY, p, sizeof(packet_wedding_ready));
        marriage::get_manager().ready_wedding(p->map_index, p->pid1, p->pid2);
    }

    void client_manager::wedding_end(packet_wedding_end *p) {
        metin2::log::info("Marriage: wedding of pid {:d} and pid {:d} ended", p->pid1, p->pid2);
        marriage::get_manager().end_wedding(p->pid1, p->pid2);
    }

    void client_manager::myshop_pricelist_update(const item_price_list_table *packet) {
        if (packet->count > SHOP_PRICELIST_MAX_NUM) {
            metin2::log::warn("Price list update for owner pid {:d} has too many entries ({:d}), ignored", packet->owner_id, packet->count);
            return;
        }

        item_price_list_table_cache *cache = item_price_list_cache(packet->owner_id);

        if (cache) {
            item_price_list_table table;

            table.owner_id = packet->owner_id;
            table.count = packet->count;

            std::memcpy(table.price_info, packet->price_info, sizeof(item_price_info) * packet->count);

            cache->update_list(&table);
        } else {
            auto update_table_value = std::make_unique<item_price_list_table>();

            update_table_value->owner_id = packet->owner_id;
            update_table_value->count = packet->count;

            std::memcpy(update_table_value->price_info, packet->price_info, sizeof(item_price_info) * packet->count);

            const std::string query = sql::format("SELECT item_vnum, price FROM myshop_pricelist WHERE owner_id={}", packet->owner_id);
            get_db_manager().return_query(query, [this, update_table = std::move(update_table_value)](sql_msg &msg) {
                result_pricelist_load_for_update(msg, *update_table);
            });
        }
    }

    void client_manager::myshop_pricelist_request(peer *peer, uint32_t handle_value, uint32_t player_id) {
        if (item_price_list_table_cache *cache = item_price_list_cache(player_id)) {
            metin2::log::debug("Price list: sending cached list for handle {:d}, pid {:d}", handle_value, player_id);

            item_price_list_table *table = cache->get(false);

            packet_myshop_pricelist_header header =
                {
                    table->owner_id,
                    table->count};

            size_t size_price_list_size = sizeof(item_price_info) * table->count;

            peer->encode_header(HEADER_DG_MYSHOP_PRICELIST_RES, handle_value, sizeof(header) + size_price_list_size);
            peer->encode(&header, sizeof(header));
            peer->encode(table->price_info, size_price_list_size);

        } else {
            metin2::log::debug("Price list: loading list for handle {:d}, pid {:d}", handle_value, player_id);

            const std::string query = sql::format("SELECT item_vnum, price FROM myshop_pricelist WHERE owner_id={}", player_id);
            get_db_manager().return_query(query, [this, handle = peer->handle(), req_info = item_pricelist_req_info(handle_value, player_id)](sql_msg &msg) {
                if (auto *peer_value = get_peer(handle))
                    result_pricelist_load(peer_value, msg, req_info);
            });
        }
    }

    void client_manager::process_packets(peer *peer) {
        uint8_t header;
        uint32_t handle_value;
        uint32_t length_value;
        const char *data = nullptr;
        int i = 0;
        int count = 0;

        while (peer->peek_packet(i, header, handle_value, length_value, &data)) {
            ++count;

            if (metin2::config::test_server()) {
                if (header != HEADER_GD_PLAYER_COUNT)
                    metin2::log::debug("Packet header {:d}, handle {:d}, length {:d}, #{:d} in batch", header, handle_value, length_value, count);
            }

            switch (header) {
            case HEADER_GD_BOOT:
                query_boot(peer, (packet_gd_boot *)data);
                break;

            case HEADER_GD_LOGIN_BY_KEY:
                query_login_by_key(peer, handle_value, (packet_gd_login_by_key *)data);
                break;

            case HEADER_GD_LOGOUT:
                query_logout(peer, handle_value, data);
                break;

            case HEADER_GD_PLAYER_LOAD:
                metin2::log::debug("Player load request (handle {:d}, length {:d})", handle_value, length_value);
                query_player_load(peer, handle_value, (player_load_packet *)data);
                break;

            case HEADER_GD_PLAYER_SAVE:
                metin2::log::debug("Player save request (handle {:d}, length {:d})", handle_value, length_value);
                query_player_save(peer, handle_value, (player_table *)data);
                break;

            case HEADER_GD_PLAYER_CREATE:
                metin2::log::debug("Player create request (handle {:d}, length {:d})", handle_value, length_value);
                query_player_create(peer, handle_value, (player_create_packet *)data);
                break;

            case HEADER_GD_PLAYER_DELETE:
                metin2::log::debug("Player delete request (handle {:d}, length {:d})", handle_value, length_value);
                query_player_delete(peer, handle_value, (player_delete_packet *)data);
                break;

            case HEADER_GD_PLAYER_COUNT:
                break;

            case HEADER_GD_QUEST_SAVE:
                metin2::log::debug("Quest save request (handle {:d}, length {:d})", handle_value, length_value);
                query_quest_save(peer, (quest_table *)data, length_value);
                break;

            case HEADER_GD_SAFEBOX_LOAD:
                query_safebox_load(peer, handle_value, (safebox_load_packet *)data, 0);
                break;

            case HEADER_GD_SAFEBOX_SAVE:
                metin2::log::debug("Safebox save request (handle {:d}, length {:d})", handle_value, length_value);
                query_safebox_save(peer, (safebox_table *)data);
                break;

            case HEADER_GD_SAFEBOX_CHANGE_SIZE:
                query_safebox_change_size(peer, handle_value, (safebox_change_size_packet *)data);
                break;

            case HEADER_GD_SAFEBOX_CHANGE_PASSWORD:
                query_safebox_change_password(peer, handle_value, (safebox_change_password_packet *)data);
                break;

            case HEADER_GD_MALL_LOAD:
                query_safebox_load(peer, handle_value, (safebox_load_packet *)data, 1);
                break;

            case HEADER_GD_EMPIRE_SELECT:
                query_empire_select(peer, handle_value, (empire_select_packet *)data);
                break;

            case HEADER_GD_SETUP:
                query_setup(peer, handle_value, data);
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
                query_change_name(peer, handle_value, (packet_gd_change_name *)data);
                break;

            case HEADER_GD_SMS:
                query_sms(peer, (packet_gdsms *)data);
                break;

            case HEADER_GD_AUTH_LOGIN:
                query_auth_login(peer, handle_value, (packet_gd_auth_login *)data);
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
                myshop_pricelist_request(peer, handle_value, *(uint32_t *)data);
                break;

            case HEADER_GD_RELOAD_ADMIN:
                reload_admin(peer, (packet_reload_admin *)data);
                break;

            case HEADER_GD_BREAK_MARRIAGE:
                break_marriage(peer, data);
                break;

            case HEADER_GD_ELECT_MONARCH:
                election(peer, handle_value, data);
                break;

            case HEADER_GD_CANDIDACY:
                candidacy(peer, handle_value, data);
                break;

            case HEADER_GD_ADD_MONARCH_MONEY:
                add_monarch_money(peer, handle_value, data);
                break;

            case HEADER_GD_DEC_MONARCH_MONEY:
                dec_monarch_money(peer, handle_value, data);
                break;

            case HEADER_GD_TAKE_MONARCH_MONEY:
                take_monarch_money(peer, handle_value, data);
                break;

            case HEADER_GD_COME_TO_VOTE:
                come_to_vote(peer, handle_value, data);
                break;

            case HEADER_GD_RMCANDIDACY:
                rm_candidacy(peer, handle_value, data);
                break;

            case HEADER_GD_SETMONARCH:
                set_monarch(peer, handle_value, data);
                break;

            case HEADER_GD_RMMONARCH:
                rm_monarch(peer, handle_value, data);
                break;

            case HEADER_GD_CHANGE_MONARCH_LORD:
                change_monarch_lord(peer, handle_value, (packet_change_monarch_lord *)data);
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
                request_channel_status(peer, handle_value);
                break;
            default:
                metin2::log::error("Unknown packet header {:d} (handle {:d}, length {:d})", header, handle_value, length_value);
                break;
            }
        }

        peer->recv_end(i);
    }

    void client_manager::add_peer(net::tcp::socket socket) {
        constexpr std::size_t max_peer_buffer = 256 * 1024 * 1024;

        auto connection = net::connection::create(std::move(socket), max_peer_buffer, max_peer_buffer);

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
            auto it = logon_accounts_.begin();

            while (it != logon_accounts_.end()) {
                login_data *ld = it->second;

                if (ld->connected_peer_handle() == peer_value->handle()) {
                    ld->set_play(false);

                    if (ld->is_deleted()) {
                        metin2::log::debug("Deleting login data of account {}", ld->account_ref().login);
                        delete ld;
                    }

                    logon_accounts_.erase(it++);
                } else
                    ++it;
            }
        }

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
                const sql_stats ret_stats = get_db_manager().return_stats(SQL_PLAYER);
                const sql_stats async_stats = get_db_manager().async_stats(SQL_PLAYER);

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

                get_db_manager().reset_counter();

                uint32_t count = get_client_manager().user_count();

                usage_avg += count;
                usage_max = std::max<int>(usage_max, count);

                thecore_idle_time = 0;

                if (!(pulse % (per_sec * 3600)))
                    usage_log();

                cache_flush_count_ = 0;

                update_player_cache();
                update_item_cache();
                update_logout_player();

                update_item_price_list_cache();

                get_guild_manager().update();
                get_priv_manager().update();
                marriage::get_manager().update();
            }

            if (!(pulse % (per_sec * 5))) {
                get_item_award_manager().request_load();
            }

            if (!(pulse % (per_sec * 10))) {
                metin2::log::debug("Perf: {:d} main queries, {:d} async queries in the last 10 s", query_count[0], query_count[1]);
                query_count[0] = 0;
                query_count[1] = 0;

                metin2::log::debug("Perf: {:d} item saves in the last 10 s", item_count);
                item_count = 0;
            }

            if (!(pulse % (per_sec * 60))) {
                get_client_manager().send_time();
            }

            if (!(pulse % (per_sec * 3600))) {
                db::logs::get_money_log().save();
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
        return logon_accounts_.size();
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
        const uint32_t min = metin2::config::db().item_id_min;
        const uint32_t max = metin2::config::db().item_id_max;

        metin2::log::info("Item id range from config: {:d} to {:d}", min, max);

        if (get_item_id_range_manager().build_range(min, max, item_range_) == false) {
            metin2::log::error("Cannot build item id range");
            return false;
        }

        metin2::log::info("Item id range {:d} to {:d}, next free id {:d}", item_range_.min, item_range_.max, item_range_.usable_item_id_min);

        return true;
    }

    auto client_manager::gain_item_id() -> uint64_t {
        return item_range_.usable_item_id_min++;
    }

    auto client_manager::item_id() -> uint64_t {
        return item_range_.usable_item_id_min;
    }

    auto client_manager::initialize_localization() -> bool {
        const std::string query = "SELECT mValue, mKey FROM locale";
        auto msg = get_db_manager().direct_query(query, SQL_COMMON);

        if (msg->get()->num_rows == 0) {
            metin2::log::error("Locale table is empty or cannot be read ({})", query);
            return false;
        }

        metin2::log::info("Loaded {:d} locale settings", msg->get()->num_rows);

        MYSQL_ROW row = nullptr;

        while ((row = mysql_fetch_row(msg->get()->res.get())) != nullptr) {
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

        auto msg = get_db_manager().direct_query(query, SQL_COMMON);

        if (msg->get()->num_rows == 0) {
            metin2::log::info("No gm accounts found ({})", query);
            return false;
        }

        MYSQL_ROW row;
        admin_vec.reserve(msg->get()->num_rows);

        while ((row = mysql_fetch_row(msg->get()->res.get()))) {
            int idx = 0;
            t_admin_info info;

            str_to_number(info.id, row[idx++]);
            trim_and_lower(row[idx++], info.account);
            strlcpy(info.name, row[idx++], sizeof(info.name));
            strlcpy(info.contact_ip, row[idx++], sizeof(info.contact_ip));
            strlcpy(info.server_ip, row[idx++], sizeof(info.server_ip));
            std::string auth = row[idx++];

            if (!auth.compare("IMPLEMENTOR"))
                info.authority = GM_IMPLEMENTOR;
            else if (!auth.compare("GOD"))
                info.authority = GM_GOD;
            else if (!auth.compare("HIGH_WIZARD"))
                info.authority = GM_HIGH_WIZARD;
            else if (!auth.compare("LOW_WIZARD"))
                info.authority = GM_LOW_WIZARD;
            else if (!auth.compare("WIZARD"))
                info.authority = GM_WIZARD;
            else
                continue;

            admin_vec.push_back(info);

            metin2::log::info("Gm: pid {:d}, account {}, character {}, contact ip {}, server ip {}, authority {:d} ({})",
                info.id, info.account, info.name, info.contact_ip, info.server_ip, info.authority, auth);
        }

        return true;
    }

    auto client_manager::host_info(std::vector<std::string> &ip_vec) -> bool {
        const std::string query = "SELECT mIP FROM gmhost";
        auto msg = get_db_manager().direct_query(query, SQL_COMMON);

        if (msg->get()->num_rows == 0) {
            metin2::log::info("No gm hosts configured ({})", query);
            return false;
        }

        ip_vec.reserve(msg->get()->num_rows);

        MYSQL_ROW row;

        while ((row = mysql_fetch_row(msg->get()->res.get()))) {
            if (row[0] && *row[0]) {
                ip_vec.push_back(row[0]);
                metin2::log::info("Gm host: {}", row[0]);
            }
        }

        return true;
    }

    void client_manager::reload_admin(peer *, packet_reload_admin *p) {
        std::vector<t_admin_info> v_admin;
        std::vector<std::string> v_host;

        host_info(v_host);
        admin_info(p->ip, v_admin);

        uint32_t packet_size = sizeof(uint16_t) + sizeof(uint16_t) + sizeof(t_admin_info) * v_admin.size() +
                               sizeof(uint16_t) + sizeof(uint16_t) + 16 * v_host.size();

        for (auto it = peer_list_.begin(); it != peer_list_.end(); ++it) {
            peer *peer = *it;

            if (!peer->channel())
                continue;

            peer->encode_header(HEADER_DG_RELOAD_ADMIN, 0, packet_size);

            peer->encode_word(16);
            peer->encode_word(v_host.size());

            for (size_t n = 0; n < v_host.size(); ++n)
                peer->encode(v_host[n].c_str(), 16);

            peer->encode_word(sizeof(t_admin_info));
            peer->encode_word(v_admin.size());

            for (size_t n = 0; n < v_admin.size(); ++n)
                peer->encode(&v_admin[n], sizeof(t_admin_info));
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
        marriage::get_manager().remove(pid1, pid2);
    }

    void client_manager::update_item_cache_set(uint32_t pid) {
        auto it = item_cache_set_ptrs_.find(pid);

        if (it == item_cache_set_ptrs_.end()) {
            if (metin2::config::test_server())
                metin2::log::debug("Item cache: no cache to update for pid {:d}", pid);
            return;
        }

        item_cache_set *set = it->second.get();
        auto it_set = set->begin();

        while (it_set != set->end()) {
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

        const int success = get_monarch().vote_monarch(selectingpid, idx);

        if (metin2::config::test_server()) {
            if (success)
                metin2::log::debug("Monarch election: vote {:d} for pid {:d} recorded", idx, selectingpid);
            else
                metin2::log::debug("Monarch election: vote {:d} for pid {:d} failed", idx, selectingpid);
        }

        peer->encode_header(HEADER_DG_ELECT_MONARCH, handle, sizeof(int));
        peer->encode(&success, sizeof(int));
    }

    void client_manager::candidacy(peer *peer, uint32_t handle, const char *data) {
        uint32_t pid;

        pid = *(uint32_t *)data;
        data += sizeof(uint32_t);

        if (!get_monarch().add_candidacy(pid, data)) {
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
        int empire_value = *(int *)data;
        data += sizeof(int);

        int money_value = *(int *)data;
        data += sizeof(int);

        if (metin2::config::test_server())
            metin2::log::debug("Monarch: add {1:d} money to empire {0:d}", empire_value, money_value);

        get_monarch().add_money(empire_value, money_value);

        broadcast_to_game_peers(peer_list_, HEADER_DG_ADD_MONARCH_MONEY, peer, handle, 0, {
                                                                                              {&empire_value, sizeof(int)},
                                                                                              {&money_value,  sizeof(int)}
        });
    }
    void client_manager::dec_monarch_money(peer *peer, uint32_t handle, const char *data) {
        int empire_value = *(int *)data;
        data += sizeof(int);

        int money_value = *(int *)data;
        data += sizeof(int);

        if (metin2::config::test_server())
            metin2::log::debug("Monarch: remove {1:d} money from empire {0:d}", empire_value, money_value);

        get_monarch().dec_money(empire_value, money_value);

        broadcast_to_game_peers(peer_list_, HEADER_DG_DEC_MONARCH_MONEY, peer, handle, 0, {
                                                                                              {&empire_value, sizeof(int)},
                                                                                              {&money_value,  sizeof(int)}
        });
    }

    void client_manager::take_monarch_money(peer *peer, uint32_t handle, const char *data) {
        int empire_value = *(int *)data;
        data += sizeof(int);

        uint32_t pid = *(uint32_t *)data;
        data += sizeof(int);

        int money_value = *(int *)data;
        data += sizeof(int);

        if (metin2::config::test_server())
            metin2::log::debug("Monarch: take {1:d} money from empire {0:d}", empire_value, money_value);

        if (!get_monarch().take_money(empire_value, pid, money_value))
            money_value = 0;

        peer->encode_header(HEADER_DG_TAKE_MONARCH_MONEY, handle, sizeof(int) + sizeof(int));
        peer->encode(&empire_value, sizeof(int));
        peer->encode(&money_value, sizeof(int));
    }

    void client_manager::come_to_vote(peer *peer, uint32_t handle, const char *data) {
        get_monarch().elect_monarch();
    }

    void client_manager::rm_candidacy(peer *peer, uint32_t handle, const char *data) {
        char name[32];

        strlcpy(name, data, sizeof(name));
        metin2::log::info("Monarch: gm removed candidacy of {}", name);

        int ret = get_monarch().del_candidacy(name) ? 1 : 0;

        peer->encode_header(HEADER_DG_RMCANDIDACY, handle, sizeof(int) + sizeof(name));
        peer->encode(&ret, sizeof(int));
        peer->encode(name, sizeof(name));
    }

    void client_manager::set_monarch(peer *peer, uint32_t handle, const char *data) {
        char name[32];

        strlcpy(name, data, sizeof(name));

        if (metin2::config::test_server())
            metin2::log::debug("Monarch: gm set {} as monarch", name);

        int ret = get_monarch().set_monarch(name) ? 1 : 0;

        peer->encode_header(HEADER_DG_SETMONARCH, handle, sizeof(int) + sizeof(name));
        peer->encode(&ret, sizeof(int));
        peer->encode(name, sizeof(name));

        if (ret == 1)
            for (db::clients::peer *p : peer_list_) {
                p->encode_header(HEADER_DG_UPDATE_MONARCH_INFO, 0, sizeof(monarch_info));
                p->encode(get_monarch().info(), sizeof(monarch_info));
            }
    }

    void client_manager::rm_monarch(peer *peer, uint32_t handle, const char *data) {
        char name[32];

        strlcpy(name, data, sizeof(name));

        if (metin2::config::test_server())
            metin2::log::debug("Monarch: gm removed monarch {}", name);

        int ret = get_monarch().del_monarch(name) ? 1 : 0;

        peer->encode_header(HEADER_DG_RMMONARCH, handle, sizeof(int) + sizeof(name));
        peer->encode(&ret, sizeof(int));
        peer->encode(name, sizeof(name));

        if (ret == 1)
            for (db::clients::peer *p : peer_list_) {
                p->encode_header(HEADER_DG_UPDATE_MONARCH_INFO, 0, sizeof(monarch_info));
                p->encode(get_monarch().info(), sizeof(monarch_info));
            }
    }

    void client_manager::change_monarch_lord(peer *peer, uint32_t handle, packet_change_monarch_lord *info) {
        const std::string query = sql::format(
            "SELECT a.name, NOW() FROM player AS a, player_index AS b WHERE (a.account_id=b.id AND a.id={0} AND b.empire={1}) AND "
            "(b.pid1={0} OR b.pid2={0} OR b.pid3={0} OR b.pid4={0})",
            info->pid, info->empire);

        auto msg = get_db_manager().direct_query(query, SQL_PLAYER);

        if (msg->get()->num_rows != 0) {
            packet_change_monarch_lord_ack ack;
            ack.empire = info->empire;
            ack.pid = info->pid;

            MYSQL_ROW row = mysql_fetch_row(msg->get()->res.get());
            strlcpy(ack.name, row[0], sizeof(ack.name));
            strlcpy(ack.date, row[1], sizeof(ack.date));

            const std::string update_query = sql::format("UPDATE monarch SET pid={}, windate=NOW() WHERE empire={}", ack.pid, ack.empire);
            auto msg2 = get_db_manager().direct_query(update_query, SQL_PLAYER);

            if (msg2->get()->affected_rows > 0) {
                get_monarch().load_monarch();

                monarch_info *new_info = get_monarch().info();

                for (auto it = peer_list_.begin(); it != peer_list_.end(); it++) {
                    db::clients::peer *client = *it;

                    client->encode_header(HEADER_DG_CHANGE_MONARCH_LORD_ACK, 0, sizeof(packet_change_monarch_lord_ack));
                    client->encode(&ack, sizeof(packet_change_monarch_lord_ack));

                    client->encode_header(HEADER_DG_UPDATE_MONARCH_INFO, 0, sizeof(monarch_info));
                    client->encode(new_info, sizeof(monarch_info));
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

        login_data *ld = get_login_data_by_login(login);

        if (ld) {
            auto it = login_data_.find(ld->key());

            if (it != login_data_.end())
                login_data_.erase(it);
        }
    }

    void client_manager::delete_award_id(packet_delete_award_id *data) {
        if (get_item_award_manager().remove(data->id))
            metin2::log::debug("Item award: removed award {:d} from cache", data->id);
        else
            metin2::log::warn("Item award: cannot delete award {:d}, not found", data->id);
    }

    void client_manager::update_channel_status(channel_status *data) {
        auto it = channel_status_.find(data->port);
        if (it != channel_status_.end()) {
            it->second = data->status;
        } else {
            channel_status_.insert(channel_status_map::value_type(data->port, data->status));
        }
    }

    void client_manager::request_channel_status(peer *peer, uint32_t handle) {
        const int size = channel_status_.size();
        peer->encode_header(HEADER_DG_RESPOND_CHANNELSTATUS, handle, sizeof(channel_status) * size + sizeof(int));
        peer->encode(&size, sizeof(int));
        for (auto it = channel_status_.begin(); it != channel_status_.end(); it++) {
            peer->encode(&it->first, sizeof(short));
            peer->encode(&it->second, sizeof(uint8_t));
        }
    }

    void client_manager::reset_last_player_id(const packet_need_login_log_info *data) {
        login_data *ld = get_login_data_by_aid(data->player_id);

        if (nullptr != ld) {
            ld->set_last_player_id(0);
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

        get_db_manager().async_query(query, SQL_ACCOUNT);
    }
}
