#pragma once

#include <metin2/proto/item_types.h>
#include <metin2/proto/limits.h>

#include <cstdint>

namespace metin2::inline proto {
    inline constexpr int INVENTORY_MAX_NUM = 90;
    inline constexpr int DRAGON_SOUL_BOX_SIZE = 32;
    inline constexpr int DRAGON_SOUL_BOX_COLUMN_NUM = 8;
    inline constexpr int DRAGON_SOUL_REFINE_GRID_SIZE = 15;
    inline constexpr int WEAR_MAX_NUM = 32;
    inline constexpr int BELT_INVENTORY_SLOT_WIDTH = 4;
    inline constexpr int BELT_INVENTORY_SLOT_HEIGHT = 4;
    inline constexpr int BELT_INVENTORY_SLOT_COUNT = BELT_INVENTORY_SLOT_WIDTH * BELT_INVENTORY_SLOT_HEIGHT;

    enum wear_positions {
        WEAR_BODY,
        WEAR_HEAD,
        WEAR_FOOTS,
        WEAR_WRIST,
        WEAR_WEAPON,
        WEAR_NECK,
        WEAR_EAR,
        WEAR_UNIQUE1,
        WEAR_UNIQUE2,
        WEAR_ARROW,
        WEAR_SHIELD,
        WEAR_ABILITY1,
        WEAR_ABILITY2,
        WEAR_ABILITY3,
        WEAR_ABILITY4,
        WEAR_ABILITY5,
        WEAR_ABILITY6,
        WEAR_ABILITY7,
        WEAR_ABILITY8,
        WEAR_COSTUME_BODY,
        WEAR_COSTUME_HAIR,

        WEAR_RING1,
        WEAR_RING2,

        WEAR_BELT,

        WEAR_MAX = 32
    };

    enum dragon_soul_deck_type {
        DRAGON_SOUL_DECK_0,
        DRAGON_SOUL_DECK_1,
        DRAGON_SOUL_DECK_MAX_NUM = 2,

        DRAGON_SOUL_DECK_RESERVED_MAX_NUM = 3, // Reserved for more decks: adding a deck must take the same amount from RESERVED.
    };

    enum windows {
        RESERVED_WINDOW,
        INVENTORY,
        EQUIPMENT,
        SAFEBOX,
        MALL,
        DRAGON_SOUL_INVENTORY,
        BELT_INVENTORY,
        GROUND
    };

    inline constexpr int DRAGON_SOUL_INVENTORY_MAX_NUM = static_cast<int>(DS_SLOT_MAX) * static_cast<int>(DRAGON_SOUL_GRADE_MAX) * DRAGON_SOUL_BOX_SIZE;
    inline constexpr int DRAGON_SOUL_EQUIP_SLOT_START = INVENTORY_MAX_NUM + WEAR_MAX_NUM;
    inline constexpr int DRAGON_SOUL_EQUIP_SLOT_END = DRAGON_SOUL_EQUIP_SLOT_START + (static_cast<int>(DS_SLOT_MAX) * static_cast<int>(DRAGON_SOUL_DECK_MAX_NUM));
    inline constexpr int DRAGON_SOUL_EQUIP_RESERVED_SLOT_END = DRAGON_SOUL_EQUIP_SLOT_END + (static_cast<int>(DS_SLOT_MAX) * static_cast<int>(DRAGON_SOUL_DECK_RESERVED_MAX_NUM));
    inline constexpr int BELT_INVENTORY_SLOT_START = DRAGON_SOUL_EQUIP_RESERVED_SLOT_END;
    inline constexpr int BELT_INVENTORY_SLOT_END = BELT_INVENTORY_SLOT_START + BELT_INVENTORY_SLOT_COUNT;
    inline constexpr int INVENTORY_AND_EQUIP_SLOT_MAX = BELT_INVENTORY_SLOT_END;

#pragma pack(push, 1)
    struct item_pos {
        uint8_t window_type;
        uint16_t cell;
        item_pos() {
            window_type = INVENTORY;
            cell = WORD_MAX;
        }

        item_pos(uint8_t window_type_value, uint16_t cell_value) {
            window_type = window_type_value;
            cell = cell_value;
        }

        auto is_valid_item_position() const -> bool {
            switch (window_type) {
            case RESERVED_WINDOW:
                return false;

            case INVENTORY:
            case EQUIPMENT:
            case BELT_INVENTORY:
                return cell < INVENTORY_AND_EQUIP_SLOT_MAX;

            case DRAGON_SOUL_INVENTORY:
                return cell < (DRAGON_SOUL_INVENTORY_MAX_NUM);

            case SAFEBOX:
            case MALL:
                return false;

            default:
                return false;
            }

            return false;
        }

        auto is_same_position(const item_pos &other) const -> bool {
            return *this == other || ((INVENTORY == window_type || EQUIPMENT == window_type) && (INVENTORY == other.window_type || EQUIPMENT == other.window_type) && cell == other.cell);
        }

        auto is_equip_position() const -> bool {
            return ((INVENTORY == window_type || EQUIPMENT == window_type) && cell >= INVENTORY_MAX_NUM && cell < INVENTORY_MAX_NUM + WEAR_MAX_NUM) || is_dragon_soul_equip_position();
        }

        auto is_dragon_soul_equip_position() const -> bool {
            return (DRAGON_SOUL_EQUIP_SLOT_START <= cell) && (DRAGON_SOUL_EQUIP_SLOT_END > cell);
        }

        auto is_belt_inventory_position() const -> bool {
            return (BELT_INVENTORY_SLOT_START <= cell) && (BELT_INVENTORY_SLOT_END > cell);
        }

        auto is_default_inventory_position() const -> bool {
            return INVENTORY == window_type && cell < INVENTORY_MAX_NUM;
        }

        auto operator==(const struct item_pos &rhs) const -> bool {
            return (window_type == rhs.window_type) && (cell == rhs.cell);
        }

        auto operator<(const struct item_pos &rhs) const -> bool {
            return (window_type < rhs.window_type) || ((window_type == rhs.window_type) && (cell < rhs.cell));
        }
    };

#pragma pack(pop)

    const item_pos NPOS(RESERVED_WINDOW, WORD_MAX);
}
