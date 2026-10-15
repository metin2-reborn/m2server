#pragma once

#include <metin2/game/characters/character.h>
#include <metin2/game/app/event.h>

#include <metin2/common/singleton.h>
#include <metin2/log/log.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>

namespace metin2::game::inline world {
    class dungeon;
}

namespace metin2::game::inline parties {

enum
{
	PARTY_ENOUGH_MINUTE_FOR_EXP_BONUS = 60,
	PARTY_HEAL_COOLTIME_LONG = 60,
	PARTY_HEAL_COOLTIME_SHORT = 30,
	PARTY_MAX_MEMBER = 8,
	PARTY_DEFAULT_RANGE = 5000,
};

enum party_role
{   
	PARTY_ROLE_NORMAL,  
	PARTY_ROLE_LEADER,
	PARTY_ROLE_ATTACKER,
	PARTY_ROLE_TANKER,
	PARTY_ROLE_BUFFER,  
	PARTY_ROLE_SKILL_MASTER,
	PARTY_ROLE_HASTE,   
	PARTY_ROLE_DEFENDER,
	PARTY_ROLE_MAX_NUM, 
};  

enum party_exp_distribution_modes
{
	PARTY_EXP_DISTRIBUTION_NON_PARITY,
	PARTY_EXP_DISTRIBUTION_PARITY,
	PARTY_EXP_DISTRIBUTION_MAX_NUM
};

class party;

class party_manager : public singleton<party_manager>
{
	public:
		typedef std::map<uint32_t, party *> party_map;
		typedef std::set<party *> pc_party_set;

	public:
		party_manager();
		~party_manager() override;

		void		initialize();

		void		enable_pc_party() { enable_pc_party_ = true; metin2::log::info("PARTY Enable"); }
		void		disable_pc_party() { enable_pc_party_ = false; metin2::log::info("PARTY Disable"); }
		auto		is_enable_pc_party() -> bool { return enable_pc_party_; }

		auto		create_party(character * leader) -> party *;
		void		delete_party(party * party_value);
		void		delete_all_party();
		auto		set_party(character * ch) -> bool;

		void		set_party_member(uint32_t pid, party * party_value);

		void		p2_p_login(uint32_t pid, const char* name);
		void		p2_p_logout(uint32_t pid);

		auto		p2_p_create_party(uint32_t pid) -> party *;
		void		p2_p_delete_party(uint32_t pid);
		void		p2_p_join_party(uint32_t leader, uint32_t pid, uint8_t role = 0);
		void		p2_p_quit_party(uint32_t pid);

	private:
		party_map	map_pk_party_;

		pc_party_set	set_pk_pc_party_;

		bool		enable_pc_party_;
};

enum party_messages
{
	PM_ATTACK,
	PM_RETURN,
	PM_ATTACKED_BY,
	PM_AGGRO_INCREASE,
};

class party
{
	public:
		struct member {
			game::characters::character *	character;
			bool	near;
			uint8_t	role;
			uint8_t	level;
			std::string name;
		};

		typedef std::map<uint32_t, member> member_map;

		typedef std::map<std::string, int> flag_map;

	public:
		party();
		virtual ~party();

		void		p2_p_join(uint32_t pid);
		void		p2_p_quit(uint32_t pid);
		virtual void	join(uint32_t pid_value);
		void		quit(uint32_t pid_value);
		void		link(character * chr);
		void		unlink(character * chr);

		void		chat_packet_to_all_member(uint8_t type, const char* format, ...);	

		void		update_online_state(uint32_t pid_value, const char* name);
		void		update_offline_state(uint32_t pid_value);

		auto		leader_pid() -> uint32_t;
		auto	leader_character() -> character *;
		auto	leader() -> character * { return chr_leader_; }

		auto		member_count() -> uint32_t;
		auto		near_member_count() -> uint32_t	{ return count_near_party_member_; }

		auto		is_member(uint32_t pid) -> bool { return member_map_.find(pid) != member_map_.end(); }

		auto		is_position_near_leader(character * ch) -> bool;

		void		send_message(character * ch, uint8_t msg, uint32_t arg1, uint32_t arg2);

		void		send_party_join_one_to_all(uint32_t pid);
		void		send_party_join_all_to_one(character * ch);
		void		send_party_remove_one_to_all(uint32_t pid);

		void		send_party_info_one_to_all(uint32_t pid);
		void		send_party_info_one_to_all(character * ch);
		void		send_party_info_all_to_one(character * ch);

		void		send_party_link_one_to_all(character * ch);
		void		send_party_link_all_to_one(character * ch);
		void		send_party_unlink_one_to_all(character * ch);

		auto		party_bonus_exp_percent() -> int	{ return exp_bonus_; }

		auto	compute_party_bonus_exp_percent() -> int;

		template <class Func> void for_each_member(Func & f);
		template <class Func> void for_each_member_ptr(Func & f);
		template <class Func> void for_each_online_member(Func & f);
		template <class Func> void for_each_near_member(Func & f);
		template <class Func> void for_each_on_map_member (Func & f, int32_t map_index_value);
		template <class Func> auto for_each_on_map_member_bool (Func & f, int32_t map_index_value) -> bool;

		void		update();

		auto		exp_bonus_percent() -> int;

		auto		set_role(uint32_t pid, uint8_t role_value, bool set) -> bool;
		auto		role(uint32_t pid) -> uint8_t;

		auto		member_max_level() -> uint8_t;

		void		compute_role_point(character * ch, uint8_t role, bool add);

		void		heal_party();
		void		summon_to_leader(uint32_t pid);

		void		set_pc_party(bool b) { pc_party_ = b; }

		auto	next_ownership(character * ch, int32_t x, int32_t y) -> character *;

		void		set_flag(const std::string& name, int value);
		auto		flag(const std::string& name) -> int;

		void		set_dungeon(dungeon * dungeon_value);
		auto	get_dungeon() -> dungeon *;

		auto		count_member_by_vnum(uint32_t vnum_value) -> uint8_t;

		void		set_parameter(int mode);
		auto		exp_distribution_mode() -> int;

		auto	exp_centralize_character() -> character *;

		void		request_set_member_level(uint32_t pid, uint8_t level);
		void		p2_p_set_member_level(uint32_t pid, uint8_t level);

	protected:
		void		increase_ownership();

		virtual void	initialize();
		void		destroy();

		void		remove_bonus();
		void		remove_bonus_for_one(uint32_t pid);

		void		send_parameter(character * ch);
		void		send_parameter_to_all();

		member_map	member_map_;
		uint32_t		leader_pid_;
		character *	chr_leader_;

		event_ptr		event_update_;

		member_map::iterator next_owner_;

	private:
		int		exp_distribution_mode_;
		character *	chr_exp_centralize_;

		uint32_t		party_start_time_;

		uint32_t		party_heal_time_;
		bool		party_heal_ready_;
		bool		can_use_party_heal_;

		int		an_role_count_[PARTY_ROLE_MAX_NUM];
		int		an_max_role_[PARTY_ROLE_MAX_NUM];

		int		long_time_exp_bonus_;

		int		leadership_;
		int		exp_bonus_;

		int		count_near_party_member_;

		bool		pc_party_;

		flag_map	map_i_flag_;

		dungeon *	dungeon_;
		dungeon *	dungeon_for_only_party_;
	public:
		void set_dungeon_for_only_party(dungeon * dungeon_value);
		auto dungeon_for_only_party() -> dungeon *;
};

template <class Func> void party::for_each_member(Func & f)
{
	member_map::iterator it;

	for (it = member_map_.begin(); it != member_map_.end(); ++it)
		f(it->first);
}

template <class Func> void party::for_each_member_ptr(Func & f)
{
	member_map::iterator it;

	for (it = member_map_.begin(); it != member_map_.end(); ++it)
		f(it->second.character);
}

template <class Func> void party::for_each_online_member(Func & f)
{
	member_map::iterator it;

	for (it = member_map_.begin(); it != member_map_.end(); ++it)
		if (it->second.character)
			f(it->second.character);
}

template <class Func> void party::for_each_near_member(Func & f)
{
	member_map::iterator it;

	for (it = member_map_.begin(); it != member_map_.end(); ++it)
		if (it->second.character && it->second.near)
			f(it->second.character);
}

template <class Func> void party::for_each_on_map_member (Func & f, int32_t map_index_value)
{
	member_map::iterator it;

	for (it = member_map_.begin(); it != member_map_.end(); ++it)
	{
		character * ch = it->second.character;
		if (ch)
		{
			if (ch->map_index () == map_index_value)
				f(ch);
		}
	}
}

template <class Func> auto party::for_each_on_map_member_bool(Func & f, int32_t map_index_value) -> bool
{
	member_map::iterator it;

	for (it = member_map_.begin(); it != member_map_.end(); ++it)
	{
		character * ch = it->second.character;
		if (ch)
		{
			if (ch->map_index () == map_index_value)
			{
				if(f(ch) == false)
				{
					return false;
			
				}
			}
		}
	}
	return true;
}
}
