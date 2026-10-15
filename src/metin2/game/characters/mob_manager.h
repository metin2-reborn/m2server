#pragma once
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/common/singleton.h>
#include <metin2/proto/tables.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <map>
#include <string>
#include <vector>

namespace metin2::game::inline characters {

struct mob_splash_attack_info {
	uint32_t	timing; // Time to wait after using a skill until damage actually lands (ms)
	uint32_t	hit_distance; // Distance at which the skill is actually computed on use (cm forward)

	mob_splash_attack_info(uint32_t timing_value, uint32_t hit_distance_value)
		: timing(timing_value)
		, hit_distance(hit_distance_value)
		{}
};

struct mob_skill_info {
	uint32_t	skill_vnum;
	uint8_t	skill_level;
	std::vector<mob_splash_attack_info> splash_attack;
};

class mob
{
	public:
		mob();

		~mob();

		mob_table	table;
		game::characters::mob_skill_info	mob_skill_info[MOB_SKILL_MAX_NUM];

		void add_skill_splash(int index, uint32_t timing, uint32_t hit_distance);
};

class mob_instance
{
	public:
		mob_instance();

		pixel_position	pos_last_attacked;	// Last hit position
		uint32_t		last_attacked_time;	// Last hit time
		uint32_t		last_warp_time;

		bool is_berserk;
		bool is_god_speed;
		bool is_revive;
};

class mob_group_group
{
	public:
		mob_group_group(uint32_t vnum_value)
		{
			vnum = vnum_value;
		}

		// ADD_MOB_GROUP_GROUP_PROB
		void add_member(uint32_t vnum, int prob = 1)
		{   
			if (prob == 0)
				return;

			if (!vec_i_probs.empty())
				prob += vec_i_probs.back();

			vec_i_probs.push_back(prob);
			vec_dw_member_vnum.push_back(vnum);
		}
		// END_OF_ADD_MOB_GROUP_GROUP_PROB

		uint32_t member()
		{
			if (vec_dw_member_vnum.empty())
				return 0;

			// ADD_MOB_GROUP_GROUP_PROB
			int n = number(1, vec_i_probs.back());
			auto it = lower_bound(vec_i_probs.begin(), vec_i_probs.end(), n);

			return vec_dw_member_vnum[std::distance(vec_i_probs.begin(), it)];
			// END_OF_ADD_MOB_GROUP_GROUP_PROB
		}

		uint32_t                   vnum;
		std::vector<uint32_t>      vec_dw_member_vnum;

		// ADD_MOB_GROUP_GROUP_PROB
		std::vector<int>	vec_i_probs;
		// END_OF_ADD_MOB_GROUP_GROUP_PROB
};

class mob_group
{
	public:
		void create(uint32_t vnum, std::string & r_st_name)
		{
			vnum_ = vnum;
			name_ = r_st_name;
		}

		const std::vector<uint32_t> & member_vector()
		{
			return vec_dw_member_vnum_;
		}

		void add_member(uint32_t vnum)
		{   
			vec_dw_member_vnum_.push_back(vnum);
		}

	protected:                  
		uint32_t                   vnum_;
		std::string             name_;
		std::vector<uint32_t>      vec_dw_member_vnum_;
};

class mob_manager : public singleton<mob_manager>
{
	public:
		typedef std::map<uint32_t, mob *>::iterator iterator;

		mob_manager();
		~mob_manager() override;

		bool		initialize(mob_table * table_value, int size);
		void		destroy();

		bool		load_group(const char * file_name);
		bool		load_group_group(const char * file_name);
		mob_group *	group(uint32_t vnum);
		uint32_t		group_from_group_group(uint32_t vnum);

		const mob *	get(uint32_t vnum);
		const mob *	get(const char * name, bool is_abbrev);

		const iterator	begin()	{ return map_pk_mob_by_vnum_.begin();	}
		const iterator	end()	{ return map_pk_mob_by_vnum_.end();	}

		void rebind_mob_proto(character * ch);

		void			inc_regen_count(uint8_t regen_type, uint32_t vnum, int count, int time);
		void			dump_regen_count(const char* filename);

	private:
		std::map<uint32_t, mob *> map_pk_mob_by_vnum_;
		std::map<std::string, mob *> map_pk_mob_by_name_;
		std::map<uint32_t, mob_group *> map_pk_mob_group_;
		std::map<uint32_t, mob_group_group *> map_pk_mob_group_group_;

		std::map<uint32_t, double> regen_count_;
};
}
