#include <metin2/game/trade/shop_manager.h>

#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/data/group_text_parse_tree.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/empire/monarch.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/trade/shop.h>
#include <metin2/game/trade/shop_ex.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/common/grid.h>
#include <metin2/log/log.h>

#include <algorithm>
#include <iterator>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <strings.h>

namespace metin2::game::inline trade {

shop_manager::shop_manager()
{
}

shop_manager::~shop_manager()
{
	destroy();
}

bool shop_manager::initialize(shop_table * table, int size)
{
	if (!map_pk_shop_.empty())
		return false;

	int i; 

	for (i = 0; i < size; ++i, ++table)
	{
		auto pkNewShop = std::make_unique<shop>();

		if (!pkNewShop->create(table->vnum, table->npc_vnum, table->items))
			continue;

		shop * shop = pkNewShop.release();
		map_pk_shop_.insert(shop_map::value_type(table->vnum, shop));
		map_pk_shop_by_npc_vnum_.insert(shop_map::value_type(table->npc_vnum, shop));
	}
	char szShopTableExFileName[256];

	snprintf(szShopTableExFileName, sizeof(szShopTableExFileName),
		"%s/shop_table_ex.txt", locale_service_get_base_path().c_str());

	return read_shop_table_ex(szShopTableExFileName);
}

void shop_manager::destroy()
{
	shop_map::iterator it = map_pk_shop_.begin();

	while (it != map_pk_shop_.end())
	{
		delete it->second;
		++it;
	}

	map_pk_shop_.clear();
}

shop * shop_manager::get(uint32_t dwVnum)
{
	shop_map::const_iterator it = map_pk_shop_.find(dwVnum);

	if (it == map_pk_shop_.end())
		return nullptr;

	return (it->second);
}

shop * shop_manager::by_npc_vnum(uint32_t dwVnum)
{
	shop_map::const_iterator it = map_pk_shop_by_npc_vnum_.find(dwVnum);

	if (it == map_pk_shop_by_npc_vnum_.end())
		return nullptr;

	return (it->second);
}

/*
 * Interface functions
 */

// Start shop trading
bool shop_manager::start_shopping(character * pkChr, character * pkChrShopKeeper, int iShopVnum)
{
	if (pkChr->shop_owner() == pkChrShopKeeper)
		return false;
	// this method is only for NPC
	if (pkChrShopKeeper->is_pc())
		return false;

	//PREVENT_TRADE_WINDOW
	if (pkChr->is_open_safebox() || pkChr->get_exchange() || pkChr->my_shop() || pkChr->is_cube_open())
	{
		pkChr->chat_packet(CHAT_TYPE_INFO, lc_text("다른 거래창이 열린상태에서는 상점거래를 할수 가 없습니다."));
		return false;
	}
	//END_PREVENT_TRADE_WINDOW

	int32_t distance = distance_approx(pkChr->x() - pkChrShopKeeper->x(), pkChr->y() - pkChrShopKeeper->y());

	if (distance >= SHOP_MAX_DISTANCE)
	{
		metin2::log::debug( "SHOP: TOO_FAR: {} distance {:d}", pkChr->name(), distance);
		return false;
	}

	shop * pkShop;

	if (iShopVnum)
		pkShop = get(iShopVnum);
	else
		pkShop = by_npc_vnum(pkChrShopKeeper->race_num());

	if (!pkShop)
	{
		metin2::log::debug( "SHOP: NO SHOP");
		return false;
	}

	bool bOtherEmpire = false;

	if (pkChr->empire() != pkChrShopKeeper->empire())
		bOtherEmpire = true;

	pkShop->add_guest(pkChr, pkChrShopKeeper->get_vid(), bOtherEmpire);
	pkChr->set_shop_owner(pkChrShopKeeper);
	metin2::log::info( "SHOP: START: {}", pkChr->name());
	return true;
}

shop * shop_manager::find_pc_shop(uint32_t dwVID)
{
	shop_map::iterator it = map_pk_shop_by_pc_.find(dwVID);

	if (it == map_pk_shop_by_pc_.end())
		return nullptr;

	return it->second;
}

shop * shop_manager::create_pc_shop(character * ch, shop_item_table * pTable, uint8_t bItemCount)
{
	if (find_pc_shop(ch->get_vid()))
		return nullptr;

	shop * pkShop = new shop;
	pkShop->set_pc_shop(ch);
	pkShop->set_shop_items(pTable, bItemCount);

	map_pk_shop_by_pc_.insert(shop_map::value_type(ch->get_vid(), pkShop));
	return pkShop;
}

void shop_manager::destroy_pc_shop(character * ch)
{
	shop * pkShop = find_pc_shop(ch->get_vid());

	if (!pkShop)
		return;

	//PREVENT_ITEM_COPY;
	ch->set_my_shop_time();
	//END_PREVENT_ITEM_COPY
	
	map_pk_shop_by_pc_.erase(ch->get_vid());
	delete pkShop;
}

// End shop trading
void shop_manager::stop_shopping(character * ch)
{
	shop * shop;

	if (!(shop = ch->get_shop()))
		return;

	//PREVENT_ITEM_COPY;
	ch->set_my_shop_time();
	//END_PREVENT_ITEM_COPY
	
	shop->remove_guest(ch);
	metin2::log::info( "SHOP: END: {}", ch->name());
}

// Buy item
void shop_manager::buy(character * ch, uint8_t pos)
{
	if (!ch->get_shop())
		return;

	if (!ch->shop_owner())
		return;

	if (distance_approx(ch->x() - ch->shop_owner()->x(), ch->y() - ch->shop_owner()->y()) > 2000)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("상점과의 거리가 너무 멀어 물건을 살 수 없습니다."));
		return;
	}

	shop* pkShop = ch->get_shop();

	//PREVENT_ITEM_COPY
	ch->set_my_shop_time();
	//END_PREVENT_ITEM_COPY

	int ret = pkShop->buy(ch, pos);

	if (SHOP_SUBHEADER_GC_OK != ret) // Send if there was a problem.
	{
		packet_gc_shop pack;

		pack.header	= HEADER_GC_SHOP;
		pack.subheader	= ret;
		pack.size	= sizeof(packet_gc_shop);

		ch->get_desc()->packet(&pack, sizeof(pack));
	}
}

void shop_manager::sell(character * ch, uint8_t bCell, uint8_t bCount)
{
	if (!ch->get_shop())
		return;

	if (!ch->shop_owner())
		return;

	if (!ch->can_handle_item())
		return;

	if (ch->get_shop()->is_pc_shop())
		return;

	if (distance_approx(ch->x()-ch->shop_owner()->x(), ch->y()-ch->shop_owner()->y())>2000)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("상점과의 거리가 너무 멀어 물건을 팔 수 없습니다."));
		return;
	}
	
	item * item = ch->inventory_item(bCell);

	if (!item)
		return;

	if (item->is_equipped() == true)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("착용 중인 아이템은 판매할 수 없습니다."));
		return;
	}

	if (true == item->is_locked())
	{
		return;
	}

	if (is_set(item->anti_flag(), ITEM_ANTIFLAG_SELL))
		return;

	uint32_t dwPrice;

	if (bCount == 0 || bCount > item->count())
		bCount = item->count();

	dwPrice = item->shop_buy_price();

	if (is_set(item->flag(), ITEM_FLAG_COUNT_PER_1GOLD))
	{
		if (dwPrice == 0)
			dwPrice = bCount;
		else
			dwPrice = bCount / dwPrice;
	}
	else
		dwPrice *= bCount;

	dwPrice /= 5;
	
	//Tax calculation
	uint32_t dwTax = 0;
	int iVal = 3;
	
	dwTax = dwPrice * iVal/100;
	dwPrice -= dwTax;

	if (metin2::config::test_server())
		metin2::log::info( "Sell Item price id {:d} {} itemid {:d}", ch->player_id(), ch->name(), item->id());

	const int64_t nTotalMoney = static_cast<int64_t>(ch->gold()) + static_cast<int64_t>(dwPrice);

	if (GOLD_MAX <= nTotalMoney)
	{
		metin2::log::error("[OVERFLOW_GOLD] id {:d} name {} gold {:d}", ch->player_id(), ch->name(), ch->gold());
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("20억냥이 초과하여 물품을 팔수 없습니다."));
		return;
	}

	// 20050802.myevan.Added item ID to the shop sale log
	metin2::log::info( "SHOP: SELL: {} item name: {}(x{:d}):{:d} price: {:d}", ch->name(), item->name(), bCount, item->id(), dwPrice);

	if (iVal > 0)
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("판매금액의 %d %% 가 세금으로 나가게됩니다"), iVal);

	db_manager::instance().send_money_log(MONEY_LOG_SHOP, item->vnum(), dwPrice);

	if (bCount == item->count())
	{
		// In Korea there are many troublesome users who drop items and ask for them to be restored, so
		// log attributes on shop sales.

		item_manager::instance().remove_item(item, "SELL");
	}
	else
		item->set_count(item->count() - bCount);

	//Monarch system: tax collection
	monarch::instance().sendto_db_add_money(dwTax, ch->empire(), ch);

	ch->point_change(POINT_GOLD, dwPrice, false);
}

bool compare_shop_item_name(const shop_item_table& lhs, const shop_item_table& rhs)
{
	item_table* lItem = item_manager::instance().table(lhs.vnum);
	item_table* rItem = item_manager::instance().table(rhs.vnum);
	if (lItem && rItem)
		return strcmp(lItem->locale_name, rItem->locale_name) < 0;
	else
		return true;
}

bool convert_to_shop_item_table(group_node* pNode, shop_table_ex& shopTable)
{
	if (!pNode->value("vnum", 0, shopTable.vnum))
	{
		metin2::log::error("Group {} does not have vnum.", pNode->node_name().c_str());
		return false;
	}

	if (!pNode->value("name", 0, shopTable.name))
	{
		metin2::log::error("Group {} does not have name.", pNode->node_name().c_str());
		return false;
	}
	
	if (shopTable.name.length() >= SHOP_TAB_NAME_MAX)
	{
		metin2::log::error("Shop name length must be less than {:d}. Error in Group {}, name {}", SHOP_TAB_NAME_MAX, pNode->node_name().c_str(), shopTable.name.c_str());
		return false;
	}

	std::string stCoinType;
	if (!pNode->value("cointype", 0, stCoinType))
	{
		stCoinType = "Gold";
	}
	
	if (strcasecmp(stCoinType.c_str(), "Gold") == 0)
	{
		shopTable.coin_type = SHOP_COIN_TYPE_GOLD;
	}
	else if (strcasecmp(stCoinType.c_str(), "SecondaryCoin") == 0)
	{
		shopTable.coin_type = SHOP_COIN_TYPE_SECONDARY_COIN;
	}
	else
	{
		metin2::log::error("Group {} has undefine cointype({}).", pNode->node_name().c_str(), stCoinType.c_str());
		return false;
	}

	group_node* pItemGroup = pNode->child_node("items");
	if (!pItemGroup)
	{
		metin2::log::error("Group {} does not have 'group items'.", pNode->node_name().c_str());
		return false;
	}

	int itemGroupSize = pItemGroup->row_count();
	std::vector <shop_item_table> shopItems(itemGroupSize);
	if (itemGroupSize >= SHOP_HOST_ITEM_MAX_NUM)
	{
		metin2::log::error("count({:d}) of rows of group items of group {} must be smaller than {:d}", itemGroupSize, pNode->node_name().c_str(), SHOP_HOST_ITEM_MAX_NUM);
		return false;
	}

	for (int i = 0; i < itemGroupSize; i++)
	{
		if (!pItemGroup->value(i, "vnum", shopItems[i].vnum))
		{
			metin2::log::error("row({:d}) of group items of group {} does not have vnum column", i, pNode->node_name().c_str());
			return false;
		}
		
		if (!pItemGroup->value(i, "count", shopItems[i].count))
		{
			metin2::log::error("row({:d}) of group items of group {} does not have count column", i, pNode->node_name().c_str());
			return false;
		}
		if (!pItemGroup->value(i, "price", shopItems[i].price))
		{
			metin2::log::error("row({:d}) of group items of group {} does not have price column", i, pNode->node_name().c_str());
			return false;
		}
	}
	std::string stSort;
	if (!pNode->value("sort", 0, stSort))
	{
		stSort = "None";
	}

	if (strcasecmp(stSort.c_str(), "Asc") == 0)
	{
		std::sort(shopItems.begin(), shopItems.end(), compare_shop_item_name);
	}
	else if(strcasecmp(stSort.c_str(), "Desc") == 0)
	{
		std::sort(shopItems.rbegin(), shopItems.rend(), compare_shop_item_name);
	}

	grid grid = common::grid(5, 9);
	int iPos;

	std::fill(std::begin(shopTable.items), std::end(shopTable.items), shop_item_table());

	for (size_t i = 0; i < shopItems.size(); i++)
	{
		item_table * item_table = item_manager::instance().table(shopItems[i].vnum);
		if (!item_table)
		{
			metin2::log::error("vnum({:d}) of group items of group {} does not exist", shopItems[i].vnum, pNode->node_name().c_str());
			return false;
		}

		iPos = grid.find_blank(1, item_table->size);

		grid.put(iPos, 1, item_table->size);
		shopTable.items[iPos] = shopItems[i];
	}

	shopTable.item_count = shopItems.size();
	return true;
}

bool shop_manager::read_shop_table_ex(const char* stFileName)
{
	// Check whether the file exists.
	// Absence is not treated as an error.
	FILE* fp = fopen(stFileName, "rb");
	if (nullptr == fp)
		return true;
	fclose(fp);

	group_text_parse_tree_loader loader;
	if (!loader.load(stFileName))
	{
		metin2::log::error("{} Load fail.", stFileName);
		return false;
	}

	group_node* pShopNPCGroup = loader.group("shopnpc");
	if (nullptr == pShopNPCGroup)
	{
		metin2::log::error("Group ShopNPC is not exist.");
		return false;
	}

	typedef std::multimap <uint32_t, shop_table_ex> map_np_cshop;
	map_np_cshop map_npcShop;
	for (int i = 0; i < pShopNPCGroup->row_count(); i++)
	{
		uint32_t npcVnum;
		std::string shopName;
		if (!pShopNPCGroup->value(i, "npc", npcVnum) || !pShopNPCGroup->value(i, "group", shopName))
		{
			metin2::log::error("Invalid row({:d}). Group ShopNPC rows must have 'npc', 'group' columns", i);
			return false;
		}
		std::transform(shopName.begin(), shopName.end(), shopName.begin(), (int(*)(int))std::tolower);
		group_node* pShopGroup = loader.group(shopName.c_str());
		if (!pShopGroup)
		{
			metin2::log::error("Group {} is not exist.", shopName.c_str());
			return false;
		}
		shop_table_ex table;
		if (!convert_to_shop_item_table(pShopGroup, table))
		{
			metin2::log::error("Cannot read Group {}.", shopName.c_str());
			return false;
		}
		if (map_pk_shop_by_npc_vnum_.find(npcVnum) != map_pk_shop_by_npc_vnum_.end())
		{
			metin2::log::error("{:d} cannot have both original shop and extended shop", npcVnum);
			return false;
		}
		
		map_npcShop.insert(map_np_cshop::value_type(npcVnum, table));	
	}

	for (map_np_cshop::iterator it = map_npcShop.begin(); it != map_npcShop.end(); ++it)
	{
		uint32_t npcVnum = it->first;
		shop_table_ex& table = it->second;
		if (map_pk_shop_.find(table.vnum) != map_pk_shop_.end())
		{
			metin2::log::error("Shop vnum({:d}) already exists", table.vnum);
			return false;
		}
		shop_map::iterator shop_it = map_pk_shop_by_npc_vnum_.find(npcVnum);
		
		shop_ex * pkShopEx = nullptr;
		if (map_pk_shop_by_npc_vnum_.end() == shop_it)
		{
			pkShopEx = new shop_ex;
			pkShopEx->create(0, npcVnum);
			map_pk_shop_by_npc_vnum_.insert(shop_map::value_type(npcVnum, pkShopEx));
		}
		else
		{
			pkShopEx = dynamic_cast <shop_ex*> (shop_it->second);
			if (nullptr == pkShopEx)
			{
				metin2::log::error("WTF!!! It can't be happend. NPC({:d}) Shop is not extended version.", shop_it->first);
				return false;
			}
		}

		if (pkShopEx->tab_count() >= SHOP_TAB_COUNT_MAX)
		{
			metin2::log::error("ShopEx cannot have tab more than {:d}", SHOP_TAB_COUNT_MAX);
			return false;
		}

		if (pkShopEx->vnum() != 0 && map_pk_shop_.find(pkShopEx->vnum()) != map_pk_shop_.end())
		{
			metin2::log::error("Shop vnum({:d}) already exist.", pkShopEx->vnum());
			return false;
		}
		map_pk_shop_.insert(shop_map::value_type (pkShopEx->vnum(), pkShopEx));
		pkShopEx->add_shop_table(table);
	}

	return true;
}
}
