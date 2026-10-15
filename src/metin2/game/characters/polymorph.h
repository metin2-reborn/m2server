
#pragma once

#include <metin2/game/characters/character.h>
#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>

#include <cstdint>
#include <unordered_map>

namespace metin2::game::inline characters {

inline constexpr int POLYMORPH_SKILL_ID = 129;
inline constexpr int POLYMORPH_BOOK_ID = 50322;
enum polymorph_bonus_type
{
	POLYMORPH_NO_BONUS,
	POLYMORPH_ATK_BONUS,
	POLYMORPH_DEF_BONUS,
	POLYMORPH_SPD_BONUS,
};

class polymorph_utils : public singleton<polymorph_utils>
{
	private :
		std::unordered_map<uint32_t, uint32_t> spd_type_;
		std::unordered_map<uint32_t, uint32_t> atk_type_;
		std::unordered_map<uint32_t, uint32_t> def_type_;

	public :
		polymorph_utils();

		polymorph_bonus_type bonus_type(uint32_t dwVnum);

		bool polymorph_character(character * pChar, item * pItem, const mob* pMob);
		bool update_book_practice_grade(character * pChar, item * pItem);
		bool give_book(character * pChar, uint32_t dwMobVnum, uint32_t dwPracticeCount, uint8_t BookLevel, uint8_t LevelLimit);
		bool book_upgrade(character * pChar, item * pItem);
};
}
