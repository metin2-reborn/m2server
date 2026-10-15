#include "building.h"
#include <common/building.h>

#include "buffer_manager.h"
#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "desc.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "entity.h"
#include "guild.h"
#include "guild_manager.h"
#include "item_manager.h"
#include "packet.h"
#include "questmanager.h"
#include "questpc.h"
#include "sectree.h"
#include "sectree_manager.h"
#include "typedef.h"

#include <common/length.h>
#include <common/tables.h>
#include <metin2/log/log.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <numbers>
#include <utility>

enum
{
	// ADD_SUPPLY_BUILDING
	BUILDING_INCREASE_GUILD_MEMBER_COUNT_SMALL = 14061,
	BUILDING_INCREASE_GUILD_MEMBER_COUNT_MEDIUM = 14062,
	BUILDING_INCREASE_GUILD_MEMBER_COUNT_LARGE = 14063,
	// END_OF_ADD_SUPPLY_BUILDING

	FLAG_VNUM = 14200,
	WALL_DOOR_VNUM	= 14201,
	WALL_BACK_VNUM	= 14202,
	WALL_LEFT_VNUM	= 14203,
	WALL_RIGHT_VNUM	= 14204,
};

using namespace building;

CObject::CObject(TObject * pData, TObjectProto * pProto)
	: m_pProto(pProto), m_dwVID(0), m_chNPC(nullptr)
{
	CEntity::Initialize(ENTITY_OBJECT);

	std::memcpy(&m_data, pData, sizeof(TObject));
}

CObject::~CObject()
{
	Destroy();
}

void CObject::Destroy()
{
	if (m_pProto)
	{
		SECTREE_MANAGER::instance().ForAttrRegion(GetMapIndex(),
				GetX() + m_pProto->lRegion[0],
				GetY() + m_pProto->lRegion[1],
				GetX() + m_pProto->lRegion[2],
				GetY() + m_pProto->lRegion[3],
				(int32_t)m_data.zRot, // ADD_BUILDING_ROTATION
				ATTR_OBJECT,
				ATTR_REGION_MODE_REMOVE);
	}

	CEntity::Destroy();

	if (GetSectree())
		GetSectree()->RemoveEntity(this);

	// <Factor> NPC should be destroyed in CHARACTER_MANAGER
	// BUILDING_NPC

	RemoveSpecialEffect();
	// END_OF_BUILDING_NPC
}

// BUILDING_NPC
void CObject::Reconstruct(uint32_t dwVnum)
{
	const TMapRegion * r = SECTREE_MANAGER::instance().GetMapRegion(m_data.lMapIndex);
	if (!r)
		return;

	CLand* pLand = GetLand();
	pLand->RequestDeleteObject(GetID());
	pLand->RequestCreateObject(dwVnum, m_data.lMapIndex, m_data.x - r->sx, m_data.y - r->sy, m_data.xRot, m_data.yRot, m_data.zRot, false);
}
// END_OF_BUILDING_NPC

void CObject::EncodeInsertPacket(CEntity * entity)
{
	DESC * d;

	if (!(d = entity->GetDesc()))
		return;

	metin2::log::info( "ObjectInsertPacket vid {:d} vnum {:d} rot {:f} {:f} {:f}", 
			m_dwVID, m_data.dwVnum, m_data.xRot, m_data.yRot, m_data.zRot);

	TPacketGCCharacterAdd pack;

	memset(&pack, 0, sizeof(TPacketGCCharacterAdd));

	pack.header         = HEADER_GC_CHARACTER_ADD;
	pack.dwVID          = m_dwVID;
	pack.bType          = CHAR_TYPE_BUILDING;
	pack.angle          = m_data.zRot;
	pack.x              = GetX();
	pack.y              = GetY();
	pack.z              = GetZ();
	pack.wRaceNum       = m_data.dwVnum;

	// Convert building rotation info (door position for walls)
	pack.dwAffectFlag[0] = unsigned(m_data.xRot);
	pack.dwAffectFlag[1] = unsigned(m_data.yRot);


	if (GetLand())
	{
	}

	d->Packet(&pack, sizeof(pack));
}

void CObject::EncodeRemovePacket(CEntity * entity)
{
	DESC * d;

	if (!(d = entity->GetDesc()))
		return;

	metin2::log::info( "ObjectRemovePacket vid {:d}", m_dwVID);

	TPacketGCCharacterDelete pack;

	pack.header = HEADER_GC_CHARACTER_DEL;
	pack.id     = m_dwVID;

	d->Packet(&pack, sizeof(TPacketGCCharacterDelete));
}

void CObject::SetVID(uint32_t dwVID)
{
	m_dwVID = dwVID;
}

bool CObject::Show(int32_t lMapIndex, int32_t x, int32_t y)
{
	SECTREE * tree = SECTREE_MANAGER::instance().Get(lMapIndex, x, y);

	if (!tree)
	{
		metin2::log::error("cannot find sectree by {:d}x{:d} mapindex {:d}", x, y, lMapIndex);
		return false;
	}

	if (GetSectree())
	{
		GetSectree()->RemoveEntity(this);
		ViewCleanup();
	}

	m_data.lMapIndex = lMapIndex;
	m_data.x = x;
	m_data.y = y;

	Save();

	SetMapIndex(lMapIndex);
	SetXYZ(x, y, 0);

	tree->InsertEntity(this);
	UpdateSectree();

	SECTREE_MANAGER::instance().ForAttrRegion(lMapIndex,
			x + m_pProto->lRegion[0],
			y + m_pProto->lRegion[1],
			x + m_pProto->lRegion[2],
			y + m_pProto->lRegion[3],
			(int32_t)m_data.zRot,
			ATTR_OBJECT,
			ATTR_REGION_MODE_SET);

	return true;
}

void CObject::Save()
{
}

void CObject::ApplySpecialEffect()
{
	if (m_pProto)
	{
		// ADD_SUPPLY_BUILDING
		if (m_pProto->dwVnum == BUILDING_INCREASE_GUILD_MEMBER_COUNT_SMALL ||
				m_pProto->dwVnum == BUILDING_INCREASE_GUILD_MEMBER_COUNT_MEDIUM ||
				m_pProto->dwVnum == BUILDING_INCREASE_GUILD_MEMBER_COUNT_LARGE)
		{
			CLand* pLand = GetLand();
			uint32_t guild_id = 0;
			if (pLand)
				guild_id = pLand->GetOwner();
			CGuild* pGuild = CGuildManager::instance().FindGuild(guild_id);
			if (pGuild)
			{
				switch (m_pProto->dwVnum)
				{
					case BUILDING_INCREASE_GUILD_MEMBER_COUNT_SMALL:
						pGuild->SetMemberCountBonus(6);
						break;
					case BUILDING_INCREASE_GUILD_MEMBER_COUNT_MEDIUM:
						pGuild->SetMemberCountBonus(12);
						break;
					case BUILDING_INCREASE_GUILD_MEMBER_COUNT_LARGE:
						pGuild->SetMemberCountBonus(18);
						break;
				}
				if (map_allow_find(pLand->GetMapIndex()))
				{
					pGuild->BroadcastMemberCountBonus();
				}
			}
		}
		// END_OF_ADD_SUPPLY_BUILDING
	}
}

void CObject::RemoveSpecialEffect()
{
	if (m_pProto)
	{
		// ADD_SUPPLY_BUILDING
		if (m_pProto->dwVnum == BUILDING_INCREASE_GUILD_MEMBER_COUNT_SMALL ||
				m_pProto->dwVnum == BUILDING_INCREASE_GUILD_MEMBER_COUNT_MEDIUM ||
				m_pProto->dwVnum == BUILDING_INCREASE_GUILD_MEMBER_COUNT_LARGE)
		{
			CLand* pLand = GetLand();
			uint32_t guild_id = 0;
			if (pLand)
				guild_id = pLand->GetOwner();
			CGuild* pGuild = CGuildManager::instance().FindGuild(guild_id);
			if (pGuild)
			{
				pGuild->SetMemberCountBonus(0);
				if (map_allow_find(pLand->GetMapIndex()))
					pGuild->BroadcastMemberCountBonus();
			}
		}
		// END_OF_ADD_SUPPLY_BUILDING
	}
}

// BUILDING_NPC
void CObject::RegenNPC()
{
	if (!m_pProto)
		return;

	if (!m_pProto->dwNPCVnum)
		return;

	if (!m_pkLand)
		return;

	uint32_t dwGuildID = m_pkLand->GetOwner();
	CGuild* pGuild = CGuildManager::instance().FindGuild(dwGuildID);

	if (!pGuild)
		return;

	int x = m_pProto->lNPCX;
	int y = m_pProto->lNPCY;
	int newX, newY;

	float rot = m_data.zRot * 2.0f * std::numbers::pi / 360.0f;

	newX = (int)(( x * cosf(rot)) + ( y * sinf(rot)));
	newY = (int)(( y * cosf(rot)) - ( x * sinf(rot)));

	m_chNPC = CHARACTER_MANAGER::instance().SpawnMob(m_pProto->dwNPCVnum,
			GetMapIndex(),
			GetX() + newX,
			GetY() + newY,
			GetZ(),
			false,
			(int)m_data.zRot);


	if (!m_chNPC)
	{
		metin2::log::error("Cannot create guild npc");
		return;
	}

	m_chNPC->SetGuild(pGuild);

	// For the Temple of Power, store the guild level on the guild master
	if ( m_pProto->dwVnum == 14061 || m_pProto->dwVnum == 14062 || m_pProto->dwVnum == 14063 )
	{
		quest::PC* pPC = quest::CQuestManager::instance().GetPC(pGuild->GetMasterPID());

		if ( pPC != nullptr )
		{
			pPC->SetFlag("alter_of_power.build_level", pGuild->GetLevel());
		}
	}
}
// END_OF_BUILDING_NPC

////////////////////////////////////////////////////////////////////////////////////

CLand::CLand(TLand * pData)
{
	std::memcpy(&m_data, pData, sizeof(TLand));
}

CLand::~CLand()
{
	Destroy();
}

void CLand::Destroy()
{
	auto it = m_map_pkObject.begin();

	while (it != m_map_pkObject.end())
	{
		CObject * pkObj = (it++)->second;
		CManager::instance().UnregisterObject(pkObj);
		delete pkObj;
	}

	m_map_pkObject.clear();
	m_map_pkObjectByVID.clear();
}

const TLand & CLand::GetData()
{
	return m_data;
}

void CLand::PutData(const TLand * data)
{
	memcpy(&m_data, data, sizeof(TLand));

	if (m_data.dwGuildID)
	{
		const TMapRegion * r = SECTREE_MANAGER::instance().GetMapRegion(m_data.lMapIndex);

		if (r)
		{
			CharacterVectorInteractor i;

			if (CHARACTER_MANAGER::instance().GetCharactersByRaceNum(20040, i))
			{
				CharacterVectorInteractor::iterator it = i.begin();

				while (it != i.end())
				{
					CHARACTER * ch = *(it++);

					if (ch->GetMapIndex() != m_data.lMapIndex)
						continue;

					int x = ch->GetX() - r->sx;
					int y = ch->GetY() - r->sy;

					if (x > m_data.x + m_data.width || x < m_data.x)
						continue;

					if (y > m_data.y + m_data.height || y < m_data.y)
						continue;

					CHARACTER_MANAGER::instance().DestroyCharacter(ch);
				}
			}
		}
	}
}

void CLand::InsertObject(CObject * pkObj)
{
	m_map_pkObject.insert(std::make_pair(pkObj->GetID(), pkObj));
	m_map_pkObjectByVID.insert(std::make_pair(pkObj->GetVID(), pkObj));

	pkObj->SetLand(this);
}

CObject * CLand::FindObject(uint32_t dwID)
{
	std::map<uint32_t, CObject *>::iterator it = m_map_pkObject.find(dwID);

	if (it == m_map_pkObject.end())
		return nullptr;

	return it->second;
}

CObject * CLand::FindObjectByGroup(uint32_t dwGroupVnum)
{
	std::map<uint32_t, CObject *>::iterator it;
	for (it = m_map_pkObject.begin(); it != m_map_pkObject.end(); ++it)
	{
		CObject * pObj = it->second;
		if (pObj->GetGroup() == dwGroupVnum)
			return pObj;
	}

	return nullptr;
}

// BUILDING_NPC
CObject * CLand::FindObjectByNPC(CHARACTER * npc)
{
	if (!npc)
		return nullptr;

	std::map<uint32_t, CObject *>::iterator it;
	for (it = m_map_pkObject.begin(); it != m_map_pkObject.end(); ++it)
	{
		CObject * pObj = it->second;
		if (pObj->GetNPC() == npc)
			return pObj;
	}

	return nullptr;
}
// END_OF_BUILDING_NPC

CObject * CLand::FindObjectByVID(uint32_t dwVID)
{
	std::map<uint32_t, CObject *>::iterator it = m_map_pkObjectByVID.find(dwVID);

	if (it == m_map_pkObjectByVID.end())
		return nullptr;

	return it->second;
}

void CLand::DeleteObject(uint32_t dwID)
{
	CObject * pkObj;

	if (!(pkObj = FindObject(dwID)))
		return;

	metin2::log::info( "Land::DeleteObject {:d}", dwID);
	CManager::instance().UnregisterObject(pkObj);
	CHARACTER_MANAGER::instance().DestroyCharacter(pkObj->GetNPC());

	m_map_pkObject.erase(dwID);
	m_map_pkObjectByVID.erase(dwID);

	delete pkObj;
}

struct FIsIn
{
	int32_t sx, sy;
	int32_t ex, ey;
	
	bool bIn;
	FIsIn (	int32_t sx_, int32_t sy_, int32_t ex_, int32_t ey_)
		: sx(sx_), sy(sy_), ex(ex_), ey(ey_), bIn(false) 
	{}

	void operator () (CEntity * ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			CHARACTER * ch = (CHARACTER *) ent;
			if (ch->IsMonster())
			{
				return;
			}
			if (sx <= ch->GetX() && ch->GetX() <= ex
				&& sy <= ch->GetY() && ch->GetY() <= ey)
			{
				bIn = true;
			}
		}
	}
};

bool CLand::RequestCreateObject(uint32_t dwVnum, int32_t lMapIndex, int32_t x, int32_t y, float xRot, float yRot, float zRot, bool checkAnother)
{
	SECTREE_MANAGER& rkSecTreeMgr = SECTREE_MANAGER::instance();
	TObjectProto * pkProto = CManager::instance().GetObjectProto(dwVnum);

	if (!pkProto)
	{
		metin2::log::error("Invalid Object vnum {:d}", dwVnum);
		return false;
	}
	const TMapRegion * r = rkSecTreeMgr.GetMapRegion(lMapIndex);
	if (!r)
		return false;

	metin2::log::info( "RequestCreateObject(vnum={:d}, map={:d}, pos=({:d},{:d}), rot=({:.1f},{:.1f},{:.1f}) region({:d},{:d} ~ {:d},{:d})", 
			dwVnum, lMapIndex, x, y, xRot, yRot, zRot, r->sx, r->sy, r->ex, r->ey);

	x += r->sx;
	y += r->sy;

	int sx = r->sx + m_data.x;
	int ex = sx + m_data.width;
	int sy = r->sy + m_data.y;
	int ey = sy + m_data.height;

	int osx = x + pkProto->lRegion[0];
	int osy = y + pkProto->lRegion[1];
	int oex = x + pkProto->lRegion[2];
	int oey = y + pkProto->lRegion[3];

	float rad = zRot * 2.0f * std::numbers::pi / 360.0f;

	int tsx = (int)(pkProto->lRegion[0] * cosf(rad) + pkProto->lRegion[1] * sinf(rad) + x);
	int tsy = (int)(pkProto->lRegion[0] * -sinf(rad) + pkProto->lRegion[1] * cosf(rad) + y);

	int tex = (int)(pkProto->lRegion[2] * cosf(rad) + pkProto->lRegion[3] * sinf(rad) + x);
	int tey = (int)(pkProto->lRegion[2] * -sinf(rad) + pkProto->lRegion[3] * cosf(rad) + y);

	if (tsx < sx || tex > ex || tsy < sy || tey > ey)
	{
		metin2::log::error("invalid position: object is outside of land region\nLAND: {:d} {:d} ~ {:d} {:d}\nOBJ: {:d} {:d} ~ {:d} {:d}", sx, sy, ex, ey, osx, osy, oex, oey);
		return false;
	}

	// ADD_BUILDING_ROTATION
	if ( checkAnother )
	{
		if (rkSecTreeMgr.ForAttrRegion(lMapIndex, osx, osy, oex, oey, (int32_t)zRot, ATTR_OBJECT, ATTR_REGION_MODE_CHECK))
		{
			metin2::log::error("another object already exist");
			return false;
		}
		FIsIn f (osx, osy, oex, oey);
		rkSecTreeMgr.GetMap(lMapIndex)->for_each(f);
		
		if (f.bIn)
		{
			metin2::log::error("another object already exist");
			return false;
		}
	}
	// END_OF_BUILDING_NPC

	TPacketGDCreateObject p;

	p.dwVnum = dwVnum;
	p.dwLandID = m_data.dwID;
	p.lMapIndex = lMapIndex;
	p.x = x;
	p.y = y;
	p.xRot = xRot;
	p.yRot = yRot;
	p.zRot = zRot;

	db_clientdesc->DBPacket(HEADER_GD_CREATE_OBJECT, 0, &p, sizeof(TPacketGDCreateObject));
	return true;
}

void CLand::RequestDeleteObject(uint32_t dwID)
{
	if (!FindObject(dwID))
	{
		metin2::log::error("no object by id {:d}", dwID);
		return;
	}

	db_clientdesc->DBPacket(HEADER_GD_DELETE_OBJECT, 0, &dwID, sizeof(uint32_t));
	metin2::log::info( "RequestDeleteObject id {:d}", dwID);
}

void CLand::RequestDeleteObjectByVID(uint32_t dwVID)
{
	CObject * pkObj;

	if (!(pkObj = FindObjectByVID(dwVID)))
	{
		metin2::log::error("no object by vid {:d}", dwVID);
		return;
	}

	uint32_t dwID = pkObj->GetID();
	db_clientdesc->DBPacket(HEADER_GD_DELETE_OBJECT, 0, &dwID, sizeof(uint32_t));
	metin2::log::info( "RequestDeleteObject vid {:d} id {:d}", dwVID, dwID);
}

void CLand::SetOwner(uint32_t dwGuild)
{
	if (m_data.dwGuildID != dwGuild)
	{
		m_data.dwGuildID = dwGuild;
		RequestUpdate(dwGuild);
	}
}

void CLand::RequestUpdate(uint32_t dwGuild)
{
	uint32_t a[2];

	a[0] = GetID();
	a[1] = dwGuild;

	db_clientdesc->DBPacket(HEADER_GD_UPDATE_LAND, 0, &a[0], sizeof(uint32_t) * 2);
	metin2::log::info( "RequestUpdate id {:d} guild {:d}", a[0], a[1]);
}

////////////////////////////////////////////////////////////////////////////////////

CManager::CManager()
{
}

CManager::~CManager()
{
	Destroy();
}

void CManager::Destroy()
{
	m_map_pkLand.clear();
}

bool CManager::LoadObjectProto(const TObjectProto * pProto, int size) // from DB
{
	m_vec_kObjectProto.resize(size);
	std::memcpy(&m_vec_kObjectProto[0], pProto, sizeof(TObjectProto) * size);

	for (int i = 0; i < size; ++i)
	{
		TObjectProto & r = m_vec_kObjectProto[i];

		// BUILDING_NPC
		metin2::log::info( "ObjectProto {:d} price {:d} upgrade {:d} upg_limit {:d} life {:d} NPC {:d}",
				r.dwVnum, r.dwPrice, r.dwUpgradeVnum, r.dwUpgradeLimitTime, r.lLife, r.dwNPCVnum);
		// END_OF_BUILDING_NPC

		for (int j = 0; j < OBJECT_MATERIAL_MAX_NUM; ++j)
		{
			if (!r.kMaterials[j].dwItemVnum)
				break;

			if (nullptr == ITEM_MANAGER::instance().GetTable(r.kMaterials[j].dwItemVnum))
			{
				metin2::log::error("          mat: ERROR!! no item by vnum {:d}", r.kMaterials[j].dwItemVnum);
				return false;
			}

			metin2::log::info( "          mat: {:d} {:d}", r.kMaterials[j].dwItemVnum, r.kMaterials[j].dwCount);
		}

		m_map_pkObjectProto.insert(std::make_pair(r.dwVnum, &m_vec_kObjectProto[i]));
	}

	return true;
}

TObjectProto * CManager::GetObjectProto(uint32_t dwVnum)
{
	auto it = m_map_pkObjectProto.find(dwVnum);

	if (it == m_map_pkObjectProto.end())
		return nullptr;

	return it->second;
}

bool CManager::LoadLand(TLand * pTable) // from DB
{
	// Land must be loaded even if its map is not in MapAllow.
	//	To know which guild a building (object) belongs to, we must know which guild owns the land it was built on.
	//	If the land isn't loaded, we can't tell which guild a guild building belongs to,
	//	so the guild buff from guild buildings is not received.

	auto pkLand = std::make_unique<CLand>(pTable);
	uint32_t dwID = pkLand->GetID();
	m_map_pkLand.insert(std::make_pair(dwID, std::move(pkLand)));

	metin2::log::info( "LAND: {:d} map {:d} {:d}x{:d} w {:d} h {:d}", 
			pTable->dwID, pTable->lMapIndex, pTable->x, pTable->y, pTable->width, pTable->height);

	return true;
}

void CManager::UpdateLand(TLand * pTable)
{
	CLand * pkLand = FindLand(pTable->dwID);

	if (!pkLand)
	{
		metin2::log::error("cannot find land by id {:d}", pTable->dwID);
		return;
	}

	pkLand->PutData(pTable);

	const DESC_MANAGER::DESC_SET & cont = DESC_MANAGER::instance().GetClientSet();

	auto it = cont.begin();

	TPacketGCLandList p;

	p.header = HEADER_GC_LAND_LIST;
	p.size = sizeof(TPacketGCLandList) + sizeof(TLandPacketElement);

	TLandPacketElement e;

	e.dwID = pTable->dwID;
	e.x = pTable->x;
	e.y = pTable->y;
	e.width = pTable->width;
	e.height = pTable->height;
	e.dwGuildID = pTable->dwGuildID;

	metin2::log::info( "BUILDING: UpdateLand {:d} pos {:d}x{:d} guild {:d}", e.dwID, e.x, e.y, e.dwGuildID);

	CGuild *guild = CGuildManager::instance().FindGuild(pTable->dwGuildID);
	while (it != cont.end())
	{
		DESC * d = *(it++);

		if (d->GetCharacter() && d->GetCharacter()->GetMapIndex() == pTable->lMapIndex)
		{
			// we must send the guild name first
			d->GetCharacter()->SendGuildName(guild);

			d->BufferedPacket(&p, sizeof(TPacketGCLandList));
			d->Packet(&e, sizeof(TLandPacketElement));
		}
	}
}

CLand * CManager::FindLand(uint32_t dwID)
{
	auto it = m_map_pkLand.find(dwID);

	if (it == m_map_pkLand.end())
		return nullptr;

	return it->second.get();
}

CLand * CManager::FindLand(int32_t lMapIndex, int32_t x, int32_t y)
{
	metin2::log::info( "BUILDING: FindLand {:d} {:d} {:d}", lMapIndex, x, y);

	const TMapRegion * r = SECTREE_MANAGER::instance().GetMapRegion(lMapIndex);

	if (!r)
		return nullptr;

	x -= r->sx;
	y -= r->sy;

	auto it = m_map_pkLand.begin();

	while (it != m_map_pkLand.end())
	{
		CLand * pkLand = (it++)->second.get();
		const TLand & r = pkLand->GetData();

		if (r.lMapIndex != lMapIndex)
			continue;

		if (x < r.x || y < r.y)
			continue;

		if (x > r.x + r.width || y > r.y + r.height)
			continue;

		return pkLand;
	}

	return nullptr;
}

CLand * CManager::FindLandByGuild(uint32_t GID)
{
	auto it = m_map_pkLand.begin();

	while (it != m_map_pkLand.end())
	{
		CLand * pkLand = (it++)->second.get();

		if (pkLand->GetData().dwGuildID == GID)
			return pkLand;
	}

	return nullptr;
}

bool CManager::LoadObject(TObject * pTable, bool isBoot) // from DB
{
	if (!map_allow_find(pTable->lMapIndex)) // Fix
		return false;

	CLand * pkLand = FindLand(pTable->dwLandID);

	if (!pkLand)
	{
		metin2::log::info( "Cannot find land by id {:d}", pTable->dwLandID);
		return false;
	}

	TObjectProto * pkProto = GetObjectProto(pTable->dwVnum);

	if (!pkProto)
	{
		metin2::log::error("Cannot find object {:d} in prototype (id {:d})", pTable->dwVnum, pTable->dwID);
		return false;
	}

	metin2::log::info( "OBJ: id {:d} vnum {:d} map {:d} pos {:d}x{:d}", pTable->dwID, pTable->dwVnum, pTable->lMapIndex, pTable->x, pTable->y);

	CObject * pkObj = new CObject(pTable, pkProto);

	uint32_t dwVID = CHARACTER_MANAGER::instance().AllocVID();
	pkObj->SetVID(dwVID);

	m_map_pkObjByVID.insert(std::make_pair(dwVID, pkObj));
	m_map_pkObjByID.insert(std::make_pair(pTable->dwID, pkObj));

	pkLand->InsertObject(pkObj);

	if (!isBoot)
		pkObj->Show(pTable->lMapIndex, pTable->x, pTable->y);
	else
	{
		pkObj->SetMapIndex(pTable->lMapIndex);
		pkObj->SetXYZ(pTable->x, pTable->y, 0);
	}

	// BUILDING_NPC
	if (!isBoot)
	{ 
		if (pkProto->dwNPCVnum)
			pkObj->RegenNPC();

		pkObj->ApplySpecialEffect();
	}
	// END_OF_BUILDING_NPC

	return true;
}

void CManager::FinalizeBoot()
{
	auto it = m_map_pkObjByID.begin();

	while (it != m_map_pkObjByID.end())
	{
		CObject * pkObj = (it++)->second;

		pkObj->Show(pkObj->GetMapIndex(), pkObj->GetX(), pkObj->GetY());
		// BUILDING_NPC
		pkObj->RegenNPC();
		pkObj->ApplySpecialEffect();
		// END_OF_BUILDING_NPC
	}

	// BUILDING_NPC
	metin2::log::info( "FinalizeBoot");
	// END_OF_BUILDING_NPC

	auto it2 = m_map_pkLand.begin();

	while (it2 != m_map_pkLand.end())
	{
		CLand * pkLand = (it2++)->second.get();

		const TLand & r = pkLand->GetData();

		// LAND_MASTER_LOG	
		metin2::log::info( "LandMaster map_index={:d} pos=({:d}, {:d})", r.lMapIndex, r.x, r.y);
		// END_OF_LAND_MASTER_LOG

		if (r.dwGuildID != 0)
			continue;

		if (!map_allow_find(r.lMapIndex))
			continue;

		const TMapRegion * region = SECTREE_MANAGER::instance().GetMapRegion(r.lMapIndex);
		if (!region)
			continue;

		CHARACTER_MANAGER::instance().SpawnMob(20040, r.lMapIndex, region->sx + r.x + (r.width / 2), region->sy + r.y + (r.height / 2), 0);
	}
}

void CManager::DeleteObject(uint32_t dwID) // from DB
{
	metin2::log::info( "OBJ_DEL: {:d}", dwID);

	auto it = m_map_pkObjByID.find(dwID);

	if (it == m_map_pkObjByID.end())
		return;

	it->second->GetLand()->DeleteObject(dwID);
}

CObject * CManager::FindObjectByVID(uint32_t dwVID)
{
	auto it = m_map_pkObjByVID.find(dwVID);

	if (it == m_map_pkObjByVID.end())
		return nullptr;

	return it->second;
}

void CManager::UnregisterObject(CObject * pkObj)
{
	m_map_pkObjByID.erase(pkObj->GetID());
	m_map_pkObjByVID.erase(pkObj->GetVID());
}

void CManager::SendLandList(DESC * d, int32_t lMapIndex)
{
	TLandPacketElement e;

	TEMP_BUFFER buf;

	uint16_t wCount = 0;

	auto it = m_map_pkLand.begin();

	while (it != m_map_pkLand.end())
	{
		CLand * pkLand = (it++)->second.get();
		const TLand & r = pkLand->GetData();

		if (r.lMapIndex != lMapIndex)
			continue;

		//
		CHARACTER * ch  = d->GetCharacter();
		if (ch)
		{
			CGuild *guild = CGuildManager::instance().FindGuild(r.dwGuildID);
			ch->SendGuildName(guild);
		}
		//

		e.dwID = r.dwID;
		e.x = r.x;
		e.y = r.y;
		e.width = r.width;
		e.height = r.height;
		e.dwGuildID = r.dwGuildID;

		buf.write(&e, sizeof(TLandPacketElement));
		++wCount;
	}

	metin2::log::info( "SendLandList map {:d} count {:d} elem_size: {:d}", lMapIndex, wCount, buf.size());

	if (wCount != 0)
	{
		TPacketGCLandList p;

		p.header = HEADER_GC_LAND_LIST;
		p.size = sizeof(TPacketGCLandList) + buf.size();

		d->BufferedPacket(&p, sizeof(TPacketGCLandList));
		d->Packet(buf.read_peek(), buf.size());
	}
}

// LAND_CLEAR
void CManager::ClearLand(uint32_t dwLandID)
{
	CLand* pLand = FindLand(dwLandID);

	if ( pLand == nullptr )
	{
		metin2::log::info( "LAND_CLEAR: there is no LAND id like {:d}", dwLandID);
		return;
	}

	pLand->ClearLand();

	metin2::log::info( "LAND_CLEAR: request Land Clear. LandID: {:d}", pLand->GetID());
}

void CManager::ClearLandByGuildID(uint32_t dwGuildID)
{
	CLand* pLand = FindLandByGuild(dwGuildID);

	if ( pLand == nullptr )
	{
		metin2::log::info( "LAND_CLEAR: there is no GUILD id like {:d}", dwGuildID);
		return;
	}

	pLand->ClearLand();

	metin2::log::info( "LAND_CLEAR: request Land Clear. LandID: {:d}", pLand->GetID());
}

void CLand::ClearLand()
{
	auto iter = m_map_pkObject.begin();

	while ( iter != m_map_pkObject.end() )
	{
		RequestDeleteObject(iter->second->GetID());
		iter++;
	}

	SetOwner(0);

	const TLand & r = GetData();
	const TMapRegion * region = SECTREE_MANAGER::instance().GetMapRegion(r.lMapIndex);

	CHARACTER_MANAGER::instance().SpawnMob(20040, r.lMapIndex, region->sx + r.x + (r.width / 2), region->sy + r.y + (r.height / 2), 0);
}
// END_LAND_CLEAR

// BUILD_WALL
void CLand::DrawWall(uint32_t dwVnum, int32_t nMapIndex, int32_t& x, int32_t& y, char length, float zRot)
{
	int rot = (int)zRot;
	rot = ((rot%360) / 90) * 90;

	int dx=0, dy=0;

	switch ( rot )
	{
		case 0 :
			dx = -500;
			dy = 0;
			break;

		case 90 :
			dx = 0;
			dy = 500;
			break;

		case 180 :
			dx = 500;
			dy = 0;
			break;

		case 270 :
			dx = 0;
			dy = -500;
			break;
	}

	for ( int i=0; i < length; i++ )
	{
		this->RequestCreateObject(dwVnum, nMapIndex, x, y, 0, 0, rot, false);
		x += dx;
		y += dy;
	}
}


bool CLand::RequestCreateWall(int32_t nMapIndex, float rot)
{
	const bool 	WALL_ANOTHER_CHECKING_ENABLE = false;

	const TLand& land = GetData();

	int center_x = land.x + land.width  / 2;
	int center_y = land.y + land.height / 2;

	int wall_x = center_x;
	int wall_y = center_y;
	int wall_half_w = 1000;
	int wall_half_h = 1362;

	if (rot == 0.0f) 		// South gate
	{
		int door_x = wall_x;
		int door_y = wall_y + wall_half_h;
		RequestCreateObject(WALL_DOOR_VNUM,	nMapIndex, wall_x, wall_y + wall_half_h, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_BACK_VNUM,	nMapIndex, wall_x, wall_y - wall_half_h, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_LEFT_VNUM,	nMapIndex, wall_x - wall_half_w, wall_y, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_RIGHT_VNUM,	nMapIndex, wall_x + wall_half_w, wall_y, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
	}	
	else if (rot == 180.0f)		// North gate
	{
		int door_x = wall_x;
		int door_y = wall_y - wall_half_h;
		RequestCreateObject(WALL_DOOR_VNUM,	nMapIndex, wall_x, wall_y - wall_half_h, door_x, door_y, 180.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_BACK_VNUM,	nMapIndex, wall_x, wall_y + wall_half_h, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_LEFT_VNUM,	nMapIndex, wall_x - wall_half_w, wall_y, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_RIGHT_VNUM,	nMapIndex, wall_x + wall_half_w, wall_y, door_x, door_y,   0.0f, WALL_ANOTHER_CHECKING_ENABLE);
	}
	else if (rot == 90.0f)		// East gate 
	{
		int door_x = wall_x + wall_half_h;
		int door_y = wall_y;
		RequestCreateObject(WALL_DOOR_VNUM,	nMapIndex, wall_x + wall_half_h, wall_y, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_BACK_VNUM,	nMapIndex, wall_x - wall_half_h, wall_y, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_LEFT_VNUM,	nMapIndex, wall_x, wall_y - wall_half_w, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_RIGHT_VNUM,	nMapIndex, wall_x, wall_y + wall_half_w, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
	}
	else if (rot == 270.0f)		// West gate 
	{
		int door_x = wall_x - wall_half_h;
		int door_y = wall_y;
		RequestCreateObject(WALL_DOOR_VNUM,	nMapIndex, wall_x - wall_half_h, wall_y, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_BACK_VNUM,	nMapIndex, wall_x + wall_half_h, wall_y, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_LEFT_VNUM,	nMapIndex, wall_x, wall_y - wall_half_w, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(WALL_RIGHT_VNUM,	nMapIndex, wall_x, wall_y + wall_half_w, door_x, door_y,  90.0f, WALL_ANOTHER_CHECKING_ENABLE);
	}

	if (metin2::config::test_server())
	{
		RequestCreateObject(FLAG_VNUM, nMapIndex, land.x + 50, 			land.y + 50, 0, 0, 0.0, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(FLAG_VNUM, nMapIndex, land.x + land.width - 50,	land.y + 50, 0, 0, 90.0, WALL_ANOTHER_CHECKING_ENABLE);
		RequestCreateObject(FLAG_VNUM, nMapIndex, land.x + land.width - 50,	land.y + land.height - 50, 0, 0, 180.0, WALL_ANOTHER_CHECKING_ENABLE); 
		RequestCreateObject(FLAG_VNUM, nMapIndex, land.x + 50, 			land.y + land.height - 50, 0, 0, 270.0, WALL_ANOTHER_CHECKING_ENABLE);
	}
	return true;
}

void CLand::RequestDeleteWall()
{
	auto iter = m_map_pkObject.begin();

	while (iter != m_map_pkObject.end())
	{
		unsigned id   = iter->second->GetID();
		unsigned vnum = iter->second->GetVnum();

		switch (vnum)
		{
			case WALL_DOOR_VNUM:
			case WALL_BACK_VNUM:
			case WALL_LEFT_VNUM:
			case WALL_RIGHT_VNUM:
				RequestDeleteObject(id);
				break;
		}


		if (metin2::config::test_server())
		{
			if (FLAG_VNUM == vnum)
				RequestDeleteObject(id);

		}

		iter++;
	}
}

bool CLand::RequestCreateWallBlocks(uint32_t dwVnum, int32_t nMapIndex, char wallSize, bool doorEast, bool doorWest, bool doorSouth, bool doorNorth)
{
	const TLand & r = GetData();

	int32_t startX = r.x + (r.width  / 2) - (1300 + wallSize*500);
	int32_t startY = r.y + (r.height / 2) + (1300 + wallSize*500);

	uint32_t corner = dwVnum - 4;
	uint32_t wall   = dwVnum - 3;
	uint32_t door   = dwVnum - 1;

	bool checkAnother = false;
	int32_t* ptr = nullptr;
	int delta = 1;
	int rot = 270;

	bool doorOpen[4];
	doorOpen[0] = doorWest;
	doorOpen[1] = doorNorth;
	doorOpen[2] = doorEast;
	doorOpen[3] = doorSouth;

	if ( wallSize > 3 ) wallSize = 3;
	else if ( wallSize < 0 ) wallSize = 0;

	for ( int i=0; i < 4; i++, rot -= 90 )
	{
		switch ( i )
		{
			case 0 :
				delta = -1;
				ptr = &startY;
				break;
			case 1 :
				delta = 1;
				ptr = &startX;
				break;
			case 2 :
				ptr = &startY;
				delta = 1;
				break;
			case 3 :
				ptr = &startX;
				delta = -1;
				break;
		}

		this->RequestCreateObject(corner, nMapIndex, startX, startY, 0, 0, rot, checkAnother);

		*ptr = *ptr + ( 700 * delta );

		if ( doorOpen[i] )
		{
			this->DrawWall(wall, nMapIndex, startX, startY, wallSize, rot);

			*ptr = *ptr + ( 700 * delta );

			this->RequestCreateObject(door, nMapIndex, startX, startY, 0, 0, rot, checkAnother);

			*ptr = *ptr + ( 1300 * delta );

			this->DrawWall(wall, nMapIndex, startX, startY, wallSize, rot);
		}
		else
		{
			this->DrawWall(wall, nMapIndex, startX, startY, wallSize*2 + 4, rot);
		}

		*ptr = *ptr + ( 100 * delta );
	}

	return true;
}

void CLand::RequestDeleteWallBlocks(uint32_t dwID)
{
	auto iter = m_map_pkObject.begin();

	uint32_t corner = dwID - 4;
	uint32_t wall = dwID - 3;
	uint32_t door = dwID - 1;
	uint32_t dwVnum = 0;

	while ( iter != m_map_pkObject.end() )
	{
		dwVnum = iter->second->GetVnum();

		if ( dwVnum == corner || dwVnum == wall || dwVnum == door )
		{
			RequestDeleteObject(iter->second->GetID());
		}
		iter++;
	}
}
// END_BUILD_WALL

