#include <metin2/game/trade/shop.h>

#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/empire/monarch.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/text/locale.hpp>

#include <metin2/common/grid.h>
#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/inventory.h>
#include <metin2/proto/item_types.h>
#include <metin2/proto/limits.h>
#include <metin2/proto/log_types.h>
#include <metin2/proto/shop_types.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <memory>
#include <string>

namespace metin2::game::inline trade {
    shop::shop()
        : vnum_(0), grid_(std::make_unique<grid>(5, 9)), pc_(nullptr) {
    }

    shop::~shop() {
        packet_gc_shop pack;

        pack.header = HEADER_GC_SHOP;
        pack.subheader = SHOP_SUBHEADER_GC_END;
        pack.size = sizeof(packet_gc_shop);

        broadcast(&pack, sizeof(pack));

        guest_map_type::iterator it;

        it = map_guest_.begin();

        while (it != map_guest_.end()) {
            character *ch = it->first;
            ch->set_shop(nullptr);
            ++it;
        }
    }

    void shop::set_pc_shop(character *ch) {
        pc_ = ch;
    }

    auto shop::create(uint32_t vnum_value, uint32_t npc_vnum, shop_item_table *table) -> bool {
        metin2::log::debug("Shop: creating shop {:d} (npc {:d})", vnum_value, npc_vnum);

        vnum_ = vnum_value;

        uint8_t bItemCount;

        for (bItemCount = 0; bItemCount < SHOP_HOST_ITEM_MAX_NUM; ++bItemCount)
            if (0 == (table + bItemCount)->vnum)
                break;

        set_shop_items(table, bItemCount);
        return true;
    }

    void shop::set_shop_items(shop_item_table *table_value, uint8_t item_count) {
        if (item_count > SHOP_HOST_ITEM_MAX_NUM)
            return;

        grid_->clear();

        item_vector_.resize(SHOP_HOST_ITEM_MAX_NUM);
        std::fill(item_vector_.begin(), item_vector_.end(), shop_item());

        for (int i = 0; i < item_count; ++i) {
            item *pkItem = nullptr;
            const item_table *item_table;

            if (pc_) {
                pkItem = pc_->get_item(table_value->pos);

                if (!pkItem) {
                    metin2::log::error("Personal shop of {2}: no item in window {0:d} cell {1:d}", table_value->pos.window_type, table_value->pos.cell, pc_->name());
                    continue;
                }

                item_table = pkItem->proto();
            } else {
                if (!table_value->vnum)
                    continue;

                item_table = item_manager::instance().table(table_value->vnum);
            }

            if (!item_table) {
                metin2::log::error("Shop item vnum {:d} has no item proto", table_value->vnum);
                continue;
            }

            int iPos;

            if (is_pc_shop()) {
                metin2::log::debug("Personal shop: using slot {:d}", table_value->display_pos);
                iPos = table_value->display_pos;
            } else
                iPos = grid_->find_blank(1, item_table->size);

            if (iPos < 0) {
                metin2::log::error("Shop {:d} has no room for item vnum {:d}", vnum_, table_value->vnum);
                continue;
            }

            if (!grid_->is_empty(iPos, 1, item_table->size)) {
                if (is_pc_shop()) {
                    metin2::log::error("Personal shop slot is taken for player {} (pid {:d})", pc_->name(), pc_->player_id());
                } else {
                    metin2::log::error("Shop {:d} slot is taken for item vnum {:d}", vnum_, table_value->vnum);
                }
                continue;
            }

            grid_->put(iPos, 1, item_table->size);

            shop_item &item = item_vector_[iPos];

            item.item = pkItem;
            item.itemid = 0;

            if (item.item) {
                item.vnum = pkItem->vnum();
                item.count = pkItem->count();
                item.price = table_value->price;
                item.itemid = pkItem->id();
            } else {
                item.vnum = table_value->vnum;
                item.count = table_value->count;

                if (is_set(item_table->flags, ITEM_FLAG_COUNT_PER_1GOLD)) {
                    if (item_table->gold == 0)
                        item.price = item.count;
                    else
                        item.price = item.count / item_table->gold;
                } else
                    item.price = item_table->gold * item.count;
            }

            const std::string name = std::format("{:<20}(#{:<5}) (x {})", item_table->name, (int)item.vnum, item.count);

            metin2::log::debug("Shop: item {} price {:d}", name, item.price);
            ++table_value;
        }
    }

    auto shop::buy(character *ch, uint8_t pos) -> int {
        if (pos >= item_vector_.size()) {
            metin2::log::warn("Player {1} tried to buy from invalid shop slot {0:d}", pos, ch->name());
            return SHOP_SUBHEADER_GC_INVALID_POS;
        }

        metin2::log::debug("Shop: player {} buys slot {:d}", ch->name(), pos);

        auto it = map_guest_.find(ch);

        if (it == map_guest_.end())
            return SHOP_SUBHEADER_GC_END;

        shop_item &r_item = item_vector_[pos];

        if (r_item.price < 0) {
            log_manager::instance().hack_log("SHOP_BUY_GOLD_OVERFLOW", ch);
            return SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY;
        }

        item *pkSelectedItem = item_manager::instance().find(r_item.itemid);

        if (is_pc_shop()) {
            if (!pkSelectedItem) {
                metin2::log::warn("Player (pid {:d}) tried to buy an invalid item from the personal shop of pid {:d} (possible cheat)",
                    ch->player_id(),
                    pc_->player_id());

                return false;
            }

            if ((pkSelectedItem->owner() != pc_)) {
                metin2::log::warn("Player (pid {:d}) tried to buy an invalid item from the personal shop of pid {:d} (possible cheat)",
                    ch->player_id(),
                    pc_->player_id());

                return false;
            }
        }

        const int64_t llPrice = static_cast<int64_t>(r_item.price) * (it->second ? 3 : 1);

        if (llPrice > GOLD_MAX) {
            metin2::log::warn("Player {} tried to buy for {:d} gold, above the gold limit", ch->name(), static_cast<long long>(llPrice));
            return SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY;
        }

        auto dwPrice = static_cast<uint32_t>(llPrice);

        if (ch->gold() < (int)dwPrice) {
            metin2::log::debug("Shop: {} cannot afford item (has {:d}, price {:d})", ch->name(), ch->gold(), dwPrice);
            return SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY;
        }

        item *item;

        if (pc_)
            item = r_item.item;
        else
            item = item_manager::instance().create_item(r_item.vnum, r_item.count);

        if (!item)
            return SHOP_SUBHEADER_GC_SOLD_OUT;

        if (!pc_) {
            if (quest::quest_manager::instance().event_flag("hivalue_item_sell") == 0) {
                if (item->vnum() == 70024 || item->vnum() == 70035) {
                    return SHOP_SUBHEADER_GC_END;
                }
            }
        }

        int iEmptyPos;
        if (item->is_dragon_soul()) {
            iEmptyPos = ch->empty_dragon_soul_inventory(item);
        } else {
            iEmptyPos = ch->empty_inventory(item->size());
        }

        if (iEmptyPos < 0) {
            if (pc_) {
                metin2::log::debug("Personal shop: inventory of {} is full (item size {:d})", ch->name(), item->size());
                return SHOP_SUBHEADER_GC_INVENTORY_FULL;
            } else {
                metin2::log::debug("Shop: inventory of {} is full (item size {:d})", ch->name(), item->size());
                item_manager::instance().destroy_item(item);
                return SHOP_SUBHEADER_GC_INVENTORY_FULL;
            }
        }

        ch->point_change(POINT_GOLD, -dwPrice, false);

        uint32_t dwTax = 0;
        int iVal = 0;

        iVal = quest::quest_manager::instance().event_flag("personal_shop");

        if (0 < iVal) {
            if (iVal > 100)
                iVal = 100;

            dwTax = dwPrice * iVal / 100;
            dwPrice = dwPrice - dwTax;
        } else {
            iVal = 0;
            dwTax = 0;
        }

        if (!pc_) {
            monarch::instance().sendto_db_add_money(dwTax, ch->empire(), ch);
        }

        if (pc_) {
            pc_->sync_quickslot(QUICKSLOT_TYPE_ITEM, item->cell(), 255);

            std::string buf;

            if (item->vnum() >= 80003 && item->vnum() <= 80007) {
                buf = std::format("{} FROM: {} TO: {} PRICE: {}", item->name(), ch->player_id(), pc_->player_id(), dwPrice);
                log_manager::instance().gold_bar_log(ch->player_id(), item->id(), SHOP_BUY, buf.c_str());
                log_manager::instance().gold_bar_log(pc_->player_id(), item->id(), SHOP_SELL, buf.c_str());
            }

            item->remove_from_character();
            if (item->is_dragon_soul())
                item->add_to_character(ch, item_pos(DRAGON_SOUL_INVENTORY, iEmptyPos));
            else
                item->add_to_character(ch, item_pos(INVENTORY, iEmptyPos));
            item_manager::instance().flush_delayed_save(item);

            buf = std::format("{} {}({}) {} {}", item->name(), pc_->player_id(), pc_->name(), dwPrice, item->count());
            log_manager::instance().item_log(ch, item, "SHOP_BUY", buf.c_str());

            buf = std::format("{} {}({}) {} {}", item->name(), ch->player_id(), ch->name(), dwPrice, item->count());
            log_manager::instance().item_log(pc_, item, "SHOP_SELL", buf.c_str());

            r_item.item = nullptr;
            broadcast_update_item(pos);

            pc_->point_change(POINT_GOLD, dwPrice, false);

            if (iVal > 0)
                pc_->chat_packet(CHAT_TYPE_INFO, lc_text("판매금액의 %d %% 가 세금으로 나가게됩니다"), iVal);

            monarch::instance().sendto_db_add_money(dwTax, pc_->empire(), pc_);
        } else {
            if (item->is_dragon_soul())
                item->add_to_character(ch, item_pos(DRAGON_SOUL_INVENTORY, iEmptyPos));
            else
                item->add_to_character(ch, item_pos(INVENTORY, iEmptyPos));
            item_manager::instance().flush_delayed_save(item);
            log_manager::instance().item_log(ch, item, "BUY", item->name());

            if (item->vnum() >= 80003 && item->vnum() <= 80007) {
                log_manager::instance().gold_bar_log(ch->player_id(), item->id(), PERSONAL_SHOP_BUY, "");
            }

            db_manager::instance().send_money_log(MONEY_LOG_SHOP, item->vnum(), -dwPrice);
        }

        if (item)
            metin2::log::info("Shop: {} bought {} (count {:d}, id {:d}) for {:d}", ch->name(), item->name(), item->count(), item->id(), dwPrice);

        ch->save();

        return (SHOP_SUBHEADER_GC_OK);
    }

    auto shop::add_guest(character *ch, uint32_t owner_vid, bool other_empire) -> bool {
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
        pack.subheader = SHOP_SUBHEADER_GC_START;

        packet_gc_shop_start pack2;

        memset(&pack2, 0, sizeof(pack2));
        pack2.owner_vid = owner_vid;

        for (uint32_t i = 0; i < item_vector_.size() && i < SHOP_HOST_ITEM_MAX_NUM; ++i) {
            const shop_item &item = item_vector_[i];

            if (quest::quest_manager::instance().event_flag("hivalue_item_sell") == 0) {
                if (item.vnum == 70024 || item.vnum == 70035) {
                    continue;
                }
            }
            if (pc_ && !item.item)
                continue;

            pack2.items[i].vnum = item.vnum;

            if (other_empire)
                pack2.items[i].price = item.price * 3;
            else
                pack2.items[i].price = item.price;

            pack2.items[i].count = item.count;

            if (item.item) {
                std::memcpy(pack2.items[i].sockets, item.item->sockets(), sizeof(pack2.items[i].sockets));
                std::memcpy(pack2.items[i].attr, item.item->attributes(), sizeof(pack2.items[i].attr));
            }
        }

        pack.size = sizeof(pack) + sizeof(pack2);

        ch->get_desc()->buffered_packet(&pack, sizeof(packet_gc_shop));
        ch->get_desc()->packet(&pack2, sizeof(packet_gc_shop_start));
        return true;
    }

    void shop::remove_guest(character *ch) {
        if (ch->get_shop() != this)
            return;

        map_guest_.erase(ch);
        ch->set_shop(nullptr);

        packet_gc_shop pack;

        pack.header = HEADER_GC_SHOP;
        pack.subheader = SHOP_SUBHEADER_GC_END;
        pack.size = sizeof(packet_gc_shop);

        ch->get_desc()->packet(&pack, sizeof(pack));
    }

    void shop::broadcast(const void *data, int bytes) {
        metin2::log::debug("Shop: broadcast {} ({:d} bytes)", static_cast<const void *>(data), bytes);

        guest_map_type::iterator it;

        it = map_guest_.begin();

        while (it != map_guest_.end()) {
            character *ch = it->first;

            if (ch->get_desc())
                ch->get_desc()->packet(data, bytes);

            ++it;
        }
    }

    void shop::broadcast_update_item(uint8_t pos) {
        packet_gc_shop pack;
        packet_gc_shop_update_item pack2;

        temp_buffer buf;

        pack.header = HEADER_GC_SHOP;
        pack.subheader = SHOP_SUBHEADER_GC_UPDATE_ITEM;
        pack.size = sizeof(pack) + sizeof(pack2);

        pack2.pos = pos;

        if (pc_ && !item_vector_[pos].item)
            pack2.item.vnum = 0;
        else {
            pack2.item.vnum = item_vector_[pos].vnum;
            if (item_vector_[pos].item) {
                std::memcpy(pack2.item.sockets, item_vector_[pos].item->sockets(), sizeof(pack2.item.sockets));
                std::memcpy(pack2.item.attr, item_vector_[pos].item->attributes(), sizeof(pack2.item.attr));
            } else {
                memset(pack2.item.sockets, 0, sizeof(pack2.item.sockets));
                memset(pack2.item.attr, 0, sizeof(pack2.item.attr));
            }
        }

        pack2.item.price = item_vector_[pos].price;
        pack2.item.count = item_vector_[pos].count;

        buf.write(&pack, sizeof(pack));
        buf.write(&pack2, sizeof(pack2));

        broadcast(buf.read_peek(), buf.size());
    }

    auto shop::number_by_vnum(uint32_t vnum_value) -> int {
        int itemNumber = 0;

        for (uint32_t i = 0; i < item_vector_.size() && i < SHOP_HOST_ITEM_MAX_NUM; ++i) {
            const shop_item &item = item_vector_[i];

            if (item.vnum == vnum_value) {
                itemNumber += item.count;
            }
        }

        return itemNumber;
    }

    auto shop::is_selling_item(uint32_t item_id) -> bool {
        bool isSelling = false;

        for (uint32_t i = 0; i < item_vector_.size() && i < SHOP_HOST_ITEM_MAX_NUM; ++i) {
            if (static_cast<uint32_t>(item_vector_[i].itemid) == item_id) {
                isSelling = true;
                break;
            }
        }

        return isSelling;
    }
}
