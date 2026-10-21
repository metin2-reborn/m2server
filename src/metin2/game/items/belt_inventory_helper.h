#pragma once

#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/items/item.h>

#include <metin2/proto/inventory.h>
#include <metin2/proto/item_types.h>

#include <cstdint>

namespace metin2::game::inline items {

    class belt_inventory_helper {
      public:
        using grade_unit = uint8_t;

        static auto available_rule_table_by_grade() -> const grade_unit * {

            static grade_unit available_rule_by_grade[BELT_INVENTORY_SLOT_COUNT] = {
                1, 2, 4, 6,
                3, 3, 4, 6,
                5, 5, 5, 6,
                7, 7, 7, 7};

            return available_rule_by_grade;
        }

        static auto is_available_cell(uint16_t cell, int belt_grade) -> bool {
            const grade_unit *rule_table = available_rule_table_by_grade();

            return rule_table[cell] <= belt_grade;
        }

        static auto is_exist_item_in_belt_inventory(character *pc) -> bool {
            for (uint16_t i = BELT_INVENTORY_SLOT_START; i < BELT_INVENTORY_SLOT_END; ++i) {
                item *belt_inventory_item = pc->inventory_item(i);

                if (nullptr != belt_inventory_item)
                    return true;
            }

            return false;
        }

        static auto can_move_into_belt_inventory(item *item) -> bool {
            bool can_move = false;

            if (item->type() == ITEM_USE) {
                switch (item->sub_type()) {
                case USE_POTION:
                case USE_POTION_NODELAY:
                case USE_ABILITY_UP:
                    can_move = true;
                    break;
                }
            }

            return can_move;
        }
    };
}
