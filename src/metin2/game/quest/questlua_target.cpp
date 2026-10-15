#include <metin2/game/characters/character.h>
#include <metin2/game/app/event.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/quest/target.h>
#include <metin2/game/app/typedef.h>

#include <metin2/log/log.h>

#include <cstdint>

namespace metin2::game {

namespace quest 
{
	auto target_pos(lua_State* lua) -> int
	{
		character * ch = quest_manager::instance().current_character_ptr();
		int iQuestIndex = quest_manager::instance().current_pc()->current_quest_index();

		if (!lua_isstring(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3))
		{
			metin2::log::error("invalid argument, name: {}, quest_index {:d}", ch->name(), iQuestIndex);
			return 0;
		}

		pixel_position pos;

		if (!sectree_manager::instance().map_base_position_by_map_index(ch->map_index(), pos))
		{
			metin2::log::error("cannot find base position in this map {:d}", ch->map_index());
			return 0;
		}

		int x = pos.x + (int) lua_tonumber(lua, 2) * 100;
		int y = pos.y + (int) lua_tonumber(lua, 3) * 100;

		target_manager::instance().create_target(ch->player_id(),
				iQuestIndex,
				lua_tostring(lua, 1),
				TARGET_TYPE_POS,
				x,
				y,
				(int) lua_tonumber(lua, 4),
				lua_isstring(lua, 5) ? lua_tostring(lua, 5) : nullptr,
				lua_isnumber(lua, 6) ? (int)lua_tonumber(lua, 6): 1);

		return 0;
	}

	auto target_vid(lua_State* lua) -> int
	{
		character * ch = quest_manager::instance().current_character_ptr();
		int iQuestIndex = quest_manager::instance().current_pc()->current_quest_index();

		if (!lua_isstring(lua, 1) || !lua_isnumber(lua, 2))
		{
			metin2::log::error("invalid argument, name: {}, quest_index {:d}", ch->name(), iQuestIndex);
			return 0;
		}


		target_manager::instance().create_target(ch->player_id(),
				iQuestIndex,
				lua_tostring(lua, 1),
				TARGET_TYPE_VID,
				(int) lua_tonumber(lua, 2),
				0,
				ch->map_index(),
				lua_isstring(lua, 3) ? lua_tostring(lua, 3) : nullptr,
				lua_isnumber(lua, 4) ? (int)lua_tonumber(lua, 4): 1);

		return 0;
	}

	auto target_delete(lua_State* lua) -> int
	{
		character * ch = quest_manager::instance().current_character_ptr();
		int iQuestIndex = quest_manager::instance().current_pc()->current_quest_index();

		if (!lua_isstring(lua, 1))
		{
			metin2::log::error("invalid argument, name: {}, quest_index {:d}", ch->name(), iQuestIndex);
			return 0;
		}

		target_manager::instance().delete_target(ch->player_id(), iQuestIndex, lua_tostring(lua, 1));

		return 0;
	}

	auto target_clear(lua_State* lua) -> int
	{
		character * ch = quest_manager::instance().current_character_ptr();
		int iQuestIndex = quest_manager::instance().current_pc()->current_quest_index();

		target_manager::instance().delete_target(ch->player_id(), iQuestIndex, nullptr);

		return 0;
	}

	auto target_id(lua_State* lua) -> int
	{
		character * ch = quest_manager::instance().current_character_ptr();
		uint32_t dwQuestIndex = quest_manager::instance().current_pc()->current_quest_index();

		if (!lua_isstring(lua, 1))
		{
			metin2::log::error("invalid argument, name: {}, quest_index {:d}", ch->name(), dwQuestIndex);
			lua_pushnumber(lua, 0);
			return 1;
		}

		event_ptr pkEvent = target_manager::instance().target_event(ch->player_id(), dwQuestIndex, (const char *) lua_tostring(lua, 1));

		if (pkEvent)
		{
			target_info * pInfo = dynamic_cast<target_info *>(pkEvent->info);

			if ( pInfo == nullptr )
			{
				metin2::log::error( "target_id> <Factor> Null pointer" );
				lua_pushnumber(lua, 0);
				return 1;
			}

			if (pInfo->type == TARGET_TYPE_VID)
			{
				lua_pushnumber(lua, pInfo->arg1);
				return 1;
			}
		}

		lua_pushnumber(lua, 0);
		return 1;
	}

	void register_target_function_table()
	{
		luaL_reg target_functions[] =
		{
			{ "pos",			target_pos		},
			{ "vid",			target_vid		},
			{ "npc",			target_vid		},
			{ "pc",			target_vid		},
			{ "delete",			target_delete		},
			{ "clear",			target_clear		},
			{ "id",			target_id		},
			{ nullptr,			nullptr			},
		};

		quest_manager::instance().add_lua_function_table("target", target_functions);
	}
};
}
