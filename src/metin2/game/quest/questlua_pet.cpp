
#include <metin2/game/characters/char.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/characters/PetSystem.h>
#include <metin2/game/quest/questlua.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/app/typedef.h>

#include <cstdint>

namespace metin2::game {



namespace quest
{

	// syntax in LUA: pet.summon(mob_vnum, pet's name, (bool)run to me from far away)
	int pet_summon(lua_State* L)
	{
		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();
		CPetSystem* petSystem = ch->GetPetSystem();
		CItem * pItem = CQuestManager::instance().GetCurrentItem();
		if (!ch || !petSystem || !pItem)
		{
			lua_pushnumber (L, 0);
			return 1;
		}

		if (nullptr == petSystem)
		{
			lua_pushnumber (L, 0);
			return 1;
		}

		// vnum of the summoned creature
		uint32_t mobVnum= lua_isnumber(L, 1) ? lua_tonumber(L, 1) : 0;

		// name of the summoned creature
		const char* petName = lua_isstring(L, 2) ? lua_tostring(L, 2) : nullptr;

		// Whether it runs in from afar when summoned
		bool bFromFar = lua_isboolean(L, 3) ? lua_toboolean(L, 3) : false;

		CPetActor* pet = petSystem->Summon(mobVnum, pItem, petName, bFromFar);

		if (pet != nullptr)
			lua_pushnumber (L, pet->GetVID());
		else
			lua_pushnumber (L, 0);

		return 1;
	}

	// syntax: pet.unsummon(mob_vnum)
	int pet_unsummon(lua_State* L)
	{
		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();
		CPetSystem* petSystem = ch->GetPetSystem();

		if (nullptr == petSystem)
			return 0;

		// vnum of the summoned creature
		uint32_t mobVnum= lua_isnumber(L, 1) ? lua_tonumber(L, 1) : 0;

		petSystem->Unsummon(mobVnum);
		return 1;
	}

	// syntax: pet.unsummon(mob_vnum)
	int pet_count_summoned(lua_State* L)
	{
		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();
		CPetSystem* petSystem = ch->GetPetSystem();

		lua_Number count = 0;

		if (nullptr != petSystem)
			count = (lua_Number)petSystem->CountSummoned();

		lua_pushnumber(L, count);

		return 1;
	}

	// syntax: pet.is_summon(mob_vnum)
	int pet_is_summon(lua_State* L)
	{
		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();
		CPetSystem* petSystem = ch->GetPetSystem();

		if (nullptr == petSystem)
			return 0;

		// vnum of the summoned creature
		uint32_t mobVnum= lua_isnumber(L, 1) ? lua_tonumber(L, 1) : 0;

		CPetActor* petActor = petSystem->GetByVnum(mobVnum);

		if (nullptr == petActor)
			lua_pushboolean(L, false);
		else
			lua_pushboolean(L, petActor->IsSummoned());

		return 1;
	}

	int pet_spawn_effect(lua_State* L)
	{
		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();
		CPetSystem* petSystem = ch->GetPetSystem();

		if (nullptr == petSystem)
			return 0;

		uint32_t mobVnum = lua_isnumber(L, 1) ? lua_tonumber(L, 1) : 0;

		CPetActor* petActor = petSystem->GetByVnum(mobVnum);
		if (nullptr == petActor)
			return 0;
		CHARACTER * pet_ch = petActor->GetCharacter();
		if (nullptr == pet_ch)
			return 0;

		if (lua_isstring(L, 2))
		{
			pet_ch->SpecificEffectPacket (lua_tostring(L, 2));
		}
		return 0;
	}

	void RegisterPetFunctionTable()
	{
		luaL_reg pet_functions[] =
		{
			{ "summon",			pet_summon			},
			{ "unsummon",		pet_unsummon		},
			{ "is_summon",		pet_is_summon		},
			{ "count_summoned",	pet_count_summoned	},
			{ "spawn_effect",	pet_spawn_effect	},
			{ nullptr,				nullptr				}
		};

		CQuestManager::instance().AddLuaFunctionTable("pet", pet_functions);
	}

}
}
