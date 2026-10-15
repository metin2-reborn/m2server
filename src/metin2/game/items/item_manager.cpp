#include <metin2/game/items/item_manager.h>

#include <metin2/game/items/blend_item.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/items/dragon_soul.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager_private_types.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/empire/priv_manager.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/trade/safebox.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/app/utils.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <strings.h>

namespace metin2::game::inline items {

item_manager::item_manager()
	: vid_count_(0), current_id_(0)
{
	item_id_range_.min = item_id_range_.max = item_id_range_.usable_item_id_min = 0;
	item_id_spare_range_.min = item_id_spare_range_.max = item_id_spare_range_.usable_item_id_min = 0;
}

item_manager::~item_manager()
{
	destroy();
}

void item_manager::destroy()
{
	auto it = vid_map_.begin();
	for ( ; it != vid_map_.end(); ++it) {
		delete it->second;
	}
	vid_map_.clear();
}

void item_manager::graceful_shutdown()
{
	std::unordered_set<item *>::iterator it = set_pk_item_for_delayed_save_.begin();

	while (it != set_pk_item_for_delayed_save_.end())
		save_single_item(*(it++));

	set_pk_item_for_delayed_save_.clear();
}

bool item_manager::initialize(item_table * table_value, int size)
{
	if (!vec_prototype_.empty())
		vec_prototype_.clear();

	int	i;

	vec_prototype_.resize(size);
	std::memcpy(&vec_prototype_[0], table_value, sizeof(item_table) * size);
	for (int i = 0; i < size; i++)
	{
		if (0 != vec_prototype_[i].vnum_range)
		{
			vec_item_vnum_range_info_.push_back( &vec_prototype_[i]);
		}
	}

	map_item_refine_from_.clear();
	for (i = 0; i < size; ++i)
	{

		if (vec_prototype_[i].refined_vnum)
			map_item_refine_from_.insert(std::make_pair(vec_prototype_[i].refined_vnum, vec_prototype_[i].vnum));

		// NOTE : the QUEST_GIVE flag is triggered by an npc event.
		if (vec_prototype_[i].type == ITEM_QUEST || is_set(vec_prototype_[i].flags, ITEM_FLAG_QUEST_USE | ITEM_FLAG_QUEST_USE_MULTIPLE))
			quest::quest_manager::instance().register_npc_vnum(vec_prototype_[i].vnum);

		map_vid.insert( std::map<uint32_t,item_table>::value_type( vec_prototype_[i].vnum, vec_prototype_[i] ) ); 
		if ( metin2::config::test_server() )
			metin2::log::info( "ITEM_INFO {:d} {} ", vec_prototype_[i].vnum, vec_prototype_[i].name );	
	}

	int len = 0, len2;
	char buf[512];

	for (i = 0; i < size; ++i)
	{
		len2 = snprintf(buf + len, sizeof(buf) - len, "%5u %-16s", vec_prototype_[i].vnum, vec_prototype_[i].locale_name);

		if (len2 < 0 || len2 >= (int) sizeof(buf) - len)
			len += (sizeof(buf) - len) - 1;
		else
			len += len2;

		if (!((i + 1) % 4))
		{
			if ( !metin2::config::test_server() )
				metin2::log::info( "{}", buf);
			len = 0;
		}
		else
		{
			buf[len++] = '\t';
			buf[len] = '\0';
		}
	}

	if ((i + 1) % 4)
	{
		if ( !metin2::config::test_server() )
			metin2::log::info( "{}", buf);
	}

	item_vid_map::iterator it = vid_map_.begin();

	metin2::log::debug( "ITEM_VID_MAP {:d}", vid_map_.size() );

	while (it != vid_map_.end())
	{
		item * item = it->second;
		++it;

		const item_table* tableInfo = table(item->original_vnum());

		if (nullptr == tableInfo)
		{
			metin2::log::error("cannot reset item table");
			item->set_proto(nullptr);
		}

		item->set_proto(tableInfo);
	}

	return true;
}

item * item_manager::create_item(uint32_t vnum, uint32_t count, uint32_t id, bool try_magic, int rare_pct, bool skip_save)
{
	if (0 == vnum)
		return nullptr;

	uint32_t dwMaskVnum = 0;
	if (mask_vnum(vnum))
	{
		dwMaskVnum = mask_vnum(vnum);
	}


	const item_table* table_value = table(vnum);

	if (nullptr == table_value)
		return nullptr;

	item * item = nullptr;

	//if it exists when checked by id -- return! 
	if (map_pk_item_by_id_.find(id) != map_pk_item_by_id_.end())
	{
		item = map_pk_item_by_id_[id];
		character * owner = item->owner();
		metin2::log::error("ITEM_ID_DUP: {:d} {} owner {}", id, item->name(), static_cast<const void *>(owner));
		return nullptr;
	}

	//allocate one item
	item = new game::items::item(vnum);

	bool bIsNewItem = (0 == id);

	//initialize it. set the table
	item->initialize();
	item->set_proto(table_value);
	item->set_mask_vnum(dwMaskVnum);

	if (item->type() == ITEM_ELK) // Gold needs no ID and no saving.
		item->set_skip_save(true);

	// Set the Unique ID
	else if (!bIsNewItem)
	{
		item->set_id(id);
		item->set_skip_save(true);
	}
	else
	{
		item->set_id(new_id());

		if (item->type() == ITEM_UNIQUE) // Unique items record the remaining time in a socket on creation.
		{
			if (item->value(2) == 0)
				item->set_socket(ITEM_SOCKET_UNIQUE_REMAIN_TIME, item->value(0)); // Game-time unique
			else
			{
				item->set_socket(ITEM_SOCKET_UNIQUE_REMAIN_TIME, get_global_time() + item->value(0)); // Real-time unique
			}
		}
	}


	switch (item->vnum())
	{
		case ITEM_AUTO_HP_RECOVERY_S:
		case ITEM_AUTO_HP_RECOVERY_M:
		case ITEM_AUTO_HP_RECOVERY_L:
		case ITEM_AUTO_HP_RECOVERY_X:
		case ITEM_AUTO_SP_RECOVERY_S:
		case ITEM_AUTO_SP_RECOVERY_M:
		case ITEM_AUTO_SP_RECOVERY_L:
		case ITEM_AUTO_SP_RECOVERY_X:
		case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_XS:
		case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_S:
		case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_XS:
		case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_S:
		case FUCKING_BRAZIL_ITEM_AUTO_SP_RECOVERY_S:
		case FUCKING_BRAZIL_ITEM_AUTO_HP_RECOVERY_S:
			if (bIsNewItem)
				item->set_socket(2, item->value(0), true);
			else
				item->set_socket(2, item->value(0), false);
			break;
	}

	if (item->type() == ITEM_ELK) // Gold needs no processing
		;
	else if (item->is_stackable())  // For stackable items
	{
		count = std::clamp<int>(count, 1, ITEM_MAX_COUNT);

		if (try_magic && count <= 1 && is_set(item->flag(), ITEM_FLAG_MAKECOUNT))
			count = item->value(1);
	}
	else
		count = 1;

	item->set_vid(++vid_count_);

	if (skip_save == false)
		vid_map_.insert(item_vid_map::value_type(item->get_vid(), item));

	if (item->id() != 0 && skip_save == false)
		map_pk_item_by_id_.insert(std::map<uint32_t, game::items::item *>::value_type(item->id(), item));

	if (!item->set_count(count))
		return nullptr;

	item->set_skip_save(false);

	if (item->type() == ITEM_UNIQUE && item->value(2) != 0)
		item->start_unique_expire_event();

	for (int i=0 ; i < ITEM_LIMIT_MAX_NUM ; i++)
	{
		// Time is deducted from creation even if unused
		if (LIMIT_REAL_TIME == item->limit_type(i))
		{
			if (item->limit_value(i))
			{
				item->set_socket(0, time(nullptr) + item->limit_value(i)); 
			}
			else
			{
				item->set_socket(0, time(nullptr) + 60*60*24*7); 
			}

			item->start_real_time_expire_event();
		}

		// Like existing unique items, usable time is deducted only while equipped
		else if (LIMIT_TIMER_BASED_ON_WEAR == item->limit_type(i))
		{
			// If the item is already equipped, start the timer; for newly created items, set the usable time. (
			// When granted through the item mall, Socket0 must be set before entering this logic.
			if (true == item->is_equipped())
			{
				item->start_timer_based_on_wear_expire_event();
			}
			else if(0 == id)
			{
				int32_t duration = item->get_socket(0);
				if (0 == duration)
					duration = item->limit_value(i);

				if (0 == duration)
					duration = 60 * 60 * 10;	// If there is no info, default to 10 hours

				item->set_socket(0, duration);
			}
		}
	}

	if (id == 0) // Only for newly created items
	{
		// Newly added herbs get different stats
		if (ITEM_BLEND==item->type())
		{
			if (blend_item_find(item->vnum()))
			{
				blend_item_set_value(item);
				return item;
			}
		}

		if (table_value->s_addon_type)
		{
			item->apply_addon(table_value->s_addon_type);
		}

		if (try_magic)
		{
			if (rare_pct == -1)
				rare_pct = table_value->alter_to_magic_item_pct;

			if (number(1, 100) <= rare_pct)
				item->alter_to_magic_item();
		}

		if (table_value->gain_socket_pct)
			item->alter_to_socket_item(table_value->gain_socket_pct);

		// 50300 == skill book
		if (vnum == 50300 || vnum == ITEM_SKILLFORGET_VNUM)
		{
			uint32_t dwSkillVnum;

			do
			{
				dwSkillVnum = number(1, 111);

				if (nullptr != skill_manager::instance().get(dwSkillVnum))
					break;
			} while (true);

			item->set_socket(0, dwSkillVnum);
		}
		else if (ITEM_SKILLFORGET2_VNUM == vnum)
		{
			uint32_t dwSkillVnum;

			do
			{
				dwSkillVnum = number(112, 119);

				if (nullptr != skill_manager::instance().get(dwSkillVnum))
					break;
			} while (true);

			item->set_socket(0, dwSkillVnum);
		}
	}

	if (item->type() == ITEM_QUEST)
	{
		for (auto it = map_pk_quest_item_group_.begin(); it != map_pk_quest_item_group_.end(); it++)
		{
			if (it->second->type == special_item_group::QUEST && it->second->contains(vnum))
			{
				item->set_sig_vnum(it->first);
			}
		}
	}
	else if (item->type() == ITEM_UNIQUE)
	{
		for (auto it = map_pk_special_item_group_.begin(); it != map_pk_special_item_group_.end(); it++)
		{
			if (it->second->type == special_item_group::SPECIAL && it->second->contains(vnum))
			{
				item->set_sig_vnum(it->first);
			}
		}
	}

	// Handle newly created dragon soul stones.
	if (item->is_dragon_soul() && 0 == id)
	{
		ds_manager::instance().dragon_soul_item_initialize(item);
	}
	return item;
}

void item_manager::delayed_save(item * item)
{
	if (item->id() != 0)
		set_pk_item_for_delayed_save_.insert(item);
}

void item_manager::flush_delayed_save(item * item)
{
	std::unordered_set<game::items::item *>::iterator it = set_pk_item_for_delayed_save_.find(item);

	if (it == set_pk_item_for_delayed_save_.end())
	{
		return;
	}

	set_pk_item_for_delayed_save_.erase(it);
	save_single_item(item);
}

void item_manager::save_single_item(item * item)
{
	if (!item->owner())
	{
		uint32_t dwID = item->id();
		uint32_t dwOwnerID = item->last_owner_pid();

		db_clientdesc->db_packet_header(HEADER_GD_ITEM_DESTROY, 0, sizeof(uint32_t) + sizeof(uint32_t));
		db_clientdesc->packet(&dwID, sizeof(uint32_t));
		db_clientdesc->packet(&dwOwnerID, sizeof(uint32_t));

		metin2::log::debug( "ITEM_DELETE {}:{:d}", item->name(), dwID);
		return;
	}

	metin2::log::debug( "ITEM_SAVE {}:{:d} in {} window {:d}", item->name(), item->id(), item->owner()->name(), item->window());

	player_item t;

	t.id = item->id();
	t.window = item->window();
	t.pos = t.window == EQUIPMENT ? item->cell() - INVENTORY_MAX_NUM : item->cell();
	t.count = item->count();
	t.vnum = item->original_vnum();
	t.owner = (t.window == SAFEBOX || t.window == MALL) ? item->owner()->get_desc()->get_account_table().id : item->owner()->player_id();
	std::memcpy(t.sockets, item->sockets(), sizeof(t.sockets));
	std::memcpy(t.attr, item->attributes(), sizeof(t.attr));

	db_clientdesc->db_packet_header(HEADER_GD_ITEM_SAVE, 0, sizeof(player_item));
	db_clientdesc->packet(&t, sizeof(player_item));
}

void item_manager::update()
{
	std::unordered_set<item *>::iterator it = set_pk_item_for_delayed_save_.begin();
	std::unordered_set<item *>::iterator this_it;

	while (it != set_pk_item_for_delayed_save_.end())
	{
		this_it = it++;
		item * item = *this_it;

		// Items with the SLOW_QUERY flag are saved only on shutdown.
		if (item->owner() && is_set(item->flag(), ITEM_FLAG_SLOW_QUERY))
			continue;

		save_single_item(item);

		set_pk_item_for_delayed_save_.erase(this_it);
	}
}

void item_manager::remove_item(item * item, const char * reason)
{
	character * o;

	if ((o = item->owner()))
	{
		char szHint[64];
		snprintf(szHint, sizeof(szHint), "%s %u ", item->name(), item->count());
		log_manager::instance().item_log(o, item, reason ? reason : "REMOVE", szHint);

		// SAFEBOX_TIME_LIMIT_ITEM_BUG_FIX
		if (item->window() == MALL || item->window() == SAFEBOX)
		{
			// 20050613.ipkn.If a timed item is in a shop, the server crashes when it expires.
			safebox* pSafebox = item->window() == MALL ? o->mall() : o->get_safebox();
			if (pSafebox)
			{
				pSafebox->remove(item->cell());
			}
		}
		// END_OF_SAFEBOX_TIME_LIMIT_ITEM_BUG_FIX
		else
		{
			o->sync_quickslot(QUICKSLOT_TYPE_ITEM, item->cell(), 255);
			item->remove_from_character();
		}
	}

	item_manager::instance().destroy_item(item);
}

void item_manager::destroy_item(item * item)
{
	if (item->get_sectree())
		item->remove_from_ground();

	if (item->owner())
	{
		if (character_manager::instance().find(item->owner()->player_id()) != nullptr)
		{
			metin2::log::error("DestroyItem: GetOwner {} {}!!", item->name(), item->owner()->name());
			item->remove_from_character();
		}
		else
		{
			metin2::log::error("WTH! Invalid item owner. owner pointer : {}", static_cast<const void *>(item->owner()));
		}
	}

	std::unordered_set<game::items::item *>::iterator it = set_pk_item_for_delayed_save_.find(item);

	if (it != set_pk_item_for_delayed_save_.end())
		set_pk_item_for_delayed_save_.erase(it);

	uint32_t dwID = item->id();
	metin2::log::debug( "ITEM_DESTROY {}:{:d}", item->name(), dwID);

	if (!item->skip_save() && dwID)
	{
		uint32_t dwOwnerID = item->last_owner_pid();

		db_clientdesc->db_packet_header(HEADER_GD_ITEM_DESTROY, 0, sizeof(uint32_t) + sizeof(uint32_t));
		db_clientdesc->packet(&dwID, sizeof(uint32_t));
		db_clientdesc->packet(&dwOwnerID, sizeof(uint32_t));
	}
	else
	{
		metin2::log::debug( "ITEM_DESTROY_SKIP {}:{:d} (skip={:d})", item->name(), dwID, item->skip_save());
	}

	if (dwID)
		map_pk_item_by_id_.erase(dwID);

	vid_map_.erase(item->get_vid());

	delete item;
}

item * item_manager::find(uint32_t id)
{
	auto it = map_pk_item_by_id_.find(id);
	if (it == map_pk_item_by_id_.end())
		return nullptr;
	return it->second;
}

item * item_manager::find_by_vid(uint32_t vid)
{
	item_vid_map::iterator it = vid_map_.find(vid);

	if (it == vid_map_.end())
		return nullptr;

	return (it->second);
}

item_table * item_manager::table(uint32_t vnum)
{
	int rnum = real_number(vnum);

	if (rnum < 0)
	{
		for (size_t i = 0; i < vec_item_vnum_range_info_.size(); i++)
		{
			item_table* p = vec_item_vnum_range_info_[i];
			if ((p->vnum < vnum) &&
				vnum < (p->vnum + p->vnum_range))
			{
				return p;
			}
		}
			
		return nullptr;
	}

	return &vec_prototype_[rnum];
}

int item_manager::real_number(uint32_t vnum)
{
	int bot, top, mid;

	bot = 0;
	top = vec_prototype_.size();

	item_table * pTable = &vec_prototype_[0];

	while (1)
	{
		mid = (bot + top) >> 1;

		if ((pTable + mid)->vnum == vnum)
			return (mid);

		if (bot >= top)
			return (-1);

		if ((pTable + mid)->vnum > vnum)
			top = mid - 1;
		else        
			bot = mid + 1;
	}
}

bool item_manager::vnum(const char * name, uint32_t & r_dw_vnum)
{
	int len = strlen(name);

	item_table * pTable = &vec_prototype_[0];

	for (uint32_t i = 0; i < vec_prototype_.size(); ++i, ++pTable)
	{
		if (!strncasecmp(name, pTable->locale_name, len))
		{
			r_dw_vnum = pTable->vnum;
			return true;
		}
	}

	return false;
}

bool item_manager::vnum_by_original_name(const char * name_value, uint32_t & r_dw_vnum)
{
	int len = strlen(name_value);

	item_table * pTable = &vec_prototype_[0];

	for (uint32_t i = 0; i < vec_prototype_.size(); ++i, ++pTable)
	{
		if (!strncasecmp(name_value, pTable->name, len))
		{
			r_dw_vnum = pTable->vnum;
			return true;
		}
	}

	return false;
}

std::set<uint32_t> set_lotto;


// 20050503.ipkn.
// If less than iMinimum, set iDefault (iMinimum must be greater than 0)
// Exists to support ON/OFF switching as 1, 0
int get_drop_per_kill_pct(int minimum, int fallback, int delta_percent, const char * flag)
{
	int iVal = 0;

	if ((iVal = quest::quest_manager::instance().event_flag(flag)))
	{
		if (!metin2::config::test_server())
		{
			if (iVal < minimum)
				iVal = fallback;

			if (iVal < 0)
				iVal = fallback;
		}
	}

	if (iVal == 0)
		return 0;

	// Default setting (iDeltaPercent=100) 
	// Constant meant to give a feel of one per 40000 iVal mobs
	return (40000 * delta_percent / iVal);
}

bool item_manager::drop_pct(character * chr, character * killer, int& delta_percent, int& rand_range)
{
 	if (nullptr == chr || nullptr == killer)
		return false;

	int iLevel = killer->level();
	delta_percent = 100;

	if (!chr->is_stone() && chr->get_mob_rank() >= MOB_RANK_BOSS)
		delta_percent = percent_lvdelta_boss(killer->level(), chr->level());
	else
		delta_percent = percent_lvdelta(killer->level(), chr->level());

	uint8_t bRank = chr->get_mob_rank();

	if (1 == number(1, 50000))
		delta_percent += 1000;
	else if (1 == number(1, 10000))
		delta_percent += 500;

	metin2::log::debug( "CreateDropItem for level: {:d} rank: {:d} pct: {:d}", iLevel, bRank, delta_percent);
	delta_percent = delta_percent * character_manager::instance().mob_item_rate(killer) / 100;

	// ADD_PREMIUM
	if (killer->premium_remain_seconds(PREMIUM_ITEM) > 0 ||
			killer->is_equip_unique_group(UNIQUE_GROUP_DOUBLE_ITEM))
		delta_percent += delta_percent;
	// END_OF_ADD_PREMIUM

	rand_range = 4000000;
	rand_range = rand_range * 100 / 
		(100 +
		 priv_manager::instance().priv(killer, PRIV_ITEM_DROP) +
		 (killer->is_equip_unique_item(UNIQUE_ITEM_DOUBLE_ITEM) ? 100 : 0));

	if (metin2::config::game().distribution_test_server) rand_range /= 3;

	return true;
}

bool item_manager::create_drop_item(character * chr, character * killer, std::vector<item *> & vec_item)
{
	int iLevel = killer->level();

	int iDeltaPercent, iRandRange;
	if (!drop_pct(chr, killer, iDeltaPercent, iRandRange))
		return false;

	uint8_t bRank = chr->get_mob_rank();
	item * item = nullptr;

	// Common Drop Items
	std::vector<item_drop_info>::iterator it = vec_pk_common_drop_item[bRank].begin();

	while (it != vec_pk_common_drop_item[bRank].end())
	{
		const item_drop_info & c_rInfo = *(it++);

		if (iLevel < c_rInfo.level_start || iLevel > c_rInfo.level_end)
			continue;

		int iPercent = (c_rInfo.percent * iDeltaPercent) / 100;
		metin2::log::debug( "CreateDropItem {:d} ~ {:d} {:d} {:d}({:d})", c_rInfo.level_start, c_rInfo.level_end, c_rInfo.vnum, iPercent, c_rInfo.percent);

		if (iPercent >= number(1, iRandRange))
		{
			item_table * table_value = table(c_rInfo.vnum);

			if (!table_value)
				continue;

			item = nullptr;

			if (table_value->type == ITEM_POLYMORPH)
			{
				if (c_rInfo.vnum == chr->polymorph_item_vnum())
				{
					item = create_item(c_rInfo.vnum, 1, 0, true);

					if (item)
						item->set_socket(0, chr->race_num());
				}
			}
			else
				item = create_item(c_rInfo.vnum, 1, 0, true);

			if (item) vec_item.push_back(item);
		}
	}

	// Drop Item Group
	{
		decltype(map_pk_drop_item_group_.begin()) it;
		it = map_pk_drop_item_group_.find(chr->race_num());

		if (it != map_pk_drop_item_group_.end())
		{
			__typeof(it->second->get_vector()) v = it->second->get_vector();

			for (uint32_t i = 0; i < v.size(); ++i)
			{
				int iPercent = (v[i].pct * iDeltaPercent) / 100;

				if (iPercent >= number(1, iRandRange))
				{
					item = create_item(v[i].vnum, v[i].count, 0, true);

					if (item)
					{
						if (item->type() == ITEM_POLYMORPH)
						{
							if (item->vnum() == chr->polymorph_item_vnum())
							{
								item->set_socket(0, chr->race_num());
							}
						}

						vec_item.push_back(item);
					}
				}
			}
		}
	}

	// MobDropItem Group
	{
		decltype(map_pk_mob_item_group_.begin()) it;
		it = map_pk_mob_item_group_.find(chr->race_num());

		if ( it != map_pk_mob_item_group_.end() )
		{
			mob_item_group* pGroup = it->second;

			// MOB_DROP_ITEM_BUG_FIX
			// 20050805.myevan.Fixed problem accessing CMobItemGroup::GetOne() when MobDropItem has no items
			if (pGroup && !pGroup->is_empty())
			{
				int iPercent = 40000 * iDeltaPercent / pGroup->kill_per_drop();
				if (iPercent >= number(1, iRandRange))
				{
					const mob_item_group::mob_item_group_info& info = pGroup->one();
					item = create_item(info.item_vnum, info.count, 0, true, info.rare_pct);

					if (item) vec_item.push_back(item);
				}
			}
			// END_OF_MOB_DROP_ITEM_BUG_FIX
		}
	}

	// Level Item Group
	{
		decltype(map_pk_level_item_group_.begin()) it;
		it = map_pk_level_item_group_.find(chr->race_num());

		if ( it != map_pk_level_item_group_.end() )
		{
			if ( it->second->level_limit() <= (uint32_t)iLevel )
			{
				__typeof(it->second->get_vector()) v = it->second->get_vector();

				for ( uint32_t i=0; i < v.size(); i++ )
				{
					if ( v[i].pct >= (uint32_t)number(1, 1000000/*iRandRange*/) )
					{
						uint32_t dwVnum = v[i].v_num;
						item = create_item(dwVnum, v[i].count, 0, true);
						if ( item ) vec_item.push_back(item);
					}
				}
			}
		}
	}
	
	// BuyerTheitGloves Item Group
	{
		if (killer->premium_remain_seconds(PREMIUM_ITEM) > 0 ||
				killer->is_equip_unique_group(UNIQUE_GROUP_DOUBLE_ITEM))
		{
			decltype(map_pk_glove_item_group_.begin()) it;
			it = map_pk_glove_item_group_.find(chr->race_num());

			if (it != map_pk_glove_item_group_.end())
			{
				__typeof(it->second->get_vector()) v = it->second->get_vector();

				for (uint32_t i = 0; i < v.size(); ++i)
				{
					int iPercent = (v[i].pct * iDeltaPercent) / 100;

					if (iPercent >= number(1, iRandRange))
					{
						uint32_t dwVnum = v[i].vnum;
						item = create_item(dwVnum, v[i].count, 0, true);
						if (item) vec_item.push_back(item);
					}
				}
			}
		}
	}
	
	// Junk items
	if (chr->mob_drop_item_vnum())
	{
		auto it = map_dw_etc_item_drop_prob_.find(chr->mob_drop_item_vnum());

		if (it != map_dw_etc_item_drop_prob_.end())
		{
			int iPercent = (it->second * iDeltaPercent) / 100;

			if (iPercent >= number(1, iRandRange))
			{
				item = create_item(chr->mob_drop_item_vnum(), 1, 0, true);
				if (item) vec_item.push_back(item);
			}
		}
	}

	if (chr->is_stone())
	{
		if (chr->drop_metin_stone_vnum())
		{
			int iPercent = (chr->drop_metin_stone_pct() * iDeltaPercent) * 400;

			if (iPercent >= number(1, iRandRange))
			{
				item = create_item(chr->drop_metin_stone_vnum(), 1, 0, true);
				if (item) vec_item.push_back(item);
			}
		}
	}

	if (killer->is_horse_riding() && 
			get_drop_per_kill_pct(1000, 1000000, iDeltaPercent, "horse_skill_book_drop") >= number(1, iRandRange))
	{
		metin2::log::info( "EVENT HORSE_SKILL_BOOK_DROP");

		if ((item = create_item(ITEM_HORSE_SKILL_TRAIN_BOOK, 1, 0, true)))
			vec_item.push_back(item);
	}


	if (get_drop_per_kill_pct(100, 1000, iDeltaPercent, "lotto_drop") >= number(1, iRandRange))
	{
		uint32_t * pdw = new uint32_t[3];

		pdw[0] = 50001;
		pdw[1] = 1;
		pdw[2] = quest::quest_manager::instance().event_flag("lotto_round");

		// Set the socket for the Book of Fortune
		db_manager::instance().return_query(QID_LOTTO, killer->player_id(), pdw,
				"INSERT INTO lotto_list VALUES(0, 'server%s', %u, NOW())",
				get_table_postfix(), killer->player_id());
	}

	//
	// Special drop items
	// 
	create_quest_drop_item(chr, killer, vec_item, iDeltaPercent, iRandRange);

	for (auto it = vec_item.begin(); it != vec_item.end(); ++it)
	{
		game::items::item * item = *it;
		db_manager::instance().send_money_log(MONEY_LOG_DROP, item->vnum(), item->count());
	}

	//
	// Water Dragon room entry ticket only on Seungryong Valley, Cave of Heaven floor 2
	//

	//
	// Skill books 7, 8 drop only on Seungryong Valley floors 1 and 2
	//

	return vec_item.size();
}

// ADD_GRANDMASTER_SKILL
int get_three_skill_level_adjust(int level)
{
	if (level < 40)
		return 32;
	if (level < 45)
		return 16;
	if (level < 50)
		return 8;
	if (level < 55)
		return 4;
	if (level < 60)
		return 2;
	return 1;
}
// END_OF_ADD_GRANDMASTER_SKILL

// DROPEVENT_CHARSTONE
// drop_char_stone 1
// drop_char_stone.percent_lv01_10 5
// drop_char_stone.percent_lv11_30 10
// drop_char_stone.percent_lv31_MX 15
// drop_char_stone.level_range	   10
static struct drop_event_char_stone
{
	int percent_lv01_10;
	int percent_lv11_30;
	int percent_lv31_mx;
	int level_range;
	bool alive;

	drop_event_char_stone()
	{
		percent_lv01_10 =  100;
		percent_lv11_30 =  200;
		percent_lv31_mx =  300;
		level_range = 10;
		alive = false;
	}
} gs_drop_event_char_stone;

static int drop_event_char_stone_get_drop_percent(int killer_level)
{
	int killer_levelStep = (killer_level-1)/10;

	switch (killer_levelStep)
	{
		case 0:
			return gs_drop_event_char_stone.percent_lv01_10;

		case 1:
		case 2:
			return gs_drop_event_char_stone.percent_lv11_30;
	}

	return gs_drop_event_char_stone.percent_lv31_mx;
}

static void drop_event_char_stone_drop_item(character & killer, character & victim, item_manager& item_mgr, std::vector<item *>& vec_item)
{
	if (!gs_drop_event_char_stone.alive)
		return;

	int killer_level = killer.level();
	int dropPercent = drop_event_char_stone_get_drop_percent(killer_level);

	int MaxRange = 10000;


	if (number(1, MaxRange) <= dropPercent)
	{
		int victim_level = victim.level();
		int level_diff = victim_level - killer_level;

		if (level_diff >= +gs_drop_event_char_stone.level_range || level_diff <= -gs_drop_event_char_stone.level_range)
		{
			metin2::log::debug( 
					"dropevent.drop_char_stone.level_range_over: killer({}: lv{:d}), victim({}: lv:{:d}), level_diff({:d})",
					killer.name(), killer.level(), victim.name(), victim.level(), level_diff);	
			return;
		}

		static const int Stones[] = { 30210, 30211, 30212, 30213, 30214, 30215, 30216, 30217, 30218, 30219, 30258, 30259, 30260, 30261, 30262, 30263 };
		int item_vnum = Stones[number(0, static_cast<int>(std::size(Stones)) - 1)];

		item * p_item = nullptr;

		if ((p_item = item_mgr.create_item(item_vnum, 1, 0, true)))
		{
			vec_item.push_back(p_item);

			metin2::log::debug( 
					"dropevent.drop_char_stone.item_drop: killer({}: lv{:d}), victim({}: lv:{:d}), item_name({})",
					killer.name(), killer.level(), victim.name(), victim.level(), p_item->name());	
		}
	}
}

bool drop_event_char_stone_set_value(const std::string& name, int value)
{
	if (name == "drop_char_stone")
	{
		gs_drop_event_char_stone.alive = value;

		if (value)
			metin2::log::info( "dropevent.drop_char_stone = on");
		else
			metin2::log::info( "dropevent.drop_char_stone = off");

	}
	else if (name == "drop_char_stone.percent_lv01_10")
		gs_drop_event_char_stone.percent_lv01_10 = value;
	else if (name == "drop_char_stone.percent_lv11_30")
		gs_drop_event_char_stone.percent_lv11_30 = value;
	else if (name == "drop_char_stone.percent_lv31_MX")
		gs_drop_event_char_stone.percent_lv31_mx = value;
	else if (name == "drop_char_stone.level_range")
		gs_drop_event_char_stone.level_range = value;
	else
		return false;

	metin2::log::info( "dropevent.drop_char_stone: {:d}", gs_drop_event_char_stone.alive ? true : false);
	metin2::log::info( "dropevent.drop_char_stone.percent_lv01_10: {:f}", gs_drop_event_char_stone.percent_lv01_10/100.0f);
	metin2::log::info( "dropevent.drop_char_stone.percent_lv11_30: {:f}", gs_drop_event_char_stone.percent_lv11_30/100.0f);
	metin2::log::info( "dropevent.drop_char_stone.percent_lv31_MX: {:f}", gs_drop_event_char_stone.percent_lv31_mx/100.0f);
	metin2::log::info( "dropevent.drop_char_stone.level_range: {:d}", gs_drop_event_char_stone.level_range);

	return true;
}

// END_OF_DROPEVENT_CHARSTONE

// fixme
// Let's move these into quests along with the above. 
// This is way too dirty...
// �?. I hate hardcoding T_T
// Improved item reward start.
// by rtsummit really need to fix this
static struct drop_event_refine_box
{
	int percent_low;
	int low;
	int percent_mid;
	int mid;
	int percent_high;
	bool alive;

	drop_event_refine_box()
	{
		percent_low =  100;
		low = 20;
		percent_mid =  100;
		mid = 45;
		percent_high =  100;
		alive = false;
	}
} gs_drop_event_refine_box;

static item * drop_event_refine_box_get_drop_item(character & killer, character & victim, item_manager& item_mgr)
{
	static const int lowerBox[] = { 50197, 50198, 50199 };
	static const int lowerBox_range = 3;
	static const int midderBox[] = { 50203, 50204, 50205, 50206 };
	static const int midderBox_range = 4;
	static const int higherBox[] = { 50207, 50208, 50209, 50210, 50211 };
	static const int higherBox_range = 5;

	if (victim.get_mob_rank() < MOB_RANK_KNIGHT)
		return nullptr;

	int killer_level = killer.level();

	if (killer_level <= gs_drop_event_refine_box.low)
	{
		if (number (1, gs_drop_event_refine_box.percent_low) == 1)
		{
			return item_mgr.create_item(lowerBox [number (1,lowerBox_range) - 1], 1, 0, true);
		}
	}
	else if (killer_level <= gs_drop_event_refine_box.mid)
	{
		if (number (1, gs_drop_event_refine_box.percent_mid) == 1)
		{
			return item_mgr.create_item(midderBox [number (1,midderBox_range) - 1], 1, 0, true);
		}
	}
	else
	{
		if (number (1, gs_drop_event_refine_box.percent_high) == 1)
		{
			return item_mgr.create_item(higherBox [number (1,higherBox_range) - 1], 1, 0, true);
		}
	}
	return nullptr;
}

static void drop_event_refine_box_drop_item(character & killer, character & victim, item_manager& item_mgr, std::vector<item *>& vec_item)
{
	if (!gs_drop_event_refine_box.alive)
		return;


	item * p_item = drop_event_refine_box_get_drop_item(killer, victim, item_mgr);

	if (p_item)
	{
		vec_item.push_back(p_item);

		metin2::log::debug( 
			"dropevent.drop_refine_box.item_drop: killer({}: lv{:d}), victim({}: lv:{:d}), item_name({})",
			killer.name(), killer.level(), victim.name(), victim.level(), p_item->name());	
	}
}

bool drop_event_refine_box_set_value(const std::string& name, int value)
{
	if (name == "refine_box_drop")
	{
		gs_drop_event_refine_box.alive = value;

		if (value)
			metin2::log::info( "refine_box_drop = on");
		else
			metin2::log::info( "refine_box_drop = off");

	}
	else if (name == "refine_box_low")
		gs_drop_event_refine_box.percent_low = value < 100 ? 100 : value;
	else if (name == "refine_box_mid")
		gs_drop_event_refine_box.percent_mid = value < 100 ? 100 : value;
	else if (name == "refine_box_high")
		gs_drop_event_refine_box.percent_high = value < 100 ? 100 : value;
	else
		return false;

	metin2::log::info( "refine_box_drop: {:d}", gs_drop_event_refine_box.alive ? true : false);
	metin2::log::info( "refine_box_low: {:d}", gs_drop_event_refine_box.percent_low);
	metin2::log::info( "refine_box_mid: {:d}", gs_drop_event_refine_box.percent_mid);
	metin2::log::info( "refine_box_high: {:d}", gs_drop_event_refine_box.percent_high);

	return true;
}
// Improved item reward end.


void item_manager::create_quest_drop_item(character * chr, character * killer, std::vector<item *> & vec_item, int delta_percent, int rand_range)
{
	item * item = nullptr;

	if (!chr)
		return;

	if (!killer)
		return;

	metin2::log::debug( "CreateQuestDropItem victim({}), killer({})", chr->name(), killer->name() );

	// DROPEVENT_CHARSTONE
	drop_event_char_stone_drop_item(*killer, *chr, *this, vec_item);
	// END_OF_DROPEVENT_CHARSTONE
	drop_event_refine_box_drop_item(*killer, *chr, *this, vec_item);

	// Christmas sock
	if (quest::quest_manager::instance().event_flag("xmas_sock"))
	{
		uint32_t	SOCK_ITEM_VNUM	= 0;
		SOCK_ITEM_VNUM = 50010;

		int iDropPerKill[MOB_RANK_MAX_NUM] =
		{
			2000,
			1000,
			300,
			50,
			0,
			0,
		};

		if ( iDropPerKill[chr->get_mob_rank()] != 0 )
		{
			int iPercent = 40000 * delta_percent / iDropPerKill[chr->get_mob_rank()];


			metin2::log::info( "SOCK DROP {:d} {:d}", iPercent, rand_range);
			if (iPercent >= number(1, rand_range))
			{
				if ((item = create_item(SOCK_ITEM_VNUM, 1, 0, true)))
					vec_item.push_back(item);
			}
		}
	}

	// Moonlight treasure box
	if (quest::quest_manager::instance().event_flag("drop_moon"))
	{
		const uint32_t ITEM_VNUM = 50011;

		int iDropPerKill[MOB_RANK_MAX_NUM] =
		{
			2000,
			1000,
			300,
			50,
			0,
			0,
		};

		if (iDropPerKill[chr->get_mob_rank()])
		{
			int iPercent = 40000 * delta_percent / iDropPerKill[chr->get_mob_rank()];

			if (iPercent >= number(1, rand_range))
			{
				if ((item = create_item(ITEM_VNUM, 1, 0, true)))
					vec_item.push_back(item);
			}
		}
	}

	if (killer->level() >= 15 && abs(killer->level() - chr->level()) <= 5)
	{
		int pct = quest::quest_manager::instance().event_flag("hc_drop");

		if (pct > 0)
		{
			const uint32_t ITEM_VNUM = 30178;

			if (number(1,100) <= pct)
			{
				if ((item = create_item(ITEM_VNUM, 1, 0, true)))
					vec_item.push_back(item);
			}
		}
	}

	//Hexagonal treasure box
	if (get_drop_per_kill_pct(100, 2000, delta_percent, "2006_drop") >= number(1, rand_range))
	{
		metin2::log::info( "육각보합 DROP EVENT ");

		const static uint32_t dwVnum = 50037;

		if ((item = create_item(dwVnum, 1, 0, true)))
			vec_item.push_back(item);

	}

	//Hexagonal treasure box+
	if (get_drop_per_kill_pct(100, 2000, delta_percent, "2007_drop") >= number(1, rand_range))
	{
		metin2::log::info( "육각보합 DROP EVENT ");

		const static uint32_t dwVnum = 50043;

		if ((item = create_item(dwVnum, 1, 0, true)))
			vec_item.push_back(item);
	}

	// New Year firecracker event
	if (get_drop_per_kill_pct(/* minimum */ 100, /* default */ 1000, delta_percent, "newyear_fire") >= number(1, rand_range))
	{
		// Firecrackers in China, spinning tops in Korea
		const uint32_t ITEM_VNUM_FIRE = 50107;

		if ((item = create_item(ITEM_VNUM_FIRE, 1, 0, true)))
			vec_item.push_back(item);
	}

	// New Year full-moon festival event
	if (get_drop_per_kill_pct(100, 500, delta_percent, "newyear_moon") >= number(1, rand_range))
	{
		metin2::log::info( "EVENT NEWYEAR_MOON DROP");

		const static uint32_t wonso_items[6] = { 50016, 50017, 50018, 50019, 50019, 50019, };
		uint32_t dwVnum = wonso_items[number(0,5)];

		if ((item = create_item(dwVnum, 1, 0, true)))
			vec_item.push_back(item);
	}

	// Valentine's Day event. Changed event minimum to 1 at OGE's request. (Other events left as they are for now.)
	if (get_drop_per_kill_pct(1, 2000, delta_percent, "valentine_drop") >= number(1, rand_range))
	{
		metin2::log::info( "EVENT VALENTINE_DROP");

		const static uint32_t valentine_items[2] = { 50024, 50025 };
		uint32_t dwVnum = valentine_items[number(0, 1)];

		if ((item = create_item(dwVnum, 1, 0, true)))
			vec_item.push_back(item);
	}

	// Ice cream event
	if (get_drop_per_kill_pct(100, 2000, delta_percent, "icecream_drop") >= number(1, rand_range))
	{
		const static uint32_t icecream = 50123;

		if ((item = create_item(icecream, 1, 0, true)))
			vec_item.push_back(item);
	}

	// new Christmas event
	// 53002 : baby reindeer summon ticket
	if ((killer->count_specify_item(53002) > 0) && (get_drop_per_kill_pct(50, 100, delta_percent, "new_xmas_event") >= number(1, rand_range)))
	{
		const static uint32_t xmas_sock = 50010;
		killer->auto_give_item (xmas_sock, 1);
	}

	if ((killer->count_specify_item(53007) > 0) && (get_drop_per_kill_pct(50, 100, delta_percent, "new_xmas_event") >= number(1, rand_range)))
	{
		const static uint32_t xmas_sock = 50010;
		killer->auto_give_item (xmas_sock, 1);
	}

	if ( get_drop_per_kill_pct(100, 2000, delta_percent, "halloween_drop") >= number(1, rand_range) )
	{
		const static uint32_t halloween_item = 30321;

		if ( (item=create_item(halloween_item, 1, 0, true)) )
			vec_item.push_back(item);
	}
	
	if ( get_drop_per_kill_pct(100, 2000, delta_percent, "ramadan_drop") >= number(1, rand_range) )
	{
		const static uint32_t ramadan_item = 30315;

		if ( (item=create_item(ramadan_item, 1, 0, true)) )
			vec_item.push_back(item);
	}

	if ( get_drop_per_kill_pct(100, 2000, delta_percent, "easter_drop") >= number(1, rand_range) )
	{
		const static uint32_t easter_item_base = 50160;

		if ( (item=create_item(easter_item_base+number(0,19), 1, 0, true)) )
			vec_item.push_back(item);
	}

	// World Cup event
	if ( get_drop_per_kill_pct(100, 2000, delta_percent, "football_drop") >= number(1, rand_range) )
	{
		const static uint32_t football_item = 50096;

		if ( (item=create_item(football_item, 1, 0, true)) )
			vec_item.push_back(item);
	}

	// White Day event
	if (get_drop_per_kill_pct(100, 2000, delta_percent, "whiteday_drop") >= number(1, rand_range))
	{
		metin2::log::info( "EVENT WHITEDAY_DROP");
		const static uint32_t whiteday_items[2] = { ITEM_WHITEDAY_ROSE, ITEM_WHITEDAY_CANDY };
		uint32_t dwVnum = whiteday_items[number(0,1)];

		if ((item = create_item(dwVnum, 1, 0, true)))
			vec_item.push_back(item);
	}

	// Children's Day mystery box event
	if (killer->level()>=50)
	{
		if (get_drop_per_kill_pct(100, 1000, delta_percent, "kids_day_drop_high") >= number(1, rand_range))
		{
			uint32_t ITEM_QUIZ_BOX = 50034;

			if ((item = create_item(ITEM_QUIZ_BOX, 1, 0, true)))
				vec_item.push_back(item);
		}
	}
	else
	{
		if (get_drop_per_kill_pct(100, 1000, delta_percent, "kids_day_drop") >= number(1, rand_range))
		{
			uint32_t ITEM_QUIZ_BOX = 50034;

			if ((item = create_item(ITEM_QUIZ_BOX, 1, 0, true)))
				vec_item.push_back(item);
		}
	}

	// Olympic drop event
	if (chr->level() >= 30 && get_drop_per_kill_pct(50, 100, delta_percent, "medal_part_drop") >= number(1, rand_range))
	{
		const static uint32_t drop_items[] = { 30265, 30266, 30267, 30268, 30269 };
		int i = number (0, 4);
		item = create_item(drop_items[i]);
		if (item != nullptr)
			vec_item.push_back(item);
	}

	// ADD_GRANDMASTER_SKILL
	// Soul stone item drop
	if (chr->level() >= 40 && chr->get_mob_rank() >= MOB_RANK_BOSS && get_drop_per_kill_pct(/* minimum */ 1, /* default */ 1000, delta_percent, "three_skill_item") / get_three_skill_level_adjust(chr->level()) >= number(1, rand_range))
	{
		const uint32_t ITEM_VNUM = 50513;

		if ((item = create_item(ITEM_VNUM, 1, 0, true)))
			vec_item.push_back(item);
	}
	// END_OF_ADD_GRANDMASTER_SKILL

	//
	// Seed item drop
	//
	if (get_drop_per_kill_pct(100, 1000, delta_percent, "dragon_boat_festival_drop") >= number(1, rand_range))
	{
		const uint32_t ITEM_SEED = 50085;

		if ((item = create_item(ITEM_SEED, 1, 0, true)))
			vec_item.push_back(item);
	}

	// Ten-thousand-year iron drop for the Blessing of the War God
	if (killer->level() >= 15 && quest::quest_manager::instance().event_flag("mars_drop"))
	{
		const uint32_t ITEM_HANIRON = 70035;
		int iDropMultiply[MOB_RANK_MAX_NUM] =
		{
			50,
			30,
			5,
			1,
			0,
			0,
		};

		if (iDropMultiply[chr->get_mob_rank()] &&
				get_drop_per_kill_pct(1000, 1500, delta_percent, "mars_drop") >= number(1, rand_range) * iDropMultiply[chr->get_mob_rank()])
		{
			if ((item = create_item(ITEM_HANIRON, 1, 0, true)))
				vec_item.push_back(item);
		}
	}
}

uint32_t item_manager::refine_from_vnum(uint32_t vnum)
{
	auto it = map_item_refine_from_.find(vnum);
	if (it != map_item_refine_from_.end())
		return it->second;
	return 0;
}

const special_item_group* item_manager::get_special_item_group(uint32_t vnum)
{
	auto it = map_pk_special_item_group_.find(vnum);
	if (it != map_pk_special_item_group_.end())
	{
		return it->second;
	}
	return nullptr;
}

const special_attr_group* item_manager::get_special_attr_group(uint32_t vnum)
{
	auto it = map_pk_special_attr_group_.find(vnum);
	if (it != map_pk_special_attr_group_.end())
	{
		return it->second;
	}
	return nullptr;
}

uint32_t item_manager::mask_vnum(uint32_t vnum)
{
	map_dw2_dw::iterator it = map_new_to_ori_.find (vnum);
	if (it != map_new_to_ori_.end())
	{
		return it->second;
	}
	else
		return 0;
}

// Copies all attributes and socket values to pkNewItem.
// Copied as-is from TransformRefineItem, a local function formerly in char_item.cpp
void item_manager::copy_all_attr_to(item * old_item, item * new_item)
{
	// ACCESSORY_REFINE
	if (old_item->is_accessory_for_socket())
	{
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			new_item->set_socket(i, old_item->get_socket(i));
		}
	}
	// END_OF_ACCESSORY_REFINE
	else
	{
		// Broken stones are cleaned up automatically here
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			if (!old_item->get_socket(i))
				break;
			else
				new_item->set_socket(i, 1);
		}

		// Set sockets
		int slot = 0;

		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			int32_t socket = old_item->get_socket(i);
			const int ITEM_BROKEN_METIN_VNUM = 28960; // Why is the same constant in 3 places... just make it one T_T I'm passing, Hong will do it
			if (socket > 2 && socket != ITEM_BROKEN_METIN_VNUM)
				new_item->set_socket(slot++, socket);
		}

	}

	// Set magic item
	old_item->copy_attribute_to(new_item);
}
}
