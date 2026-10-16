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

auto polymorph_utils::bonus_type(uint32_t vnum) -> polymorph_bonus_type
{
	std::unordered_map<uint32_t, uint32_t>::iterator iter;

	iter = spd_type_.find(vnum);

	if (iter != spd_type_.end())
		return POLYMORPH_SPD_BONUS;

	iter = atk_type_.find(vnum);

	if (iter != atk_type_.end())
		return POLYMORPH_ATK_BONUS;

	iter = def_type_.find(vnum);

	if (iter != def_type_.end())
		return POLYMORPH_DEF_BONUS;

	return POLYMORPH_NO_BONUS;
}

auto polymorph_utils::polymorph_character(character * ch, item * item_value, const mob* mob_value) -> bool
{
	uint8_t bySkillLevel = ch->skill_level(POLYMORPH_SKILL_ID);
	uint32_t dwDuration = 0;
	uint32_t dwBonusPercent = 0;
	int iPolyPercent = 0;

	switch (ch->skill_master_type(POLYMORPH_SKILL_ID))
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

	iPolyPercent = ch->level() - mob_value->table.level + item_value->get_socket(2) + (29 + bySkillLevel);

	if (iPolyPercent <= 0)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("둔갑에 실패 하였습니다"));
		return false;
	}
	else
	{
		if (number(1, 100) > iPolyPercent)
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("둔갑에 실패 하였습니다"));
			return false;
		}
	}

	ch->add_affect(AFFECT_POLYMORPH, POINT_POLYMORPH, mob_value->table.vnum, AFF_POLYMORPH, dwDuration, 0, true);

	dwBonusPercent = bySkillLevel + item_value->get_socket(2);

	switch (bonus_type(mob_value->table.vnum))
	{
		case POLYMORPH_ATK_BONUS:
			ch->add_affect(AFFECT_POLYMORPH, POINT_ATT_BONUS, dwBonusPercent, AFF_POLYMORPH, dwDuration - 1, 0, false);
			break;

		case POLYMORPH_DEF_BONUS:
			ch->add_affect(AFFECT_POLYMORPH, POINT_DEF_BONUS, dwBonusPercent, AFF_POLYMORPH, dwDuration - 1, 0, false);
			break;

		case POLYMORPH_SPD_BONUS:
			ch->add_affect(AFFECT_POLYMORPH, POINT_MOV_SPEED, dwBonusPercent, AFF_POLYMORPH, dwDuration - 1, 0, false);
			break;

		default:
		case POLYMORPH_NO_BONUS:
			break;
	}

	return true;
}

auto polymorph_utils::update_book_practice_grade(character * ch, item * item_value) -> bool
{
	if (ch == nullptr || item_value == nullptr)
		return false;

	if (item_value->get_socket(1) > 0)
		item_value->set_socket(1, item_value->get_socket(1) - 1);
	else
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("둔갑서 수련을 마첬습니다. 신선에게 찾아가세요."));

	return true;
}

auto polymorph_utils::give_book(character * ch, uint32_t mob_vnum, uint32_t practice_count, uint8_t book_level, uint8_t level_limit) -> bool
{
	if (ch == nullptr)
		return false;

	item * pItem = ch->auto_give_item(POLYMORPH_BOOK_ID, 1);

	if (pItem == nullptr)
		return false;

	if (mob_manager::instance().get(mob_vnum) == nullptr)
	{
		metin2::log::error("Cannot give polymorph book to pid {:d}: invalid mob vnum {:d} (practice count {:d}, book level {:d}, level limit {:d})", 
				ch->player_id(), mob_vnum, practice_count, book_level, level_limit);
		return false;
	}

	pItem->set_socket(0, mob_vnum);
	pItem->set_socket(1, practice_count);
	pItem->set_socket(2, book_level);
	return true;
}

auto polymorph_utils::book_upgrade(character * ch, item * item_value) -> bool
{
	if (ch == nullptr || item_value == nullptr)
		return false;

	item_value->set_socket(1, item_value->get_socket(2) * 50);
	item_value->set_socket(2, item_value->get_socket(2)+1);
	return true;
}
}
