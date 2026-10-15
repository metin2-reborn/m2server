#pragma once

#include <metin2/game/app/constants.h>
#include <metin2/game/app/event.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>

#include <cstdint>
#include <map>
#include <memory>
#include <set>

namespace metin2::game::inline guilds {

enum war_map_types
{
	WAR_MAP_TYPE_NORMAL,
	WAR_MAP_TYPE_FLAG,
};

struct war_map_info {
	uint8_t		type;
	int32_t		map_index;
	pixel_position	pos_start[3];
};

namespace warmap
{
	enum
	{
		WAR_FLAG_VNUM_START = 20035,
		WAR_FLAG_VNUM_END = 20037,

		WAR_FLAG_VNUM0 = 20035,
		WAR_FLAG_VNUM1 = 20036,

		WAR_FLAG_BASE_VNUM = 20038
	};

	inline bool is_war_flag(uint32_t vnum)
	{
		if (vnum >= WAR_FLAG_VNUM_START && vnum <= WAR_FLAG_VNUM_END)
			return true;

		return false;
	}

	inline bool is_war_flag_base(uint32_t vnum)
	{
		return vnum == WAR_FLAG_BASE_VNUM ? true : false;
	}
};

class war_map
{
	public:
		friend class guild;

		war_map(int32_t map_index_value, const guild_war_info & r_info, war_map_info * war_map_info_value, uint32_t guild_id1, uint32_t guild_id2);
		~war_map();

		bool	team_index(uint32_t guild_id, uint8_t & idx);

		void	inc_member(character * ch);
		void	dec_member(character * ch);

		guild * get_guild(uint8_t idx);

		uint8_t	type();
		int32_t	map_index();
		uint32_t	guild_opponent(character * ch);

		uint32_t	winner_guild();
		void	use_potion(character * ch, item * item);

		void	draw();
		void	timeout();
		void	check_war_end();
		bool	set_ended();
		void	exit_all();

		void	set_begin_event(event_ptr ev);
		void	set_timeout_event(event_ptr ev);
		void	set_end_event(event_ptr ev);
		void	set_reset_flag_event(event_ptr ev);

		void	update_score(uint32_t g1, int score1, uint32_t g2, int score2);
		bool	check_score();

		int	reward_gold(uint8_t winner_idx);

		void	packet(const void * p, int size);
		void	notice(const char * psz);
		void	send_war_packet(desc * d);
		void	send_score_packet(uint8_t idx, desc * d = nullptr);

		void	on_kill(character * killer, character * ch);

		void	add_flag(uint8_t idx, uint32_t x=0, uint32_t y=0);
		void	add_flag_base(uint8_t idx, uint32_t x=0, uint32_t y=0);
		void	remove_flag(uint8_t idx);
		bool	is_flag_on_base(uint8_t idx);
		void	reset_flag();

	private:
		void	update_user_count();

	private:
		war_map_info	map_info_;
		bool		ended_;

		event_ptr begin_event_;
		event_ptr timeout_event_;
		event_ptr end_event_;
		event_ptr	reset_flag_event_;

		struct team_data {
			uint32_t	id;
			game::guilds::guild * guild;
			int		member_count;
			int		use_potion_price;
			int		score;
			character * chr_flag;
			character * chr_flag_base;

			std::set<uint32_t> set_pid_joiner;

			void initialize();

			int accumulated_joiner_count();
			int cur_jointer_count();

			void append_member(character * ch);
			void remove_member(character * ch);
		};

		team_data	team_data_[2];
		int		observer_count_;
		uint32_t		start_time_;
		uint8_t		timeout_;

		guild_war_info	war_info_;

		character_set set_pk_chr_;
};

class war_map_manager : public singleton<war_map_manager>
{
	public:
		war_map_manager();
		~war_map_manager() override;

		bool		load_war_map_info(const char * file_name);
		bool		is_war_map(int32_t map_index);
		war_map_info *	get_war_map_info(int32_t map_index);
		bool		start_position(int32_t map_index, uint8_t idx, pixel_position & pos);

		template <typename Func> Func for_each(Func f);
		int32_t		create_war_map(const guild_war_info & guild_war_info_value, uint32_t guild_id1, uint32_t guild_id2);
		void		destroy_war_map(war_map* map);
		war_map *	find(int32_t map_index);

		void		on_shutdown();

	private:
		std::map<int32_t, std::unique_ptr<war_map_info>> map_k_war_map_info_;
		std::map<int32_t, war_map *> war_map_;
};

template <typename Func> Func war_map_manager::for_each(Func f)
{
	for (auto it = war_map_.begin(); it != war_map_.end(); ++it)
		f(it->second);

	return f;
}
}
