
#include <metin2/game/characters/character.h>
#include <metin2/game/world/entity.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/app/typedef.h>

namespace metin2::game {

namespace quest
{
	struct f_warp_to_home
	{
		void operator() ( entity * ent )
		{
			if ( ent->is_type(ENTITY_CHARACTER) )
			{
				character * ch = (character *) ent;

				if ( ch->is_pc() == true && ch->is_gm() != true )
				{
					if ( ((ch->x() >= 764503 && ch->x() <= 772362) && (ch->y() >= 22807 && ch->y() <= 26499)) == false )
					{
						ch->go_home();
					}
				}
			}
		};
	};

	auto dance_event_go_home( lua_State* lua ) -> int
	{
		sectree_map * pSecMap = sectree_manager::instance().map( 115 );

		if ( pSecMap != nullptr )
		{
			f_warp_to_home f;
			pSecMap->for_each( f );
		}

		return 0;
	}

	void register_dance_event_function_table()
	{
		luaL_reg dance_event_functions[] =
		{
			{ "gohome",		dance_event_go_home	},

			{	nullptr,	nullptr}
		};
		
		quest_manager::instance().add_lua_function_table("dance_event", dance_event_functions);
	}
}
}
