#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questlua.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/quest/questpc.h>

#include <metin2/log/log.h>
#include <metin2/proto/chat_types.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace metin2::game {

    namespace quest {
        auto party_clear_ready(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (ch->get_party()) {
                ch->get_party()->for_each_near_member([](character *member) {
                    member->remove_affect(AFFECT_DUNGEON_READY);
                });
            }
            return 0;
        }

        auto party_get_max_level(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (ch->get_party())
                lua_pushnumber(lua, ch->get_party()->member_max_level());
            else
                lua_pushnumber(lua, 1);

            return 1;
        }

        auto party_run_cinematic(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1))
                return 0;

            metin2::log::debug("Party cinematic: run {}", lua_tostring(lua, 1));
            character *ch = get_quest_manager().current_character_ptr();

            if (ch->get_party()) {
                std::string data = "[RUN_CINEMA value;";
                data += lua_tostring(lua, 1);
                data += "]";

                struct packet_script pack;
                pack.header = HEADER_GC_SCRIPT;
                pack.skin = quest_manager::QUEST_SKIN_CINEMATIC;
                pack.src_size = data.size();
                pack.size = pack.src_size + sizeof(struct packet_script);

                ch->get_party()->update();
                ch->get_party()->for_each_near_member([&](character *member) {
                    metin2::log::debug("Party cinematic: sending to {}", member->name());

                    if (member->get_desc()) {
                        metin2::log::debug("Party cinematic: sent to {}", member->name());
                        member->get_desc()->buffered_packet(&pack, sizeof(struct packet_script));
                        member->get_desc()->packet(data.c_str(), data.size());
                    }
                });
            }

            return 0;
        }

        auto party_show_cinematic(lua_State *lua) -> int {
            if (!lua_isstring(lua, 1))
                return 0;

            metin2::log::debug("Party script: show {}", lua_tostring(lua, 1));
            character *ch = get_quest_manager().current_character_ptr();

            if (ch->get_party()) {
                const char *str = lua_tostring(lua, 1);
                int len = strlen(str);

                struct metin2::game::packet_script packet_script;
                packet_script.header = HEADER_GC_SCRIPT;
                packet_script.skin = quest_manager::QUEST_SKIN_CINEMATIC;
                packet_script.src_size = len;
                packet_script.size = packet_script.src_size + sizeof(struct game::protocol::packet_script);

                ch->get_party()->update();
                ch->get_party()->for_each_near_member([&](character *member) {
                    metin2::log::debug("Party script: sending to {}", member->name());

                    if (member->get_desc()) {
                        metin2::log::debug("Party script: sent to {}", member->name());
                        member->get_desc()->buffered_packet(&packet_script, sizeof(struct game::protocol::packet_script));
                        member->get_desc()->packet(str, len);
                    }
                });
            }
            return 0;
        }

        auto party_get_near_count(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (ch->get_party())
                lua_pushnumber(lua, ch->get_party()->near_member_count());
            else
                lua_pushnumber(lua, 0);

            return 1;
        }

        auto party_syschat(lua_State *lua) -> int {
            party *party = get_quest_manager().current_character_ptr()->get_party();

            if (party) {
                std::ostringstream s;
                combine_lua_string(lua, s);

                party->for_each_online_member([&](character *member) {
                    member->chat_packet(CHAT_TYPE_INFO, "%s", s.str().c_str());
                });
            }

            return 0;
        }

        auto party_is_leader(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (ch->get_party() && ch->get_party()->leader_pid() == ch->player_id())
                lua_pushboolean(lua, 1);
            else
                lua_pushboolean(lua, 0);

            return 1;
        }

        auto party_is_party(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            lua_pushboolean(lua, ch->get_party() ? 1 : 0);
            return 1;
        }

        auto party_get_leader_pid(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();
            if (ch->get_party()) {
                lua_pushnumber(lua, ch->get_party()->leader_pid());
            } else {
                lua_pushnumber(lua, -1);
            }
            return 1;
        }

        auto party_chat(lua_State *lua) -> int {
            party *party = get_quest_manager().current_character_ptr()->get_party();

            if (party) {
                std::ostringstream s;
                combine_lua_string(lua, s);

                party->for_each_online_member([&](character *member) {
                    member->chat_packet(CHAT_TYPE_TALKING, "%s", s.str().c_str());
                });
            }

            return 0;
        }

        auto party_is_map_member_flag_lt(lua_State *lua) -> int {

            if (!lua_isstring(lua, 1) || !lua_isnumber(lua, 2)) {
                lua_pushnumber(lua, 0);
                return 1;
            }

            quest_manager &q = get_quest_manager();
            party *party = q.current_character_ptr()->get_party();
            character *ch = q.current_character_ptr();
            pc *pc_value = q.current_pc();

            const char *sz = lua_tostring(lua, 1);

            if (party) {
                std::string flagname = pc_value->current_quest_name() + "." + sz;
                int value = (int)rint(lua_tonumber(lua, 2));

                bool return_bool = party->for_each_on_map_member_bool([&](character *member) -> bool {
                    if (!member->is_pc())
                        return false;

                    pc *member_pc = get_quest_manager().pc_force(member->player_id());
                    return member_pc && value > member_pc->flag(flagname);
                },
                    ch->map_index());
                lua_pushboolean(lua, return_bool);
            }

            return 1;
        }

        auto party_set_flag(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (ch->get_party() && lua_isstring(lua, 1) && lua_isnumber(lua, 2))
                ch->get_party()->set_flag(lua_tostring(lua, 1), (int)lua_tonumber(lua, 2));

            return 0;
        }

        auto party_get_flag(lua_State *lua) -> int {
            character *ch = get_quest_manager().current_character_ptr();

            if (!ch->get_party() || !lua_isstring(lua, 1))
                lua_pushnumber(lua, 0);
            else
                lua_pushnumber(lua, ch->get_party()->flag(lua_tostring(lua, 1)));

            return 1;
        }

        auto party_set_quest_flag(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();

            std::string flagname = q.current_pc()->current_quest_name() + "." + lua_tostring(lua, 1);
            int value = (int)rint(lua_tonumber(lua, 2));
            auto f = [&](character *member) { set_pc_quest_flag(member, flagname, value); };

            character *ch = q.current_character_ptr();

            if (ch->get_party())
                ch->get_party()->for_each_online_member(f);
            else
                f(ch);

            return 0;
        }

        auto party_is_in_dungeon(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();
            character *ch = q.current_character_ptr();
            party *party = ch->get_party();
            if (party != nullptr) {
                lua_pushboolean(lua, party->get_dungeon() ? true : false);
                return 1;
            }
            lua_pushboolean(lua, false);
            return 1;
        }

        auto party_give_buff(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();
            character *ch = q.current_character_ptr();

            if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3) || !lua_isnumber(lua, 4) ||
                !lua_isnumber(lua, 5) || !lua_isnumber(lua, 6) || !lua_isboolean(lua, 7) || !lua_isboolean(lua, 8)) {
                lua_pushboolean(lua, false);
                return 1;
            }
            uint32_t type = lua_tonumber(lua, 1);
            uint8_t apply_on = lua_tonumber(lua, 2);
            int32_t apply_value = lua_tonumber(lua, 3);
            uint32_t flag = lua_tonumber(lua, 4);
            int32_t duration = lua_tonumber(lua, 5);
            int32_t sp_cost = lua_tonumber(lua, 6);
            bool override = lua_toboolean(lua, 7);
            bool is_cube = lua_toboolean(lua, 8);

            auto f = [&](character *member) {
                member->add_affect(type, apply_on, apply_value, flag, duration, sp_cost, override, is_cube);
            };
            if (ch->get_party())
                ch->get_party()->for_each_on_map_member(f, ch->map_index());
            else
                f(ch);

            lua_pushboolean(lua, true);
            return 1;
        }

        auto party_get_member_pids(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();
            character *ch = q.current_character_ptr();
            party *party = ch->get_party();
            if (nullptr == party) {
                return 0;
            }
            std::vector<uint32_t> pids;
            party->for_each_on_map_member([&](character *member) {
                pids.push_back(member->player_id());
            },
                ch->map_index());

            for (uint32_t pid : pids) {
                lua_pushnumber(lua, pid);
            }
            return pids.size();
        }

        void register_party_function_table() {
            luaL_reg party_functions[] =
                {
                    {"is_leader",             party_is_leader            },
                    {"is_party",              party_is_party             },
                    {"get_leader_pid",        party_get_leader_pid       },
                    {"setf",                  party_set_flag             },
                    {"getf",                  party_get_flag             },
                    {"setqf",                 party_set_quest_flag       },
                    {"chat",                  party_chat                 },
                    {"syschat",               party_syschat              },
                    {"get_near_count",        party_get_near_count       },
                    {"show_cinematic",        party_show_cinematic       },
                    {"run_cinematic",         party_run_cinematic        },
                    {"get_max_level",         party_get_max_level        },
                    {"clear_ready",           party_clear_ready          },
                    {"is_in_dungeon",         party_is_in_dungeon        },
                    {"give_buff",             party_give_buff            },
                    {"is_map_member_flag_lt", party_is_map_member_flag_lt},
                    {"get_member_pids",       party_get_member_pids      },
                    {nullptr,                 nullptr                    }
            };

            get_quest_manager().add_lua_function_table("party", party_functions);
        }
    }
}
