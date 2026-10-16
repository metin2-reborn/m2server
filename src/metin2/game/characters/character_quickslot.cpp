#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/items/item.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/packet.h>

#include <metin2/proto/character_types.h>
#include <metin2/proto/inventory.h>
#include <metin2/proto/tables.h>

#include <cstdint>
#include <cstring>

namespace metin2::game::inline characters {

    void character::sync_quickslot(uint8_t type_value, uint8_t old_pos, uint8_t new_pos) {
        if (old_pos == new_pos)
            return;

        for (int i = 0; i < QUICKSLOT_MAX_NUM; ++i) {
            if (quickslot_[i].type == type_value && quickslot_[i].pos == old_pos) {
                if (new_pos == 255)
                    del_quickslot(i);
                else {
                    quickslot slot;

                    slot.type = type_value;
                    slot.pos = new_pos;

                    set_quickslot(i, slot);
                }
            }
        }
    }

    auto character::get_quickslot(uint8_t pos, quickslot **pp_slot) -> bool {
        if (pos >= QUICKSLOT_MAX_NUM)
            return false;

        *pp_slot = &quickslot_[pos];
        return true;
    }

    auto character::set_quickslot(uint8_t pos, quickslot &slot_value) -> bool {
        struct packet_quickslot_add pack_quickslot_add;

        if (pos >= QUICKSLOT_MAX_NUM)
            return false;

        if (slot_value.type >= QUICKSLOT_TYPE_MAX_NUM)
            return false;

        for (int i = 0; i < QUICKSLOT_MAX_NUM; ++i) {
            if (slot_value.type == 0)
                continue;
            else if (quickslot_[i].type == slot_value.type && quickslot_[i].pos == slot_value.pos)
                del_quickslot(i);
        }

        item_pos srcCell(INVENTORY, slot_value.pos);

        switch (slot_value.type) {
        case QUICKSLOT_TYPE_ITEM:
            if (false == srcCell.is_default_inventory_position() && false == srcCell.is_belt_inventory_position())
                return false;

            break;

        case QUICKSLOT_TYPE_SKILL:
            if ((int)slot_value.pos >= SKILL_MAX_NUM)
                return false;

            break;

        case QUICKSLOT_TYPE_COMMAND:
            break;

        default:
            return false;
        }

        quickslot_[pos] = slot_value;

        if (get_desc()) {
            pack_quickslot_add.header = HEADER_GC_QUICKSLOT_ADD;
            pack_quickslot_add.pos = pos;
            pack_quickslot_add.slot = quickslot_[pos];

            get_desc()->packet(&pack_quickslot_add, sizeof(pack_quickslot_add));
        }

        return true;
    }

    auto character::del_quickslot(uint8_t pos) -> bool {
        struct packet_quickslot_del pack_quickslot_del;

        if (pos >= QUICKSLOT_MAX_NUM)
            return false;

        memset(&quickslot_[pos], 0, sizeof(quickslot));

        pack_quickslot_del.header = HEADER_GC_QUICKSLOT_DEL;
        pack_quickslot_del.pos = pos;

        get_desc()->packet(&pack_quickslot_del, sizeof(pack_quickslot_del));
        return true;
    }

    auto character::swap_quickslot(uint8_t a, uint8_t b) -> bool {
        struct packet_quickslot_swap pack_quickslot_swap;
        quickslot quickslot;

        if (a >= QUICKSLOT_MAX_NUM || b >= QUICKSLOT_MAX_NUM)
            return false;

        quickslot = quickslot_[a];

        quickslot_[a] = quickslot_[b];
        quickslot_[b] = quickslot;

        pack_quickslot_swap.header = HEADER_GC_QUICKSLOT_SWAP;
        pack_quickslot_swap.pos = a;
        pack_quickslot_swap.pos_to = b;

        get_desc()->packet(&pack_quickslot_swap, sizeof(pack_quickslot_swap));
        return true;
    }

    void character::chain_quickslot_item(item *item_value, uint8_t type_value, uint8_t old_pos) {
        if (item_value->is_dragon_soul())
            return;
        for (int i = 0; i < QUICKSLOT_MAX_NUM; ++i) {
            if (quickslot_[i].type == type_value && quickslot_[i].pos == old_pos) {
                quickslot slot;
                slot.type = type_value;
                slot.pos = item_value->cell();

                set_quickslot(i, slot);

                break;
            }
        }
    }
}
