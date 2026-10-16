#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/trade/shop_manager.h>
#include <metin2/game/world/entity.h>
#include <metin2/game/world/sectree.h>

#include <metin2/log/log.h>
#include <metin2/proto/mob_types.h>
#include <metin2/proto/tables.h>

#include <cstdint>
#include <limits>

namespace metin2::game::inline characters {

    auto on_click_shop(TRIGGERPARAM) -> int;
    auto on_click_talk(TRIGGERPARAM) -> int;

    auto on_attack_default(TRIGGERPARAM) -> int;

    struct trigger_function {
        int (*func)(TRIGGERPARAM);
    };

    trigger_function on_click_triggers[ON_CLICK_MAX_NUM] =
        {
            {
             nullptr,
             },
            {
             on_click_shop,
             },
    };

    void character::assign_triggers(const mob_table *table) {
        if (table->on_click_type >= ON_CLICK_MAX_NUM) {
            metin2::log::error("{} has invalid on-click type {:d}", name(), table->on_click_type);
            abort();
        }

        trigger_on_click_.type = table->on_click_type;
        trigger_on_click_.func = on_click_triggers[table->on_click_type].func;
    }

    auto on_click_shop(TRIGGERPARAM) -> int {
        shop_manager::instance().start_shopping(causer, ch);
        return 1;
    }

    auto find_victim(character *chr, int max_distance) -> character * {
        int min_distance = std::numeric_limits<int>::max();
        const int32_t lx = chr->x();
        const int32_t ly = chr->y();
        character *chr_victim = nullptr;
        character *chr_building = nullptr;

        if (chr->get_sectree() != nullptr) {
            chr->get_sectree()->for_each_around([&](entity *ent) {
                if (!ent->is_type(ENTITY_CHARACTER))
                    return;

                auto *pkChr = (character *)ent;

                if (pkChr->is_building() &&
                    (pkChr->is_affect_flag(AFF_BUILDING_CONSTRUCTION_SMALL) ||
                        pkChr->is_affect_flag(AFF_BUILDING_CONSTRUCTION_LARGE) ||
                        pkChr->is_affect_flag(AFF_BUILDING_UPGRADE))) {
                    chr_building = pkChr;
                }

                if (pkChr->is_npc()) {
                    if (!pkChr->is_monster() || !chr->is_attack_mob() || chr->is_aggressive())
                        return;
                }

                if (pkChr->is_dead())
                    return;

                if (pkChr->is_affect_flag(AFF_EUNHYUNG) ||
                    pkChr->is_affect_flag(AFF_INVISIBILITY) ||
                    pkChr->is_affect_flag(AFF_REVIVE_INVISIBLE))
                    return;

                if (pkChr->is_affect_flag(AFF_TERROR) && chr->is_immune(IMMUNE_TERROR) == false) {
                    if (pkChr->level() >= chr->level())
                        return;
                }

                if (chr->is_no_attack_shinsu()) {
                    if (pkChr->empire() == 1)
                        return;
                }

                if (chr->is_no_attack_chunjo()) {
                    if (pkChr->empire() == 2)
                        return;
                }

                if (chr->is_no_attack_jinno()) {
                    if (pkChr->empire() == 3)
                        return;
                }

                int iDistance = distance_approx(lx - pkChr->x(), ly - pkChr->y());

                if (iDistance < min_distance && iDistance <= max_distance) {
                    chr_victim = pkChr;
                    min_distance = iDistance;
                }
            });
        }

        if ((chr_building && chr->hp() * 2 > chr->max_hp()) || !chr_victim)
            return chr_building;

        return chr_victim;
    }
}
