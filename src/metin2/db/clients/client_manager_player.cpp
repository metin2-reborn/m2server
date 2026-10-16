
#include <metin2/db/app/main.h>
#include <metin2/db/caches/cache.h>
#include <metin2/db/clients/client_manager.h>
#include <metin2/db/clients/peer.h>
#include <metin2/db/clients/qid.h>
#include <metin2/db/items/item_award_manager.h>
#include <metin2/db/login/login_data.h>
#include <metin2/db/storage/db_manager.h>

#include <metin2/common/utils.h>
#include <metin2/config/config.h>
#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/inventory.h>
#include <metin2/proto/item_types.h>
#include <metin2/proto/limits.h>
#include <metin2/proto/tables.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace metin2::db::inline clients {
    auto create_item_table_from_res(MYSQL_RES *res, std::vector<player_item> *vec, uint32_t pid) -> bool {
        if (!res) {
            vec->clear();
            return true;
        }

        int rows;

        if ((rows = mysql_num_rows(res)) <= 0) {
            vec->clear();
            return true;
        }

        vec->resize(rows);

        for (int i = 0; i < rows; ++i) {
            MYSQL_ROW row = mysql_fetch_row(res);
            player_item &item = vec->at(i);

            int cur = 0;

            str_to_number(item.id, row[cur++]);
            str_to_number(item.window, row[cur++]);
            str_to_number(item.pos, row[cur++]);
            str_to_number(item.count, row[cur++]);
            str_to_number(item.vnum, row[cur++]);
            str_to_number(item.sockets[0], row[cur++]);
            str_to_number(item.sockets[1], row[cur++]);
            str_to_number(item.sockets[2], row[cur++]);

            for (int j = 0; j < ITEM_ATTRIBUTE_MAX_NUM; j++) {
                str_to_number(item.attr[j].type, row[cur++]);
                str_to_number(item.attr[j].s_value, row[cur++]);
            }

            item.owner = pid;
        }

        return true;
    }

    auto create_item_save_query(const player_item &item) -> std::string {
        return sql::format(
            "REPLACE INTO item (id, owner_id, window, pos, count, vnum, socket0, socket1, socket2, "
            "attrtype0, attrvalue0, attrtype1, attrvalue1, attrtype2, attrvalue2, attrtype3, attrvalue3, "
            "attrtype4, attrvalue4, attrtype5, attrvalue5, attrtype6, attrvalue6) "
            "VALUES({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {})",
            item.id, item.owner, item.window, item.pos, item.count, item.vnum,
            static_cast<uint32_t>(item.sockets[0]), static_cast<uint32_t>(item.sockets[1]), static_cast<uint32_t>(item.sockets[2]),
            item.attr[0].type, item.attr[0].s_value,
            item.attr[1].type, item.attr[1].s_value,
            item.attr[2].type, item.attr[2].s_value,
            item.attr[3].type, item.attr[3].s_value,
            item.attr[4].type, item.attr[4].s_value,
            item.attr[5].type, item.attr[5].s_value,
            item.attr[6].type, item.attr[6].s_value);
    }

    auto create_player_save_query(const player_table *tab) -> std::string {
        return sql::format(
            "UPDATE player SET "
            "job = {}, "
            "voice = {}, "
            "dir = {}, "
            "x = {}, "
            "y = {}, "
            "z = {}, "
            "map_index = {}, "
            "exit_x = {}, "
            "exit_y = {}, "
            "exit_map_index = {}, "
            "hp = {}, "
            "mp = {}, "
            "stamina = {}, "
            "random_hp = {}, "
            "random_sp = {}, "
            "playtime = {}, "
            "level = {}, "
            "level_step = {}, "
            "st = {}, "
            "ht = {}, "
            "dx = {}, "
            "iq = {}, "
            "gold = {}, "
            "exp = {}, "
            "stat_point = {}, "
            "skill_point = {}, "
            "sub_skill_point = {}, "
            "stat_reset_count = {}, "
            "ip = {}, "
            "part_main = {}, "
            "part_hair = {}, "
            "last_play = NOW(), "
            "skill_group = {}, "
            "alignment = {}, "
            "horse_level = {}, "
            "horse_riding = {}, "
            "horse_hp = {}, "
            "horse_hp_droptime = {}, "
            "horse_stamina = {}, "
            "horse_skill_point = {}, "
            "skill_level = {}, "
            "quickslot = {} "
            " WHERE id={}",
            tab->job,
            tab->voice,
            tab->dir,
            tab->x,
            tab->y,
            tab->z,
            tab->map_index,
            tab->exit_x,
            tab->exit_y,
            tab->exit_map_index,
            tab->hp,
            tab->sp,
            tab->stamina,
            tab->s_random_hp,
            tab->s_random_sp,
            tab->playtime,
            tab->level,
            tab->level_step,
            tab->st,
            tab->ht,
            tab->dx,
            tab->iq,
            tab->gold,
            tab->exp,
            tab->stat_point,
            tab->skill_point,
            tab->sub_skill_point,
            tab->stat_reset_count,
            tab->ip,
            tab->parts[PART_MAIN],
            tab->parts[PART_HAIR],
            tab->skill_group,
            tab->alignment,
            tab->horse.level,
            tab->horse.riding,
            tab->horse.s_health,
            tab->horse.horse_health_drop_time,
            tab->horse.s_stamina,
            tab->horse_skill_point,
            std::string_view(reinterpret_cast<const char *>(tab->skills), sizeof(tab->skills)),
            std::string_view(reinterpret_cast<const char *>(tab->quickslot), sizeof(tab->quickslot)),
            tab->id);
    }

    auto client_manager::player_cache(uint32_t id) -> player_table_cache * {
        auto it = player_caches_.find(id);
        if (it == player_caches_.end()) {
            return nullptr;
        }

        player_table *table = it->second->get(false);
        table->logoff_interval = it->second->seconds_since_update();
        return it->second.get();
    }

    void client_manager::put_player_cache(player_table *updated) {
        auto *c = player_cache(updated->id);
        if (!c) {
            c = player_caches_.emplace(updated->id, std::make_unique<player_table_cache>()).first->second.get();
        }

        c->put(updated);
    }

    void client_manager::query_player_load(peer *peer, uint32_t handle_value, player_load_packet *packet) {
        auto return_composite_query = [this, peer_handle = peer->handle()](std::string_view query, uint32_t qid, std::unique_ptr<client_handle_info> info) {
            get_db_manager().return_query(query, [this, peer_handle, qid, info = std::move(info)](sql_msg &msg) {
                if (auto *peer_value = get_peer(peer_handle))
                    result_composite_player(peer_value, msg, qid, *info);
            });
        };

        player_table_cache *c;
        player_table *tab;

        login_data *login_data_value = get_login_data_by_aid(packet->account_id);

        if (login_data_value) {
            for (int n = 0; n < PLAYER_PER_ACCOUNT; ++n) {
                if (login_data_value->account_ref().players[n].id != 0) {
                    delete_logout_player(login_data_value->account_ref().players[n].id);
                }
            }
        }

        if ((c = player_cache(packet->player_id))) {
            login_data *ld = get_login_data_by_aid(packet->account_id);

            if (!ld || ld->is_play()) {
                metin2::log::warn("Cannot load player pid {:d} from cache: login data {}, already playing {:d}", packet->player_id, static_cast<const void *>(ld), ld ? ld->is_play() : 0);
                peer->encode_header(HEADER_DG_PLAYER_LOAD_FAILED, handle_value, 0);
                return;
            }

            tab = c->get();

            ld->set_play(true);
            std::memcpy(tab->premium_times, ld->premium_ptr(), sizeof(tab->premium_times));

            peer->encode_header(HEADER_DG_PLAYER_LOAD_SUCCESS, handle_value, sizeof(player_table));
            peer->encode(tab, sizeof(player_table));

            if (packet->player_id != ld->last_player_id()) {
                packet_need_login_log_info log_info;
                log_info.player_id = packet->player_id;

                ld->set_last_player_id(packet->player_id);

                peer->encode_header(HEADER_DG_NEED_LOGIN_LOG, handle_value, sizeof(packet_need_login_log_info));
                peer->encode(&log_info, sizeof(packet_need_login_log_info));
            }

            std::string query;

            item_cache_set *set = get_item_cache_set(tab->id);

            metin2::log::info("Player {} (pid {:d}) loaded from cache, gold {:d}", tab->name, tab->id, tab->gold);

            if (set) {
                std::vector<player_item> s_items(set->size());

                uint32_t count = 0;
                auto it = set->begin();

                while (it != set->end()) {
                    item_cache *c = *it++;
                    player_item *p = c->get();

                    if (p->vnum) {
                        std::memcpy(&s_items[count++], p, sizeof(player_item));
                    }
                }

                if (metin2::config::test_server())
                    metin2::log::debug("Item cache hit for player {}: {:d} items", tab->name, count);

                peer->encode_header(HEADER_DG_ITEM_LOAD, handle_value, sizeof(uint32_t) + sizeof(player_item) * count);
                peer->encode_dword(count);

                if (count) {
                    peer->encode(s_items.data(), sizeof(player_item) * count);
                }

                query = sql::format("SELECT dwPID,szName,szState,lValue FROM quest WHERE dwPID={} AND lValue<>0", tab->id);

                return_composite_query(query, QID_QUEST, std::make_unique<client_handle_info>(handle_value, 0, packet->account_id));

                query = sql::format("SELECT dwPID,bType,bApplyOn,lApplyValue,dwFlag,lDuration,lSPCost FROM affect WHERE dwPID={}", tab->id);
                return_composite_query(query, QID_AFFECT, std::make_unique<client_handle_info>(handle_value));
            } else {
                query = sql::format("SELECT {} FROM item WHERE owner_id={} AND (window < {} or window = {})",
                    sql::raw(item_select_columns), tab->id, SAFEBOX, DRAGON_SOUL_INVENTORY);

                return_composite_query(query, QID_ITEM, std::make_unique<client_handle_info>(handle_value, tab->id));
                query = sql::format("SELECT dwPID, szName, szState, lValue FROM quest WHERE dwPID={}", tab->id);

                return_composite_query(query, QID_QUEST, std::make_unique<client_handle_info>(handle_value, tab->id));
                query = sql::format("SELECT dwPID, bType, bApplyOn, lApplyValue, dwFlag, lDuration, lSPCost FROM affect WHERE dwPID={}", tab->id);

                return_composite_query(query, QID_AFFECT, std::make_unique<client_handle_info>(handle_value, tab->id));
            }
        } else {
            metin2::log::info("Loading player pid {:d} from the database", packet->player_id);

            std::string query = sql::format("SELECT "
                                            "id,name,job,voice,dir,x,y,z,map_index,exit_x,exit_y,exit_map_index,hp,mp,stamina,random_hp,random_sp,playtime,"
                                            "gold,level,level_step,st,ht,dx,iq,exp,"
                                            "stat_point,skill_point,sub_skill_point,stat_reset_count,part_base,part_hair,"
                                            "skill_level,quickslot,skill_group,alignment,mobile,horse_level,horse_riding,horse_hp,horse_hp_droptime,horse_stamina,"
                                            "UNIX_TIMESTAMP(NOW())-UNIX_TIMESTAMP(last_play),horse_skill_point FROM player WHERE id={}",
                packet->player_id);

            auto info = std::make_unique<client_handle_info>(handle_value, packet->player_id);
            info->account_id = packet->account_id;
            return_composite_query(query, QID_PLAYER, std::move(info));

            query = sql::format("SELECT {} FROM item WHERE owner_id={} AND (window < {} or window = {})",
                sql::raw(item_select_columns), packet->player_id, SAFEBOX, DRAGON_SOUL_INVENTORY);
            return_composite_query(query, QID_ITEM, std::make_unique<client_handle_info>(handle_value, packet->player_id));

            query = sql::format("SELECT dwPID,szName,szState,lValue FROM quest WHERE dwPID={}", packet->player_id);
            return_composite_query(query, QID_QUEST, std::make_unique<client_handle_info>(handle_value, packet->player_id, packet->account_id));
            query = sql::format("SELECT dwPID,bType,bApplyOn,lApplyValue,dwFlag,lDuration,lSPCost FROM affect WHERE dwPID={}", packet->player_id);
            return_composite_query(query, QID_AFFECT, std::make_unique<client_handle_info>(handle_value, packet->player_id));
        }
    }
    void client_manager::item_award(peer *peer, char *login) {
        std::string login_t = login;

        auto *set = get_item_award_manager().by_login(login_t.c_str());
        if (set == nullptr) {
            return;
        }

        for (const db::items::item_award *award : *set) {
            inform_gift_award(award->login, award->why, award->vnum);
        }
    }

    void client_manager::inform_gift_award(const char *login, const char *why, uint32_t vnum) {
        const std::string_view text(why);
        if (!text.starts_with("[GIFT") || (text.size() > 5 && text[5] != ']')) {
            return;
        }

        packet_item_award_infromer gift_data{};

        strlcpy(gift_data.login, login, sizeof(gift_data.login));
        strlcpy(gift_data.command, "GIFT", sizeof(gift_data.command));

        gift_data.vnum = vnum;

        forward_packet(HEADER_DG_ITEMAWARD_INFORMER, &gift_data, sizeof(gift_data));
    }

    auto create_player_table_from_res(MYSQL_RES *res, player_table *tab) -> bool {
        if (mysql_num_rows(res) == 0) {
            return false;
        }

        memset(tab, 0, sizeof(player_table));

        MYSQL_ROW row = mysql_fetch_row(res);
        const unsigned long *lengths = mysql_fetch_lengths(res);

        int col = 0;

        str_to_number(tab->id, row[col++]);
        strlcpy(tab->name, row[col++], sizeof(tab->name));
        str_to_number(tab->job, row[col++]);
        str_to_number(tab->voice, row[col++]);
        str_to_number(tab->dir, row[col++]);
        str_to_number(tab->x, row[col++]);
        str_to_number(tab->y, row[col++]);
        str_to_number(tab->z, row[col++]);
        str_to_number(tab->map_index, row[col++]);
        str_to_number(tab->exit_x, row[col++]);
        str_to_number(tab->exit_y, row[col++]);
        str_to_number(tab->exit_map_index, row[col++]);
        str_to_number(tab->hp, row[col++]);
        str_to_number(tab->sp, row[col++]);
        str_to_number(tab->stamina, row[col++]);
        str_to_number(tab->s_random_hp, row[col++]);
        str_to_number(tab->s_random_sp, row[col++]);
        str_to_number(tab->playtime, row[col++]);
        str_to_number(tab->gold, row[col++]);
        str_to_number(tab->level, row[col++]);
        str_to_number(tab->level_step, row[col++]);
        str_to_number(tab->st, row[col++]);
        str_to_number(tab->ht, row[col++]);
        str_to_number(tab->dx, row[col++]);
        str_to_number(tab->iq, row[col++]);
        str_to_number(tab->exp, row[col++]);
        str_to_number(tab->stat_point, row[col++]);
        str_to_number(tab->skill_point, row[col++]);
        str_to_number(tab->sub_skill_point, row[col++]);
        str_to_number(tab->stat_reset_count, row[col++]);
        str_to_number(tab->part_base, row[col++]);
        str_to_number(tab->parts[PART_HAIR], row[col++]);

        std::memset(&tab->skills, 0, sizeof(tab->skills));
        if (row[col]) {
            std::memcpy(tab->skills, row[col], std::min<size_t>(lengths[col], sizeof(tab->skills)));
        }

        col++;

        std::memset(tab->quickslot, 0, sizeof(tab->quickslot));
        if (row[col]) {
            std::memcpy(tab->quickslot, row[col], std::min<size_t>(lengths[col], sizeof(tab->quickslot)));
        }

        col++;

        str_to_number(tab->skill_group, row[col++]);
        str_to_number(tab->alignment, row[col++]);

        if (row[col]) {
            strlcpy(tab->mobile, row[col], sizeof(tab->mobile));
        }

        col++;

        str_to_number(tab->horse.level, row[col++]);
        str_to_number(tab->horse.riding, row[col++]);
        str_to_number(tab->horse.s_health, row[col++]);
        str_to_number(tab->horse.horse_health_drop_time, row[col++]);
        str_to_number(tab->horse.s_stamina, row[col++]);
        str_to_number(tab->logoff_interval, row[col++]);
        str_to_number(tab->horse_skill_point, row[col++]);

        {
            tab->skills[123].level = 0;

            if (tab->level > 9) {
                int max_point = tab->level - 9;

                int skill_point =
                    std::min<int>(20, tab->skills[121].level) +
                    std::min<int>(20, tab->skills[124].level) +
                    std::min<int>(10, tab->skills[131].level) +
                    std::min<int>(20, tab->skills[141].level) +
                    std::min<int>(20, tab->skills[142].level);

                tab->sub_skill_point = max_point - skill_point;
            } else {
                tab->sub_skill_point = 0;
            }
        }

        return true;
    }

    void client_manager::result_composite_player(peer *peer, sql_msg &msg, uint32_t qid, client_handle_info &info) {
        MYSQL_RES *sql_result = msg.get()->res.get();
        if (!sql_result) {
            metin2::log::error("Query {:d} returned no result set", qid);
            return;
        }

        switch (qid) {
        case QID_PLAYER:
            metin2::log::debug("Player query result for handle {:d}, pid {:d}", info.handle, info.player_id);
            result_player_load(peer, sql_result, &info);

            break;

        case QID_ITEM:
            metin2::log::debug("Item query result for handle {:d}", info.handle);
            result_item_load(peer, sql_result, info.handle, info.player_id);
            break;

        case QID_QUEST:
            {
                metin2::log::debug("Quest query result for handle {:d}", info.handle);
                result_quest_load(peer, sql_result, info.handle, info.player_id);

                login_data *login_data1 = get_login_data_by_aid(info.account_id);
                if (login_data1 == nullptr) {
                    break;
                }

                item_award(peer, login_data1->account_ref().login);
            }
            break;

        case QID_AFFECT:
            metin2::log::debug("Affect query result for handle {:d}", info.handle);
            result_affect_load(peer, sql_result, info.handle);
            break;
        }
    }

    void client_manager::result_player_load(peer *peer, MYSQL_RES *res, client_handle_info *info_value) {
        player_table tab;

        if (!create_player_table_from_res(res, &tab)) {
            peer->encode_header(HEADER_DG_PLAYER_LOAD_FAILED, info_value->handle, 0);
            return;
        }

        login_data *ld = get_login_data_by_aid(info_value->account_id);

        if (!ld || ld->is_play()) {
            metin2::log::warn("Cannot load player: login data {}, already playing {:d}", static_cast<const void *>(ld), ld ? ld->is_play() : 0);
            peer->encode_header(HEADER_DG_PLAYER_LOAD_FAILED, info_value->handle, 0);
            return;
        }

        ld->set_play(true);
        std::memcpy(tab.premium_times, ld->premium_ptr(), sizeof(tab.premium_times));

        peer->encode_header(HEADER_DG_PLAYER_LOAD_SUCCESS, info_value->handle, sizeof(player_table));
        peer->encode(&tab, sizeof(player_table));

        if (tab.id != ld->last_player_id()) {
            packet_need_login_log_info log_info;
            log_info.player_id = tab.id;

            ld->set_last_player_id(tab.id);

            peer->encode_header(HEADER_DG_NEED_LOGIN_LOG, info_value->handle, sizeof(packet_need_login_log_info));
            peer->encode(&log_info, sizeof(packet_need_login_log_info));
        }
    }

    void client_manager::result_item_load(peer *peer, MYSQL_RES *res, uint32_t handle, uint32_t pid) {
        std::vector<player_item> s_items;
        create_item_table_from_res(res, &s_items, pid);
        uint32_t count = s_items.size();

        peer->encode_header(HEADER_DG_ITEM_LOAD, handle, sizeof(uint32_t) + sizeof(player_item) * count);
        peer->encode_dword(count);

        create_item_cache_set(pid);

        metin2::log::debug("Loaded {:d} items for pid {:d}", count, pid);

        if (count) {
            peer->encode(s_items.data(), sizeof(player_item) * count);

            for (uint32_t i = 0; i < count; ++i) {
                put_item_cache(&s_items[i], true);
            }
        }
    }

    void client_manager::result_affect_load(peer *peer, MYSQL_RES *res, uint32_t handle) {
        int num_rows;
        if ((num_rows = mysql_num_rows(res)) == 0) {
            return;
        }

        std::vector<packet_affect_element> s_elements(num_rows);

        uint32_t pid_value = 0;

        MYSQL_ROW row;

        for (int i = 0; i < num_rows; ++i) {
            packet_affect_element &r = s_elements[i];
            row = mysql_fetch_row(res);

            if (pid_value == 0) {
                str_to_number(pid_value, row[0]);
            }

            str_to_number(r.type, row[1]);
            str_to_number(r.apply_on, row[2]);
            str_to_number(r.apply_value, row[3]);
            str_to_number(r.flag, row[4]);
            str_to_number(r.duration, row[5]);
            str_to_number(r.sp_cost, row[6]);
        }

        metin2::log::debug("Loaded {:d} affects for pid {:d}", s_elements.size(), pid_value);

        uint32_t count = s_elements.size();

        peer->encode_header(HEADER_DG_AFFECT_LOAD, handle, sizeof(uint32_t) + sizeof(uint32_t) + sizeof(packet_affect_element) * count);
        peer->encode(&pid_value, sizeof(uint32_t));
        peer->encode(&count, sizeof(uint32_t));
        peer->encode(s_elements.data(), sizeof(packet_affect_element) * count);
    }

    void client_manager::result_quest_load(peer *peer, MYSQL_RES *res, uint32_t handle, uint32_t pid) {
        int num_rows;

        if ((num_rows = mysql_num_rows(res)) == 0) {
            uint32_t count = 0;
            peer->encode_header(HEADER_DG_QUEST_LOAD, handle, sizeof(uint32_t));
            peer->encode(&count, sizeof(uint32_t));
            return;
        }

        std::vector<quest_table> s_table(num_rows);

        MYSQL_ROW row;

        for (int i = 0; i < num_rows; ++i) {
            quest_table &r = s_table[i];

            row = mysql_fetch_row(res);

            str_to_number(r.pid, row[0]);
            strlcpy(r.name, row[1], sizeof(r.name));
            strlcpy(r.state, row[2], sizeof(r.state));
            str_to_number(r.value, row[3]);
        }

        metin2::log::debug("Loaded {:d} quest flags for pid {:d}", s_table.size(), s_table[0].pid);

        uint32_t count = s_table.size();

        peer->encode_header(HEADER_DG_QUEST_LOAD, handle, sizeof(uint32_t) + sizeof(quest_table) * count);
        peer->encode(&count, sizeof(uint32_t));
        peer->encode(s_table.data(), sizeof(quest_table) * count);
    }

    void client_manager::query_player_save(peer *peer, uint32_t handle, player_table *tab) {
        if (metin2::config::test_server()) {
            metin2::log::debug("Saving player {}", tab->name);
        }

        put_player_cache(tab);
    }

    using time_by_id_map = std::map<uint32_t, time_t>;
    static time_by_id_map create_time_by_account_id;

    void client_manager::query_player_create(peer *peer, uint32_t handle, player_create_packet *packet) {
        int player_id;

        auto it = create_time_by_account_id.find(packet->account_id);

        if (it != create_time_by_account_id.end()) {
            time_t curtime = time(nullptr);

            if (curtime - it->second < 30) {
                peer->encode_header(HEADER_DG_PLAYER_CREATE_FAILED, handle, 0);
                return;
            }
        }

        std::string query = sql::format(
            "SELECT pid{} FROM player_index WHERE id={}", packet->account_index + 1, packet->account_id);

        auto msg0 = get_db_manager().direct_query(query);

        if (msg0->get()->num_rows != 0) {
            if (!msg0->get()->res.get()) {
                peer->encode_header(HEADER_DG_PLAYER_CREATE_FAILED, handle, 0);
                return;
            }

            MYSQL_ROW row = mysql_fetch_row(msg0->get()->res.get());

            uint32_t pid_value = 0;
            str_to_number(pid_value, row[0]);
            if (row[0] && pid_value > 0) {
                peer->encode_header(HEADER_DG_PLAYER_CREATE_ALREADY, handle, 0);
                metin2::log::info("Player create: slot {:d} already holds pid {:d}", packet->account_index, pid_value);
                return;
            }
        } else {
            peer->encode_header(HEADER_DG_PLAYER_CREATE_FAILED, handle, 0);
            return;
        }

        query = sql::format(
            "SELECT COUNT(*) as count FROM player WHERE name={}", packet->player_table.name);

        auto msg1 = get_db_manager().direct_query(query);

        if (msg1->get()->num_rows) {
            if (!msg1->get()->res.get()) {
                peer->encode_header(HEADER_DG_PLAYER_CREATE_FAILED, handle, 0);
                return;
            }

            MYSQL_ROW row = mysql_fetch_row(msg1->get()->res.get());

            if (*row[0] != '0') {
                metin2::log::info("Player create: name {} is already taken (count {}, query: {})", packet->player_table.name, row[0], query);
                peer->encode_header(HEADER_DG_PLAYER_CREATE_ALREADY, handle, 0);
                return;
            }
        } else {
            peer->encode_header(HEADER_DG_PLAYER_CREATE_FAILED, handle, 0);
            return;
        }

        query = sql::format(
            "INSERT INTO player "
            "(id, account_id, name, level, st, ht, dx, iq, "
            "job, voice, dir, x, y, z, "
            "hp, mp, random_hp, random_sp, stat_point, stamina, part_base, part_main, part_hair, gold, playtime, "
            "skill_level, quickslot) "
            "VALUES(0, {}, {}, {}, {}, {}, {}, {}, "
            "{}, {}, {}, {}, {}, {}, {}, "
            "{}, {}, {}, {}, {}, {}, {}, 0, {}, 0, "
            "{}, {})",
            packet->account_id, packet->player_table.name, packet->player_table.level, packet->player_table.st, packet->player_table.ht, packet->player_table.dx, packet->player_table.iq,
            packet->player_table.job, packet->player_table.voice, packet->player_table.dir, packet->player_table.x, packet->player_table.y, packet->player_table.z,
            packet->player_table.hp, packet->player_table.sp, packet->player_table.s_random_hp, packet->player_table.s_random_sp, packet->player_table.stat_point, packet->player_table.stamina, packet->player_table.part_base, packet->player_table.part_base, packet->player_table.gold,
            std::string_view(reinterpret_cast<const char *>(packet->player_table.skills), sizeof(packet->player_table.skills)),
            std::string_view(reinterpret_cast<const char *>(packet->player_table.quickslot), sizeof(packet->player_table.quickslot)));

        metin2::log::info("Player create: account {:d}, name {}, level {:d}, gold {:d}, st {:d}, ht {:d}, job {:d}",
            packet->account_id,
            packet->player_table.name,
            packet->player_table.level,
            packet->player_table.gold,
            packet->player_table.st,
            packet->player_table.ht,
            packet->player_table.job);

        auto msg2 = get_db_manager().direct_query(query);
        if (metin2::config::test_server()) {
            metin2::log::debug("Player create: query length {:d}", query.size());
        }

        if (msg2->get()->affected_rows <= 0) {
            peer->encode_header(HEADER_DG_PLAYER_CREATE_ALREADY, handle, 0);
            metin2::log::warn("Player create: insert affected no rows, treating name as taken (query: {}, affected rows {:d})", query, msg2->get()->affected_rows);
            return;
        }

        player_id = msg2->get()->insert_id;

        query = sql::format("UPDATE player_index SET pid{}={} WHERE id={}", packet->account_index + 1, player_id, packet->account_id);
        auto msg3 = get_db_manager().direct_query(query);

        if (msg3->get()->affected_rows <= 0) {
            metin2::log::error("Player create: cannot update player_index, rolling back: {}", query);

            query = sql::format("DELETE FROM player WHERE id={}", player_id);
            get_db_manager().direct_query(query);

            peer->encode_header(HEADER_DG_PLAYER_CREATE_FAILED, handle, 0);
            return;
        }

        packet_dg_create_success pack;
        memset(&pack, 0, sizeof(pack));

        pack.account_character_index = packet->account_index;

        pack.player.id = player_id;
        strlcpy(pack.player.name, packet->player_table.name, sizeof(pack.player.name));
        pack.player.job = packet->player_table.job;
        pack.player.level = 1;
        pack.player.play_minutes = 0;
        pack.player.st = packet->player_table.st;
        pack.player.ht = packet->player_table.ht;
        pack.player.dx = packet->player_table.dx;
        pack.player.iq = packet->player_table.iq;
        pack.player.main_part = packet->player_table.part_base;
        pack.player.x = packet->player_table.x;
        pack.player.y = packet->player_table.y;

        peer->encode_header(HEADER_DG_PLAYER_CREATE_SUCCESS, handle, sizeof(packet_dg_create_success));
        peer->encode(&pack, sizeof(packet_dg_create_success));

        metin2::log::info("Player create: created {} (job {:d})", pack.player.name, pack.player.job);

        create_time_by_account_id[packet->account_id] = time(nullptr);
    }

    void client_manager::query_player_delete(peer *peer, uint32_t handle_value, player_delete_packet *packet) {
        if (!packet->login[0] || !packet->player_id || packet->account_index >= PLAYER_PER_ACCOUNT) {
            return;
        }

        login_data *ld = get_login_data_by_login(packet->login);

        if (!ld) {
            peer->encode_header(HEADER_DG_PLAYER_DELETE_FAILED, handle_value, 1);
            peer->encode_byte(packet->account_index);
            return;
        }

        account_table &r = ld->account_ref();

        if (strlen(r.social_id) < 7 || strncmp(packet->private_code, r.social_id + strlen(r.social_id) - 7, 7)) {
            metin2::log::info("Player delete: private code mismatch for pid {:d} (social id length {:d})", packet->player_id, strlen(r.social_id));
            peer->encode_header(HEADER_DG_PLAYER_DELETE_FAILED, handle_value, 1);
            peer->encode_byte(packet->account_index);
            return;
        }

        player_table_cache *player_cache_value = player_cache(packet->player_id);
        if (player_cache_value) {
            player_table *tab = player_cache_value->get();

            if (tab->level >= player_delete_level_limit_) {
                metin2::log::info("Player delete: pid {:d} level {:d} is at or above the limit {:d}", packet->player_id, tab->level, player_delete_level_limit_);
                peer->encode_header(HEADER_DG_PLAYER_DELETE_FAILED, handle_value, 1);
                peer->encode_byte(packet->account_index);
                return;
            }

            if (tab->level < player_delete_level_limit_lower_) {
                metin2::log::info("Player delete: pid {:d} level {:d} is below the minimum {:d}", packet->player_id, tab->level, player_delete_level_limit_lower_);
                peer->encode_header(HEADER_DG_PLAYER_DELETE_FAILED, handle_value, 1);
                peer->encode_byte(packet->account_index);
                return;
            }
        }

        const std::string query = sql::format("SELECT p.id, p.level, p.name FROM player_index AS i, player AS p WHERE pid{0}={1} AND pid{0}=p.id", packet->account_index + 1, packet->player_id);

        auto pi = std::make_unique<client_handle_info>(handle_value, packet->player_id);
        pi->account_index = packet->account_index;

        metin2::log::info("Player delete: account {} requested deletion of pid {:d} (slot {:d})", packet->login, packet->player_id, packet->account_index + 1);
        get_db_manager().return_query(query, [this, handle = peer->handle(), pi = std::move(pi)](sql_msg &msg) {
            if (auto *peer_value = get_peer(handle))
                result_player_delete(peer_value, msg, *pi);
        });
    }

    void client_manager::result_player_delete(peer *peer, sql_msg &msg, client_handle_info &pi) {
        if (msg.get() && msg.get()->num_rows) {
            MYSQL_ROW row = mysql_fetch_row(msg.get()->res.get());

            uint32_t pid_value = 0;
            str_to_number(pid_value, row[0]);

            int deleted_level_limit = 0;
            str_to_number(deleted_level_limit, row[1]);

            std::string name = row[2];

            if (deleted_level_limit >= player_delete_level_limit_) {
                metin2::log::info("Player delete: pid {:d} level {:d} is at or above the limit {:d}", pid_value, deleted_level_limit, player_delete_level_limit_);
                peer->encode_header(HEADER_DG_PLAYER_DELETE_FAILED, pi.handle, 1);
                peer->encode_byte(pi.account_index);
                return;
            }

            if (deleted_level_limit < player_delete_level_limit_lower_) {
                metin2::log::info("Player delete: pid {:d} level {:d} is below the minimum {:d}", pid_value, deleted_level_limit, player_delete_level_limit_lower_);
                peer->encode_header(HEADER_DG_PLAYER_DELETE_FAILED, pi.handle, 1);
                peer->encode_byte(pi.account_index);
                return;
            }

            std::string query = sql::format("INSERT INTO player_deleted SELECT * FROM player WHERE id={}", pi.player_id);
            auto ins = get_db_manager().direct_query(query);

            if (ins->get()->affected_rows == 0 || ins->get()->affected_rows == (uint32_t)-1) {
                metin2::log::error("Player delete: cannot copy pid {:d} into player_deleted", pid_value);

                peer->encode_header(HEADER_DG_PLAYER_DELETE_FAILED, pi.handle, 1);
                peer->encode_byte(pi.account_index);
                return;
            }

            metin2::log::info("Player delete: deleted pid {:d}", pid_value);

            player_table_cache *player_cache_value = player_cache(pi.player_id);

            if (player_cache_value) {
                player_caches_.erase(pi.player_id);
            }

            item_cache_set *set = get_item_cache_set(pi.player_id);

            if (set) {
                auto it = set->begin();

                while (it != set->end()) {
                    item_cache *item_cache = *it++;
                    delete_item_cache(item_cache->get()->id);
                }

                set->clear();

                item_cache_set_ptrs_.erase(pi.player_id);
            }

            query = sql::format("UPDATE player_index SET pid{}=0 WHERE pid{}={}",
                pi.account_index + 1,
                pi.account_index + 1,
                pi.player_id);

            auto msg = get_db_manager().direct_query(query);

            if (msg->get()->affected_rows == 0 || msg->get()->affected_rows == (uint32_t)-1) {
                metin2::log::error("Player delete: cannot clear pid {:d} from player_index", pi.player_id);
                peer->encode_header(HEADER_DG_PLAYER_DELETE_FAILED, pi.handle, 1);
                peer->encode_byte(pi.account_index);
                return;
            }

            query = sql::format("DELETE FROM player WHERE id={}", pi.player_id);
            get_db_manager().direct_query(query);

            query = sql::format("DELETE FROM item WHERE owner_id={} AND (window < {} or window = {})", pi.player_id, SAFEBOX, DRAGON_SOUL_INVENTORY);
            get_db_manager().direct_query(query);

            query = sql::format("DELETE FROM quest WHERE dwPID={}", pi.player_id);
            get_db_manager().async_query(query);

            query = sql::format("DELETE FROM affect WHERE dwPID={}", pi.player_id);
            get_db_manager().async_query(query);

            query = sql::format("DELETE FROM guild_member WHERE pid={}", pi.player_id);
            get_db_manager().async_query(query);

            query = sql::format("DELETE FROM myshop_pricelist WHERE owner_id={}", pi.player_id);
            get_db_manager().async_query(query);

            query = sql::format("DELETE FROM messenger_list WHERE account={} OR companion={}", name, name);
            get_db_manager().async_query(query);

            peer->encode_header(HEADER_DG_PLAYER_DELETE_SUCCESS, pi.handle, 1);
            peer->encode_byte(pi.account_index);
        } else {
            metin2::log::warn("Player delete: pid {:d} not found in player_index", pi.player_id);
            peer->encode_header(HEADER_DG_PLAYER_DELETE_FAILED, pi.handle, 1);
            peer->encode_byte(pi.account_index);
        }
    }

    void client_manager::query_add_affect(peer *peer, packet_gd_add_affect *p) {
        const std::string query = sql::format(
            "REPLACE INTO affect (dwPID, bType, bApplyOn, lApplyValue, dwFlag, lDuration, lSPCost) "
            "VALUES({}, {}, {}, {}, {}, {}, {})",
            p->pid,
            p->elem.type,
            p->elem.apply_on,
            p->elem.apply_value,
            p->elem.flag,
            p->elem.duration,
            p->elem.sp_cost);

        get_db_manager().async_query(query);
    }

    void client_manager::query_remove_affect(peer *peer, packet_gd_remove_affect *p) {
        const std::string query = sql::format(
            "DELETE FROM affect WHERE dwPID={} AND bType={} AND bApplyOn={}", p->pid, p->type, p->apply_on);

        get_db_manager().async_query(query);
    }

    void client_manager::query_highscore_register(peer *peer, packet_gd_highscore *data) {
        const std::string query = sql::format("SELECT value FROM highscore WHERE board={} AND pid = {}", data->board, data->pid);

        metin2::log::debug("Highscore: register for pid {:d}", data->pid);

        auto pi = std::make_unique<client_handle_info>(0);
        strlcpy(pi->login, data->board, sizeof(pi->login));
        pi->account_id = (uint32_t)data->value;
        pi->player_id = data->pid;
        pi->account_index = (data->dir > 0);

        get_db_manager().return_query(query, [this, handle = peer->handle(), pi = std::move(pi)](sql_msg &msg) {
            if (auto *peer_value = get_peer(handle))
                result_highscore_register(peer_value, msg, *pi);
        });
    }

    void client_manager::result_highscore_register(peer *peer_value, sql_msg &msg, client_handle_info &pi) {
        metin2::log::debug("Highscore register query finished");

        char board[21];
        strlcpy(board, pi.login, sizeof(board));
        int value = (int)pi.account_id;

        sql_result *res = msg.get();

        if (res->num_rows == 0) {
            const std::string query = sql::format("INSERT INTO highscore VALUES({}, {}, {})", board, pi.player_id, value);
            get_db_manager().async_query(query);
        } else {
            if (!res->res.get()) {
                return;
            }

            MYSQL_ROW row = mysql_fetch_row(res->res.get());
            if (row && row[0]) {
                int current_value = 0;
                str_to_number(current_value, row[0]);
                if ((pi.account_index && current_value >= value) || (!pi.account_index && current_value <= value)) {
                    value = current_value;
                } else {
                    const std::string query = sql::format("REPLACE INTO highscore VALUES({}, {}, {})", board, pi.player_id, value);
                    get_db_manager().async_query(query);
                }
            } else {
                const std::string query = sql::format("INSERT INTO highscore VALUES({}, {}, {})", board, pi.player_id, value);
                get_db_manager().async_query(query);
            }
        }
    }

    void client_manager::insert_logout_player(uint32_t pid) {
        auto it = logouts_.find(pid);

        if (it != logouts_.end()) {
            if (metin2::config::db().login_log) {
                metin2::log::info("Logout: refreshed logout time of pid {:d}", pid);
            }

            it->second->time = time(nullptr);
            return;
        }

        auto logout_value = std::make_unique<logout_player>();

        logout_value->pid = pid;
        logout_value->time = time(nullptr);

        logouts_.insert(std::make_pair(pid, std::move(logout_value)));

        if (metin2::config::db().login_log)
            metin2::log::info("Logout: recorded logout time of pid {:d}", pid);
    }

    void client_manager::delete_logout_player(uint32_t pid) {
        auto it = logouts_.find(pid);
        if (it != logouts_.end()) {
            logouts_.erase(it);
        }
    }

    void client_manager::update_logout_player() {
        time_t now = time(nullptr);

        auto it = logouts_.begin();
        while (it != logouts_.end()) {
            logout_player *logout = it->second.get();

            if (now - logout_seconds > logout->time) {
                flush_item_cache_set(logout->pid);
                flush_player_cache_set(logout->pid);

                logouts_.erase(it++);
            } else {
                ++it;
            }
        }
    }

    void client_manager::flush_player_cache_set(uint32_t pid) {
        auto it = player_caches_.find(pid);

        if (it != player_caches_.end()) {
            it->second->flush();

            player_caches_.erase(it);
        }
    }
}
