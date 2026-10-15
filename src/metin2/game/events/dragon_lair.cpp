
#include <metin2/game/events/dragon_lair.h>

#include <metin2/game/characters/character.h>
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


struct f_warp_to_dragron_lair_with_guild_members
{
	uint32_t guild_id;
	int32_t index;
	int32_t x, y;

	f_warp_to_dragron_lair_with_guild_members( uint32_t guildID, int32_t map, int32_t X, int32_t Y )
		: guild_id(guildID), index(map), x(X), y(Y)
	{
	}

	void operator()(entity * ent)
	{
		if (nullptr != ent && true == ent->is_type(ENTITY_CHARACTER))
		{
			character * pChar = static_cast<character *>(ent);

			if (true == pChar->is_pc())
			{
				if (nullptr != pChar->get_guild())
				{
					if (guild_id == pChar->get_guild()->id())
					{
						pChar->warp_set(x, y, index);
					}
				}
			}
		}
	}
};

struct f_warp_to_village
{
	void operator() (entity * ent)
	{
		if (nullptr != ent)
		{
			character * pChar = static_cast<character *>(ent);

			if (nullptr != pChar)
			{
				if (true == pChar->is_pc())
				{
					pChar->go_home();
				}
			}
		}
	}
};

EVENTINFO(tag_dragon_lair_collapse_event_info)
{
	int step;
	dragon_lair* lair;
	int32_t instance_map_index;

	tag_dragon_lair_collapse_event_info()
	: step( 0 )
	, lair( nullptr )
	, instance_map_index( 0 )
	{
	}
};

EVENTFUNC( dragon_lair_collapse_event )
{
	tag_dragon_lair_collapse_event_info* pInfo = dynamic_cast<tag_dragon_lair_collapse_event_info*>(event->info);

	if ( pInfo == nullptr )
	{
		metin2::log::error( "DragonLair_Collapse_Event> <Factor> Null pointer" );
		return 0;
	}

	if (0 == pInfo->step)
	{
		char buf[512];
		snprintf(buf, 512, lc_text("용가리가 %d 초만에 죽어써효ㅠㅠ"), pInfo->lair->estimated_time());
		send_notice_map(buf, pInfo->instance_map_index, true);

		pInfo->step++;

		return passes_per_sec( 30 );
	}
	else if (1 == pInfo->step)
	{
		sectree_map * pMap = sectree_manager::instance().map( pInfo->instance_map_index );

		if (nullptr != pMap)
		{
			f_warp_to_village f;
			pMap->for_each( f );
		}

		pInfo->step++;

		return passes_per_sec( 30 );
	}
	else
	{
		sectree_manager::instance().destroy_private_map( pInfo->instance_map_index );
		delete pInfo->lair;
	}

	return 0;
}









dragon_lair::dragon_lair(uint32_t guildID)
	: guild_id_(guildID)
{
	start_time_ = get_global_time();
}

dragon_lair::~dragon_lair()
{
}

uint32_t dragon_lair::estimated_time() const
{
	return get_global_time() - start_time_;
}

void dragon_lair::on_dragon_dead(character * pDragon)
{
	metin2::log::info( "DragonLair: 도라곤이 죽어써효");

	log_manager::instance().dragon_slay_log(  guild_id_, pDragon->get_mob_table().vnum, start_time_, get_global_time() );
}












dragon_lair_manager::dragon_lair_manager()
{
}

dragon_lair_manager::~dragon_lair_manager()
{
}

bool dragon_lair_manager::start(int32_t MapIndexFrom, int32_t BaseMapIndex, uint32_t GuildID)
{
	int32_t instanceMapIndex = sectree_manager::instance().create_private_map(BaseMapIndex);
	if (instanceMapIndex == 0) {
		metin2::log::error("CDragonLairManager::Start() : no private map index available");
		return false;
	}

	sectree_map * pMap = sectree_manager::instance().map(MapIndexFrom);

	if (nullptr != pMap)
	{
		sectree_map * pTargetMap = sectree_manager::instance().map(BaseMapIndex);

		if (nullptr == pTargetMap)
		{
			return false;
		}

		const map_region* pRegionInfo = sectree_manager::instance().get_map_region( pTargetMap->setting.index );

		if (nullptr != pRegionInfo)
		{
			f_warp_to_dragron_lair_with_guild_members f(GuildID, instanceMapIndex, 844000, 1066900);

			pMap->for_each( f );

			lair_map_.insert( std::make_pair(GuildID, new dragon_lair(GuildID)) );

			std::string strMapBasePath( locale_service_get_map_path() );

			strMapBasePath += "/" + pRegionInfo->map_name + "/instance_regen.txt";

			metin2::log::info( "{}", strMapBasePath.c_str());

			regen_do(strMapBasePath.c_str(), instanceMapIndex, pTargetMap->setting.base_x, pTargetMap->setting.base_y, nullptr, true);

			return true;
		}
	}

	return false;
}

void dragon_lair_manager::on_dragon_dead(character * pDragon, uint32_t KillerGuildID)
{
	if (nullptr == pDragon)
		return;

	if (false == pDragon->is_monster())
		return;

	std::unordered_map<uint32_t, dragon_lair*>::iterator iter = lair_map_.find( KillerGuildID );

	if (lair_map_.end() == iter)
	{
		return;
	}

	sectree_map * pMap = sectree_manager::instance().map( pDragon->map_index() );

	if (nullptr == iter->second || nullptr == pMap)
	{
		lair_map_.erase( iter );
		return;
	}

	iter->second->on_dragon_dead( pDragon );

	// Send everyone home and destroy the map

	tag_dragon_lair_collapse_event_info* info;
	info = alloc_event_info<tag_dragon_lair_collapse_event_info>();

	info->step = 0;
	info->lair = iter->second;
	info->instance_map_index = pDragon->map_index();

	event_create(dragon_lair_collapse_event, info, passes_per_sec(10));

	lair_map_.erase( iter );
}
}
