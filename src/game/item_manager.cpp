#include "item_manager.h"

#include "blend_item.h"
#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "constants.h"
#include "db.h"
#include "desc_client.h"
#include "DragonSoul.h"
#include "item.h"
#include "item_manager_private_types.h"
#include "log.h"
#include "priv_manager.h"
#include "questmanager.h"
#include "safebox.h"
#include "skill.h"
#include "unique_item.h"
#include "utils.h"

#include <common/length.h>
#include <common/tables.h>
#include <libthecore/log.h>
#include <libthecore/utils.h>

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

ITEM_MANAGER::ITEM_MANAGER()
	: m_dwVIDCount(0), m_dwCurrentID(0)
{
	m_ItemIDRange.dwMin = m_ItemIDRange.dwMax = m_ItemIDRange.dwUsableItemIDMin = 0;
	m_ItemIDSpareRange.dwMin = m_ItemIDSpareRange.dwMax = m_ItemIDSpareRange.dwUsableItemIDMin = 0;
}

ITEM_MANAGER::~ITEM_MANAGER()
{
	Destroy();
}

void ITEM_MANAGER::Destroy()
{
	auto it = m_VIDMap.begin();
	for ( ; it != m_VIDMap.end(); ++it) {
		delete it->second;
	}
	m_VIDMap.clear();
}

void ITEM_MANAGER::GracefulShutdown()
{
	std::unordered_set<CItem *>::iterator it = m_set_pkItemForDelayedSave.begin();

	while (it != m_set_pkItemForDelayedSave.end())
		SaveSingleItem(*(it++));

	m_set_pkItemForDelayedSave.clear();
}

bool ITEM_MANAGER::Initialize(TItemTable * table, int size)
{
	if (!m_vec_prototype.empty())
		m_vec_prototype.clear();

	int	i;

	m_vec_prototype.resize(size);
	std::memcpy(&m_vec_prototype[0], table, sizeof(TItemTable) * size);
	for (int i = 0; i < size; i++)
	{
		if (0 != m_vec_prototype[i].dwVnumRange)
		{
			m_vec_item_vnum_range_info.push_back( &m_vec_prototype[i]);
		}
	}

	m_map_ItemRefineFrom.clear();
	for (i = 0; i < size; ++i)
	{

		if (m_vec_prototype[i].dwRefinedVnum)
			m_map_ItemRefineFrom.insert(std::make_pair(m_vec_prototype[i].dwRefinedVnum, m_vec_prototype[i].dwVnum));

		// NOTE : the QUEST_GIVE flag is triggered by an npc event.
		if (m_vec_prototype[i].bType == ITEM_QUEST || IS_SET(m_vec_prototype[i].dwFlags, ITEM_FLAG_QUEST_USE | ITEM_FLAG_QUEST_USE_MULTIPLE))
			quest::CQuestManager::instance().RegisterNPCVnum(m_vec_prototype[i].dwVnum);

		m_map_vid.insert( std::map<uint32_t,TItemTable>::value_type( m_vec_prototype[i].dwVnum, m_vec_prototype[i] ) ); 
		if ( test_server )
			sys_log( 0, "ITEM_INFO %d %s ", m_vec_prototype[i].dwVnum, m_vec_prototype[i].szName );	
	}

	int len = 0, len2;
	char buf[512];

	for (i = 0; i < size; ++i)
	{
		len2 = snprintf(buf + len, sizeof(buf) - len, "%5u %-16s", m_vec_prototype[i].dwVnum, m_vec_prototype[i].szLocaleName);

		if (len2 < 0 || len2 >= (int) sizeof(buf) - len)
			len += (sizeof(buf) - len) - 1;
		else
			len += len2;

		if (!((i + 1) % 4))
		{
			if ( !test_server )
				sys_log(0, "%s", buf);
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
		if ( !test_server )
			sys_log(0, "%s", buf);
	}

	ITEM_VID_MAP::iterator it = m_VIDMap.begin();

	sys_log (1, "ITEM_VID_MAP %zu", m_VIDMap.size() );

	while (it != m_VIDMap.end())
	{
		CItem * item = it->second;
		++it;

		const TItemTable* tableInfo = GetTable(item->GetOriginalVnum());

		if (nullptr == tableInfo)
		{
			sys_err("cannot reset item table");
			item->SetProto(nullptr);
		}

		item->SetProto(tableInfo);
	}

	return true;
}

CItem * ITEM_MANAGER::CreateItem(uint32_t vnum, uint32_t count, uint32_t id, bool bTryMagic, int iRarePct, bool bSkipSave)
{
	if (0 == vnum)
		return nullptr;

	uint32_t dwMaskVnum = 0;
	if (GetMaskVnum(vnum))
	{
		dwMaskVnum = GetMaskVnum(vnum);
	}


	const TItemTable* table = GetTable(vnum);

	if (nullptr == table)
		return nullptr;

	CItem * item = nullptr;

	//if it exists when checked by id -- return! 
	if (m_map_pkItemByID.find(id) != m_map_pkItemByID.end())
	{
		item = m_map_pkItemByID[id];
		CHARACTER * owner = item->GetOwner();
		sys_err("ITEM_ID_DUP: %u %s owner %p", id, item->GetName(), owner);
		return nullptr;
	}

	//allocate one item
	item = new CItem(vnum);

	bool bIsNewItem = (0 == id);

	//initialize it. set the table
	item->Initialize();
	item->SetProto(table);
	item->SetMaskVnum(dwMaskVnum);

	if (item->GetType() == ITEM_ELK) // Gold needs no ID and no saving.
		item->SetSkipSave(true);

	// Set the Unique ID
	else if (!bIsNewItem)
	{
		item->SetID(id);
		item->SetSkipSave(true);
	}
	else
	{
		item->SetID(GetNewID());

		if (item->GetType() == ITEM_UNIQUE) // Unique items record the remaining time in a socket on creation.
		{
			if (item->GetValue(2) == 0)
				item->SetSocket(ITEM_SOCKET_UNIQUE_REMAIN_TIME, item->GetValue(0)); // Game-time unique
			else
			{
				item->SetSocket(ITEM_SOCKET_UNIQUE_REMAIN_TIME, get_global_time() + item->GetValue(0)); // Real-time unique
			}
		}
	}


	switch (item->GetVnum())
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
				item->SetSocket(2, item->GetValue(0), true);
			else
				item->SetSocket(2, item->GetValue(0), false);
			break;
	}

	if (item->GetType() == ITEM_ELK) // Gold needs no processing
		;
	else if (item->IsStackable())  // For stackable items
	{
		count = std::clamp<int>(count, 1, ITEM_MAX_COUNT);

		if (bTryMagic && count <= 1 && IS_SET(item->GetFlag(), ITEM_FLAG_MAKECOUNT))
			count = item->GetValue(1);
	}
	else
		count = 1;

	item->SetVID(++m_dwVIDCount);

	if (bSkipSave == false)
		m_VIDMap.insert(ITEM_VID_MAP::value_type(item->GetVID(), item));

	if (item->GetID() != 0 && bSkipSave == false)
		m_map_pkItemByID.insert(std::map<uint32_t, CItem *>::value_type(item->GetID(), item));

	if (!item->SetCount(count))
		return nullptr;

	item->SetSkipSave(false);

	if (item->GetType() == ITEM_UNIQUE && item->GetValue(2) != 0)
		item->StartUniqueExpireEvent();

	for (int i=0 ; i < ITEM_LIMIT_MAX_NUM ; i++)
	{
		// Time is deducted from creation even if unused
		if (LIMIT_REAL_TIME == item->GetLimitType(i))
		{
			if (item->GetLimitValue(i))
			{
				item->SetSocket(0, time(nullptr) + item->GetLimitValue(i)); 
			}
			else
			{
				item->SetSocket(0, time(nullptr) + 60*60*24*7); 
			}

			item->StartRealTimeExpireEvent();
		}

		// Like existing unique items, usable time is deducted only while equipped
		else if (LIMIT_TIMER_BASED_ON_WEAR == item->GetLimitType(i))
		{
			// If the item is already equipped, start the timer; for newly created items, set the usable time. (
			// When granted through the item mall, Socket0 must be set before entering this logic.
			if (true == item->IsEquipped())
			{
				item->StartTimerBasedOnWearExpireEvent();
			}
			else if(0 == id)
			{
				int32_t duration = item->GetSocket(0);
				if (0 == duration)
					duration = item->GetLimitValue(i);

				if (0 == duration)
					duration = 60 * 60 * 10;	// If there is no info, default to 10 hours

				item->SetSocket(0, duration);
			}
		}
	}

	if (id == 0) // Only for newly created items
	{
		// Newly added herbs get different stats
		if (ITEM_BLEND==item->GetType())
		{
			if (Blend_Item_find(item->GetVnum()))
			{
				Blend_Item_set_value(item);
				return item;
			}
		}

		if (table->sAddonType)
		{
			item->ApplyAddon(table->sAddonType);
		}

		if (bTryMagic)
		{
			if (iRarePct == -1)
				iRarePct = table->bAlterToMagicItemPct;

			if (number(1, 100) <= iRarePct)
				item->AlterToMagicItem();
		}

		if (table->bGainSocketPct)
			item->AlterToSocketItem(table->bGainSocketPct);

		// 50300 == skill book
		if (vnum == 50300 || vnum == ITEM_SKILLFORGET_VNUM)
		{
			uint32_t dwSkillVnum;

			do
			{
				dwSkillVnum = number(1, 111);

				if (nullptr != CSkillManager::instance().Get(dwSkillVnum))
					break;
			} while (true);

			item->SetSocket(0, dwSkillVnum);
		}
		else if (ITEM_SKILLFORGET2_VNUM == vnum)
		{
			uint32_t dwSkillVnum;

			do
			{
				dwSkillVnum = number(112, 119);

				if (nullptr != CSkillManager::instance().Get(dwSkillVnum))
					break;
			} while (true);

			item->SetSocket(0, dwSkillVnum);
		}
	}

	if (item->GetType() == ITEM_QUEST)
	{
		for (auto it = m_map_pkQuestItemGroup.begin(); it != m_map_pkQuestItemGroup.end(); it++)
		{
			if (it->second->m_bType == CSpecialItemGroup::QUEST && it->second->Contains(vnum))
			{
				item->SetSIGVnum(it->first);
			}
		}
	}
	else if (item->GetType() == ITEM_UNIQUE)
	{
		for (auto it = m_map_pkSpecialItemGroup.begin(); it != m_map_pkSpecialItemGroup.end(); it++)
		{
			if (it->second->m_bType == CSpecialItemGroup::SPECIAL && it->second->Contains(vnum))
			{
				item->SetSIGVnum(it->first);
			}
		}
	}

	// Handle newly created dragon soul stones.
	if (item->IsDragonSoul() && 0 == id)
	{
		DSManager::instance().DragonSoulItemInitialize(item);
	}
	return item;
}

void ITEM_MANAGER::DelayedSave(CItem * item)
{
	if (item->GetID() != 0)
		m_set_pkItemForDelayedSave.insert(item);
}

void ITEM_MANAGER::FlushDelayedSave(CItem * item)
{
	std::unordered_set<CItem *>::iterator it = m_set_pkItemForDelayedSave.find(item);

	if (it == m_set_pkItemForDelayedSave.end())
	{
		return;
	}

	m_set_pkItemForDelayedSave.erase(it);
	SaveSingleItem(item);
}

void ITEM_MANAGER::SaveSingleItem(CItem * item)
{
	if (!item->GetOwner())
	{
		uint32_t dwID = item->GetID();
		uint32_t dwOwnerID = item->GetLastOwnerPID();

		db_clientdesc->DBPacketHeader(HEADER_GD_ITEM_DESTROY, 0, sizeof(uint32_t) + sizeof(uint32_t));
		db_clientdesc->Packet(&dwID, sizeof(uint32_t));
		db_clientdesc->Packet(&dwOwnerID, sizeof(uint32_t));

		sys_log(1, "ITEM_DELETE %s:%u", item->GetName(), dwID);
		return;
	}

	sys_log(1, "ITEM_SAVE %s:%d in %s window %d", item->GetName(), item->GetID(), item->GetOwner()->GetName(), item->GetWindow());

	TPlayerItem t;

	t.id = item->GetID();
	t.window = item->GetWindow();
	t.pos = t.window == EQUIPMENT ? item->GetCell() - INVENTORY_MAX_NUM : item->GetCell();
	t.count = item->GetCount();
	t.vnum = item->GetOriginalVnum();
	t.owner = (t.window == SAFEBOX || t.window == MALL) ? item->GetOwner()->GetDesc()->GetAccountTable().id : item->GetOwner()->GetPlayerID();
	std::memcpy(t.alSockets, item->GetSockets(), sizeof(t.alSockets));
	std::memcpy(t.aAttr, item->GetAttributes(), sizeof(t.aAttr));

	db_clientdesc->DBPacketHeader(HEADER_GD_ITEM_SAVE, 0, sizeof(TPlayerItem));
	db_clientdesc->Packet(&t, sizeof(TPlayerItem));
}

void ITEM_MANAGER::Update()
{
	std::unordered_set<CItem *>::iterator it = m_set_pkItemForDelayedSave.begin();
	std::unordered_set<CItem *>::iterator this_it;

	while (it != m_set_pkItemForDelayedSave.end())
	{
		this_it = it++;
		CItem * item = *this_it;

		// Items with the SLOW_QUERY flag are saved only on shutdown.
		if (item->GetOwner() && IS_SET(item->GetFlag(), ITEM_FLAG_SLOW_QUERY))
			continue;

		SaveSingleItem(item);

		m_set_pkItemForDelayedSave.erase(this_it);
	}
}

void ITEM_MANAGER::RemoveItem(CItem * item, const char * c_pszReason)
{
	CHARACTER * o;

	if ((o = item->GetOwner()))
	{
		char szHint[64];
		snprintf(szHint, sizeof(szHint), "%s %u ", item->GetName(), item->GetCount());
		LogManager::instance().ItemLog(o, item, c_pszReason ? c_pszReason : "REMOVE", szHint);

		// SAFEBOX_TIME_LIMIT_ITEM_BUG_FIX
		if (item->GetWindow() == MALL || item->GetWindow() == SAFEBOX)
		{
			// 20050613.ipkn.If a timed item is in a shop, the server crashes when it expires.
			CSafebox* pSafebox = item->GetWindow() == MALL ? o->GetMall() : o->GetSafebox();
			if (pSafebox)
			{
				pSafebox->Remove(item->GetCell());
			}
		}
		// END_OF_SAFEBOX_TIME_LIMIT_ITEM_BUG_FIX
		else
		{
			o->SyncQuickslot(QUICKSLOT_TYPE_ITEM, item->GetCell(), 255);
			item->RemoveFromCharacter();
		}
	}

	ITEM_MANAGER::instance().DestroyItem(item);
}

void ITEM_MANAGER::DestroyItem(CItem * item)
{
	if (item->GetSectree())
		item->RemoveFromGround();

	if (item->GetOwner())
	{
		if (CHARACTER_MANAGER::instance().Find(item->GetOwner()->GetPlayerID()) != nullptr)
		{
			sys_err("DestroyItem: GetOwner %s %s!!", item->GetName(), item->GetOwner()->GetName());
			item->RemoveFromCharacter();
		}
		else
		{
			sys_err ("WTH! Invalid item owner. owner pointer : %p", item->GetOwner());
		}
	}

	std::unordered_set<CItem *>::iterator it = m_set_pkItemForDelayedSave.find(item);

	if (it != m_set_pkItemForDelayedSave.end())
		m_set_pkItemForDelayedSave.erase(it);

	uint32_t dwID = item->GetID();
	sys_log(2, "ITEM_DESTROY %s:%u", item->GetName(), dwID);

	if (!item->GetSkipSave() && dwID)
	{
		uint32_t dwOwnerID = item->GetLastOwnerPID();

		db_clientdesc->DBPacketHeader(HEADER_GD_ITEM_DESTROY, 0, sizeof(uint32_t) + sizeof(uint32_t));
		db_clientdesc->Packet(&dwID, sizeof(uint32_t));
		db_clientdesc->Packet(&dwOwnerID, sizeof(uint32_t));
	}
	else
	{
		sys_log(2, "ITEM_DESTROY_SKIP %s:%u (skip=%d)", item->GetName(), dwID, item->GetSkipSave());
	}

	if (dwID)
		m_map_pkItemByID.erase(dwID);

	m_VIDMap.erase(item->GetVID());

	delete item;
}

CItem * ITEM_MANAGER::Find(uint32_t id)
{
	auto it = m_map_pkItemByID.find(id);
	if (it == m_map_pkItemByID.end())
		return nullptr;
	return it->second;
}

CItem * ITEM_MANAGER::FindByVID(uint32_t vid)
{
	ITEM_VID_MAP::iterator it = m_VIDMap.find(vid);

	if (it == m_VIDMap.end())
		return nullptr;

	return (it->second);
}

TItemTable * ITEM_MANAGER::GetTable(uint32_t vnum)
{
	int rnum = RealNumber(vnum);

	if (rnum < 0)
	{
		for (int i = 0; i < m_vec_item_vnum_range_info.size(); i++)
		{
			TItemTable* p = m_vec_item_vnum_range_info[i];
			if ((p->dwVnum < vnum) &&
				vnum < (p->dwVnum + p->dwVnumRange))
			{
				return p;
			}
		}
			
		return nullptr;
	}

	return &m_vec_prototype[rnum];
}

int ITEM_MANAGER::RealNumber(uint32_t vnum)
{
	int bot, top, mid;

	bot = 0;
	top = m_vec_prototype.size();

	TItemTable * pTable = &m_vec_prototype[0];

	while (1)
	{
		mid = (bot + top) >> 1;

		if ((pTable + mid)->dwVnum == vnum)
			return (mid);

		if (bot >= top)
			return (-1);

		if ((pTable + mid)->dwVnum > vnum)
			top = mid - 1;
		else        
			bot = mid + 1;
	}
}

bool ITEM_MANAGER::GetVnum(const char * c_pszName, uint32_t & r_dwVnum)
{
	int len = strlen(c_pszName);

	TItemTable * pTable = &m_vec_prototype[0];

	for (uint32_t i = 0; i < m_vec_prototype.size(); ++i, ++pTable)
	{
		if (!strncasecmp(c_pszName, pTable->szLocaleName, len))
		{
			r_dwVnum = pTable->dwVnum;
			return true;
		}
	}

	return false;
}

bool ITEM_MANAGER::GetVnumByOriginalName(const char * c_pszName, uint32_t & r_dwVnum)
{
	int len = strlen(c_pszName);

	TItemTable * pTable = &m_vec_prototype[0];

	for (uint32_t i = 0; i < m_vec_prototype.size(); ++i, ++pTable)
	{
		if (!strncasecmp(c_pszName, pTable->szName, len))
		{
			r_dwVnum = pTable->dwVnum;
			return true;
		}
	}

	return false;
}

std::set<uint32_t> g_set_lotto;


// 20050503.ipkn.
// If less than iMinimum, set iDefault (iMinimum must be greater than 0)
// Exists to support ON/OFF switching as 1, 0
int GetDropPerKillPct(int iMinimum, int iDefault, int iDeltaPercent, const char * c_pszFlag)
{
	int iVal = 0;

	if ((iVal = quest::CQuestManager::instance().GetEventFlag(c_pszFlag)))
	{
		if (!test_server)
		{
			if (iVal < iMinimum)
				iVal = iDefault;

			if (iVal < 0)
				iVal = iDefault;
		}
	}

	if (iVal == 0)
		return 0;

	// Default setting (iDeltaPercent=100) 
	// Constant meant to give a feel of one per 40000 iVal mobs
	return (40000 * iDeltaPercent / iVal);
}

bool ITEM_MANAGER::GetDropPct(CHARACTER * pkChr, CHARACTER * pkKiller, int& iDeltaPercent, int& iRandRange)
{
 	if (nullptr == pkChr || nullptr == pkKiller)
		return false;

	int iLevel = pkKiller->GetLevel();
	iDeltaPercent = 100;

	if (!pkChr->IsStone() && pkChr->GetMobRank() >= MOB_RANK_BOSS)
		iDeltaPercent = PERCENT_LVDELTA_BOSS(pkKiller->GetLevel(), pkChr->GetLevel());
	else
		iDeltaPercent = PERCENT_LVDELTA(pkKiller->GetLevel(), pkChr->GetLevel());

	uint8_t bRank = pkChr->GetMobRank();

	if (1 == number(1, 50000))
		iDeltaPercent += 1000;
	else if (1 == number(1, 10000))
		iDeltaPercent += 500;

	sys_log(3, "CreateDropItem for level: %d rank: %u pct: %d", iLevel, bRank, iDeltaPercent);
	iDeltaPercent = iDeltaPercent * CHARACTER_MANAGER::instance().GetMobItemRate(pkKiller) / 100;

	// ADD_PREMIUM
	if (pkKiller->GetPremiumRemainSeconds(PREMIUM_ITEM) > 0 ||
			pkKiller->IsEquipUniqueGroup(UNIQUE_GROUP_DOUBLE_ITEM))
		iDeltaPercent += iDeltaPercent;
	// END_OF_ADD_PREMIUM

	iRandRange = 4000000;
	iRandRange = iRandRange * 100 / 
		(100 +
		 CPrivManager::instance().GetPriv(pkKiller, PRIV_ITEM_DROP) +
		 (pkKiller->IsEquipUniqueItem(UNIQUE_ITEM_DOUBLE_ITEM) ? 100 : 0));

	if (distribution_test_server) iRandRange /= 3;

	return true;
}

bool ITEM_MANAGER::CreateDropItem(CHARACTER * pkChr, CHARACTER * pkKiller, std::vector<CItem *> & vec_item)
{
	int iLevel = pkKiller->GetLevel();

	int iDeltaPercent, iRandRange;
	if (!GetDropPct(pkChr, pkKiller, iDeltaPercent, iRandRange))
		return false;

	uint8_t bRank = pkChr->GetMobRank();
	CItem * item = nullptr;

	// Common Drop Items
	std::vector<CItemDropInfo>::iterator it = g_vec_pkCommonDropItem[bRank].begin();

	while (it != g_vec_pkCommonDropItem[bRank].end())
	{
		const CItemDropInfo & c_rInfo = *(it++);

		if (iLevel < c_rInfo.m_iLevelStart || iLevel > c_rInfo.m_iLevelEnd)
			continue;

		int iPercent = (c_rInfo.m_iPercent * iDeltaPercent) / 100;
		sys_log(3, "CreateDropItem %d ~ %d %u %d(%d)", c_rInfo.m_iLevelStart, c_rInfo.m_iLevelEnd, c_rInfo.m_dwVnum, iPercent, c_rInfo.m_iPercent);

		if (iPercent >= number(1, iRandRange))
		{
			TItemTable * table = GetTable(c_rInfo.m_dwVnum);

			if (!table)
				continue;

			item = nullptr;

			if (table->bType == ITEM_POLYMORPH)
			{
				if (c_rInfo.m_dwVnum == pkChr->GetPolymorphItemVnum())
				{
					item = CreateItem(c_rInfo.m_dwVnum, 1, 0, true);

					if (item)
						item->SetSocket(0, pkChr->GetRaceNum());
				}
			}
			else
				item = CreateItem(c_rInfo.m_dwVnum, 1, 0, true);

			if (item) vec_item.push_back(item);
		}
	}

	// Drop Item Group
	{
		decltype(m_map_pkDropItemGroup.begin()) it;
		it = m_map_pkDropItemGroup.find(pkChr->GetRaceNum());

		if (it != m_map_pkDropItemGroup.end())
		{
			__typeof(it->second->GetVector()) v = it->second->GetVector();

			for (uint32_t i = 0; i < v.size(); ++i)
			{
				int iPercent = (v[i].dwPct * iDeltaPercent) / 100;

				if (iPercent >= number(1, iRandRange))
				{
					item = CreateItem(v[i].dwVnum, v[i].iCount, 0, true);

					if (item)
					{
						if (item->GetType() == ITEM_POLYMORPH)
						{
							if (item->GetVnum() == pkChr->GetPolymorphItemVnum())
							{
								item->SetSocket(0, pkChr->GetRaceNum());
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
		decltype(m_map_pkMobItemGroup.begin()) it;
		it = m_map_pkMobItemGroup.find(pkChr->GetRaceNum());

		if ( it != m_map_pkMobItemGroup.end() )
		{
			CMobItemGroup* pGroup = it->second;

			// MOB_DROP_ITEM_BUG_FIX
			// 20050805.myevan.Fixed problem accessing CMobItemGroup::GetOne() when MobDropItem has no items
			if (pGroup && !pGroup->IsEmpty())
			{
				int iPercent = 40000 * iDeltaPercent / pGroup->GetKillPerDrop();
				if (iPercent >= number(1, iRandRange))
				{
					const CMobItemGroup::SMobItemGroupInfo& info = pGroup->GetOne();
					item = CreateItem(info.dwItemVnum, info.iCount, 0, true, info.iRarePct);

					if (item) vec_item.push_back(item);
				}
			}
			// END_OF_MOB_DROP_ITEM_BUG_FIX
		}
	}

	// Level Item Group
	{
		decltype(m_map_pkLevelItemGroup.begin()) it;
		it = m_map_pkLevelItemGroup.find(pkChr->GetRaceNum());

		if ( it != m_map_pkLevelItemGroup.end() )
		{
			if ( it->second->GetLevelLimit() <= (uint32_t)iLevel )
			{
				__typeof(it->second->GetVector()) v = it->second->GetVector();

				for ( uint32_t i=0; i < v.size(); i++ )
				{
					if ( v[i].dwPct >= (uint32_t)number(1, 1000000/*iRandRange*/) )
					{
						uint32_t dwVnum = v[i].dwVNum;
						item = CreateItem(dwVnum, v[i].iCount, 0, true);
						if ( item ) vec_item.push_back(item);
					}
				}
			}
		}
	}
	
	// BuyerTheitGloves Item Group
	{
		if (pkKiller->GetPremiumRemainSeconds(PREMIUM_ITEM) > 0 ||
				pkKiller->IsEquipUniqueGroup(UNIQUE_GROUP_DOUBLE_ITEM))
		{
			decltype(m_map_pkGloveItemGroup.begin()) it;
			it = m_map_pkGloveItemGroup.find(pkChr->GetRaceNum());

			if (it != m_map_pkGloveItemGroup.end())
			{
				__typeof(it->second->GetVector()) v = it->second->GetVector();

				for (uint32_t i = 0; i < v.size(); ++i)
				{
					int iPercent = (v[i].dwPct * iDeltaPercent) / 100;

					if (iPercent >= number(1, iRandRange))
					{
						uint32_t dwVnum = v[i].dwVnum;
						item = CreateItem(dwVnum, v[i].iCount, 0, true);
						if (item) vec_item.push_back(item);
					}
				}
			}
		}
	}
	
	// Junk items
	if (pkChr->GetMobDropItemVnum())
	{
		auto it = m_map_dwEtcItemDropProb.find(pkChr->GetMobDropItemVnum());

		if (it != m_map_dwEtcItemDropProb.end())
		{
			int iPercent = (it->second * iDeltaPercent) / 100;

			if (iPercent >= number(1, iRandRange))
			{
				item = CreateItem(pkChr->GetMobDropItemVnum(), 1, 0, true);
				if (item) vec_item.push_back(item);
			}
		}
	}

	if (pkChr->IsStone())
	{
		if (pkChr->GetDropMetinStoneVnum())
		{
			int iPercent = (pkChr->GetDropMetinStonePct() * iDeltaPercent) * 400;

			if (iPercent >= number(1, iRandRange))
			{
				item = CreateItem(pkChr->GetDropMetinStoneVnum(), 1, 0, true);
				if (item) vec_item.push_back(item);
			}
		}
	}

	if (pkKiller->IsHorseRiding() && 
			GetDropPerKillPct(1000, 1000000, iDeltaPercent, "horse_skill_book_drop") >= number(1, iRandRange))
	{
		sys_log(0, "EVENT HORSE_SKILL_BOOK_DROP");

		if ((item = CreateItem(ITEM_HORSE_SKILL_TRAIN_BOOK, 1, 0, true)))
			vec_item.push_back(item);
	}


	if (GetDropPerKillPct(100, 1000, iDeltaPercent, "lotto_drop") >= number(1, iRandRange))
	{
		uint32_t * pdw = new uint32_t[3];

		pdw[0] = 50001;
		pdw[1] = 1;
		pdw[2] = quest::CQuestManager::instance().GetEventFlag("lotto_round");

		// Set the socket for the Book of Fortune
		DBManager::instance().ReturnQuery(QID_LOTTO, pkKiller->GetPlayerID(), pdw,
				"INSERT INTO lotto_list VALUES(0, 'server%s', %u, NOW())",
				get_table_postfix(), pkKiller->GetPlayerID());
	}

	//
	// Special drop items
	// 
	CreateQuestDropItem(pkChr, pkKiller, vec_item, iDeltaPercent, iRandRange);

	for (auto it = vec_item.begin(); it != vec_item.end(); ++it)
	{
		CItem * item = *it;
		DBManager::instance().SendMoneyLog(MONEY_LOG_DROP, item->GetVnum(), item->GetCount());
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
int GetThreeSkillLevelAdjust(int level)
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
static struct DropEvent_CharStone
{
	int percent_lv01_10;
	int percent_lv11_30;
	int percent_lv31_MX;
	int level_range;
	bool alive;

	DropEvent_CharStone()
	{
		percent_lv01_10 =  100;
		percent_lv11_30 =  200;
		percent_lv31_MX =  300;
		level_range = 10;
		alive = false;
	}
} gs_dropEvent_charStone;

static int __DropEvent_CharStone_GetDropPercent(int killer_level)
{
	int killer_levelStep = (killer_level-1)/10;

	switch (killer_levelStep)
	{
		case 0:
			return gs_dropEvent_charStone.percent_lv01_10;

		case 1:
		case 2:
			return gs_dropEvent_charStone.percent_lv11_30;
	}

	return gs_dropEvent_charStone.percent_lv31_MX;
}

static void __DropEvent_CharStone_DropItem(CHARACTER & killer, CHARACTER & victim, ITEM_MANAGER& itemMgr, std::vector<CItem *>& vec_item)
{
	if (!gs_dropEvent_charStone.alive)
		return;

	int killer_level = killer.GetLevel();
	int dropPercent = __DropEvent_CharStone_GetDropPercent(killer_level);

	int MaxRange = 10000;


	if (number(1, MaxRange) <= dropPercent)
	{
		int log_level = (test_server || killer.GetGMLevel() >= GM_LOW_WIZARD) ? 0 : 1;
		int victim_level = victim.GetLevel();
		int level_diff = victim_level - killer_level;

		if (level_diff >= +gs_dropEvent_charStone.level_range || level_diff <= -gs_dropEvent_charStone.level_range)
		{
			sys_log(log_level, 
					"dropevent.drop_char_stone.level_range_over: killer(%s: lv%d), victim(%s: lv:%d), level_diff(%d)",
					killer.GetName(), killer.GetLevel(), victim.GetName(), victim.GetLevel(), level_diff);	
			return;
		}

		static const int Stones[] = { 30210, 30211, 30212, 30213, 30214, 30215, 30216, 30217, 30218, 30219, 30258, 30259, 30260, 30261, 30262, 30263 };
		int item_vnum = Stones[number(0, static_cast<int>(std::size(Stones)) - 1)];

		CItem * p_item = nullptr;

		if ((p_item = itemMgr.CreateItem(item_vnum, 1, 0, true)))
		{
			vec_item.push_back(p_item);

			sys_log(log_level, 
					"dropevent.drop_char_stone.item_drop: killer(%s: lv%d), victim(%s: lv:%d), item_name(%s)",
					killer.GetName(), killer.GetLevel(), victim.GetName(), victim.GetLevel(), p_item->GetName());	
		}
	}
}

bool DropEvent_CharStone_SetValue(const std::string& name, int value)
{
	if (name == "drop_char_stone")
	{
		gs_dropEvent_charStone.alive = value;

		if (value)
			sys_log(0, "dropevent.drop_char_stone = on");
		else
			sys_log(0, "dropevent.drop_char_stone = off");

	}
	else if (name == "drop_char_stone.percent_lv01_10")
		gs_dropEvent_charStone.percent_lv01_10 = value;
	else if (name == "drop_char_stone.percent_lv11_30")
		gs_dropEvent_charStone.percent_lv11_30 = value;
	else if (name == "drop_char_stone.percent_lv31_MX")
		gs_dropEvent_charStone.percent_lv31_MX = value;
	else if (name == "drop_char_stone.level_range")
		gs_dropEvent_charStone.level_range = value;
	else
		return false;

	sys_log(0, "dropevent.drop_char_stone: %d", gs_dropEvent_charStone.alive ? true : false);
	sys_log(0, "dropevent.drop_char_stone.percent_lv01_10: %f", gs_dropEvent_charStone.percent_lv01_10/100.0f);
	sys_log(0, "dropevent.drop_char_stone.percent_lv11_30: %f", gs_dropEvent_charStone.percent_lv11_30/100.0f);
	sys_log(0, "dropevent.drop_char_stone.percent_lv31_MX: %f", gs_dropEvent_charStone.percent_lv31_MX/100.0f);
	sys_log(0, "dropevent.drop_char_stone.level_range: %d", gs_dropEvent_charStone.level_range);

	return true;
}

// END_OF_DROPEVENT_CHARSTONE

// fixme
// Let's move these into quests along with the above. 
// This is way too dirty...
// �?. I hate hardcoding T_T
// Improved item reward start.
// by rtsummit really need to fix this
static struct DropEvent_RefineBox
{
	int percent_low;
	int low;
	int percent_mid;
	int mid;
	int percent_high;
	bool alive;

	DropEvent_RefineBox()
	{
		percent_low =  100;
		low = 20;
		percent_mid =  100;
		mid = 45;
		percent_high =  100;
		alive = false;
	}
} gs_dropEvent_refineBox;

static CItem * __DropEvent_RefineBox_GetDropItem(CHARACTER & killer, CHARACTER & victim, ITEM_MANAGER& itemMgr)
{
	static const int lowerBox[] = { 50197, 50198, 50199 };
	static const int lowerBox_range = 3;
	static const int midderBox[] = { 50203, 50204, 50205, 50206 };
	static const int midderBox_range = 4;
	static const int higherBox[] = { 50207, 50208, 50209, 50210, 50211 };
	static const int higherBox_range = 5;

	if (victim.GetMobRank() < MOB_RANK_KNIGHT)
		return nullptr;

	int killer_level = killer.GetLevel();

	if (killer_level <= gs_dropEvent_refineBox.low)
	{
		if (number (1, gs_dropEvent_refineBox.percent_low) == 1)
		{
			return itemMgr.CreateItem(lowerBox [number (1,lowerBox_range) - 1], 1, 0, true);
		}
	}
	else if (killer_level <= gs_dropEvent_refineBox.mid)
	{
		if (number (1, gs_dropEvent_refineBox.percent_mid) == 1)
		{
			return itemMgr.CreateItem(midderBox [number (1,midderBox_range) - 1], 1, 0, true);
		}
	}
	else
	{
		if (number (1, gs_dropEvent_refineBox.percent_high) == 1)
		{
			return itemMgr.CreateItem(higherBox [number (1,higherBox_range) - 1], 1, 0, true);
		}
	}
	return nullptr;
}

static void __DropEvent_RefineBox_DropItem(CHARACTER & killer, CHARACTER & victim, ITEM_MANAGER& itemMgr, std::vector<CItem *>& vec_item)
{
	if (!gs_dropEvent_refineBox.alive)
		return;

	int log_level = (test_server || killer.GetGMLevel() >= GM_LOW_WIZARD) ? 0 : 1;

	CItem * p_item = __DropEvent_RefineBox_GetDropItem(killer, victim, itemMgr);

	if (p_item)
	{
		vec_item.push_back(p_item);

		sys_log(log_level, 
			"dropevent.drop_refine_box.item_drop: killer(%s: lv%d), victim(%s: lv:%d), item_name(%s)",
			killer.GetName(), killer.GetLevel(), victim.GetName(), victim.GetLevel(), p_item->GetName());	
	}
}

bool DropEvent_RefineBox_SetValue(const std::string& name, int value)
{
	if (name == "refine_box_drop")
	{
		gs_dropEvent_refineBox.alive = value;

		if (value)
			sys_log(0, "refine_box_drop = on");
		else
			sys_log(0, "refine_box_drop = off");

	}
	else if (name == "refine_box_low")
		gs_dropEvent_refineBox.percent_low = value < 100 ? 100 : value;
	else if (name == "refine_box_mid")
		gs_dropEvent_refineBox.percent_mid = value < 100 ? 100 : value;
	else if (name == "refine_box_high")
		gs_dropEvent_refineBox.percent_high = value < 100 ? 100 : value;
	else
		return false;

	sys_log(0, "refine_box_drop: %d", gs_dropEvent_refineBox.alive ? true : false);
	sys_log(0, "refine_box_low: %d", gs_dropEvent_refineBox.percent_low);
	sys_log(0, "refine_box_mid: %d", gs_dropEvent_refineBox.percent_mid);
	sys_log(0, "refine_box_high: %d", gs_dropEvent_refineBox.percent_high);

	return true;
}
// Improved item reward end.


void ITEM_MANAGER::CreateQuestDropItem(CHARACTER * pkChr, CHARACTER * pkKiller, std::vector<CItem *> & vec_item, int iDeltaPercent, int iRandRange)
{
	CItem * item = nullptr;

	if (!pkChr)
		return;

	if (!pkKiller)
		return;

	sys_log(1, "CreateQuestDropItem victim(%s), killer(%s)", pkChr->GetName(), pkKiller->GetName() );

	// DROPEVENT_CHARSTONE
	__DropEvent_CharStone_DropItem(*pkKiller, *pkChr, *this, vec_item);
	// END_OF_DROPEVENT_CHARSTONE
	__DropEvent_RefineBox_DropItem(*pkKiller, *pkChr, *this, vec_item);

	// Christmas sock
	if (quest::CQuestManager::instance().GetEventFlag("xmas_sock"))
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

		if ( iDropPerKill[pkChr->GetMobRank()] != 0 )
		{
			int iPercent = 40000 * iDeltaPercent / iDropPerKill[pkChr->GetMobRank()];


			sys_log(0, "SOCK DROP %d %d", iPercent, iRandRange);
			if (iPercent >= number(1, iRandRange))
			{
				if ((item = CreateItem(SOCK_ITEM_VNUM, 1, 0, true)))
					vec_item.push_back(item);
			}
		}
	}

	// Moonlight treasure box
	if (quest::CQuestManager::instance().GetEventFlag("drop_moon"))
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

		if (iDropPerKill[pkChr->GetMobRank()])
		{
			int iPercent = 40000 * iDeltaPercent / iDropPerKill[pkChr->GetMobRank()];

			if (iPercent >= number(1, iRandRange))
			{
				if ((item = CreateItem(ITEM_VNUM, 1, 0, true)))
					vec_item.push_back(item);
			}
		}
	}

	if (pkKiller->GetLevel() >= 15 && abs(pkKiller->GetLevel() - pkChr->GetLevel()) <= 5)
	{
		int pct = quest::CQuestManager::instance().GetEventFlag("hc_drop");

		if (pct > 0)
		{
			const uint32_t ITEM_VNUM = 30178;

			if (number(1,100) <= pct)
			{
				if ((item = CreateItem(ITEM_VNUM, 1, 0, true)))
					vec_item.push_back(item);
			}
		}
	}

	//Hexagonal treasure box
	if (GetDropPerKillPct(100, 2000, iDeltaPercent, "2006_drop") >= number(1, iRandRange))
	{
		sys_log(0, "육각보합 DROP EVENT ");

		const static uint32_t dwVnum = 50037;

		if ((item = CreateItem(dwVnum, 1, 0, true)))
			vec_item.push_back(item);

	}

	//Hexagonal treasure box+
	if (GetDropPerKillPct(100, 2000, iDeltaPercent, "2007_drop") >= number(1, iRandRange))
	{
		sys_log(0, "육각보합 DROP EVENT ");

		const static uint32_t dwVnum = 50043;

		if ((item = CreateItem(dwVnum, 1, 0, true)))
			vec_item.push_back(item);
	}

	// New Year firecracker event
	if (GetDropPerKillPct(/* minimum */ 100, /* default */ 1000, iDeltaPercent, "newyear_fire") >= number(1, iRandRange))
	{
		// Firecrackers in China, spinning tops in Korea
		const uint32_t ITEM_VNUM_FIRE = 50107;

		if ((item = CreateItem(ITEM_VNUM_FIRE, 1, 0, true)))
			vec_item.push_back(item);
	}

	// New Year full-moon festival event
	if (GetDropPerKillPct(100, 500, iDeltaPercent, "newyear_moon") >= number(1, iRandRange))
	{
		sys_log(0, "EVENT NEWYEAR_MOON DROP");

		const static uint32_t wonso_items[6] = { 50016, 50017, 50018, 50019, 50019, 50019, };
		uint32_t dwVnum = wonso_items[number(0,5)];

		if ((item = CreateItem(dwVnum, 1, 0, true)))
			vec_item.push_back(item);
	}

	// Valentine's Day event. Changed event minimum to 1 at OGE's request. (Other events left as they are for now.)
	if (GetDropPerKillPct(1, 2000, iDeltaPercent, "valentine_drop") >= number(1, iRandRange))
	{
		sys_log(0, "EVENT VALENTINE_DROP");

		const static uint32_t valentine_items[2] = { 50024, 50025 };
		uint32_t dwVnum = valentine_items[number(0, 1)];

		if ((item = CreateItem(dwVnum, 1, 0, true)))
			vec_item.push_back(item);
	}

	// Ice cream event
	if (GetDropPerKillPct(100, 2000, iDeltaPercent, "icecream_drop") >= number(1, iRandRange))
	{
		const static uint32_t icecream = 50123;

		if ((item = CreateItem(icecream, 1, 0, true)))
			vec_item.push_back(item);
	}

	// new Christmas event
	// 53002 : baby reindeer summon ticket
	if ((pkKiller->CountSpecifyItem(53002) > 0) && (GetDropPerKillPct(50, 100, iDeltaPercent, "new_xmas_event") >= number(1, iRandRange)))
	{
		const static uint32_t xmas_sock = 50010;
		pkKiller->AutoGiveItem (xmas_sock, 1);
	}

	if ((pkKiller->CountSpecifyItem(53007) > 0) && (GetDropPerKillPct(50, 100, iDeltaPercent, "new_xmas_event") >= number(1, iRandRange)))
	{
		const static uint32_t xmas_sock = 50010;
		pkKiller->AutoGiveItem (xmas_sock, 1);
	}

	if ( GetDropPerKillPct(100, 2000, iDeltaPercent, "halloween_drop") >= number(1, iRandRange) )
	{
		const static uint32_t halloween_item = 30321;

		if ( (item=CreateItem(halloween_item, 1, 0, true)) )
			vec_item.push_back(item);
	}
	
	if ( GetDropPerKillPct(100, 2000, iDeltaPercent, "ramadan_drop") >= number(1, iRandRange) )
	{
		const static uint32_t ramadan_item = 30315;

		if ( (item=CreateItem(ramadan_item, 1, 0, true)) )
			vec_item.push_back(item);
	}

	if ( GetDropPerKillPct(100, 2000, iDeltaPercent, "easter_drop") >= number(1, iRandRange) )
	{
		const static uint32_t easter_item_base = 50160;

		if ( (item=CreateItem(easter_item_base+number(0,19), 1, 0, true)) )
			vec_item.push_back(item);
	}

	// World Cup event
	if ( GetDropPerKillPct(100, 2000, iDeltaPercent, "football_drop") >= number(1, iRandRange) )
	{
		const static uint32_t football_item = 50096;

		if ( (item=CreateItem(football_item, 1, 0, true)) )
			vec_item.push_back(item);
	}

	// White Day event
	if (GetDropPerKillPct(100, 2000, iDeltaPercent, "whiteday_drop") >= number(1, iRandRange))
	{
		sys_log(0, "EVENT WHITEDAY_DROP");
		const static uint32_t whiteday_items[2] = { ITEM_WHITEDAY_ROSE, ITEM_WHITEDAY_CANDY };
		uint32_t dwVnum = whiteday_items[number(0,1)];

		if ((item = CreateItem(dwVnum, 1, 0, true)))
			vec_item.push_back(item);
	}

	// Children's Day mystery box event
	if (pkKiller->GetLevel()>=50)
	{
		if (GetDropPerKillPct(100, 1000, iDeltaPercent, "kids_day_drop_high") >= number(1, iRandRange))
		{
			uint32_t ITEM_QUIZ_BOX = 50034;

			if ((item = CreateItem(ITEM_QUIZ_BOX, 1, 0, true)))
				vec_item.push_back(item);
		}
	}
	else
	{
		if (GetDropPerKillPct(100, 1000, iDeltaPercent, "kids_day_drop") >= number(1, iRandRange))
		{
			uint32_t ITEM_QUIZ_BOX = 50034;

			if ((item = CreateItem(ITEM_QUIZ_BOX, 1, 0, true)))
				vec_item.push_back(item);
		}
	}

	// Olympic drop event
	if (pkChr->GetLevel() >= 30 && GetDropPerKillPct(50, 100, iDeltaPercent, "medal_part_drop") >= number(1, iRandRange))
	{
		const static uint32_t drop_items[] = { 30265, 30266, 30267, 30268, 30269 };
		int i = number (0, 4);
		item = CreateItem(drop_items[i]);
		if (item != nullptr)
			vec_item.push_back(item);
	}

	// ADD_GRANDMASTER_SKILL
	// Soul stone item drop
	if (pkChr->GetLevel() >= 40 && pkChr->GetMobRank() >= MOB_RANK_BOSS && GetDropPerKillPct(/* minimum */ 1, /* default */ 1000, iDeltaPercent, "three_skill_item") / GetThreeSkillLevelAdjust(pkChr->GetLevel()) >= number(1, iRandRange))
	{
		const uint32_t ITEM_VNUM = 50513;

		if ((item = CreateItem(ITEM_VNUM, 1, 0, true)))
			vec_item.push_back(item);
	}
	// END_OF_ADD_GRANDMASTER_SKILL

	//
	// Seed item drop
	//
	if (GetDropPerKillPct(100, 1000, iDeltaPercent, "dragon_boat_festival_drop") >= number(1, iRandRange))
	{
		const uint32_t ITEM_SEED = 50085;

		if ((item = CreateItem(ITEM_SEED, 1, 0, true)))
			vec_item.push_back(item);
	}

	// Ten-thousand-year iron drop for the Blessing of the War God
	if (pkKiller->GetLevel() >= 15 && quest::CQuestManager::instance().GetEventFlag("mars_drop"))
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

		if (iDropMultiply[pkChr->GetMobRank()] &&
				GetDropPerKillPct(1000, 1500, iDeltaPercent, "mars_drop") >= number(1, iRandRange) * iDropMultiply[pkChr->GetMobRank()])
		{
			if ((item = CreateItem(ITEM_HANIRON, 1, 0, true)))
				vec_item.push_back(item);
		}
	}
}

uint32_t ITEM_MANAGER::GetRefineFromVnum(uint32_t dwVnum)
{
	auto it = m_map_ItemRefineFrom.find(dwVnum);
	if (it != m_map_ItemRefineFrom.end())
		return it->second;
	return 0;
}

const CSpecialItemGroup* ITEM_MANAGER::GetSpecialItemGroup(uint32_t dwVnum)
{
	auto it = m_map_pkSpecialItemGroup.find(dwVnum);
	if (it != m_map_pkSpecialItemGroup.end())
	{
		return it->second;
	}
	return nullptr;
}

const CSpecialAttrGroup* ITEM_MANAGER::GetSpecialAttrGroup(uint32_t dwVnum)
{
	auto it = m_map_pkSpecialAttrGroup.find(dwVnum);
	if (it != m_map_pkSpecialAttrGroup.end())
	{
		return it->second;
	}
	return nullptr;
}

uint32_t ITEM_MANAGER::GetMaskVnum(uint32_t dwVnum)
{
	TMapDW2DW::iterator it = m_map_new_to_ori.find (dwVnum);
	if (it != m_map_new_to_ori.end())
	{
		return it->second;
	}
	else
		return 0;
}

// Copies all attributes and socket values to pkNewItem.
// Copied as-is from TransformRefineItem, a local function formerly in char_item.cpp
void ITEM_MANAGER::CopyAllAttrTo(CItem * pkOldItem, CItem * pkNewItem)
{
	// ACCESSORY_REFINE
	if (pkOldItem->IsAccessoryForSocket())
	{
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			pkNewItem->SetSocket(i, pkOldItem->GetSocket(i));
		}
	}
	// END_OF_ACCESSORY_REFINE
	else
	{
		// Broken stones are cleaned up automatically here
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			if (!pkOldItem->GetSocket(i))
				break;
			else
				pkNewItem->SetSocket(i, 1);
		}

		// Set sockets
		int slot = 0;

		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			int32_t socket = pkOldItem->GetSocket(i);
			const int ITEM_BROKEN_METIN_VNUM = 28960; // Why is the same constant in 3 places... just make it one T_T I'm passing, Hong will do it
			if (socket > 2 && socket != ITEM_BROKEN_METIN_VNUM)
				pkNewItem->SetSocket(slot++, socket);
		}

	}

	// Set magic item
	pkOldItem->CopyAttributeTo(pkNewItem);
}
