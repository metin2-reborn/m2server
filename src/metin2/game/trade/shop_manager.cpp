#include <metin2/config/config.h>
#include <metin2/game/trade/shop_manager.h>

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
#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <strings.h>

namespace metin2::game::inline trade {

    shop_manager::shop_manager() {
    }

    shop_manager::~shop_manager() {
        destroy();
    }

    auto shop_manager::initialize(shop_table *table, int size) -> bool {
        if (!map_shop_.empty())
            return false;

        int i;

        for (i = 0; i < size; ++i, ++table) {
            auto pkNewShop = std::make_unique<shop>();

            if (!pkNewShop->create(table->vnum, table->npc_vnum, table->items))
                continue;

            shop *shop = pkNewShop.release();
            map_shop_.insert(shop_map::value_type(table->vnum, shop));
            map_shop_by_npc_vnum_.insert(shop_map::value_type(table->npc_vnum, shop));
        }
        char szShopTableExFileName[256];

        snprintf(szShopTableExFileName, sizeof(szShopTableExFileName),
            "%s/shop_table_ex.txt", locale_service_get_base_path().c_str());

        return read_shop_table_ex(szShopTableExFileName);
    }

    void shop_manager::destroy() {
        shop_map::iterator it = map_shop_.begin();

        while (it != map_shop_.end()) {
            delete it->second;
            ++it;
        }

        map_shop_.clear();
    }

    auto shop_manager::get(uint32_t vnum) -> shop * {
        shop_map::const_iterator it = map_shop_.find(vnum);

        if (it == map_shop_.end())
            return nullptr;

        return (it->second);
    }

    auto shop_manager::by_npc_vnum(uint32_t vnum) -> shop * {
        shop_map::const_iterator it = map_shop_by_npc_vnum_.find(vnum);

        if (it == map_shop_by_npc_vnum_.end())
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

        shop *pkShop;

        if (shop_vnum)
            pkShop = get(shop_vnum);
        else
            pkShop = by_npc_vnum(chr_shop_keeper->race_num());

        if (!pkShop) {
            metin2::log::debug("Shop: npc {} has no shop", chr_shop_keeper->name());
            return false;
        }

        bool bOtherEmpire = false;

        if (chr->empire() != chr_shop_keeper->empire())
            bOtherEmpire = true;

        pkShop->add_guest(chr, chr_shop_keeper->get_vid(), bOtherEmpire);
        chr->set_shop_owner(chr_shop_keeper);
        metin2::log::debug("Shop: {} opened a shop", chr->name());
        return true;
    }

    auto shop_manager::find_pc_shop(uint32_t vid) -> shop * {
        shop_map::iterator it = map_shop_by_pc_.find(vid);

        if (it == map_shop_by_pc_.end())
            return nullptr;

        return it->second;
    }

    auto shop_manager::create_pc_shop(character *ch, shop_item_table *table, uint8_t item_count) -> shop * {
        if (find_pc_shop(ch->get_vid()))
            return nullptr;

        shop *pkShop = new shop;
        pkShop->set_pc_shop(ch);
        pkShop->set_shop_items(table, item_count);

        map_shop_by_pc_.insert(shop_map::value_type(ch->get_vid(), pkShop));
        return pkShop;
    }

    void shop_manager::destroy_pc_shop(character *ch) {
        shop *pkShop = find_pc_shop(ch->get_vid());

        if (!pkShop)
            return;

        ch->set_my_shop_time();

        map_shop_by_pc_.erase(ch->get_vid());
        delete pkShop;
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

        shop *pkShop = ch->get_shop();

        ch->set_my_shop_time();

        int ret = pkShop->buy(ch, pos);

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

        uint32_t dwPrice;

        if (count_value == 0 || count_value > item->count())
            count_value = item->count();

        dwPrice = item->shop_buy_price();

        if (is_set(item->flag(), ITEM_FLAG_COUNT_PER_1GOLD)) {
            if (dwPrice == 0)
                dwPrice = count_value;
            else
                dwPrice = count_value / dwPrice;
        } else
            dwPrice *= count_value;

        dwPrice /= 5;

        uint32_t dwTax = 0;
        int iVal = 3;

        dwTax = dwPrice * iVal / 100;
        dwPrice -= dwTax;

        if (metin2::config::test_server())
            metin2::log::info("Shop: pid {:d} ({}) sells item id {:d}", ch->player_id(), ch->name(), item->id());

        const int64_t nTotalMoney = static_cast<int64_t>(ch->gold()) + static_cast<int64_t>(dwPrice);

        if (GOLD_MAX <= nTotalMoney) {
            metin2::log::warn("Player {1} (pid {0:d}) cannot sell: gold {2:d} would exceed the limit", ch->player_id(), ch->name(), ch->gold());
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("20억냥이 초과하여 물품을 팔수 없습니다."));
            return;
        }

        metin2::log::info("Shop: {} sold {} (count {:d}, id {:d}) for {:d}", ch->name(), item->name(), count_value, item->id(), dwPrice);

        if (iVal > 0)
            ch->chat_packet(CHAT_TYPE_INFO, lc_text("판매금액의 %d %% 가 세금으로 나가게됩니다"), iVal);

        db_manager::instance().send_money_log(MONEY_LOG_SHOP, item->vnum(), dwPrice);

        if (count_value == item->count()) {

            item_manager::instance().remove_item(item, "SELL");
        } else
            item->set_count(item->count() - count_value);

        monarch::instance().sendto_db_add_money(dwTax, ch->empire(), ch);

        ch->point_change(POINT_GOLD, dwPrice, false);
    }

    auto compare_shop_item_name(const shop_item_table &lhs, const shop_item_table &rhs) -> bool {
        item_table *lItem = item_manager::instance().table(lhs.vnum);
        item_table *rItem = item_manager::instance().table(rhs.vnum);
        if (lItem && rItem)
            return strcmp(lItem->locale_name, rItem->locale_name) < 0;
        else
            return true;
    }

    auto convert_to_shop_item_table(group_node *node, shop_table_ex &shop_table) -> bool {
        if (!node->value("vnum", 0, shop_table.vnum)) {
            metin2::log::error("Shop group {} has no vnum", node->node_name().c_str());
            return false;
        }

        if (!node->value("name", 0, shop_table.name)) {
            metin2::log::error("Shop group {} has no name", node->node_name().c_str());
            return false;
        }

        if (shop_table.name.length() >= SHOP_TAB_NAME_MAX) {
            metin2::log::error("Shop name must be shorter than {:d} characters (group {}, name {})", SHOP_TAB_NAME_MAX, node->node_name().c_str(), shop_table.name.c_str());
            return false;
        }

        std::string stCoinType;
        if (!node->value("cointype", 0, stCoinType)) {
            stCoinType = "Gold";
        }

        if (strcasecmp(stCoinType.c_str(), "Gold") == 0) {
            shop_table.coin_type = SHOP_COIN_TYPE_GOLD;
        } else if (strcasecmp(stCoinType.c_str(), "SecondaryCoin") == 0) {
            shop_table.coin_type = SHOP_COIN_TYPE_SECONDARY_COIN;
        } else {
            metin2::log::error("Shop group {} has unknown cointype {}", node->node_name().c_str(), stCoinType.c_str());
            return false;
        }

        group_node *pItemGroup = node->child_node("items");
        if (!pItemGroup) {
            metin2::log::error("Shop group {} has no items group", node->node_name().c_str());
            return false;
        }

        int itemGroupSize = pItemGroup->row_count();
        std::vector<shop_item_table> shopItems(itemGroupSize);
        if (itemGroupSize >= SHOP_HOST_ITEM_MAX_NUM) {
            metin2::log::error("Shop group has {:d} items in group {}, must be fewer than {:d}", itemGroupSize, node->node_name().c_str(), SHOP_HOST_ITEM_MAX_NUM);
            return false;
        }

        for (int i = 0; i < itemGroupSize; i++) {
            if (!pItemGroup->value(i, "vnum", shopItems[i].vnum)) {
                metin2::log::error("Item row {:d} of shop group {} has no vnum", i, node->node_name().c_str());
                return false;
            }

            if (!pItemGroup->value(i, "count", shopItems[i].count)) {
                metin2::log::error("Item row {:d} of shop group {} has no count", i, node->node_name().c_str());
                return false;
            }
            if (!pItemGroup->value(i, "price", shopItems[i].price)) {
                metin2::log::error("Item row {:d} of shop group {} has no price", i, node->node_name().c_str());
                return false;
            }
        }
        std::string stSort;
        if (!node->value("sort", 0, stSort)) {
            stSort = "None";
        }

        if (strcasecmp(stSort.c_str(), "Asc") == 0) {
            std::sort(shopItems.begin(), shopItems.end(), compare_shop_item_name);
        } else if (strcasecmp(stSort.c_str(), "Desc") == 0) {
            std::sort(shopItems.rbegin(), shopItems.rend(), compare_shop_item_name);
        }

        grid grid = common::grid(5, 9);
        int iPos;

        std::fill(std::begin(shop_table.items), std::end(shop_table.items), shop_item_table());

        for (size_t i = 0; i < shopItems.size(); i++) {
            item_table *item_table = item_manager::instance().table(shopItems[i].vnum);
            if (!item_table) {
                metin2::log::error("Unknown item vnum {:d} in shop group {}", shopItems[i].vnum, node->node_name().c_str());
                return false;
            }

            iPos = grid.find_blank(1, item_table->size);

            grid.put(iPos, 1, item_table->size);
            shop_table.items[iPos] = shopItems[i];
        }

        shop_table.item_count = shopItems.size();
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

        group_node *pShopNPCGroup = loader.group("shopnpc");
        if (nullptr == pShopNPCGroup) {
            metin2::log::error("ShopNPC group is missing");
            return false;
        }

        typedef std::multimap<uint32_t, shop_table_ex> map_np_cshop;
        map_np_cshop map_npcShop;
        for (int i = 0; i < pShopNPCGroup->row_count(); i++) {
            uint32_t npcVnum;
            std::string shopName;
            if (!pShopNPCGroup->value(i, "npc", npcVnum) || !pShopNPCGroup->value(i, "group", shopName)) {
                metin2::log::error("ShopNPC row {:d} needs 'npc' and 'group' columns", i);
                return false;
            }
            std::transform(shopName.begin(), shopName.end(), shopName.begin(), (int (*)(int))std::tolower);
            group_node *pShopGroup = loader.group(shopName.c_str());
            if (!pShopGroup) {
                metin2::log::error("Shop group {} does not exist", shopName.c_str());
                return false;
            }
            shop_table_ex table;
            if (!convert_to_shop_item_table(pShopGroup, table)) {
                metin2::log::error("Cannot read shop group {}", shopName.c_str());
                return false;
            }
            if (map_shop_by_npc_vnum_.find(npcVnum) != map_shop_by_npc_vnum_.end()) {
                metin2::log::error("Npc {:d} cannot have both a regular and an extended shop", npcVnum);
                return false;
            }

            map_npcShop.insert(map_np_cshop::value_type(npcVnum, table));
        }

        for (map_np_cshop::iterator it = map_npcShop.begin(); it != map_npcShop.end(); ++it) {
            uint32_t npcVnum = it->first;
            shop_table_ex &table = it->second;
            if (map_shop_.find(table.vnum) != map_shop_.end()) {
                metin2::log::error("Shop vnum {:d} already exists", table.vnum);
                return false;
            }
            shop_map::iterator shop_it = map_shop_by_npc_vnum_.find(npcVnum);

            shop_ex *pkShopEx = nullptr;
            if (map_shop_by_npc_vnum_.end() == shop_it) {
                pkShopEx = new shop_ex;
                pkShopEx->create(0, npcVnum);
                map_shop_by_npc_vnum_.insert(shop_map::value_type(npcVnum, pkShopEx));
            } else {
                pkShopEx = dynamic_cast<shop_ex *>(shop_it->second);
                if (nullptr == pkShopEx) {
                    metin2::log::error("Shop of npc {:d} is not an extended shop", shop_it->first);
                    return false;
                }
            }

            if (pkShopEx->tab_count() >= SHOP_TAB_COUNT_MAX) {
                metin2::log::error("Extended shop cannot have more than {:d} tabs", SHOP_TAB_COUNT_MAX);
                return false;
            }

            if (pkShopEx->vnum() != 0 && map_shop_.find(pkShopEx->vnum()) != map_shop_.end()) {
                metin2::log::error("Shop vnum {:d} already exists", pkShopEx->vnum());
                return false;
            }
            map_shop_.insert(shop_map::value_type(pkShopEx->vnum(), pkShopEx));
            pkShopEx->add_shop_table(table);
        }

        return true;
    }
}
