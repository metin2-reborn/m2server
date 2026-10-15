#pragma once

#include <metin2/game/characters/character.h>
#include <metin2/game/items/item.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>

#include <cstdint>

namespace metin2::game::inline items {

class belt_inventory_helper
{
public:
	typedef uint8_t	grade_unit;

	static const grade_unit* available_rule_table_by_grade()
	{

		static grade_unit availableRuleByGrade[BELT_INVENTORY_SLOT_COUNT] = {
			1, 2, 4, 6,
			3, 3, 4, 6,
			5, 5, 5, 6,
			7, 7, 7, 7
		};

		return availableRuleByGrade;
	}

	static bool is_available_cell(uint16_t cell, int belt_grade )
	{
		const grade_unit* ruleTable = available_rule_table_by_grade();

		return ruleTable[cell] <= belt_grade;
	}

	static bool is_exist_item_in_belt_inventory(character * pc)
	{
		for (uint16_t i = BELT_INVENTORY_SLOT_START; i < BELT_INVENTORY_SLOT_END; ++i)
		{
			item * beltInventoryItem = pc->inventory_item(i);

			if (nullptr != beltInventoryItem)
				return true;
		}

		return false;
	}

	static bool can_move_into_belt_inventory(item * item)
	{
		bool canMove = false;

		if (item->type() == ITEM_USE)
		{
			switch (item->sub_type())
			{
			case USE_POTION:
			case USE_POTION_NODELAY:
			case USE_ABILITY_UP:
				canMove = true;
				break;
			}
		}

		return canMove;
	}

};
}
