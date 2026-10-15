#include "char_manager.h"

#include "char.h"
#include "cmd.h"
#include "config.h"
#include "db.h"
#include "desc.h"
#include "dungeon.h"
#include "FSM.h"
#include "mining.h"
#include "mob_manager.h"
#include "packet.h"
#include "party.h"
#include "questlua.h"
#include "questmanager.h"
#include "regen.h"
#include "sectree.h"
#include "sectree_manager.h"
#include "typedef.h"
#include "utils.h"
#include "vid.h"
#include "xmas_event.h"

#include <common/length.h>
#include <metin2/log/log.h>
#include <libthecore/utils.h>

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

CHARACTER_MANAGER::CHARACTER_MANAGER() :
	m_iVIDCount(0),
	m_pkChrSelectedStone(nullptr),
	m_bUsePendingDestroy(false)
{
	RegisterRaceNum(xmas::MOB_XMAS_FIRWORK_SELLER_VNUM);
	RegisterRaceNum(xmas::MOB_SANTA_VNUM);
	RegisterRaceNum(xmas::MOB_XMAS_TREE_VNUM);

	m_iMobItemRate = 100;
	m_iMobDamageRate = 100;
	m_iMobGoldAmountRate = 100;
	m_iMobGoldDropRate = 100;
	m_iMobExpRate = 100;

	m_iMobItemRatePremium = 100;
	m_iMobGoldAmountRatePremium = 100;
	m_iMobGoldDropRatePremium = 100;
	m_iMobExpRatePremium = 100;
	
}

CHARACTER_MANAGER::~CHARACTER_MANAGER()
{
	Destroy();
}

void CHARACTER_MANAGER::Destroy()
{
	auto it = m_map_pkChrByVID.begin();
	while (it != m_map_pkChrByVID.end()) {
		CHARACTER * ch = it->second;
		CHARACTER_MANAGER::instance().DestroyCharacter(ch); // m_map_pkChrByVID is changed here
		it = m_map_pkChrByVID.begin();
	}
}

void CHARACTER_MANAGER::GracefulShutdown()
{
	NAME_MAP::iterator it = m_map_pkPCChr.begin();

	while (it != m_map_pkPCChr.end())
		(it++)->second->Disconnect("GracefulShutdown");
}

uint32_t CHARACTER_MANAGER::AllocVID()
{
	++m_iVIDCount;
	return m_iVIDCount;
}

CHARACTER * CHARACTER_MANAGER::CreateCharacter(const char * name, uint32_t dwPID)
{
	uint32_t dwVID = AllocVID();

	CHARACTER * ch = new CHARACTER;
	ch->Create(name, dwVID, dwPID ? true : false);

	m_map_pkChrByVID.insert(std::make_pair(dwVID, ch));

	if (dwPID)
	{
		char szName[CHARACTER_NAME_MAX_LEN + 1];
		str_lower(name, szName, sizeof(szName));

		m_map_pkPCChr.insert(NAME_MAP::value_type(szName, ch));
		m_map_pkChrByPID.insert(std::make_pair(dwPID, ch));
	}

	return (ch);
}

void CHARACTER_MANAGER::DestroyCharacter(CHARACTER * ch)
{
	if (!ch)
		return;

	// <Factor> Check whether it has been already deleted or not.
	auto it = m_map_pkChrByVID.find(ch->GetVID());
	if (it == m_map_pkChrByVID.end()) {
		metin2::log::error("[CHARACTER_MANAGER::DestroyCharacter] <Factor> {:d} not found", (int32_t)(ch->GetVID()));
		return; // prevent duplicated destrunction
	}

	// monsters belonging to a dungeon are also removed from the dungeon.
	if (ch->IsNPC() && !ch->IsPet() && ch->GetRider() == nullptr)
	{
		if (ch->GetDungeon())
		{
			ch->GetDungeon()->DeadCharacter(ch);
		}
	}

	if (m_bUsePendingDestroy)
	{
		m_set_pkChrPendingDestroy.insert(ch);
		return;
	}

	m_map_pkChrByVID.erase(it);

	if (true == ch->IsPC())
	{
		char szName[CHARACTER_NAME_MAX_LEN + 1];

		str_lower(ch->GetName(), szName, sizeof(szName));

		NAME_MAP::iterator it = m_map_pkPCChr.find(szName);

		if (m_map_pkPCChr.end() != it)
			m_map_pkPCChr.erase(it);
	}

	if (0 != ch->GetPlayerID())
	{
		auto it = m_map_pkChrByPID.find(ch->GetPlayerID());

		if (m_map_pkChrByPID.end() != it)
		{
			m_map_pkChrByPID.erase(it);
		}
	}

	UnregisterRaceNumMap(ch);

	RemoveFromStateList(ch);

	delete ch;
}

CHARACTER * CHARACTER_MANAGER::Find(uint32_t dwVID)
{
	auto it = m_map_pkChrByVID.find(dwVID);

	if (m_map_pkChrByVID.end() == it)
		return nullptr;
	
	// <Factor> Added sanity check
	CHARACTER * found = it->second;
	if (found != nullptr && dwVID != (uint32_t)found->GetVID()) {
		metin2::log::error("[CHARACTER_MANAGER::Find] <Factor> {:d} != {:d}", dwVID, (uint32_t)found->GetVID());
		return nullptr;
	}
	return found;
}

CHARACTER * CHARACTER_MANAGER::Find(const VID & vid)
{
	CHARACTER * tch = Find((uint32_t) vid);

	if (!tch || tch->GetVID() != vid)
		return nullptr;

	return tch;
}

CHARACTER * CHARACTER_MANAGER::FindByPID(uint32_t dwPID)
{
	auto it = m_map_pkChrByPID.find(dwPID);

	if (m_map_pkChrByPID.end() == it)
		return nullptr;

	// <Factor> Added sanity check
	CHARACTER * found = it->second;
	if (found != nullptr && dwPID != found->GetPlayerID()) {
		metin2::log::error("[CHARACTER_MANAGER::FindByPID] <Factor> {:d} != {:d}", dwPID, found->GetPlayerID());
		return nullptr;
	}
	return found;
}

CHARACTER * CHARACTER_MANAGER::FindPC(const char * name)
{
	char szName[CHARACTER_NAME_MAX_LEN + 1];
	str_lower(name, szName, sizeof(szName));
	NAME_MAP::iterator it = m_map_pkPCChr.find(szName);

	if (it == m_map_pkPCChr.end())
		return nullptr;

	// <Factor> Added sanity check
	CHARACTER * found = it->second;
	if (found != nullptr && strncasecmp(szName, found->GetName(), CHARACTER_NAME_MAX_LEN) != 0) {
		metin2::log::error("[CHARACTER_MANAGER::FindPC] <Factor> {} != {}", name, found->GetName());
		return nullptr;
	}
	return found;
}

CHARACTER * CHARACTER_MANAGER::SpawnMobRandomPosition(uint32_t dwVnum, int32_t lMapIndex)
{
	// allows deciding whether to spawn Japanese pirates (Waegu)
	{
		if (dwVnum == 5001 && !quest::CQuestManager::instance().GetEventFlag("japan_regen"))
		{
			metin2::log::debug( "WAEGU[5001] regen disabled.");
			return nullptr;
		}
	}

	// allows deciding whether to spawn Haetae
	{
		if (dwVnum == 5002 && !quest::CQuestManager::instance().GetEventFlag("newyear_mob"))
		{
			metin2::log::debug( "HAETAE (new-year-mob) [5002] regen disabled.");
			return nullptr;
		}
	}

	// Liberation Day event 
	{
		if (dwVnum == 5004 && !quest::CQuestManager::instance().GetEventFlag("independence_day"))
		{
			metin2::log::debug( "INDEPENDECE DAY [5004] regen disabled.");
			return nullptr;
		}
	}

	const CMob * pkMob = CMobManager::instance().Get(dwVnum);

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

	SECTREE_MAP * pkSectreeMap = SECTREE_MANAGER::instance().GetMap(lMapIndex);
	if (pkSectreeMap == nullptr) {
		return nullptr;
	}

	int i;
	int32_t x, y;
	for (i=0; i<2000; i++)
	{
		x = number(1, (pkSectreeMap->m_setting.iWidth / 100)  - 1) * 100 + pkSectreeMap->m_setting.iBaseX;
		y = number(1, (pkSectreeMap->m_setting.iHeight / 100) - 1) * 100 + pkSectreeMap->m_setting.iBaseY;
		SECTREE * tree = pkSectreeMap->Find(x, y);

		if (!tree)
			continue;

		uint32_t dwAttr = tree->GetAttribute(x, y);

		if (IS_SET(dwAttr, ATTR_BLOCK | ATTR_OBJECT))
			continue;

		if (IS_SET(dwAttr, ATTR_BANPK))
			continue;

		break;
	}

	if (i == 2000)
	{
		metin2::log::error("cannot find valid location");
		return nullptr;
	}

	SECTREE * sectree = SECTREE_MANAGER::instance().Get(lMapIndex, x, y);

	if (!sectree)
	{
		metin2::log::info( "SpawnMobRandomPosition: cannot create monster at non-exist sectree {:d} x {:d} (map {:d})", x, y, lMapIndex);
		return nullptr;
	}

	CHARACTER * ch = CHARACTER_MANAGER::instance().CreateCharacter(pkMob->m_table.szLocaleName);

	if (!ch)
	{
		metin2::log::info( "SpawnMobRandomPosition: cannot create new character");
		return nullptr;
	}

	ch->SetProto(pkMob);

	// if mob is npc with no empire assigned, assign to empire of map
	if (pkMob->m_table.bType == CHAR_TYPE_NPC)
		if (ch->GetEmpire() == 0)
			ch->SetEmpire(SECTREE_MANAGER::instance().GetEmpireFromMapIndex(lMapIndex));

	ch->SetRotation(number(0, 360));

	if (!ch->Show(lMapIndex, x, y, 0, false))
	{
		CHARACTER_MANAGER::instance().DestroyCharacter(ch);
		metin2::log::error("SpawnMobRandomPosition: cannot show monster");
		return nullptr;
	}

	char buf[512+1];
	int32_t local_x = x - pkSectreeMap->m_setting.iBaseX;
	int32_t local_y = y - pkSectreeMap->m_setting.iBaseY;
	snprintf(buf, sizeof(buf), "spawn %s[%d] random position at %d %d %d %d (time: %ld)", ch->GetName(), dwVnum, x, y, local_x, local_y, get_global_time());
	
	if (test_server)
		SendNotice(buf);

	metin2::log::info( "{}", buf);
	return (ch);
}

CHARACTER * CHARACTER_MANAGER::SpawnMob(uint32_t dwVnum, int32_t lMapIndex, int32_t x, int32_t y, int32_t z, bool bSpawnMotion, int iRot, bool bShow)
{
	const CMob * pkMob = CMobManager::instance().Get(dwVnum);
	if (!pkMob)
	{
		metin2::log::error("SpawnMob: no mob data for vnum {:d}", dwVnum);
		return nullptr;
	}

	if (!(pkMob->m_table.bType == CHAR_TYPE_NPC || pkMob->m_table.bType == CHAR_TYPE_WARP || pkMob->m_table.bType == CHAR_TYPE_GOTO) || mining::IsVeinOfOre (dwVnum))
	{
		SECTREE * tree = SECTREE_MANAGER::instance().Get(lMapIndex, x, y);

		if (!tree)
		{
			metin2::log::info( "no sectree for spawn at {:d} {:d} mobvnum {:d} mapindex {:d}", x, y, dwVnum, lMapIndex);
			return nullptr;
		}

		uint32_t dwAttr = tree->GetAttribute(x, y);

		bool is_set = false;

		if ( mining::IsVeinOfOre (dwVnum) ) is_set = IS_SET(dwAttr, ATTR_BLOCK);
		else is_set = IS_SET(dwAttr, ATTR_BLOCK | ATTR_OBJECT);

		if ( is_set )
		{
			// SPAWN_BLOCK_LOG
			static bool s_isLog=quest::CQuestManager::instance().GetEventFlag("spawn_block_log");
			static uint32_t s_nextTime=get_global_time()+10000;

			uint32_t curTime=get_global_time();

			if (curTime>s_nextTime)
			{
				s_nextTime=curTime;
				s_isLog=quest::CQuestManager::instance().GetEventFlag("spawn_block_log");

			}

			if (s_isLog)
				metin2::log::info( "SpawnMob: BLOCKED position for spawn {} {:d} at {:d} {:d} (attr {:d})", pkMob->m_table.szName, dwVnum, x, y, dwAttr);
			// END_OF_SPAWN_BLOCK_LOG
			return nullptr;
		}

		if (IS_SET(dwAttr, ATTR_BANPK))
		{
			metin2::log::info( "SpawnMob: BAN_PK position for mob spawn {} {:d} at {:d} {:d}", pkMob->m_table.szName, dwVnum, x, y);
			return nullptr;
		}
	}

	SECTREE * sectree = SECTREE_MANAGER::instance().Get(lMapIndex, x, y);

	if (!sectree)
	{
		metin2::log::info( "SpawnMob: cannot create monster at non-exist sectree {:d} x {:d} (map {:d})", x, y, lMapIndex);
		return nullptr;
	}

	CHARACTER * ch = CHARACTER_MANAGER::instance().CreateCharacter(pkMob->m_table.szLocaleName);

	if (!ch)
	{
		metin2::log::info( "SpawnMob: cannot create new character");
		return nullptr;
	}

	if (iRot == -1)
		iRot = number(0, 360);

	ch->SetProto(pkMob);

	// if mob is npc with no empire assigned, assign to empire of map
	if (pkMob->m_table.bType == CHAR_TYPE_NPC)
		if (ch->GetEmpire() == 0)
			ch->SetEmpire(SECTREE_MANAGER::instance().GetEmpireFromMapIndex(lMapIndex));

	ch->SetRotation(iRot);

	if (bShow && !ch->Show(lMapIndex, x, y, z, bSpawnMotion))
	{
		CHARACTER_MANAGER::instance().DestroyCharacter(ch);
		metin2::log::info( "SpawnMob: cannot show monster");
		return nullptr;
	}

	return (ch);
}

CHARACTER * CHARACTER_MANAGER::SpawnMobRange(uint32_t dwVnum, int32_t lMapIndex, int sx, int sy, int ex, int ey, bool bIsException, bool bSpawnMotion, bool bAggressive )
{
	const CMob * pkMob = CMobManager::instance().Get(dwVnum);

	if (!pkMob)
		return nullptr;

	if (pkMob->m_table.bType == CHAR_TYPE_STONE)	// stones always have a SPAWN motion.
		bSpawnMotion = true;

	int i = 16;

	while (i--)
	{
		int x = number(sx, ex);
		int y = number(sy, ey);
		CHARACTER * ch = SpawnMob(dwVnum, lMapIndex, x, y, 0, bSpawnMotion);

		if (ch)
		{
			metin2::log::debug( "MOB_SPAWN: {}({:d}) {:d}x{:d}", ch->GetName(), (uint32_t) ch->GetVID(), ch->GetX(), ch->GetY());
			if ( bAggressive )
				ch->SetAggressive();
			return (ch);
		}
	}

	return nullptr;
}

void CHARACTER_MANAGER::SelectStone(CHARACTER * pkChr)
{
	m_pkChrSelectedStone = pkChr;
}

bool CHARACTER_MANAGER::SpawnMoveGroup(uint32_t dwVnum, int32_t lMapIndex, int sx, int sy, int ex, int ey, int tx, int ty, REGEN * pkRegen, bool bAggressive_)
{
	CMobGroup * pkGroup = CMobManager::instance().GetGroup(dwVnum);

	if (!pkGroup)
	{
		metin2::log::error("NOT_EXIST_GROUP_VNUM({:d}) Map({:d}) ", dwVnum, lMapIndex);
		return false;
	}

	CHARACTER * pkChrMaster = nullptr;
	CParty * pkParty = nullptr;

	const std::vector<uint32_t> & c_rdwMembers = pkGroup->GetMemberVector();

	bool bSpawnedByStone = false;
	bool bAggressive = bAggressive_;

	if (m_pkChrSelectedStone)
	{
		bSpawnedByStone = true;
		if (m_pkChrSelectedStone->GetDungeon())
			bAggressive = true;
	}

	for (uint32_t i = 0; i < c_rdwMembers.size(); ++i)
	{
		CHARACTER * tch = SpawnMobRange(c_rdwMembers[i], lMapIndex, sx, sy, ex, ey, true, bSpawnedByStone);

		if (!tch)
		{
			if (i == 0)	// if the monster that failed to be created is the leader, just fail
				return false;

			continue;
		}

		sx = tch->GetX() - number(300, 500);
		sy = tch->GetY() - number(300, 500);
		ex = tch->GetX() + number(300, 500);
		ey = tch->GetY() + number(300, 500);

		if (m_pkChrSelectedStone)
			tch->SetStone(m_pkChrSelectedStone);
		else if (pkParty)
		{
			pkParty->Join(tch->GetVID());
			pkParty->Link(tch);
		}
		else if (!pkChrMaster)
		{
			pkChrMaster = tch;
			pkChrMaster->SetRegen(pkRegen);

			pkParty = CPartyManager::instance().CreateParty(pkChrMaster);
		}
		if (bAggressive)
			tch->SetAggressive();

		if (tch->Goto(tx, ty))
			tch->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);
	}

	return true;
}

bool CHARACTER_MANAGER::SpawnGroupGroup(uint32_t dwVnum, int32_t lMapIndex, int sx, int sy, int ex, int ey, REGEN * pkRegen, bool bAggressive_, CDungeon * pDungeon)
{
	const uint32_t dwGroupID = CMobManager::instance().GetGroupFromGroupGroup(dwVnum);

	if( dwGroupID != 0 )
	{
		return SpawnGroup(dwGroupID, lMapIndex, sx, sy, ex, ey, pkRegen, bAggressive_, pDungeon);
	}
	else
	{
		metin2::log::error( "NOT_EXIST_GROUP_GROUP_VNUM({:d}) MAP({:d})", dwVnum, lMapIndex );
		return false;
	}
}

CHARACTER * CHARACTER_MANAGER::SpawnGroup(uint32_t dwVnum, int32_t lMapIndex, int sx, int sy, int ex, int ey, REGEN * pkRegen, bool bAggressive_, CDungeon * pDungeon)
{
	CMobGroup * pkGroup = CMobManager::instance().GetGroup(dwVnum);

	if (!pkGroup)
	{
		metin2::log::error("NOT_EXIST_GROUP_VNUM({:d}) Map({:d}) ", dwVnum, lMapIndex);
		return nullptr;
	}

	CHARACTER * pkChrMaster = nullptr;
	CParty * pkParty = nullptr;

	const std::vector<uint32_t> & c_rdwMembers = pkGroup->GetMemberVector();

	bool bSpawnedByStone = false;
	bool bAggressive = bAggressive_;

	if (m_pkChrSelectedStone)
	{
		bSpawnedByStone = true;

		if (m_pkChrSelectedStone->GetDungeon())
			bAggressive = true;
	}

	CHARACTER * chLeader = nullptr;

	for (uint32_t i = 0; i < c_rdwMembers.size(); ++i)
	{
		CHARACTER * tch = SpawnMobRange(c_rdwMembers[i], lMapIndex, sx, sy, ex, ey, true, bSpawnedByStone);

		if (!tch)
		{
			if (i == 0)	// if the monster that failed to be created is the leader, just fail
				return nullptr;

			continue;
		}

		if (i == 0)
			chLeader = tch;

		tch->SetDungeon(pDungeon);

		sx = tch->GetX() - number(300, 500);
		sy = tch->GetY() - number(300, 500);
		ex = tch->GetX() + number(300, 500);
		ey = tch->GetY() + number(300, 500);

		if (m_pkChrSelectedStone)
			tch->SetStone(m_pkChrSelectedStone);
		else if (pkParty)
		{
			pkParty->Join(tch->GetVID());
			pkParty->Link(tch);
		}
		else if (!pkChrMaster)
		{
			pkChrMaster = tch;
			pkChrMaster->SetRegen(pkRegen);

			pkParty = CPartyManager::instance().CreateParty(pkChrMaster);
		}

		if (bAggressive)
			tch->SetAggressive();
	}

	return chLeader;
}

struct FuncUpdateAndResetChatCounter
{
	void operator () (CHARACTER * ch)
	{
		ch->ResetChatCounter();
		ch->CFSM::Update();
	}
};

void CHARACTER_MANAGER::Update(int iPulse)
{
	BeginPendingDestroy();

	// PC character update
	{
		if (!m_map_pkPCChr.empty())
		{
			// copy container
			CHARACTER_VECTOR v;
			v.reserve(m_map_pkPCChr.size());
			for (const auto & [name, chr] : m_map_pkPCChr)
				v.push_back(chr);
			if (0 == (iPulse % PASSES_PER_SEC(5)))
			{
				FuncUpdateAndResetChatCounter f;
				for_each(v.begin(), v.end(), f);
			}
			else
			{
				for_each(v.begin(), v.end(), std::bind(&CHARACTER::UpdateCharacter, std::placeholders::_1, iPulse));
			}
		}
	}

	// monster update
	{
		if (!m_set_pkChrState.empty())
		{
			CHARACTER_VECTOR v;
			v.reserve(m_set_pkChrState.size());
			v.insert(v.end(), m_set_pkChrState.begin(), m_set_pkChrState.end());
			for_each(v.begin(), v.end(), std::bind(&CHARACTER::UpdateStateMachine, std::placeholders::_1, iPulse));
		}
	}

	// update Santa separately
	{
		CharacterVectorInteractor i;

		if (CHARACTER_MANAGER::instance().GetCharactersByRaceNum(xmas::MOB_SANTA_VNUM, i))
		{
			for_each(i.begin(), i.end(), std::bind(&CHARACTER::UpdateStateMachine, std::placeholders::_1, iPulse));
		}
	}

	// record mob kill counts once per hour 
	if (0 == (iPulse % PASSES_PER_SEC(3600)))
	{
		for (auto it = m_map_dwMobKillCount.begin(); it != m_map_dwMobKillCount.end(); ++it)
			DBManager::instance().SendMoneyLog(MONEY_LOG_MONSTER_KILL, it->first, it->second);


		m_map_dwMobKillCount.clear();
	}

	// on the test server, count characters every 60 seconds
	if (test_server && 0 == (iPulse % PASSES_PER_SEC(60)))
		metin2::log::info( "CHARACTER COUNT vid {:d} pid {:d}", m_map_pkChrByVID.size(), m_map_pkChrByPID.size());

	// perform delayed DestroyCharacter
	FlushPendingDestroy();
}

void CHARACTER_MANAGER::ProcessDelayedSave()
{
	CHARACTER_SET::iterator it = m_set_pkChrForDelayedSave.begin();

	while (it != m_set_pkChrForDelayedSave.end())
	{
		CHARACTER * pkChr = *it++;
		pkChr->SaveReal();
	}

	m_set_pkChrForDelayedSave.clear();
}

bool CHARACTER_MANAGER::AddToStateList(CHARACTER * ch)
{
	assert(ch != nullptr);

	CHARACTER_SET::iterator it = m_set_pkChrState.find(ch);

	if (it == m_set_pkChrState.end())
	{
		m_set_pkChrState.insert(ch);
		return true;
	}

	return false;
}

void CHARACTER_MANAGER::RemoveFromStateList(CHARACTER * ch)
{
	CHARACTER_SET::iterator it = m_set_pkChrState.find(ch);

	if (it != m_set_pkChrState.end())
	{
		m_set_pkChrState.erase(it);
	}
}

void CHARACTER_MANAGER::DelayedSave(CHARACTER * ch)
{
	m_set_pkChrForDelayedSave.insert(ch);
}

bool CHARACTER_MANAGER::FlushDelayedSave(CHARACTER * ch)
{
	CHARACTER_SET::iterator it = m_set_pkChrForDelayedSave.find(ch);

	if (it == m_set_pkChrForDelayedSave.end())
		return false;

	m_set_pkChrForDelayedSave.erase(it);
	ch->SaveReal();
	return true;
}

void CHARACTER_MANAGER::RegisterForMonsterLog(CHARACTER * ch)
{
	m_set_pkChrMonsterLog.insert(ch);
}

void CHARACTER_MANAGER::UnregisterForMonsterLog(CHARACTER * ch)
{
	m_set_pkChrMonsterLog.erase(ch);
}

void CHARACTER_MANAGER::PacketMonsterLog(CHARACTER * ch, const void* buf, int size)
{
	decltype(m_set_pkChrMonsterLog.begin()) it;

	for (it = m_set_pkChrMonsterLog.begin(); it!=m_set_pkChrMonsterLog.end();++it)
	{
		CHARACTER * c = *it;

		if (ch && DISTANCE_APPROX(c->GetX()-ch->GetX(), c->GetY()-ch->GetY())>6000)
			continue;

		DESC * d = c->GetDesc();

		if (d)
			d->Packet(buf, size);
	}
}

void CHARACTER_MANAGER::KillLog(uint32_t dwVnum)
{
	const uint32_t SEND_LIMIT = 10000;

	auto it = m_map_dwMobKillCount.find(dwVnum);

	if (it == m_map_dwMobKillCount.end())
		m_map_dwMobKillCount.insert(std::make_pair(dwVnum, 1));
	else
	{
		++it->second;

		if (it->second > SEND_LIMIT)
		{
			DBManager::instance().SendMoneyLog(MONEY_LOG_MONSTER_KILL, it->first, it->second);
			m_map_dwMobKillCount.erase(it);
		}
	}
}

void CHARACTER_MANAGER::RegisterRaceNum(uint32_t dwVnum)
{
	m_set_dwRegisteredRaceNum.insert(dwVnum);
}

void CHARACTER_MANAGER::RegisterRaceNumMap(CHARACTER * ch)
{
	uint32_t dwVnum = ch->GetRaceNum();

	if (m_set_dwRegisteredRaceNum.find(dwVnum) != m_set_dwRegisteredRaceNum.end()) // if it's a registered number
	{
		metin2::log::info( "RegisterRaceNumMap {} {:d}", ch->GetName(), dwVnum);
		m_map_pkChrByRaceNum[dwVnum].insert(ch);
	}
}

void CHARACTER_MANAGER::UnregisterRaceNumMap(CHARACTER * ch)
{
	uint32_t dwVnum = ch->GetRaceNum();

	auto it = m_map_pkChrByRaceNum.find(dwVnum);

	if (it != m_map_pkChrByRaceNum.end())
		it->second.erase(ch);
}

bool CHARACTER_MANAGER::GetCharactersByRaceNum(uint32_t dwRaceNum, CharacterVectorInteractor & i)
{
	std::map<uint32_t, CHARACTER_SET>::iterator it = m_map_pkChrByRaceNum.find(dwRaceNum);

	if (it == m_map_pkChrByRaceNum.end())
		return false;

	// copy container
	i = it->second;
	return true;
}


//
// (job+1)*3+(skill_group)
//
CHARACTER * CHARACTER_MANAGER::FindSpecifyPC(unsigned int uiJobFlag, int32_t lMapIndex, CHARACTER * except, int iMinLevel, int iMaxLevel)
{
	CHARACTER * chFind = nullptr;
	decltype(m_map_pkChrByPID.begin()) it;
	int n = 0;

	for (it = m_map_pkChrByPID.begin(); it != m_map_pkChrByPID.end(); ++it)
	{
		CHARACTER * ch = it->second;

		if (ch == except)
			continue;

		if (ch->GetLevel() < iMinLevel)
			continue;

		if (ch->GetLevel() > iMaxLevel)
			continue;

		if (ch->GetMapIndex() != lMapIndex)
			continue;

		if (uiJobFlag)
		{
			unsigned int uiChrJob = (1 << ((ch->GetJob() + 1) * 3 + ch->GetSkillGroup()));

			if (!IS_SET(uiJobFlag, uiChrJob))
				continue;
		}

		if (!chFind || number(1, ++n) == 1)
			chFind = ch;
	}

	return chFind;
}

int CHARACTER_MANAGER::GetMobItemRate(CHARACTER * ch)	
{ 
	//PREVENT_TOXICATION_FOR_CHINA
	//END_PREVENT_TOXICATION_FOR_CHINA
	if (ch && ch->GetPremiumRemainSeconds(PREMIUM_ITEM) > 0)
		return m_iMobItemRatePremium;
	return m_iMobItemRate; 
}

int CHARACTER_MANAGER::GetMobDamageRate(CHARACTER * ch)	
{ 
	return m_iMobDamageRate; 
}

int CHARACTER_MANAGER::GetMobGoldAmountRate(CHARACTER * ch)
{ 
	if ( !ch )
		return m_iMobGoldAmountRate;

	//PREVENT_TOXICATION_FOR_CHINA
	//END_PREVENT_TOXICATION_FOR_CHINA
	if (ch && ch->GetPremiumRemainSeconds(PREMIUM_GOLD) > 0)
		return m_iMobGoldAmountRatePremium;
	return m_iMobGoldAmountRate; 
}

int CHARACTER_MANAGER::GetMobGoldDropRate(CHARACTER * ch)
{
	if ( !ch )
		return m_iMobGoldDropRate;

	//PREVENT_TOXICATION_FOR_CHINA
	//END_PREVENT_TOXICATION_FOR_CHINA
	
	if (ch && ch->GetPremiumRemainSeconds(PREMIUM_GOLD) > 0)
		return m_iMobGoldDropRatePremium;
	return m_iMobGoldDropRate;
}

int CHARACTER_MANAGER::GetMobExpRate(CHARACTER * ch)
{ 
	if ( !ch )
		return m_iMobExpRate;

	if (ch && ch->GetPremiumRemainSeconds(PREMIUM_EXP) > 0)
		return m_iMobExpRatePremium;
	return m_iMobExpRate; 
}

void CHARACTER_MANAGER::SendScriptToMap(int32_t lMapIndex, const std::string & s)
{
	SECTREE_MAP * pSecMap = SECTREE_MANAGER::instance().GetMap(lMapIndex);

	if (nullptr == pSecMap)
		return;

	struct packet_script p;

	p.header = HEADER_GC_SCRIPT;
	p.skin = 1;
	p.src_size = s.size();

	quest::FSendPacket f;
	p.size = p.src_size + sizeof(struct packet_script);
	f.buf.write(&p, sizeof(struct packet_script));
	f.buf.write(&s[0], s.size());

	pSecMap->for_each(f);
}

bool CHARACTER_MANAGER::BeginPendingDestroy()
{
	// to support not flushing when Begin is called again after Begin
	// return false if already started
	if (m_bUsePendingDestroy)
		return false;

	m_bUsePendingDestroy = true;
	return true;
}

void CHARACTER_MANAGER::FlushPendingDestroy()
{
	m_bUsePendingDestroy = false; // the flag must be set first for the actual Destroy to happen

	if (!m_set_pkChrPendingDestroy.empty())
	{
		metin2::log::info( "FlushPendingDestroy size {:d}", m_set_pkChrPendingDestroy.size());
		
		CHARACTER_SET::iterator it = m_set_pkChrPendingDestroy.begin(),
			end = m_set_pkChrPendingDestroy.end();
		for ( ; it != end; ++it) {
			CHARACTER_MANAGER::instance().DestroyCharacter(*it);
		}

		m_set_pkChrPendingDestroy.clear();
	}
}

CharacterVectorInteractor::CharacterVectorInteractor(const CHARACTER_SET & r)
{
	reserve(r.size());
	insert(end(), r.begin(), r.end());

	if (CHARACTER_MANAGER::instance().BeginPendingDestroy())
		m_bMyBegin = true;
}

CharacterVectorInteractor::~CharacterVectorInteractor()
{
	if (m_bMyBegin)
		CHARACTER_MANAGER::instance().FlushPendingDestroy();
}

