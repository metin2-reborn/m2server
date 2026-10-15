#pragma once

#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/app/event.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/app/typedef.h>

#include <cstdint>
#include <sstream>
#include <string>

namespace metin2::game {

namespace quest
{
	int quest_setstate(lua_State* L);
	extern void register_pc_function_table();
	extern void register_npc_function_table();
	extern void register_target_function_table();
	extern void register_affect_function_table();
	extern void register_building_function_table();
	extern void register_marriage_function_table();
	extern void register_item_function_table();
	extern void register_dungeon_function_table();
	extern void register_quest_function_table();
	extern void register_party_function_table();
	extern void register_horse_function_table();
	extern void register_pet_function_table();
	extern void register_guild_function_table();
	extern void register_game_function_table();
	extern void register_arena_function_table();
	extern void register_global_function_table(lua_State* L);
	extern void register_forked_function_table();
	extern void register_monarch_function_table(); 
	extern void register_ox_event_function_table();
	extern void register_mgmt_function_table();
	extern void register_battle_arena_function_table();
	extern void register_dance_event_function_table();
	extern void register_dragon_lair_function_table();
	extern void register_speed_server_function_table();
	extern void register_dragon_soul_function_table();

	extern void combine_lua_string(lua_State* L, std::ostringstream &s);
	
	struct f_set_warp_location
	{
		int32_t map_index;
		int32_t x;
		int32_t y;

		f_set_warp_location (int32_t _map_index, int32_t _x, int32_t _y) :
			map_index (_map_index), x (_x), y (_y)
		{}
		void operator () (character * ch);
	};

	struct f_set_quest_flag
	{
		std::string flagname;
		int value;

		void operator () (character * ch);
	};

	struct f_party_check_flag_lt
	{
		std::string flagname;
		int value;

		bool operator () (character * ch);
	};

	struct f_party_chat
	{
		int chat_type;
		const char* str;

		f_party_chat(int ChatType, const char* str);
		void operator() (character * ch);
	};

	struct f_party_clear_ready
	{
		void operator() (character * ch);
	};

	struct f_send_packet
	{
		temp_buffer buf;

		void operator() (entity * ent);
	};

	struct f_send_packet_to_empire
	{
		temp_buffer buf;
		uint8_t empire;

		void operator() (entity * ent);
	};

	struct f_warp_empire
	{
		uint8_t empire;
		int32_t map_index_to;
		int32_t x;
		int32_t y;

		void operator() (entity * ent);
	};

	EVENTINFO(warp_all_to_map_my_empire_event_info)
	{
		uint8_t 	empire;
		int32_t	map_index_from;
		int32_t 	map_index_to;
		int32_t 	x;
		int32_t	y;

		warp_all_to_map_my_empire_event_info() 
		: empire( 0 )
		, map_index_from( 0 )
		, map_index_to( 0 )
		, x( 0 )
		, y( 0 )
		{
		}
	};

	EVENTFUNC(warp_all_to_map_my_empire_event);

	struct f_build_lua_guild_war_list
	{
		lua_State * l;
		int count;

		f_build_lua_guild_war_list(lua_State * L);
		void operator() (uint32_t g1, uint32_t g2);
	};
}
}
