#include <metin2/game/world/dungeon.h>

#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/world/entity.h>
#include <metin2/game/app/event.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/world/regen.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/empire/start_position.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>

#include <metin2/proto/length.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>

namespace metin2::game::inline world {

dungeon::dungeon(id_type id, int32_t original_map_index, int32_t map_index)
	: id_(id),
	map_index_(map_index),
	map_area_(sectree_manager::instance().dungeon_area(original_map_index))
{
	initialize();
}

dungeon::~dungeon()
{
	if (party_ != nullptr)
	{
		party_->set_dungeon_for_only_party (nullptr);
	}
	clear_regen();
	event_cancel(&dead_event_);
	event_cancel(&exit_all_event_);
	event_cancel(&jump_to_event_);
}

void dungeon::initialize()
{
	dead_event_ = nullptr;
	exit_all_event_ = nullptr;
	jump_to_event_ = nullptr;
	regen_id_ = 0;

	mob_kill_ = 0;
	stone_kill_ = 0;
	use_potion_ = false;
	use_revive_ = false;

	monster_count_ = 0;

	exit_all_at_eliminate_ = false;
	warp_at_eliminate_ = false;

	warp_delay_ = 0;
	warp_map_index_ = 0;
	warp_x_ = 0;
	warp_y_ = 0;

	regen_file_ = "";

	party_ = nullptr;
}

void dungeon::set_flag(std::string name, int value)
{
	auto it =  map_flag_.find(name);
	if (it != map_flag_.end())
		it->second = value;
	else
		map_flag_.insert(make_pair(name, value));
}

auto dungeon::flag(std::string name) -> int
{
	auto it =  map_flag_.find(name);
	if (it != map_flag_.end())
		return it->second;
	else
		return 0;
}

struct f_send_dest_position
{
	f_send_dest_position(int32_t x, int32_t y)
	{
		p1.header = HEADER_GC_DUNGEON;
		p1.subheader = DUNGEON_SUBHEADER_GC_DESTINATION_POSITION;
		p2.x = x;
		p2.y = y;
		p1.size = sizeof(p1)+sizeof(p2);
	}

	void operator()(character * ch)
	{
		ch->get_desc()->buffered_packet(&p1, sizeof(packet_gc_dungeon));
		ch->get_desc()->packet(&p2, sizeof(packet_gc_dungeon_dest_position));
	}

	packet_gc_dungeon p1;
	packet_gc_dungeon_dest_position p2;
};

void dungeon::send_dest_position_to_party(party * party_value, int32_t x, int32_t y)
{
	if (map_pk_party_.find(party_value) == map_pk_party_.end())
	{
		metin2::log::error("Party of leader pid {:d} is not in dungeon {:d}", party_value->leader_pid(), map_index_);
		return;
	}

	f_send_dest_position f(x, y);
	party_value->for_each_near_member(f);
}

struct f_warp_to_dungeon
{
	f_warp_to_dungeon(int32_t map_index_value, game::world::dungeon * d)
		: map_index(map_index_value), dungeon(d)
		{
			sectree_map * pkSectreeMap = sectree_manager::instance().map(map_index_value);
			x = pkSectreeMap->setting.pos_spawn.x;
			y = pkSectreeMap->setting.pos_spawn.y; 
		}

	void operator () (character * ch)
	{
		ch->save_exit_location();
		ch->warp_set(x, y, map_index);
	}

	int32_t map_index;
	int32_t x;
	int32_t y;
	game::world::dungeon * dungeon;
};

void dungeon::join(character * ch)
{
	if (sectree_manager::instance().map(map_index_) == nullptr) {
		metin2::log::error("No sectree map for dungeon map {:d}", map_index_);
		return;
	}
	f_warp_to_dungeon(map_index_, this) (ch);
}

void dungeon::join_party(party * party_value)
{
	party_value->set_dungeon(this);
	map_pk_party_.insert(std::make_pair(party_value,0));

	if (sectree_manager::instance().map(map_index_) == nullptr) {
		metin2::log::error("No sectree map for dungeon map {:d}", map_index_);
		return;
	}
	f_warp_to_dungeon f(map_index_, this);
	party_value->for_each_online_member(f);
}

void dungeon::quit_party(party * party_value)
{
	party_value->set_dungeon(nullptr);
	party_map::iterator it = map_pk_party_.find(party_value);

	if (it != map_pk_party_.end())
		map_pk_party_.erase(it);
}

EVENTINFO(dungeon_id_info)
{
	dungeon::id_type dungeon_id;

	dungeon_id_info() 
	: dungeon_id(0)
	{
	}
};

EVENTFUNC(dungeon_dead_event)
{
	dungeon_id_info* info = dynamic_cast<dungeon_id_info*>( event->info );
	
	if ( info == nullptr )
	{
		metin2::log::error( "Dungeon dead event has no event info" );
		return 0;
	}

	dungeon * pDungeon = dungeon_manager::instance().find(info->dungeon_id);
	if (pDungeon == nullptr) {
		return 0;
	}

	pDungeon->dead_event_ = nullptr;

	dungeon_manager::instance().destroy(info->dungeon_id);
	return 0;
}

void dungeon::inc_member(character * ch)
{
	if (set_pk_character_.find(ch) == set_pk_character_.end())
		set_pk_character_.insert(ch);

	event_cancel(&dead_event_);
}

void dungeon::dec_member(character * ch)
{
	auto it = set_pk_character_.find(ch);

	if (it == set_pk_character_.end()) {
		return;
	}

	set_pk_character_.erase(it);

	if (set_pk_character_.empty())
	{
		dungeon_id_info* info = alloc_event_info<dungeon_id_info>();
		info->dungeon_id = id_;

		event_cancel(&dead_event_);
		dead_event_ = event_create(dungeon_dead_event, info, passes_per_sec(10));
	}
}

void dungeon::inc_party_member(party * party_value, character * ch)
{
	party_map::iterator it = map_pk_party_.find(party_value);

	if (it != map_pk_party_.end())
		it->second++;
	else
		map_pk_party_.insert(std::make_pair(party_value,1));

	inc_member(ch);
}

void dungeon::dec_party_member(party * party_value, character * ch)
{
	party_map::iterator it = map_pk_party_.find(party_value);

	if (it == map_pk_party_.end())
		metin2::log::error("Cannot remove party, it is not in the dungeon");
	else
	{
		it->second--;

		if (it->second == 0)
			quit_party(party_value);
	}

	dec_member(ch);
}

struct f_warp_to_position
{
	int32_t map_index;
	int32_t x;
	int32_t y;
	f_warp_to_position(int32_t map_index_value, int32_t x, int32_t y)
		: map_index(map_index_value), x(x), y(y)
		{}

	void operator()(entity * ent)
	{
		if (!ent->is_type(ENTITY_CHARACTER)) {
			return;
		}
		character * ch = (character *)ent;
		if (!ch->is_pc()) {
			return;
		}
		if (ch->map_index() == map_index)
		{
			ch->show(map_index, x, y, 0);
			ch->stop();
		}
		else
		{
			ch->warp_set(x,y,map_index);
		}
	}
};

struct f_warp_to_position_force
{
	int32_t map_index;
	int32_t x;
	int32_t y;
	f_warp_to_position_force(int32_t map_index_value, int32_t x, int32_t y)
		: map_index(map_index_value), x(x), y(y)
		{}

	void operator()(entity * ent)
	{
		if (!ent->is_type(ENTITY_CHARACTER)) {
			return;
		}
		character * ch = (character *)ent;
		if (!ch->is_pc()) {
			return;
		}
		ch->warp_set(x,y,map_index);
	}
};

void dungeon::jump_all(int32_t from_map_index, int x, int y)
{
	x *= 100;
	y *= 100;

	sectree_map * pMap = sectree_manager::instance().map(from_map_index);

	if (!pMap)
	{
		metin2::log::error("Map {:d} not found", from_map_index);
		return;
	}

	f_warp_to_position f(map_index_, x, y);

	pMap->for_each(f);
}

void dungeon::warp_all(int32_t from_map_index, int x, int y)
{
	x *= 100;
	y *= 100;

	sectree_map * pMap = sectree_manager::instance().map(from_map_index);

	if (!pMap)
	{
		metin2::log::error("Map {:d} not found", from_map_index);
		return;
	}

	f_warp_to_position_force f(map_index_, x, y);

	pMap->for_each(f);
}

void dungeon::jump_party(party * party_value, int32_t from_map_index, int x, int y)
{
	x *= 100;
	y *= 100;

	sectree_map * pMap = sectree_manager::instance().map(from_map_index);

	if (!pMap)
	{
		metin2::log::error("Map {:d} not found", from_map_index);
		return;
	}

	if (party_value->dungeon_for_only_party() == nullptr)
	{
		if (party_ == nullptr)
		{
			party_ = party_value;
		}
		else if (party_ != party_value)
		{
			metin2::log::warn("Another party tried to enter dungeon {:d}, which already has a party", map_index());
			return;
		}
		party_value->set_dungeon_for_only_party (this);
	}

	f_warp_to_position f(map_index_, x, y);

	party_value->for_each_on_map_member(f, from_map_index);
}

void dungeon::set_party_null()
{
	party_ = nullptr;
}


void dungeon_manager::destroy(dungeon::id_type dungeon_id)
{
	metin2::log::info( "Destroying dungeon {:d}", dungeon_id);
	dungeon * pDungeon = find(dungeon_id);
	if (pDungeon == nullptr) {
		return;
	}
	map_pk_dungeon_.erase(dungeon_id);

	int32_t lMapIndex = pDungeon->map_index_;
	map_pk_map_dungeon_.erase(lMapIndex);

	uint32_t server_timer_arg = lMapIndex;
	quest::quest_manager::instance().cancel_server_timers(server_timer_arg);

	sectree_manager::instance().destroy_private_map(lMapIndex);
	delete pDungeon;
}

auto dungeon_manager::find(dungeon::id_type dungeon_id) -> dungeon *
{
	auto it = map_pk_dungeon_.find(dungeon_id);
	if (it != map_pk_dungeon_.end())
		return it->second;
	return nullptr;
}

auto dungeon_manager::find_by_map_index(int32_t map_index) -> dungeon *
{
	auto it = map_pk_map_dungeon_.find(map_index);
	if (it != map_pk_map_dungeon_.end()) {
		return it->second;
	}
	return nullptr;
}

auto dungeon_manager::create(int32_t original_map_index) -> dungeon *
{
	uint32_t lMapIndex = sectree_manager::instance().create_private_map(original_map_index);

	if (!lMapIndex) 
	{
		metin2::log::error( "Failed to create dungeon from map {:d} (new map index {:d})", original_map_index, lMapIndex );
		return nullptr;
	}

	dungeon::id_type id = next_id_++;
	while (find(id) != nullptr) {
		id = next_id_++;
	}

	dungeon * pDungeon = new dungeon(id, original_map_index, lMapIndex);
	if (!pDungeon)
	{
		metin2::log::error("Failed to allocate dungeon");
		return nullptr;
	}
	map_pk_dungeon_.insert(std::make_pair(id, pDungeon));
	map_pk_map_dungeon_.insert(std::make_pair(lMapIndex, pDungeon));

	return pDungeon;
}

dungeon_manager::dungeon_manager()
	: next_id_(0)
{
}

dungeon_manager::~dungeon_manager()
{
}

void dungeon::unique_set_max_hp(const std::string& key, int max_hp)
{
	unique_mob_map::iterator it = map_unique_mob_.find(key);
	if (it == map_unique_mob_.end())
	{
		metin2::log::error("Dungeon has no unique mob with key {}", key.c_str());
		return;
	}
	it->second->set_max_hp(max_hp);
}

void dungeon::unique_set_hp(const std::string& key, int hp)
{
	unique_mob_map::iterator it = map_unique_mob_.find(key);
	if (it == map_unique_mob_.end())
	{
		metin2::log::error("Dungeon has no unique mob with key {}", key.c_str());
		return;
	}
	it->second->set_hp(hp);
}

void dungeon::unique_set_def_grade(const std::string& key, int grade)
{
	unique_mob_map::iterator it = map_unique_mob_.find(key);
	if (it == map_unique_mob_.end())
	{
		metin2::log::error("Dungeon has no unique mob with key {}", key.c_str());
		return;
	}
	it->second->point_change(POINT_DEF_GRADE,grade - it->second->point(POINT_DEF_GRADE));
}

void dungeon::spawn_move_unique(const char* key, uint32_t vnum, const char* pos_from, const char* pos_to)
{
	area_map::iterator it_to = map_area_.find(pos_to);
	if (it_to == map_area_.end())
	{
		metin2::log::error("Unknown dungeon position {}", pos_to);
		return;
	}

	area_map::iterator it_from = map_area_.find(pos_from);
	if (it_from == map_area_.end())
	{
		metin2::log::error("Unknown dungeon position {}", pos_from);
		return;
	}

	area_info & ai = it_from->second;
	area_info & ai_to = it_to->second;
	int dir = ai.dir;
	if (dir==-1)
		dir = number(0,359);

	sectree_map * pkSectreeMap = sectree_manager::instance().map(map_index_);
	if (pkSectreeMap == nullptr) {
		metin2::log::error("No sectree map for dungeon map {:d}", map_index_);
		return;
	}
	for (int i=0;i<100;i++)
	{
		int dx = number(ai.sx, ai.ex);
		int dy = number(ai.sy, ai.ey);
		int tx = number(ai_to.sx, ai_to.ex);
		int ty = number(ai_to.sy, ai_to.ey);

		character * ch = character_manager::instance().spawn_mob(vnum, map_index_, pkSectreeMap->setting.base_x+dx, pkSectreeMap->setting.base_y+dy, 0, false, dir);

		if (ch)
		{
			map_unique_mob_.insert(make_pair(std::string(key), ch));
			ch->add_affect(AFFECT_DUNGEON_UNIQUE, POINT_NONE, 0, AFF_DUNGEON_UNIQUE, 65535, 0, true);
			ch->set_dungeon(this);

			if (ch->go_to(pkSectreeMap->setting.base_x+tx, pkSectreeMap->setting.base_y+ty))
				ch->send_move_packet(FUNC_WAIT, 0, 0, 0, 0);
		}
		else
		{
			metin2::log::error("Dungeon: failed to spawn at {:d} {:d}", pkSectreeMap->setting.base_x+((ai.sx+ai.ex)>>1), pkSectreeMap->setting.base_y+((ai.sy+ai.ey)>>1));
		}
	}

}

void dungeon::spawn_unique(const char* key, uint32_t vnum, const char* pos)
{
	area_map::iterator it = map_area_.find(pos);
	if (it == map_area_.end())
	{
		metin2::log::error("Unknown dungeon position {}", pos);
		return;
	}

	area_info & ai = it->second;
	int dir = ai.dir;
	if (dir==-1)
		dir = number(0,359);

	sectree_map * pkSectreeMap = sectree_manager::instance().map(map_index_);
	if (pkSectreeMap == nullptr) {
		metin2::log::error("No sectree map for dungeon map {:d}", map_index_);
		return;
	}
	for (int i=0;i<100;i++)
	{
		int dx = number(ai.sx, ai.ex);
		int dy = number(ai.sy, ai.ey);

		character * ch = character_manager::instance().spawn_mob(vnum, map_index_, pkSectreeMap->setting.base_x+dx, pkSectreeMap->setting.base_y+dy, 0, false, dir);

		if (ch)
		{
			map_unique_mob_.insert(make_pair(std::string(key), ch));
			ch->set_dungeon(this);
			ch->add_affect(AFFECT_DUNGEON_UNIQUE, POINT_NONE, 0, AFF_DUNGEON_UNIQUE, 65535, 0, true);
			break;
		}
		else
		{
			metin2::log::error("Dungeon: failed to spawn at {:d} {:d}", pkSectreeMap->setting.base_x+((ai.sx+ai.ex)>>1), pkSectreeMap->setting.base_y+((ai.sy+ai.ey)>>1));
		}
	}
}

void dungeon::set_unique(const char* key, uint32_t vid)
{
	character * ch = character_manager::instance().find(vid);
	if (ch)
	{
		map_unique_mob_.insert(make_pair(std::string(key), ch));
		ch->add_affect(AFFECT_DUNGEON_UNIQUE, POINT_NONE, 0, AFF_DUNGEON_UNIQUE, 65535, 0, true);
	}
}

void dungeon::spawn_stone_door(const char* key, const char* pos) 
{
	spawn_unique(key, 13001, pos);
}

void dungeon::spawn_wooden_door(const char* key, const char* pos)
{
	spawn_unique(key, 13000, pos);
	unique_set_max_hp(key, 10000);
	unique_set_hp(key, 10000);
	unique_set_def_grade(key, 300);
}

void dungeon::purge_unique(const std::string& key)
{
	unique_mob_map::iterator it = map_unique_mob_.find(key);
	if (it == map_unique_mob_.end())
	{
		metin2::log::error("Dungeon unique mob {} not found or dead", key.c_str());
		return;
	}
	character * ch = it->second;
	map_unique_mob_.erase(it);
	character_manager::instance().destroy_character(ch);
}

void dungeon::kill_unique(const std::string& key)
{
	unique_mob_map::iterator it = map_unique_mob_.find(key);
	if (it == map_unique_mob_.end())
	{
		metin2::log::error("Dungeon unique mob {} not found or dead", key.c_str());
		return;
	}
	character * ch = it->second;
	map_unique_mob_.erase(it);
	ch->dead();
}

auto dungeon::unique_vid(const std::string& key) -> uint32_t
{
	unique_mob_map::iterator it = map_unique_mob_.find(key);
	if (it == map_unique_mob_.end())
	{
		metin2::log::error("Dungeon unique mob {} not found or dead", key.c_str());
		return 0;
	}
	character * ch = it->second;
	return ch->get_vid();
}

auto dungeon::unique_hp_perc(const std::string& key) -> float
{
	unique_mob_map::iterator it = map_unique_mob_.find(key);
	if (it == map_unique_mob_.end())
	{
		metin2::log::error("Dungeon has no unique mob with key {}", key.c_str());
		return false;
	}
	return (100.f*it->second->hp())/it->second->max_hp();
}

void dungeon::dead_character(character * ch)
{
	if (!ch->is_pc())
	{
		unique_mob_map::iterator it = map_unique_mob_.begin();
		while (it!=map_unique_mob_.end())
		{
			if (it->second == ch)
			{
				map_unique_mob_.erase(it);
				break;
			}
			++it;
		}
	}
}

auto dungeon::is_unique_dead(const std::string& key) -> bool
{
	unique_mob_map::iterator it = map_unique_mob_.find(key);

	if (it == map_unique_mob_.end())
	{
		metin2::log::error("Dungeon unique mob {} not found or dead", key.c_str());
		return true;
	}

	return it->second->is_dead();
}

void dungeon::spawn(uint32_t vnum, const char* pos)
{
	area_map::iterator it = map_area_.find(pos);

	if (it == map_area_.end())
	{
		metin2::log::error("Unknown dungeon position {}", pos);
		return;
	}

	area_info & ai = it->second;
	int dir = ai.dir;
	if (dir==-1)
		dir = number(0,359);

	sectree_map * pkSectreeMap = sectree_manager::instance().map(map_index_);
	if (pkSectreeMap == nullptr)
	{
		metin2::log::error("Map {:d} not found", map_index_);
		return;
	}
	int dx = number(ai.sx, ai.ex);
	int dy = number(ai.sy, ai.ey);

	character * ch = character_manager::instance().spawn_mob(vnum, map_index_, pkSectreeMap->setting.base_x+dx, pkSectreeMap->setting.base_y+dy, 0, false, dir);
	if (ch)
		ch->set_dungeon(this);
}

auto dungeon::spawn_mob(uint32_t vnum, int x, int y, int dir) -> character *
{
	sectree_map * pkSectreeMap = sectree_manager::instance().map(map_index_);
	if (pkSectreeMap == nullptr) {
		metin2::log::error("No sectree map for dungeon map {:d}", map_index_);
		return nullptr;
	}
	metin2::log::debug( "Dungeon: spawning mob vnum {:d} at {:d} {:d}", vnum, x,  y);

	character * ch = character_manager::instance().spawn_mob(vnum, map_index_, pkSectreeMap->setting.base_x+x*100, pkSectreeMap->setting.base_y+y*100, 0, false, dir == 0 ? -1 : (dir - 1) * 45);

	if (ch)
	{
		ch->set_dungeon(this);
		metin2::log::debug( "Dungeon: spawned mob {}", ch->name());
	}

	return ch;
}

auto dungeon::spawn_mob_ac_dir(uint32_t vnum, int x, int y, int dir) -> character *
{
	sectree_map * pkSectreeMap = sectree_manager::instance().map(map_index_);
	if (pkSectreeMap == nullptr) {
		metin2::log::error("No sectree map for dungeon map {:d}", map_index_);
		return nullptr;
	}
	metin2::log::debug( "Dungeon: spawning mob vnum {:d} at {:d} {:d}", vnum, x,  y);

	character * ch = character_manager::instance().spawn_mob(vnum, map_index_, pkSectreeMap->setting.base_x+x*100, pkSectreeMap->setting.base_y+y*100, 0, false, dir);

	if (ch)
	{
		ch->set_dungeon(this);
		metin2::log::debug( "Dungeon: spawned mob {}", ch->name());
	}

	return ch;
}

void dungeon::spawn_name_mob(uint32_t vnum, int x, int y, const char* name)
{
	sectree_map * pkSectreeMap = sectree_manager::instance().map(map_index_);
	if (pkSectreeMap == nullptr) {
		metin2::log::error("No sectree map for dungeon map {:d}", map_index_);
		return;
	}

	character * ch = character_manager::instance().spawn_mob(vnum, map_index_, pkSectreeMap->setting.base_x+x, pkSectreeMap->setting.base_y+y, 0, false, -1);
	if (ch)
	{
		ch->set_name(name);
		ch->set_dungeon(this);
	}
}

void dungeon::spawn_goto_mob(int32_t from_x, int32_t from_y, int32_t to_x, int32_t to_y)
{
	const int MOB_GOTO_VNUM = 20039;

	sectree_map * pkSectreeMap = sectree_manager::instance().map(map_index_);
	if (pkSectreeMap == nullptr) {
		metin2::log::error("No sectree map for dungeon map {:d}", map_index_);
		return;
	}

	metin2::log::debug( "Dungeon: spawning goto mob at {:d} {:d}, heading to {:d} {:d}", from_x, from_y, to_x, to_y);

	from_x = pkSectreeMap->setting.base_x+from_x*100;
	from_y = pkSectreeMap->setting.base_y+from_y*100;

	character * ch = character_manager::instance().spawn_mob(MOB_GOTO_VNUM, map_index_, from_x, from_y, 0, false, -1);

	if (ch)
	{
		char buf[30+1];
		snprintf(buf, sizeof(buf), ". %d %d", to_x, to_y);

		ch->set_name(buf);
		ch->set_dungeon(this);
	}
}

auto dungeon::spawn_group(uint32_t vnum, int32_t x, int32_t y, float radius, bool aggressive, int count) -> character *
{
	sectree_map * pkSectreeMap = sectree_manager::instance().map(map_index_);
	if (pkSectreeMap == nullptr) {
		metin2::log::error("No sectree map for dungeon map {:d}", map_index_);
		return nullptr;
	}

	int iRadius = (int) radius;

	int sx = pkSectreeMap->setting.base_x + x - iRadius;
	int sy = pkSectreeMap->setting.base_y + y - iRadius;
	int ex = sx + iRadius;
	int ey = sy + iRadius;

	character * ch = nullptr;

	while (count--)
	{
		character * chLeader = character_manager::instance().spawn_group(vnum, map_index_, sx, sy, ex, ey, nullptr, aggressive, this);
		if (chLeader && !ch)
			ch = chLeader;
	}

	return ch;
}

void dungeon::spawn_regen(const char* filename, bool once)
{
	if (!filename)
	{
		metin2::log::error("Cannot spawn dungeon regen without a file name (once {:d}), dungeon map {:d}", once, map_index_); 
		return;
	}

	sectree_map * pkSectreeMap = sectree_manager::instance().map(map_index_);
	if (!pkSectreeMap)
	{
		metin2::log::error("Cannot spawn dungeon regen {} (once {:d}), dungeon map {:d} not found", filename, once, map_index_); 
		return;
	}
	regen_do(filename, map_index_, pkSectreeMap->setting.base_x, pkSectreeMap->setting.base_y, this, once);
}

void dungeon::add_regen(regen * regen)
{
	regen->id = regen_id_++;
	regen_.push_back(regen);
}

void dungeon::clear_regen()
{
	for (auto it = regen_.begin(); it != regen_.end(); ++it)
	{
		regen * regen = *it;

		event_cancel(&regen->event);
		delete regen;
	}
	regen_.clear();
}

auto dungeon::is_valid_regen(regen * regen, size_t regen_id) -> bool {
	auto it = std::find(regen_.begin(), regen_.end(), regen);
	if (it == regen_.end()) {
		return false;
	}
	auto * found = *it;
	return (found->id == regen_id);
}

void dungeon::spawn_move_group(uint32_t vnum, const char* pos_from, const char* pos_to, int count)
{
	area_map::iterator it_to = map_area_.find(pos_to);

	if (it_to == map_area_.end())
	{
		metin2::log::error("Unknown dungeon position {}", pos_to);
		return;
	}

	area_map::iterator it_from = map_area_.find(pos_from);

	if (it_from == map_area_.end())
	{
		metin2::log::error("Unknown dungeon position {}", pos_from);
		return;
	}

	area_info & ai = it_from->second;
	area_info & ai_to = it_to->second;
	int dir = ai.dir;

	if (dir == -1)
		dir = number(0,359);

	sectree_map * pkSectreeMap = sectree_manager::instance().map(map_index_);
	if (pkSectreeMap == nullptr) {
		metin2::log::error("No sectree map for dungeon map {:d}", map_index_);
		return;
	}

	while (count--)
	{
		int tx = number(ai_to.sx, ai_to.ex)+pkSectreeMap->setting.base_x;
		int ty = number(ai_to.sy, ai_to.ey)+pkSectreeMap->setting.base_y;
		character_manager::instance().spawn_move_group(vnum, map_index_, pkSectreeMap->setting.base_x+ai.sx, pkSectreeMap->setting.base_y+ai.sy, pkSectreeMap->setting.base_x+ai.ex, pkSectreeMap->setting.base_y+ai.ey, tx, ty, nullptr, true);
	}
}

namespace
{
	struct f_kill_sectree
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

	struct f_purge_sectree
	{
		void operator () (entity * ent)
		{
			if (ent->is_type(ENTITY_CHARACTER))
			{
				character * ch = (character *) ent;

				if (!ch->is_pc() && !ch->is_pet()) {
					character_manager::instance().destroy_character(ch);
				}
			}
			else if (ent->is_type(ENTITY_ITEM))
			{
				item * item = (game::items::item *) ent;
				item_manager::instance().destroy_item(item);
			}
			else
				metin2::log::error("Dungeon contains entity of unknown type {:d}", ent->type());
		}
	};
}

void dungeon::kill_all()
{
	sectree_map * pkMap = sectree_manager::instance().map(map_index_);
	if (pkMap == nullptr) {
		metin2::log::error("No sectree map for dungeon map {:d}", map_index_);
		return;
	}
	f_kill_sectree f;
	pkMap->for_each(f);
}

void dungeon::purge()
{
	sectree_map * pkMap = sectree_manager::instance().map(map_index_);
	if (pkMap == nullptr) {
		metin2::log::error("No sectree map for dungeon map {:d}", map_index_);
		return;
	}
	f_purge_sectree f;
	pkMap->for_each(f);
}

void dungeon::inc_kill_count(character * killer, character * victim)
{
	if (victim->is_stone())
		stone_kill_ ++;
	else
		mob_kill_ ++;
}

void dungeon::use_potion(character * ch)
{
	use_potion_ = true;
}

void dungeon::use_revive(character * ch)
{
	use_revive_ = true;
}

auto dungeon::is_use_potion() -> bool
{
	return use_potion_;
}

auto dungeon::is_use_revive() -> bool
{
	return use_revive_;
}

auto dungeon::kill_mob_count() -> int
{
	return mob_kill_;
}
auto dungeon::kill_stone_count() -> int
{
	return stone_kill_;
}


struct f_exit_dungeon
{
	void operator()(entity * ent)
	{
		if (ent->is_type(ENTITY_CHARACTER))
		{
			character * ch = (character *) ent;

			if (ch->is_pc())
				ch->exit_to_saved_location();
		}
	}
};

void dungeon::exit_all()
{
	sectree_map * pMap = sectree_manager::instance().map(map_index_);

	if (!pMap)
	{
		metin2::log::error("Map {:d} not found", map_index_);
		return;
	}

	f_exit_dungeon f;

	pMap->for_each(f);
}

namespace
{
	struct f_notice
	{
		f_notice(const char * psz_value) : psz(psz_value)
		{
		}

		void operator() (entity * ent)
		{
			if (ent->is_type(ENTITY_CHARACTER))
			{
				character * ch = (character *) ent;
				ch->chat_packet(CHAT_TYPE_NOTICE, "%s", psz);
			}
		}

		const char * psz;
	};
}

void dungeon::notice(const char* msg)
{
	metin2::log::debug( "Dungeon {}: notice {}", static_cast<const void *>(this), msg);
	sectree_map * pMap = sectree_manager::instance().map(map_index_);

	if (!pMap)
	{
		metin2::log::error("Map {:d} not found", map_index_);
		return;
	}

	f_notice f(msg);
	pMap->for_each(f);
}

struct f_exit_dungeon_to_start_position
{
	void operator () (entity * ent)
	{
		if (ent->is_type(ENTITY_CHARACTER))
		{
			character * ch = (character *) ent;

			if (ch->is_pc())
			{
				pixel_position posWarp;

				if (sectree_manager::instance().recall_position_by_empire(start_map[ch->empire()], ch->empire(), posWarp))
					ch->warp_set(posWarp.x, posWarp.y);
				else
					ch->exit_to_saved_location();
			}
		}
	}
};

void dungeon::exit_all_to_start_position()
{
	sectree_map * pMap = sectree_manager::instance().map(map_index_);

	if (!pMap)
	{
		metin2::log::error("Map {:d} not found", map_index_);
		return;
	}

	f_exit_dungeon_to_start_position f;

	pMap->for_each(f);
}

EVENTFUNC(dungeon_jump_to_event)
{
	dungeon_id_info * info = dynamic_cast<dungeon_id_info *>(event->info);

	if ( info == nullptr )
	{
		metin2::log::error( "Dungeon jump event has no event info" );
		return 0;
	}

	dungeon * pDungeon = dungeon_manager::instance().find(info->dungeon_id);
	pDungeon->jump_to_event_ = nullptr;

	if (pDungeon)
		pDungeon->jump_to_eliminate_location();
	else
		metin2::log::error("Dungeon {:d} not found", info->dungeon_id);

	return 0;
}

EVENTFUNC(dungeon_exit_all_event)
{
	dungeon_id_info * info = dynamic_cast<dungeon_id_info *>(event->info);

	if ( info == nullptr )
	{
		metin2::log::error( "Dungeon exit-all event has no event info" );
		return 0;
	}

	dungeon * pDungeon = dungeon_manager::instance().find(info->dungeon_id);
	pDungeon->exit_all_event_ = nullptr;

	if (pDungeon)
		pDungeon->exit_all();

	return 0;
}

void dungeon::check_eliminated()
{
	if (monster_count_ > 0)
		return;

	if (exit_all_at_eliminate_)
	{
		metin2::log::debug( "Dungeon: all monsters eliminated, scheduling exit for everyone");
		exit_all_at_eliminate_ = false;

		if (warp_delay_)
		{
			dungeon_id_info* info = alloc_event_info<dungeon_id_info>();
			info->dungeon_id = id_;

			event_cancel(&exit_all_event_);
			exit_all_event_ = event_create(dungeon_exit_all_event, info, passes_per_sec(warp_delay_));
		}
		else
		{
			exit_all();
		}
	}
	else if (warp_at_eliminate_)
	{
		metin2::log::debug( "Dungeon: all monsters eliminated, scheduling warp");
		warp_at_eliminate_ = false;

		if (warp_delay_)
		{
			dungeon_id_info* info = alloc_event_info<dungeon_id_info>();
			info->dungeon_id = id_;

			event_cancel(&jump_to_event_);
			jump_to_event_ = event_create(dungeon_jump_to_event, info, passes_per_sec(warp_delay_));
		}
		else
		{
			jump_to_eliminate_location();
		}
	}
	else
		metin2::log::debug( "Dungeon: all monsters eliminated, nothing to do");
}

void dungeon::set_exit_all_at_eliminate(int32_t time)
{
	metin2::log::debug( "Dungeon: everyone exits {:d} seconds after all monsters are eliminated", time);
	exit_all_at_eliminate_ = true;
	warp_delay_ = time;
}

void dungeon::set_warp_at_eliminate(int32_t time, int32_t map_index, int x, int y, const char* regen_file)
{
	warp_at_eliminate_ = true;
	warp_delay_ = time;
	warp_map_index_ = map_index;
	warp_x_ = x;
	warp_y_ = y;

	if (!regen_file || !*regen_file)
		regen_file_.clear();
	else
		regen_file_ = regen_file;

	metin2::log::debug( "Dungeon: warp {:d} seconds after all monsters are eliminated to map {:d} at {:d}x{:d}, regen file {}", time, map_index, x, y, regen_file_.c_str());
}

void dungeon::jump_to_eliminate_location()
{
	dungeon * pDungeon = dungeon_manager::instance().find_by_map_index(warp_map_index_);

	if (pDungeon)
	{
		pDungeon->jump_all(map_index_, warp_x_, warp_y_);

		if (!regen_file_.empty())
		{
			pDungeon->spawn_regen(regen_file_.c_str());
			regen_file_.clear();
		}
	}
	else
	{
		sectree_map * pMap = sectree_manager::instance().map(map_index_);

		if (!pMap)
		{
			metin2::log::error("Map {:d} not found", map_index_);
			return;
		}

		f_warp_to_position f(warp_map_index_, warp_x_ * 100, warp_y_ * 100);

		pMap->for_each(f);
	}
}

struct f_near_position
{
	int32_t x;
	int32_t y;
	int dist;
	bool ret;

	f_near_position(int32_t x, int32_t y, int d) :
		x(x), y(y), dist(d), ret(true)
	{
	}

	void operator()(entity * ent)
	{
		if (ret == false)
			return;

		if (ent->is_type(ENTITY_CHARACTER))
		{
			character * ch = (character *) ent;

			if (ch->is_pc())
			{
				if (distance_approx(ch->x() - x * 100, ch->y() - y * 100) > dist * 100)
					ret = false;
			}
		}
	}
};

auto dungeon::is_all_pc_near_to(int x, int y, int dist) -> bool
{
	sectree_map * pMap = sectree_manager::instance().map(map_index_);

	if (!pMap)
	{
		metin2::log::error("Map {:d} not found", map_index_);
		return false;
	}

	f_near_position f(x, y, dist);

	pMap->for_each(f);

	return f.ret;
}

void dungeon::create_item_group (std::string& group_name, item_group& item_group)
{
	map_item_group_.insert (item_group_map::value_type (group_name, item_group));
}

auto dungeon::get_item_group (std::string& group_name) -> const dungeon::item_group*
{
	item_group_map::iterator it = map_item_group_.find (group_name);
	if (it != map_item_group_.end())
		return &(it->second);
	else
		return nullptr;
}
}
