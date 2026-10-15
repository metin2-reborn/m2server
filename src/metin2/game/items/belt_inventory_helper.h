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

	// Returns which cells are usable based on the current belt level
	static const grade_unit* available_rule_table_by_grade()
	{
		/**
			A belt can have levels +0 to +9; by level it falls into 7 grades, which activate the inventory.
			Usable cells per belt level are shown below. A cell is usable if current grade >= its activation grade.
			(However, at level 0 nothing is usable; the number in parentheses is the grade)
			
				2(1)  4(2)  6(4)  8(6)
				5(3)  5(3)  6(4)  8(6)
				7(5)  7(5)  7(5)  8(6)
				9(7)  9(7)  9(7)  9(7)

			The belt inventory is 4x4 (16 cells)
		*/

		static grade_unit availableRuleByGrade[BELT_INVENTORY_SLOT_COUNT] = {
			1, 2, 4, 6,
			3, 3, 4, 6,
			5, 5, 5, 6,
			7, 7, 7, 7
		};

		return availableRuleByGrade;
	}

	static bool is_available_cell(uint16_t cell, int belt_grade /*int beltLevel*/)
	{
		// Design changed again.. ugh...
		//const TGradeUnit beltGrade = GetBeltGradeByRefineLevel(beltLevel);		
		const grade_unit* ruleTable = available_rule_table_by_grade();

		return ruleTable[cell] <= belt_grade;
	}

	/// Checks whether the pc's belt inventory contains at least one item.
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

	/// Checks whether the item's type can go into the belt inventory. (the designers decide this rule)
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
