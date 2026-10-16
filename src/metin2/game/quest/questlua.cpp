
#include <metin2/config/config.h>
#include <metin2/game/quest/questlua.h>

#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/quest.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/quest/questpc.h>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/common/utils.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/length.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <numbers>
#include <sstream>
#include <string>

#include <dirent.h>

namespace metin2::game {

    namespace quest {
        auto script_to_string(const std::string &str) -> std::string {
            lua_State *L = quest_manager::instance().lua_state();
            int x = lua_gettop(L);

            int errcode = lua_dobuffer(L, ("return " + str).c_str(), str.size() + 7, "ScriptToString");
            std::string retstr;
            if (!errcode) {
                if (lua_isstring(L, -1))
                    retstr = lua_tostring(L, -1);
            } else {
                quest::error("Lua script failed (code {:d}): {}", errcode, str.c_str());
            }
            lua_settop(L, x);
            return retstr;
        }

        void set_pc_quest_flag(character *ch, const std::string &flagname, int value) {
            if (!ch->is_pc())
                return;

            pc *pPC = quest_manager::instance().pc_force(ch->player_id());

            if (pPC)
                pPC->set_flag(flagname, value);
        }

        void purge_monsters_in_area(sectree_map *map, int x1, int y1, int x2, int y2, character *except_char) {
            map->for_each([&](entity *ent) {
                if (true == ent->is_type(ENTITY_CHARACTER)) {
                    character *pChar = static_cast<character *>(ent);

                    if (pChar == except_char)
                        return;

                    if (!pChar->is_pet() && (true == pChar->is_monster() || true == pChar->is_stone())) {
                        if (x1 <= pChar->x() && pChar->x() <= x2 && y1 <= pChar->y() && pChar->y() <= y2) {
                            character_manager::instance().destroy_character(pChar);
                        }
                    }
                }
            });
        }

        void send_buffer_to_map(sectree_map *map, const temp_buffer &buf, uint8_t empire) {
            map->for_each([&](entity *ent) {
                if (!ent->is_type(ENTITY_CHARACTER))
                    return;

                character *ch = static_cast<character *>(ent);
                if (ch->get_desc() && (empire == 0 || ch->empire() == empire))
                    ch->get_desc()->packet(buf.read_peek(), buf.size());
            });
        }

        auto is_script_true(const char *code, int size) -> bool {
            if (size == 0)
                return true;

            lua_State *L = quest_manager::instance().lua_state();
            int x = lua_gettop(L);
            int errcode = lua_dobuffer(L, code, size, "IsScriptTrue");
            int bStart = lua_toboolean(L, -1);
            if (errcode) {
                quest::error("Lua script failed (code {:d}): {}", errcode, std::string_view(code, static_cast<int>(size)));
            }
            lua_settop(L, x);
            return bStart != 0;
        }

        void combine_lua_string(lua_State *lua, std::ostringstream &s) {
            int n = lua_gettop(lua);
            int i;

            for (i = 1; i <= n; ++i) {
                if (lua_isstring(lua, i))
                    s << lua_tostring(lua, i);
                else if (lua_isnumber(lua, i)) {
                    s << std::format("{:.14g}\n", lua_tonumber(lua, i));
                }
            }
        }

        auto highscore_show(lua_State *lua) -> int {
            const char *pszBoardName = lua_isstring(lua, 1) ? lua_tostring(lua, 1) : "";
            bool bOrder = (int)lua_tonumber(lua, 2) != 0 ? true : false;

            db_manager::instance().func_query([](sql_msg &) {}, sql::format("SELECT h.pid, p.name, h.value FROM highscore{} as h, player{} as p WHERE h.board = {} AND h.pid = p.id ORDER BY h.value {} LIMIT 10", sql::raw(get_table_postfix()), sql::raw(get_table_postfix()), pszBoardName, sql::raw(bOrder ? "DESC" : "")));
            return 0;
        }

        auto highscore_register(lua_State *lua) -> int {
            quest_manager &q = quest_manager::instance();

            char board[20 + 1];
            strlcpy(board, lua_tostring(lua, 1), sizeof(board));
            uint32_t pid = q.current_character_ptr()->player_id();
            int value = (int)lua_tonumber(lua, 2);
            bool order = (int)lua_tonumber(lua, 3);

            db_manager::instance().func_query([board = std::string(board), pid, value, order](sql_msg &msg) {
                bool bQuery = true;

                if (msg.get()->num_rows) {
                    MYSQL_ROW row = mysql_fetch_row(msg.get()->res);

                    if (row && row[0]) {
                        int iCur = 0;
                        str_to_number(iCur, row[0]);

                        if ((order && iCur >= value) ||
                            (!order && iCur <= value))
                            bQuery = false;
                    }
                }

                if (bQuery)
                    db_manager::instance().query(sql::format("REPLACE INTO highscore{} VALUES({}, {}, {})",
                        sql::raw(get_table_postfix()), board, pid, value));
            },
                sql::format("SELECT value FROM highscore{} WHERE board={} AND pid={}", sql::raw(get_table_postfix()), board, pid));
            return 0;
        }

        auto member_chat(lua_State *lua) -> int {
            std::ostringstream s;
            combine_lua_string(lua, s);
            quest_manager::instance().current_party_member()->chat_packet(CHAT_TYPE_TALKING, "%s", s.str().c_str());
            return 0;
        }

        auto member_clear_ready(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_party_member();
            ch->remove_affect(AFFECT_DUNGEON_READY);
            return 0;
        }

        auto member_set_ready(lua_State *lua) -> int {
            character *ch = quest_manager::instance().current_party_member();
            ch->add_affect(AFFECT_DUNGEON_READY, POINT_NONE, 0, AFF_DUNGEON_READY, 65535, 0, true);
            return 0;
        }

        auto mob_spawn(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3) || !lua_isnumber(lua, 4)) {
                quest::error("Invalid arguments");
                return 0;
            }

            uint32_t mob_vnum = (uint32_t)lua_tonumber(lua, 1);
            int32_t local_x = (int32_t)lua_tonumber(lua, 2) * 100;
            int32_t local_y = (int32_t)lua_tonumber(lua, 3) * 100;
            float radius = (float)lua_tonumber(lua, 4) * 100;
            bool bAggressive = lua_toboolean(lua, 5);
            uint32_t count = (lua_isnumber(lua, 6)) ? (uint32_t)lua_tonumber(lua, 6) : 1;

            if (count == 0)
                count = 1;
            else if (count > 10) {
                quest::error("Count {:d} capped at 10", count);
                count = 10;
            }

            character *ch = quest_manager::instance().current_character_ptr();
            sectree_map *pMap = sectree_manager::instance().map(ch->map_index());
            if (pMap == nullptr) {
                return 0;
            }
            uint32_t dwQuestIdx = quest_manager::instance().current_pc()->current_quest_index();

            bool ret = false;
            character *mob = nullptr;

            while (count--) {
                for (int loop = 0; loop < 8; ++loop) {
                    float angle = number(0, 999) * std::numbers::pi * 2 / 1000;
                    float r = number(0, 999) * radius / 1000;

                    int32_t x = local_x + pMap->setting.base_x + (int32_t)(r * cos(angle));
                    int32_t y = local_y + pMap->setting.base_y + (int32_t)(r * sin(angle));

                    mob = character_manager::instance().spawn_mob(mob_vnum, ch->map_index(), x, y, 0);

                    if (mob)
                        break;
                }

                if (mob) {
                    if (bAggressive)
                        mob->set_aggressive();

                    mob->set_quest_by(dwQuestIdx);

                    if (!ret) {
                        ret = true;
                        lua_pushnumber(lua, (uint32_t)mob->get_vid());
                    }
                }
            }

            if (!ret)
                lua_pushnumber(lua, 0);

            return 1;
        }

        auto mob_spawn_group(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3) || !lua_isnumber(lua, 4) || !lua_isnumber(lua, 6)) {
                quest::error("Invalid arguments");
                lua_pushnumber(lua, 0);
                return 1;
            }

            uint32_t group_vnum = (uint32_t)lua_tonumber(lua, 1);
            int32_t local_x = (int32_t)lua_tonumber(lua, 2) * 100;
            int32_t local_y = (int32_t)lua_tonumber(lua, 3) * 100;
            float radius = (float)lua_tonumber(lua, 4) * 100;
            bool bAggressive = lua_toboolean(lua, 5);
            uint32_t count = (uint32_t)lua_tonumber(lua, 6);

            if (count == 0)
                count = 1;
            else if (count > 10) {
                quest::error("Count {:d} capped at 10", count);
                count = 10;
            }

            character *ch = quest_manager::instance().current_character_ptr();
            sectree_map *pMap = sectree_manager::instance().map(ch->map_index());
            if (pMap == nullptr) {
                lua_pushnumber(lua, 0);
                return 1;
            }
            uint32_t dwQuestIdx = quest_manager::instance().current_pc()->current_quest_index();

            bool ret = false;
            character *mob = nullptr;

            while (count--) {
                for (int loop = 0; loop < 8; ++loop) {
                    float angle = number(0, 999) * std::numbers::pi * 2 / 1000;
                    float r = number(0, 999) * radius / 1000;

                    int32_t x = local_x + pMap->setting.base_x + (int32_t)(r * cos(angle));
                    int32_t y = local_y + pMap->setting.base_y + (int32_t)(r * sin(angle));

                    mob = character_manager::instance().spawn_group(group_vnum, ch->map_index(), x, y, x, y, nullptr, bAggressive);

                    if (mob)
                        break;
                }

                if (mob) {
                    mob->set_quest_by(dwQuestIdx);

                    if (!ret) {
                        ret = true;
                        lua_pushnumber(lua, (uint32_t)mob->get_vid());
                    }
                }
            }

            if (!ret)
                lua_pushnumber(lua, 0);

            return 1;
        }

        void quest_manager::add_lua_function_table(const char *name_value, luaL_reg *preg) {
            lua_newtable(l_);

            while ((preg->name)) {
                lua_pushstring(l_, preg->name);
                lua_pushcfunction(l_, preg->func);
                lua_rawset(l_, -3);
                preg++;
            }

            lua_setglobal(l_, name_value);
        }

        void quest_manager::build_state_index_to_name(const char *quest_name) {
            int x = lua_gettop(l_);
            lua_getglobal(l_, quest_name);

            if (lua_isnil(l_, -1)) {
                quest::error("Bad state file for quest {}", quest_name);
                lua_settop(l_, x);
                return;
            }

            for (lua_pushnil(l_); lua_next(l_, -2);) {
                if (lua_isstring(l_, -2) && lua_isnumber(l_, -1)) {
                    lua_pushvalue(l_, -2);
                    lua_rawset(l_, -4);
                } else {
                    lua_pop(l_, 1);
                }
            }

            lua_settop(l_, x);
        }

        auto quest_manager::initialize_lua() -> bool {
            l_ = lua_open();

            luaopen_base(l_);
            luaopen_table(l_);
            luaopen_string(l_);
            luaopen_math(l_);
            luaopen_io(l_);
            luaopen_debug(l_);

            register_affect_function_table();
            register_building_function_table();
            register_dungeon_function_table();
            register_game_function_table();
            register_guild_function_table();
            register_horse_function_table();
            register_pet_function_table();
            register_item_function_table();
            register_marriage_function_table();
            register_npc_function_table();
            register_party_function_table();
            register_pc_function_table();
            register_quest_function_table();
            register_target_function_table();
            register_arena_function_table();
            register_forked_function_table();
            register_monarch_function_table();
            register_ox_event_function_table();
            register_mgmt_function_table();
            register_battle_arena_function_table();
            register_dance_event_function_table();
            register_dragon_lair_function_table();
            register_speed_server_function_table();
            register_dragon_soul_function_table();

            {
                luaL_reg member_functions[] =
                    {
                        {"chat",        member_chat       },
                        {"set_ready",   member_set_ready  },
                        {"clear_ready", member_clear_ready},
                        {nullptr,       nullptr           }
                };

                add_lua_function_table("member", member_functions);
            }

            {
                luaL_reg highscore_functions[] =
                    {
                        {"register", highscore_register},
                        {"show",     highscore_show    },
                        {nullptr,    nullptr           }
                };

                add_lua_function_table("highscore", highscore_functions);
            }

            {
                luaL_reg mob_functions[] =
                    {
                        {"spawn",       mob_spawn      },
                        {"spawn_group", mob_spawn_group},
                        {nullptr,       nullptr        }
                };

                add_lua_function_table("mob", mob_functions);
            }

            register_global_function_table(l_);

            {
                const std::string settingsFileName = std::format("{}/settings.lua", locale_service_get_base_path());

                int settingsLoadingResult = lua_dofile(l_, settingsFileName.c_str());
                metin2::log::debug("Quest: loaded {} (result {:d})", settingsFileName, settingsLoadingResult);
                if (settingsLoadingResult != 0) {
                    quest::error("Cannot load {}", settingsFileName);
                    return false;
                }
            }

            {
                const std::string questlibFileName = std::format("{}/questlib.lua", locale_service_get_quest_path());

                int questlibLoadingResult = lua_dofile(l_, questlibFileName.c_str());
                metin2::log::debug("Quest: loaded {} (result {:d})", questlibFileName, questlibLoadingResult);
                if (questlibLoadingResult != 0) {
                    quest::error("Cannot load {}", questlibFileName);
                    return false;
                }
            }

            const std::string translateFileName = std::format("{}/translate.lua", locale_service_get_base_path());

            int translateLoadingResult = lua_dofile(l_, translateFileName.c_str());
            metin2::log::debug("Quest: loaded {} (result {:d})", translateFileName, translateLoadingResult);
            if (translateLoadingResult != 0) {
                quest::error("Cannot load {}", translateFileName);
                return false;
            }

            {
                const std::string questLocaleFileName = std::format("{}/locale.lua", quest_dir);

                int questLocaleLoadingResult = lua_dofile(l_, questLocaleFileName.c_str());
                metin2::log::debug("Quest: loaded {} (result {:d})", questLocaleFileName, questLocaleLoadingResult);
                if (questLocaleLoadingResult != 0) {
                    quest::error("Cannot load {}", questLocaleFileName);
                    return false;
                }
            }

            for (auto it = quest_object_dir.begin(); it != quest_object_dir.end(); ++it) {
                const std::string &stQuestObjectDir = *it;
                const std::string state_dir = std::format("{}/state/", stQuestObjectDir);
                DIR *pdir = opendir(state_dir.c_str());
                int iQuestIdx = 0;

                if (pdir) {
                    dirent *pde;

                    while ((pde = readdir(pdir))) {
                        if (pde->d_name[0] == '.')
                            continue;

                        register_quest(pde->d_name, ++iQuestIdx);
                        int ret = lua_dofile(l_, (stQuestObjectDir + "/state/" + pde->d_name).c_str());
                        metin2::log::debug("Quest: loaded {} (result {:d})", (stQuestObjectDir + "/state/" + pde->d_name), ret);

                        build_state_index_to_name(pde->d_name);
                    }

                    closedir(pdir);
                }
            }

            lua_setgcthreshold(l_, 0);

            lua_newtable(l_);
            lua_setglobal(l_, "__codecache");
            return true;
        }

        void quest_manager::goto_select_state(quest_state &qs) {
            lua_checkstack(qs.co, 1);

            int n = luaL_getn(qs.co, -1);
            qs.args = n;

            std::ostringstream os;
            os << "[QUESTION ";

            for (int i = 1; i <= n; i++) {
                lua_rawgeti(qs.co, -1, i);
                if (lua_isstring(qs.co, -1)) {
                    if (i != 1)
                        os << "|";
                    os << i << ";" << lua_tostring(qs.co, -1);
                } else {
                    quest::error("Select option has wrong type {}", lua_typename(qs.co, -1));
                }
                lua_pop(qs.co, 1);
            }
            os << "]";

            add_script(os.str());
            qs.suspend_state = SUSPEND_STATE_SELECT;
            if (metin2::config::test_server())
                metin2::log::debug("Quest: select script {}", script_);
            send_script();
        }

        EVENTINFO(confirm_timeout_event_info) {
            uint32_t wait_pid;
            uint32_t reply_pid;

            confirm_timeout_event_info()
                : wait_pid(0), reply_pid(0) {
            }
        };

        EVENTFUNC(confirm_timeout_event) {
            confirm_timeout_event_info *info = dynamic_cast<confirm_timeout_event_info *>(event->info);

            if (info == nullptr) {
                quest::error("Confirm timeout event has no info");
                return 0;
            }

            character *chWait = character_manager::instance().find_by_pid(info->wait_pid);
            character *chReply = nullptr;

            if (chReply) {
            }

            if (chWait) {
                quest_manager::instance().confirm(info->wait_pid, CONFIRM_TIMEOUT);
            }

            return 0;
        }

        void quest_manager::goto_confirm_state(quest_state &qs) {
            qs.suspend_state = SUSPEND_STATE_CONFIRM;
            uint32_t dwVID = (uint32_t)lua_tonumber(qs.co, -3);
            const char *szMsg = lua_tostring(qs.co, -2);
            int iTimeout = (int)lua_tonumber(qs.co, -1);

            metin2::log::debug("Quest: confirm request to vid {:d}, message '{}', timeout {:d}", dwVID, szMsg, iTimeout);

            character *ch = character_manager::instance().find(dwVID);
            if (ch && ch->is_pc()) {
                ch->confirm_with_msg(szMsg, iTimeout, current_character_ptr()->player_id());
            }

            current_pc()->set_confirm_wait((ch && ch->is_pc()) ? ch->player_id() : 0);
            std::ostringstream os;
            os << "[CONFIRM_WAIT timeout;" << iTimeout << "]";
            add_script(os.str());
            send_script();

            confirm_timeout_event_info *info = alloc_event_info<confirm_timeout_event_info>();

            info->wait_pid = current_character_ptr()->player_id();
            info->reply_pid = (ch && ch->is_pc()) ? ch->player_id() : 0;

            event_create(confirm_timeout_event, info, passes_per_sec(iTimeout));
        }

        void quest_manager::goto_select_item_state(quest_state &qs) {
            qs.suspend_state = SUSPEND_STATE_SELECT_ITEM;
            add_script("[SELECT_ITEM]");
            send_script();
        }

        void quest_manager::goto_input_state(quest_state &qs) {
            qs.suspend_state = SUSPEND_STATE_INPUT;
            add_script("[INPUT]");
            send_script();
        }

        void quest_manager::goto_pause_state(quest_state &qs) {
            qs.suspend_state = SUSPEND_STATE_PAUSE;
            add_script("[NEXT]");
            send_script();
        }

        void quest_manager::goto_end_state(quest_state &qs) {
            add_script("[DONE]");
            send_script();
        }

        auto quest_manager::open_state(const std::string &quest_name, int state_index) -> quest_state {
            quest_state qs;
            qs.args = 0;
            qs.st = state_index;
            qs.co = lua_newthread(l_);
            qs.ico = lua_ref(l_, 1);
            return qs;
        }

        auto quest_manager::run_state(quest_state &qs) -> bool {
            clear_error();

            current_running_state_ = &qs;
            int ret = lua_resume(qs.co, qs.args);

            if (ret == 0) {
                if (lua_gettop(qs.co) == 0) {
                    goto_end_state(qs);
                    return false;
                }

                if (!strcmp(lua_tostring(qs.co, 1), "select")) {
                    goto_select_state(qs);
                    return true;
                }

                if (!strcmp(lua_tostring(qs.co, 1), "wait")) {
                    goto_pause_state(qs);
                    return true;
                }

                if (!strcmp(lua_tostring(qs.co, 1), "input")) {
                    goto_input_state(qs);
                    return true;
                }

                if (!strcmp(lua_tostring(qs.co, 1), "confirm")) {
                    goto_confirm_state(qs);
                    return true;
                }

                if (!strcmp(lua_tostring(qs.co, 1), "select_item")) {
                    goto_select_item_state(qs);
                    return true;
                }
            } else {
                quest::error("Lua error: {}", lua_tostring(qs.co, 1));
            }

            write_running_state_to_syserr();
            set_error();

            goto_end_state(qs);
            return false;
        }

        void quest_manager::close_state(quest_state &qs) {
            if (qs.co) {
                lua_unref(l_, qs.ico);
                qs.co = nullptr;
            }
        }
    }
}
