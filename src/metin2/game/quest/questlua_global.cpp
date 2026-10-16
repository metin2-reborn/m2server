#include <metin2/config/config.h>
#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/empire/priv_manager.h>
#include <metin2/game/empire/start_position.h>
#include <metin2/game/events/fishing.h>
#include <metin2/game/events/mining.h>
#include <metin2/game/events/ox_event.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/items/over9refine.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/quest.h>
#include <metin2/game/quest/questevent.h>
#include <metin2/game/quest/questlua.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/quest/questpc.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/world/regen.h>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/log_types.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <ostream>
#include <sstream>
#include <string>

namespace metin2::game {

    namespace quest {
        auto get_locale(lua_State *lua) -> int {
            lua_pushstring(lua, db_charset.c_str());
            return 1;
        }

        auto random_number(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2))
                lua_pushnumber(lua, 0);
            else
                lua_pushnumber(lua, number((int)lua_tonumber(lua, 1), (int)lua_tonumber(lua, 2)));
            return 1;
        }

        auto time_to_str(lua_State *lua) -> int {
            auto cur_time = (time_t)lua_tonumber(lua, -1);
            lua_pushstring(lua, asctime(gmtime(&cur_time)));
            return 1;
        }

        auto say(lua_State *lua) -> int {
            std::ostringstream s;
            combine_lua_string(lua, s);
            get_quest_manager().add_script(s.str() + "[ENTER]");
            return 0;
        }

        auto chat(lua_State *lua) -> int {
            std::ostringstream s;
            combine_lua_string(lua, s);

            get_quest_manager().current_character_ptr()->chat_packet(CHAT_TYPE_TALKING, "%s", s.str().c_str());
            return 0;
        }

        auto cmdchat(lua_State *lua) -> int {
            std::ostringstream s;
            combine_lua_string(lua, s);
            get_quest_manager().current_character_ptr()->chat_packet(CHAT_TYPE_COMMAND, "%s", s.str().c_str());
            return 0;
        }

        auto syschat(lua_State *lua) -> int {
            std::ostringstream s;
            combine_lua_string(lua, s);
            get_quest_manager().current_character_ptr()->chat_packet(CHAT_TYPE_INFO, "%s", s.str().c_str());
            return 0;
        }

        auto notice(lua_State *lua) -> int {
            std::ostringstream s;
            combine_lua_string(lua, s);
            get_quest_manager().current_character_ptr()->chat_packet(CHAT_TYPE_NOTICE, "%s", s.str().c_str());
            return 0;
        }

        auto left_image(lua_State *lua) -> int {
            if (lua_isstring(lua, -1)) {
                std::string s = lua_tostring(lua, -1);
                get_quest_manager().add_script("[LEFTIMAGE src;" + s + "]");
            }
            return 0;
        }

        auto top_image(lua_State *lua) -> int {
            if (lua_isstring(lua, -1)) {
                std::string s = lua_tostring(lua, -1);
                get_quest_manager().add_script("[TOPIMAGE src;" + s + "]");
            }
            return 0;
        }

        auto set_skin(lua_State *lua) -> int {
            if (lua_isnumber(lua, -1)) {
                get_quest_manager().set_skin_style((int)rint(lua_tonumber(lua, -1)));
            } else {
                quest::error("Invalid skin index");
            }

            return 0;
        }

        auto set_server_timer(lua_State *lua) -> int {
            int n = lua_gettop(lua);
            if ((n != 2 || !lua_isnumber(lua, 2) || !lua_isstring(lua, 1)) &&
                (n != 3 || !lua_isstring(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3))) {
                quest::error("Invalid server timer arguments");
                return 0;
            }

            const char *name = lua_tostring(lua, 1);
            double t = lua_tonumber(lua, 2);
            uint32_t arg = 0;

            quest_manager &q = get_quest_manager();

            if (lua_isnumber(lua, 3))
                arg = (uint32_t)lua_tonumber(lua, 3);

            int timernpc = q.load_timer_script(name);

            event_ptr event = quest_create_server_timer_event(name, t, timernpc, false, arg);
            q.add_server_timer(name, arg, event);
            return 0;
        }

        auto set_server_loop_timer(lua_State *lua) -> int {
            int n = lua_gettop(lua);
            if ((n != 2 || !lua_isnumber(lua, 2) || !lua_isstring(lua, 1)) &&
                (n != 3 || !lua_isstring(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3))) {
                quest::error("Invalid server timer arguments");
                return 0;
            }
            const char *name = lua_tostring(lua, 1);
            double t = lua_tonumber(lua, 2);
            uint32_t arg = 0;
            quest_manager &q = get_quest_manager();

            if (lua_isnumber(lua, 3))
                arg = (uint32_t)lua_tonumber(lua, 3);

            int timernpc = q.load_timer_script(name);

            event_ptr event = quest_create_server_timer_event(name, t, timernpc, true, arg);
            q.add_server_timer(name, arg, event);
            return 0;
        }

        auto clear_server_timer(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();
            const char *name = lua_tostring(lua, 1);
            auto arg = (uint32_t)lua_tonumber(lua, 2);
            q.clear_server_timer(name, arg);
            return 0;
        }

        auto set_named_loop_timer(lua_State *lua) -> int {
            int n = lua_gettop(lua);

            if (n != 2 || !lua_isnumber(lua, -1) || !lua_isstring(lua, -2))
                quest::error("Invalid timer arguments");
            else {
                const char *name = lua_tostring(lua, -2);
                double t = lua_tonumber(lua, -1);

                quest_manager &q = get_quest_manager();
                int timernpc = q.load_timer_script(name);
                q.current_pc()->add_timer(name, quest_create_timer_event(name, q.current_character_ptr()->player_id(), t, timernpc, true));
            }

            return 0;
        }

        auto get_server_timer_arg(lua_State *lua) -> int {
            lua_pushnumber(lua, get_quest_manager().server_timer_arg());
            return 1;
        }

        auto set_timer(lua_State *lua) -> int {
            if (lua_gettop(lua) != 1 || !lua_isnumber(lua, -1))
                quest::error("Invalid arguments");
            else {
                double t = lua_tonumber(lua, -1);

                quest_manager &q = get_quest_manager();
                quest_create_timer_event("", q.current_character_ptr()->player_id(), t);
            }

            return 0;
        }

        auto set_named_timer(lua_State *lua) -> int {
            int n = lua_gettop(lua);

            if (n != 2 || !lua_isnumber(lua, -1) || !lua_isstring(lua, -2)) {
                quest::error("Invalid timer arguments");
            } else {
                const char *name = lua_tostring(lua, -2);
                double t = lua_tonumber(lua, -1);

                quest_manager &q = get_quest_manager();
                int timernpc = q.load_timer_script(name);
                q.current_pc()->add_timer(name, quest_create_timer_event(name, q.current_character_ptr()->player_id(), t, timernpc));
            }

            return 0;
        }

        auto timer(lua_State *lua) -> int {
            if (lua_gettop(lua) == 1)
                return set_timer(lua);
            else
                return set_named_timer(lua);
        }

        auto clear_named_timer(lua_State *lua) -> int {
            int n = lua_gettop(lua);

            if (n != 1 || !lua_isstring(lua, -1))
                quest::error("Invalid timer arguments");
            else {
                quest_manager &q = get_quest_manager();
                q.current_pc()->remove_timer(lua_tostring(lua, -1));
            }

            return 0;
        }

        auto getnpcid(lua_State *lua) -> int {
            const char *name = lua_tostring(lua, -1);
            quest_manager &q = get_quest_manager();
            lua_pushnumber(lua, q.find_npcid_by_name(name));
            return 1;
        }

        auto is_test_server(lua_State *lua) -> int {
            lua_pushboolean(lua, metin2::config::test_server());
            return 1;
        }

        auto is_speed_server(lua_State *lua) -> int {
            lua_pushboolean(lua, metin2::config::game().speed_server);
            return 1;
        }

        auto raw_script(lua_State *lua) -> int {
            if (metin2::config::test_server())
                metin2::log::debug("Quest: raw script {}", lua_tostring(lua, -1));
            if (lua_isstring(lua, -1))
                get_quest_manager().add_script(lua_tostring(lua, -1));
            else
                quest::error("Invalid raw script argument in quest {}", get_quest_manager().current_quest_name().c_str());

            return 0;
        }

        auto char_log(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();
            character *ch = q.current_character_ptr();

            uint32_t what = 0;
            const char *how = "";
            const char *hint = "";

            if (lua_isnumber(lua, 1)) what = (uint32_t)lua_tonumber(lua, 1);
            if (lua_isstring(lua, 2)) how = lua_tostring(lua, 2);
            if (lua_tostring(lua, 3)) hint = lua_tostring(lua, 3);

            get_log_manager().char_log(ch, what, how, hint);
            return 0;
        }

        auto item_log(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();
            character *ch = q.current_character_ptr();

            uint32_t item_id = 0;
            const char *how = "";
            const char *hint = "";

            if (lua_isnumber(lua, 1)) item_id = (uint32_t)lua_tonumber(lua, 1);
            if (lua_isstring(lua, 2)) how = lua_tostring(lua, 2);
            if (lua_tostring(lua, 3)) hint = lua_tostring(lua, 3);

            item *item = get_item_manager().find(item_id);

            if (item)
                get_log_manager().item_log(ch, item, how, hint);

            return 0;
        }

        auto syslog(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isstring(lua, 2))
                return 0;

            if (lua_tonumber(lua, 1) >= 1) {
                if (!metin2::config::test_server())
                    return 0;
            }

            pc *pc = get_quest_manager().current_pc();

            if (!pc)
                return 0;

            character *ch = get_quest_manager().current_character_ptr();

            if (!ch)
                return 0;

            metin2::log::info("Quest {}: player {}: {}", pc->current_quest_name(), ch->name(), lua_tostring(lua, 2));

            if (true == metin2::config::test_server()) {
                ch->chat_packet(CHAT_TYPE_INFO, "QUEST_SYSLOG %s", lua_tostring(lua, 2));
            }

            return 0;
        }

        auto syserr(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1))
                return 0;

            pc *pc = get_quest_manager().current_pc();

            if (!pc)
                return 0;

            character *ch = get_quest_manager().current_character_ptr();

            if (!ch)
                return 0;

            quest::error("Quest {}: player {}: {}", pc->current_quest_name().c_str(), ch->name(), lua_tostring(lua, 1));
            ch->chat_packet(CHAT_TYPE_INFO, "QUEST_SYSERR %s", lua_tostring(lua, 1));
            return 0;
        }

        auto set_bgm_volume_enable(lua_State *lua) -> int {
            character_set_bgm_volume_enable();

            return 0;
        }

        auto add_bgm_info(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isstring(lua, 2))
                return 0;

            int map_index = (int)lua_tonumber(lua, 1);

            const char *bgm_name = lua_tostring(lua, 2);
            if (!bgm_name)
                return 0;

            float bgm_vol = lua_isnumber(lua, 3) ? lua_tonumber(lua, 3) : (1.0f / 5.0f) * 0.1f;

            character_add_bgm_info(map_index, bgm_name, bgm_vol);

            return 0;
        }

        auto add_goto_info(lua_State *lua) -> int {
            const char *name = lua_tostring(lua, 1);

            int empire = (int)lua_tonumber(lua, 2);
            int map_index = (int)lua_tonumber(lua, 3);
            int x = (int)lua_tonumber(lua, 4);
            int y = (int)lua_tonumber(lua, 5);

            if (!name)
                return 0;

            character_add_goto_info(name, empire, map_index, x, y);
            return 0;
        }

        auto refine_pick(lua_State *lua) -> int {
            auto cell = (uint8_t)lua_tonumber(lua, -1);

            quest_manager &q = get_quest_manager();

            character *ch = q.current_character_ptr();

            item *item = ch->inventory_item(cell);

            int ret = mining::real_refine_pick(ch, item);
            lua_pushnumber(lua, ret);
            return 1;
        }

        auto fish_real_refine_rod(lua_State *lua) -> int {
            auto cell = (uint8_t)lua_tonumber(lua, -1);

            quest_manager &q = get_quest_manager();

            character *ch = q.current_character_ptr();

            item *item = ch->inventory_item(cell);

            int ret = fishing::real_refine_rod(ch, item);
            lua_pushnumber(lua, ret);
            return 1;
        }

        auto give_char_privilege(lua_State *lua) -> int {
            int pid = get_quest_manager().current_character_ptr()->player_id();
            int type = (int)lua_tonumber(lua, 1);
            int value = (int)lua_tonumber(lua, 2);

            if (MAX_PRIV_NUM <= type) {
                quest::error("Invalid privilege type {:d}", type);
                return 0;
            }

            get_priv_manager().request_give_character_priv(pid, type, value);

            return 0;
        }

        auto give_empire_privilege(lua_State *lua) -> int {
            int empire = (int)lua_tonumber(lua, 1);
            int type = (int)lua_tonumber(lua, 2);
            int value = (int)lua_tonumber(lua, 3);
            int time = (int)lua_tonumber(lua, 4);
            character *ch = get_quest_manager().current_character_ptr();

            if (MAX_PRIV_NUM <= type) {
                quest::error("Invalid privilege type {:d}", type);
                return 0;
            }

            if (ch)
                metin2::log::info("Priv: quest grants empire {:d} privilege type {:d} value {:d} for {:d}s, player {}",
                    empire, type, value, time, ch->name());
            else
                metin2::log::info("Priv: quest grants empire {:d} privilege type {:d} value {:d} for {:d}s",
                    empire, type, value, time);

            get_priv_manager().request_give_empire_priv(empire, type, value, time);
            return 0;
        }

        auto give_guild_privilege(lua_State *lua) -> int {
            int guild_id = (int)lua_tonumber(lua, 1);
            int type = (int)lua_tonumber(lua, 2);
            int value = (int)lua_tonumber(lua, 3);
            int time = (int)lua_tonumber(lua, 4);

            if (MAX_PRIV_NUM <= type) {
                quest::error("Invalid privilege type {:d}", type);
                return 0;
            }

            metin2::log::info("Priv: quest grants guild {:d} privilege type {:d} value {:d} for {:d}s",
                guild_id, type, value, time);

            get_priv_manager().request_give_guild_priv(guild_id, type, value, time);

            return 0;
        }

        auto get_empire_privilege_string(lua_State *lua) -> int {
            int empire = (int)lua_tonumber(lua, 1);
            std::ostringstream os;
            bool found = false;

            for (int type = PRIV_NONE + 1; type < MAX_PRIV_NUM; ++type) {
                priv_manager::priv_empire_data *priv_empire_data = get_priv_manager().priv_by_empire_ex(empire, type);

                if (priv_empire_data && priv_empire_data->value) {
                    if (found)
                        os << ", ";

                    os << lc_text(priv_names[type]) << " : " << priv_empire_data->value << "%" << " (" << ((priv_empire_data->end_time_sec - get_global_time()) / 3600.0f) << " hours)" << std::endl;
                    found = true;
                }
            }

            if (!found)
                os << "None!" << std::endl;

            lua_pushstring(lua, os.str().c_str());
            return 1;
        }

        auto get_empire_privilege(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2)) {
                lua_pushnumber(lua, 0);
                return 1;
            }
            int empire = (int)lua_tonumber(lua, 1);
            int type = (int)lua_tonumber(lua, 2);
            int value = get_priv_manager().priv_by_empire(empire, type);
            lua_pushnumber(lua, value);
            return 1;
        }

        auto get_guild_privilege_string(lua_State *lua) -> int {
            int guild = (int)lua_tonumber(lua, 1);
            std::ostringstream os;
            bool found = false;

            for (int type = PRIV_NONE + 1; type < MAX_PRIV_NUM; ++type) {
                const priv_manager::priv_guild_data *priv_guild_data = get_priv_manager().priv_by_guild_ex(guild, type);

                if (priv_guild_data && priv_guild_data->value) {
                    if (found)
                        os << ", ";

                    os << lc_text(priv_names[type]) << " : " << priv_guild_data->value << "%"
                       << " (" << ((priv_guild_data->end_time_sec - get_global_time()) / 3600.0f) << " hours)" << std::endl;
                    found = true;
                }
            }

            if (!found)
                os << "None!" << std::endl;

            lua_pushstring(lua, os.str().c_str());
            return 1;
        }

        auto get_guildid_byname(lua_State *lua) -> int {
            if (!require_string(lua, 1, "Invalid guild name argument")) {
                lua_pushnumber(lua, 0);
                return 1;
            }

            const char *guild_name = lua_tostring(lua, 1);
            guild *find_guild = get_guild_manager().find_guild_by_name(guild_name);
            if (find_guild)
                lua_pushnumber(lua, find_guild->id());
            else
                lua_pushnumber(lua, 0);

            return 1;
        }

        auto get_guild_privilege(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2)) {
                lua_pushnumber(lua, 0);
                return 1;
            }
            int guild = (int)lua_tonumber(lua, 1);
            int type = (int)lua_tonumber(lua, 2);
            int value = get_priv_manager().priv_by_guild(guild, type);
            lua_pushnumber(lua, value);
            return 1;
        }

        auto item_name(lua_State *lua) -> int {
            if (lua_isnumber(lua, 1)) {
                auto vnum = (uint32_t)lua_tonumber(lua, 1);
                item_table *table = get_item_manager().table(vnum);
                if (table)
                    lua_pushstring(lua, table->locale_name);
                else
                    lua_pushstring(lua, "");
            } else
                lua_pushstring(lua, "");
            return 1;
        }

        auto mob_name(lua_State *lua) -> int {
            if (lua_isnumber(lua, 1)) {
                auto vnum = (uint32_t)lua_tonumber(lua, 1);
                const mob *mob = get_mob_manager().get(vnum);

                if (mob)
                    lua_pushstring(lua, mob->table.locale_name);
                else
                    lua_pushstring(lua, "");
            } else
                lua_pushstring(lua, "");

            return 1;
        }

        auto mob_vnum(lua_State *lua) -> int {
            if (lua_isstring(lua, 1)) {
                const char *str = lua_tostring(lua, 1);
                const mob *mob = get_mob_manager().get(str, false);
                if (mob)
                    lua_pushnumber(lua, mob->table.vnum);
                else
                    lua_pushnumber(lua, 0);
            } else
                lua_pushnumber(lua, 0);

            return 1;
        }

        auto global_time(lua_State *lua) -> int {
            lua_pushnumber(lua, get_global_time());
            return 1;
        }

        auto get_channel_id(lua_State *lua) -> int {
            lua_pushnumber(lua, metin2::config::server().channel);

            return 1;
        }

        auto do_command(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1))
                return 0;

            const char *str = lua_tostring(lua, 1);
            size_t len = strlen(str);

            character *ch = get_quest_manager().current_character_ptr();
            metin2::game::interpret_command(ch, str, len);
            return 0;
        }

        auto find_pc(lua_State *lua) -> int {
            if (!require_string(lua, 1)) {
                lua_pushnumber(lua, 0);
                return 1;
            }

            const char *name = lua_tostring(lua, 1);
            character *tch = get_character_manager().find_pc(name);
            lua_pushnumber(lua, tch ? tch->get_vid() : 0);
            return 1;
        }

        auto find_pc_cond(lua_State *lua) -> int {
            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3)) {
                quest::error("Invalid arguments");
                lua_pushnumber(lua, 0);
                return 1;
            }

            int min_lev = (int)lua_tonumber(lua, 1);
            int max_lev = (int)lua_tonumber(lua, 2);
            auto job_flag = (unsigned int)lua_tonumber(lua, 3);

            character *ch = get_quest_manager().current_character_ptr();
            character *tch;

            if (metin2::config::test_server()) {
                metin2::log::debug("Quest: find player on map {:d}, job flags {:d}, level {:d}-{:d}",
                    ch->map_index(),
                    job_flag,
                    min_lev, max_lev);
            }

            tch = get_character_manager().find_specify_pc(job_flag,
                ch->map_index(),
                ch,
                min_lev,
                max_lev);

            lua_pushnumber(lua, tch ? tch->get_vid() : 0);
            return 1;
        }

        auto find_npc_by_vnum(lua_State *lua) -> int {
            if (!require_number(lua, 1)) {
                lua_pushnumber(lua, 0);
                return 1;
            }

            auto race = (uint32_t)lua_tonumber(lua, 1);

            character_vector_interactor i;

            if (get_character_manager().characters_by_race_num(race, i)) {
                auto it = i.begin();

                while (it != i.end()) {
                    character *tch = *(it++);

                    if (tch->map_index() == get_quest_manager().current_character_ptr()->map_index()) {
                        lua_pushnumber(lua, tch->get_vid());
                        return 1;
                    }
                }
            }

            lua_pushnumber(lua, 0);
            return 1;
        }

        auto set_quest_state(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1) || !lua_isstring(lua, 2))
                return 0;

            quest_manager &q = get_quest_manager();
            quest_state *pqs = q.current_state();
            pc *pc = q.current_pc();
            if (lua != pqs->co) {
                luaL_error(lua, "running thread != current thread???");
                metin2::log::error("Running Lua thread is not the current quest thread");
                return -1;
            }
            if (pc) {
                const std::string quest_name(lua_tostring(lua, 1));
                const std::string state_name(lua_tostring(lua, 2));
                if (metin2::config::test_server())
                    metin2::log::debug("Quest: {} set state {}", quest_name, state_name);
                if (pc->current_quest_name() == quest_name) {
                    pqs->st = q.quest_state_index(pc->current_quest_name(), lua_tostring(lua, -1));
                    pc->set_current_quest_state_name(lua_tostring(lua, -1));
                } else {
                    pc->set_quest_state(quest_name, state_name);
                }
            }
            return 0;
        }

        auto get_quest_state(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1))
                return 0;

            quest_manager &q = get_quest_manager();
            pc *pc = q.current_pc();

            if (pc) {
                std::string quest_name = lua_tostring(lua, 1);
                quest_name += ".__status";

                int ret = pc->flag(quest_name.c_str());

                lua_pushnumber(lua, ret);

                if (metin2::config::test_server())
                    metin2::log::debug("Quest: {} state is {:d}", quest_name, ret);
            } else {
                if (metin2::config::test_server())
                    metin2::log::debug("Quest: no current player for state lookup");

                lua_pushnumber(lua, 0);
            }
            return 1;
        }

        auto notice_all(lua_State *lua) -> int {
            std::ostringstream s;
            combine_lua_string(lua, s);

            packet_gg_notice p;
            p.header = HEADER_GG_NOTICE;
            p.size = strlen(s.str().c_str()) + 1;

            temp_buffer buf;
            buf.write(&p, sizeof(p));
            buf.write(s.str().c_str(), p.size);

            get_p2p_manager().send(buf.read_peek(), buf.size());

            send_notice(s.str().c_str());
            return 1;
        }

        EVENTINFO(warp_all_to_village_event_info) {
            uint32_t warp_map_index;

            warp_all_to_village_event_info()
                : warp_map_index(0) {
            }
        };

        EVENTFUNC(warp_all_to_village_event) {
            auto *info = dynamic_cast<warp_all_to_village_event_info *>(event->info.get());

            if (info == nullptr) {
                quest::error("Warp-all-to-village event has no info");
                return 0;
            }

            sectree_map *sec_map = get_sectree_manager().map(info->warp_map_index);

            if (nullptr != sec_map) {
                sec_map->for_each([](entity *ent) {
                    if (ent->is_type(ENTITY_CHARACTER)) {
                        auto *ch = (character *)ent;
                        if (ch->is_pc()) {
                            uint8_t empire_value = ch->empire();
                            if (empire_value == 0) {
                                quest::error("Player {} (pid {:d}) has no empire", ch->name(), ch->player_id());
                                return;
                            }

                            ch->warp_set(start_position[empire_value][0], start_position[empire_value][1]);
                        }
                    }
                });
            }

            return 0;
        }

        auto warp_all_to_village(lua_State *lua) -> int {
            int map_index = static_cast<int>(lua_tonumber(lua, 1));
            int sec = static_cast<int>(lua_tonumber(lua, 2));

            auto *info = alloc_event_info<warp_all_to_village_event_info>();

            info->warp_map_index = map_index;

            event_create(warp_all_to_village_event, info, passes_per_sec(sec));

            send_notice_map(lc_text("잠시후 모두 마을로 이동됩니다."), map_index, false);

            return 0;
        }

        auto warp_to_village(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (nullptr != ch) {
                uint8_t empire = ch->empire();
                ch->warp_set(start_position[empire][0], start_position[empire][1]);
            }

            return 0;
        }

        auto say_in_map(lua_State *lua) -> int {
            int map_index = static_cast<int>(lua_tonumber(lua, 1));
            std::string script(lua_tostring(lua, 2));

            script += "[ENTER]";
            script += "[DONE]";

            struct metin2::game::packet_script packet_script;

            packet_script.header = HEADER_GC_SCRIPT;
            packet_script.skin = quest_manager::QUEST_SKIN_NORMAL;
            packet_script.src_size = script.size();
            packet_script.size = packet_script.src_size + sizeof(struct packet_script);

            temp_buffer buf;
            buf.write(&packet_script, sizeof(struct packet_script));
            buf.write(&script[0], script.size());

            sectree_map *sec_map = get_sectree_manager().map(map_index);

            if (sec_map) {
                send_buffer_to_map(sec_map, buf);
            }

            return 0;
        }

        auto kill_all_in_map(lua_State *lua) -> int {
            sectree_map *sec_map = get_sectree_manager().map(lua_tonumber(lua, 1));

            if (nullptr != sec_map) {
                sec_map->for_each([](entity *ent) {
                    if (ent->is_type(ENTITY_CHARACTER)) {
                        auto *ch = (character *)ent;

                        if (!ch->is_pc() && !ch->is_pet())
                            ch->dead();
                    }
                });
            }

            return 0;
        }

        auto regen_in_map(lua_State *lua) -> int {
            int map_index = static_cast<int>(lua_tonumber(lua, 1));
            std::string filename(lua_tostring(lua, 2));

            sectree_map *map = get_sectree_manager().map(map_index);

            if (map != nullptr) {
                regen_load_in_file(filename.c_str(), map_index, map->setting.base_x, map->setting.base_y);
            }

            return 0;
        }

        auto enable_over9refine(lua_State *lua) -> int {
            if (lua_isnumber(lua, 1) == true && lua_isnumber(lua, 2) == true) {
                auto vnum_from = (uint32_t)lua_tonumber(lua, 1);
                auto vnum_to = (uint32_t)lua_tonumber(lua, 2);

                get_over9_refine_manager().enable_over9_refine(vnum_from, vnum_to);
            }

            return 0;
        }

        auto add_ox_quiz(lua_State *lua) -> int {
            int level = (int)lua_tonumber(lua, 1);
            const char *quiz = lua_tostring(lua, 2);
            bool answer = lua_toboolean(lua, 3);

            if (get_ox_event_manager().add_quiz(level, quiz, answer) == false) {
                metin2::log::error("Cannot add OX quiz (level {:d}, question {}, answer {:d})", level, quiz, answer);
            }

            return 1;
        }

        EVENTFUNC(warp_all_to_map_my_empire_event) {
            auto *info = dynamic_cast<warp_all_to_map_my_empire_event_info *>(event->info.get());

            if (info == nullptr) {
                quest::error("Warp-all-to-map event has no info");
                return 0;
            }

            sectree_map *sec_map = get_sectree_manager().map(info->map_index_from);

            if (sec_map) {
                sec_map->for_each([info](entity *ent) {
                    if (ent->is_type(ENTITY_CHARACTER)) {
                        auto *ch = (character *)ent;

                        if (ch->is_pc() && ch->empire() == info->empire) {
                            ch->warp_set(info->x, info->y, info->map_index_to);
                        }
                    }
                });
            }

            return 0;
        }

        auto block_chat(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (ch != nullptr) {
                if (lua_isstring(lua, 1) != true && lua_isstring(lua, 2) != true) {
                    lua_pushboolean(lua, false);
                    return 1;
                }

                std::string name(lua_tostring(lua, 1));
                std::string time(lua_tostring(lua, 2));

                std::string arg = name + " " + time;

                do_block_chat(ch, const_cast<char *>(arg.c_str()), 0, 0);

                lua_pushboolean(lua, true);
                return 1;
            }

            lua_pushboolean(lua, false);
            return 1;
        }

        auto spawn_mob_at(lua_State *lua) -> int {
            if (false == lua_isnumber(lua, 1) || false == lua_isnumber(lua, 2) || false == lua_isboolean(lua, 3)) {
                lua_pushnumber(lua, 0);
                return 1;
            }

            const auto vnum_value = static_cast<uint32_t>(lua_tonumber(lua, 1));
            const size_t count = std::clamp<int>(static_cast<size_t>(lua_tonumber(lua, 2)), 1, 10);
            const bool is_aggresive = static_cast<bool>(lua_toboolean(lua, 3));
            size_t spawn_count = 0;

            const mob *monster = get_mob_manager().get(vnum_value);

            if (nullptr != monster) {
                character *const ch = get_quest_manager().current_character_ptr();

                for (size_t i = 0; i < count; ++i) {
                    character *const spawn_monster = get_character_manager().spawn_mob_range(vnum_value,
                        ch->map_index(),
                        ch->x() - number(200, 750),
                        ch->y() - number(200, 750),
                        ch->x() + number(200, 750),
                        ch->y() + number(200, 750),
                        true,
                        monster->table.type == CHAR_TYPE_STONE,
                        is_aggresive);

                    if (nullptr != spawn_monster) {
                        ++spawn_count;
                    }
                }

                metin2::log::debug("Quest: spawned {1:d} mobs (vnum {0:d}, aggressive {2})", vnum_value, spawn_count, is_aggresive);
            }

            lua_pushnumber(lua, spawn_count);

            return 1;
        }

        auto notice_in_map(lua_State *lua) -> int {
            character *const ch = get_quest_manager().current_character_ptr();

            if (nullptr != ch) {
                send_notice_map(lua_tostring(lua, 1), ch->map_index(), lua_toboolean(lua, 2));
            }

            return 0;
        }

        auto get_locale_base_path(lua_State *lua) -> int {
            lua_pushstring(lua, locale_service_get_base_path().c_str());

            return 1;
        }

        auto purge_area(lua_State *lua) -> int {
            int x1 = lua_tonumber(lua, 1);
            int y1 = lua_tonumber(lua, 2);
            int x2 = lua_tonumber(lua, 3);
            int y2 = lua_tonumber(lua, 4);

            const int map_index = get_sectree_manager().map_index(x1, y1);

            if (0 == map_index) {
                quest::error("No map at ({:d}, {:d})", x1, y1);
                return 0;
            }

            sectree_map *sectree = get_sectree_manager().map(map_index);

            if (nullptr != sectree) {
                purge_monsters_in_area(sectree, x1, y1, x2, y2, get_quest_manager().current_npc_character_ptr());
            }

            return 0;
        }

        auto warp_all_in_area_to_area(lua_State *lua) -> int {
            int from_x1 = lua_tonumber(lua, 1);
            int from_y1 = lua_tonumber(lua, 2);
            int from_x2 = lua_tonumber(lua, 3);
            int from_y2 = lua_tonumber(lua, 4);

            int to_x1 = lua_tonumber(lua, 5);
            int to_y1 = lua_tonumber(lua, 6);
            int to_x2 = lua_tonumber(lua, 7);
            int to_y2 = lua_tonumber(lua, 8);

            const int map_index = get_sectree_manager().map_index(from_x1, from_y1);

            if (0 == map_index) {
                quest::error("No map at ({:d}, {:d})", from_x1, from_y1);
                lua_pushnumber(lua, 0);
                return 1;
            }

            sectree_map *sectree_value = get_sectree_manager().map(map_index);

            if (nullptr != sectree_value) {
                size_t warp_count = 0;

                sectree_value->for_each([&](entity *ent) {
                    if (true == ent->is_type(ENTITY_CHARACTER)) {
                        auto *ch = static_cast<character *>(ent);

                        if (true == ch->is_pc()) {
                            if (from_x1 <= ch->x() && ch->x() <= from_x2 && from_y1 <= ch->y() && ch->y() <= from_y2) {
                                ++warp_count;

                                ch->warp_set(number(to_x1, to_x2), number(to_y1, to_y2));
                            }
                        }
                    }
                });

                lua_pushnumber(lua, warp_count);
                metin2::log::info("Quest: warped {:d} players between areas", warp_count);
                return 1;
            } else {
                lua_pushnumber(lua, 0);
                quest::error("No sectree for the source area");
                return 1;
            }
        }

        void register_global_function_table(lua_State *lua) {

            luaL_reg global_functions[] =
                {
                    {"sys_err",                  syserr                     },
                    {"sys_log",                  syslog                     },
                    {"char_log",                 char_log                   },
                    {"item_log",                 item_log                   },
                    {"set_state",                quest_setstate             },
                    {"set_skin",                 set_skin                   },
                    {"setskin",                  set_skin                   },
                    {"time_to_str",              time_to_str                },
                    {"say",                      say                        },
                    {"chat",                     chat                       },
                    {"cmdchat",                  cmdchat                    },
                    {"syschat",                  syschat                    },
                    {"get_locale",               get_locale                 },
                    {"setleftimage",             left_image                 },
                    {"settopimage",              top_image                  },
                    {"server_timer",             set_server_timer           },
                    {"clear_server_timer",       clear_server_timer         },
                    {"server_loop_timer",        set_server_loop_timer      },
                    {"get_server_timer_arg",     get_server_timer_arg       },
                    {"timer",                    timer                      },
                    {"loop_timer",               set_named_loop_timer       },
                    {"cleartimer",               clear_named_timer          },
                    {"getnpcid",                 getnpcid                   },
                    {"is_test_server",           is_test_server             },
                    {"is_speed_server",          is_speed_server            },
                    {"raw_script",               raw_script                 },
                    {"number",                   random_number              },

                    {"set_bgm_volume_enable",    set_bgm_volume_enable      },
                    {"add_bgm_info",             add_bgm_info               },

                    {"add_goto_info",            add_goto_info              },

                    {"__refine_pick",            refine_pick                },

                    {"add_ox_quiz",              add_ox_quiz                },
                    {"__fish_real_refine_rod",   fish_real_refine_rod       },
                    {"__give_char_priv",         give_char_privilege        },
                    {"__give_empire_priv",       give_empire_privilege      },
                    {"__give_guild_priv",        give_guild_privilege       },
                    {"__get_empire_priv_string", get_empire_privilege_string},
                    {"__get_empire_priv",        get_empire_privilege       },
                    {"__get_guild_priv_string",  get_guild_privilege_string },
                    {"__get_guildid_byname",     get_guildid_byname         },
                    {"__get_guild_priv",         get_guild_privilege        },
                    {"item_name",                item_name                  },
                    {"mob_name",                 mob_name                   },
                    {"mob_vnum",                 mob_vnum                   },
                    {"get_time",                 global_time                },
                    {"get_global_time",          global_time                },
                    {"get_channel_id",           get_channel_id             },
                    {"command",                  do_command                 },
                    {"find_pc_cond",             find_pc_cond               },
                    {"find_pc_by_name",          find_pc                    },
                    {"find_npc_by_vnum",         find_npc_by_vnum           },
                    {"set_quest_state",          set_quest_state            },
                    {"get_quest_state",          get_quest_state            },
                    {"notice",                   notice                     },
                    {"notice_all",               notice_all                 },
                    {"notice_in_map",            notice_in_map              },
                    {"warp_all_to_village",      warp_all_to_village        },
                    {"warp_to_village",          warp_to_village            },
                    {"say_in_map",               say_in_map                 },
                    {"kill_all_in_map",          kill_all_in_map            },
                    {"regen_in_map",             regen_in_map               },
                    {"enable_over9refine",       enable_over9refine         },
                    {"block_chat",               block_chat                 },
                    {"spawn_mob",                spawn_mob_at               },
                    {"get_locale_base_path",     get_locale_base_path       },
                    {"purge_area",               purge_area                 },
                    {"warp_all_in_area_to_area", warp_all_in_area_to_area   },

                    {nullptr,                    nullptr                    }
            };

            int i = 0;

            while (global_functions[i].name != nullptr) {
                lua_register(lua, global_functions[i].name, global_functions[i].func);
                ++i;
            }
        }
    }
}
