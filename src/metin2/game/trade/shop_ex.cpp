#include <metin2/game/trade/shop_ex.h>

#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/trade/shop.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/log/log.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace metin2::game::inline trade {

bool shop_ex::create(uint32_t vnum, uint32_t npc_vnum)
{
	vnum_ = vnum;
	return true;
}

bool shop_ex::add_shop_table(shop_table_ex& shop_table)
{
	for (auto it = vec_shop_tabs_.begin(); it != vec_shop_tabs_.end(); it++)
	{
		const shop_table_ex& _shopTable = *it;
		if (0 != _shopTable.vnum && _shopTable.vnum == shop_table.vnum)
			return false;
		if (0 != _shopTable.npc_vnum && _shopTable.npc_vnum == shop_table.vnum)
			return false;
	}
	vec_shop_tabs_.push_back(shop_table);
	return true;
}

bool shop_ex::add_guest(character * ch,uint32_t owner_vid, bool other_empire)
{
	if (!ch)
		return false;

	if (ch->get_exchange())
		return false;

	if (ch->get_shop())
		return false;

	ch->set_shop(this);

	map_guest_.insert(guest_map_type::value_type(ch, other_empire));

	packet_gc_shop pack;

	pack.header		= HEADER_GC_SHOP;
	pack.subheader	= SHOP_SUBHEADER_GC_START_EX;

	packet_gc_shop_start_ex pack2;

	memset(&pack2, 0, sizeof(pack2));
	
	pack2.owner_vid = owner_vid;
	pack2.shop_tab_count = vec_shop_tabs_.size();
	char temp[8096];
	char* buf = &temp[0];
	size_t size = 0;
	for (auto it = vec_shop_tabs_.begin(); it != vec_shop_tabs_.end(); it++)
	{
		const shop_table_ex& shop_tab = *it;
		packet_gc_shop_start_ex::sub_packet_shop_tab pack_tab;
		pack_tab.coin_type = shop_tab.coin_type;
		memcpy(pack_tab.name, shop_tab.name.c_str(), SHOP_TAB_NAME_MAX);
		
		for (uint8_t i = 0; i < SHOP_HOST_ITEM_MAX_NUM; i++)
		{
			pack_tab.items[i].vnum = shop_tab.items[i].vnum;
			pack_tab.items[i].count = shop_tab.items[i].count;
			switch(shop_tab.coin_type)
			{
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

int shop_ex::buy(character * ch, uint8_t pos)
{
	uint8_t tabIdx = pos / SHOP_HOST_ITEM_MAX_NUM;
	uint8_t slotPos = pos % SHOP_HOST_ITEM_MAX_NUM;
	if (tabIdx >= tab_count())
	{
		metin2::log::info( "ShopEx::Buy : invalid position {:d} : {}", pos, ch->name());
		return SHOP_SUBHEADER_GC_INVALID_POS;
	}

	metin2::log::info( "ShopEx::Buy : name {} pos {:d}", ch->name(), pos);

	guest_map_type::iterator it = map_guest_.find(ch);

	if (it == map_guest_.end())
		return SHOP_SUBHEADER_GC_END;

	shop_table_ex& shopTab = vec_shop_tabs_[tabIdx];
	shop_item_table& r_item = shopTab.items[slotPos];

	if (r_item.price <= 0)
	{
		log_manager::instance().hack_log("SHOP_BUY_GOLD_OVERFLOW", ch);
		return SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY;
	}

	uint32_t dwPrice = r_item.price;

	switch (shopTab.coin_type)
	{
	case SHOP_COIN_TYPE_GOLD:
		if (it->second)
			dwPrice *= 3;

		if (ch->gold() < (int) dwPrice)
		{
			metin2::log::debug( "ShopEx::Buy : Not enough money : {} has {:d}, price {:d}", ch->name(), ch->gold(), dwPrice);
			return SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY;
		}
		break;
	case SHOP_COIN_TYPE_SECONDARY_COIN:
		{
			int count = ch->count_specify_type_item(ITEM_SECONDARY_COIN);
			if (static_cast<uint32_t>(count) < dwPrice)
			{
				metin2::log::debug( "ShopEx::Buy : Not enough myeongdojun : {} has {:d}, price {:d}", ch->name(), count, dwPrice);
				return SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY_EX;
			}
		}
		break;
	}
	
	item * item;

	item = item_manager::instance().create_item(r_item.vnum, r_item.count);

	if (!item)
		return SHOP_SUBHEADER_GC_SOLD_OUT;

	int iEmptyPos;
	if (item->is_dragon_soul())
	{
		iEmptyPos = ch->empty_dragon_soul_inventory(item);
	}
	else
	{
		iEmptyPos = ch->empty_inventory(item->size());
	}

	if (iEmptyPos < 0)
	{
		metin2::log::debug( "ShopEx::Buy : Inventory full : {} size {:d}", ch->name(), item->size());
		item_manager::instance().destroy_item(item);
		return SHOP_SUBHEADER_GC_INVENTORY_FULL;
	}

	switch (shopTab.coin_type)
	{
	case SHOP_COIN_TYPE_GOLD:
		ch->point_change(POINT_GOLD, -dwPrice, false);
		break;
	case SHOP_COIN_TYPE_SECONDARY_COIN:
		ch->remove_specify_type_item(ITEM_SECONDARY_COIN, dwPrice);
		break;
	}


	if (item->is_dragon_soul())
		item->add_to_character(ch, item_pos(DRAGON_SOUL_INVENTORY, iEmptyPos));
	else
		item->add_to_character(ch, item_pos(INVENTORY, iEmptyPos));

	item_manager::instance().flush_delayed_save(item);
	log_manager::instance().item_log(ch, item, "BUY", item->name());

	if (item->vnum() >= 80003 && item->vnum() <= 80007)
	{
		log_manager::instance().gold_bar_log(ch->player_id(), item->id(), PERSONAL_SHOP_BUY, "");
	}

	db_manager::instance().send_money_log(MONEY_LOG_SHOP, item->vnum(), -dwPrice);

	if (item)
		metin2::log::info( "ShopEx: BUY: name {} {}(x {:d}):{:d} price {:d}", ch->name(), item->name(), item->count(), item->id(), dwPrice);

	ch->save();

    return (SHOP_SUBHEADER_GC_OK);
}
}
