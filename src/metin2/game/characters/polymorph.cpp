#include <metin2/game/characters/polymorph.h>

#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/items/item.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/characters/mob_manager.h>

#include <metin2/proto/length.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <cstdint>
#include <unordered_map>
#include <utility>

namespace metin2::game::inline characters {

polymorph_utils::polymorph_utils()
{
	spd_type_.insert(std::make_pair(101, 101));
	spd_type_.insert(std::make_pair(1901, 1901));
}

polymorph_bonus_type polymorph_utils::bonus_type(uint32_t dwVnum)
{
	std::unordered_map<uint32_t, uint32_t>::iterator iter;

	iter = spd_type_.find(dwVnum);

	if (iter != spd_type_.end())
		return POLYMORPH_SPD_BONUS;

	iter = atk_type_.find(dwVnum);

	if (iter != atk_type_.end())
		return POLYMORPH_ATK_BONUS;

	iter = def_type_.find(dwVnum);

	if (iter != def_type_.end())
		return POLYMORPH_DEF_BONUS;

	return POLYMORPH_NO_BONUS;
}

bool polymorph_utils::polymorph_character(character * pChar, item * pItem, const mob* pMob)
{
	uint8_t bySkillLevel = pChar->skill_level(POLYMORPH_SKILL_ID);
	uint32_t dwDuration = 0;
	uint32_t dwBonusPercent = 0;
	int iPolyPercent = 0;

	switch (pChar->skill_master_type(POLYMORPH_SKILL_ID))
	{
		case SKILL_NORMAL:
			dwDuration = 10;
			break;

		case SKILL_MASTER:
			dwDuration = 15;
			break;

		case SKILL_GRAND_MASTER:
			dwDuration = 20;
			break;

		case SKILL_PERFECT_MASTER:
			dwDuration = 25;
			break;

		default:
			return false;
	}

	// Polymorph chance = character level - mob level + polymorph book level + 29 + polymorph skill level
	iPolyPercent = pChar->level() - pMob->table.level + pItem->get_socket(2) + (29 + bySkillLevel);

	if (iPolyPercent <= 0)
	{
		pChar->chat_packet(CHAT_TYPE_INFO, lc_text("둔갑에 실패 하였습니다"));
		return false;
	}
	else
	{
		if (number(1, 100) > iPolyPercent)
		{
			pChar->chat_packet(CHAT_TYPE_INFO, lc_text("둔갑에 실패 하였습니다"));
			return false;
		}
	}

	pChar->add_affect(AFFECT_POLYMORPH, POINT_POLYMORPH, pMob->table.vnum, AFF_POLYMORPH, dwDuration, 0, true);

	// Polymorph bonus = polymorph skill level + polymorph book level
	dwBonusPercent = bySkillLevel + pItem->get_socket(2);

	switch (bonus_type(pMob->table.vnum))
	{
		case POLYMORPH_ATK_BONUS:
			pChar->add_affect(AFFECT_POLYMORPH, POINT_ATT_BONUS, dwBonusPercent, AFF_POLYMORPH, dwDuration - 1, 0, false);
			break;

		case POLYMORPH_DEF_BONUS:
			pChar->add_affect(AFFECT_POLYMORPH, POINT_DEF_BONUS, dwBonusPercent, AFF_POLYMORPH, dwDuration - 1, 0, false);
			break;

		case POLYMORPH_SPD_BONUS:
			pChar->add_affect(AFFECT_POLYMORPH, POINT_MOV_SPEED, dwBonusPercent, AFF_POLYMORPH, dwDuration - 1, 0, false);
			break;

		default:
		case POLYMORPH_NO_BONUS:
			break;
	}

	return true;
}

bool polymorph_utils::update_book_practice_grade(character * pChar, item * pItem)
{
	if (pChar == nullptr || pItem == nullptr)
		return false;

	if (pItem->get_socket(1) > 0)
		pItem->set_socket(1, pItem->get_socket(1) - 1);
	else
		pChar->chat_packet(CHAT_TYPE_INFO, lc_text("둔갑서 수련을 마첬습니다. 신선에게 찾아가세요."));

	return true;
}

bool polymorph_utils::give_book(character * pChar, uint32_t dwMobVnum, uint32_t dwPracticeCount, uint8_t BookLevel, uint8_t LevelLimit)
{
	// Socket0                Socket1     Socket2
	// Polymorph mob number   Training    Book level
	if (pChar == nullptr)
		return false;

	item * pItem = pChar->auto_give_item(POLYMORPH_BOOK_ID, 1);

	if (pItem == nullptr)
		return false;

	if (mob_manager::instance().get(dwMobVnum) == nullptr)
	{
		metin2::log::error("Wrong Polymorph vnum passed: CPolymorphUtils::GiveBook(PID({:d}), {:d} {:d} {:d} {:d})", 
				pChar->player_id(), dwMobVnum, dwPracticeCount, BookLevel, LevelLimit);
		return false;
	}

	pItem->set_socket(0, dwMobVnum);			// Monster number to polymorph into
	pItem->set_socket(1, dwPracticeCount);		// Number of trainings required
	pItem->set_socket(2, BookLevel);			// Training level
	return true;
}

bool polymorph_utils::book_upgrade(character * pChar, item * pItem)
{
	if (pChar == nullptr || pItem == nullptr)
		return false;

	pItem->set_socket(1, pItem->get_socket(2) * 50);
	pItem->set_socket(2, pItem->get_socket(2)+1);
	return true;
}
}
