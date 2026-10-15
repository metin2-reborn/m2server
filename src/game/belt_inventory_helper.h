#ifndef __HEADER_BELT_INVENTORY_HELPER__
#define __HEADER_BELT_INVENTORY_HELPER__

#include "char.h"
#include "item.h"
#include "typedef.h"

#include <common/length.h>

#include <cstdint>


class CBeltInventoryHelper
{
public:
	typedef uint8_t	TGradeUnit;

	// Returns which cells are usable based on the current belt level
	static const TGradeUnit* GetAvailableRuleTableByGrade()
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

		static TGradeUnit availableRuleByGrade[BELT_INVENTORY_SLOT_COUNT] = {
			1, 2, 4, 6,
			3, 3, 4, 6,
			5, 5, 5, 6,
			7, 7, 7, 7
		};

		return availableRuleByGrade;
	}

	static bool IsAvailableCell(uint16_t cell, int beltGrade /*int beltLevel*/)
	{
		// Design changed again.. ugh...
		//const TGradeUnit beltGrade = GetBeltGradeByRefineLevel(beltLevel);		
		const TGradeUnit* ruleTable = GetAvailableRuleTableByGrade();

		return ruleTable[cell] <= beltGrade;
	}

	/// Checks whether the pc's belt inventory contains at least one item.
	static bool IsExistItemInBeltInventory(CHARACTER * pc)
	{
		for (uint16_t i = BELT_INVENTORY_SLOT_START; i < BELT_INVENTORY_SLOT_END; ++i)
		{
			CItem * beltInventoryItem = pc->GetInventoryItem(i);

			if (nullptr != beltInventoryItem)
				return true;
		}

		return false;
	}

	/// Checks whether the item's type can go into the belt inventory. (the designers decide this rule)
	static bool CanMoveIntoBeltInventory(CItem * item)
	{
		bool canMove = false;

		if (item->GetType() == ITEM_USE)
		{
			switch (item->GetSubType())
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

#endif //__HEADER_BELT_INVENTORY_HELPER__