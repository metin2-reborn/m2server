

#include "char.h"
#include "DragonLair.h"
#include "guild.h"
#include "lua_incl.h"
#include "questmanager.h"

#include <cstdint>

namespace quest
{
	int dl_startRaid(lua_State* L)
	{
		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();
		int32_t baseMapIndex = lua_tonumber(L, -1);

		CDragonLairManager::instance().Start(ch->GetMapIndex(), baseMapIndex, ch->GetGuild()->GetID());

		return 0;
	}

	void RegisterDragonLairFunctionTable()
	{
		luaL_reg dl_functions[] =
		{
			{	"startRaid",	dl_startRaid	},

			{	nullptr,			nullptr			}
		};

		CQuestManager::instance(). AddLuaFunctionTable("DragonLair", dl_functions);
	}
}

