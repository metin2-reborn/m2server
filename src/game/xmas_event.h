#pragma once

#include <cstdint>
#include <string>

namespace metin2::game {

namespace xmas
{
	enum
	{
		MOB_SANTA_VNUM = 20031,	//Santa 
	//	MOB_SANTA_VNUM = 20095,	//Nohae
		MOB_XMAS_TREE_VNUM = 20032,
		MOB_XMAS_FIRWORK_SELLER_VNUM = 9004,
	};

	void ProcessEventFlag(const std::string& name, int prev_value, int value);
	void SpawnSanta(int32_t lMapIndex, int iTimeGapSec);
	void SpawnEventHelper(bool spawn);
}
}
