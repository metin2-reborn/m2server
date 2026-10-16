#include <metin2/config/config.h>
#include <metin2/game/trade/shop_manager.h>

#include <algorithm>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/data/group_text_parse_tree.h>
#include <metin2/game/empire/monarch.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/trade/shop.h>
#include <metin2/game/trade/shop_ex.h>

#include <metin2/common/grid.h>
#include <metin2/log/log.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/item_types.h>
#include <metin2/proto/limits.h>
#include <metin2/proto/log_types.h>
#include <metin2/proto/shop_types.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <iterator>
#include <map>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

#include <strings.h>

namespace metin2::game::inline trade {

    shop_manager::shop_manager() = default;

    shop_manager::~shop_manager() {
        destroy();
    }

    auto shop_manager::initialize(shop_table *table, int size) -> bool {
        if (!map_shop_.empty())
            return false;

        int i;

        for (i = 0; i < size; ++i, ++table) {
            auto new_shop = std::make_unique<shop>();

            if (!new_shop->create(table->vnum, table->npc_vnum, table->items))
                continue;

            shop *shop = shops_.emplace_back(std::move(new_shop)).get();
            map_shop_.insert(shop_map::value_type(table->vnum, shop));
            shops_by_npc_vnum_.insert(shop_map::value_type(table->npc_vnum, shop));
        }
        const std::string shop_table_ex_file_name = std::format("{}/shop_table_ex.txt", locale_service_get_base_path());

        return read_shop_table_ex(shop_table_ex_file_name.c_str());
    }

    void shop_manager::destroy() {
        map_shop_.clear();
        shops_by_npc_vnum_.clear();
        shops_.clear();
    }

    auto shop_manager::get(uint32_t vnum) -> shop * {
        auto it = map_shop_.find(vnum);

        if (it == map_shop_.end())
            return nullptr;

        return (it->second);
    }

    auto shop_manager::by_npc_vnum(uint32_t vnum) -> shop * {
        auto it = shops_by_npc_vnum_.find(vnum);

        if (it == shops_by_npc_vnum_.end())
            return nullptr;

        return (it->second);
    }

    auto shop_manager::start_shopping(character *chr, character *chr_shop_keeper, int shop_vnum) -> bool {
        if (chr->shop_owner() == chr_shop_keeper)
            return false;
        if (chr_shop_keeper->is_pc())
            return false;

        if (chr->is_open_safebox() || chr->get_exchange() || chr->my_shop() || chr->is_cube_open()) {
            chr->chat_packet(CHAT_TYPE_INFO, lc_text("다른 거래창이 열린상태에서는 상점거래를 할수 가 없습니다."));
            return false;
        }

        int32_t distance = distance_approx(chr->x() - chr_shop_keeper->x(), chr->y() - chr_shop_keeper->y());

        if (distance >= SHOP_MAX_DISTANCE) {
            metin2::log::debug("Shop: {} is too far from the shop (distance {:d})", chr->name(), distance);
            return false;
        }

        shop *shop_value;

        if (shop_vnum)
            shop_value = get(shop_vnum);
        else
            shop_value = by_npc_vnum(chr_shop_keeper->race_num());

        if (!shop_value) {
            metin2::log::debug("Shop: npc {} has no shop", chr_shop_keeper->name());
            return false;
        }

        bool other_empire = false;

        if (chr->empire() != chr_shop_keeper->empire())
            other_empire = true;

        shop_value->add_guest(chr, chr_shop_keeper->get_vid(), other_empire);
        chr->set_shop_owner(chr_shop_keeper);
        metin2::log::debug("Shop: {} opened a shop", chr->name());
        return true;
    }

    auto shop_manager::find_pc_shop(uint32_t vid) -> shop * {
        auto it = shops_by_pc_.find(vid);

        if (it == shops_by_pc_.end())
            return nullptr;

        return it->second.get();
    }

    auto shop_manager::create_pc_shop(character *ch, shop_item_table *table, uint8_t item_count) -> shop * {
        if (find_pc_shop(ch->get_vid()))
            return nullptr;

        auto new_shop = std::make_unique<shop>();
        shop *shop = new_shop.get();
        shop->set_pc_shop(ch);
        shop->set_shop_items(table, item_count);

        shops_by_pc_.emplace(ch->get_vid(), std::move(new_shop));
        return shop;
    }

    void shop_manager::destroy_pc_shop(character *ch) {
        shop *shop = find_pc_shop(ch->get_vid());

        if (!shop)
            return;

        ch->set_my_shop_time();

        shops_by_pc_.erase(ch->get_vid());
    }

    void shop_manager::stop_shopping(character *ch) {
        shop *shop;

        if (!(shop = ch->get_shop()))
            return;

        ch->set_my_shop_time();

        shop->remove_guest(ch);
        metin2::log::debug("Shop: {} closed the shop", ch->name());
    }

    void shop_manager::buy(character *ch, uint8_t pos) {
        if (!ch->get_shop())
            return;

        if (!ch->shop_owner())
            return;

        if (distance_approx(ch->x() - ch->shop_owner()->x(), ch->y() - ch->shop_owner()->y()) > 2000) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("상점과의 거리가 너무 멀어 물건을 살 수 없습니다."));
            return;
        }

        shop *shop = ch->get_shop();

        ch->set_my_shop_time();

        int ret = shop->buy(ch, pos);

        if (SHOP_SUBHEADER_GC_OK != ret) {
            packet_gc_shop pack;

            pack.header = HEADER_GC_SHOP;
            pack.subheader = ret;
            pack.size = sizeof(packet_gc_shop);

            ch->get_desc()->packet(&pack, sizeof(pack));
        }
    }

    void shop_manager::sell(character *ch, uint8_t cell, uint8_t count_value) {
        if (!ch->get_shop())
            return;

        if (!ch->shop_owner())
            return;

        if (!ch->can_handle_item())
            return;

        if (ch->get_shop()->is_pc_shop())
            return;

        if (distance_approx(ch->x() - ch->shop_owner()->x(), ch->y() - ch->shop_owner()->y()) > 2000) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("상점과의 거리가 너무 멀어 물건을 팔 수 없습니다."));
            return;
        }

        item *item = ch->inventory_item(cell);

        if (!item)
            return;

        if (item->is_equipped() == true) {
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("착용 중인 아이템은 판매할 수 없습니다."));
            return;
        }

        if (true == item->is_locked()) {
            return;
        }

        if (is_set(item->anti_flag(), ITEM_ANTIFLAG_SELL))
            return;

        uint32_t price;

        if (count_value == 0 || count_value > item->count())
            count_value = item->count();

        price = item->shop_buy_price();

        if (is_set(item->flag(), ITEM_FLAG_COUNT_PER_1GOLD)) {
            if (price == 0)
                price = count_value;
            else
                price = count_value / price;
        } else
            price *= count_value;

        price /= 5;

        uint32_t tax = 0;
        int val = 3;

        tax = price * val / 100;
        price -= tax;

        if (metin2::config::test_server())
            metin2::log::info("Shop: pid {:d} ({}) sells item id {:d}", ch->player_id(), ch->name(), item->id());

        const int64_t total_money = static_cast<int64_t>(ch->gold()) + static_cast<int64_t>(price);

        if (GOLD_MAX <= total_money) {
            metin2::log::warn("Player {1} (pid {0:d}) cannot sell: gold {2:d} would exceed the limit", ch->player_id(), ch->name(), ch->gold());
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("20억냥이 초과하여 물품을 팔수 없습니다."));
            return;
        }

        metin2::log::info("Shop: {} sold {} (count {:d}, id {:d}) for {:d}", ch->name(), item->name(), count_value, item->id(), price);

        if (val > 0)
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("판매금액의 %d %% 가 세금으로 나가게됩니다"), val);

        get_db_manager().send_money_log(MONEY_LOG_SHOP, item->vnum(), price);

        if (count_value == item->count()) {

            get_item_manager().remove_item(item, "SELL");
        } else
            item->set_count(item->count() - count_value);

        get_monarch().sendto_db_add_money(tax, ch->empire(), ch);

        ch->point_change(POINT_GOLD, price, false);
    }

    auto compare_shop_item_name(const shop_item_table &lhs, const shop_item_table &rhs) -> bool {
        item_table *item = get_item_manager().table(lhs.vnum);
        item_table *item_value = get_item_manager().table(rhs.vnum);
        if (item && item_value)
            return strcmp(item->locale_name, item_value->locale_name) < 0;
        else
            return true;
    }

    auto convert_to_shop_item_table(group_node *node, shop_table_ex &shop_table) -> bool {
        if (!node->value("vnum", 0, shop_table.vnum)) {
            metin2::log::error("Shop group {} has no vnum", node->node_name());
            return false;
        }

        if (!node->value("name", 0, shop_table.name)) {
            metin2::log::error("Shop group {} has no name", node->node_name());
            return false;
        }

        if (shop_table.name.length() >= SHOP_TAB_NAME_MAX) {
            metin2::log::error("Shop name must be shorter than {:d} characters (group {}, name {})", SHOP_TAB_NAME_MAX, node->node_name(), shop_table.name);
            return false;
        }

        std::string coin_type;
        if (!node->value("cointype", 0, coin_type)) {
            coin_type = "Gold";
        }

        if (strcasecmp(coin_type.c_str(), "Gold") == 0) {
            shop_table.coin_type = SHOP_COIN_TYPE_GOLD;
        } else if (strcasecmp(coin_type.c_str(), "SecondaryCoin") == 0) {
            shop_table.coin_type = SHOP_COIN_TYPE_SECONDARY_COIN;
        } else {
            metin2::log::error("Shop group {} has unknown cointype {}", node->node_name(), coin_type);
            return false;
        }

        group_node *item_group = node->child_node("items");
        if (!item_group) {
            metin2::log::error("Shop group {} has no items group", node->node_name());
            return false;
        }

        int item_group_size = item_group->row_count();
        std::vector<shop_item_table> shop_items(item_group_size);
        if (item_group_size >= SHOP_HOST_ITEM_MAX_NUM) {
            metin2::log::error("Shop group has {:d} items in group {}, must be fewer than {:d}", item_group_size, node->node_name(), SHOP_HOST_ITEM_MAX_NUM);
            return false;
        }

        for (int i = 0; i < item_group_size; i++) {
            if (!item_group->value(i, "vnum", shop_items[i].vnum)) {
                metin2::log::error("Item row {:d} of shop group {} has no vnum", i, node->node_name());
                return false;
            }

            if (!item_group->value(i, "count", shop_items[i].count)) {
                metin2::log::error("Item row {:d} of shop group {} has no count", i, node->node_name());
                return false;
            }
            if (!item_group->value(i, "price", shop_items[i].price)) {
                metin2::log::error("Item row {:d} of shop group {} has no price", i, node->node_name());
                return false;
            }
        }
        std::string sort_value;
        if (!node->value("sort", 0, sort_value)) {
            sort_value = "None";
        }

        if (strcasecmp(sort_value.c_str(), "Asc") == 0) {
            std::ranges::sort(shop_items, compare_shop_item_name);
        } else if (strcasecmp(sort_value.c_str(), "Desc") == 0) {
            std::ranges::sort(std::views::reverse(shop_items), compare_shop_item_name);
        }

        grid grid = common::grid(5, 9);
        int pos;

        std::ranges::fill(shop_table.items, shop_item_table());

        for (size_t i = 0; i < shop_items.size(); i++) {
            item_table *item_table = get_item_manager().table(shop_items[i].vnum);
            if (!item_table) {
                metin2::log::error("Unknown item vnum {:d} in shop group {}", shop_items[i].vnum, node->node_name());
                return false;
            }

            pos = grid.find_blank(1, item_table->size);

            grid.put(pos, 1, item_table->size);
            shop_table.items[pos] = shop_items[i];
        }

        shop_table.item_count = shop_items.size();
        return true;
    }

    auto shop_manager::read_shop_table_ex(const char *file_name) -> bool {
        FILE *fp = fopen(file_name, "rb");
        if (nullptr == fp)
            return true;
        fclose(fp);

        group_text_parse_tree_loader loader;
        if (!loader.load(file_name)) {
            metin2::log::error("Cannot load {}", file_name);
            return false;
        }

        group_node *shop_npc_group = loader.group("shopnpc");
        if (nullptr == shop_npc_group) {
            metin2::log::error("ShopNPC group is missing");
            return false;
        }

        using map_np_cshop = std::multimap<uint32_t, shop_table_ex>;
        map_np_cshop npc_shops;
        for (int i = 0; i < shop_npc_group->row_count(); i++) {
            uint32_t npc_vnum;
            std::string shop_name;
            if (!shop_npc_group->value(i, "npc", npc_vnum) || !shop_npc_group->value(i, "group", shop_name)) {
                metin2::log::error("ShopNPC row {:d} needs 'npc' and 'group' columns", i);
                return false;
            }
            std::ranges::transform(shop_name, shop_name.begin(), (int (*)(int))std::tolower);
            group_node *shop_group = loader.group(shop_name.c_str());
            if (!shop_group) {
                metin2::log::error("Shop group {} does not exist", shop_name);
                return false;
            }
            shop_table_ex table;
            if (!convert_to_shop_item_table(shop_group, table)) {
                metin2::log::error("Cannot read shop group {}", shop_name);
                return false;
            }
            if (shops_by_npc_vnum_.find(npc_vnum) != shops_by_npc_vnum_.end()) {
                metin2::log::error("Npc {:d} cannot have both a regular and an extended shop", npc_vnum);
                return false;
            }

            npc_shops.insert(map_np_cshop::value_type(npc_vnum, table));
        }

        for (auto it = npc_shops.begin(); it != npc_shops.end(); ++it) {
            uint32_t npc_vnum = it->first;
            shop_table_ex &table = it->second;
            if (map_shop_.find(table.vnum) != map_shop_.end()) {
                metin2::log::error("Shop vnum {:d} already exists", table.vnum);
                return false;
            }
            auto shop_it = shops_by_npc_vnum_.find(npc_vnum);

            shop_ex *shop_ex_value = nullptr;
            if (shops_by_npc_vnum_.end() == shop_it) {
                auto new_shop_ex = std::make_unique<shop_ex>();
                shop_ex_value = new_shop_ex.get();
                shops_.push_back(std::move(new_shop_ex));
                shop_ex_value->create(0, npc_vnum);
                shops_by_npc_vnum_.insert(shop_map::value_type(npc_vnum, shop_ex_value));
            } else {
                shop_ex_value = dynamic_cast<shop_ex *>(shop_it->second);
                if (nullptr == shop_ex_value) {
                    metin2::log::error("Shop of npc {:d} is not an extended shop", shop_it->first);
                    return false;
                }
            }

            if (shop_ex_value->tab_count() >= SHOP_TAB_COUNT_MAX) {
                metin2::log::error("Extended shop cannot have more than {:d} tabs", SHOP_TAB_COUNT_MAX);
                return false;
            }

            if (shop_ex_value->vnum() != 0 && map_shop_.find(shop_ex_value->vnum()) != map_shop_.end()) {
                metin2::log::error("Shop vnum {:d} already exists", shop_ex_value->vnum());
                return false;
            }
            map_shop_.insert(shop_map::value_type(shop_ex_value->vnum(), shop_ex_value));
            shop_ex_value->add_shop_table(table);
        }

        return true;
    }
}
