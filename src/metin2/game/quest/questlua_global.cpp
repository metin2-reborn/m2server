#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/app/event.h>
#include <metin2/game/events/fishing.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/events/mining.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/items/over9refine.h>
#include <metin2/game/events/ox_event.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/empire/priv_manager.h>
#include <metin2/game/quest/quest.h>
#include <metin2/game/quest/questevent.h>
#include <metin2/game/quest/questlua.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/quest/questpc.h>
#include <metin2/game/world/regen.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/empire/start_position.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

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



namespace quest
{
	auto get_locale(lua_State* lua) -> int
	{
		lua_pushstring(lua, locale.c_str());
		return 1;
	}

	auto random_number(lua_State* lua) -> int
	{
		if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2))
			lua_pushnumber(lua, 0);
		else
			lua_pushnumber(lua, number((int)lua_tonumber(lua, 1), (int)lua_tonumber(lua, 2)));
		return 1;
	}

	auto time_to_str(lua_State* lua) -> int
	{
		time_t curTime = (time_t)lua_tonumber(lua, -1);
		lua_pushstring(lua, asctime(gmtime(&curTime)));
		return 1;
	}

	auto say(lua_State* lua) -> int
	{
		std::ostringstream s;
		combine_lua_string(lua, s);
		quest_manager::instance().add_script(s.str() + "[ENTER]");
		return 0;
	}

	auto chat(lua_State* lua) -> int
	{
		std::ostringstream s;
		combine_lua_string(lua, s);

		quest_manager::instance().current_character_ptr()->chat_packet(CHAT_TYPE_TALKING, "%s", s.str().c_str());
		return 0;
	}

	auto cmdchat(lua_State* lua) -> int
	{
		std::ostringstream s;
		combine_lua_string(lua, s);
		quest_manager::instance().current_character_ptr()->chat_packet(CHAT_TYPE_COMMAND, "%s", s.str().c_str());
		return 0;
	}

	auto syschat(lua_State* lua) -> int
	{
		std::ostringstream s;
		combine_lua_string(lua, s);
		quest_manager::instance().current_character_ptr()->chat_packet(CHAT_TYPE_INFO, "%s", s.str().c_str());
		return 0;
	}

	auto notice(lua_State* lua) -> int
	{
		std::ostringstream s;
		combine_lua_string(lua, s);
		quest_manager::instance().current_character_ptr()->chat_packet(CHAT_TYPE_NOTICE, "%s", s.str().c_str());
		return 0;
	}

	auto left_image(lua_State* lua) -> int
	{
		if (lua_isstring(lua, -1))
		{
			std::string s = lua_tostring(lua,-1);
			quest_manager::instance().add_script("[LEFTIMAGE src;"+s+"]");
		}
		return 0;
	}

	auto top_image(lua_State* lua) -> int
	{
		if (lua_isstring(lua, -1))
		{
			std::string s = lua_tostring(lua,-1);
			quest_manager::instance().add_script("[TOPIMAGE src;"+s+"]");
		}
		return 0;
	}

	auto set_skin(lua_State* lua) -> int
	{
		if (lua_isnumber(lua, -1))
		{
			quest_manager::instance().set_skin_style((int)rint(lua_tonumber(lua,-1)));
		}
		else
		{
			quest::error("QUEST wrong skin index");
		}

		return 0;
	}

	auto set_server_timer(lua_State* lua) -> int
	{
		int n = lua_gettop(lua);
		if ((n != 2 || !lua_isnumber(lua, 2) || !lua_isstring(lua, 1)) &&
				(n != 3 || !lua_isstring(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3)))
		{
			quest::error("QUEST set_server_timer argument count wrong.");
			return 0;
		}

		const char * name = lua_tostring(lua, 1);
		double t = lua_tonumber(lua, 2);
		uint32_t arg = 0;

		quest_manager & q = quest_manager::instance();

		if (lua_isnumber(lua, 3))
			arg = (uint32_t) lua_tonumber(lua, 3);

		int timernpc = q.load_timer_script(name);

		event_ptr event = quest_create_server_timer_event(name, t, timernpc, false, arg);
		q.add_server_timer(name, arg, event);
		return 0;
	}

	auto set_server_loop_timer(lua_State* lua) -> int
	{
		int n = lua_gettop(lua);
		if ((n != 2 || !lua_isnumber(lua, 2) || !lua_isstring(lua, 1)) &&
				(n != 3 || !lua_isstring(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3)))
		{
			quest::error("QUEST set_server_timer argument count wrong.");
			return 0;
		}
		const char * name = lua_tostring(lua, 1);
		double t = lua_tonumber(lua, 2);
		uint32_t arg = 0;
		quest_manager & q = quest_manager::instance();

		if (lua_isnumber(lua, 3))
			arg = (uint32_t) lua_tonumber(lua, 3);

		int timernpc = q.load_timer_script(name);

		event_ptr event = quest_create_server_timer_event(name, t, timernpc, true, arg);
		q.add_server_timer(name, arg, event);
		return 0;
	}

	auto clear_server_timer(lua_State* lua) -> int
	{
		quest_manager & q = quest_manager::instance();
		const char * name = lua_tostring(lua, 1);
		uint32_t arg = (uint32_t) lua_tonumber(lua, 2);
		q.clear_server_timer(name, arg);
		return 0;
	}

	auto set_named_loop_timer(lua_State* lua) -> int
	{
		int n = lua_gettop(lua);

		if (n != 2 || !lua_isnumber(lua, -1) || !lua_isstring(lua, -2))
			quest::error("QUEST set_timer argument count wrong.");
		else
		{
			const char * name = lua_tostring(lua, -2);
			double t = lua_tonumber(lua, -1);

			quest_manager & q = quest_manager::instance();
			int timernpc = q.load_timer_script(name);
			q.current_pc()->add_timer(name, quest_create_timer_event(name, q.current_character_ptr()->player_id(), t, timernpc, true));
		}

		return 0;
	}

	auto get_server_timer_arg(lua_State* lua) -> int
	{
		lua_pushnumber(lua, quest_manager::instance().server_timer_arg());
		return 1;
	}

	auto set_timer(lua_State* lua) -> int
	{
		if (lua_gettop(lua) != 1 || !lua_isnumber(lua, -1))
			quest::error("QUEST invalid argument.");
		else
		{
			double t = lua_tonumber(lua, -1);

			quest_manager& q = quest_manager::instance();
			quest_create_timer_event("", q.current_character_ptr()->player_id(), t);
		}

		return 0;
	}

	auto set_named_timer(lua_State* lua) -> int
	{
		int n = lua_gettop(lua);

		if (n != 2 || !lua_isnumber(lua, -1) || !lua_isstring(lua, -2))
		{
			quest::error("QUEST set_timer argument count wrong.");
		}
		else
		{
			const char * name = lua_tostring(lua,-2);
			double t = lua_tonumber(lua, -1);

			quest_manager & q = quest_manager::instance();
			int timernpc = q.load_timer_script(name);
			q.current_pc()->add_timer(name, quest_create_timer_event(name, q.current_character_ptr()->player_id(), t, timernpc));
		}

		return 0;
	}

	auto timer(lua_State * lua) -> int
	{
		if (lua_gettop(lua) == 1)
			return set_timer(lua);
		else
			return set_named_timer(lua);
	}

	auto clear_named_timer(lua_State* lua) -> int
	{
		int n = lua_gettop(lua);

		if (n != 1 || !lua_isstring(lua, -1))
			quest::error("QUEST set_timer argument count wrong.");
		else
		{
			quest_manager & q = quest_manager::instance();
			q.current_pc()->remove_timer(lua_tostring(lua, -1));
		}

		return 0;
	}

	auto getnpcid(lua_State * lua) -> int
	{
		const char * name = lua_tostring(lua, -1);
		quest_manager & q = quest_manager::instance();
		lua_pushnumber(lua, q.find_npcid_by_name(name));
		return 1;
	}

	auto is_test_server(lua_State * lua) -> int
	{
		lua_pushboolean(lua, metin2::config::test_server());
		return 1;
	}

	auto is_speed_server(lua_State * lua) -> int
	{
		lua_pushboolean(lua, metin2::config::game().speed_server);
		return 1;
	}

	auto raw_script(lua_State* lua) -> int
	{
		if ( metin2::config::test_server() )
			metin2::log::info( "_raw_script : {} ", lua_tostring(lua,-1));
		if (lua_isstring(lua, -1))
			quest_manager::instance().add_script(lua_tostring(lua,-1));
		else
			quest::error("QUEST wrong argument: questname: {}", quest_manager::instance().current_quest_name().c_str());

		return 0;
	}

	auto char_log(lua_State * lua) -> int
	{
		quest_manager& q = quest_manager::instance();
		character * ch = q.current_character_ptr();

		uint32_t what = 0;
		const char* how = "";
		const char* hint = "";

		if (lua_isnumber(lua, 1)) what = (uint32_t)lua_tonumber(lua, 1);
		if (lua_isstring(lua, 2)) how = lua_tostring(lua, 2);
		if (lua_tostring(lua, 3)) hint = lua_tostring(lua, 3);

		log_manager::instance().char_log(ch, what, how, hint);
		return 0;
	}
	
	auto item_log(lua_State* lua) -> int
	{
		quest_manager& q = quest_manager::instance();
		character * ch = q.current_character_ptr();

		uint32_t dwItemID = 0;
		const char* how = "";
		const char* hint = "";

		if ( lua_isnumber(lua, 1) ) dwItemID = (uint32_t)lua_tonumber(lua, 1);
		if ( lua_isstring(lua, 2) ) how = lua_tostring(lua, 2);
		if ( lua_tostring(lua, 3) ) hint = lua_tostring(lua, 3);

		item * item = item_manager::instance().find(dwItemID);

		if (item)
			log_manager::instance().item_log(ch, item, how, hint);

		return 0;
	}

	auto syslog(lua_State* lua) -> int
	{
		if (!lua_isnumber(lua, 1) || !lua_isstring(lua, 2))
			return 0;

		if (lua_tonumber(lua, 1) >= 1)
		{
			if (!metin2::config::test_server())
				return 0;
		}

		pc* pc = quest_manager::instance().current_pc();

		if (!pc)
			return 0;

		character * ch = quest_manager::instance().current_character_ptr();

		if (!ch)
			return 0;

		metin2::log::info( "QUEST: quest: {} player: {} : {}", pc->current_quest_name().c_str(), ch->name(), lua_tostring(lua, 2));

		if (true == metin2::config::test_server())
		{
			ch->chat_packet(CHAT_TYPE_INFO, "QUEST_SYSLOG %s", lua_tostring(lua, 2));
		}

		return 0;
	}

	auto syserr(lua_State* lua) -> int
	{
		if (!lua_isstring(lua, 1))
			return 0;

		pc* pc = quest_manager::instance().current_pc();

		if (!pc)
			return 0;

		character * ch = quest_manager::instance().current_character_ptr();

		if (!ch)
			return 0;

		quest::error("QUEST: quest: {} player: {} : {}", pc->current_quest_name().c_str(), ch->name(), lua_tostring(lua, 1));
		ch->chat_packet(CHAT_TYPE_INFO, "QUEST_SYSERR %s", lua_tostring(lua, 1));
		return 0;
	}
	
	auto set_bgm_volume_enable(lua_State* lua) -> int
	{
		character_set_bgm_volume_enable();

		return 0;
	}

	auto add_bgm_info(lua_State* lua) -> int
	{
		if (!lua_isnumber(lua, 1) || !lua_isstring(lua, 2))
			return 0;

		int mapIndex		= (int)lua_tonumber(lua, 1);

		const char*	bgmName	= lua_tostring(lua, 2);
		if (!bgmName)
			return 0;

		float bgmVol = lua_isnumber(lua, 3) ? lua_tonumber(lua, 3) : (1.0f/5.0f)*0.1f;

		character_add_bgm_info(mapIndex, bgmName, bgmVol);

		return 0;
	}

	auto add_goto_info(lua_State* lua) -> int
	{
		const char* name = lua_tostring(lua, 1);

		int empire 	= (int)lua_tonumber(lua, 2);
		int mapIndex 	= (int)lua_tonumber(lua, 3);
		int x 		= (int)lua_tonumber(lua, 4);
		int y 		= (int)lua_tonumber(lua, 5);

		if (!name)
			return 0;

		character_add_goto_info(name, empire, mapIndex, x, y);
		return 0;
	}

	auto refine_pick(lua_State* lua) -> int
	{
		uint8_t bCell = (uint8_t) lua_tonumber(lua,-1);

		quest_manager& q = quest_manager::instance();

		character * ch = q.current_character_ptr();

		item * item = ch->inventory_item(bCell);

		int ret = mining::real_refine_pick(ch, item);
		lua_pushnumber(lua, ret);
		return 1;
	}

	auto fish_real_refine_rod(lua_State* lua) -> int
	{
		uint8_t bCell = (uint8_t) lua_tonumber(lua,-1);

		quest_manager& q = quest_manager::instance();

		character * ch = q.current_character_ptr();

		item * item = ch->inventory_item(bCell);

		int ret = fishing::real_refine_rod(ch, item);
		lua_pushnumber(lua, ret);
		return 1;
	}

	auto give_char_privilege(lua_State* lua) -> int
	{
		int pid = quest_manager::instance().current_character_ptr()->player_id();
		int type = (int)lua_tonumber(lua, 1);
		int value = (int)lua_tonumber(lua, 2);

		if (MAX_PRIV_NUM <= type)
		{
			quest::error("PRIV_MANAGER: _give_char_privilege: wrong empire priv type({:d})", type);
			return 0;
		}

		priv_manager::instance().request_give_character_priv(pid, type, value);

		return 0;
	}

	auto give_empire_privilege(lua_State* lua) -> int
	{
		int empire = (int)lua_tonumber(lua,1);
		int type = (int)lua_tonumber(lua, 2);
		int value = (int)lua_tonumber(lua, 3);
		int time = (int) lua_tonumber(lua,4);
		character * ch = quest_manager::instance().current_character_ptr();

		if (MAX_PRIV_NUM <= type)
		{
			quest::error("PRIV_MANAGER: _give_empire_privilege: wrong empire priv type({:d})", type);
			return 0;
		}

		if (ch)
			metin2::log::info( "_give_empire_privileage(empire={:d}, type={:d}, value={:d}, time={:d}), by quest, {}", 
					empire, type, value, time, ch->name());
		else
			metin2::log::info( "_give_empire_privileage(empire={:d}, type={:d}, value={:d}, time={:d}), by quest, NULL",
					empire, type, value, time);

		priv_manager::instance().request_give_empire_priv(empire, type, value, time);
		return 0;
	}

	auto give_guild_privilege(lua_State* lua) -> int
	{
		int guild_id = (int)lua_tonumber(lua,1);
		int type = (int)lua_tonumber(lua, 2);
		int value = (int)lua_tonumber(lua, 3);
		int time = (int)lua_tonumber( lua, 4 );

		if (MAX_PRIV_NUM <= type)
		{
			quest::error("PRIV_MANAGER: _give_guild_privilege: wrong empire priv type({:d})", type);
			return 0;
		}

		metin2::log::info( "_give_guild_privileage(empire={:d}, type={:d}, value={:d}, time={:d})", 
				guild_id, type, value, time);

		priv_manager::instance().request_give_guild_priv(guild_id,type,value,time);

		return 0;
	}

	auto get_empire_privilege_string(lua_State* lua) -> int
	{
		int empire = (int) lua_tonumber(lua, 1);
		std::ostringstream os;
		bool found = false;

		for (int type = PRIV_NONE + 1; type < MAX_PRIV_NUM; ++type)
		{
			priv_manager::priv_empire_data* pkPrivEmpireData = priv_manager::instance().priv_by_empire_ex(empire, type);

			if (pkPrivEmpireData && pkPrivEmpireData->value)
			{
				if (found)
					os << ", ";

				os << lc_text(apsz_priv_names[type]) << " : " << 
					pkPrivEmpireData->value << "%" << " (" <<
					((pkPrivEmpireData->end_time_sec-get_global_time())/3600.0f) << " hours)" << std::endl;
				found = true;
			}
		}

		if (!found)
			os << "None!" << std::endl;

		lua_pushstring(lua, os.str().c_str());
		return 1;
	}

	auto get_empire_privilege(lua_State* lua) -> int
	{
		if (!lua_isnumber(lua,1) || !lua_isnumber(lua,2))
		{
			lua_pushnumber(lua,0);
			return 1;
		}
		int empire = (int)lua_tonumber(lua,1);
		int type = (int)lua_tonumber(lua,2);
		int value = priv_manager::instance().priv_by_empire(empire,type);
		lua_pushnumber(lua, value);
		return 1;
	}

	auto get_guild_privilege_string(lua_State* lua) -> int
	{
		int guild = (int) lua_tonumber(lua,1);
		std::ostringstream os;
		bool found = false;

		for (int type = PRIV_NONE+1; type < MAX_PRIV_NUM; ++type)
		{
			const priv_manager::priv_guild_data* pPrivGuildData = priv_manager::instance().priv_by_guild_ex( guild, type );

			if (pPrivGuildData && pPrivGuildData->value)
			{
				if (found)
					os << ", ";

				os << lc_text(apsz_priv_names[type]) << " : " << pPrivGuildData->value << "%"
					<< " (" << ((pPrivGuildData->end_time_sec - get_global_time()) / 3600.0f) << " hours)" << std::endl;
				found = true;
			}
		}

		if (!found)
			os << "None!" << std::endl;

		lua_pushstring(lua, os.str().c_str());
		return 1;
	}

	auto get_guildid_byname( lua_State* lua ) -> int
	{
		if ( !lua_isstring( lua, 1 ) ) {
			quest::error( "_get_guildid_byname() - invalud argument" );
			lua_pushnumber( lua, 0 );
			return 1;
		}

		const char* pszGuildName = lua_tostring( lua, 1 );
		guild* pFindGuild = guild_manager::instance().find_guild_by_name( pszGuildName );
		if ( pFindGuild )
			lua_pushnumber( lua, pFindGuild->id() );
		else
			lua_pushnumber( lua, 0 );

		return 1;
	}

	auto get_guild_privilege(lua_State* lua) -> int
	{
		if (!lua_isnumber(lua,1) || !lua_isnumber(lua,2))
		{
			lua_pushnumber(lua,0);
			return 1;
		}
		int guild = (int)lua_tonumber(lua,1);
		int type = (int)lua_tonumber(lua,2);
		int value = priv_manager::instance().priv_by_guild(guild,type);
		lua_pushnumber(lua, value);
		return 1;
	}

	auto item_name(lua_State* lua) -> int
	{
		if (lua_isnumber(lua,1))
		{
			uint32_t dwVnum = (uint32_t)lua_tonumber(lua,1);
			item_table* pTable = item_manager::instance().table(dwVnum);
			if (pTable)
				lua_pushstring(lua,pTable->locale_name);
			else
				lua_pushstring(lua,"");
		}
		else
			lua_pushstring(lua,"");
		return 1;
	}

	auto mob_name(lua_State* lua) -> int
	{
		if (lua_isnumber(lua, 1))
		{
			uint32_t dwVnum = (uint32_t) lua_tonumber(lua,1);
			const mob * pkMob = mob_manager::instance().get(dwVnum);

			if (pkMob)
				lua_pushstring(lua, pkMob->table.locale_name);
			else
				lua_pushstring(lua, "");
		}
		else
			lua_pushstring(lua,"");

		return 1;
	}

	auto mob_vnum(lua_State* lua) -> int
	{
		if (lua_isstring(lua,1))
		{
			const char* str = lua_tostring(lua, 1);
			const mob* pkMob = mob_manager::instance().get(str, false);
			if (pkMob)
				lua_pushnumber(lua,pkMob->table.vnum);
			else
				lua_pushnumber(lua,0);
		}
		else
			lua_pushnumber(lua,0);

		return 1;
	}

	auto global_time(lua_State* lua) -> int
	{
		lua_pushnumber(lua, get_global_time());
		return 1;
	}
	

	auto get_channel_id(lua_State* lua) -> int
	{
		lua_pushnumber(lua, metin2::config::server().channel);

		return 1;
	}

	auto do_command(lua_State* lua) -> int
	{
		if (!lua_isstring(lua, 1))
			return 0;

		const char * str = lua_tostring(lua, 1);
		size_t len = strlen(str);

		character * ch = quest_manager::instance().current_character_ptr();
		metin2::game::interpret_command(ch, str, len);
		return 0;
	}

	auto find_pc(lua_State* lua) -> int
	{
		if (!lua_isstring(lua, 1))
		{
			quest::error("invalid argument");
			lua_pushnumber(lua, 0);
			return 1;
		}

		const char * name = lua_tostring(lua, 1);
		character * tch = character_manager::instance().find_pc(name);
		lua_pushnumber(lua, tch ? tch->get_vid() : 0);
		return 1;
	}

	auto find_pc_cond(lua_State* lua) -> int
	{
		if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2) || !lua_isnumber(lua, 3))
		{
			quest::error("invalid argument");
			lua_pushnumber(lua, 0);
			return 1;
		}

		int iMinLev = (int) lua_tonumber(lua, 1);
		int iMaxLev = (int) lua_tonumber(lua, 2);
		unsigned int uiJobFlag = (unsigned int) lua_tonumber(lua, 3);

		character * ch = quest_manager::instance().current_character_ptr();
		character * tch;

		if (metin2::config::test_server())
		{
			metin2::log::info( "find_pc_cond map={:d}, job={:d}, level={:d}~{:d}",
					ch->map_index(),
					uiJobFlag,
					iMinLev, iMaxLev);
		}

		tch = character_manager::instance().find_specify_pc(uiJobFlag,
				ch->map_index(),
				ch,
				iMinLev,
				iMaxLev);

		lua_pushnumber(lua, tch ? tch->get_vid() : 0);
		return 1;
	}

	auto find_npc_by_vnum(lua_State* lua) -> int
	{
		if (!lua_isnumber(lua, 1))
		{
			quest::error("invalid argument");
			lua_pushnumber(lua, 0);
			return 1;
		}

		uint32_t race = (uint32_t) lua_tonumber(lua, 1);

		character_vector_interactor i;

		if (character_manager::instance().characters_by_race_num(race, i))
		{
			character_vector_interactor::iterator it = i.begin();

			while (it != i.end())
			{
				character * tch = *(it++);

				if (tch->map_index() == quest_manager::instance().current_character_ptr()->map_index())
				{
					lua_pushnumber(lua, tch->get_vid());
					return 1;
				}
			}
		}

		lua_pushnumber(lua, 0);
		return 1;
	}

	auto set_quest_state(lua_State* lua) -> int
	{
		if (!lua_isstring(lua, 1) || !lua_isstring(lua, 2))
			return 0;

		quest_manager& q = quest_manager::instance();
		quest_state * pqs = q.current_state();
		pc* pPC = q.current_pc();
		if (lua!=pqs->co) 
		{
			luaL_error(lua, "running thread != current thread???");
			metin2::log::info("running thread != current thread???");
			return -1;
		}
		if (pPC)
		{
			const std::string stQuestName(lua_tostring(lua, 1));
			const std::string stStateName(lua_tostring(lua, 2));
			if ( metin2::config::test_server() )
				metin2::log::info("set_state {} {} ", stQuestName.c_str(), stStateName.c_str() );
			if (pPC->current_quest_name() == stQuestName)
			{
				pqs->st = q.quest_state_index(pPC->current_quest_name(), lua_tostring(lua, -1));
				pPC->set_current_quest_state_name(lua_tostring(lua,-1));
			}
			else
			{
				pPC->set_quest_state(stQuestName, stStateName);
			}
		}
		return 0;
	}

	auto get_quest_state(lua_State* lua) -> int
	{
		if (!lua_isstring(lua, 1) )
			return 0;

		quest_manager& q = quest_manager::instance();
		pc* pPC = q.current_pc();
		
		if (pPC)
		{
			std::string stQuestName	= lua_tostring(lua, 1);
			stQuestName += ".__status";

			int nRet = pPC->flag( stQuestName.c_str() ); 

			lua_pushnumber(lua, nRet );

			if ( metin2::config::test_server() )
				metin2::log::info("Get_quest_state name {} value {:d}", stQuestName.c_str(), nRet );
		}
		else
		{
			if ( metin2::config::test_server() )
				metin2::log::info("PC == 0 ");

			lua_pushnumber(lua, 0);
		}
		return 1;
	}

	auto notice_all( lua_State* lua ) -> int
	{
		std::ostringstream s;
		combine_lua_string(lua, s);

		packet_gg_notice p;
		p.header = HEADER_GG_NOTICE;
		p.size = strlen(s.str().c_str()) + 1;

		temp_buffer buf;
		buf.write(&p, sizeof(p));
		buf.write(s.str().c_str(), p.size);

		p2_p_manager::instance().send(buf.read_peek(), buf.size());

		send_notice(s.str().c_str());
		return 1;	
	}

	EVENTINFO(warp_all_to_village_event_info)
	{
		uint32_t warp_map_index;

		warp_all_to_village_event_info() 
		: warp_map_index( 0 )
		{
		}
	};

	struct f_warp_all_to_village
	{
		f_warp_all_to_village() {};
		void operator()(entity * ent)
		{
			if (ent->is_type(ENTITY_CHARACTER))
			{
				character * ch = (character *) ent;
				if (ch->is_pc())
				{
					uint8_t bEmpire =  ch->empire();
					if ( bEmpire == 0 )
					{
						quest::error( "Unkonwn Empire {} {:d} ", ch->name(), ch->player_id() );
						return;
					}

					ch->warp_set( start_position[bEmpire][0], start_position[bEmpire][1] );
				}
			}
		}
	};

	EVENTFUNC(warp_all_to_village_event)
	{
		warp_all_to_village_event_info * info = dynamic_cast<warp_all_to_village_event_info *>(event->info);

		if ( info == nullptr )
		{
			quest::error( "warp_all_to_village_event> <Factor> Null pointer" );
			return 0;
		}

		sectree_map * pSecMap = sectree_manager::instance().map( info->warp_map_index );

		if (nullptr != pSecMap)
		{
			f_warp_all_to_village f;
			pSecMap->for_each( f );
		}

		return 0;
	}

	auto warp_all_to_village( lua_State * lua ) -> int
	{
		int iMapIndex 	= static_cast<int>(lua_tonumber(lua, 1));
		int iSec		= static_cast<int>(lua_tonumber(lua, 2));
		
		warp_all_to_village_event_info* info = alloc_event_info<warp_all_to_village_event_info>();

		info->warp_map_index = iMapIndex;

		event_create(warp_all_to_village_event, info, passes_per_sec(iSec));

		send_notice_map(lc_text("잠시후 모두 마을로 이동됩니다."), iMapIndex, false);

		return 0;
	}

	auto warp_to_village( lua_State * lua ) -> int
	{
		character * ch = quest_manager::instance().current_character_ptr();
	
		if (nullptr != ch)
		{
			uint8_t bEmpire = ch->empire();	
			ch->warp_set( start_position[bEmpire][0], start_position[bEmpire][1] );
		}

		return 0;
	}

	auto say_in_map( lua_State * lua ) -> int
	{
		int iMapIndex 		= static_cast<int>(lua_tonumber( lua, 1 ));
		std::string Script(lua_tostring( lua, 2 ));

		Script += "[ENTER]";
		Script += "[DONE]";
		
		struct metin2::game::packet_script packet_script;

		packet_script.header = HEADER_GC_SCRIPT;
		packet_script.skin = quest_manager::QUEST_SKIN_NORMAL;
		packet_script.src_size = Script.size();
		packet_script.size = packet_script.src_size + sizeof(struct packet_script);

		f_send_packet f;
		f.buf.write(&packet_script, sizeof(struct packet_script));
		f.buf.write(&Script[0], Script.size());

		sectree_map * pSecMap = sectree_manager::instance().map( iMapIndex );

		if ( pSecMap )
		{
			pSecMap->for_each( f );
		}

		return 0;
	}

	struct f_kill_sectree2
	{
		void operator () (entity * ent)
		{
			if (ent->is_type(ENTITY_CHARACTER))
			{
				character * ch = (character *) ent;

				if (!ch->is_pc() && !ch->is_pet())
					ch->dead();
			}
		}
	};

	auto kill_all_in_map ( lua_State * lua ) -> int
	{
		sectree_map * pSecMap = sectree_manager::instance().map( lua_tonumber(lua,1) );

		if (nullptr != pSecMap)
		{
			f_kill_sectree2 f;
			pSecMap->for_each( f );
		}

		return 0;
	}

	auto regen_in_map( lua_State * lua ) -> int
	{
		int iMapIndex = static_cast<int>(lua_tonumber(lua, 1));
		std::string szFilename(lua_tostring(lua, 2));

		sectree_map * pkMap = sectree_manager::instance().map(iMapIndex);

		if (pkMap != nullptr)
		{
			regen_load_in_file( szFilename.c_str(), iMapIndex, pkMap->setting.base_x ,pkMap->setting.base_y );
		}

		return 0;
	}

	auto enable_over9refine( lua_State* lua ) -> int
	{
		if ( lua_isnumber(lua, 1) == true && lua_isnumber(lua, 2) == true )
		{
			uint32_t dwVnumFrom = (uint32_t)lua_tonumber(lua, 1);
			uint32_t dwVnumTo = (uint32_t)lua_tonumber(lua, 2);

			over9_refine_manager::instance().enable_over9_refine(dwVnumFrom, dwVnumTo);
		}

		return 0;
	}

	auto add_ox_quiz(lua_State* lua) -> int
	{
		int level = (int)lua_tonumber(lua, 1);
		const char* quiz = lua_tostring(lua, 2);
		bool answer = lua_toboolean(lua, 3);

		if ( ox_event_manager::instance().add_quiz(level, quiz, answer) == false )
		{
			metin2::log::info( "OXEVENT : Cannot add quiz. {:d} {} {:d}", level, quiz, answer);
		}

		return 1;
	}

	EVENTFUNC(warp_all_to_map_my_empire_event)
	{
		warp_all_to_map_my_empire_event_info * info = dynamic_cast<warp_all_to_map_my_empire_event_info *>(event->info);

		if ( info == nullptr )
		{
			quest::error( "warp_all_to_map_my_empire_event> <Factor> Null pointer" );
			return 0;
		}

		sectree_map * pSecMap = sectree_manager::instance().map( info->map_index_from );

		if (pSecMap)
		{
			f_warp_empire f;

			f.map_index_to = info->map_index_to;
			f.x			= info->x;
			f.y			= info->y;
			f.empire		= info->empire;

			pSecMap->for_each(f);
		}

		return 0;
	}

	auto block_chat(lua_State* lua) -> int
	{
		character * pChar = quest_manager::instance().current_character_ptr();

		if (pChar != nullptr)
		{
			if (lua_isstring(lua, 1) != true && lua_isstring(lua, 2) != true)
			{
				lua_pushboolean(lua, false);
				return 1;
			}

			std::string strName(lua_tostring(lua, 1));
			std::string strTime(lua_tostring(lua, 2));

			std::string strArg = strName + " " + strTime;

			do_block_chat(pChar, const_cast<char*>(strArg.c_str()), 0, 0);

			lua_pushboolean(lua, true);
			return 1;
		}

		lua_pushboolean(lua, false);
		return 1;
	}

	auto spawn_mob_at(lua_State* lua) -> int
	{
		if( false == lua_isnumber(lua, 1) || false == lua_isnumber(lua, 2) || false == lua_isboolean(lua, 3) )
		{
			lua_pushnumber(lua, 0);
			return 1;
		}

		const uint32_t dwVnum = static_cast<uint32_t>(lua_tonumber(lua, 1));
		const size_t count = std::clamp<int>(static_cast<size_t>(lua_tonumber(lua, 2)), 1, 10);
		const bool isAggresive = static_cast<bool>(lua_toboolean(lua, 3));
		size_t SpawnCount = 0;

		const mob* pMonster = mob_manager::instance().get( dwVnum );

		if( nullptr != pMonster )
		{
			character * const pChar = quest_manager::instance().current_character_ptr();

			for( size_t i=0 ; i < count ; ++i )
			{
				character * const pSpawnMonster = character_manager::instance().spawn_mob_range( dwVnum,
						pChar->map_index(),
						pChar->x() - number(200, 750),
						pChar->y() - number(200, 750),
						pChar->x() + number(200, 750),
						pChar->y() + number(200, 750),
						true,
						pMonster->table.type == CHAR_TYPE_STONE,
						isAggresive );

				if( nullptr != pSpawnMonster )
				{
					++SpawnCount;
				}
			}

			metin2::log::info( "QUEST Spawn Monstster: VNUM({:d}) COUNT({:d}) isAggresive({})", dwVnum, SpawnCount, isAggresive);
		}

		lua_pushnumber(lua, SpawnCount);

		return 1;
	}

	auto notice_in_map( lua_State* lua ) -> int
	{
		character * const pChar = quest_manager::instance().current_character_ptr();

		if (nullptr != pChar)
		{
			send_notice_map( lua_tostring(lua,1), pChar->map_index(), lua_toboolean(lua,2) );
		}

		return 0;
	}

	auto get_locale_base_path( lua_State* lua ) -> int
	{
		lua_pushstring( lua, locale_service_get_base_path().c_str() );

		return 1;
	}

	struct f_purge_area
	{
		int x1, y1, x2, y2;
		character * except_char;

		f_purge_area(int a, int b, int c, int d, character * p)
			: x1(a), y1(b), x2(c), y2(d),
			except_char(p)
		{}

		void operator () (entity * ent)
		{
			if (true == ent->is_type(ENTITY_CHARACTER))
			{
				character * pChar = static_cast<character *>(ent);

				if (pChar == except_char)
					return;
					
				if (!pChar->is_pet() && (true == pChar->is_monster() || true == pChar->is_stone()))
				{
					if (x1 <= pChar->x() && pChar->x() <= x2 && y1 <= pChar->y() && pChar->y() <= y2)
					{
						character_manager::instance().destroy_character(pChar);
					}
				}
			}
		}
	};

	auto purge_area( lua_State* lua ) -> int
	{
		int x1 = lua_tonumber(lua, 1);
		int y1 = lua_tonumber(lua, 2);
		int x2 = lua_tonumber(lua, 3);
		int y2 = lua_tonumber(lua, 4);

		const int mapIndex = sectree_manager::instance().map_index( x1, y1 );

		if (0 == mapIndex)
		{
			quest::error("_purge_area: cannot get a map index with ({:d}, {:d})", x1, y1);
			return 0;
		}

		sectree_map * pSectree = sectree_manager::instance().map(mapIndex);

		if (nullptr != pSectree)
		{
			f_purge_area func(x1, y1, x2, y2, quest_manager::instance().current_npc_character_ptr());

			pSectree->for_each(func);
		}

		return 0;
	}

	struct f_warp_all_in_area_to_area
	{
		int from_x1, from_y1, from_x2, from_y2;
		int to_x1, to_y1, to_x2, to_y2;
		size_t warp_count;

		f_warp_all_in_area_to_area(int a, int b, int c, int d, int e, int f, int g, int h)
			: from_x1(a), from_y1(b), from_x2(c), from_y2(d),
			to_x1(e), to_y1(f), to_x2(g), to_y2(h),
			warp_count(0)
		{}

		void operator () (entity * ent)
		{
			if (true == ent->is_type(ENTITY_CHARACTER))
			{
				character * pChar = static_cast<character *>(ent);

				if (true == pChar->is_pc())
				{
					if (from_x1 <= pChar->x() && pChar->x() <= from_x2 && from_y1 <= pChar->y() && pChar->y() <= from_y2)
					{
						++warp_count;

						pChar->warp_set( number(to_x1, to_x2), number(to_y1, to_y2) );
					}
				}
			}
		}
	};

	auto warp_all_in_area_to_area( lua_State* lua ) -> int
	{
		int from_x1 = lua_tonumber(lua, 1);
		int from_y1 = lua_tonumber(lua, 2);
		int from_x2 = lua_tonumber(lua, 3);
		int from_y2 = lua_tonumber(lua, 4);

		int to_x1 = lua_tonumber(lua, 5);
		int to_y1 = lua_tonumber(lua, 6);
		int to_x2 = lua_tonumber(lua, 7);
		int to_y2 = lua_tonumber(lua, 8);

		const int mapIndex = sectree_manager::instance().map_index( from_x1, from_y1 );

		if (0 == mapIndex)
		{
			quest::error("_warp_all_in_area_to_area: cannot get a map index with ({:d}, {:d})", from_x1, from_y1);
			lua_pushnumber(lua, 0);
			return 1;
		}

		sectree_map * pSectree = sectree_manager::instance().map(mapIndex);

		if (nullptr != pSectree)
		{
			f_warp_all_in_area_to_area func(from_x1, from_y1, from_x2, from_y2, to_x1, to_y1, to_x2, to_y2);

			pSectree->for_each(func);

			lua_pushnumber(lua, func.warp_count);
			metin2::log::info( "_warp_all_in_area_to_area: {:d} character warp", func.warp_count);
			return 1;
		}
		else
		{
			lua_pushnumber(lua, 0);
			quest::error("_warp_all_in_area_to_area: no sectree");
			return 1;
		}
	}

	void register_global_function_table(lua_State* lua)
	{

		luaL_reg global_functions[] =
		{
			{	"sys_err",					syserr					},
			{	"sys_log",					syslog					},
			{	"char_log",					char_log				},
			{	"item_log",					item_log				},
			{	"set_state",				quest_setstate			},
			{	"set_skin",					set_skin				},
			{	"setskin",					set_skin				},
			{	"time_to_str",				time_to_str			},
			{	"say",						say					},
			{	"chat",						chat					},
			{	"cmdchat",					cmdchat				},
			{	"syschat",					syschat				},
			{	"get_locale",				get_locale				},
			{	"setleftimage",				left_image				},
			{	"settopimage",				top_image				},
			{	"server_timer",				set_server_timer		},
			{	"clear_server_timer",		clear_server_timer		},
			{	"server_loop_timer",		set_server_loop_timer	},
			{	"get_server_timer_arg",		get_server_timer_arg	},
			{	"timer",					timer					},
			{	"loop_timer",				set_named_loop_timer	},
			{	"cleartimer",				clear_named_timer		},
			{	"getnpcid",					getnpcid				},
			{	"is_test_server",			is_test_server			},
			{	"is_speed_server",			is_speed_server		},
			{	"raw_script",				raw_script				},
			{	"number",					random_number	   				},

			{	"set_bgm_volume_enable",	set_bgm_volume_enable	},
			{	"add_bgm_info",				add_bgm_info			},

			{	"add_goto_info",			add_goto_info			},

			{	"__refine_pick",			refine_pick			},

			{	"add_ox_quiz",					add_ox_quiz					},
			{	"__fish_real_refine_rod",		fish_real_refine_rod			},
			{	"__give_char_priv",				give_char_privilege			},
			{	"__give_empire_priv",			give_empire_privilege			},
			{	"__give_guild_priv",			give_guild_privilege			},
			{	"__get_empire_priv_string",		get_empire_privilege_string	},
			{	"__get_empire_priv",			get_empire_privilege			},
			{	"__get_guild_priv_string",		get_guild_privilege_string		},
			{	"__get_guildid_byname",			get_guildid_byname				},
			{	"__get_guild_priv",				get_guild_privilege			},
			{	"item_name",					item_name						},
			{	"mob_name",						mob_name						},
			{	"mob_vnum",						mob_vnum						},
			{	"get_time",						global_time				},
			{	"get_global_time",				global_time				},
			{	"get_channel_id",				get_channel_id					},
			{	"command",						do_command						},
			{	"find_pc_cond",					find_pc_cond					},
			{	"find_pc_by_name",				find_pc						},
			{	"find_npc_by_vnum",				find_npc_by_vnum				},
			{	"set_quest_state",				set_quest_state				},
			{	"get_quest_state",				get_quest_state				},
			{	"notice",						notice							},
			{	"notice_all",					notice_all						},
			{	"notice_in_map",				notice_in_map					},
			{	"warp_all_to_village",			warp_all_to_village			},
			{	"warp_to_village",				warp_to_village				},	
			{	"say_in_map",					say_in_map						},	
			{	"kill_all_in_map",				kill_all_in_map				},
			{	"regen_in_map",					regen_in_map					},
			{	"enable_over9refine",			enable_over9refine				},
			{	"block_chat",					block_chat						},
			{	"spawn_mob",					spawn_mob_at						},
			{	"get_locale_base_path",			get_locale_base_path			},
			{	"purge_area",					purge_area						},
			{	"warp_all_in_area_to_area",		warp_all_in_area_to_area		},

			{	nullptr,	nullptr	}
		};
	
		int i = 0;

		while (global_functions[i].name != nullptr)
		{
			lua_register(lua, global_functions[i].name, global_functions[i].func);
			++i;
		}
	}
}
}
