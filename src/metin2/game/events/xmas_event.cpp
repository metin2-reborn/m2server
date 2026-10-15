#include <metin2/game/events/xmas_event.h>

#include <metin2/game/characters/char.h>
#include <metin2/game/characters/char_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/app/event.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/log/log.h>

#include <cstdint>
#include <string>

namespace metin2::game::inline events {

namespace xmas
{
	void process_event_flag(const std::string& name, int prev_value, int value)
	{
		if (name == "xmas_snow" || name == "xmas_boom" || name == "xmas_song" || name == "xmas_tree")
		{
			// Spread them
			const desc_manager::desc_set & c_ref_set = desc_manager::instance().client_set();

			for (auto it = c_ref_set.begin(); it != c_ref_set.end(); ++it)
			{
				character * ch = (*it)->get_character();

				if (!ch)
					continue;

				ch->chat_packet(CHAT_TYPE_COMMAND, "%s %d", name.c_str(), value);
			}

			if (name == "xmas_boom")
			{
				if (value && !prev_value)
				{
					spawn_event_helper(true);
				}
				else if (!value && prev_value)
				{
					spawn_event_helper(false);
				}
			}
			else if (name == "xmas_tree")
			{
				if (value > 0 && prev_value == 0)
				{
					character_vector_interactor i;

					// Create if it doesn't exist
					if (!character_manager::instance().characters_by_race_num(MOB_XMAS_TREE_VNUM, i))
						character_manager::instance().spawn_mob(MOB_XMAS_TREE_VNUM, 61, 76500 + 358400, 60900 + 153600, 0, false, -1);
				}
				else if (prev_value > 0 && value == 0)
				{
					// Remove if it exists
					character_vector_interactor i;

					if (character_manager::instance().characters_by_race_num(MOB_XMAS_TREE_VNUM, i))
					{
						character_vector_interactor::iterator it = i.begin();

						while (it != i.end())
							character_manager::instance().destroy_character(*it++);
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
						character_vector_interactor i;

						if (character_manager::instance().characters_by_race_num(MOB_SANTA_VNUM, i))
						{
							character_vector_interactor::iterator it = i.begin();

							while (it != i.end())
								character_manager::instance().destroy_character(*it++);
						}
					}

					break;

				case 1:
					// If I am Mt. Seohan, create Santa if missing and set the state to 2.
					if (map_allow_find(61))
					{
						quest::quest_manager::instance().request_set_event_flag("xmas_santa", 2);

						character_vector_interactor i;

						if (character_manager::instance().characters_by_race_num(MOB_SANTA_VNUM, i))
							character_manager::instance().spawn_mob_random_position(MOB_SANTA_VNUM, 61);
					}
					break;

				case 2:
					break;
			}
		}
	}

	EVENTINFO(spawn_santa_info)
	{
		int32_t map_index;

		spawn_santa_info() 
		: map_index( 0 )
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

		int32_t lMapIndex = info->map_index;

		if (quest::quest_manager::instance().event_flag("xmas_santa") == 0)
			return 0;

		character_vector_interactor i;

		if (character_manager::instance().characters_by_race_num(MOB_SANTA_VNUM, i))
			return 0;

		if (character_manager::instance().spawn_mob_random_position(xmas::MOB_SANTA_VNUM, lMapIndex))
		{
			metin2::log::info( "santa comes to town!");
			return 0;
		}

		return passes_per_sec(5);
	}

	void spawn_santa(int32_t lMapIndex, int iTimeGapSec)
	{
		if (metin2::config::test_server())
		{
			iTimeGapSec /= 60;
		}

		metin2::log::info( "santa respawn time = {:d}", iTimeGapSec);
		spawn_santa_info* info = alloc_event_info<spawn_santa_info>();

		info->map_index = lMapIndex;

		event_create(spawn_santa_event, info, passes_per_sec(iTimeGapSec));
	}

	void spawn_event_helper(bool spawn)
	{
		if ( spawn == true )
		{
			// Create if it doesn't exist
			struct npc_sell_firework_position
			{
				int32_t map_index;
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

			npc_sell_firework_position* p = positions;
			while (p->map_index)
			{
				if (map_allow_find(p->map_index))
				{
					pixel_position posBase;
					if (!sectree_manager::instance().map_base_position_by_map_index(p->map_index, posBase))
					{
						metin2::log::error("cannot get map base position {:d}", p->map_index);
						p++;
						continue;
					}

					character_manager::instance().spawn_mob(
							MOB_XMAS_FIRWORK_SELLER_VNUM, p->map_index, posBase.x + p->x * 100, posBase.y + p->y * 100, 0, false, -1);
				}
				p++;
			}
		}
		else
		{
			character_vector_interactor i;

			// Remove if it exists
			if (character_manager::instance().characters_by_race_num(MOB_XMAS_FIRWORK_SELLER_VNUM, i))
			{
				character_vector_interactor::iterator it = i.begin();

				while (it != i.end())
					character_manager::instance().destroy_character(*it++);
			}
		}
	}
}
}
