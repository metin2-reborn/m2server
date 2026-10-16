#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/items/dragon_soul.h>
#include <metin2/game/items/item.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/world/entity.h>

#include <metin2/log/log.h>
#include <metin2/proto/length.h>

namespace metin2::game::inline characters {

    // Needs affects and quests loaded, so load_affect calls it.
    void character::dragon_soul_initialize() {
        for (int i = INVENTORY_MAX_NUM + WEAR_MAX_NUM; i < DRAGON_SOUL_EQUIP_SLOT_END; i++) {
            item *pItem = get_item(item_pos(INVENTORY, i));
            if (nullptr != pItem)
                pItem->set_socket(ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX, 0);
        }

        if (find_affect(AFFECT_DRAGON_SOUL_DECK_0)) {
            dragon_soul_activate_deck(DRAGON_SOUL_DECK_0);
        } else if (find_affect(AFFECT_DRAGON_SOUL_DECK_1)) {
            dragon_soul_activate_deck(DRAGON_SOUL_DECK_1);
        }
    }

    auto character::dragon_soul_get_active_deck() const -> int {
        return points_instant_.dragon_soul_active_deck;
    }

    auto character::dragon_soul_is_deck_activated() const -> bool {
        return points_instant_.dragon_soul_active_deck >= 0;
    }

    auto character::dragon_soul_is_qualified() const -> bool {
        return find_affect(AFFECT_DRAGON_SOUL_QUALIFIED) != nullptr;
    }

    void character::dragon_soul_give_qualification() {
        if (nullptr == find_affect(AFFECT_DRAGON_SOUL_QUALIFIED)) {
            log_manager::instance().char_log(this, 0, "DS_QUALIFIED", "");
        }
        add_affect(AFFECT_DRAGON_SOUL_QUALIFIED, APPLY_NONE, 0, AFF_NONE, INFINITE_AFFECT_DURATION, 0, false, false);
    }

    auto character::dragon_soul_activate_deck(int deck_idx) -> bool {
        if (deck_idx < DRAGON_SOUL_DECK_0 || deck_idx >= DRAGON_SOUL_DECK_MAX_NUM) {
            return false;
        }

        if (dragon_soul_get_active_deck() == deck_idx)
            return true;

        dragon_soul_deactivate_all();

        if (!dragon_soul_is_qualified()) {
            chat_packet(CHAT_TYPE_INFO, lc_text("용혼석 상자가 활성화되지 않았습니다."));
            return false;
        }

        add_affect(AFFECT_DRAGON_SOUL_DECK_0 + deck_idx, APPLY_NONE, 0, 0, INFINITE_AFFECT_DURATION, 0, false);

        points_instant_.dragon_soul_active_deck = deck_idx;

        for (int i = DRAGON_SOUL_EQUIP_SLOT_START + DS_SLOT_MAX * deck_idx;
            i < DRAGON_SOUL_EQUIP_SLOT_START + DS_SLOT_MAX * (deck_idx + 1); i++) {
            item *pItem = inventory_item(i);
            if (nullptr != pItem)
                ds_manager::instance().activate_dragon_soul(pItem);
        }

        return true;
    }

    void character::dragon_soul_deactivate_all() {
        for (int i = DRAGON_SOUL_EQUIP_SLOT_START; i < DRAGON_SOUL_EQUIP_SLOT_END; i++) {
            ds_manager::instance().deactivate_dragon_soul(inventory_item(i), true);
        }
        points_instant_.dragon_soul_active_deck = -1;
        remove_affect(AFFECT_DRAGON_SOUL_DECK_0);
        remove_affect(AFFECT_DRAGON_SOUL_DECK_1);
    }

    void character::dragon_soul_clean_up() {
        for (int i = DRAGON_SOUL_EQUIP_SLOT_START; i < DRAGON_SOUL_EQUIP_SLOT_END; i++) {
            ds_manager::instance().deactivate_dragon_soul(inventory_item(i), true);
        }
    }

    auto character::dragon_soul_refine_window_open(entity *entity_value) -> bool {
        if (nullptr == points_instant_.dragon_soul_refine_window_opener) {
            points_instant_.dragon_soul_refine_window_opener = entity_value;
        }

        packet_gc_dragon_soul_refine PDS;
        PDS.header = HEADER_GC_DRAGON_SOUL_REFINE;
        PDS.sub_type = DS_SUB_HEADER_OPEN;
        desc *d = get_desc();

        if (nullptr == d) {
            metin2::log::error("Player {} has no descriptor", name());
            return false;
        }

        d->packet(&PDS, sizeof(PDS));
        return true;
    }

    auto character::dragon_soul_refine_window_close() -> bool {
        points_instant_.dragon_soul_refine_window_opener = nullptr;
        return true;
    }

    auto character::dragon_soul_refine_window_can_refine() -> bool {
        return nullptr != points_instant_.dragon_soul_refine_window_opener;
    }
}
