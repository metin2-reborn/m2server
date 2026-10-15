#include "dungeon.h"

#include "affect.h"
#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "desc.h"
#include "entity.h"
#include "event.h"
#include "item_manager.h"
#include "packet.h"
#include "party.h"
#include "questmanager.h"
#include "regen.h"
#include "sectree_manager.h"
#include "start_position.h"
#include "typedef.h"
#include "utils.h"

#include <common/length.h>
#include <metin2/log/log.h>
#include <libthecore/utils.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>

namespace metin2::game {

CDungeon::CDungeon(IdType id, int32_t lOriginalMapIndex, int32_t lMapIndex)
	: m_id(id),
	m_lMapIndex(lMapIndex),
	m_map_Area(SECTREE_MANAGER::instance().GetDungeonArea(lOriginalMapIndex))
{
	Initialize();
}

CDungeon::~CDungeon()
{
	if (m_pParty != nullptr)
	{
		m_pParty->SetDungeon_for_Only_party (nullptr);
	}
	ClearRegen();
	event_cancel(&deadEvent);
	// <Factor>
	event_cancel(&exit_all_event_);
	event_cancel(&jump_to_event_);
}

void CDungeon::Initialize()
{
	deadEvent = nullptr;
	// <Factor>
	exit_all_event_ = nullptr;
	jump_to_event_ = nullptr;
	regen_id_ = 0;

	m_iMobKill = 0;
	m_iStoneKill = 0;
	m_bUsePotion = false;
	m_bUseRevive = false;

	m_iMonsterCount = 0;

	m_bExitAllAtEliminate = false;
	m_bWarpAtEliminate = false;

	m_iWarpDelay = 0;
	m_lWarpMapIndex = 0;
	m_lWarpX = 0;
	m_lWarpY = 0;

	m_stRegenFile = "";

	m_pParty = nullptr;
}

void CDungeon::SetFlag(std::string name, int value)
{
	auto it =  m_map_Flag.find(name);
	if (it != m_map_Flag.end())
		it->second = value;
	else
		m_map_Flag.insert(make_pair(name, value));
}

int CDungeon::GetFlag(std::string name)
{
	auto it =  m_map_Flag.find(name);
	if (it != m_map_Flag.end())
		return it->second;
	else
		return 0;
}

struct FSendDestPosition
{
	FSendDestPosition(int32_t x, int32_t y)
	{
		p1.bHeader = HEADER_GC_DUNGEON;
		p1.subheader = DUNGEON_SUBHEADER_GC_DESTINATION_POSITION;
		p2.x = x;
		p2.y = y;
		p1.size = sizeof(p1)+sizeof(p2);
	}

	void operator()(CHARACTER * ch)
	{
		ch->GetDesc()->BufferedPacket(&p1, sizeof(TPacketGCDungeon));
		ch->GetDesc()->Packet(&p2, sizeof(TPacketGCDungeonDestPosition));
	}

	TPacketGCDungeon p1;
	TPacketGCDungeonDestPosition p2;
};

void CDungeon::SendDestPositionToParty(CParty * pParty, int32_t x, int32_t y)
{
	if (m_map_pkParty.find(pParty) == m_map_pkParty.end())
	{
		metin2::log::error("PARTY {:d} not in DUNGEON {:d}", pParty->GetLeaderPID(), m_lMapIndex);
		return;
	}

	FSendDestPosition f(x, y);
	pParty->ForEachNearMember(f);
}

struct FWarpToDungeon
{
	FWarpToDungeon(int32_t lMapIndex, CDungeon * d)
		: m_lMapIndex(lMapIndex), m_pkDungeon(d)
		{
			SECTREE_MAP * pkSectreeMap = SECTREE_MANAGER::instance().GetMap(lMapIndex);
			m_x = pkSectreeMap->m_setting.posSpawn.x;
			m_y = pkSectreeMap->m_setting.posSpawn.y; 
		}

	void operator () (CHARACTER * ch)
	{
		ch->SaveExitLocation();
		ch->WarpSet(m_x, m_y, m_lMapIndex);
	}

	int32_t m_lMapIndex;
	int32_t m_x;
	int32_t m_y;
	CDungeon * m_pkDungeon;
};

void CDungeon::Join(CHARACTER * ch)
{
	if (SECTREE_MANAGER::instance().GetMap(m_lMapIndex) == nullptr) {
		metin2::log::error("CDungeon: SECTREE_MAP not found for #{:d}", m_lMapIndex);
		return;
	}
	FWarpToDungeon(m_lMapIndex, this) (ch);
}

void CDungeon::JoinParty(CParty * pParty)
{
	pParty->SetDungeon(this);
	m_map_pkParty.insert(std::make_pair(pParty,0));

	if (SECTREE_MANAGER::instance().GetMap(m_lMapIndex) == nullptr) {
		metin2::log::error("CDungeon: SECTREE_MAP not found for #{:d}", m_lMapIndex);
		return;
	}
	FWarpToDungeon f(m_lMapIndex, this);
	pParty->ForEachOnlineMember(f);
}

void CDungeon::QuitParty(CParty * pParty)
{
	pParty->SetDungeon(nullptr);
	TPartyMap::iterator it = m_map_pkParty.find(pParty);

	if (it != m_map_pkParty.end())
		m_map_pkParty.erase(it);
}

EVENTINFO(dungeon_id_info)
{
	CDungeon::IdType dungeon_id;

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
		metin2::log::error( "dungeon_dead_event> <Factor> Null pointer" );
		return 0;
	}

	CDungeon * pDungeon = CDungeonManager::instance().Find(info->dungeon_id);
	if (pDungeon == nullptr) {
		return 0;
	}

	pDungeon->deadEvent = nullptr;

	CDungeonManager::instance().Destroy(info->dungeon_id);
	return 0;
}

void CDungeon::IncMember(CHARACTER * ch)
{
	if (m_set_pkCharacter.find(ch) == m_set_pkCharacter.end())
		m_set_pkCharacter.insert(ch);

	event_cancel(&deadEvent);
}

void CDungeon::DecMember(CHARACTER * ch)
{
	auto it = m_set_pkCharacter.find(ch);

	if (it == m_set_pkCharacter.end()) {
		return;
	}

	m_set_pkCharacter.erase(it);

	if (m_set_pkCharacter.empty())
	{
		dungeon_id_info* info = AllocEventInfo<dungeon_id_info>();
		info->dungeon_id = m_id;

		event_cancel(&deadEvent);
		deadEvent = event_create(dungeon_dead_event, info, PASSES_PER_SEC(10));
	}
}

void CDungeon::IncPartyMember(CParty * pParty, CHARACTER * ch)
{
	TPartyMap::iterator it = m_map_pkParty.find(pParty);

	if (it != m_map_pkParty.end())
		it->second++;
	else
		m_map_pkParty.insert(std::make_pair(pParty,1));

	IncMember(ch);
}

void CDungeon::DecPartyMember(CParty * pParty, CHARACTER * ch)
{
	TPartyMap::iterator it = m_map_pkParty.find(pParty);

	if (it == m_map_pkParty.end())
		metin2::log::error("cannot find party");
	else
	{
		it->second--;

		if (it->second == 0)
			QuitParty(pParty);
	}

	DecMember(ch);
}

struct FWarpToPosition
{
	int32_t lMapIndex;
	int32_t x;
	int32_t y;
	FWarpToPosition(int32_t lMapIndex, int32_t x, int32_t y)
		: lMapIndex(lMapIndex), x(x), y(y)
		{}

	void operator()(CEntity * ent)
	{
		if (!ent->IsType(ENTITY_CHARACTER)) {
			return;
		}
		CHARACTER * ch = (CHARACTER *)ent;
		if (!ch->IsPC()) {
			return;
		}
		if (ch->GetMapIndex() == lMapIndex)
		{
			ch->Show(lMapIndex, x, y, 0);
			ch->Stop();
		}
		else
		{
			ch->WarpSet(x,y,lMapIndex);
		}
	}
};

struct FWarpToPositionForce
{
	int32_t lMapIndex;
	int32_t x;
	int32_t y;
	FWarpToPositionForce(int32_t lMapIndex, int32_t x, int32_t y)
		: lMapIndex(lMapIndex), x(x), y(y)
		{}

	void operator()(CEntity * ent)
	{
		if (!ent->IsType(ENTITY_CHARACTER)) {
			return;
		}
		CHARACTER * ch = (CHARACTER *)ent;
		if (!ch->IsPC()) {
			return;
		}
		ch->WarpSet(x,y,lMapIndex);
	}
};

void CDungeon::JumpAll(int32_t lFromMapIndex, int x, int y)
{
	x *= 100;
	y *= 100;

	SECTREE_MAP * pMap = SECTREE_MANAGER::instance().GetMap(lFromMapIndex);

	if (!pMap)
	{
		metin2::log::error("cannot find map by index {:d}", lFromMapIndex);
		return;
	}

	FWarpToPosition f(m_lMapIndex, x, y);

	// <Factor> SECTREE::for_each -> SECTREE::for_each_entity
	pMap->for_each(f);
}

void CDungeon::WarpAll(int32_t lFromMapIndex, int x, int y)
{
	x *= 100;
	y *= 100;

	SECTREE_MAP * pMap = SECTREE_MANAGER::instance().GetMap(lFromMapIndex);

	if (!pMap)
	{
		metin2::log::error("cannot find map by index {:d}", lFromMapIndex);
		return;
	}

	FWarpToPositionForce f(m_lMapIndex, x, y);

	// <Factor> SECTREE::for_each -> SECTREE::for_each_entity
	pMap->for_each(f);
}

void CDungeon::JumpParty(CParty * pParty, int32_t lFromMapIndex, int x, int y)
{
	x *= 100;
	y *= 100;

	SECTREE_MAP * pMap = SECTREE_MANAGER::instance().GetMap(lFromMapIndex);

	if (!pMap)
	{
		metin2::log::error("cannot find map by index {:d}", lFromMapIndex);
		return;
	}

	if (pParty->GetDungeon_for_Only_party() == nullptr)
	{
		if (m_pParty == nullptr)
		{
			m_pParty = pParty;
		}
		else if (m_pParty != pParty)
		{
			metin2::log::error("Dungeon already has party. Another party cannot jump in dungeon : index {:d}", GetMapIndex());
			return;
		}
		pParty->SetDungeon_for_Only_party (this);
	}

	FWarpToPosition f(m_lMapIndex, x, y);

	pParty->ForEachOnMapMember(f, lFromMapIndex);
}

void CDungeon::SetPartyNull()
{
	m_pParty = nullptr;
}


void CDungeonManager::Destroy(CDungeon::IdType dungeon_id)
{
	metin2::log::info( "DUNGEON destroy : map index {:d}", dungeon_id);
	CDungeon * pDungeon = Find(dungeon_id);
	if (pDungeon == nullptr) {
		return;
	}
	m_map_pkDungeon.erase(dungeon_id);

	int32_t lMapIndex = pDungeon->m_lMapIndex;
	m_map_pkMapDungeon.erase(lMapIndex);

	uint32_t server_timer_arg = lMapIndex;
	quest::CQuestManager::instance().CancelServerTimers(server_timer_arg);

	SECTREE_MANAGER::instance().DestroyPrivateMap(lMapIndex);
	delete pDungeon;
}

CDungeon * CDungeonManager::Find(CDungeon::IdType dungeon_id)
{
	auto it = m_map_pkDungeon.find(dungeon_id);
	if (it != m_map_pkDungeon.end())
		return it->second;
	return nullptr;
}

CDungeon * CDungeonManager::FindByMapIndex(int32_t lMapIndex)
{
	auto it = m_map_pkMapDungeon.find(lMapIndex);
	if (it != m_map_pkMapDungeon.end()) {
		return it->second;
	}
	return nullptr;
}

CDungeon * CDungeonManager::Create(int32_t lOriginalMapIndex)
{
	uint32_t lMapIndex = SECTREE_MANAGER::instance().CreatePrivateMap(lOriginalMapIndex);

	if (!lMapIndex) 
	{
		metin2::log::info( "Fail to Create Dungeon : OrginalMapindex {:d} NewMapindex {:d}", lOriginalMapIndex, lMapIndex );
		return nullptr;
	}

	// <Factor> TODO: Change id assignment, or drop it
	CDungeon::IdType id = next_id_++;
	while (Find(id) != nullptr) {
		id = next_id_++;
	}

	CDungeon * pDungeon = new CDungeon(id, lOriginalMapIndex, lMapIndex);
	if (!pDungeon)
	{
		metin2::log::error("M2_NEW CDungeon failed");
		return nullptr;
	}
	m_map_pkDungeon.insert(std::make_pair(id, pDungeon));
	m_map_pkMapDungeon.insert(std::make_pair(lMapIndex, pDungeon));

	return pDungeon;
}

CDungeonManager::CDungeonManager()
	: next_id_(0)
{
}

CDungeonManager::~CDungeonManager()
{
}

void CDungeon::UniqueSetMaxHP(const std::string& key, int iMaxHP)
{
	TUniqueMobMap::iterator it = m_map_UniqueMob.find(key);
	if (it == m_map_UniqueMob.end())
	{
		metin2::log::error("Unknown Key : {}", key.c_str());
		return;
	}
	it->second->SetMaxHP(iMaxHP);
}

void CDungeon::UniqueSetHP(const std::string& key, int iHP)
{
	TUniqueMobMap::iterator it = m_map_UniqueMob.find(key);
	if (it == m_map_UniqueMob.end())
	{
		metin2::log::error("Unknown Key : {}", key.c_str());
		return;
	}
	it->second->SetHP(iHP);
}

void CDungeon::UniqueSetDefGrade(const std::string& key, int iGrade)
{
	TUniqueMobMap::iterator it = m_map_UniqueMob.find(key);
	if (it == m_map_UniqueMob.end())
	{
		metin2::log::error("Unknown Key : {}", key.c_str());
		return;
	}
	it->second->PointChange(POINT_DEF_GRADE,iGrade - it->second->GetPoint(POINT_DEF_GRADE));
}

void CDungeon::SpawnMoveUnique(const char* key, uint32_t vnum, const char* pos_from, const char* pos_to)
{
	TAreaMap::iterator it_to = m_map_Area.find(pos_to);
	if (it_to == m_map_Area.end())
	{
		metin2::log::error("Wrong position string : {}", pos_to);
		return;
	}

	TAreaMap::iterator it_from = m_map_Area.find(pos_from);
	if (it_from == m_map_Area.end())
	{
		metin2::log::error("Wrong position string : {}", pos_from);
		return;
	}

	TAreaInfo & ai = it_from->second;
	TAreaInfo & ai_to = it_to->second;
	int dir = ai.dir;
	if (dir==-1)
		dir = number(0,359);

	SECTREE_MAP * pkSectreeMap = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);
	if (pkSectreeMap == nullptr) {
		metin2::log::error("CDungeon: SECTREE_MAP not found for #{:d}", m_lMapIndex);
		return;
	}
	for (int i=0;i<100;i++)
	{
		int dx = number(ai.sx, ai.ex);
		int dy = number(ai.sy, ai.ey);
		int tx = number(ai_to.sx, ai_to.ex);
		int ty = number(ai_to.sy, ai_to.ey);

		CHARACTER * ch = CHARACTER_MANAGER::instance().SpawnMob(vnum, m_lMapIndex, pkSectreeMap->m_setting.iBaseX+dx, pkSectreeMap->m_setting.iBaseY+dy, 0, false, dir);

		if (ch)
		{
			m_map_UniqueMob.insert(make_pair(std::string(key), ch));
			ch->AddAffect(AFFECT_DUNGEON_UNIQUE, POINT_NONE, 0, AFF_DUNGEON_UNIQUE, 65535, 0, true);
			ch->SetDungeon(this);

			if (ch->Goto(pkSectreeMap->m_setting.iBaseX+tx, pkSectreeMap->m_setting.iBaseY+ty))
				ch->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);
		}
		else
		{
			metin2::log::error("Cannot spawn at {:d} {:d}", pkSectreeMap->m_setting.iBaseX+((ai.sx+ai.ex)>>1), pkSectreeMap->m_setting.iBaseY+((ai.sy+ai.ey)>>1));
		}
	}

}

void CDungeon::SpawnUnique(const char* key, uint32_t vnum, const char* pos)
{
	TAreaMap::iterator it = m_map_Area.find(pos);
	if (it == m_map_Area.end())
	{
		metin2::log::error("Wrong position string : {}", pos);
		return;
	}

	TAreaInfo & ai = it->second;
	int dir = ai.dir;
	if (dir==-1)
		dir = number(0,359);

	SECTREE_MAP * pkSectreeMap = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);
	if (pkSectreeMap == nullptr) {
		metin2::log::error("CDungeon: SECTREE_MAP not found for #{:d}", m_lMapIndex);
		return;
	}
	for (int i=0;i<100;i++)
	{
		int dx = number(ai.sx, ai.ex);
		int dy = number(ai.sy, ai.ey);

		CHARACTER * ch = CHARACTER_MANAGER::instance().SpawnMob(vnum, m_lMapIndex, pkSectreeMap->m_setting.iBaseX+dx, pkSectreeMap->m_setting.iBaseY+dy, 0, false, dir);

		if (ch)
		{
			m_map_UniqueMob.insert(make_pair(std::string(key), ch));
			ch->SetDungeon(this);
			ch->AddAffect(AFFECT_DUNGEON_UNIQUE, POINT_NONE, 0, AFF_DUNGEON_UNIQUE, 65535, 0, true);
			break;
		}
		else
		{
			metin2::log::error("Cannot spawn at {:d} {:d}", pkSectreeMap->m_setting.iBaseX+((ai.sx+ai.ex)>>1), pkSectreeMap->m_setting.iBaseY+((ai.sy+ai.ey)>>1));
		}
	}
}

void CDungeon::SetUnique(const char* key, uint32_t vid)
{
	CHARACTER * ch = CHARACTER_MANAGER::instance().Find(vid);
	if (ch)
	{
		m_map_UniqueMob.insert(make_pair(std::string(key), ch));
		ch->AddAffect(AFFECT_DUNGEON_UNIQUE, POINT_NONE, 0, AFF_DUNGEON_UNIQUE, 65535, 0, true);
	}
}

void CDungeon::SpawnStoneDoor(const char* key, const char* pos) 
{
	SpawnUnique(key, 13001, pos);
}

void CDungeon::SpawnWoodenDoor(const char* key, const char* pos)
{
	SpawnUnique(key, 13000, pos);
	UniqueSetMaxHP(key, 10000);
	UniqueSetHP(key, 10000);
	UniqueSetDefGrade(key, 300);
}

void CDungeon::PurgeUnique(const std::string& key)
{
	TUniqueMobMap::iterator it = m_map_UniqueMob.find(key);
	if (it == m_map_UniqueMob.end())
	{
		metin2::log::error("Unknown Key or Dead: {}", key.c_str());
		return;
	}
	CHARACTER * ch = it->second;
	m_map_UniqueMob.erase(it);
	CHARACTER_MANAGER::instance().DestroyCharacter(ch);
}

void CDungeon::KillUnique(const std::string& key)
{
	TUniqueMobMap::iterator it = m_map_UniqueMob.find(key);
	if (it == m_map_UniqueMob.end())
	{
		metin2::log::error("Unknown Key or Dead: {}", key.c_str());
		return;
	}
	CHARACTER * ch = it->second;
	m_map_UniqueMob.erase(it);
	ch->Dead();
}

uint32_t CDungeon::GetUniqueVid(const std::string& key)
{
	TUniqueMobMap::iterator it = m_map_UniqueMob.find(key);
	if (it == m_map_UniqueMob.end())
	{
		metin2::log::error("Unknown Key or Dead: {}", key.c_str());
		return 0;
	}
	CHARACTER * ch = it->second;
	return ch->GetVID();
}

float CDungeon::GetUniqueHpPerc(const std::string& key)
{
	TUniqueMobMap::iterator it = m_map_UniqueMob.find(key);
	if (it == m_map_UniqueMob.end())
	{
		metin2::log::error("Unknown Key : {}", key.c_str());
		return false;
	}
	return (100.f*it->second->GetHP())/it->second->GetMaxHP();
}

void CDungeon::DeadCharacter(CHARACTER * ch)
{
	if (!ch->IsPC())
	{
		TUniqueMobMap::iterator it = m_map_UniqueMob.begin();
		while (it!=m_map_UniqueMob.end())
		{
			if (it->second == ch)
			{
				m_map_UniqueMob.erase(it);
				break;
			}
			++it;
		}
	}
}

bool CDungeon::IsUniqueDead(const std::string& key)
{
	TUniqueMobMap::iterator it = m_map_UniqueMob.find(key);

	if (it == m_map_UniqueMob.end())
	{
		metin2::log::error("Unknown Key or Dead : {}", key.c_str());
		return true;
	}

	return it->second->IsDead();
}

void CDungeon::Spawn(uint32_t vnum, const char* pos)
{
	TAreaMap::iterator it = m_map_Area.find(pos);

	if (it == m_map_Area.end())
	{
		metin2::log::error("Wrong position string : {}", pos);
		return;
	}

	TAreaInfo & ai = it->second;
	int dir = ai.dir;
	if (dir==-1)
		dir = number(0,359);

	SECTREE_MAP * pkSectreeMap = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);
	if (pkSectreeMap == nullptr)
	{
		metin2::log::error("cannot find map by index {:d}", m_lMapIndex);
		return;
	}
	int dx = number(ai.sx, ai.ex);
	int dy = number(ai.sy, ai.ey);

	CHARACTER * ch = CHARACTER_MANAGER::instance().SpawnMob(vnum, m_lMapIndex, pkSectreeMap->m_setting.iBaseX+dx, pkSectreeMap->m_setting.iBaseY+dy, 0, false, dir);
	if (ch)
		ch->SetDungeon(this);
}

CHARACTER * CDungeon::SpawnMob(uint32_t vnum, int x, int y, int dir)
{
	SECTREE_MAP * pkSectreeMap = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);
	if (pkSectreeMap == nullptr) {
		metin2::log::error("CDungeon: SECTREE_MAP not found for #{:d}", m_lMapIndex);
		return nullptr;
	}
	metin2::log::info( "CDungeon::SpawnMob {:d} {:d} {:d}", vnum, x,  y);

	CHARACTER * ch = CHARACTER_MANAGER::instance().SpawnMob(vnum, m_lMapIndex, pkSectreeMap->m_setting.iBaseX+x*100, pkSectreeMap->m_setting.iBaseY+y*100, 0, false, dir == 0 ? -1 : (dir - 1) * 45);

	if (ch)
	{
		ch->SetDungeon(this);
		metin2::log::info( "CDungeon::SpawnMob name {}", ch->GetName());
	}

	return ch;
}

CHARACTER * CDungeon::SpawnMob_ac_dir(uint32_t vnum, int x, int y, int dir)
{
	SECTREE_MAP * pkSectreeMap = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);
	if (pkSectreeMap == nullptr) {
		metin2::log::error("CDungeon: SECTREE_MAP not found for #{:d}", m_lMapIndex);
		return nullptr;
	}
	metin2::log::info( "CDungeon::SpawnMob {:d} {:d} {:d}", vnum, x,  y);

	CHARACTER * ch = CHARACTER_MANAGER::instance().SpawnMob(vnum, m_lMapIndex, pkSectreeMap->m_setting.iBaseX+x*100, pkSectreeMap->m_setting.iBaseY+y*100, 0, false, dir);

	if (ch)
	{
		ch->SetDungeon(this);
		metin2::log::info( "CDungeon::SpawnMob name {}", ch->GetName());
	}

	return ch;
}

void CDungeon::SpawnNameMob(uint32_t vnum, int x, int y, const char* name)
{
	SECTREE_MAP * pkSectreeMap = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);
	if (pkSectreeMap == nullptr) {
		metin2::log::error("CDungeon: SECTREE_MAP not found for #{:d}", m_lMapIndex);
		return;
	}

	CHARACTER * ch = CHARACTER_MANAGER::instance().SpawnMob(vnum, m_lMapIndex, pkSectreeMap->m_setting.iBaseX+x, pkSectreeMap->m_setting.iBaseY+y, 0, false, -1);
	if (ch)
	{
		ch->SetName(name);
		ch->SetDungeon(this);
	}
}

void CDungeon::SpawnGotoMob(int32_t lFromX, int32_t lFromY, int32_t lToX, int32_t lToY)
{
	const int MOB_GOTO_VNUM = 20039;

	SECTREE_MAP * pkSectreeMap = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);
	if (pkSectreeMap == nullptr) {
		metin2::log::error("CDungeon: SECTREE_MAP not found for #{:d}", m_lMapIndex);
		return;
	}

	metin2::log::info( "SpawnGotoMob {:d} {:d} to {:d} {:d}", lFromX, lFromY, lToX, lToY);

	lFromX = pkSectreeMap->m_setting.iBaseX+lFromX*100;
	lFromY = pkSectreeMap->m_setting.iBaseY+lFromY*100;

	CHARACTER * ch = CHARACTER_MANAGER::instance().SpawnMob(MOB_GOTO_VNUM, m_lMapIndex, lFromX, lFromY, 0, false, -1);

	if (ch)
	{
		char buf[30+1];
		snprintf(buf, sizeof(buf), ". %d %d", lToX, lToY);

		ch->SetName(buf);
		ch->SetDungeon(this);
	}
}

CHARACTER * CDungeon::SpawnGroup(uint32_t vnum, int32_t x, int32_t y, float radius, bool bAggressive, int count)
{
	SECTREE_MAP * pkSectreeMap = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);
	if (pkSectreeMap == nullptr) {
		metin2::log::error("CDungeon: SECTREE_MAP not found for #{:d}", m_lMapIndex);
		return nullptr;
	}

	int iRadius = (int) radius;

	int sx = pkSectreeMap->m_setting.iBaseX + x - iRadius;
	int sy = pkSectreeMap->m_setting.iBaseY + y - iRadius;
	int ex = sx + iRadius;
	int ey = sy + iRadius;

	CHARACTER * ch = nullptr;

	while (count--)
	{
		CHARACTER * chLeader = CHARACTER_MANAGER::instance().SpawnGroup(vnum, m_lMapIndex, sx, sy, ex, ey, nullptr, bAggressive, this);
		if (chLeader && !ch)
			ch = chLeader;
	}

	return ch;
}

void CDungeon::SpawnRegen(const char* filename, bool bOnce)
{
	if (!filename)
	{
		metin2::log::error("CDungeon::SpawnRegen(filename=NULL, bOnce={:d}) - m_lMapIndex[{:d}]", bOnce, m_lMapIndex); 
		return;
	}

	SECTREE_MAP * pkSectreeMap = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);
	if (!pkSectreeMap)
	{
		metin2::log::error("CDungeon::SpawnRegen(filename={}, bOnce={:d}) - m_lMapIndex[{:d}]", filename, bOnce, m_lMapIndex); 
		return;
	}
	regen_do(filename, m_lMapIndex, pkSectreeMap->m_setting.iBaseX, pkSectreeMap->m_setting.iBaseY, this, bOnce);
}

void CDungeon::AddRegen(REGEN * regen)
{
	regen->id = regen_id_++;
	m_regen.push_back(regen);
}

void CDungeon::ClearRegen()
{
	for (auto it = m_regen.begin(); it != m_regen.end(); ++it)
	{
		REGEN * regen = *it;

		event_cancel(&regen->event);
		delete regen;
	}
	m_regen.clear();
}

bool CDungeon::IsValidRegen(REGEN * regen, size_t regen_id) {
	auto it = std::find(m_regen.begin(), m_regen.end(), regen);
	if (it == m_regen.end()) {
		return false;
	}
	auto * found = *it;
	return (found->id == regen_id);
}

void CDungeon::SpawnMoveGroup(uint32_t vnum, const char* pos_from, const char* pos_to, int count)
{
	TAreaMap::iterator it_to = m_map_Area.find(pos_to);

	if (it_to == m_map_Area.end())
	{
		metin2::log::error("Wrong position string : {}", pos_to);
		return;
	}

	TAreaMap::iterator it_from = m_map_Area.find(pos_from);

	if (it_from == m_map_Area.end())
	{
		metin2::log::error("Wrong position string : {}", pos_from);
		return;
	}

	TAreaInfo & ai = it_from->second;
	TAreaInfo & ai_to = it_to->second;
	int dir = ai.dir;

	if (dir == -1)
		dir = number(0,359);

	SECTREE_MAP * pkSectreeMap = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);
	if (pkSectreeMap == nullptr) {
		metin2::log::error("CDungeon: SECTREE_MAP not found for #{:d}", m_lMapIndex);
		return;
	}

	while (count--)
	{
		int tx = number(ai_to.sx, ai_to.ex)+pkSectreeMap->m_setting.iBaseX;
		int ty = number(ai_to.sy, ai_to.ey)+pkSectreeMap->m_setting.iBaseY;
		CHARACTER_MANAGER::instance().SpawnMoveGroup(vnum, m_lMapIndex, pkSectreeMap->m_setting.iBaseX+ai.sx, pkSectreeMap->m_setting.iBaseY+ai.sy, pkSectreeMap->m_setting.iBaseX+ai.ex, pkSectreeMap->m_setting.iBaseY+ai.ey, tx, ty, nullptr, true);
	}
}

namespace
{
	// DUNGEON_KILL_ALL_BUG_FIX
	struct FKillSectree
	{
		void operator () (CEntity * ent)
		{
			if (ent->IsType(ENTITY_CHARACTER))
			{
				CHARACTER * ch = (CHARACTER *) ent;

				if (!ch->IsPC() && !ch->IsPet())
					ch->Dead();
			}
		}
	};
	// END_OF_DUNGEON_KILL_ALL_BUG_FIX

	struct FPurgeSectree
	{
		void operator () (CEntity * ent)
		{
			if (ent->IsType(ENTITY_CHARACTER))
			{
				CHARACTER * ch = (CHARACTER *) ent;

				if (!ch->IsPC() && !ch->IsPet()) {
					CHARACTER_MANAGER::instance().DestroyCharacter(ch);
				}
			}
			else if (ent->IsType(ENTITY_ITEM))
			{
				CItem * item = (CItem *) ent;
				ITEM_MANAGER::instance().DestroyItem(item);
			}
			else
				metin2::log::error("unknown entity type {:d} is in dungeon", ent->GetType());
		}
	};
}

// DUNGEON_KILL_ALL_BUG_FIX
void CDungeon::KillAll()
{
	SECTREE_MAP * pkMap = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);
	if (pkMap == nullptr) {
		metin2::log::error("CDungeon: SECTREE_MAP not found for #{:d}", m_lMapIndex);
		return;
	}
	FKillSectree f;
	pkMap->for_each(f);
}
// END_OF_DUNGEON_KILL_ALL_BUG_FIX

void CDungeon::Purge()
{
	SECTREE_MAP * pkMap = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);
	if (pkMap == nullptr) {
		metin2::log::error("CDungeon: SECTREE_MAP not found for #{:d}", m_lMapIndex);
		return;
	}
	FPurgeSectree f;
	pkMap->for_each(f);
}

void CDungeon::IncKillCount(CHARACTER * pkKiller, CHARACTER * pkVictim)
{
	if (pkVictim->IsStone())
		m_iStoneKill ++;
	else
		m_iMobKill ++;
}

void CDungeon::UsePotion(CHARACTER * ch)
{
	m_bUsePotion = true;
}

void CDungeon::UseRevive(CHARACTER * ch)
{
	m_bUseRevive = true;
}

bool CDungeon::IsUsePotion()
{
	return m_bUsePotion;
}

bool CDungeon::IsUseRevive()
{
	return m_bUseRevive;
}

int CDungeon::GetKillMobCount()
{
	return m_iMobKill;
}
int CDungeon::GetKillStoneCount()
{
	return m_iStoneKill;
}


struct FExitDungeon
{
	void operator()(CEntity * ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			CHARACTER * ch = (CHARACTER *) ent;

			if (ch->IsPC())
				ch->ExitToSavedLocation();
		}
	}
};

void CDungeon::ExitAll()
{
	SECTREE_MAP * pMap = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);

	if (!pMap)
	{
		metin2::log::error("cannot find map by index {:d}", m_lMapIndex);
		return;
	}

	FExitDungeon f;

	// <Factor> SECTREE::for_each -> SECTREE::for_each_entity
	pMap->for_each(f);
}

// DUNGEON_NOTICE
namespace
{
	struct FNotice
	{
		FNotice(const char * psz) : m_psz(psz)
		{
		}

		void operator() (CEntity * ent)
		{
			if (ent->IsType(ENTITY_CHARACTER))
			{
				CHARACTER * ch = (CHARACTER *) ent;
				ch->ChatPacket(CHAT_TYPE_NOTICE, "%s", m_psz);
			}
		}

		const char * m_psz;
	};
}

void CDungeon::Notice(const char* msg)
{
	metin2::log::info( "XXX Dungeon Notice {} {}", static_cast<const void *>(this), msg);
	SECTREE_MAP * pMap = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);

	if (!pMap)
	{
		metin2::log::error("cannot find map by index {:d}", m_lMapIndex);
		return;
	}

	FNotice f(msg);
	pMap->for_each(f);
}
// END_OF_DUNGEON_NOTICE

struct FExitDungeonToStartPosition
{
	void operator () (CEntity * ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			CHARACTER * ch = (CHARACTER *) ent;

			if (ch->IsPC())
			{
				PIXEL_POSITION posWarp;

				// Store the starting map index, not the current map index.
				if (SECTREE_MANAGER::instance().GetRecallPositionByEmpire(g_start_map[ch->GetEmpire()], ch->GetEmpire(), posWarp))
					ch->WarpSet(posWarp.x, posWarp.y);
				else
					ch->ExitToSavedLocation();
			}
		}
	}
};

void CDungeon::ExitAllToStartPosition()
{
	SECTREE_MAP * pMap = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);

	if (!pMap)
	{
		metin2::log::error("cannot find map by index {:d}", m_lMapIndex);
		return;
	}

	FExitDungeonToStartPosition f;

	// <Factor> SECTREE::for_each -> SECTREE::for_each_entity
	pMap->for_each(f);
}

EVENTFUNC(dungeon_jump_to_event)
{
	dungeon_id_info * info = dynamic_cast<dungeon_id_info *>(event->info);

	if ( info == nullptr )
	{
		metin2::log::error( "dungeon_jump_to_event> <Factor> Null pointer" );
		return 0;
	}

	CDungeon * pDungeon = CDungeonManager::instance().Find(info->dungeon_id);
	pDungeon->jump_to_event_ = nullptr;

	if (pDungeon)
		pDungeon->JumpToEliminateLocation();
	else
		metin2::log::error("cannot find dungeon with map index {:d}", info->dungeon_id);

	return 0;
}

EVENTFUNC(dungeon_exit_all_event)
{
	dungeon_id_info * info = dynamic_cast<dungeon_id_info *>(event->info);

	if ( info == nullptr )
	{
		metin2::log::error( "dungeon_exit_all_event> <Factor> Null pointer" );
		return 0;
	}

	CDungeon * pDungeon = CDungeonManager::instance().Find(info->dungeon_id);
	pDungeon->exit_all_event_ = nullptr;

	if (pDungeon)
		pDungeon->ExitAll();

	return 0;
}

void CDungeon::CheckEliminated()
{
	if (m_iMonsterCount > 0)
		return;

	if (m_bExitAllAtEliminate)
	{
		metin2::log::info( "CheckEliminated: exit");
		m_bExitAllAtEliminate = false;

		if (m_iWarpDelay)
		{
			dungeon_id_info* info = AllocEventInfo<dungeon_id_info>();
			info->dungeon_id = m_id;

			event_cancel(&exit_all_event_);
			exit_all_event_ = event_create(dungeon_exit_all_event, info, PASSES_PER_SEC(m_iWarpDelay));
		}
		else
		{
			ExitAll();
		}
	}
	else if (m_bWarpAtEliminate)
	{
		metin2::log::info( "CheckEliminated: warp");
		m_bWarpAtEliminate = false;

		if (m_iWarpDelay)
		{
			dungeon_id_info* info = AllocEventInfo<dungeon_id_info>();
			info->dungeon_id = m_id;

			event_cancel(&jump_to_event_);
			jump_to_event_ = event_create(dungeon_jump_to_event, info, PASSES_PER_SEC(m_iWarpDelay));
		}
		else
		{
			JumpToEliminateLocation();
		}
	}
	else
		metin2::log::info( "CheckEliminated: none");
}

void CDungeon::SetExitAllAtEliminate(int32_t time)
{
	metin2::log::info( "SetExitAllAtEliminate: time {:d}", time);
	m_bExitAllAtEliminate = true;
	m_iWarpDelay = time;
}

void CDungeon::SetWarpAtEliminate(int32_t time, int32_t lMapIndex, int x, int y, const char* regen_file)
{
	m_bWarpAtEliminate = true;
	m_iWarpDelay = time;
	m_lWarpMapIndex = lMapIndex;
	m_lWarpX = x;
	m_lWarpY = y;

	if (!regen_file || !*regen_file)
		m_stRegenFile.clear();
	else
		m_stRegenFile = regen_file;

	metin2::log::info( "SetWarpAtEliminate: time {:d} map {:d} {:d}x{:d} regenfile {}", time, lMapIndex, x, y, m_stRegenFile.c_str());
}

void CDungeon::JumpToEliminateLocation()
{
	CDungeon * pDungeon = CDungeonManager::instance().FindByMapIndex(m_lWarpMapIndex);

	if (pDungeon)
	{
		pDungeon->JumpAll(m_lMapIndex, m_lWarpX, m_lWarpY);

		if (!m_stRegenFile.empty())
		{
			pDungeon->SpawnRegen(m_stRegenFile.c_str());
			m_stRegenFile.clear();
		}
	}
	else
	{
		// Warp to a normal map
		SECTREE_MAP * pMap = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);

		if (!pMap)
		{
			metin2::log::error("no map by index {:d}", m_lMapIndex);
			return;
		}

		FWarpToPosition f(m_lWarpMapIndex, m_lWarpX * 100, m_lWarpY * 100);

		// <Factor> SECTREE::for_each -> SECTREE::for_each_entity
		pMap->for_each(f);
	}
}

struct FNearPosition
{
	int32_t x;
	int32_t y;
	int dist;
	bool ret;

	FNearPosition(int32_t x, int32_t y, int d) :
		x(x), y(y), dist(d), ret(true)
	{
	}

	void operator()(CEntity * ent)
	{
		if (ret == false)
			return;

		if (ent->IsType(ENTITY_CHARACTER))
		{
			CHARACTER * ch = (CHARACTER *) ent;

			if (ch->IsPC())
			{
				if (DISTANCE_APPROX(ch->GetX() - x * 100, ch->GetY() - y * 100) > dist * 100)
					ret = false;
			}
		}
	}
};

bool CDungeon::IsAllPCNearTo(int x, int y, int dist)
{
	SECTREE_MAP * pMap = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);

	if (!pMap)
	{
		metin2::log::error("cannot find map by index {:d}", m_lMapIndex);
		return false;
	}

	FNearPosition f(x, y, dist);

	// <Factor> SECTREE::for_each -> SECTREE::for_each_entity
	pMap->for_each(f);

	return f.ret;
}

void CDungeon::CreateItemGroup (std::string& group_name, ItemGroup& item_group)
{
	m_map_ItemGroup.insert (ItemGroupMap::value_type (group_name, item_group));
}

const CDungeon::ItemGroup* CDungeon::GetItemGroup (std::string& group_name)
{
	ItemGroupMap::iterator it = m_map_ItemGroup.find (group_name);
	if (it != m_map_ItemGroup.end())
		return &(it->second);
	else
		return nullptr;
}
}
