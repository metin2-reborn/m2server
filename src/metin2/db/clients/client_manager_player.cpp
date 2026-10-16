
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
#include <metin2/proto/length.h>
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
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace metin2::db::inline clients {

    // The column order matches every SELECT on the item table; change them together.
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

            // The column order matches every SELECT on the item table; change them together.
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

    auto create_player_save_query(const player_table *tab) -> std::string {
        return sql::format(
            "UPDATE player{} SET "
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
            sql::raw(get_table_postfix()),
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
        player_table_cache_map::iterator it = map_player_cache_.find(id);

        if (it == map_player_cache_.end())
            return nullptr;

        player_table *pTable = it->second->get(false);
        pTable->logoff_interval = current_time() - it->second->last_update_time();
        return it->second.get();
    }

    void client_manager::put_player_cache(player_table *updated) {
        player_table_cache *c;

        c = player_cache(updated->id);

        if (!c) {
            c = map_player_cache_.emplace(updated->id, std::make_unique<player_table_cache>()).first->second.get();
        }

        c->put(updated);
    }

    void client_manager::query_player_load(peer *peer, uint32_t handle_value, player_load_packet *packet) {
        auto return_composite_query = [this, peer_handle = peer->handle()](std::string_view query, uint32_t qid, std::unique_ptr<client_handle_info> info) {
            db_manager::instance().return_query(query, [this, peer_handle, qid, info = std::move(info)](sql_msg &msg) {
                if (auto *peer_value = get_peer(peer_handle))
                    result_composite_player(peer_value, msg, qid, *info);
            });
        };

        player_table_cache *c;
        player_table *pTab;

        login_data *pLoginData = get_login_data_by_aid(packet->account_id);

        if (pLoginData) {
            for (int n = 0; n < PLAYER_PER_ACCOUNT; ++n)
                if (pLoginData->account_ref().players[n].id != 0)
                    delete_logout_player(pLoginData->account_ref().players[n].id);
        }

        if ((c = player_cache(packet->player_id))) {
            login_data *pkLD = get_login_data_by_aid(packet->account_id);

            if (!pkLD || pkLD->is_play()) {
                metin2::log::warn("Cannot load player pid {:d} from cache: login data {}, already playing {:d}", packet->player_id, static_cast<const void *>(pkLD), pkLD ? pkLD->is_play() : 0);
                peer->encode_header(HEADER_DG_PLAYER_LOAD_FAILED, handle_value, 0);
                return;
            }

            pTab = c->get();

            pkLD->set_play(true);
            std::memcpy(pTab->premium_times, pkLD->premium_ptr(), sizeof(pTab->premium_times));

            peer->encode_header(HEADER_DG_PLAYER_LOAD_SUCCESS, handle_value, sizeof(player_table));
            peer->encode(pTab, sizeof(player_table));

            if (packet->player_id != pkLD->last_player_id()) {
                packet_need_login_log_info logInfo;
                logInfo.player_id = packet->player_id;

                pkLD->set_last_player_id(packet->player_id);

                peer->encode_header(HEADER_DG_NEED_LOGIN_LOG, handle_value, sizeof(packet_need_login_log_info));
                peer->encode(&logInfo, sizeof(packet_need_login_log_info));
            }

            std::string query;

            item_cache_set *pSet = get_item_cache_set(pTab->id);

            metin2::log::info("Player {} (pid {:d}) loaded from cache, gold {:d}", pTab->name, pTab->id, pTab->gold);

            if (pSet) {
                static std::vector<player_item> s_items;
                s_items.resize(pSet->size());

                uint32_t dwCount = 0;
                item_cache_set::iterator it = pSet->begin();

                while (it != pSet->end()) {
                    item_cache *c = *it++;
                    player_item *p = c->get();

                    if (p->vnum)
                        std::memcpy(&s_items[dwCount++], p, sizeof(player_item));
                }

                if (metin2::config::test_server())
                    metin2::log::debug("Item cache hit for player {}: {:d} items", pTab->name, dwCount);

                peer->encode_header(HEADER_DG_ITEM_LOAD, handle_value, sizeof(uint32_t) + sizeof(player_item) * dwCount);
                peer->encode_dword(dwCount);

                if (dwCount)
                    peer->encode(&s_items[0], sizeof(player_item) * dwCount);

                query = sql::format("SELECT dwPID,szName,szState,lValue FROM quest{} WHERE dwPID={} AND lValue<>0",
                    sql::raw(get_table_postfix()), pTab->id);

                return_composite_query(query, QID_QUEST, std::make_unique<client_handle_info>(handle_value, 0, packet->account_id));

                query = sql::format("SELECT dwPID,bType,bApplyOn,lApplyValue,dwFlag,lDuration,lSPCost FROM affect{} WHERE dwPID={}",
                    sql::raw(get_table_postfix()), pTab->id);
                return_composite_query(query, QID_AFFECT, std::make_unique<client_handle_info>(handle_value));
            } else {
                query = sql::format("SELECT id,window+0,pos,count,vnum,socket0,socket1,socket2,attrtype0,attrvalue0,attrtype1,attrvalue1,attrtype2,attrvalue2,attrtype3,attrvalue3,attrtype4,attrvalue4,attrtype5,attrvalue5,attrtype6,attrvalue6 "
                    "FROM item{} WHERE owner_id={} AND (window < {} or window = {})",
                    sql::raw(get_table_postfix()), pTab->id, SAFEBOX, DRAGON_SOUL_INVENTORY);

                return_composite_query(query, QID_ITEM, std::make_unique<client_handle_info>(handle_value, pTab->id));
                query = sql::format("SELECT dwPID, szName, szState, lValue FROM quest{} WHERE dwPID={}",
                    sql::raw(get_table_postfix()), pTab->id);

                return_composite_query(query, QID_QUEST, std::make_unique<client_handle_info>(handle_value, pTab->id));
                query = sql::format("SELECT dwPID, bType, bApplyOn, lApplyValue, dwFlag, lDuration, lSPCost FROM affect{} WHERE dwPID={}",
                    sql::raw(get_table_postfix()), pTab->id);

                return_composite_query(query, QID_AFFECT, std::make_unique<client_handle_info>(handle_value, pTab->id));
            }
        } else {
            metin2::log::info("Loading player pid {:d} from the database", packet->player_id);

            std::string query = sql::format("SELECT "
                "id,name,job,voice,dir,x,y,z,map_index,exit_x,exit_y,exit_map_index,hp,mp,stamina,random_hp,random_sp,playtime,"
                "gold,level,level_step,st,ht,dx,iq,exp,"
                "stat_point,skill_point,sub_skill_point,stat_reset_count,part_base,part_hair,"
                "skill_level,quickslot,skill_group,alignment,mobile,horse_level,horse_riding,horse_hp,horse_hp_droptime,horse_stamina,"
                "UNIX_TIMESTAMP(NOW())-UNIX_TIMESTAMP(last_play),horse_skill_point FROM player{} WHERE id={}",
                sql::raw(get_table_postfix()), packet->player_id);

            auto pkInfo = std::make_unique<client_handle_info>(handle_value, packet->player_id);
            pkInfo->account_id = packet->account_id;
            return_composite_query(query, QID_PLAYER, std::move(pkInfo));

            query = sql::format("SELECT id,window+0,pos,count,vnum,socket0,socket1,socket2,attrtype0,attrvalue0,attrtype1,attrvalue1,attrtype2,attrvalue2,attrtype3,attrvalue3,attrtype4,attrvalue4,attrtype5,attrvalue5,attrtype6,attrvalue6 "
                "FROM item{} WHERE owner_id={} AND (window < {} or window = {})",
                sql::raw(get_table_postfix()), packet->player_id, SAFEBOX, DRAGON_SOUL_INVENTORY);
            return_composite_query(query, QID_ITEM, std::make_unique<client_handle_info>(handle_value, packet->player_id));

            query = sql::format("SELECT dwPID,szName,szState,lValue FROM quest{} WHERE dwPID={}",
                sql::raw(get_table_postfix()), packet->player_id);
            return_composite_query(query, QID_QUEST, std::make_unique<client_handle_info>(handle_value, packet->player_id, packet->account_id));
            query = sql::format("SELECT dwPID,bType,bApplyOn,lApplyValue,dwFlag,lDuration,lSPCost FROM affect{} WHERE dwPID={}",
                sql::raw(get_table_postfix()), packet->player_id);
            return_composite_query(query, QID_AFFECT, std::make_unique<client_handle_info>(handle_value, packet->player_id));
        }
    }
    void client_manager::item_award(peer *peer, char *login) {
        std::string login_t = login;
        std::set<db::items::item_award *> *pSet = item_award_manager::instance().by_login(login_t.c_str());
        if (pSet == nullptr)
            return;
        __typeof(pSet->begin()) it = pSet->begin();
        while (it != pSet->end()) {
            db::items::item_award *pItemAward = *(it++);
            inform_gift_award(pItemAward->login, pItemAward->why, pItemAward->vnum);
        }
    }

    void client_manager::inform_gift_award(const char *login, const char *why, uint32_t vnum) {
        const std::string_view text(why);
        if (!text.starts_with("[GIFT") || (text.size() > 5 && text[5] != ']'))
            return;

        packet_item_award_infromer giftData{};
        strlcpy(giftData.login, login, sizeof(giftData.login));
        strlcpy(giftData.command, "GIFT", sizeof(giftData.command));
        giftData.vnum = vnum;
        forward_packet(HEADER_DG_ITEMAWARD_INFORMER, &giftData, sizeof(giftData));
    }

    auto create_player_table_from_res(MYSQL_RES *res, player_table *tab) -> bool {
        if (mysql_num_rows(res) == 0)
            return false;

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
        if (row[col])
            std::memcpy(tab->skills, row[col], std::min<size_t>(lengths[col], sizeof(tab->skills)));

        col++;

        std::memset(tab->quickslot, 0, sizeof(tab->quickslot));
        if (row[col])
            std::memcpy(tab->quickslot, row[col], std::min<size_t>(lengths[col], sizeof(tab->quickslot)));

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
            } else
                tab->sub_skill_point = 0;
        }

        return true;
    }

    void client_manager::result_composite_player(peer *peer, sql_msg &msg, uint32_t qid, client_handle_info &info) {
        MYSQL_RES *pSQLResult = msg.get()->res;
        if (!pSQLResult) {
            metin2::log::error("Query {:d} returned no result set", qid);
            return;
        }

        switch (qid) {
        case QID_PLAYER:
            metin2::log::debug("Player query result for handle {:d}, pid {:d}", info.handle, info.player_id);
            result_player_load(peer, pSQLResult, &info);

            break;

        case QID_ITEM:
            metin2::log::debug("Item query result for handle {:d}", info.handle);
            result_item_load(peer, pSQLResult, info.handle, info.player_id);
            break;

        case QID_QUEST:
            {
                metin2::log::debug("Quest query result for handle {:d}", info.handle);
                result_quest_load(peer, pSQLResult, info.handle, info.player_id);

                login_data *pLoginData1 = get_login_data_by_aid(info.account_id);
                if (pLoginData1 == nullptr)
                    break;
                item_award(peer, pLoginData1->account_ref().login);
            }
            break;

        case QID_AFFECT:
            metin2::log::debug("Affect query result for handle {:d}", info.handle);
            result_affect_load(peer, pSQLResult, info.handle);
            break;
        }
    }

    void client_manager::result_player_load(peer *peer, MYSQL_RES *res, client_handle_info *info_value) {
        player_table tab;

        if (!create_player_table_from_res(res, &tab)) {
            peer->encode_header(HEADER_DG_PLAYER_LOAD_FAILED, info_value->handle, 0);
            return;
        }

        login_data *pkLD = get_login_data_by_aid(info_value->account_id);

        if (!pkLD || pkLD->is_play()) {
            metin2::log::warn("Cannot load player: login data {}, already playing {:d}", static_cast<const void *>(pkLD), pkLD ? pkLD->is_play() : 0);
            peer->encode_header(HEADER_DG_PLAYER_LOAD_FAILED, info_value->handle, 0);
            return;
        }

        pkLD->set_play(true);
        std::memcpy(tab.premium_times, pkLD->premium_ptr(), sizeof(tab.premium_times));

        peer->encode_header(HEADER_DG_PLAYER_LOAD_SUCCESS, info_value->handle, sizeof(player_table));
        peer->encode(&tab, sizeof(player_table));

        if (tab.id != pkLD->last_player_id()) {
            packet_need_login_log_info logInfo;
            logInfo.player_id = tab.id;

            pkLD->set_last_player_id(tab.id);

            peer->encode_header(HEADER_DG_NEED_LOGIN_LOG, info_value->handle, sizeof(packet_need_login_log_info));
            peer->encode(&logInfo, sizeof(packet_need_login_log_info));
        }
    }

    void client_manager::result_item_load(peer *peer, MYSQL_RES *res, uint32_t handle, uint32_t pid) {
        static std::vector<player_item> s_items;
        create_item_table_from_res(res, &s_items, pid);
        uint32_t dwCount = s_items.size();

        peer->encode_header(HEADER_DG_ITEM_LOAD, handle, sizeof(uint32_t) + sizeof(player_item) * dwCount);
        peer->encode_dword(dwCount);

        create_item_cache_set(pid);

        metin2::log::debug("Loaded {:d} items for pid {:d}", dwCount, pid);

        if (dwCount) {
            peer->encode(&s_items[0], sizeof(player_item) * dwCount);

            for (uint32_t i = 0; i < dwCount; ++i)
                put_item_cache(&s_items[i], true);
        }
    }

    void client_manager::result_affect_load(peer *peer, MYSQL_RES *res, uint32_t handle) {
        int iNumRows;

        if ((iNumRows = mysql_num_rows(res)) == 0)
            return;

        static std::vector<packet_affect_element> s_elements;
        s_elements.resize(iNumRows);

        uint32_t dwPID = 0;

        MYSQL_ROW row;

        for (int i = 0; i < iNumRows; ++i) {
            packet_affect_element &r = s_elements[i];
            row = mysql_fetch_row(res);

            if (dwPID == 0)
                str_to_number(dwPID, row[0]);

            str_to_number(r.type, row[1]);
            str_to_number(r.apply_on, row[2]);
            str_to_number(r.apply_value, row[3]);
            str_to_number(r.flag, row[4]);
            str_to_number(r.duration, row[5]);
            str_to_number(r.sp_cost, row[6]);
        }

        metin2::log::debug("Loaded {:d} affects for pid {:d}", s_elements.size(), dwPID);

        uint32_t dwCount = s_elements.size();

        peer->encode_header(HEADER_DG_AFFECT_LOAD, handle, sizeof(uint32_t) + sizeof(uint32_t) + sizeof(packet_affect_element) * dwCount);
        peer->encode(&dwPID, sizeof(uint32_t));
        peer->encode(&dwCount, sizeof(uint32_t));
        peer->encode(&s_elements[0], sizeof(packet_affect_element) * dwCount);
    }

    void client_manager::result_quest_load(peer *peer, MYSQL_RES *res, uint32_t handle, uint32_t pid) {
        int iNumRows;

        if ((iNumRows = mysql_num_rows(res)) == 0) {
            uint32_t dwCount = 0;
            peer->encode_header(HEADER_DG_QUEST_LOAD, handle, sizeof(uint32_t));
            peer->encode(&dwCount, sizeof(uint32_t));
            return;
        }

        static std::vector<quest_table> s_table;
        s_table.resize(iNumRows);

        MYSQL_ROW row;

        for (int i = 0; i < iNumRows; ++i) {
            quest_table &r = s_table[i];

            row = mysql_fetch_row(res);

            str_to_number(r.pid, row[0]);
            strlcpy(r.name, row[1], sizeof(r.name));
            strlcpy(r.state, row[2], sizeof(r.state));
            str_to_number(r.value, row[3]);
        }

        metin2::log::debug("Loaded {:d} quest flags for pid {:d}", s_table.size(), s_table[0].pid);

        uint32_t dwCount = s_table.size();

        peer->encode_header(HEADER_DG_QUEST_LOAD, handle, sizeof(uint32_t) + sizeof(quest_table) * dwCount);
        peer->encode(&dwCount, sizeof(uint32_t));
        peer->encode(&s_table[0], sizeof(quest_table) * dwCount);
    }

    void client_manager::query_player_save(peer *peer, uint32_t handle, player_table *tab) {
        if (metin2::config::test_server())
            metin2::log::debug("Saving player {}", tab->name);

        put_player_cache(tab);
    }

    typedef std::map<uint32_t, time_t> time_by_id_map_t;
    static time_by_id_map_t create_time_by_account_id;

    void client_manager::query_player_create(peer *peer, uint32_t handle, player_create_packet *packet) {
        int player_id;

        time_by_id_map_t::iterator it = create_time_by_account_id.find(packet->account_id);

        if (it != create_time_by_account_id.end()) {
            time_t curtime = time(nullptr);

            if (curtime - it->second < 30) {
                peer->encode_header(HEADER_DG_PLAYER_CREATE_FAILED, handle, 0);
                return;
            }
        }

        const auto postfix = sql::raw(get_table_postfix());

        std::string query = sql::format(
            "SELECT pid{} FROM player_index{} WHERE id={}", packet->account_index + 1, postfix, packet->account_id);

        auto pMsg0 = db_manager::instance().direct_query(query);

        if (pMsg0->get()->num_rows != 0) {
            if (!pMsg0->get()->res) {
                peer->encode_header(HEADER_DG_PLAYER_CREATE_FAILED, handle, 0);
                return;
            }

            MYSQL_ROW row = mysql_fetch_row(pMsg0->get()->res);

            uint32_t dwPID = 0;
            str_to_number(dwPID, row[0]);
            if (row[0] && dwPID > 0) {
                peer->encode_header(HEADER_DG_PLAYER_CREATE_ALREADY, handle, 0);
                metin2::log::info("Player create: slot {:d} already holds pid {:d}", packet->account_index, dwPID);
                return;
            }
        } else {
            peer->encode_header(HEADER_DG_PLAYER_CREATE_FAILED, handle, 0);
            return;
        }

        query = sql::format(
            "SELECT COUNT(*) as count FROM player{} WHERE name={}", postfix, packet->player_table.name);

        auto pMsg1 = db_manager::instance().direct_query(query);

        if (pMsg1->get()->num_rows) {
            if (!pMsg1->get()->res) {
                peer->encode_header(HEADER_DG_PLAYER_CREATE_FAILED, handle, 0);
                return;
            }

            MYSQL_ROW row = mysql_fetch_row(pMsg1->get()->res);

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
            "INSERT INTO player{} "
            "(id, account_id, name, level, st, ht, dx, iq, "
            "job, voice, dir, x, y, z, "
            "hp, mp, random_hp, random_sp, stat_point, stamina, part_base, part_main, part_hair, gold, playtime, "
            "skill_level, quickslot) "
            "VALUES(0, {}, {}, {}, {}, {}, {}, {}, "
            "{}, {}, {}, {}, {}, {}, {}, "
            "{}, {}, {}, {}, {}, {}, {}, 0, {}, 0, "
            "{}, {})",
            postfix,
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

        auto pMsg2 = db_manager::instance().direct_query(query);
        if (metin2::config::test_server())
            metin2::log::debug("Player create: query length {:d}", query.size());

        if (pMsg2->get()->affected_rows <= 0) {
            peer->encode_header(HEADER_DG_PLAYER_CREATE_ALREADY, handle, 0);
            metin2::log::warn("Player create: insert affected no rows, treating name as taken (query: {}, affected rows {:d})", query, pMsg2->get()->affected_rows);
            return;
        }

        player_id = pMsg2->get()->insert_id;

        query = sql::format("UPDATE player_index{} SET pid{}={} WHERE id={}",
            postfix, packet->account_index + 1, player_id, packet->account_id);
        auto pMsg3 = db_manager::instance().direct_query(query);

        if (pMsg3->get()->affected_rows <= 0) {
            metin2::log::error("Player create: cannot update player_index, rolling back: {}", query);

            query = sql::format("DELETE FROM player{} WHERE id={}", postfix, player_id);
            db_manager::instance().direct_query(query);

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
        if (!packet->login[0] || !packet->player_id || packet->account_index >= PLAYER_PER_ACCOUNT)
            return;

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

        player_table_cache *pkPlayerCache = player_cache(packet->player_id);
        if (pkPlayerCache) {
            player_table *pTab = pkPlayerCache->get();

            if (pTab->level >= player_delete_level_limit_) {
                metin2::log::info("Player delete: pid {:d} level {:d} is at or above the limit {:d}", packet->player_id, pTab->level, player_delete_level_limit_);
                peer->encode_header(HEADER_DG_PLAYER_DELETE_FAILED, handle_value, 1);
                peer->encode_byte(packet->account_index);
                return;
            }

            if (pTab->level < player_delete_level_limit_lower_) {
                metin2::log::info("Player delete: pid {:d} level {:d} is below the minimum {:d}", packet->player_id, pTab->level, player_delete_level_limit_lower_);
                peer->encode_header(HEADER_DG_PLAYER_DELETE_FAILED, handle_value, 1);
                peer->encode_byte(packet->account_index);
                return;
            }
        }

        const std::string query = sql::format("SELECT p.id, p.level, p.name FROM player_index{0} AS i, player{0} AS p WHERE pid{1}={2} AND pid{1}=p.id",
            sql::raw(get_table_postfix()), packet->account_index + 1, packet->player_id);

        auto pi = std::make_unique<client_handle_info>(handle_value, packet->player_id);
        pi->account_index = packet->account_index;

        metin2::log::info("Player delete: account {} requested deletion of pid {:d} (slot {:d})", packet->login, packet->player_id, packet->account_index + 1);
        db_manager::instance().return_query(query, [this, handle = peer->handle(), pi = std::move(pi)](sql_msg &msg) {
            if (auto *peer_value = get_peer(handle))
                result_player_delete(peer_value, msg, *pi);
        });
    }

    void client_manager::result_player_delete(peer *peer, sql_msg &msg, client_handle_info &pi) {
        if (msg.get() && msg.get()->num_rows) {
            MYSQL_ROW row = mysql_fetch_row(msg.get()->res);

            uint32_t dwPID = 0;
            str_to_number(dwPID, row[0]);

            int deletedLevelLimit = 0;
            str_to_number(deletedLevelLimit, row[1]);

            std::string strName = row[2];

            if (deletedLevelLimit >= player_delete_level_limit_) {
                metin2::log::info("Player delete: pid {:d} level {:d} is at or above the limit {:d}", dwPID, deletedLevelLimit, player_delete_level_limit_);
                peer->encode_header(HEADER_DG_PLAYER_DELETE_FAILED, pi.handle, 1);
                peer->encode_byte(pi.account_index);
                return;
            }

            if (deletedLevelLimit < player_delete_level_limit_lower_) {
                metin2::log::info("Player delete: pid {:d} level {:d} is below the minimum {:d}", dwPID, deletedLevelLimit, player_delete_level_limit_lower_);
                peer->encode_header(HEADER_DG_PLAYER_DELETE_FAILED, pi.handle, 1);
                peer->encode_byte(pi.account_index);
                return;
            }

            const auto postfix = sql::raw(get_table_postfix());

            std::string query = sql::format("INSERT INTO player{}_deleted SELECT * FROM player{} WHERE id={}",
                postfix, postfix, pi.player_id);
            auto pIns = db_manager::instance().direct_query(query);

            if (pIns->get()->affected_rows == 0 || pIns->get()->affected_rows == (uint32_t)-1) {
                metin2::log::error("Player delete: cannot copy pid {:d} into player{}_deleted", dwPID, get_table_postfix());

                peer->encode_header(HEADER_DG_PLAYER_DELETE_FAILED, pi.handle, 1);
                peer->encode_byte(pi.account_index);
                return;
            }

            metin2::log::info("Player delete: deleted pid {:d}", dwPID);

            player_table_cache *pkPlayerCache = player_cache(pi.player_id);

            if (pkPlayerCache) {
                map_player_cache_.erase(pi.player_id);
            }

            item_cache_set *pSet = get_item_cache_set(pi.player_id);

            if (pSet) {
                item_cache_set::iterator it = pSet->begin();

                while (it != pSet->end()) {
                    item_cache *pkItemCache = *it++;
                    delete_item_cache(pkItemCache->get()->id);
                }

                pSet->clear();

                map_item_cache_set_ptr_.erase(pi.player_id);
            }

            query = sql::format("UPDATE player_index{} SET pid{}=0 WHERE pid{}={}",
                postfix,
                pi.account_index + 1,
                pi.account_index + 1,
                pi.player_id);

            auto pMsg = db_manager::instance().direct_query(query);

            if (pMsg->get()->affected_rows == 0 || pMsg->get()->affected_rows == (uint32_t)-1) {
                metin2::log::error("Player delete: cannot clear pid {:d} from player_index", pi.player_id);
                peer->encode_header(HEADER_DG_PLAYER_DELETE_FAILED, pi.handle, 1);
                peer->encode_byte(pi.account_index);
                return;
            }

            query = sql::format("DELETE FROM player{} WHERE id={}", postfix, pi.player_id);
            db_manager::instance().direct_query(query);

            query = sql::format("DELETE FROM item{} WHERE owner_id={} AND (window < {} or window = {})", postfix, pi.player_id, SAFEBOX, DRAGON_SOUL_INVENTORY);
            db_manager::instance().direct_query(query);

            query = sql::format("DELETE FROM quest{} WHERE dwPID={}", postfix, pi.player_id);
            db_manager::instance().async_query(query);

            query = sql::format("DELETE FROM affect{} WHERE dwPID={}", postfix, pi.player_id);
            db_manager::instance().async_query(query);

            query = sql::format("DELETE FROM guild_member{} WHERE pid={}", postfix, pi.player_id);
            db_manager::instance().async_query(query);

            query = sql::format("DELETE FROM myshop_pricelist{} WHERE owner_id={}", postfix, pi.player_id);
            db_manager::instance().async_query(query);

            query = sql::format("DELETE FROM messenger_list{} WHERE account={} OR companion={}", postfix, strName, strName);
            db_manager::instance().async_query(query);

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
            "REPLACE INTO affect{} (dwPID, bType, bApplyOn, lApplyValue, dwFlag, lDuration, lSPCost) "
            "VALUES({}, {}, {}, {}, {}, {}, {})",
            sql::raw(get_table_postfix()),
            p->pid,
            p->elem.type,
            p->elem.apply_on,
            p->elem.apply_value,
            p->elem.flag,
            p->elem.duration,
            p->elem.sp_cost);

        db_manager::instance().async_query(query);
    }

    void client_manager::query_remove_affect(peer *peer, packet_gd_remove_affect *p) {
        const std::string query = sql::format(
            "DELETE FROM affect{} WHERE dwPID={} AND bType={} AND bApplyOn={}",
            sql::raw(get_table_postfix()), p->pid, p->type, p->apply_on);

        db_manager::instance().async_query(query);
    }

    void client_manager::query_highscore_register(peer *peer, packet_gd_highscore *data) {
        const std::string query = sql::format("SELECT value FROM highscore{} WHERE board={} AND pid = {}", sql::raw(get_table_postfix()), data->board, data->pid);

        metin2::log::debug("Highscore: register for pid {:d}", data->pid);

        auto pi = std::make_unique<client_handle_info>(0);
        strlcpy(pi->login, data->board, sizeof(pi->login));
        pi->account_id = (uint32_t)data->value;
        pi->player_id = data->pid;
        pi->account_index = (data->c_dir > 0);

        db_manager::instance().return_query(query, [this, handle = peer->handle(), pi = std::move(pi)](sql_msg &msg) {
            if (auto *peer_value = get_peer(handle))
                result_highscore_register(peer_value, msg, *pi);
        });
    }

    void client_manager::result_highscore_register(peer *peer_value, sql_msg &msg, client_handle_info &pi) {
        metin2::log::debug("Highscore register query finished");

        char szBoard[21];
        strlcpy(szBoard, pi.login, sizeof(szBoard));
        int value = (int)pi.account_id;

        sql_result *res = msg.get();

        if (res->num_rows == 0) {
            const std::string query = sql::format("INSERT INTO highscore{} VALUES({}, {}, {})", sql::raw(get_table_postfix()), szBoard, pi.player_id, value);
            db_manager::instance().async_query(query);
        } else {
            if (!res->res)
                return;

            MYSQL_ROW row = mysql_fetch_row(res->res);
            if (row && row[0]) {
                int current_value = 0;
                str_to_number(current_value, row[0]);
                if ((pi.account_index && current_value >= value) || (!pi.account_index && current_value <= value)) {
                    value = current_value;
                } else {
                    const std::string query = sql::format("REPLACE INTO highscore{} VALUES({}, {}, {})", sql::raw(get_table_postfix()), szBoard, pi.player_id, value);
                    db_manager::instance().async_query(query);
                }
            } else {
                const std::string query = sql::format("INSERT INTO highscore{} VALUES({}, {}, {})", sql::raw(get_table_postfix()), szBoard, pi.player_id, value);
                db_manager::instance().async_query(query);
            }
        }
    }

    void client_manager::insert_logout_player(uint32_t pid) {
        logout_player_map::iterator it = map_logout_.find(pid);

        if (it != map_logout_.end()) {
            if (metin2::config::db().login_log)
                metin2::log::info("Logout: refreshed logout time of pid {:d}", pid);

            it->second->time = time(nullptr);
            return;
        }

        auto pLogout = std::make_unique<logout_player>();
        pLogout->pid = pid;
        pLogout->time = time(nullptr);
        map_logout_.insert(std::make_pair(pid, std::move(pLogout)));

        if (metin2::config::db().login_log)
            metin2::log::info("Logout: recorded logout time of pid {:d}", pid);
    }

    void client_manager::delete_logout_player(uint32_t pid) {
        logout_player_map::iterator it = map_logout_.find(pid);

        if (it != map_logout_.end()) {
            map_logout_.erase(it);
        }
    }

    void client_manager::update_logout_player() {
        time_t now = time(nullptr);

        logout_player_map::iterator it = map_logout_.begin();

        while (it != map_logout_.end()) {
            logout_player *pLogout = it->second.get();

            if (now - logout_seconds > pLogout->time) {
                flush_item_cache_set(pLogout->pid);
                flush_player_cache_set(pLogout->pid);

                map_logout_.erase(it++);
            } else
                ++it;
        }
    }

    void client_manager::flush_player_cache_set(uint32_t pid) {
        player_table_cache_map::iterator it = map_player_cache_.find(pid);

        if (it != map_player_cache_.end()) {
            it->second->flush();
            map_player_cache_.erase(it);
        }
    }
}
