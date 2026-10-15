#include <metin2/game/characters/char_manager.h>

#include <metin2/game/characters/char.h>
#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/config.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/world/dungeon.h>
#include <metin2/game/characters/FSM.h>
#include <metin2/game/events/mining.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/quest/questlua.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/world/regen.h>
#include <metin2/game/world/sectree.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/world/vid.h>
#include <metin2/game/events/xmas_event.h>

#include <metin2/proto/length.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <strings.h>

namespace metin2::game::inline characters {

character_manager::character_manager() :
	vid_count_(0),
	chr_selected_stone_(nullptr),
	use_pending_destroy_(false)
{
	register_race_num(xmas::MOB_XMAS_FIRWORK_SELLER_VNUM);
	register_race_num(xmas::MOB_SANTA_VNUM);
	register_race_num(xmas::MOB_XMAS_TREE_VNUM);

	mob_item_rate_ = 100;
	mob_damage_rate_ = 100;
	mob_gold_amount_rate_ = 100;
	mob_gold_drop_rate_ = 100;
	mob_exp_rate_ = 100;

	mob_item_rate_premium_ = 100;
	mob_gold_amount_rate_premium_ = 100;
	mob_gold_drop_rate_premium_ = 100;
	mob_exp_rate_premium_ = 100;
	
}

character_manager::~character_manager()
{
	destroy();
}

void character_manager::destroy()
{
	auto it = map_pk_chr_by_vid_.begin();
	while (it != map_pk_chr_by_vid_.end()) {
		character * ch = it->second;
		character_manager::instance().destroy_character(ch); // m_map_pkChrByVID is changed here
		it = map_pk_chr_by_vid_.begin();
	}
}

void character_manager::graceful_shutdown()
{
	name_map::iterator it = map_pk_pc_chr_.begin();

	while (it != map_pk_pc_chr_.end())
		(it++)->second->disconnect("GracefulShutdown");
}

uint32_t character_manager::alloc_vid()
{
	++vid_count_;
	return vid_count_;
}

character * character_manager::create_character(const char * name, uint32_t dwPID)
{
	uint32_t dwVID = alloc_vid();

	character * ch = new character;
	ch->create(name, dwVID, dwPID ? true : false);

	map_pk_chr_by_vid_.insert(std::make_pair(dwVID, ch));

	if (dwPID)
	{
		char szName[CHARACTER_NAME_MAX_LEN + 1];
		str_lower(name, szName, sizeof(szName));

		map_pk_pc_chr_.insert(name_map::value_type(szName, ch));
		map_pk_chr_by_pid_.insert(std::make_pair(dwPID, ch));
	}

	return (ch);
}

void character_manager::destroy_character(character * ch)
{
	if (!ch)
		return;

	// <Factor> Check whether it has been already deleted or not.
	auto it = map_pk_chr_by_vid_.find(ch->get_vid());
	if (it == map_pk_chr_by_vid_.end()) {
		metin2::log::error("[CHARACTER_MANAGER::DestroyCharacter] <Factor> {:d} not found", (int32_t)(ch->get_vid()));
		return; // prevent duplicated destrunction
	}

	// monsters belonging to a dungeon are also removed from the dungeon.
	if (ch->is_npc() && !ch->is_pet() && ch->rider() == nullptr)
	{
		if (ch->get_dungeon())
		{
			ch->get_dungeon()->dead_character(ch);
		}
	}

	if (use_pending_destroy_)
	{
		set_pk_chr_pending_destroy_.insert(ch);
		return;
	}

	map_pk_chr_by_vid_.erase(it);

	if (true == ch->is_pc())
	{
		char szName[CHARACTER_NAME_MAX_LEN + 1];

		str_lower(ch->name(), szName, sizeof(szName));

		name_map::iterator it = map_pk_pc_chr_.find(szName);

		if (map_pk_pc_chr_.end() != it)
			map_pk_pc_chr_.erase(it);
	}

	if (0 != ch->player_id())
	{
		auto it = map_pk_chr_by_pid_.find(ch->player_id());

		if (map_pk_chr_by_pid_.end() != it)
		{
			map_pk_chr_by_pid_.erase(it);
		}
	}

	unregister_race_num_map(ch);

	remove_from_state_list(ch);

	delete ch;
}

character * character_manager::find(uint32_t dwVID)
{
	auto it = map_pk_chr_by_vid_.find(dwVID);

	if (map_pk_chr_by_vid_.end() == it)
		return nullptr;
	
	// <Factor> Added sanity check
	character * found = it->second;
	if (found != nullptr && dwVID != (uint32_t)found->get_vid()) {
		metin2::log::error("[CHARACTER_MANAGER::Find] <Factor> {:d} != {:d}", dwVID, (uint32_t)found->get_vid());
		return nullptr;
	}
	return found;
}

character * character_manager::find(const vid & vid)
{
	character * tch = find((uint32_t) vid);

	if (!tch || tch->get_vid() != vid)
		return nullptr;

	return tch;
}

character * character_manager::find_by_pid(uint32_t dwPID)
{
	auto it = map_pk_chr_by_pid_.find(dwPID);

	if (map_pk_chr_by_pid_.end() == it)
		return nullptr;

	// <Factor> Added sanity check
	character * found = it->second;
	if (found != nullptr && dwPID != found->player_id()) {
		metin2::log::error("[CHARACTER_MANAGER::FindByPID] <Factor> {:d} != {:d}", dwPID, found->player_id());
		return nullptr;
	}
	return found;
}

character * character_manager::find_pc(const char * name)
{
	char szName[CHARACTER_NAME_MAX_LEN + 1];
	str_lower(name, szName, sizeof(szName));
	name_map::iterator it = map_pk_pc_chr_.find(szName);

	if (it == map_pk_pc_chr_.end())
		return nullptr;

	// <Factor> Added sanity check
	character * found = it->second;
	if (found != nullptr && strncasecmp(szName, found->name(), CHARACTER_NAME_MAX_LEN) != 0) {
		metin2::log::error("[CHARACTER_MANAGER::FindPC] <Factor> {} != {}", name, found->name());
		return nullptr;
	}
	return found;
}

character * character_manager::spawn_mob_random_position(uint32_t dwVnum, int32_t lMapIndex)
{
	// allows deciding whether to spawn Japanese pirates (Waegu)
	{
		if (dwVnum == 5001 && !quest::quest_manager::instance().event_flag("japan_regen"))
		{
			metin2::log::debug( "WAEGU[5001] regen disabled.");
			return nullptr;
		}
	}

	// allows deciding whether to spawn Haetae
	{
		if (dwVnum == 5002 && !quest::quest_manager::instance().event_flag("newyear_mob"))
		{
			metin2::log::debug( "HAETAE (new-year-mob) [5002] regen disabled.");
			return nullptr;
		}
	}

	// Liberation Day event 
	{
		if (dwVnum == 5004 && !quest::quest_manager::instance().event_flag("independence_day"))
		{
			metin2::log::debug( "INDEPENDECE DAY [5004] regen disabled.");
			return nullptr;
		}
	}

	const mob * pkMob = mob_manager::instance().get(dwVnum);

	if (!pkMob)
	{
		metin2::log::error("no mob data for vnum {:d}", dwVnum);
		return nullptr;
	}

	if (!map_allow_find(lMapIndex))
	{
		metin2::log::error("not allowed map {:d}", lMapIndex);
		return nullptr;
	}

	sectree_map * pkSectreeMap = sectree_manager::instance().map(lMapIndex);
	if (pkSectreeMap == nullptr) {
		return nullptr;
	}

	int i;
	int32_t x, y;
	for (i=0; i<2000; i++)
	{
		x = number(1, (pkSectreeMap->setting.width / 100)  - 1) * 100 + pkSectreeMap->setting.base_x;
		y = number(1, (pkSectreeMap->setting.height / 100) - 1) * 100 + pkSectreeMap->setting.base_y;
		sectree * tree = pkSectreeMap->find(x, y);

		if (!tree)
			continue;

		uint32_t dwAttr = tree->get_attribute(x, y);

		if (is_set(dwAttr, ATTR_BLOCK | ATTR_OBJECT))
			continue;

		if (is_set(dwAttr, ATTR_BANPK))
			continue;

		break;
	}

	if (i == 2000)
	{
		metin2::log::error("cannot find valid location");
		return nullptr;
	}

	sectree * sectree = sectree_manager::instance().get(lMapIndex, x, y);

	if (!sectree)
	{
		metin2::log::info( "SpawnMobRandomPosition: cannot create monster at non-exist sectree {:d} x {:d} (map {:d})", x, y, lMapIndex);
		return nullptr;
	}

	character * ch = character_manager::instance().create_character(pkMob->table.locale_name);

	if (!ch)
	{
		metin2::log::info( "SpawnMobRandomPosition: cannot create new character");
		return nullptr;
	}

	ch->set_proto(pkMob);

	// if mob is npc with no empire assigned, assign to empire of map
	if (pkMob->table.type == CHAR_TYPE_NPC)
		if (ch->empire() == 0)
			ch->set_empire(sectree_manager::instance().empire_from_map_index(lMapIndex));

	ch->set_rotation(number(0, 360));

	if (!ch->show(lMapIndex, x, y, 0, false))
	{
		character_manager::instance().destroy_character(ch);
		metin2::log::error("SpawnMobRandomPosition: cannot show monster");
		return nullptr;
	}

	char buf[512+1];
	int32_t local_x = x - pkSectreeMap->setting.base_x;
	int32_t local_y = y - pkSectreeMap->setting.base_y;
	snprintf(buf, sizeof(buf), "spawn %s[%d] random position at %d %d %d %d (time: %ld)", ch->name(), dwVnum, x, y, local_x, local_y, get_global_time());
	
	if (metin2::config::test_server())
		send_notice(buf);

	metin2::log::info( "{}", buf);
	return (ch);
}

character * character_manager::spawn_mob(uint32_t dwVnum, int32_t lMapIndex, int32_t x, int32_t y, int32_t z, bool bSpawnMotion, int iRot, bool bShow)
{
	const mob * pkMob = mob_manager::instance().get(dwVnum);
	if (!pkMob)
	{
		metin2::log::error("SpawnMob: no mob data for vnum {:d}", dwVnum);
		return nullptr;
	}

	if (!(pkMob->table.type == CHAR_TYPE_NPC || pkMob->table.type == CHAR_TYPE_WARP || pkMob->table.type == CHAR_TYPE_GOTO) || mining::is_vein_of_ore (dwVnum))
	{
		sectree * tree = sectree_manager::instance().get(lMapIndex, x, y);

		if (!tree)
		{
			metin2::log::info( "no sectree for spawn at {:d} {:d} mobvnum {:d} mapindex {:d}", x, y, dwVnum, lMapIndex);
			return nullptr;
		}

		uint32_t dwAttr = tree->get_attribute(x, y);

		bool is_set_value = false;

		if ( mining::is_vein_of_ore (dwVnum) ) is_set_value = is_set(dwAttr, ATTR_BLOCK);
		else is_set_value = is_set(dwAttr, ATTR_BLOCK | ATTR_OBJECT);

		if ( is_set_value )
		{
			// SPAWN_BLOCK_LOG
			static bool s_isLog=quest::quest_manager::instance().event_flag("spawn_block_log");
			static uint32_t s_nextTime=get_global_time()+10000;

			uint32_t curTime=get_global_time();

			if (curTime>s_nextTime)
			{
				s_nextTime=curTime;
				s_isLog=quest::quest_manager::instance().event_flag("spawn_block_log");

			}

			if (s_isLog)
				metin2::log::info( "SpawnMob: BLOCKED position for spawn {} {:d} at {:d} {:d} (attr {:d})", pkMob->table.name, dwVnum, x, y, dwAttr);
			// END_OF_SPAWN_BLOCK_LOG
			return nullptr;
		}

		if (is_set(dwAttr, ATTR_BANPK))
		{
			metin2::log::info( "SpawnMob: BAN_PK position for mob spawn {} {:d} at {:d} {:d}", pkMob->table.name, dwVnum, x, y);
			return nullptr;
		}
	}

	sectree * sectree = sectree_manager::instance().get(lMapIndex, x, y);

	if (!sectree)
	{
		metin2::log::info( "SpawnMob: cannot create monster at non-exist sectree {:d} x {:d} (map {:d})", x, y, lMapIndex);
		return nullptr;
	}

	character * ch = character_manager::instance().create_character(pkMob->table.locale_name);

	if (!ch)
	{
		metin2::log::info( "SpawnMob: cannot create new character");
		return nullptr;
	}

	if (iRot == -1)
		iRot = number(0, 360);

	ch->set_proto(pkMob);

	// if mob is npc with no empire assigned, assign to empire of map
	if (pkMob->table.type == CHAR_TYPE_NPC)
		if (ch->empire() == 0)
			ch->set_empire(sectree_manager::instance().empire_from_map_index(lMapIndex));

	ch->set_rotation(iRot);

	if (bShow && !ch->show(lMapIndex, x, y, z, bSpawnMotion))
	{
		character_manager::instance().destroy_character(ch);
		metin2::log::info( "SpawnMob: cannot show monster");
		return nullptr;
	}

	return (ch);
}

character * character_manager::spawn_mob_range(uint32_t dwVnum, int32_t lMapIndex, int sx, int sy, int ex, int ey, bool bIsException, bool bSpawnMotion, bool bAggressive )
{
	const mob * pkMob = mob_manager::instance().get(dwVnum);

	if (!pkMob)
		return nullptr;

	if (pkMob->table.type == CHAR_TYPE_STONE)	// stones always have a SPAWN motion.
		bSpawnMotion = true;

	int i = 16;

	while (i--)
	{
		int x = number(sx, ex);
		int y = number(sy, ey);
		character * ch = spawn_mob(dwVnum, lMapIndex, x, y, 0, bSpawnMotion);

		if (ch)
		{
			metin2::log::debug( "MOB_SPAWN: {}({:d}) {:d}x{:d}", ch->name(), (uint32_t) ch->get_vid(), ch->x(), ch->y());
			if ( bAggressive )
				ch->set_aggressive();
			return (ch);
		}
	}

	return nullptr;
}

void character_manager::select_stone(character * pkChr)
{
	chr_selected_stone_ = pkChr;
}

bool character_manager::spawn_move_group(uint32_t dwVnum, int32_t lMapIndex, int sx, int sy, int ex, int ey, int tx, int ty, regen * pkRegen, bool bAggressive_)
{
	mob_group * pkGroup = mob_manager::instance().group(dwVnum);

	if (!pkGroup)
	{
		metin2::log::error("NOT_EXIST_GROUP_VNUM({:d}) Map({:d}) ", dwVnum, lMapIndex);
		return false;
	}

	character * pkChrMaster = nullptr;
	party * pkParty = nullptr;

	const std::vector<uint32_t> & c_rdwMembers = pkGroup->member_vector();

	bool bSpawnedByStone = false;
	bool bAggressive = bAggressive_;

	if (chr_selected_stone_)
	{
		bSpawnedByStone = true;
		if (chr_selected_stone_->get_dungeon())
			bAggressive = true;
	}

	for (uint32_t i = 0; i < c_rdwMembers.size(); ++i)
	{
		character * tch = spawn_mob_range(c_rdwMembers[i], lMapIndex, sx, sy, ex, ey, true, bSpawnedByStone);

		if (!tch)
		{
			if (i == 0)	// if the monster that failed to be created is the leader, just fail
				return false;

			continue;
		}

		sx = tch->x() - number(300, 500);
		sy = tch->y() - number(300, 500);
		ex = tch->x() + number(300, 500);
		ey = tch->y() + number(300, 500);

		if (chr_selected_stone_)
			tch->set_stone(chr_selected_stone_);
		else if (pkParty)
		{
			pkParty->join(tch->get_vid());
			pkParty->link(tch);
		}
		else if (!pkChrMaster)
		{
			pkChrMaster = tch;
			pkChrMaster->set_regen(pkRegen);

			pkParty = party_manager::instance().create_party(pkChrMaster);
		}
		if (bAggressive)
			tch->set_aggressive();

		if (tch->goto_(tx, ty))
			tch->send_move_packet(FUNC_WAIT, 0, 0, 0, 0);
	}

	return true;
}

bool character_manager::spawn_group_group(uint32_t dwVnum, int32_t lMapIndex, int sx, int sy, int ex, int ey, regen * pkRegen, bool bAggressive_, dungeon * pDungeon)
{
	const uint32_t dwGroupID = mob_manager::instance().group_from_group_group(dwVnum);

	if( dwGroupID != 0 )
	{
		return spawn_group(dwGroupID, lMapIndex, sx, sy, ex, ey, pkRegen, bAggressive_, pDungeon);
	}
	else
	{
		metin2::log::error( "NOT_EXIST_GROUP_GROUP_VNUM({:d}) MAP({:d})", dwVnum, lMapIndex );
		return false;
	}
}

character * character_manager::spawn_group(uint32_t dwVnum, int32_t lMapIndex, int sx, int sy, int ex, int ey, regen * pkRegen, bool bAggressive_, dungeon * pDungeon)
{
	mob_group * pkGroup = mob_manager::instance().group(dwVnum);

	if (!pkGroup)
	{
		metin2::log::error("NOT_EXIST_GROUP_VNUM({:d}) Map({:d}) ", dwVnum, lMapIndex);
		return nullptr;
	}

	character * pkChrMaster = nullptr;
	party * pkParty = nullptr;

	const std::vector<uint32_t> & c_rdwMembers = pkGroup->member_vector();

	bool bSpawnedByStone = false;
	bool bAggressive = bAggressive_;

	if (chr_selected_stone_)
	{
		bSpawnedByStone = true;

		if (chr_selected_stone_->get_dungeon())
			bAggressive = true;
	}

	character * chLeader = nullptr;

	for (uint32_t i = 0; i < c_rdwMembers.size(); ++i)
	{
		character * tch = spawn_mob_range(c_rdwMembers[i], lMapIndex, sx, sy, ex, ey, true, bSpawnedByStone);

		if (!tch)
		{
			if (i == 0)	// if the monster that failed to be created is the leader, just fail
				return nullptr;

			continue;
		}

		if (i == 0)
			chLeader = tch;

		tch->set_dungeon(pDungeon);

		sx = tch->x() - number(300, 500);
		sy = tch->y() - number(300, 500);
		ex = tch->x() + number(300, 500);
		ey = tch->y() + number(300, 500);

		if (chr_selected_stone_)
			tch->set_stone(chr_selected_stone_);
		else if (pkParty)
		{
			pkParty->join(tch->get_vid());
			pkParty->link(tch);
		}
		else if (!pkChrMaster)
		{
			pkChrMaster = tch;
			pkChrMaster->set_regen(pkRegen);

			pkParty = party_manager::instance().create_party(pkChrMaster);
		}

		if (bAggressive)
			tch->set_aggressive();
	}

	return chLeader;
}

struct func_update_and_reset_chat_counter
{
	void operator () (character * ch)
	{
		ch->reset_chat_counter();
		ch->fsm::update();
	}
};

void character_manager::update(int iPulse)
{
	begin_pending_destroy();

	// PC character update
	{
		if (!map_pk_pc_chr_.empty())
		{
			// copy container
			character_vector v;
			v.reserve(map_pk_pc_chr_.size());
			for (const auto & [name, chr] : map_pk_pc_chr_)
				v.push_back(chr);
			if (0 == (iPulse % passes_per_sec(5)))
			{
				func_update_and_reset_chat_counter f;
				for_each(v.begin(), v.end(), f);
			}
			else
			{
				for_each(v.begin(), v.end(), std::bind(&character::update_character, std::placeholders::_1, iPulse));
			}
		}
	}

	// monster update
	{
		if (!set_pk_chr_state_.empty())
		{
			character_vector v;
			v.reserve(set_pk_chr_state_.size());
			v.insert(v.end(), set_pk_chr_state_.begin(), set_pk_chr_state_.end());
			for_each(v.begin(), v.end(), std::bind(&character::update_state_machine, std::placeholders::_1, iPulse));
		}
	}

	// update Santa separately
	{
		character_vector_interactor i;

		if (character_manager::instance().characters_by_race_num(xmas::MOB_SANTA_VNUM, i))
		{
			for_each(i.begin(), i.end(), std::bind(&character::update_state_machine, std::placeholders::_1, iPulse));
		}
	}

	// record mob kill counts once per hour 
	if (0 == (iPulse % passes_per_sec(3600)))
	{
		for (auto it = map_dw_mob_kill_count_.begin(); it != map_dw_mob_kill_count_.end(); ++it)
			db_manager::instance().send_money_log(MONEY_LOG_MONSTER_KILL, it->first, it->second);


		map_dw_mob_kill_count_.clear();
	}

	// on the test server, count characters every 60 seconds
	if (metin2::config::test_server() && 0 == (iPulse % passes_per_sec(60)))
		metin2::log::info( "CHARACTER COUNT vid {:d} pid {:d}", map_pk_chr_by_vid_.size(), map_pk_chr_by_pid_.size());

	// perform delayed DestroyCharacter
	flush_pending_destroy();
}

void character_manager::process_delayed_save()
{
	character_set::iterator it = set_pk_chr_for_delayed_save_.begin();

	while (it != set_pk_chr_for_delayed_save_.end())
	{
		character * pkChr = *it++;
		pkChr->save_real();
	}

	set_pk_chr_for_delayed_save_.clear();
}

bool character_manager::add_to_state_list(character * ch)
{
	assert(ch != nullptr);

	character_set::iterator it = set_pk_chr_state_.find(ch);

	if (it == set_pk_chr_state_.end())
	{
		set_pk_chr_state_.insert(ch);
		return true;
	}

	return false;
}

void character_manager::remove_from_state_list(character * ch)
{
	character_set::iterator it = set_pk_chr_state_.find(ch);

	if (it != set_pk_chr_state_.end())
	{
		set_pk_chr_state_.erase(it);
	}
}

void character_manager::delayed_save(character * ch)
{
	set_pk_chr_for_delayed_save_.insert(ch);
}

bool character_manager::flush_delayed_save(character * ch)
{
	character_set::iterator it = set_pk_chr_for_delayed_save_.find(ch);

	if (it == set_pk_chr_for_delayed_save_.end())
		return false;

	set_pk_chr_for_delayed_save_.erase(it);
	ch->save_real();
	return true;
}

void character_manager::register_for_monster_log(character * ch)
{
	set_pk_chr_monster_log_.insert(ch);
}

void character_manager::unregister_for_monster_log(character * ch)
{
	set_pk_chr_monster_log_.erase(ch);
}

void character_manager::packet_monster_log(character * ch, const void* buf, int size)
{
	decltype(set_pk_chr_monster_log_.begin()) it;

	for (it = set_pk_chr_monster_log_.begin(); it!=set_pk_chr_monster_log_.end();++it)
	{
		character * c = *it;

		if (ch && distance_approx(c->x()-ch->x(), c->y()-ch->y())>6000)
			continue;

		desc * d = c->get_desc();

		if (d)
			d->packet(buf, size);
	}
}

void character_manager::kill_log(uint32_t dwVnum)
{
	const uint32_t SEND_LIMIT = 10000;

	auto it = map_dw_mob_kill_count_.find(dwVnum);

	if (it == map_dw_mob_kill_count_.end())
		map_dw_mob_kill_count_.insert(std::make_pair(dwVnum, 1));
	else
	{
		++it->second;

		if (it->second > SEND_LIMIT)
		{
			db_manager::instance().send_money_log(MONEY_LOG_MONSTER_KILL, it->first, it->second);
			map_dw_mob_kill_count_.erase(it);
		}
	}
}

void character_manager::register_race_num(uint32_t dwVnum)
{
	set_dw_registered_race_num_.insert(dwVnum);
}

void character_manager::register_race_num_map(character * ch)
{
	uint32_t dwVnum = ch->race_num();

	if (set_dw_registered_race_num_.find(dwVnum) != set_dw_registered_race_num_.end()) // if it's a registered number
	{
		metin2::log::info( "RegisterRaceNumMap {} {:d}", ch->name(), dwVnum);
		map_pk_chr_by_race_num_[dwVnum].insert(ch);
	}
}

void character_manager::unregister_race_num_map(character * ch)
{
	uint32_t dwVnum = ch->race_num();

	auto it = map_pk_chr_by_race_num_.find(dwVnum);

	if (it != map_pk_chr_by_race_num_.end())
		it->second.erase(ch);
}

bool character_manager::characters_by_race_num(uint32_t dwRaceNum, character_vector_interactor & i)
{
	std::map<uint32_t, character_set>::iterator it = map_pk_chr_by_race_num_.find(dwRaceNum);

	if (it == map_pk_chr_by_race_num_.end())
		return false;

	// copy container
	i = it->second;
	return true;
}


//
// (job+1)*3+(skill_group)
//
character * character_manager::find_specify_pc(unsigned int uiJobFlag, int32_t lMapIndex, character * except, int iMinLevel, int iMaxLevel)
{
	character * chFind = nullptr;
	decltype(map_pk_chr_by_pid_.begin()) it;
	int n = 0;

	for (it = map_pk_chr_by_pid_.begin(); it != map_pk_chr_by_pid_.end(); ++it)
	{
		character * ch = it->second;

		if (ch == except)
			continue;

		if (ch->level() < iMinLevel)
			continue;

		if (ch->level() > iMaxLevel)
			continue;

		if (ch->map_index() != lMapIndex)
			continue;

		if (uiJobFlag)
		{
			unsigned int uiChrJob = (1 << ((ch->job() + 1) * 3 + ch->skill_group()));

			if (!is_set(uiJobFlag, uiChrJob))
				continue;
		}

		if (!chFind || number(1, ++n) == 1)
			chFind = ch;
	}

	return chFind;
}

int character_manager::mob_item_rate(character * ch)	
{ 
	//PREVENT_TOXICATION_FOR_CHINA
	//END_PREVENT_TOXICATION_FOR_CHINA
	if (ch && ch->premium_remain_seconds(PREMIUM_ITEM) > 0)
		return mob_item_rate_premium_;
	return mob_item_rate_; 
}

int character_manager::mob_damage_rate(character * ch)	
{ 
	return mob_damage_rate_; 
}

int character_manager::mob_gold_amount_rate(character * ch)
{ 
	if ( !ch )
		return mob_gold_amount_rate_;

	//PREVENT_TOXICATION_FOR_CHINA
	//END_PREVENT_TOXICATION_FOR_CHINA
	if (ch && ch->premium_remain_seconds(PREMIUM_GOLD) > 0)
		return mob_gold_amount_rate_premium_;
	return mob_gold_amount_rate_; 
}

int character_manager::mob_gold_drop_rate(character * ch)
{
	if ( !ch )
		return mob_gold_drop_rate_;

	//PREVENT_TOXICATION_FOR_CHINA
	//END_PREVENT_TOXICATION_FOR_CHINA
	
	if (ch && ch->premium_remain_seconds(PREMIUM_GOLD) > 0)
		return mob_gold_drop_rate_premium_;
	return mob_gold_drop_rate_;
}

int character_manager::mob_exp_rate(character * ch)
{ 
	if ( !ch )
		return mob_exp_rate_;

	if (ch && ch->premium_remain_seconds(PREMIUM_EXP) > 0)
		return mob_exp_rate_premium_;
	return mob_exp_rate_; 
}

void character_manager::send_script_to_map(int32_t lMapIndex, const std::string & s)
{
	sectree_map * pSecMap = sectree_manager::instance().map(lMapIndex);

	if (nullptr == pSecMap)
		return;

	struct packet_script p;

	p.header = HEADER_GC_SCRIPT;
	p.skin = 1;
	p.src_size = s.size();

	quest::f_send_packet f;
	p.size = p.src_size + sizeof(struct packet_script);
	f.buf.write(&p, sizeof(struct packet_script));
	f.buf.write(&s[0], s.size());

	pSecMap->for_each(f);
}

bool character_manager::begin_pending_destroy()
{
	// to support not flushing when Begin is called again after Begin
	// return false if already started
	if (use_pending_destroy_)
		return false;

	use_pending_destroy_ = true;
	return true;
}

void character_manager::flush_pending_destroy()
{
	use_pending_destroy_ = false; // the flag must be set first for the actual Destroy to happen

	if (!set_pk_chr_pending_destroy_.empty())
	{
		metin2::log::info( "FlushPendingDestroy size {:d}", set_pk_chr_pending_destroy_.size());
		
		character_set::iterator it = set_pk_chr_pending_destroy_.begin(),
			end = set_pk_chr_pending_destroy_.end();
		for ( ; it != end; ++it) {
			character_manager::instance().destroy_character(*it);
		}

		set_pk_chr_pending_destroy_.clear();
	}
}

character_vector_interactor::character_vector_interactor(const character_set & r)
{
	reserve(r.size());
	insert(end(), r.begin(), r.end());

	if (character_manager::instance().begin_pending_destroy())
		my_begin_ = true;
}

character_vector_interactor::~character_vector_interactor()
{
	if (my_begin_)
		character_manager::instance().flush_pending_destroy();
}
}
