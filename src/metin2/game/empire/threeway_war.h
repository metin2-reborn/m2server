
#pragma once

#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>

#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace metin2::game::inline empire {

struct forked_sungzi_map_info
{
	int forked_sung;
	int forked_sungzi_start_position[3][2];
	std::string map_name;
	int boss_mob_vnum;
};

struct forked_pass_map_info
{
	int forked_pass[3];
	int forked_pass_start_position[3][2];
	std::string map_name[3];
};

class three_way_war : public singleton<three_way_war>
{
	public:
		three_way_war ();
		~three_way_war () override;

		void initialize ();
		auto load_setting (const char* file_name) -> bool;

		auto kill_score (uint8_t empire) const -> int;
		void set_kill_score (uint8_t empire, int count);

		void set_revive_token_for_player (uint32_t player_id, int count);
		auto revive_token_for_player (uint32_t player_id) -> int;
		void decrease_revive_token_for_player (uint32_t player_id);

		auto event_pass_map_info () const -> const forked_pass_map_info&;
		auto event_sung_zi_map_info () const -> const forked_sungzi_map_info&;

		auto is_three_way_war_map_index (int map_index) const -> bool;
		auto is_sung_zi_map_index (int map_index) const -> bool;

		void random_event_map_set ();

		void register_user (uint32_t player_id);
		auto is_registered_user (uint32_t player_id) const -> bool;

		void on_dead (character * ch, character * killer);

		void set_regen_flag (int flag) { regen_flag_ = flag; }
		auto regen_flag () const -> int { return regen_flag_; }

		void remove_all_monsters_in_three_way () const;

	private:
		int kill_score_[3];
		int regen_flag_;

		std::set<int>	map_index_set_;
		std::vector<forked_pass_map_info>	pass_info_map_;
		std::vector<forked_sungzi_map_info>	sung_zi_info_map_;

		std::unordered_map<uint32_t, uint32_t>	register_user_map_;
		std::unordered_map<uint32_t, int>	revive_token_map_;
};

auto get_sungzi_map_path() -> const char*;
auto get_pass_map_path( uint8_t empire ) -> const char*;
auto get_pass_map_index( uint8_t empire ) -> int;
auto get_sungzi_map_index() -> int;

auto get_sungzi_start_x( uint8_t empire ) -> int;
auto get_sungzi_start_y( uint8_t empire ) -> int;
}
