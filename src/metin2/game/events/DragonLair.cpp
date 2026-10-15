
#include <metin2/game/events/DragonLair.h>

#include <metin2/game/characters/char.h>
#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/config.h>
#include <metin2/game/world/entity.h>
#include <metin2/game/app/event.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/world/regen.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>

#include <metin2/log/log.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <utility>

namespace metin2::game::inline events {


struct FWarpToDragronLairWithGuildMembers
{
	uint32_t dwGuildID;
	int32_t mapIndex;
	int32_t x, y;

	FWarpToDragronLairWithGuildMembers( uint32_t guildID, int32_t map, int32_t X, int32_t Y )
		: dwGuildID(guildID), mapIndex(map), x(X), y(Y)
	{
	}

	void operator()(CEntity * ent)
	{
		if (nullptr != ent && true == ent->IsType(ENTITY_CHARACTER))
		{
			CHARACTER * pChar = static_cast<CHARACTER *>(ent);

			if (true == pChar->IsPC())
			{
				if (nullptr != pChar->GetGuild())
				{
					if (dwGuildID == pChar->GetGuild()->GetID())
					{
						pChar->WarpSet(x, y, mapIndex);
					}
				}
			}
		}
	}
};

struct FWarpToVillage
{
	void operator() (CEntity * ent)
	{
		if (nullptr != ent)
		{
			CHARACTER * pChar = static_cast<CHARACTER *>(ent);

			if (nullptr != pChar)
			{
				if (true == pChar->IsPC())
				{
					pChar->GoHome();
				}
			}
		}
	}
};

EVENTINFO(tag_DragonLair_Collapse_EventInfo)
{
	int step;
	CDragonLair* pLair;
	int32_t InstanceMapIndex;

	tag_DragonLair_Collapse_EventInfo()
	: step( 0 )
	, pLair( nullptr )
	, InstanceMapIndex( 0 )
	{
	}
};

EVENTFUNC( DragonLair_Collapse_Event )
{
	tag_DragonLair_Collapse_EventInfo* pInfo = dynamic_cast<tag_DragonLair_Collapse_EventInfo*>(event->info);

	if ( pInfo == nullptr )
	{
		metin2::log::error( "DragonLair_Collapse_Event> <Factor> Null pointer" );
		return 0;
	}

	if (0 == pInfo->step)
	{
		char buf[512];
		snprintf(buf, 512, LC_TEXT("용가리가 %d 초만에 죽어써효ㅠㅠ"), pInfo->pLair->GetEstimatedTime());
		SendNoticeMap(buf, pInfo->InstanceMapIndex, true);

		pInfo->step++;

		return PASSES_PER_SEC( 30 );
	}
	else if (1 == pInfo->step)
	{
		SECTREE_MAP * pMap = SECTREE_MANAGER::instance().GetMap( pInfo->InstanceMapIndex );

		if (nullptr != pMap)
		{
			FWarpToVillage f;
			pMap->for_each( f );
		}

		pInfo->step++;

		return PASSES_PER_SEC( 30 );
	}
	else
	{
		SECTREE_MANAGER::instance().DestroyPrivateMap( pInfo->InstanceMapIndex );
		delete pInfo->pLair;
	}

	return 0;
}









CDragonLair::CDragonLair(uint32_t guildID)
	: GuildID_(guildID)
{
	StartTime_ = get_global_time();
}

CDragonLair::~CDragonLair()
{
}

uint32_t CDragonLair::GetEstimatedTime() const
{
	return get_global_time() - StartTime_;
}

void CDragonLair::OnDragonDead(CHARACTER * pDragon)
{
	metin2::log::info( "DragonLair: 도라곤이 죽어써효");

	LogManager::instance().DragonSlayLog(  GuildID_, pDragon->GetMobTable().dwVnum, StartTime_, get_global_time() );
}












CDragonLairManager::CDragonLairManager()
{
}

CDragonLairManager::~CDragonLairManager()
{
}

bool CDragonLairManager::Start(int32_t MapIndexFrom, int32_t BaseMapIndex, uint32_t GuildID)
{
	int32_t instanceMapIndex = SECTREE_MANAGER::instance().CreatePrivateMap(BaseMapIndex);
	if (instanceMapIndex == 0) {
		metin2::log::error("CDragonLairManager::Start() : no private map index available");
		return false;
	}

	SECTREE_MAP * pMap = SECTREE_MANAGER::instance().GetMap(MapIndexFrom);

	if (nullptr != pMap)
	{
		SECTREE_MAP * pTargetMap = SECTREE_MANAGER::instance().GetMap(BaseMapIndex);

		if (nullptr == pTargetMap)
		{
			return false;
		}

		const TMapRegion* pRegionInfo = SECTREE_MANAGER::instance().GetMapRegion( pTargetMap->m_setting.iIndex );

		if (nullptr != pRegionInfo)
		{
			FWarpToDragronLairWithGuildMembers f(GuildID, instanceMapIndex, 844000, 1066900);

			pMap->for_each( f );

			LairMap_.insert( std::make_pair(GuildID, new CDragonLair(GuildID)) );

			std::string strMapBasePath( LocaleService_GetMapPath() );

			strMapBasePath += "/" + pRegionInfo->strMapName + "/instance_regen.txt";

			metin2::log::info( "{}", strMapBasePath.c_str());

			regen_do(strMapBasePath.c_str(), instanceMapIndex, pTargetMap->m_setting.iBaseX, pTargetMap->m_setting.iBaseY, nullptr, true);

			return true;
		}
	}

	return false;
}

void CDragonLairManager::OnDragonDead(CHARACTER * pDragon, uint32_t KillerGuildID)
{
	if (nullptr == pDragon)
		return;

	if (false == pDragon->IsMonster())
		return;

	std::unordered_map<uint32_t, CDragonLair*>::iterator iter = LairMap_.find( KillerGuildID );

	if (LairMap_.end() == iter)
	{
		return;
	}

	SECTREE_MAP * pMap = SECTREE_MANAGER::instance().GetMap( pDragon->GetMapIndex() );

	if (nullptr == iter->second || nullptr == pMap)
	{
		LairMap_.erase( iter );
		return;
	}

	iter->second->OnDragonDead( pDragon );

	// Send everyone home and destroy the map

	tag_DragonLair_Collapse_EventInfo* info;
	info = AllocEventInfo<tag_DragonLair_Collapse_EventInfo>();

	info->step = 0;
	info->pLair = iter->second;
	info->InstanceMapIndex = pDragon->GetMapIndex();

	event_create(DragonLair_Collapse_Event, info, PASSES_PER_SEC(10));

	LairMap_.erase( iter );
}
}
