#include <metin2/game/characters/character.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/quest/questlua.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/quest/questpc.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/log/log.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace metin2::game {


namespace quest
{
	auto party_clear_ready(lua_State* lua) -> int
	{
		character * ch = quest_manager::instance().current_character_ptr();
		
		if (ch->get_party())
		{
			f_party_clear_ready f;
			ch->get_party()->for_each_near_member(f);
		}
		return 0;
	}

	auto party_get_max_level(lua_State* lua) -> int
	{
		character * ch = quest_manager::instance().current_character_ptr();

		if (ch->get_party())
			lua_pushnumber(lua,ch->get_party()->member_max_level());
		else
			lua_pushnumber(lua, 1);

		return 1;
	}

    struct f_run_cinematic_sender
    {
        std::string data;
        struct packet_script pack;

        f_run_cinematic_sender(const char* str)
        {
            data = "[RUN_CINEMA value;";
            data += str;
            data += "]";

            pack.header = HEADER_GC_SCRIPT;
            pack.skin = quest_manager::QUEST_SKIN_CINEMATIC;
            pack.src_size = data.size();
            pack.size = pack.src_size + sizeof(struct packet_script);
        }

        void operator()(character * ch)
        {
            metin2::log::debug("Party cinematic: sending to {}", ch->name());

            if (ch->get_desc())
            {
                metin2::log::debug("Party cinematic: sent to {}", ch->name());
                ch->get_desc()->buffered_packet(&pack, sizeof(struct packet_script));
                ch->get_desc()->packet(data.c_str(),data.size());
            }
        }
    };

	auto party_run_cinematic(lua_State* lua) -> int
	{
		if (!lua_isstring(lua, 1))
			return 0;
		
		metin2::log::debug("Party cinematic: run {}", lua_tostring(lua, 1));
		character * ch = quest_manager::instance().current_character_ptr();

		if (ch->get_party())
		{
			f_run_cinematic_sender f(lua_tostring(lua, 1));

			ch->get_party()->update();
			ch->get_party()->for_each_near_member(f);
		}

		return 0;
	}

	struct f_cinematic_sender
	{
		const char* str;
		struct metin2::game::packet_script packet_script;
		int len;

		f_cinematic_sender(const char* str)
			: str(str)
		{
			len = strlen(str);

			packet_script.header = HEADER_GC_SCRIPT;
			packet_script.skin = quest_manager::QUEST_SKIN_CINEMATIC;
			packet_script.src_size = len;
			packet_script.size = packet_script.src_size + sizeof(struct game::protocol::packet_script);
		}

		void operator()(character * ch)
		{
			metin2::log::debug("Party script: sending to {}", ch->name());

			if (ch->get_desc())
			{
				metin2::log::debug("Party script: sent to {}", ch->name());
				ch->get_desc()->buffered_packet(&packet_script, sizeof(struct game::protocol::packet_script));
				ch->get_desc()->packet(str,len);
			}
		}
	};

	auto party_show_cinematic(lua_State* lua) -> int
	{
		if (!lua_isstring(lua, 1))
			return 0;

		metin2::log::debug("Party script: show {}", lua_tostring(lua, 1));
		character * ch = quest_manager::instance().current_character_ptr();

		if (ch->get_party())
		{
			f_cinematic_sender f(lua_tostring(lua, 1));

			ch->get_party()->update();
			ch->get_party()->for_each_near_member(f);
		}
		return 0;
	}

	auto party_get_near_count(lua_State* lua) -> int
	{
		character * ch = quest_manager::instance().current_character_ptr();

		if (ch->get_party())
			lua_pushnumber(lua, ch->get_party()->near_member_count());
		else
			lua_pushnumber(lua, 0);

		return 1;
	}

	auto party_syschat(lua_State* lua) -> int
	{
		party * pParty = quest_manager::instance().current_character_ptr()->get_party();

		if (pParty)
		{
			std::ostringstream s;
			combine_lua_string(lua, s);

			f_party_chat f(CHAT_TYPE_INFO, s.str().c_str());

			pParty->for_each_online_member(f);
		}

		return 0;
	}

	auto party_is_leader(lua_State* lua) -> int
	{
		character * ch = quest_manager::instance().current_character_ptr();

		if (ch->get_party() && ch->get_party()->leader_pid() == ch->player_id())
			lua_pushboolean(lua, 1);
		else
			lua_pushboolean(lua, 0);

		return 1;
	}

	auto party_is_party(lua_State* lua) -> int
	{
		character * ch = quest_manager::instance().current_character_ptr();
		lua_pushboolean(lua, ch->get_party() ? 1 : 0);
		return 1;
	}

	auto party_get_leader_pid(lua_State* lua) -> int
	{
		character * ch = quest_manager::instance().current_character_ptr();
		if (ch->get_party())
		{
			lua_pushnumber(lua, ch->get_party()->leader_pid());
		}
		else
		{
			lua_pushnumber(lua, -1);
		}
		return 1;
	}


	auto party_chat(lua_State* lua) -> int
	{
		party * pParty = quest_manager::instance().current_character_ptr()->get_party();

		if (pParty)
		{
			std::ostringstream s;
			combine_lua_string(lua, s);

			f_party_chat f(CHAT_TYPE_TALKING, s.str().c_str());

			pParty->for_each_online_member(f);
		}

		return 0;
	}


	auto party_is_map_member_flag_lt(lua_State* lua) -> int
	{

		if (!lua_isstring(lua, 1) || !lua_isnumber(lua, 2))
		{
			lua_pushnumber(lua, 0);
			return 1;
		}

		quest_manager& q = quest_manager::instance();
		party * pParty = q.current_character_ptr()->get_party();
		character * ch = q.current_character_ptr();
		pc* pPC = q.current_pc();

		const char* sz = lua_tostring(lua,1);

		if (pParty)
		{
			f_party_check_flag_lt f;
			f.flagname = pPC->current_quest_name() + "."+sz;
			f.value = (int) rint(lua_tonumber(lua, 2));

			bool returnBool = pParty->for_each_on_map_member_bool(f, ch->map_index());
			lua_pushboolean(lua, returnBool);
		}

		return 1;
	}

	auto party_set_flag(lua_State* lua) -> int
	{
		character * ch = quest_manager::instance().current_character_ptr();

		if (ch->get_party() && lua_isstring(lua, 1) && lua_isnumber(lua, 2))
			ch->get_party()->set_flag(lua_tostring(lua, 1), (int)lua_tonumber(lua, 2));

		return 0;
	}

	auto party_get_flag(lua_State* lua) -> int
	{
		character * ch = quest_manager::instance().current_character_ptr();

		if (!ch->get_party() || !lua_isstring(lua, 1))
			lua_pushnumber(lua, 0);
		else
			lua_pushnumber(lua, ch->get_party()->flag(lua_tostring(lua, 1)));

		return 1;
	}

	auto party_set_quest_flag(lua_State* lua) -> int
	{
		quest_manager & q = quest_manager::instance();

		f_set_quest_flag f;

		f.flagname = q.current_pc()->current_quest_name() + "." + lua_tostring(lua, 1);
		f.value = (int) rint(lua_tonumber(lua, 2));

		character * ch = q.current_character_ptr();

		if (ch->get_party())
			ch->get_party()->for_each_online_member(f);
		else
			f(ch);

		return 0;
	}

	auto party_is_in_dungeon (lua_State* lua) -> int
	{
		quest_manager & q = quest_manager::instance();
		character * ch = q.current_character_ptr();
		party * pParty = ch->get_party();
		if (pParty != nullptr){
			lua_pushboolean (lua, pParty->get_dungeon() ? true : false);
			return 1;
		}
		lua_pushboolean (lua, false);
		return 1;
	}

	struct f_give_buff
	{
		uint32_t type;
		uint8_t apply_on;
		int32_t apply_value;
		uint32_t flag;
		int32_t duration;
		int32_t sp_cost;
		bool override;
		bool is_cube;

		f_give_buff (uint32_t type_value, uint8_t apply_on_value, int32_t apply_value_value, uint32_t flag_value, int32_t duration_value,
					int32_t sp_cost_value, bool override_value, bool is_cube_value = false)
			: type (type_value), apply_on (apply_on_value), apply_value (apply_value_value), flag(flag_value), duration(duration_value),
				sp_cost(sp_cost_value), override(override_value), is_cube(is_cube_value)
		{}
		void operator () (character * ch)
		{
			ch->add_affect(type, apply_on, apply_value, flag, duration, sp_cost, override, is_cube);
		}
	};
	
	auto party_give_buff (lua_State* lua) -> int
	{
		quest_manager & q = quest_manager::instance();
		character * ch = q.current_character_ptr();
		
		if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3) || !lua_isnumber(lua, 4) || 
			!lua_isnumber(lua, 5) || !lua_isnumber(lua, 6) || !lua_isboolean(lua, 7) || !lua_isboolean(lua, 8))
		{
			lua_pushboolean (lua, false);
			return 1;
		}
		uint32_t dwType = lua_tonumber(lua, 1);
		uint8_t bApplyOn = lua_tonumber(lua, 2);
		int32_t lApplyValue = lua_tonumber(lua, 3);
		uint32_t dwFlag = lua_tonumber(lua, 4);
		int32_t lDuration = lua_tonumber(lua, 5);
		int32_t lSPCost = lua_tonumber(lua, 6);
		bool bOverride = lua_toboolean(lua, 7);
		bool IsCube = lua_toboolean(lua, 8);

		f_give_buff f (dwType, bApplyOn, lApplyValue, dwFlag, lDuration, lSPCost, bOverride, IsCube);
		if (ch->get_party())
			ch->get_party()->for_each_on_map_member(f, ch->map_index());
		else
			f(ch);

		lua_pushboolean (lua, true);
		return 1;
	}

	struct f_party_pid_collector
	{
		std::vector <uint32_t> pi_ds;
		f_party_pid_collector()
		{
		}
		void operator () (character * ch)
		{
			pi_ds.push_back(ch->player_id());
		}
	};
	auto party_get_member_pids(lua_State *lua) -> int
	{
		quest_manager & q = quest_manager::instance();
		character * ch = q.current_character_ptr();
		party * pParty = ch->get_party();
		if (nullptr == pParty)
		{
			return 0;
		}
		f_party_pid_collector f;
		pParty->for_each_on_map_member(f, ch->map_index());
		
		for (std::vector <uint32_t>::iterator it = f.pi_ds.begin(); it != f.pi_ds.end(); it++)
		{
			lua_pushnumber(lua, *it);
		}
		return f.pi_ds.size();
	}

	void register_party_function_table()
	{
		luaL_reg party_functions[] = 
		{
			{ "is_leader",		party_is_leader		},
			{ "is_party",		party_is_party		},
			{ "get_leader_pid",	party_get_leader_pid},
			{ "setf",			party_set_flag		},
			{ "getf",			party_get_flag		},
			{ "setqf",			party_set_quest_flag},
			{ "chat",			party_chat			},
			{ "syschat",		party_syschat		},
			{ "get_near_count",	party_get_near_count},
			{ "show_cinematic",	party_show_cinematic},
			{ "run_cinematic",	party_run_cinematic	},
			{ "get_max_level",	party_get_max_level	},
			{ "clear_ready",	party_clear_ready	},
			{ "is_in_dungeon",	party_is_in_dungeon	},
			{ "give_buff",		party_give_buff		},
			{ "is_map_member_flag_lt",	party_is_map_member_flag_lt	},
			{ "get_member_pids",		party_get_member_pids	},
			{ nullptr,				nullptr				}
		};

		quest_manager::instance().add_lua_function_table("party", party_functions);
	}
}
}
