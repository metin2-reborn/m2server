#include <metin2/game/items/item_addon.h>

#include <metin2/game/items/item.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>

#include <metin2/proto/length.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <cstdlib>

namespace metin2::game::inline items {

item_addon_manager::item_addon_manager()
{
}

item_addon_manager::~item_addon_manager()
{
}

void item_addon_manager::apply_addon_to(int iAddonType, item * pItem)
{
	if (!pItem)
	{
		metin2::log::error("ITEM pointer null");
		return;
	}

	// TODO for now, hardcoded to apply only when the normal-attack skill value changes.

	int iSkillBonus = std::clamp((int) (gauss_random(0, 5) + 0.5f), -30, 30);
	int iNormalHitBonus = 0;
	if (abs(iSkillBonus) <= 20)
		iNormalHitBonus = -2 * iSkillBonus + abs(number(-8, 8) + number(-8, 8)) + number(1, 4);
	else
		iNormalHitBonus = -2 * iSkillBonus + number(1, 5);

	pItem->remove_attribute_type(APPLY_SKILL_DAMAGE_BONUS);
	pItem->remove_attribute_type(APPLY_NORMAL_HIT_DAMAGE_BONUS);
	pItem->add_attribute(APPLY_NORMAL_HIT_DAMAGE_BONUS, iNormalHitBonus);
	pItem->add_attribute(APPLY_SKILL_DAMAGE_BONUS, iSkillBonus);
}
}
