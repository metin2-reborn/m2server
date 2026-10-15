#include <metin2/game/trade/exchange.h>

#include <metin2/game/characters/character.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/items/dragon_soul.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>

#include <metin2/proto/length.h>
#include <metin2/common/grid.h>
#include <metin2/log/log.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace metin2::game::inline trade {

void exchange_packet(character * ch, uint8_t sub_header, bool is_me, uint32_t arg1, item_pos arg2, uint32_t arg3, void * data = nullptr);

void exchange_packet(character * ch, uint8_t sub_header, bool is_me, uint32_t arg1, item_pos arg2, uint32_t arg3, void * data)
{
	if (!ch->get_desc())
		return;

	struct packet_exchange pack_exchg;

	pack_exchg.header 		= HEADER_GC_EXCHANGE;
	pack_exchg.sub_header 	= sub_header;
	pack_exchg.is_me		= is_me;
	pack_exchg.arg1		= arg1;
	pack_exchg.arg2		= arg2;
	pack_exchg.arg3		= arg3;

	if (sub_header == EXCHANGE_SUBHEADER_GC_ITEM_ADD && data)
	{
		std::memcpy(&pack_exchg.sockets, ((item *) data)->sockets(), sizeof(pack_exchg.sockets));
		std::memcpy(&pack_exchg.attr, ((item *) data)->attributes(), sizeof(pack_exchg.attr));
	}
	else
	{
		memset(&pack_exchg.sockets, 0, sizeof(pack_exchg.sockets));
		memset(&pack_exchg.attr, 0, sizeof(pack_exchg.attr));
	}

	ch->get_desc()->packet(&pack_exchg, sizeof(pack_exchg));
}

}

namespace metin2::game::inline characters {

auto character::exchange_start(character * victim) -> bool
{
	if (this == victim)
		return false;

	if (is_observer_mode())
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("관전 상태에서는 교환을 할 수 없습니다."));
		return false;
	}

	if (victim->is_npc())
		return false;

	if ( is_open_safebox() || shop_owner() || my_shop() || is_cube_open())
	{
		chat_packet( CHAT_TYPE_INFO, lc_text("다른 거래창이 열려있을경우 거래를 할수 없습니다." ) );
		return false;
	}

	if ( victim->is_open_safebox() || victim->shop_owner() || victim->my_shop() || victim->is_cube_open() )
	{
		chat_packet( CHAT_TYPE_INFO, lc_text("상대방이 다른 거래중이라 거래를 할수 없습니다." ) );
		return false;
	}
	int iDist = distance_approx(x() - victim->x(), y() - victim->y());

	if (iDist >= EXCHANGE_MAX_DISTANCE)
		return false;

	if (get_exchange())
		return false;

	if (victim->get_exchange())
	{
		exchange_packet(this, EXCHANGE_SUBHEADER_GC_ALREADY, 0, 0, NPOS, 0);
		return false;
	}

	if (victim->is_block_mode(BLOCK_EXCHANGE))
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("상대방이 교환 거부 상태입니다."));
		return false;
	}

	set_exchange(new exchange(this));
	victim->set_exchange(new exchange(victim));

	victim->get_exchange()->set_company(get_exchange());
	get_exchange()->set_company(victim->get_exchange());

	set_exchange_time();
	victim->set_exchange_time();

	exchange_packet(victim, EXCHANGE_SUBHEADER_GC_START, 0, get_vid(), NPOS, 0);
	exchange_packet(this, EXCHANGE_SUBHEADER_GC_START, 0, victim->get_vid(), NPOS, 0);

	return true;
}

}

namespace metin2::game::inline trade {

exchange::exchange(character * owner)
{
	company_ = nullptr;

	accept_ = false;

	for (int i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		ap_items_[i] = nullptr;
		item_pos_[i] = NPOS;
		item_display_pos_[i] = 0;
	}

	gold_ = 0;

	owner_ = owner;
	owner->set_exchange(this);

	grid_ = std::make_unique<grid>(4,3);
}

exchange::~exchange()
{
}

auto exchange::add_item(item_pos item_pos, uint8_t display_pos) -> bool
{
	assert(owner_ != nullptr && company());

	if (!item_pos.is_valid_item_position())
		return false;

	if (item_pos.is_equip_position())
		return false;

	item * item;

	if (!(item = owner_->get_item(item_pos)))
		return false;

	if (is_set(item->anti_flag(), ITEM_ANTIFLAG_GIVE))
	{
		owner_->chat_packet(CHAT_TYPE_INFO, lc_text("아이템을 건네줄 수 없습니다."));
		return false;
	}

	if (true == item->is_locked())
	{
		return false;
	}

	if (item->is_exchanging())
	{
		metin2::log::info( "EXCHANGE under exchanging");
		return false;
	}

	if (!grid_->is_empty(display_pos, 1, item->size()))
	{
		metin2::log::info( "EXCHANGE not empty item_pos {:d} {:d} {:d}", display_pos, 1, item->size());
		return false;
	}

	accept(false);
	company()->accept(false);

	for (int i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		if (ap_items_[i])
			continue;

		ap_items_[i]		= item;
		item_pos_[i]		= item_pos;
		item_display_pos_[i]	= display_pos;
		grid_->put(display_pos, 1, item->size());

		item->set_exchanging(true);

		exchange_packet(owner_, 
				EXCHANGE_SUBHEADER_GC_ITEM_ADD,
				true,
				item->vnum(),
				proto::item_pos(RESERVED_WINDOW, display_pos),
				item->count(),
				item);

		exchange_packet(company()->owner(),
				EXCHANGE_SUBHEADER_GC_ITEM_ADD, 
				false, 
				item->vnum(),
				proto::item_pos(RESERVED_WINDOW, display_pos),
				item->count(),
				item);

		metin2::log::info( "EXCHANGE AddItem success {} pos({:d}, {:d}) {:d}", item->name(), item_pos.window_type, item_pos.cell, display_pos);

		return true;
	}

	return false;
}

auto exchange::remove_item(uint8_t pos) -> bool
{
	if (pos >= EXCHANGE_ITEM_MAX_NUM)
		return false;

	if (!ap_items_[pos])
		return false;

	item_pos PosOfInventory = item_pos_[pos];
	ap_items_[pos]->set_exchanging(false);

	grid_->get(item_display_pos_[pos], 1, ap_items_[pos]->size());

	exchange_packet(owner(),	EXCHANGE_SUBHEADER_GC_ITEM_DEL, true, pos, NPOS, 0);
	exchange_packet(company()->owner(), EXCHANGE_SUBHEADER_GC_ITEM_DEL, false, pos, PosOfInventory, 0);

	accept(false);
	company()->accept(false);

	ap_items_[pos]	    = nullptr;
	item_pos_[pos]	    = NPOS;
	item_display_pos_[pos] = 0;
	return true;
}

auto exchange::add_gold(int32_t gold) -> bool
{
	if (gold <= 0)
		return false;

	if (owner()->gold() < gold)
	{
		exchange_packet(owner(), EXCHANGE_SUBHEADER_GC_LESS_GOLD, 0, 0, NPOS, 0);
		return false;
	}

	if ( gold_ > 0 )
	{
		return false;
	}

	accept(false);
	company()->accept(false);

	gold_ = gold;

	exchange_packet(owner(), EXCHANGE_SUBHEADER_GC_GOLD_ADD, true, gold_, NPOS, 0);
	exchange_packet(company()->owner(), EXCHANGE_SUBHEADER_GC_GOLD_ADD, false, gold_, NPOS, 0);
	return true;
}

auto exchange::check(int * item_count_value) -> bool
{
	if (owner()->gold() < gold_)
		return false;

	int item_count = 0;

	for (int i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		if (!ap_items_[i])
			continue;

		if (!item_pos_[i].is_valid_item_position())
			return false;

		if (ap_items_[i] != owner()->get_item(item_pos_[i]))
			return false;

		++item_count;
	}

	*item_count_value = item_count;
	return true;
}

auto exchange::check_space() -> bool
{
	static grid s_grid1(5, INVENTORY_MAX_NUM/5 / 2);
	static grid s_grid2(5, INVENTORY_MAX_NUM/5 / 2);

	s_grid1.clear();
	s_grid2.clear();

	character *	victim = company()->owner();
	item * item;

	int i;

	for (i = 0; i < INVENTORY_MAX_NUM / 2; ++i)
	{
		if (!(item = victim->inventory_item(i)))
			continue;

		s_grid1.put(i, 1, item->size());
	}
	for (i = INVENTORY_MAX_NUM / 2; i < INVENTORY_MAX_NUM; ++i)
	{
		if (!(item = victim->inventory_item(i)))
			continue;

		s_grid2.put(i - INVENTORY_MAX_NUM / 2, 1, item->size());
	}

	static std::vector <uint16_t> s_vDSGrid(DRAGON_SOUL_INVENTORY_MAX_NUM);
	
	bool bDSInitialized = false;
	
	for (i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		if (!(item = ap_items_[i]))
			continue;

		if (item->is_dragon_soul())
		{
			if (!victim->dragon_soul_is_qualified())
			{
				return false;
			}

			if (!bDSInitialized)
			{
				bDSInitialized = true;
				victim->copy_dragon_soul_item_grid(s_vDSGrid);
			}

			bool bExistEmptySpace = false;
			uint16_t wBasePos = ds_manager::instance().base_position(item);
			if (wBasePos >= DRAGON_SOUL_INVENTORY_MAX_NUM)
				return false;
			
			for (int i = 0; i < DRAGON_SOUL_BOX_SIZE; i++)
			{
				uint16_t wPos = wBasePos + i;
				if (0 == s_vDSGrid[wPos])
				{
					bool bEmpty = true;
					for (int j = 1; j < item->size(); j++)
					{
						if (s_vDSGrid[wPos + j * DRAGON_SOUL_BOX_COLUMN_NUM])
						{
							bEmpty = false;
							break;
						}
					}
					if (bEmpty)
					{
						for (int j = 0; j < item->size(); j++)
						{
							s_vDSGrid[wPos + j * DRAGON_SOUL_BOX_COLUMN_NUM] =  wPos + 1;
						}
						bExistEmptySpace = true;
						break;
					}
				}
				if (bExistEmptySpace)
					break;
			}
			if (!bExistEmptySpace)
				return false;
		}
		else
		{
			int iPos = s_grid1.find_blank(1, item->size());

			if (iPos >= 0)
			{
				s_grid1.put(iPos, 1, item->size());
			}
			else
			{
				iPos = s_grid2.find_blank(1, item->size());

				if (iPos >= 0)
				{
					s_grid2.put(iPos, 1, item->size());
				}
				else
				{
					return false;
				}
			}
		}
	}

	return true;
}

auto exchange::done() -> bool
{
	int		empty_pos, i;
	item *	item;

	character *	victim = company()->owner();

	for (i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		if (!(item = ap_items_[i]))
			continue;

		if (item->is_dragon_soul())
			empty_pos = victim->empty_dragon_soul_inventory(item);
		else
			empty_pos = victim->empty_inventory(item->size());

		if (empty_pos < 0)
		{
			metin2::log::error("Exchange::Done : Cannot find blank position in inventory {} <-> {} item {}", 
					owner_->name(), victim->name(), item->name());
			continue;
		}

		assert(empty_pos >= 0);

		owner_->sync_quickslot(QUICKSLOT_TYPE_ITEM, item->cell(), 255);

		item->remove_from_character();
		if (item->is_dragon_soul())
			item->add_to_character(victim, item_pos(DRAGON_SOUL_INVENTORY, empty_pos));
		else
			item->add_to_character(victim, item_pos(INVENTORY, empty_pos));
		item_manager::instance().flush_delayed_save(item);

		item->set_exchanging(false);
		{
			char exchange_buf[51];

			snprintf(exchange_buf, sizeof(exchange_buf), "%s %u %u", item->name(), owner()->player_id(), item->count());
			log_manager::instance().item_log(victim, item, "EXCHANGE_TAKE", exchange_buf);

			snprintf(exchange_buf, sizeof(exchange_buf), "%s %u %u", item->name(), victim->player_id(), item->count());
			log_manager::instance().item_log(owner(), item, "EXCHANGE_GIVE", exchange_buf);

			if (item->vnum() >= 80003 && item->vnum() <= 80007)
			{
				log_manager::instance().gold_bar_log(victim->player_id(), item->id(), EXCHANGE_TAKE, "");
				log_manager::instance().gold_bar_log(owner()->player_id(), item->id(), EXCHANGE_GIVE, "");
			}
		}

		ap_items_[i] = nullptr;
	}

	if (gold_)
	{
		owner()->point_change(POINT_GOLD, -gold_, true);
		victim->point_change(POINT_GOLD, gold_, true);

		if (gold_ > 1000)
		{
			char exchange_buf[51];
			snprintf(exchange_buf, sizeof(exchange_buf), "%u %s", owner()->player_id(), owner()->name());
			log_manager::instance().char_log(victim, gold_, "EXCHANGE_GOLD_TAKE", exchange_buf);

			snprintf(exchange_buf, sizeof(exchange_buf), "%u %s", victim->player_id(), victim->name());
			log_manager::instance().char_log(owner(), gold_, "EXCHANGE_GOLD_GIVE", exchange_buf);
		}
	}

	grid_->clear();
	return true;
}

auto exchange::accept(bool accept) -> bool
{
	if (accept_ == accept)
		return true;

	accept_ = accept;

	if (accept_ && company()->accept_)
	{
		int	iItemCount;

		character * victim = company()->owner();

		owner()->set_exchange_time();
		victim->set_exchange_time();		

		if (!check(&iItemCount))
		{
			owner()->chat_packet(CHAT_TYPE_INFO, lc_text("돈이 부족하거나 아이템이 제자리에 없습니다."));
			victim->chat_packet(CHAT_TYPE_INFO, lc_text("상대방의 돈이 부족하거나 아이템이 제자리에 없습니다."));
			goto EXCHANGE_END;
		}

		if (!check_space())
		{
			owner()->chat_packet(CHAT_TYPE_INFO, lc_text("상대방의 소지품에 빈 공간이 없습니다."));
			victim->chat_packet(CHAT_TYPE_INFO, lc_text("소지품에 빈 공간이 없습니다."));
			goto EXCHANGE_END;
		}

		if (!company()->check(&iItemCount))
		{
			victim->chat_packet(CHAT_TYPE_INFO, lc_text("돈이 부족하거나 아이템이 제자리에 없습니다."));
			owner()->chat_packet(CHAT_TYPE_INFO, lc_text("상대방의 돈이 부족하거나 아이템이 제자리에 없습니다."));
			goto EXCHANGE_END;
		}

		if (!company()->check_space())
		{
			victim->chat_packet(CHAT_TYPE_INFO, lc_text("상대방의 소지품에 빈 공간이 없습니다."));
			owner()->chat_packet(CHAT_TYPE_INFO, lc_text("소지품에 빈 공간이 없습니다."));
			goto EXCHANGE_END;
		}

		if (!db_clientdesc->is_connected())
		{
			metin2::log::error("Cannot use exchange feature while DB cache connection is dead.");
			victim->chat_packet(CHAT_TYPE_INFO, "Unknown error");
			owner()->chat_packet(CHAT_TYPE_INFO, "Unknown error");
			goto EXCHANGE_END;
		}

		if (done())
		{
			if (gold_)
				owner()->save();

			if (company()->done())
			{
				if (company()->gold_)
					victim->save();

				owner()->chat_packet(CHAT_TYPE_INFO, lc_text("%s 님과의 교환이 성사 되었습니다."), victim->name());
				victim->chat_packet(CHAT_TYPE_INFO, lc_text("%s 님과의 교환이 성사 되었습니다."), owner()->name());
			}
		}

EXCHANGE_END:
		cancel();
		return false;
	}
	else
	{
		exchange_packet(owner(), EXCHANGE_SUBHEADER_GC_ACCEPT, true, accept_, NPOS, 0);
		exchange_packet(company()->owner(), EXCHANGE_SUBHEADER_GC_ACCEPT, false, accept_, NPOS, 0);
		return true;
	}
}

void exchange::cancel()
{
	exchange_packet(owner(), EXCHANGE_SUBHEADER_GC_END, 0, 0, NPOS, 0);
	owner()->set_exchange(nullptr);

	for (int i = 0; i < EXCHANGE_ITEM_MAX_NUM; ++i)
	{
		if (ap_items_[i])
			ap_items_[i]->set_exchanging(false);
	}

	if (company())
	{
		company()->set_company(nullptr);
		company()->cancel();
	}

	delete this;
}
}
