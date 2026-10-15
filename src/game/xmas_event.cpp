#include "xmas_event.h"

#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "desc.h"
#include "desc_manager.h"
#include "event.h"
#include "questmanager.h"
#include "sectree_manager.h"
#include "typedef.h"

#include <common/length.h>
#include <metin2/log/log.h>

#include <cstdint>
#include <string>

namespace metin2::game {

namespace xmas
{
	void ProcessEventFlag(const std::string& name, int prev_value, int value)
	{
		if (name == "xmas_snow" || name == "xmas_boom" || name == "xmas_song" || name == "xmas_tree")
		{
			// Spread them
			const DESC_MANAGER::DESC_SET & c_ref_set = DESC_MANAGER::instance().GetClientSet();

			for (auto it = c_ref_set.begin(); it != c_ref_set.end(); ++it)
			{
				CHARACTER * ch = (*it)->GetCharacter();

				if (!ch)
					continue;

				ch->ChatPacket(CHAT_TYPE_COMMAND, "%s %d", name.c_str(), value);
			}

			if (name == "xmas_boom")
			{
				if (value && !prev_value)
				{
					SpawnEventHelper(true);
				}
				else if (!value && prev_value)
				{
					SpawnEventHelper(false);
				}
			}
			else if (name == "xmas_tree")
			{
				if (value > 0 && prev_value == 0)
				{
					CharacterVectorInteractor i;

					// Create if it doesn't exist
					if (!CHARACTER_MANAGER::instance().GetCharactersByRaceNum(MOB_XMAS_TREE_VNUM, i))
						CHARACTER_MANAGER::instance().SpawnMob(MOB_XMAS_TREE_VNUM, 61, 76500 + 358400, 60900 + 153600, 0, false, -1);
				}
				else if (prev_value > 0 && value == 0)
				{
					// Remove if it exists
					CharacterVectorInteractor i;

					if (CHARACTER_MANAGER::instance().GetCharactersByRaceNum(MOB_XMAS_TREE_VNUM, i))
					{
						CharacterVectorInteractor::iterator it = i.begin();

						while (it != i.end())
							CHARACTER_MANAGER::instance().DestroyCharacter(*it++);
					}
				}
			}
		}
		else if (name == "xmas_santa")
		{
			switch (value)
			{
				case 0:
					// Code that removes it if it exists
					{
						CharacterVectorInteractor i;

						if (CHARACTER_MANAGER::instance().GetCharactersByRaceNum(MOB_SANTA_VNUM, i))
						{
							CharacterVectorInteractor::iterator it = i.begin();

							while (it != i.end())
								CHARACTER_MANAGER::instance().DestroyCharacter(*it++);
						}
					}

					break;

				case 1:
					// If I am Mt. Seohan, create Santa if missing and set the state to 2.
					if (map_allow_find(61))
					{
						quest::CQuestManager::instance().RequestSetEventFlag("xmas_santa", 2);

						CharacterVectorInteractor i;

						if (CHARACTER_MANAGER::instance().GetCharactersByRaceNum(MOB_SANTA_VNUM, i))
							CHARACTER_MANAGER::instance().SpawnMobRandomPosition(MOB_SANTA_VNUM, 61);
					}
					break;

				case 2:
					break;
			}
		}
	}

	EVENTINFO(spawn_santa_info)
	{
		int32_t lMapIndex;

		spawn_santa_info() 
		: lMapIndex( 0 )
		{
		}
	};

	EVENTFUNC(spawn_santa_event)
	{
		spawn_santa_info* info = dynamic_cast<spawn_santa_info*>( event->info );

		if ( info == nullptr )
		{
			metin2::log::error( "spawn_santa_event> <Factor> Null pointer" );
			return 0;
		}

		int32_t lMapIndex = info->lMapIndex;

		if (quest::CQuestManager::instance().GetEventFlag("xmas_santa") == 0)
			return 0;

		CharacterVectorInteractor i;

		if (CHARACTER_MANAGER::instance().GetCharactersByRaceNum(MOB_SANTA_VNUM, i))
			return 0;

		if (CHARACTER_MANAGER::instance().SpawnMobRandomPosition(xmas::MOB_SANTA_VNUM, lMapIndex))
		{
			metin2::log::info( "santa comes to town!");
			return 0;
		}

		return PASSES_PER_SEC(5);
	}

	void SpawnSanta(int32_t lMapIndex, int iTimeGapSec)
	{
		if (metin2::config::test_server())
		{
			iTimeGapSec /= 60;
		}

		metin2::log::info( "santa respawn time = {:d}", iTimeGapSec);
		spawn_santa_info* info = AllocEventInfo<spawn_santa_info>();

		info->lMapIndex = lMapIndex;

		event_create(spawn_santa_event, info, PASSES_PER_SEC(iTimeGapSec));
	}

	void SpawnEventHelper(bool spawn)
	{
		if ( spawn == true )
		{
			// Create if it doesn't exist
			struct SNPCSellFireworkPosition
			{
				int32_t lMapIndex;
				int x;
				int y;
			} positions[] = {
				{  1,	615,	618 },
				{  3,	500,	625 },
				{ 21,	598,	665 },
				{ 23,	476,	360 },
				{ 41,	318,	629 },
				{ 43,	478,	375 },
				{ 0,	0,	0   },
			};

			SNPCSellFireworkPosition* p = positions;
			while (p->lMapIndex)
			{
				if (map_allow_find(p->lMapIndex))
				{
					PIXEL_POSITION posBase;
					if (!SECTREE_MANAGER::instance().GetMapBasePositionByMapIndex(p->lMapIndex, posBase))
					{
						metin2::log::error("cannot get map base position {:d}", p->lMapIndex);
						p++;
						continue;
					}

					CHARACTER_MANAGER::instance().SpawnMob(
							MOB_XMAS_FIRWORK_SELLER_VNUM, p->lMapIndex, posBase.x + p->x * 100, posBase.y + p->y * 100, 0, false, -1);
				}
				p++;
			}
		}
		else
		{
			CharacterVectorInteractor i;

			// Remove if it exists
			if (CHARACTER_MANAGER::instance().GetCharactersByRaceNum(MOB_XMAS_FIRWORK_SELLER_VNUM, i))
			{
				CharacterVectorInteractor::iterator it = i.begin();

				while (it != i.end())
					CHARACTER_MANAGER::instance().DestroyCharacter(*it++);
			}
		}
	}
}
}
