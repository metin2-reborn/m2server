#include <metin2/game/trade/shop_ex.h>

#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/trade/shop.h>

#include <metin2/log/log.h>
#include <metin2/proto/inventory.h>
#include <metin2/proto/item_types.h>
#include <metin2/proto/log_types.h>
#include <metin2/proto/shop_types.h>
#include <metin2/proto/tables.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace metin2::game::inline trade {

    auto shop_ex::create(uint32_t vnum, uint32_t npc_vnum) -> bool {
        vnum_ = vnum;
        return true;
    }

    auto shop_ex::add_shop_table(shop_table_ex &shop_table) -> bool {
        for (auto it = vec_shop_tabs_.begin(); it != vec_shop_tabs_.end(); it++) {
            const shop_table_ex &shop_table_value = *it;
            if (0 != shop_table_value.vnum && shop_table_value.vnum == shop_table.vnum)
                return false;
            if (0 != shop_table_value.npc_vnum && shop_table_value.npc_vnum == shop_table.vnum)
                return false;
        }
        vec_shop_tabs_.push_back(shop_table);
        return true;
    }

    auto shop_ex::add_guest(character *ch, uint32_t owner_vid, bool other_empire) -> bool {
        if (!ch)
            return false;

        if (ch->get_exchange())
            return false;

        if (ch->get_shop())
            return false;

        ch->set_shop(this);

        map_guest_.insert(guest_map_type::value_type(ch, other_empire));

        packet_gc_shop pack;

        pack.header = HEADER_GC_SHOP;
        pack.subheader = SHOP_SUBHEADER_GC_START_EX;

        packet_gc_shop_start_ex pack2;

        memset(&pack2, 0, sizeof(pack2));

        pack2.owner_vid = owner_vid;
        pack2.shop_tab_count = vec_shop_tabs_.size();
        char temp[8096];
        char *buf = &temp[0];
        size_t size = 0;
        for (auto it = vec_shop_tabs_.begin(); it != vec_shop_tabs_.end(); it++) {
            const shop_table_ex &shop_tab = *it;
            packet_gc_shop_start_ex::sub_packet_shop_tab pack_tab;
            pack_tab.coin_type = shop_tab.coin_type;
            memcpy(pack_tab.name, shop_tab.name.c_str(), SHOP_TAB_NAME_MAX);

            for (uint8_t i = 0; i < SHOP_HOST_ITEM_MAX_NUM; i++) {
                pack_tab.items[i].vnum = shop_tab.items[i].vnum;
                pack_tab.items[i].count = shop_tab.items[i].count;
                switch (shop_tab.coin_type) {
                case SHOP_COIN_TYPE_GOLD:
                    if (other_empire)
                        pack_tab.items[i].price = shop_tab.items[i].price * 3;
                    else
                        pack_tab.items[i].price = shop_tab.items[i].price;
                    break;
                case SHOP_COIN_TYPE_SECONDARY_COIN:
                    pack_tab.items[i].price = shop_tab.items[i].price;
                    break;
                }
                memset(pack_tab.items[i].attr, 0, sizeof(pack_tab.items[i].attr));
                memset(pack_tab.items[i].sockets, 0, sizeof(pack_tab.items[i].sockets));
            }

            memcpy(buf, &pack_tab, sizeof(pack_tab));
            buf += sizeof(pack_tab);
            size += sizeof(pack_tab);
        }

        pack.size = sizeof(pack) + sizeof(pack2) + size;

        ch->get_desc()->buffered_packet(&pack, sizeof(packet_gc_shop));
        ch->get_desc()->buffered_packet(&pack2, sizeof(packet_gc_shop_start_ex));
        ch->get_desc()->packet(temp, size);

        return true;
    }

    auto shop_ex::buy(character *ch, uint8_t pos) -> int {
        uint8_t tab_idx = pos / SHOP_HOST_ITEM_MAX_NUM;
        uint8_t slot_pos = pos % SHOP_HOST_ITEM_MAX_NUM;
        if (tab_idx >= tab_count()) {
            metin2::log::warn("Player {1} tried to buy from invalid shop slot {0:d}", pos, ch->name());
            return SHOP_SUBHEADER_GC_INVALID_POS;
        }

        metin2::log::debug("Shop: player {} buys slot {:d}", ch->name(), pos);

        auto it = map_guest_.find(ch);

        if (it == map_guest_.end())
            return SHOP_SUBHEADER_GC_END;

        shop_table_ex &shop_tab = vec_shop_tabs_[tab_idx];
        shop_item_table &r_item = shop_tab.items[slot_pos];

        if (r_item.price <= 0) {
            get_log_manager().hack_log("SHOP_BUY_GOLD_OVERFLOW", ch);
            return SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY;
        }

        uint32_t price_value = r_item.price;

        switch (shop_tab.coin_type) {
        case SHOP_COIN_TYPE_GOLD:
            if (it->second)
                price_value *= 3;

            if (ch->gold() < (int)price_value) {
                metin2::log::debug("Shop: {} cannot afford item (has {:d}, price {:d})", ch->name(), ch->gold(), price_value);
                return SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY;
            }
            break;
        case SHOP_COIN_TYPE_SECONDARY_COIN:
            {
                int count = ch->count_specify_type_item(ITEM_SECONDARY_COIN);
                if (static_cast<uint32_t>(count) < price_value) {
                    metin2::log::debug("Shop: {} lacks secondary coins (has {:d}, price {:d})", ch->name(), count, price_value);
                    return SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY_EX;
                }
            }
            break;
        }

        item *item;

        item = get_item_manager().create_item(r_item.vnum, r_item.count);

        if (!item)
            return SHOP_SUBHEADER_GC_SOLD_OUT;

        int empty_pos;
        if (item->is_dragon_soul()) {
            empty_pos = ch->empty_dragon_soul_inventory(item);
        } else {
            empty_pos = ch->empty_inventory(item->size());
        }

        if (empty_pos < 0) {
            metin2::log::debug("Shop: inventory of {} is full (item size {:d})", ch->name(), item->size());
            get_item_manager().destroy_item(item);
            return SHOP_SUBHEADER_GC_INVENTORY_FULL;
        }

        switch (shop_tab.coin_type) {
        case SHOP_COIN_TYPE_GOLD:
            ch->point_change(POINT_GOLD, -price_value, false);
            break;
        case SHOP_COIN_TYPE_SECONDARY_COIN:
            ch->remove_specify_type_item(ITEM_SECONDARY_COIN, price_value);
            break;
        }

        if (item->is_dragon_soul())
            item->add_to_character(ch, item_pos(DRAGON_SOUL_INVENTORY, empty_pos));
        else
            item->add_to_character(ch, item_pos(INVENTORY, empty_pos));

        get_item_manager().flush_delayed_save(item);
        get_log_manager().item_log(ch, item, "BUY", item->name());

        if (item->vnum() >= 80003 && item->vnum() <= 80007) {
            get_log_manager().gold_bar_log(ch->player_id(), item->id(), PERSONAL_SHOP_BUY, "");
        }

        get_db_manager().send_money_log(MONEY_LOG_SHOP, item->vnum(), -price_value);

        if (item)
            metin2::log::info("Shop: {} bought {} (count {:d}, id {:d}) for {:d}", ch->name(), item->name(), item->count(), item->id(), price_value);

        ch->save();

        return (SHOP_SUBHEADER_GC_OK);
    }
}
