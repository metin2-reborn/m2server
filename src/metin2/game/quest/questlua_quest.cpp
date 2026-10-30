#include <metin2/config/config.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/quest.h>
#include <metin2/game/quest/questlua.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/quest/questpc.h>

#include <metin2/log/log.h>

#include <cmath>
#include <cstdint>
#include <string>

namespace metin2::game {

    namespace quest {
        auto quest_start(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();

            q.current_pc()->set_current_quest_start_flag();
            return 0;
        }

        auto quest_done(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();

            q.current_pc()->set_current_quest_done_flag();
            return 0;
        }

        auto quest_set_title(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();

            if (lua_isstring(lua, -1))
                q.current_pc()->set_current_quest_title(lua_tostring(lua, -1));

            return 0;
        }

        auto quest_set_another_title(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();

            if (lua_isstring(lua, 1) && lua_isstring(lua, 2))
                q.current_pc()->set_quest_title(lua_tostring(lua, 1), lua_tostring(lua, 2));

            return 0;
        }

        auto quest_set_clock_name(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();

            if (lua_isstring(lua, -1))
                q.current_pc()->set_current_quest_clock_name(lua_tostring(lua, -1));

            return 0;
        }

        auto quest_set_clock_value(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();

            if (lua_isnumber(lua, -1))
                q.current_pc()->set_current_quest_clock_value((int)rint(lua_tonumber(lua, -1)));

            return 0;
        }

        auto quest_set_counter_name(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();

            if (lua_isstring(lua, -1))
                q.current_pc()->set_current_quest_counter_name(lua_tostring(lua, -1));

            return 0;
        }

        auto quest_set_counter_value(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();

            if (lua_isnumber(lua, -1))
                q.current_pc()->set_current_quest_counter_value((int)rint(lua_tonumber(lua, -1)));

            return 0;
        }

        auto quest_set_icon_file(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();

            if (lua_isstring(lua, -1))
                q.current_pc()->set_current_quest_icon_file(lua_tostring(lua, -1));

            return 0;
        }

        auto quest_set_letter(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();

            if (lua_isstring(lua, 1))
                q.current_pc()->set_current_quest_letter(lua_tostring(lua, 1));

            return 0;
        }

        // q.set_status(QUEST_TURN_IN) marks the current state as the hand-in of the quest,
        // QUEST_ACTIVE clears that and QUEST_NEW makes the letter unread again.
        auto quest_set_status(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();

            if (lua_isnumber(lua, 1)) {
                const int status = (int)rint(lua_tonumber(lua, 1));
                if (status >= pc::QUEST_STATUS_ACTIVE && status <= pc::QUEST_STATUS_TURN_IN)
                    q.current_pc()->set_current_quest_status(static_cast<pc::quest_status>(status));
            }

            return 0;
        }

        auto quest_setstate(lua_State *lua) -> int {
            if (lua_tostring(lua, -1) == nullptr) {
                quest::error("State name is empty");
                return 0;
            }

            quest_manager &q = get_quest_manager();
            quest_state *pqs = q.current_state();
            pc *pc = q.current_pc();

            if (lua != pqs->co) {
                luaL_error(lua, "running thread != current thread???");
                if (metin2::config::test_server())
                    metin2::log::error("Running Lua thread is not the current quest thread");
                return 0;
            }

            if (pc) {
                std::string current_state = lua_tostring(lua, -1);
                if (metin2::config::test_server())
                    metin2::log::debug("Quest: {} set state {}", pc->current_quest_name(), current_state);
                pqs->st = q.quest_state_index(pc->current_quest_name(), current_state);
                pc->set_current_quest_state_name(current_state);
            }
            return 0;
        }

        auto quest_coroutine_yield(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();
            if (q.is_in_other_pc_block()) {
                quest::error("Yield inside other_pc_block");
                pc *pc = q.other_pc_block_root_pc();
                if (nullptr == pc) {
                    quest::error("Root player of other_pc_block is missing");
                    return 0;
                }
                quest_state *qs = pc->running_quest_state();
                if (nullptr == qs || nullptr == q.quest_state_name(pc->current_quest_name(), qs->st)) {
                    quest::error("Yield in quest {} with unknown state", pc->current_quest_name().c_str());
                } else {
                    quest::error("Yield in quest {} state {}", pc->current_quest_name().c_str(), q.quest_state_name(pc->current_quest_name(), qs->st));
                }
                return 0;
            }
            return lua_yield(lua, lua_gettop(lua));
        }

        auto quest_no_send(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();
            q.set_no_send();
            return 0;
        }

        auto quest_get_current_quest_index(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();
            pc *pc = q.current_pc();

            int idx = q.quest_index_by_name(pc->current_quest_name());
            lua_pushnumber(lua, idx);
            return 1;
        }

        auto quest_begin_other_pc_block(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();
            uint32_t pid = lua_tonumber(lua, -1);
            q.begin_other_pc_block(pid);
            return 0;
        }

        auto quest_end_other_pc_block(lua_State *lua) -> int {
            quest_manager &q = get_quest_manager();
            q.end_other_pc_block();
            return 0;
        }

        void register_quest_function_table() {
            luaL_reg quest_functions[] =
                {
                    {"setstate",             quest_setstate               },
                    {"set_state",            quest_setstate               },
                    {"yield",                quest_coroutine_yield        },
                    {"set_title",            quest_set_title              },
                    {"set_title2",           quest_set_another_title      },
                    {"set_clock_name",       quest_set_clock_name         },
                    {"set_clock_value",      quest_set_clock_value        },
                    {"set_counter_name",     quest_set_counter_name       },
                    {"set_counter_value",    quest_set_counter_value      },
                    {"set_icon",             quest_set_icon_file          },
                    {"set_letter",           quest_set_letter             },
                    {"set_status",           quest_set_status             },
                    {"start",                quest_start                  },
                    {"done",                 quest_done                   },
                    {"getcurrentquestindex", quest_get_current_quest_index},
                    {"no_send",              quest_no_send                },
                    // Between begin_other_pc_block(pid) and end_other_pc_block the current pc is pid. Nothing
                    // in between may yield (wait, select, input, ...).
                    {"begin_other_pc_block", quest_begin_other_pc_block   },
                    {"end_other_pc_block",   quest_end_other_pc_block     },
                    {nullptr,                nullptr                      }
            };

            get_quest_manager().add_lua_function_table("q", quest_functions);
        }
    }
}
