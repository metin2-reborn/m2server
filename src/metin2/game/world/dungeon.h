#pragma once

#include <metin2/game/app/event.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>
#include <metin2/log/log.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace metin2::game::inline parties {
    class party;
}

namespace metin2::game::inline world {


class dungeon
{
	typedef std::unordered_map<party *, int> party_map;
	typedef std::map<std::string, character *> unique_mob_map;

	public:
	// <Factor> Non-persistent identifier type
	typedef uint32_t id_type;

	~dungeon();

	// <Factor>
	id_type id() const { return id_; }

	// DUNGEON_NOTICE
	void	notice(const char* msg);
	// END_OF_DUNGEON_NOTICE

	void	join_party(party * party_value);
	void	quit_party(party * party_value);

	void	join(character * ch);

	void	inc_member(character * ch);
	void	dec_member(character * ch);

	// DUNGEON_KILL_ALL_BUG_FIX
	void	purge();
	void	kill_all();
	// END_OF_DUNGEON_KILL_ALL_BUG_FIX

	void	inc_monster() { monster_count_++; metin2::log::info("MonsterCount {:d}", monster_count_); }
	void	dec_monster() { monster_count_--; check_eliminated(); }
	int	count_monster() { return monster_count_; }	// number of monsters regenerated from data

	void	inc_party_member(party * party_value, character * ch);
	void	dec_party_member(party * party_value, character * ch);

	void	inc_kill_count(character * killer, character * victim);
	int	kill_mob_count();
	int	kill_stone_count();
	bool	is_use_potion();
	bool	is_use_revive();
	void	use_potion(character * ch);
	void	use_revive(character * ch);

	int32_t	map_index() { return map_index_; }

	void	spawn(uint32_t vnum, const char* pos);
	character *	spawn_mob(uint32_t vnum, int x, int y, int dir = 0);
	character *	spawn_mob_ac_dir(uint32_t vnum, int x, int y, int dir = 0);
	character *	spawn_group(uint32_t vnum, int32_t x, int32_t y, float radius, bool aggressive=false, int count=1);

	void	spawn_name_mob(uint32_t vnum, int x, int y, const char* name);
	void	spawn_goto_mob(int32_t from_x, int32_t from_y, int32_t to_x, int32_t to_y);

	void	spawn_regen(const char* filename, bool once = true);
	void	add_regen(regen * regen);
	void	clear_regen();
	bool	is_valid_regen(regen * regen, size_t regen_id);

	void	set_unique(const char* key, uint32_t vid);
	void	spawn_move_unique(const char* key, uint32_t vnum, const char* pos_from, const char* pos_to);
	void	spawn_move_group(uint32_t vnum, const char* pos_from, const char* pos_to, int count=1);
	void	spawn_unique(const char* key, uint32_t vnum, const char* pos);
	void	spawn_stone_door(const char* key, const char* pos);
	void	spawn_wooden_door(const char* key, const char* pos);
	void	kill_unique(const std::string& key);
	void	purge_unique(const std::string& key);
	bool	is_unique_dead(const std::string& key);
	float	unique_hp_perc(const std::string& key);
	uint32_t	unique_vid(const std::string& key);

	void	dead_character(character * ch);

	void	unique_set_max_hp(const std::string& key, int max_hp);
	void	unique_set_hp(const std::string& key, int hp);
	void	unique_set_def_grade(const std::string& key, int grade);

	void	send_dest_position_to_party(party * party_value, int32_t x, int32_t y);

	void	check_eliminated();

	void	jump_all(int32_t from_map_index, int x, int y);
	void	warp_all(int32_t from_map_index, int x, int y);
	void	jump_party(party * party_value, int32_t from_map_index, int x, int y);

	void	exit_all();
	void	exit_all_to_start_position();
	void	jump_to_eliminate_location();
	void	set_exit_all_at_eliminate(int32_t time);
	void	set_warp_at_eliminate(int32_t time, int32_t map_index, int x, int y, const char* regen_file);

	int	flag(std::string name);
	void	set_flag(std::string name, int value);
	void	set_warp_location (int32_t map_index, int x, int y);

	// an item group consists of item_vnum and item_count.
	typedef std::vector <std::pair <uint32_t, int> > item_group;
	void	create_item_group (std::string& group_name, item_group& item_group);
	const item_group* get_item_group (std::string& group_name);

	template <class Func> Func for_each_member(Func f);

	bool is_all_pc_near_to( int x, int y, int dist );

	protected:
	dungeon(id_type id, int32_t original_map_index, int32_t map_index);

	void	initialize();

	private:
	id_type 		id_; // <Factor>
	uint32_t		map_index_;

	character_set	    set_pk_character_;
	std::map<std::string, int>  map_flag_;
	typedef std::map<std::string, item_group> item_group_map;
	item_group_map map_item_group_;
	party_map	map_pk_party_;
	area_map&	map_area_;
	unique_mob_map	map_unique_mob_;

	int		mob_kill_;
	int		stone_kill_;
	bool		use_potion_;
	bool		use_revive_;

	int		monster_count_;

	bool		exit_all_at_eliminate_;
	bool		warp_at_eliminate_;

	// position to warp to when all enemies are wiped out
	int		warp_delay_;
	int32_t		warp_map_index_;
	int32_t		warp_x_;
	int32_t		warp_y_;
	std::string	regen_file_;

	std::vector<regen *> regen_;

	event_ptr		dead_event_;
	// <Factor>
	event_ptr exit_all_event_;
	event_ptr jump_to_event_;
	size_t regen_id_;

	friend class dungeon_manager;
	friend EVENTFUNC(dungeon_dead_event);
	// <Factor>
	friend EVENTFUNC(dungeon_exit_all_event);
	friend EVENTFUNC(dungeon_jump_to_event);

	// temporary variable for party-based dungeon entry.
	// m_map_pkParty is poorly managed and judged unusable, so
	// a variable managing a single party was created temporarily.
	
	party * party_;
	public :
	void set_party_null();
};

class dungeon_manager : public singleton<dungeon_manager>
{
	typedef std::map<dungeon::id_type, dungeon *> dungeon_map;
	typedef std::map<int32_t, dungeon *> map_dungeon;

	public:
	dungeon_manager();
	~dungeon_manager() override;

	dungeon *	create(int32_t original_map_index);
	void		destroy(dungeon::id_type dungeon_id);
	dungeon *	find(dungeon::id_type dungeon_id);
	dungeon *	find_by_map_index(int32_t map_index);

	private:
	dungeon_map	map_pk_dungeon_;
	map_dungeon map_pk_map_dungeon_;

	// <Factor> Introduced unsigned 32-bit dungeon identifier
	dungeon::id_type next_id_;
};
}
