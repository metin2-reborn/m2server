
#pragma once

#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>

#include <cstdint>
#include <unordered_map>

namespace metin2::game::inline items {

class COver9RefineManager : public singleton<COver9RefineManager>
{
	private :
		typedef std::unordered_map<uint32_t, uint32_t> OVER9ITEM_MAP;
		OVER9ITEM_MAP m_mapItem;

	public :
		void enableOver9Refine(uint32_t dwVnumFrom, uint32_t dwVnumTo);

		int canOver9Refine(uint32_t dwVnum);

		bool Change9ToOver9(CHARACTER * pChar, CItem * item);
		bool Over9Refine(CHARACTER * pChat, CItem * item);

		uint32_t GetMaterialVnum(uint32_t baseVnum);
};
}
