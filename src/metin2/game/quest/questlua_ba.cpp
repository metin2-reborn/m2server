
#include <metin2/game/events/BattleArena.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questmanager.h>

namespace metin2::game {


namespace quest
{
	int ba_start(lua_State* L)
	{
		if ( lua_isnumber(L, 1) )
		{
			CBattleArena::instance().Start((int)lua_tonumber(L, 1));
		}

		return 0;
	}

	void RegisterBattleArenaFunctionTable()
	{
		luaL_reg ba_functions[] =
		{
			{	"start",	ba_start	},

			{	nullptr,		nullptr		}
		};

		CQuestManager::instance(). AddLuaFunctionTable("ba", ba_functions);
	}
}
}
