#pragma once

#include <cstdint>
#include <string>

namespace metin2::game::inline events {

namespace xmas
{
	enum
	{
		MOB_SANTA_VNUM = 20031,
		MOB_XMAS_TREE_VNUM = 20032,
		MOB_XMAS_FIRWORK_SELLER_VNUM = 9004,
	};

	void process_event_flag(const std::string& name, int prev_value, int value);
	void spawn_santa(int32_t map_index_value, int time_gap_sec);
	void spawn_event_helper(bool spawn);
}
}
