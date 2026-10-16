#include <metin2/game/items/item_addon.h>

#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/items/item.h>

#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/apply_types.h>

#include <algorithm>
#include <cstdlib>

namespace metin2::game::inline items {

    item_addon_manager::item_addon_manager() {
    }

    item_addon_manager::~item_addon_manager() {
    }

    void item_addon_manager::apply_addon_to(int addon_type, item *item_value) {
        if (!item_value) {
            metin2::log::error("Cannot apply addon {:d}: no item", addon_type);
            return;
        }

        int iSkillBonus = std::clamp((int)(gauss(0, 5) + 0.5f), -30, 30);
        int iNormalHitBonus = 0;
        if (abs(iSkillBonus) <= 20)
            iNormalHitBonus = -2 * iSkillBonus + abs(number(-8, 8) + number(-8, 8)) + number(1, 4);
        else
            iNormalHitBonus = -2 * iSkillBonus + number(1, 5);

        item_value->remove_attribute_type(APPLY_SKILL_DAMAGE_BONUS);
        item_value->remove_attribute_type(APPLY_NORMAL_HIT_DAMAGE_BONUS);
        item_value->add_attribute(APPLY_NORMAL_HIT_DAMAGE_BONUS, iNormalHitBonus);
        item_value->add_attribute(APPLY_SKILL_DAMAGE_BONUS, iSkillBonus);
    }
}
