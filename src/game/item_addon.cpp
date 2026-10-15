#include "item_addon.h"

#include "item.h"
#include "typedef.h"
#include "utils.h"

#include <common/length.h>
#include <metin2/log/log.h>
#include <libthecore/utils.h>

#include <algorithm>
#include <cstdlib>

namespace metin2::game {

CItemAddonManager::CItemAddonManager()
{
}

CItemAddonManager::~CItemAddonManager()
{
}

void CItemAddonManager::ApplyAddonTo(int iAddonType, CItem * pItem)
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

	pItem->RemoveAttributeType(APPLY_SKILL_DAMAGE_BONUS);
	pItem->RemoveAttributeType(APPLY_NORMAL_HIT_DAMAGE_BONUS);
	pItem->AddAttribute(APPLY_NORMAL_HIT_DAMAGE_BONUS, iNormalHitBonus);
	pItem->AddAttribute(APPLY_SKILL_DAMAGE_BONUS, iSkillBonus);
}
}
