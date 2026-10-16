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
            quest_manager &q = quest_manager::instance();

            q.current_pc()->set_current_quest_start_flag();
            return 0;
        }

        auto quest_done(lua_State *lua) -> int {
            quest_manager &q = quest_manager::instance();

            q.current_pc()->set_current_quest_done_flag();
            return 0;
        }

        auto quest_set_title(lua_State *lua) -> int {
            quest_manager &q = quest_manager::instance();

            if (lua_isstring(lua, -1))
                q.current_pc()->set_current_quest_title(lua_tostring(lua, -1));

            return 0;
        }

        auto quest_set_another_title(lua_State *lua) -> int {
            quest_manager &q = quest_manager::instance();

            if (lua_isstring(lua, 1) && lua_isstring(lua, 2))
                q.current_pc()->set_quest_title(lua_tostring(lua, 1), lua_tostring(lua, 2));

            return 0;
        }

        auto quest_set_clock_name(lua_State *lua) -> int {
            quest_manager &q = quest_manager::instance();

            if (lua_isstring(lua, -1))
                q.current_pc()->set_current_quest_clock_name(lua_tostring(lua, -1));

            return 0;
        }

        auto quest_set_clock_value(lua_State *lua) -> int {
            quest_manager &q = quest_manager::instance();

            if (lua_isnumber(lua, -1))
                q.current_pc()->set_current_quest_clock_value((int)rint(lua_tonumber(lua, -1)));

            return 0;
        }

        auto quest_set_counter_name(lua_State *lua) -> int {
            quest_manager &q = quest_manager::instance();

            if (lua_isstring(lua, -1))
                q.current_pc()->set_current_quest_counter_name(lua_tostring(lua, -1));

            return 0;
        }

        auto quest_set_counter_value(lua_State *lua) -> int {
            quest_manager &q = quest_manager::instance();

            if (lua_isnumber(lua, -1))
                q.current_pc()->set_current_quest_counter_value((int)rint(lua_tonumber(lua, -1)));

            return 0;
        }

        auto quest_set_icon_file(lua_State *lua) -> int {
            quest_manager &q = quest_manager::instance();

            if (lua_isstring(lua, -1))
                q.current_pc()->set_current_quest_icon_file(lua_tostring(lua, -1));

            return 0;
        }

        auto quest_setstate(lua_State *lua) -> int {
            if (lua_tostring(lua, -1) == nullptr) {
                quest::error("State name is empty");
                return 0;
            }

            quest_manager &q = quest_manager::instance();
            quest_state *pqs = q.current_state();
            pc *pPC = q.current_pc();

            if (lua != pqs->co) {
                luaL_error(lua, "running thread != current thread???");
                if (metin2::config::test_server())
                    metin2::log::error("Running Lua thread is not the current quest thread");
                return 0;
            }

            if (pPC) {
                std::string stCurrentState = lua_tostring(lua, -1);
                if (metin2::config::test_server())
                    metin2::log::debug("Quest: {} set state {}", pPC->current_quest_name().c_str(), stCurrentState.c_str());
                pqs->st = q.quest_state_index(pPC->current_quest_name(), stCurrentState);
                pPC->set_current_quest_state_name(stCurrentState);
            }
            return 0;
        }

        auto quest_coroutine_yield(lua_State *lua) -> int {
            quest_manager &q = quest_manager::instance();
            if (q.is_in_other_pc_block()) {
                quest::error("Yield inside other_pc_block");
                pc *pPC = q.other_pc_block_root_pc();
                if (nullptr == pPC) {
                    quest::error("Root player of other_pc_block is missing");
                    return 0;
                }
                quest_state *pQS = pPC->running_quest_state();
                if (nullptr == pQS || nullptr == q.quest_state_name(pPC->current_quest_name(), pQS->st)) {
                    quest::error("Yield in quest {} with unknown state", pPC->current_quest_name().c_str());
                } else {
                    quest::error("Yield in quest {} state {}", pPC->current_quest_name().c_str(), q.quest_state_name(pPC->current_quest_name(), pQS->st));
                }
                return 0;
            }
            return lua_yield(lua, lua_gettop(lua));
        }

        auto quest_no_send(lua_State *lua) -> int {
            quest_manager &q = quest_manager::instance();
            q.set_no_send();
            return 0;
        }

        auto quest_get_current_quest_index(lua_State *lua) -> int {
            quest_manager &q = quest_manager::instance();
            pc *pPC = q.current_pc();

            int idx = q.quest_index_by_name(pPC->current_quest_name());
            lua_pushnumber(lua, idx);
            return 1;
        }

        auto quest_begin_other_pc_block(lua_State *lua) -> int {
            quest_manager &q = quest_manager::instance();
            uint32_t pid = lua_tonumber(lua, -1);
            q.begin_other_pc_block(pid);
            return 0;
        }

        auto quest_end_other_pc_block(lua_State *lua) -> int {
            quest_manager &q = quest_manager::instance();
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

            quest_manager::instance().add_lua_function_table("q", quest_functions);
        }
    }
}
