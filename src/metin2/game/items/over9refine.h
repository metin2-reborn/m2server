
#pragma once

#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>

#include <cstdint>
#include <unordered_map>

namespace metin2::game::inline items {

class over9_refine_manager : public singleton<over9_refine_manager>
{
	private :
		typedef std::unordered_map<uint32_t, uint32_t> ove_r9_item_map;
		ove_r9_item_map item_;

	public :
		void enable_over9_refine(uint32_t dwVnumFrom, uint32_t dwVnumTo);

		int can_over9_refine(uint32_t dwVnum);

		bool change9_to_over9(character * pChar, item * item);
		bool over9_refine(character * pChat, item * item);

		uint32_t material_vnum(uint32_t baseVnum);
};
}
