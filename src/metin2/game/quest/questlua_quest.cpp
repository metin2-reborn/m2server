#include <metin2/game/app/config.h>
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


namespace quest
{
	//
	// "quest" Lua functions
	//
	int quest_start(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();

		q.current_pc()->set_current_quest_start_flag();
		return 0;
	}

	int quest_done(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();

		q.current_pc()->set_current_quest_done_flag();
		return 0;
	}

	int quest_set_title(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();

		if (lua_isstring(lua,-1))
			q.current_pc()->set_current_quest_title(lua_tostring(lua,-1));

		return 0;
	}

	int quest_set_another_title(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();

		if (lua_isstring(lua,1) && lua_isstring(lua,2))
			q.current_pc()->set_quest_title(lua_tostring(lua,1),lua_tostring(lua,2));

		return 0;
	}

	int quest_set_clock_name(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();

		if (lua_isstring(lua,-1))
			q.current_pc()->set_current_quest_clock_name(lua_tostring(lua,-1));

		return 0;
	}

	int quest_set_clock_value(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();

		if (lua_isnumber(lua,-1))
			q.current_pc()->set_current_quest_clock_value((int)rint(lua_tonumber(lua,-1)));

		return 0;
	}

	int quest_set_counter_name(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();

		if (lua_isstring(lua,-1))
			q.current_pc()->set_current_quest_counter_name(lua_tostring(lua,-1));

		return 0;
	}

	int quest_set_counter_value(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();

		if (lua_isnumber(lua,-1))
			q.current_pc()->set_current_quest_counter_value((int)rint(lua_tonumber(lua,-1)));

		return 0;
	}

	int quest_set_icon_file(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();

		if (lua_isstring(lua,-1))
			q.current_pc()->set_current_quest_icon_file(lua_tostring(lua,-1));

		return 0;
	}

	int quest_setstate(lua_State* lua)
	{
		if (lua_tostring(lua, -1)==nullptr)
		{
			quest::error("state name is empty");
			return 0;
		}

		quest_manager& q = quest_manager::instance();
		quest_state * pqs = q.current_state();
		pc* pPC = q.current_pc();

		if (lua!=pqs->co) 
		{
			luaL_error(lua, "running thread != current thread???");
			if ( metin2::config::test_server() )
				metin2::log::info("running thread != current thread???");
			return 0;
		}

		if (pPC)
		{
			//cerr << "QUEST new state" << pPC->GetCurrentQuestName(); << ":"
			//
			std::string stCurrentState = lua_tostring(lua,-1);
			if ( metin2::config::test_server() )
				metin2::log::info("questlua->setstate( {}, {} )", pPC->current_quest_name().c_str(), stCurrentState.c_str() );
			pqs->st = q.quest_state_index(pPC->current_quest_name(), stCurrentState);
			pPC->set_current_quest_state_name(stCurrentState );
		}
		return 0;
	}

	int quest_coroutine_yield(lua_State * lua)
	{
		quest_manager& q = quest_manager::instance();
		// A yield must never happen inside an other_pc_block. Never.
		if (q.is_in_other_pc_block())
		{
			quest::error("FATAL ERROR! Yield occur in other_pc_block.");
			pc* pPC = q.other_pc_block_root_pc();
			if (nullptr == pPC)
			{
				quest::error("	... FFFAAATTTAAALLL Error. RootPC is NULL");
				return 0;
			}
			quest_state* pQS = pPC->running_quest_state();
			if (nullptr == pQS || nullptr == q.quest_state_name(pPC->current_quest_name(), pQS->st))
			{
				quest::error("	... WHO AM I? WHERE AM I? I only know QuestName({})...", pPC->current_quest_name().c_str());
			}
			else
			{
				quest::error("	Current Quest({}). State({})", pPC->current_quest_name().c_str(), q.quest_state_name(pPC->current_quest_name(), pQS->st));
			}
			return 0;
		}
		return lua_yield(lua, lua_gettop(lua));
	}

	int quest_no_send(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		q.set_no_send();
		return 0;
	}

	int quest_get_current_quest_index(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		pc* pPC = q.current_pc();

		int idx = q.quest_index_by_name(pPC->current_quest_name());
		lua_pushnumber(lua, idx);
		return 1;
	}

	int quest_begin_other_pc_block(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		uint32_t pid = lua_tonumber(lua, -1);
		q.begin_other_pc_block(pid);
		return 0;
	}

	int quest_end_other_pc_block(lua_State* lua)
	{
		quest_manager& q = quest_manager::instance();
		q.end_other_pc_block();
		return 0;
	}

	void register_quest_function_table()
	{
		luaL_reg quest_functions[] = 
		{
			{ "setstate",				quest_setstate				},
			{ "set_state",				quest_setstate				},
			{ "yield",					quest_coroutine_yield		},
			{ "set_title",				quest_set_title				},
			{ "set_title2",				quest_set_another_title		},
			{ "set_clock_name",			quest_set_clock_name		},
			{ "set_clock_value",		quest_set_clock_value		},
			{ "set_counter_name",		quest_set_counter_name		},
			{ "set_counter_value",		quest_set_counter_value		},
			{ "set_icon",				quest_set_icon_file			},
			{ "start",					quest_start					},
			{ "done",					quest_done					},
			{ "getcurrentquestindex",	quest_get_current_quest_index	},
			{ "no_send",				quest_no_send				},
			// Call the region between begin_other_pc_block(pid) and end_other_pc_block an other_pc_block.
			// Inside an other_pc_block, current_pc is changed to pid.
			//						When it ends, it returns to the original current_pc.
			/*		Made for things like this.
					for i, pid in next, pids, nil do
						q.begin_other_pc_block(pid)
						if pc.count_item(PASS_TICKET) < 1 then
							table.insert(criminalNames, pc.get_name())
							canPass = false
						end
						q.end_other_pc_block()
					end
			*/
			// Caution: a yield must never happen inside an other_pc_block. (ex. wait, select, input, ...)
			{ "begin_other_pc_block",	quest_begin_other_pc_block	}, 
			{ "end_other_pc_block",		quest_end_other_pc_block	},
			{ nullptr,						nullptr						}
		};

		quest_manager::instance().add_lua_function_table("q", quest_functions);
	}
}
}
