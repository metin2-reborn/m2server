
#include <metin2/config/config.h>
#include <metin2/game/characters/affect.h>
#include <metin2/game/events/arena.h>
#include <metin2/game/events/battle_arena.h>
#include <metin2/game/items/belt_inventory_helper.h>
#include <metin2/game/items/blend_item.h>
#include <metin2/game/characters/buff_on_attributes.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/empire/castle.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/items/dragon_soul.h>
#include <metin2/game/world/dungeon.h>
#include <metin2/game/world/entity.h>
#include <metin2/game/app/event.h>
#include <metin2/game/events/fishing.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/persistence/log.h>
#include <metin2/game/social/marriage.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/empire/monarch.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/characters/polymorph.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/quest/questpc.h>
#include <metin2/game/items/refine.h>
#include <metin2/game/trade/safebox.h>
#include <metin2/game/world/sectree.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/trade/shop.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/empire/start_position.h>
#include <metin2/game/empire/threeway_war.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/world/vector.h>
#include <metin2/game/guilds/war_map.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/proto/vnum_helper.h>
#include <metin2/log/log.h>
#include <metin2/core/main.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iterator>
#include <map>
#include <memory>
#include <stack>
#include <string>
#include <utility>
#include <vector>

namespace metin2::game::inline characters {

const int ITEM_BROKEN_METIN_VNUM = 28960;

const uint32_t character::default_change_item_attr_cycle_ = 10;
const char character::last_change_item_attr_flag_[] = "Item.LastChangeItemAttr";
const char character::change_item_attr_cycle_flag_[] = "change_itemattr_cycle";
constexpr uint8_t buff_on_attr_points[] = { POINT_ENERGY, POINT_COSTUME_ATTR_BONUS };

struct f_find_stone
{
	std::map<uint32_t, character *> stone;

	void operator()(entity * ent)
	{
		if (ent->is_type(ENTITY_CHARACTER) == true)
		{
			character * pChar = (character *)ent;

			if (pChar->is_stone() == true)
			{
				stone[(uint32_t)pChar->get_vid()] = pChar;
			}
		}
	}
};

static auto is_summon_item(int vnum) -> bool
{
	switch (vnum)
	{
		case 22000:
		case 22010:
		case 22011:
		case 22020:
		case ITEM_MARRIAGE_RING:
			return true;
	}

	return false;
}

static auto is_monkey_dungeon(int map_index) -> bool
{
	switch (map_index)
	{
		case 5:
		case 25:
		case 45:
		case 108:
		case 109:
			return true;;
	}

	return false;
}

auto is_summonable_zone(int map_index) -> bool
{
	if (is_monkey_dungeon(map_index))
		return false;
	if (is_castle_map(map_index))
		return false;

	switch (map_index)
	{
		case 66 :
		case 71 :
		case 72 :
		case 73 :
		case 193 :
		case 216 :
		case 217 :
		case 208 :
			return false;
	}

	if (battle_arena::is_battle_arena_map(map_index)) return false;

	if (map_index > 10000) return false;

	return true;
}

static auto fn_check_item_socket(item * item) -> bool
{
	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
	{
		if (item->get_socket(i) != item->proto()->sockets[i])
			return false;
	}

	return true;
}

static void fn_copy_item_socket(item * dest, item * src)
{
	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
	{
		dest->set_socket(i, src->get_socket(i));
	}
}
static auto fn_check_item_sex(character * ch, item * item) -> bool
{
	if (is_set(item->anti_flag(), ITEM_ANTIFLAG_MALE))
	{
		if (SEX_MALE==get_sex(ch))
			return false;
	}
	if (is_set(item->anti_flag(), ITEM_ANTIFLAG_FEMALE)) 
	{
		if (SEX_FEMALE==get_sex(ch))
			return false;
	}

	return true;
}

auto character::can_handle_item(bool skip_check_refine, bool skip_observer) -> bool
{
	if (!skip_observer)
		if (is_observer_)
			return false;

	if (my_shop())
		return false;

	if (!skip_check_refine)
		if (under_refine_)
			return false;

	if (is_cube_open() || nullptr != dragon_soul_refine_window_get_opener())
		return false;

	if (is_warping())
		return false;

	return true;
}

auto character::inventory_item(uint16_t cell) const -> item *
{
	return get_item(item_pos(INVENTORY, cell));
}
auto character::get_item(item_pos cell_value) const -> item *
{
	if (!is_valid_item_position(cell_value))
		return nullptr;
	uint16_t wCell = cell_value.cell;
	uint8_t window_type = cell_value.window_type;
	switch (window_type)
	{
	case INVENTORY:
	case EQUIPMENT:
		if (wCell >= INVENTORY_AND_EQUIP_SLOT_MAX)
		{
			metin2::log::error("Invalid inventory cell {:d}", wCell);
			return nullptr;
		}
		return points_instant_.items[wCell];
	case DRAGON_SOUL_INVENTORY:
		if (wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
		{
			metin2::log::error("Invalid dragon soul inventory cell {:d}", wCell);
			return nullptr;
		}
		return points_instant_.ds_items[wCell];

	default:
		return nullptr;
	}
	return nullptr;
}

void character::set_item(item_pos cell_value, item * item_value)
{
	uint16_t wCell = cell_value.cell;
	uint8_t window_type = cell_value.window_type;
	if (reinterpret_cast<uintptr_t>((item*)item_value) == 0xff || reinterpret_cast<uintptr_t>((item*)item_value) == 0xffffffff || reinterpret_cast<uintptr_t>((item*)item_value) == UINTPTR_MAX)
	{
		metin2::log::error("Invalid item pointer 0xff for {} at cell {:d}", name(), wCell);
		core_dump();
		return;
	}

	if (item_value && item_value->owner())
	{
		assert(!"GetOwner exist");
		return;
	}
	switch(window_type)
	{
	case INVENTORY:
	case EQUIPMENT:
		{
			if (wCell >= INVENTORY_AND_EQUIP_SLOT_MAX)
			{
				metin2::log::error("Invalid inventory cell {:d}", wCell);
				return;
			}

			item * pOld = points_instant_.items[wCell];

			if (pOld)
			{
				if (wCell < INVENTORY_MAX_NUM)
				{
					for (int i = 0; i < pOld->size(); ++i)
					{
						int p = wCell + (i * 5);

						if (p >= INVENTORY_MAX_NUM)
							continue;

						if (points_instant_.items[p] && points_instant_.items[p] != pOld)
							continue;

						points_instant_.item_grid[p] = 0;
					}
				}
				else
					points_instant_.item_grid[wCell] = 0;
			}

			if (item_value)
			{
				if (wCell < INVENTORY_MAX_NUM)
				{
					for (int i = 0; i < item_value->size(); ++i)
					{
						int p = wCell + (i * 5);

						if (p >= INVENTORY_MAX_NUM)
							continue;

						points_instant_.item_grid[p] = wCell + 1;
					}
				}
				else
					points_instant_.item_grid[wCell] = wCell + 1;
			}

			points_instant_.items[wCell] = item_value;
		}
		break;
	case DRAGON_SOUL_INVENTORY:
		{
			if (wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
			{
				metin2::log::error("Invalid dragon soul inventory cell {:d}", wCell);
				return;
			}

			item * pOld = points_instant_.ds_items[wCell];

			if (pOld)
			{
				if (wCell < DRAGON_SOUL_INVENTORY_MAX_NUM)
				{
					for (int i = 0; i < pOld->size(); ++i)
					{
						int p = wCell + (i * DRAGON_SOUL_BOX_COLUMN_NUM);

						if (p >= DRAGON_SOUL_INVENTORY_MAX_NUM)
							continue;

						if (points_instant_.ds_items[p] && points_instant_.ds_items[p] != pOld)
							continue;

						points_instant_.ds_item_grid[p] = 0;
					}
				}
				else
					points_instant_.ds_item_grid[wCell] = 0;
			}

			if (item_value)
			{
				if (wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
				{
					metin2::log::error("Invalid dragon soul inventory cell {:d}", wCell);
					return;
				}

				if (wCell < DRAGON_SOUL_INVENTORY_MAX_NUM)
				{
					for (int i = 0; i < item_value->size(); ++i)
					{
						int p = wCell + (i * DRAGON_SOUL_BOX_COLUMN_NUM);

						if (p >= DRAGON_SOUL_INVENTORY_MAX_NUM)
							continue;

						points_instant_.ds_item_grid[p] = wCell + 1;
					}
				}
				else
					points_instant_.ds_item_grid[wCell] = wCell + 1;
			}

			points_instant_.ds_items[wCell] = item_value;
		}
		break;
	default:
		metin2::log::error("Invalid window type {:d}", window_type);
		return;
	}

	if (get_desc())
	{
		if (item_value)
		{
			packet_gc_item_set pack;
			pack.header = HEADER_GC_ITEM_SET;
			pack.cell = cell_value;

			pack.count = item_value->count();
			pack.vnum = item_value->vnum();
			pack.flags = item_value->flag();
			pack.anti_flags	= item_value->anti_flag();
			pack.highlight = (cell_value.window_type == DRAGON_SOUL_INVENTORY);

			std::memcpy(pack.sockets, item_value->sockets(), sizeof(pack.sockets));
			std::memcpy(pack.attr, item_value->attributes(), sizeof(pack.attr));

			get_desc()->packet(&pack, sizeof(packet_gc_item_set));
		}
		else
		{
			packet_gc_item_del_deprecated pack;
			pack.header = HEADER_GC_ITEM_DEL;
			pack.cell = cell_value;
			pack.count = 0;
			pack.vnum = 0;
			memset(pack.sockets, 0, sizeof(pack.sockets));
			memset(pack.attr, 0, sizeof(pack.attr));

			get_desc()->packet(&pack, sizeof(packet_gc_item_del_deprecated));
		}
	}

	if (item_value)
	{
		item_value->set_cell(this, wCell);
		switch (window_type)
		{
		case INVENTORY:
		case EQUIPMENT:
			if ((wCell < INVENTORY_MAX_NUM) || (BELT_INVENTORY_SLOT_START <= wCell && BELT_INVENTORY_SLOT_END > wCell))
				item_value->set_window(INVENTORY);
			else
				item_value->set_window(EQUIPMENT);
			break;
		case DRAGON_SOUL_INVENTORY:
			item_value->set_window(DRAGON_SOUL_INVENTORY);
			break;
		}
	}
}

auto character::wear(uint8_t cell) const -> item *
{
	if (cell >= WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * static_cast<int>(DS_SLOT_MAX))
	{
		metin2::log::error("Invalid wear cell {:d}", cell);
		return nullptr;
	}

	return points_instant_.items[INVENTORY_MAX_NUM + cell];
}

void character::set_wear(uint8_t cell, item * item)
{
	if (cell >= WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * static_cast<int>(DS_SLOT_MAX))
	{
		metin2::log::error("Invalid wear cell {:d}", cell);
		return;
	}

	set_item(item_pos (INVENTORY, INVENTORY_MAX_NUM + cell), item);

	if (!item && cell == WEAR_WEAPON)
	{
		if (is_affect_flag(AFF_GWIGUM))
			remove_affect(SKILL_GWIGEOM);

		if (is_affect_flag(AFF_GEOMGYEONG))
			remove_affect(SKILL_GEOMKYUNG);
	}
}

void character::clear_item()
{
	int		i;
	item *	item;

	for (i = 0; i < INVENTORY_AND_EQUIP_SLOT_MAX; ++i)
	{
		if ((item = inventory_item(i)))
		{
			item->set_skip_save(true);
			item_manager::instance().flush_delayed_save(item);

			item->remove_from_character();
			item_manager::instance().destroy_item(item);

			sync_quickslot(QUICKSLOT_TYPE_ITEM, i, 255);
		}
	}
	for (i = 0; i < DRAGON_SOUL_INVENTORY_MAX_NUM; ++i)
	{
		if ((item = get_item(item_pos(DRAGON_SOUL_INVENTORY, i))))
		{
			item->set_skip_save(true);
			item_manager::instance().flush_delayed_save(item);

			item->remove_from_character();
			item_manager::instance().destroy_item(item);
		}
	}
}

auto character::is_empty_item_grid(item_pos cell_value, uint8_t size, int exception_cell) const -> bool
{
	switch (cell_value.window_type)
	{
	case INVENTORY:
		{
			if (cell_value.cell >= INVENTORY_AND_EQUIP_SLOT_MAX)
				return false;

			uint8_t bCell = cell_value.cell;

			++exception_cell;

			if (cell_value.is_belt_inventory_position())
			{
				item * beltItem = wear(WEAR_BELT);

				if (nullptr == beltItem)
					return false;

				if (false == belt_inventory_helper::is_available_cell(bCell - BELT_INVENTORY_SLOT_START, beltItem->value(0)))
					return false;

				if (points_instant_.item_grid[bCell])
				{
					if (points_instant_.item_grid[bCell] == exception_cell)
						return true;

					return false;
				}

				if (size == 1)
					return true;

			}
			else if (bCell >= INVENTORY_MAX_NUM)
				return false;

			if (points_instant_.item_grid[bCell])
			{
				if (points_instant_.item_grid[bCell] == exception_cell)
				{
					if (size == 1)
						return true;

					int j = 1;
					uint8_t bPage = bCell / (INVENTORY_MAX_NUM / 2);

					do
					{
						uint8_t p = bCell + (5 * j);

						if (p >= INVENTORY_MAX_NUM)
							return false;

						if (p / (INVENTORY_MAX_NUM / 2) != bPage)
							return false;

						if (points_instant_.item_grid[p])
							if (points_instant_.item_grid[p] != exception_cell)
								return false;
					}
					while (++j < size);

					return true;
				}
				else
					return false;
			}

			if (1 == size)
				return true;
			else
			{
				int j = 1;
				uint8_t bPage = bCell / (INVENTORY_MAX_NUM / 2);

				do
				{
					uint8_t p = bCell + (5 * j);

					if (p >= INVENTORY_MAX_NUM)
						return false;

					if (p / (INVENTORY_MAX_NUM / 2) != bPage)
						return false;

					if (points_instant_.item_grid[p])
						if (points_instant_.item_grid[p] != exception_cell)
							return false;
				}
				while (++j < size);

				return true;
			}
		}
		break;
	case DRAGON_SOUL_INVENTORY:
		{
			uint16_t wCell = cell_value.cell;
			if (wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
				return false;

			exception_cell++;

			if (points_instant_.ds_item_grid[wCell])
			{
				if (points_instant_.ds_item_grid[wCell] == exception_cell)
				{
					if (size == 1)
						return true;

					int j = 1;

					do
					{
						const uint16_t p = wCell + (DRAGON_SOUL_BOX_COLUMN_NUM * j);

						if (p >= DRAGON_SOUL_INVENTORY_MAX_NUM)
							return false;

						if (points_instant_.ds_item_grid[p])
							if (points_instant_.ds_item_grid[p] != exception_cell)
								return false;
					}
					while (++j < size);

					return true;
				}
				else
					return false;
			}

			if (1 == size)
				return true;
			else
			{
				int j = 1;

				do
				{
					const uint16_t p = wCell + (DRAGON_SOUL_BOX_COLUMN_NUM * j);

					if (p >= DRAGON_SOUL_INVENTORY_MAX_NUM)
						return false;

					if (points_instant_.ds_item_grid[p])
						if (points_instant_.ds_item_grid[p] != exception_cell)
							return false;
				}
				while (++j < size);

				return true;
			}
		}
	}

	return false;
}

auto character::empty_inventory(uint8_t size) const -> int
{
	// The belt inventory is excluded: only the basic inventory takes new items.
	for ( int i = 0; i < INVENTORY_MAX_NUM; ++i)
		if (is_empty_item_grid(item_pos (INVENTORY, i), size))
			return i;
	return -1;
}

auto character::empty_dragon_soul_inventory(item * item_value) const -> int
{
	if (nullptr == item_value || !item_value->is_dragon_soul())
		return -1;
	if (!dragon_soul_is_qualified())
	{
		return -1;
	}
	uint8_t bSize = item_value->size();
	uint16_t wBaseCell = ds_manager::instance().base_position(item_value);

	if (WORD_MAX == wBaseCell)
		return -1;

	for (int i = 0; i < DRAGON_SOUL_BOX_SIZE; ++i)
		if (is_empty_item_grid(item_pos(DRAGON_SOUL_INVENTORY, i + wBaseCell), bSize))
			return i + wBaseCell;

	return -1;
}

void character::copy_dragon_soul_item_grid(std::vector<uint16_t>& dragon_soul_item_grid) const
{
	dragon_soul_item_grid.resize(DRAGON_SOUL_INVENTORY_MAX_NUM);

	std::copy(points_instant_.ds_item_grid, points_instant_.ds_item_grid + DRAGON_SOUL_INVENTORY_MAX_NUM, dragon_soul_item_grid.begin());
}

auto character::count_empty_inventory() const -> int
{
	int	count = 0;

	for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
		if (inventory_item(i))
			count += inventory_item(i)->size();

	return (INVENTORY_MAX_NUM - count);
}

void notify_refine_success(character * ch, item * item, const char* way)
{
	if (nullptr != ch && item != nullptr)
	{
		ch->chat_packet(CHAT_TYPE_COMMAND, "RefineSuceeded");

		log_manager::instance().refine_log(ch->player_id(), item->name(), item->id(), item->refine_level(), 1, way);
	}
}

void notify_refine_fail(character * ch, item * item, const char* way, int success = 0)
{
	if (nullptr != ch && nullptr != item)
	{
		ch->chat_packet(CHAT_TYPE_COMMAND, "RefineFailed");

		log_manager::instance().refine_log(ch->player_id(), item->name(), item->id(), item->refine_level(), success, way);
	}
}

void character::set_refine_npc(character * ch)
{
	if ( ch != nullptr )
	{
		refine_npcvid_ = ch->get_vid();
	}
	else
	{
		refine_npcvid_ = 0;
	}
}

auto character::do_refine(item * item, bool money_only) -> bool
{
	if (!can_handle_item(true))
	{
		clear_refine_mode();
		return false;
	}

	if (quest::quest_manager::instance().event_flag("update_refine_time") != 0)
	{
		if (get_global_time() < quest::quest_manager::instance().event_flag("update_refine_time") + (60 * 5))
		{
			metin2::log::info("{} (pid {:d}) cannot refine: refining is paused after an update", name(), player_id());
			return false;
		}
	}

	const refine_table * prt = refine_manager::instance().refine_recipe(item->refine_set());

	if (!prt)
		return false;

	uint32_t result_vnum = item->refined_vnum();

	int cost = compute_refine_fee(prt->cost);

	int RefineChance = quest_flag("main_quest_lv7.refine_chance");

	if (RefineChance > 0)
	{
		if (!item->check_item_use_level(20) || item->type() != ITEM_WEAPON)
		{
			chat_packet(CHAT_TYPE_INFO, lc_text("무료 개량 기회는 20 이하의 무기만 가능합니다"));
			return false;
		}

		cost = 0;
		set_quest_flag("main_quest_lv7.refine_chance", RefineChance - 1);
	}

	if (result_vnum == 0)
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("더 이상 개량할 수 없습니다."));
		return false;
	}

	if (item->type() == ITEM_USE && item->sub_type() == USE_TUNING)
		return false;

	item_table * pProto = item_manager::instance().table(item->refined_vnum());

	if (!pProto)
	{
		metin2::log::error("No proto for refined item vnum {:d}", item->refined_vnum());
		chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템은 개량할 수 없습니다."));
		return false;
	}


	if (gold() < cost)
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("개량을 하기 위한 돈이 부족합니다."));
		return false;
	}

	if (!money_only && !RefineChance)
	{
		for (int i = 0; i < prt->material_count; ++i)
		{
			if (count_specify_item(prt->materials[i].vnum) < prt->materials[i].count)
			{
				if (metin2::config::test_server())
				{
					chat_packet(CHAT_TYPE_INFO, "Find %d, count %d, require %d", prt->materials[i].vnum, count_specify_item(prt->materials[i].vnum), prt->materials[i].count);
				}
				chat_packet(CHAT_TYPE_INFO, lc_text("개량을 하기 위한 재료가 부족합니다."));
				return false;
			}
		}

		for (int i = 0; i < prt->material_count; ++i)
			remove_specify_item(prt->materials[i].vnum, prt->materials[i].count);
	}

	int prob = number(1, 100);

	if (is_refine_through_guild() || money_only)
		prob -= 10;


	if (prob <= prt->prob)
	{
		game::items::item * pkNewItem = item_manager::instance().create_item(result_vnum, 1, 0, false);

		if (pkNewItem)
		{
			item_manager::copy_all_attr_to(item, pkNewItem);
			log_manager::instance().item_log(this, pkNewItem, "REFINE SUCCESS", pkNewItem->name());

			uint8_t bCell = item->cell();

			notify_refine_success(this, item, is_refine_through_guild() ? "GUILD" : "POWER");
			db_manager::instance().send_money_log(MONEY_LOG_REFINE, item->vnum(), -cost);
			item_manager::instance().remove_item(item, "REMOVE (REFINE SUCCESS)");

			pkNewItem->add_to_character(this, item_pos(INVENTORY, bCell)); 
			item_manager::instance().flush_delayed_save(pkNewItem);

			metin2::log::debug("Refine succeeded, fee {:d}", cost);
			pkNewItem->attr_log();
			pay_refine_fee(cost);
		}
		else
		{
			metin2::log::error("Cannot create refined item {:d}", result_vnum);
			notify_refine_fail(this, item, is_refine_through_guild() ? "GUILD" : "POWER");
		}
	}
	else
	{
		db_manager::instance().send_money_log(MONEY_LOG_REFINE, item->vnum(), -cost);
		notify_refine_fail(this, item, is_refine_through_guild() ? "GUILD" : "POWER");
		item->attr_log();
		item_manager::instance().remove_item(item, "REMOVE (REFINE FAIL)");

		pay_refine_fee(cost);
	}

	return true;
}

enum enum_refine_scrolls
{
	HYUNIRON_CHN   = 1,
	YONGSIN_SCROLL = 2,
	MUSIN_SCROLL   = 3,
	YAGONG_SCROLL  = 4,
	MEMO_SCROLL	   = 5,
	BDRAGON_SCROLL	= 6,
};

auto character::do_refine_with_scroll(item * item) -> bool
{
	if (!can_handle_item(true))
	{
		clear_refine_mode();
		return false;
	}

	clear_refine_mode();

	if (quest::quest_manager::instance().event_flag("update_refine_time") != 0)
	{
		if (get_global_time() < quest::quest_manager::instance().event_flag("update_refine_time") + (60 * 5))
		{
			metin2::log::info("{} (pid {:d}) cannot refine: refining is paused after an update", name(), player_id());
			return false;
		}
	}

	const refine_table * prt = refine_manager::instance().refine_recipe(item->refine_set());

	if (!prt)
		return false;

	game::items::item * pkItemScroll;

	if (refine_additional_cell_ < 0)
		return false;

	pkItemScroll = inventory_item(refine_additional_cell_);

	if (!pkItemScroll)
		return false;

	if (!(pkItemScroll->type() == ITEM_USE && pkItemScroll->sub_type() == USE_TUNING))
		return false;

	if (pkItemScroll->vnum() == item->vnum())
		return false;

	uint32_t result_vnum = item->refined_vnum();
	uint32_t result_fail_vnum = item->refine_from_vnum();

	if (result_vnum == 0)
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("더 이상 개량할 수 없습니다."));
		return false;
	}

	if (pkItemScroll->value(0) == MUSIN_SCROLL)
	{
		if (item->refine_level() >= 4)
		{
			chat_packet(CHAT_TYPE_INFO, lc_text("이 개량서로 더 이상 개량할 수 없습니다."));
			return false;
		}
	}

	else if (pkItemScroll->value(0) == MEMO_SCROLL)
	{
		if (item->refine_level() != pkItemScroll->value(1))
		{
			chat_packet(CHAT_TYPE_INFO, lc_text("이 개량서로 개량할 수 없습니다."));
			return false;
		}
	}
	else if (pkItemScroll->value(0) == BDRAGON_SCROLL)
	{
		if (item->type() != ITEM_METIN || item->refine_level() != 4)
		{
			chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템으로 개량할 수 없습니다."));
			return false;
		}
	}

	item_table * pProto = item_manager::instance().table(item->refined_vnum());

	if (!pProto)
	{
		metin2::log::error("No proto for refined item vnum {:d}", item->refined_vnum());
		chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템은 개량할 수 없습니다."));
		return false;
	}


	if (gold() < prt->cost)
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("개량을 하기 위한 돈이 부족합니다."));
		return false;
	}

	for (int i = 0; i < prt->material_count; ++i)
	{
		if (count_specify_item(prt->materials[i].vnum) < prt->materials[i].count)
		{
			if (metin2::config::test_server())
			{
				chat_packet(CHAT_TYPE_INFO, "Find %d, count %d, require %d", prt->materials[i].vnum, count_specify_item(prt->materials[i].vnum), prt->materials[i].count);
			}
			chat_packet(CHAT_TYPE_INFO, lc_text("개량을 하기 위한 재료가 부족합니다."));
			return false;
		}
	}

	for (int i = 0; i < prt->material_count; ++i)
		remove_specify_item(prt->materials[i].vnum, prt->materials[i].count);

	int prob = number(1, 100);
	int success_prob = prt->prob;
	bool bDestroyWhenFail = false;

	const char* szRefineType = "SCROLL";

	if (pkItemScroll->value(0) == HYUNIRON_CHN || 
		pkItemScroll->value(0) == YONGSIN_SCROLL || 
		pkItemScroll->value(0) == YAGONG_SCROLL)
	{
		const char hyuniron_prob[9] = { 100, 75, 65, 55, 45, 40, 35, 25, 20 };
		const char yagong_prob[9] = { 100, 100, 90, 80, 70, 60, 50, 30, 20 };

		if (pkItemScroll->value(0) == YONGSIN_SCROLL)
		{
			success_prob = hyuniron_prob[std::clamp(item->refine_level(), 0, 8)];
		}
		else if (pkItemScroll->value(0) == YAGONG_SCROLL)
		{
			success_prob = yagong_prob[std::clamp(item->refine_level(), 0, 8)];
		}
		else
		{
			metin2::log::error("Unknown refine scroll type {:d}", pkItemScroll->value(0));
		}

		if (metin2::config::test_server()) 
		{
			chat_packet(CHAT_TYPE_INFO, "[Only Test] Success_Prob %d, RefineLevel %d ", success_prob, item->refine_level());
		}
		if (pkItemScroll->value(0) == HYUNIRON_CHN)
			bDestroyWhenFail = true;

		if (pkItemScroll->value(0) == HYUNIRON_CHN)
		{
			szRefineType = "HYUNIRON";
		}
		else if (pkItemScroll->value(0) == YONGSIN_SCROLL)
		{
			szRefineType = "GOD_SCROLL";
		}
		else if (pkItemScroll->value(0) == YAGONG_SCROLL)
		{
			szRefineType = "YAGONG_SCROLL";
		}
	}

	if (pkItemScroll->value(0) == MUSIN_SCROLL)
	{
		success_prob = 100;

		szRefineType = "MUSIN_SCROLL";
	}
	else if (pkItemScroll->value(0) == MEMO_SCROLL)
	{
		success_prob = 100;
		szRefineType = "MEMO_SCROLL";
	}
	else if (pkItemScroll->value(0) == BDRAGON_SCROLL)
	{
		success_prob = 80;
		szRefineType = "BDRAGON_SCROLL";
	}

	pkItemScroll->set_count(pkItemScroll->count() - 1);

	if (prob <= success_prob)
	{
		game::items::item * pkNewItem = item_manager::instance().create_item(result_vnum, 1, 0, false);

		if (pkNewItem)
		{
			item_manager::copy_all_attr_to(item, pkNewItem);
			log_manager::instance().item_log(this, pkNewItem, "REFINE SUCCESS", pkNewItem->name());

			uint8_t bCell = item->cell();

			notify_refine_success(this, item, szRefineType);
			db_manager::instance().send_money_log(MONEY_LOG_REFINE, item->vnum(), -prt->cost);
			item_manager::instance().remove_item(item, "REMOVE (REFINE SUCCESS)");

			pkNewItem->add_to_character(this, item_pos(INVENTORY, bCell)); 
			item_manager::instance().flush_delayed_save(pkNewItem);
			pkNewItem->attr_log();
			pay_refine_fee(prt->cost);
		}
		else
		{
			metin2::log::error("Cannot create refined item {:d}", result_vnum);
			notify_refine_fail(this, item, szRefineType);
		}
	}
	else if (!bDestroyWhenFail && result_fail_vnum)
	{
		game::items::item * pkNewItem = item_manager::instance().create_item(result_fail_vnum, 1, 0, false);

		if (pkNewItem)
		{
			item_manager::copy_all_attr_to(item, pkNewItem);
			log_manager::instance().item_log(this, pkNewItem, "REFINE FAIL", pkNewItem->name());

			uint8_t bCell = item->cell();

			db_manager::instance().send_money_log(MONEY_LOG_REFINE, item->vnum(), -prt->cost);
			notify_refine_fail(this, item, szRefineType, -1);
			item_manager::instance().remove_item(item, "REMOVE (REFINE FAIL)");

			pkNewItem->add_to_character(this, item_pos(INVENTORY, bCell)); 
			item_manager::instance().flush_delayed_save(pkNewItem);

			pkNewItem->attr_log();

			pay_refine_fee(prt->cost);
		}
		else
		{
			metin2::log::error("Cannot create refined item {:d}", result_fail_vnum);
			notify_refine_fail(this, item, szRefineType);
		}
	}
	else
	{
		notify_refine_fail(this, item, szRefineType);

		pay_refine_fee(prt->cost);
	}

	return true;
}

auto character::refine_information(uint8_t cell, uint8_t type_value, int additional_cell) -> bool
{
	if (cell > INVENTORY_MAX_NUM)
		return false;

	item * item = inventory_item(cell);

	if (!item)
		return false;

	if (type_value == REFINE_TYPE_MONEY_ONLY && !quest_flag("deviltower_zone.can_refine"))
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("사귀 타워 완료 보상은 한번까지 사용가능합니다."));
		return false;
	}

	packet_gc_refine_information p;

	p.header = HEADER_GC_REFINE_INFORMATION;
	p.pos = cell;
	p.src_vnum = item->vnum();
	p.result_vnum = item->refined_vnum();
	p.type = type_value;

	if (p.result_vnum == 0)
	{
		metin2::log::error("Refine information: item has no refine result");
		chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템은 개량할 수 없습니다."));
		return false;
	}

	if (item->type() == ITEM_USE && item->sub_type() == USE_TUNING)
	{
		if (type_value == 0)
		{
			chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템은 이 방식으로는 개량할 수 없습니다."));
			return false;
		}
		else
		{
			game::items::item * itemScroll = inventory_item(additional_cell);
			if (!itemScroll || item->vnum() == itemScroll->vnum())
			{
				chat_packet(CHAT_TYPE_INFO, lc_text("같은 개량서를 합칠 수는 없습니다."));
				chat_packet(CHAT_TYPE_INFO, lc_text("축복의 서와 현철을 합칠 수 있습니다."));
				return false;
			}
		}
	}

	refine_manager & rm = refine_manager::instance();

	const refine_table* prt = rm.refine_recipe(item->refine_set());

	if (!prt)
	{
		metin2::log::error("Refine set {:d} not found", item->refine_set());
		chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템은 개량할 수 없습니다."));
		return false;
	}


	if (quest_flag("main_quest_lv7.refine_chance") > 0)
	{
		if (!item->check_item_use_level(20) || item->type() != ITEM_WEAPON)
		{
			chat_packet(CHAT_TYPE_INFO, lc_text("무료 개량 기회는 20 이하의 무기만 가능합니다"));
			return false;
		}
		p.cost = 0;
	}
	else
		p.cost = compute_refine_fee(prt->cost);

	p.prob = prt->prob;
	if (type_value == REFINE_TYPE_MONEY_ONLY)
	{
		p.material_count = 0;
		memset(p.materials, 0, sizeof(p.materials));
	}
	else
	{
		p.material_count = prt->material_count;
		std::memcpy(&p.materials, prt->materials, sizeof(prt->materials));
	}

	get_desc()->packet(&p, sizeof(packet_gc_refine_information));

	set_refine_mode(additional_cell);
	return true;
}

auto character::refine_item(item * item_value, item * target) -> bool
{
	if (!can_handle_item())
		return false;

	if (item_value->sub_type() == USE_TUNING)
	{
		if (item_value->value(0) == MUSIN_SCROLL)
			refine_information(target->cell(), REFINE_TYPE_MUSIN, item_value->cell());
		else if (item_value->value(0) == HYUNIRON_CHN)
			refine_information(target->cell(), REFINE_TYPE_HYUNIRON, item_value->cell());
		else if (item_value->value(0) == BDRAGON_SCROLL)
		{
			if (target->refine_set() != 702) return false;
			refine_information(target->cell(), REFINE_TYPE_BDRAGON, item_value->cell());
		}
		else
		{
			if (target->refine_set() == 501) return false;
			refine_information(target->cell(), REFINE_TYPE_SCROLL, item_value->cell());
		}
	}
	else if (item_value->sub_type() == USE_DETACHMENT && is_set(target->flag(), ITEM_FLAG_REFINEABLE))
	{
		log_manager::instance().item_log(this, target, "USE_DETACHMENT", target->name());

		bool bHasMetinStone = false;

		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; i++)
		{
			int32_t socket = target->get_socket(i);
			if (socket > 2 && socket != ITEM_BROKEN_METIN_VNUM)
			{
				bHasMetinStone = true;
				break;
			}
		}

		if (bHasMetinStone)
		{
			for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
			{
				int32_t socket = target->get_socket(i);
				if (socket > 2 && socket != ITEM_BROKEN_METIN_VNUM)
				{
					auto_give_item(socket);
					target->set_socket(i, ITEM_BROKEN_METIN_VNUM);
				}
			}
			item_value->set_count(item_value->count() - 1);
			return true;
		}
		else
		{
			chat_packet(CHAT_TYPE_INFO, lc_text("빼낼 수 있는 메틴석이 없습니다."));
			return false;
		}
	}

	return false;
}

EVENTFUNC(kill_campfire_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error("Campfire kill event fired without event info");
		return 0;
	}

	character *	ch = info->ch;

	if (ch == nullptr) {
		return 0;
	}
	ch->mining_event = nullptr;
	character_manager::instance().destroy_character(ch);
	return 0;
}

auto character::give_recall_item(item * item) -> bool
{
	int idx = map_index();
	int iEmpireByMapIndex = -1;

	if (idx < 20)
		iEmpireByMapIndex = 1;
	else if (idx < 40)
		iEmpireByMapIndex = 2;
	else if (idx < 60)
		iEmpireByMapIndex = 3;
	else if (idx < 10000)
		iEmpireByMapIndex = 0;

	switch (idx)
	{
		case 66:
		case 216:
			iEmpireByMapIndex = -1;
			break;
	}

	if (iEmpireByMapIndex && empire() != iEmpireByMapIndex)
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("기억해 둘 수 없는 위치 입니다."));
		return false;
	}

	int pos;

	if (item->count() == 1)
	{
		item->set_socket(0, x());
		item->set_socket(1, y());
	}
	else if ((pos = empty_inventory(item->size())) != -1)
	{
		game::items::item * item2 = item_manager::instance().create_item(item->vnum(), 1);

		if (nullptr != item2)
		{
			item2->set_socket(0, x());
			item2->set_socket(1, y());
			item2->add_to_character(this, item_pos(INVENTORY, pos));

			item->set_count(item->count() - 1);
		}
	}
	else
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("소지품에 빈 공간이 없습니다."));
		return false;
	}

	return true;
}

void character::process_recall_item(item * item)
{
	int idx;

	if ((idx = sectree_manager::instance().map_index(item->get_socket(0), item->get_socket(1))) == 0)
		return;

	int iEmpireByMapIndex = -1;

	if (idx < 20)
		iEmpireByMapIndex = 1;
	else if (idx < 40)
		iEmpireByMapIndex = 2;
	else if (idx < 60)
		iEmpireByMapIndex = 3;
	else if (idx < 10000)
		iEmpireByMapIndex = 0;

	switch (idx)
	{
		case 66:
		case 216:
			iEmpireByMapIndex = -1;
			break;
		case 301:
		case 302:
		case 303:
		case 304:
			if( level() < 90 )
			{
				chat_packet(CHAT_TYPE_INFO, lc_text("아이템의 레벨 제한보다 레벨이 낮습니다."));
				return;
			}
			else
				break;
	}

	if (iEmpireByMapIndex && empire() != iEmpireByMapIndex)
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("기억된 위치가 타제국에 속해 있어서 귀환할 수 없습니다."));
		item->set_socket(0, 0);
		item->set_socket(1, 0);
	}
	else
	{
		metin2::log::debug("{} recalls from {:d} {:d} to {:d} {:d}", name(), x(), y(), item->get_socket(0), item->get_socket(1));
		warp_set(item->get_socket(0), item->get_socket(1));
		item->set_count(item->count() - 1);
	}
}

void character::open_private_shop()
{
	unsigned bodyPart = part(PART_MAIN);
	switch (bodyPart)
	{
		case 0:
		case 1:
		case 2:
			chat_packet(CHAT_TYPE_COMMAND, "OpenPrivateShop");
			break;
		default:
			chat_packet(CHAT_TYPE_INFO, lc_text("갑옷을 벗어야 개인 상점을 열 수 있습니다."));
			break;
	}
}

void character::send_my_shop_price_list_cmd(uint32_t item_vnum, uint32_t item_price)
{
	char szLine[256];
	snprintf(szLine, sizeof(szLine), "MyShopPriceList %u %u", item_vnum, item_price);
	chat_packet(CHAT_TYPE_COMMAND, szLine);
	metin2::log::debug("Shop price list command: {}", szLine);
}

void character::use_silk_botary_real(const packet_myshop_pricelist_header* p)
{
	const item_price_info* pInfo = (const item_price_info*)(p + 1);

	if (!p->count)
		send_my_shop_price_list_cmd(1, 0);
	else {
		for (int idx = 0; idx < p->count; idx++)
			send_my_shop_price_list_cmd(pInfo[ idx ].vnum, pInfo[ idx ].price);
	}

	open_private_shop();
}

void character::use_silk_botary(void)
{
	if (no_opened_shop_) {
		uint32_t dwPlayerID = player_id();
		db_clientdesc->db_packet(HEADER_GD_MYSHOP_PRICELIST_REQ, get_desc()->handle(), &dwPlayerID, sizeof(uint32_t));
		no_opened_shop_ = false;
	} else {
		open_private_shop();
	}
}

auto calculate_consume(character * ch) -> int
{
	static const int WARP_NEED_LIFE_PERCENT	= 30;
	static const int WARP_MIN_LIFE_PERCENT	= 10;
	int consumeLife = 0;
	{
		const int curLife		= ch->hp();
		const int needPercent	= WARP_NEED_LIFE_PERCENT;
		const int needLife = ch->max_hp() * needPercent / 100;
		if (curLife < needLife)
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("남은 생명력 양이 모자라 사용할 수 없습니다."));
			return -1;
		}

		consumeLife = needLife;

		const int minPercent	= WARP_MIN_LIFE_PERCENT;
		const int minLife	= ch->max_hp() * minPercent / 100;
		if (curLife - needLife < minLife)
			consumeLife = curLife - minLife;

		if (consumeLife < 0)
			consumeLife = 0;
	}
	return consumeLife;
}

auto calculate_consume_sp(character * ch) -> int
{
	static const int NEED_WARP_SP_PERCENT = 30;

	const int curSP = ch->sp();
	const int needSP = ch->max_sp() * NEED_WARP_SP_PERCENT / 100;

	if (curSP < needSP)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("남은 정신력 양이 모자라 사용할 수 없습니다."));
		return -1;
	}

	return needSP;
}

auto character::use_item_ex(item * item, item_pos dest_cell) -> bool
{
	int iLimitRealtimeStartFirstUseFlagIndex = -1;

	uint16_t wDestCell = dest_cell.cell;
	uint8_t bDestInven = dest_cell.window_type;
	for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
	{
		int32_t limitValue = item->proto()->limits[i].value;

		switch (item->proto()->limits[i].type)
		{
			case LIMIT_LEVEL:
				if (level() < limitValue)
				{
					chat_packet(CHAT_TYPE_INFO, lc_text("아이템의 레벨 제한보다 레벨이 낮습니다."));
					return false;
				}
				break;

			case LIMIT_REAL_TIME_START_FIRST_USE:
				iLimitRealtimeStartFirstUseFlagIndex = i;
				break;

			case LIMIT_TIMER_BASED_ON_WEAR:
				break;
		}
	}

	if (metin2::config::test_server())
	{
		metin2::log::debug("Use item {}: window {:d}, cell {:d}, type {:d}, subtype {:d}", item->name(), bDestInven, wDestCell, item->type(), item->sub_type());
	}

	if ( arena_manager::instance().is_limited_item( map_index(), item->vnum() ) == true )
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("대련 중에는 이용할 수 없는 물품입니다."));
		return false;
	}

	if (-1 != iLimitRealtimeStartFirstUseFlagIndex)
	{
		if (0 == item->get_socket(1))
		{
			int32_t duration = (0 != item->get_socket(0)) ? item->get_socket(0) : item->proto()->limits[iLimitRealtimeStartFirstUseFlagIndex].value;

			if (0 == duration)
				duration = 60 * 60 * 24 * 7;

			item->set_socket(0, time(nullptr) + duration);
			item->start_real_time_expire_event();
		}	

		if (false == item->is_equipped())
			item->set_socket(1, item->get_socket(1) + 1);		
	}

	switch (item->type())
	{
		case ITEM_HAIR:
			return item_process_hair(item, wDestCell);

		case ITEM_POLYMORPH:
			return item_process_polymorph(item);

		case ITEM_QUEST:
			if (get_arena() != nullptr || is_observer_mode() == true)
			{
				if (item->vnum() == 50051 || item->vnum() == 50052 || item->vnum() == 50053)
				{
					chat_packet(CHAT_TYPE_INFO, lc_text("대련 중에는 이용할 수 없는 물품입니다."));
					return false;
				}
			}

			if (!is_set(item->flag(), ITEM_FLAG_QUEST_USE | ITEM_FLAG_QUEST_USE_MULTIPLE))
			{
				if (item->sig_vnum() == 0)
				{
					quest::quest_manager::instance().use_item(player_id(), item, false);
				}
				else
				{
					quest::quest_manager::instance().sig_use(player_id(), item->sig_vnum(), item, false);
				}
			}
			break;

		case ITEM_CAMPFIRE:
			{
				float fx, fy;
				get_delta_by_degree(rotation(), 100.0f, &fx, &fy);

				sectree * tree = sectree_manager::instance().get(map_index(), (int32_t)(x()+fx), (int32_t)(y()+fy));

				if (!tree)
				{
					chat_packet(CHAT_TYPE_INFO, lc_text("모닥불을 피울 수 없는 지점입니다."));
					return false;
				}

				if (tree->is_attr((int32_t)(x()+fx), (int32_t)(y()+fy), ATTR_WATER))
				{
					chat_packet(CHAT_TYPE_INFO, lc_text("물 속에 모닥불을 피울 수 없습니다."));
					return false;
				}

				character * campfire = character_manager::instance().spawn_mob(fishing::CAMPFIRE_MOB, map_index(), (int32_t)(x()+fx), (int32_t)(y()+fy), 0, false, number(0, 359));

				char_event_info* info = alloc_event_info<char_event_info>();

				info->ch = campfire;

				campfire->mining_event = event_create(kill_campfire_event, info, passes_per_sec(40));

				item->set_count(item->count() - 1);
			}
			break;

		case ITEM_UNIQUE:
			{
				switch (item->sub_type())
				{
					case USE_ABILITY_UP:
						{
							switch (item->value(0))
							{
								case APPLY_MOV_SPEED:
									add_affect(AFFECT_UNIQUE_ABILITY, POINT_MOV_SPEED, item->value(2), AFF_MOV_SPEED_POTION, item->value(1), 0, true, true);
									break;

								case APPLY_ATT_SPEED:
									add_affect(AFFECT_UNIQUE_ABILITY, POINT_ATT_SPEED, item->value(2), AFF_ATT_SPEED_POTION, item->value(1), 0, true, true);
									break;

								case APPLY_STR:
									add_affect(AFFECT_UNIQUE_ABILITY, POINT_ST, item->value(2), 0, item->value(1), 0, true, true);
									break;

								case APPLY_DEX:
									add_affect(AFFECT_UNIQUE_ABILITY, POINT_DX, item->value(2), 0, item->value(1), 0, true, true);
									break;

								case APPLY_CON:
									add_affect(AFFECT_UNIQUE_ABILITY, POINT_HT, item->value(2), 0, item->value(1), 0, true, true);
									break;

								case APPLY_INT:
									add_affect(AFFECT_UNIQUE_ABILITY, POINT_IQ, item->value(2), 0, item->value(1), 0, true, true);
									break;

								case APPLY_CAST_SPEED:
									add_affect(AFFECT_UNIQUE_ABILITY, POINT_CASTING_SPEED, item->value(2), 0, item->value(1), 0, true, true);
									break;

								case APPLY_RESIST_MAGIC:
									add_affect(AFFECT_UNIQUE_ABILITY, POINT_RESIST_MAGIC, item->value(2), 0, item->value(1), 0, true, true);
									break;

								case APPLY_ATT_GRADE_BONUS:
									add_affect(AFFECT_UNIQUE_ABILITY, POINT_ATT_GRADE_BONUS, 
											item->value(2), 0, item->value(1), 0, true, true);
									break;

								case APPLY_DEF_GRADE_BONUS:
									add_affect(AFFECT_UNIQUE_ABILITY, POINT_DEF_GRADE_BONUS,
											item->value(2), 0, item->value(1), 0, true, true);
									break;
							}
						}

						if (get_dungeon())
							get_dungeon()->use_potion(this);

						if (get_war_map())
							get_war_map()->use_potion(this, item);

						item->set_count(item->count() - 1);
						break;

					default:
						{
							if (item->sub_type() == USE_SPECIAL)
							{
								metin2::log::debug("Unique item {:d}: special use", item->vnum());

								switch (item->vnum())
								{
									case 71049:
										use_silk_botary();
										break;
								}
							}
							else
							{
								if (!item->is_equipped())
									equip_item(item);
								else
									unequip_item(item);
							}
						}
						break;
				}
			}
			break;

		case ITEM_COSTUME:
		case ITEM_WEAPON:
		case ITEM_ARMOR:
		case ITEM_ROD:
		case ITEM_RING:
		case ITEM_BELT:
		case ITEM_PICK:
			if (!item->is_equipped())
				equip_item(item);
			else
				unequip_item(item);
			break;
		case ITEM_DS:
			{
				if (!item->is_equipped())
					return false;
				return ds_manager::instance().pull_out(this, NPOS, item);
			break;
			}
		case ITEM_SPECIAL_DS:
			if (!item->is_equipped())
				equip_item(item);
			else
				unequip_item(item);
			break;

		case ITEM_FISH:
			{
				if (arena_manager::instance().is_arena_map(map_index()) == true)
				{
					chat_packet(CHAT_TYPE_INFO, lc_text("대련 중에는 이용할 수 없는 물품입니다."));
					return false;
				}

				if (item->sub_type() == FISH_ALIVE)
					fishing::use_fish(this, item);
			}
			break;

		case ITEM_TREASURE_BOX:
			{
				return false;
			}
			break;

		case ITEM_TREASURE_KEY:
			{
				game::items::item * item2;

				if (!get_item(dest_cell) || !(item2 = get_item(dest_cell)))
					return false;

				if (item2->is_exchanging())
					return false;

				if (item2->type() != ITEM_TREASURE_BOX)
				{
					chat_packet(CHAT_TYPE_TALKING, lc_text("열쇠로 여는 물건이 아닌것 같다."));
					return false;
				}

				if (item->value(0) == item2->value(0))
				{
					uint32_t dwBoxVnum = item2->vnum();
					std::vector <uint32_t> dwVnums;
					std::vector <uint32_t> dwCounts;
					std::vector <game::items::item *> item_gets;
					int count = 0;

					if (give_item_from_special_item_group(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
					{
						item_manager::instance().remove_item(item);
						item_manager::instance().remove_item(item2);

						for (int i = 0; i < count; i++){
							switch (dwVnums[i])
							{
								case special_item_group::GOLD:
									chat_packet(CHAT_TYPE_INFO, lc_text("돈 %d 냥을 획득했습니다."), dwCounts[i]);
									break;
								case special_item_group::EXP:
									chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 부터 신비한 빛이 나옵니다."));
									chat_packet(CHAT_TYPE_INFO, lc_text("%d의 경험치를 획득했습니다."), dwCounts[i]);
									break;
								case special_item_group::MOB:
									chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 몬스터가 나타났습니다!"));
									break;
								case special_item_group::SLOW:
									chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 나온 빨간 연기를 들이마시자 움직이는 속도가 느려졌습니다!"));
									break;
								case special_item_group::DRAIN_HP:
									chat_packet(CHAT_TYPE_INFO, lc_text("상자가 갑자기 폭발하였습니다! 생명력이 감소했습니다."));
									break;
								case special_item_group::POISON:
									chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 나온 녹색 연기를 들이마시자 독이 온몸으로 퍼집니다!"));
									break;
								case special_item_group::MOB_GROUP:
									chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 몬스터가 나타났습니다!"));
									break;
								default:
									if (item_gets[i])
									{
										if (dwCounts[i] > 1)
											chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 %s 가 %d 개 나왔습니다."), item_gets[i]->name(), dwCounts[i]);
										else
											chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 %s 가 나왔습니다."), item_gets[i]->name());

									}
							}
						}
					}
					else
					{
						chat_packet(CHAT_TYPE_TALKING, lc_text("열쇠가 맞지 않는 것 같다."));
						return false;
					}
				}
				else
				{
					chat_packet(CHAT_TYPE_TALKING, lc_text("열쇠가 맞지 않는 것 같다."));
					return false;
				}
			}
			break;

		case ITEM_GIFTBOX:
			{
				uint32_t dwBoxVnum = item->vnum();
				std::vector <uint32_t> dwVnums;
				std::vector <uint32_t> dwCounts;
				std::vector <game::items::item *> item_gets;
				int count = 0;

				if( (dwBoxVnum > 51500 && dwBoxVnum < 52000) || (dwBoxVnum >= 50255 && dwBoxVnum <= 50260) )
				{
					if( !(this->dragon_soul_is_qualified()) )
					{
						chat_packet(CHAT_TYPE_INFO,lc_text("먼저 용혼석 퀘스트를 완료하셔야 합니다."));
						return false;
					}
				}

				if (give_item_from_special_item_group(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
				{
					item->set_count(item->count()-1);

					for (int i = 0; i < count; i++){
						switch (dwVnums[i])
						{
						case special_item_group::GOLD:
							chat_packet(CHAT_TYPE_INFO, lc_text("돈 %d 냥을 획득했습니다."), dwCounts[i]);
							break;
						case special_item_group::EXP:
							chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 부터 신비한 빛이 나옵니다."));
							chat_packet(CHAT_TYPE_INFO, lc_text("%d의 경험치를 획득했습니다."), dwCounts[i]);
							break;
						case special_item_group::MOB:
							chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 몬스터가 나타났습니다!"));
							break;
						case special_item_group::SLOW:
							chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 나온 빨간 연기를 들이마시자 움직이는 속도가 느려졌습니다!"));
							break;
						case special_item_group::DRAIN_HP:
							chat_packet(CHAT_TYPE_INFO, lc_text("상자가 갑자기 폭발하였습니다! 생명력이 감소했습니다."));
							break;
						case special_item_group::POISON:
							chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 나온 녹색 연기를 들이마시자 독이 온몸으로 퍼집니다!"));
							break;
						case special_item_group::MOB_GROUP:
							chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 몬스터가 나타났습니다!"));
							break;
						default:
							if (item_gets[i])
							{
								if (dwCounts[i] > 1)
									chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 %s 가 %d 개 나왔습니다."), item_gets[i]->name(), dwCounts[i]);
								else
									chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 %s 가 나왔습니다."), item_gets[i]->name());
							}
						}
					}
				}
				else
				{
					chat_packet(CHAT_TYPE_TALKING, lc_text("아무것도 얻을 수 없었습니다."));
					return false;
				}
			}
			break;

		case ITEM_SKILLFORGET:
			{
				if (!item->get_socket(0))
				{
					item_manager::instance().remove_item(item);
					return false;
				}

				uint32_t dwVnum = item->get_socket(0);

				if (skill_level_down(dwVnum))
				{
					item_manager::instance().remove_item(item);
					chat_packet(CHAT_TYPE_INFO, lc_text("스킬 레벨을 내리는데 성공하였습니다."));
				}
				else
					chat_packet(CHAT_TYPE_INFO, lc_text("스킬 레벨을 내릴 수 없습니다."));
			}
			break;

		case ITEM_SKILLBOOK:
			{
				if (is_polymorphed())
				{
					chat_packet(CHAT_TYPE_INFO, lc_text("변신중에는 책을 읽을수 없습니다."));
					return false;
				}

				uint32_t dwVnum = 0;

				if (item->vnum() == 50300)
				{
					dwVnum = item->get_socket(0);
				}
				else
				{
					dwVnum = item->value(0);
				}

				if (0 == dwVnum)
				{
					item_manager::instance().remove_item(item);

					return false;
				}

				if (true == learn_skill_by_book(dwVnum))
				{
					item_manager::instance().remove_item(item);

					int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);

					if (metin2::config::game().distribution_test_server)
						iReadDelay /= 3;


					set_skill_next_read_time(dwVnum, get_global_time() + iReadDelay);
				}
			}
			break;

		case ITEM_USE:
			{
				if (item->vnum() > 50800 && item->vnum() <= 50820)
				{
					if (metin2::config::test_server())
						metin2::log::debug("Additional effect item {:d} (subtype {:d})", item->original_vnum(), item->sub_type());

					int affect_type = AFFECT_EXP_BONUS_EURO_FREE;
					int apply_type = a_apply_info[item->value(0)].point_type;
					int apply_value = item->value(2);
					int apply_duration = item->value(1);

					switch (item->sub_type())
					{
						case USE_ABILITY_UP:
							if (find_affect(affect_type, apply_type))
							{
								chat_packet(CHAT_TYPE_INFO, lc_text("이미 효과가 걸려 있습니다."));
								return false;
							}

							{
								switch (item->value(0))
								{
									case APPLY_MOV_SPEED:
										add_affect(affect_type, apply_type, apply_value, AFF_MOV_SPEED_POTION, apply_duration, 0, true, true);
										break;

									case APPLY_ATT_SPEED:
										add_affect(affect_type, apply_type, apply_value, AFF_ATT_SPEED_POTION, apply_duration, 0, true, true);
										break;

									case APPLY_STR:
									case APPLY_DEX:
									case APPLY_CON:
									case APPLY_INT:
									case APPLY_CAST_SPEED:
									case APPLY_RESIST_MAGIC:
									case APPLY_ATT_GRADE_BONUS:
									case APPLY_DEF_GRADE_BONUS:
										add_affect(affect_type, apply_type, apply_value, 0, apply_duration, 0, true, true);
										break;
								}
							}

							if (get_dungeon())
								get_dungeon()->use_potion(this);

							if (get_war_map())
								get_war_map()->use_potion(this, item);

							item->set_count(item->count() - 1);
							break;

					case USE_AFFECT :
						{
							if (find_affect(AFFECT_EXP_BONUS_EURO_FREE, a_apply_info[item->value(1)].point_type))
							{
								chat_packet(CHAT_TYPE_INFO, lc_text("이미 효과가 걸려 있습니다."));
							}
							else
							{
								if (item->is_pc_bang_item() == true)
								{
									chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템은 PC방에서만 사용할 수 있습니다."));
									return false;
								}

								add_affect(AFFECT_EXP_BONUS_EURO_FREE, a_apply_info[item->value(1)].point_type, item->value(2), 0, item->value(3), 0, false, true);
								item->set_count(item->count() - 1);
							}
						}
						break;

					case USE_POTION_NODELAY:
						{
							if (arena_manager::instance().is_arena_map(map_index()) == true)
							{
								if (quest::quest_manager::instance().event_flag("arena_potion_limit") > 0)
								{
									chat_packet(CHAT_TYPE_INFO, lc_text("대련장에서 사용하실 수 없습니다."));
									return false;
								}

								switch (item->vnum())
								{
									case 70020 :
									case 71018 :
									case 71019 :
									case 71020 :
										if (quest::quest_manager::instance().event_flag("arena_potion_limit_count") < 10000)
										{
											if (potion_limit_ <= 0)
											{
												chat_packet(CHAT_TYPE_INFO, lc_text("사용 제한량을 초과하였습니다."));
												return false;
											}
										}
										break;

									default :
										chat_packet(CHAT_TYPE_INFO, lc_text("대련장에서 사용하실 수 없습니다."));
										return false;
										break;
								}
							}

							bool used = false;

							if (item->value(0) != 0)
							{
								if (hp() < max_hp())
								{
									point_change(POINT_HP, item->value(0) * (100 + point(POINT_POTION_BONUS)) / 100);
									effect_packet(SE_HPUP_RED);
									used = true;
								}
							}

							if (item->value(1) != 0)
							{
								if (sp() < max_sp())
								{
									point_change(POINT_SP, item->value(1) * (100 + point(POINT_POTION_BONUS)) / 100);
									effect_packet(SE_SPUP_BLUE);
									used = true;
								}
							}

							if (item->value(3) != 0)
							{
								if (hp() < max_hp())
								{
									point_change(POINT_HP, item->value(3) * max_hp() / 100);
									effect_packet(SE_HPUP_RED);
									used = true;
								}
							}

							if (item->value(4) != 0)
							{
								if (sp() < max_sp())
								{
									point_change(POINT_SP, item->value(4) * max_sp() / 100);
									effect_packet(SE_SPUP_BLUE);
									used = true;
								}
							}

							if (used)
							{
								if (item->vnum() == 50085 || item->vnum() == 50086)
								{
									if (metin2::config::test_server())
										chat_packet(CHAT_TYPE_INFO, lc_text("월병 또는 종자 를 사용하였습니다"));
								}
								if (get_dungeon())
									get_dungeon()->use_potion(this);

								if (get_war_map())
									get_war_map()->use_potion(this, item);

								potion_limit_--;

								item->set_count(item->count() - 1);
							}
						}
						break;
					}

					return true;
				}

				if (item->vnum() >= 27863 && item->vnum() <= 27883)
				{
					if (arena_manager::instance().is_arena_map(map_index()) == true)
					{
						chat_packet(CHAT_TYPE_INFO, lc_text("대련 중에는 이용할 수 없는 물품입니다."));
						return false;
					}
				}

				if (metin2::config::test_server())
				{
					 metin2::log::debug("Use item {}: type {:d}, subtype {:d}, vnum {:d}", item->name(), item->type(), item->sub_type(), item->original_vnum());
				}

				switch (item->sub_type())
				{
					case USE_TIME_CHARGE_PER:
						{
							game::items::item * pDestItem = get_item(dest_cell);
							if (nullptr == pDestItem)
							{
								return false;
							}
							if (pDestItem->is_dragon_soul())
							{
								int ret;
								char buf[128];
								if (item->vnum() == DRAGON_HEART_VNUM)
								{
									ret = pDestItem->give_more_time_per((float)item->get_socket(ITEM_SOCKET_CHARGING_AMOUNT_IDX));
								}
								else
								{
									ret = pDestItem->give_more_time_per((float)item->value(ITEM_VALUE_CHARGING_AMOUNT_IDX));
								}
								if (ret > 0)
								{
									if (item->vnum() == DRAGON_HEART_VNUM)
									{
										snprintf(buf, sizeof(buf), "Inc %ds by item{VN:%d SOC%d:%d}", ret, item->vnum(), ITEM_SOCKET_CHARGING_AMOUNT_IDX, item->get_socket(ITEM_SOCKET_CHARGING_AMOUNT_IDX));
									}
									else
									{
										snprintf(buf, sizeof(buf), "Inc %ds by item{VN:%d VAL%d:%d}", ret, item->vnum(), ITEM_VALUE_CHARGING_AMOUNT_IDX, item->value(ITEM_VALUE_CHARGING_AMOUNT_IDX));
									}

									chat_packet(CHAT_TYPE_INFO, lc_text("%d초 만큼 충전되었습니다."), ret);
									item->set_count(item->count() - 1);
									log_manager::instance().item_log(this, item, "DS_CHARGING_SUCCESS", buf);
									return true;
								}
								else
								{
									if (item->vnum() == DRAGON_HEART_VNUM)
									{
										snprintf(buf, sizeof(buf), "No change by item{VN:%d SOC%d:%d}", item->vnum(), ITEM_SOCKET_CHARGING_AMOUNT_IDX, item->get_socket(ITEM_SOCKET_CHARGING_AMOUNT_IDX));
									}
									else
									{
										snprintf(buf, sizeof(buf), "No change by item{VN:%d VAL%d:%d}", item->vnum(), ITEM_VALUE_CHARGING_AMOUNT_IDX, item->value(ITEM_VALUE_CHARGING_AMOUNT_IDX));
									}

									chat_packet(CHAT_TYPE_INFO, lc_text("충전할 수 없습니다."));
									log_manager::instance().item_log(this, item, "DS_CHARGING_FAILED", buf);
									return false;
								}
							}
							else
								return false;
						}
						break;
					case USE_TIME_CHARGE_FIX:
						{
							game::items::item * pDestItem = get_item(dest_cell);
							if (nullptr == pDestItem)
							{
								return false;
							}
							if (pDestItem->is_dragon_soul())
							{
								int ret = pDestItem->give_more_time_fix(item->value(ITEM_VALUE_CHARGING_AMOUNT_IDX));
								char buf[128];
								if (ret)
								{
									chat_packet(CHAT_TYPE_INFO, lc_text("%d초 만큼 충전되었습니다."), ret);
									snprintf(buf, sizeof(buf), "Increase %ds by item{VN:%d VAL%d:%d}", ret, item->vnum(), ITEM_VALUE_CHARGING_AMOUNT_IDX, item->value(ITEM_VALUE_CHARGING_AMOUNT_IDX));
									log_manager::instance().item_log(this, item, "DS_CHARGING_SUCCESS", buf);
									item->set_count(item->count() - 1);
									return true;
								}
								else
								{
									chat_packet(CHAT_TYPE_INFO, lc_text("충전할 수 없습니다."));
									snprintf(buf, sizeof(buf), "No change by item{VN:%d VAL%d:%d}", item->vnum(), ITEM_VALUE_CHARGING_AMOUNT_IDX, item->value(ITEM_VALUE_CHARGING_AMOUNT_IDX));
									log_manager::instance().item_log(this, item, "DS_CHARGING_FAILED", buf);
									return false;
								}
							}
							else
								return false;
						}
						break;
					case USE_SPECIAL:

						switch (item->vnum())
						{
							case ITEM_NOG_POCKET:
								{
									if (find_affect(AFFECT_NOG_ABILITY))
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("이미 효과가 걸려 있습니다."));
										return false;
									}
									int32_t time = item->value(0);
									int32_t moveSpeedPer	= item->value(1);
									int32_t attPer	= item->value(2);
									int32_t expPer			= item->value(3);
									add_affect(AFFECT_NOG_ABILITY, POINT_MOV_SPEED, moveSpeedPer, AFF_MOV_SPEED_POTION, time, 0, true, true);
									add_affect(AFFECT_NOG_ABILITY, POINT_MALL_ATTBONUS, attPer, AFF_NONE, time, 0, true, true);
									add_affect(AFFECT_NOG_ABILITY, POINT_MALL_EXPBONUS, expPer, AFF_NONE, time, 0, true, true);
									item->set_count(item->count() - 1);
								}
								break;

							case ITEM_RAMADAN_CANDY:
								{
									int32_t time = item->value(0);
									int32_t moveSpeedPer	= item->value(1);
									int32_t attPer	= item->value(2);
									int32_t expPer			= item->value(3);
									add_affect(AFFECT_RAMADAN_ABILITY, POINT_MOV_SPEED, moveSpeedPer, AFF_MOV_SPEED_POTION, time, 0, true, true);
									add_affect(AFFECT_RAMADAN_ABILITY, POINT_MALL_ATTBONUS, attPer, AFF_NONE, time, 0, true, true);
									add_affect(AFFECT_RAMADAN_ABILITY, POINT_MALL_EXPBONUS, expPer, AFF_NONE, time, 0, true, true);
									item->set_count(item->count() - 1);
								}
								break;
							case ITEM_MARRIAGE_RING:
								{
									marriage::marriage* pMarriage = marriage::manager::instance().get(player_id());
									if (pMarriage)
									{
										if (pMarriage->ch1 != nullptr)
										{
											if (arena_manager::instance().is_arena_map(pMarriage->ch1->map_index()) == true)
											{
												chat_packet(CHAT_TYPE_INFO, lc_text("대련 중에는 이용할 수 없는 물품입니다."));
												break;
											}
										}

										if (pMarriage->ch2 != nullptr)
										{
											if (arena_manager::instance().is_arena_map(pMarriage->ch2->map_index()) == true)
											{
												chat_packet(CHAT_TYPE_INFO, lc_text("대련 중에는 이용할 수 없는 물품입니다."));
												break;
											}
										}

										int consumeSP = calculate_consume_sp(this);

										if (consumeSP < 0)
											return false;

										point_change(POINT_SP, -consumeSP, false);

										warp_to_pid(pMarriage->other(player_id()));
									}
									else
										chat_packet(CHAT_TYPE_INFO, lc_text("결혼 상태가 아니면 결혼반지를 사용할 수 없습니다."));
								}
								break;

							case UNIQUE_ITEM_CAPE_OF_COURAGE:
							case 70057:
							case REWARD_BOX_UNIQUE_ITEM_CAPE_OF_COURAGE:
								aggregate_monster();
								item->set_count(item->count()-1);
								break;

							case UNIQUE_ITEM_WHITE_FLAG:
								forget_my_attacker();
								item->set_count(item->count()-1);
								break;

							case UNIQUE_ITEM_TREASURE_BOX:
								break;

							case 30093:
							case 30094:
							case 30095:
							case 30096:
								{
									const int MAX_BAG_INFO = 26;
									static struct lucky_bag_info
									{
										uint32_t count;
										int prob;
										uint32_t vnum;
									} b1[MAX_BAG_INFO] =
									{
										{ 1000,	302,	1 },
										{ 10,	150,	27002 },
										{ 10,	75,	27003 },
										{ 10,	100,	27005 },
										{ 10,	50,	27006 },
										{ 10,	80,	27001 },
										{ 10,	50,	27002 },
										{ 10,	80,	27004 },
										{ 10,	50,	27005 },
										{ 1,	10,	50300 },
										{ 1,	6,	92 },
										{ 1,	2,	132 },
										{ 1,	6,	1052 },
										{ 1,	2,	1092 },
										{ 1,	6,	2082 },
										{ 1,	2,	2122 },
										{ 1,	6,	3082 },
										{ 1,	2,	3122 },
										{ 1,	6,	5052 },
										{ 1,	2,	5082 },
										{ 1,	6,	7082 },
										{ 1,	2,	7122 },
										{ 1,	1,	11282 },
										{ 1,	1,	11482 },
										{ 1,	1,	11682 },
										{ 1,	1,	11882 },
									};

									lucky_bag_info * bi = nullptr;
									bi = b1;

									int pct = number(1, 1000);

									int i;
									for (i=0;i<MAX_BAG_INFO;i++)
									{
										if (pct <= bi[i].prob)
											break;
										pct -= bi[i].prob;
									}
									if (i>=MAX_BAG_INFO)
										return false;

									if (bi[i].vnum == 50300)
									{
										give_random_skill_book();
									}
									else if (bi[i].vnum == 1)
									{
										point_change(POINT_GOLD, 1000, true);
									}
									else
									{
										auto_give_item(bi[i].vnum, bi[i].count);
									}
									item_manager::instance().remove_item(item);
								}
								break;

							case 50004:
								{
									if (item->get_socket(0))
									{
										item->set_socket(0, item->get_socket(0) + 1);
									}
									else
									{
										int iMapIndex = map_index();

										pixel_position pos;

										if (sectree_manager::instance().random_location(iMapIndex, pos, 700))
										{
											item->set_socket(0, 1);
											item->set_socket(1, pos.x);
											item->set_socket(2, pos.y);
										}
										else
										{
											chat_packet(CHAT_TYPE_INFO, lc_text("이 곳에선 이벤트용 감지기가 동작하지 않는것 같습니다."));
											return false;
										}
									}

									int dist = 0;
									float distance = (distance_sqrt(x()-item->get_socket(1), y()-item->get_socket(2)));

									if (distance < 1000.0f)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("이벤트용 감지기가 신비로운 빛을 내며 사라집니다."));

										struct event_stone_info
										{
											uint32_t vnum;
											int count;
											int prob;
										};
										const int EVENT_STONE_MAX_INFO = 15;
										event_stone_info info_10[EVENT_STONE_MAX_INFO] =
										{ 
											{ 27001, 10,  8 },
											{ 27004, 10,  6 },
											{ 27002, 10, 12 },
											{ 27005, 10, 12 },
											{ 27100,  1,  9 },
											{ 27103,  1,  9 },
											{ 27101,  1, 10 },
											{ 27104,  1, 10 },
											{ 27999,  1, 12 },

											{ 25040,  1,  4 },

											{ 27410,  1,  0 },
											{ 27600,  1,  0 },
											{ 25100,  1,  0 },

											{ 50001,  1,  0 },
											{ 50003,  1,  1 },
										};
										event_stone_info info_7[EVENT_STONE_MAX_INFO] =
										{ 
											{ 27001, 10,  1 },
											{ 27004, 10,  1 },
											{ 27004, 10,  9 },
											{ 27005, 10,  9 },
											{ 27100,  1,  5 },
											{ 27103,  1,  5 },
											{ 27101,  1, 10 },
											{ 27104,  1, 10 },
											{ 27999,  1, 14 },

											{ 25040,  1,  5 },

											{ 27410,  1,  5 },
											{ 27600,  1,  5 },
											{ 25100,  1,  5 },

											{ 50001,  1,  0 },
											{ 50003,  1,  5 },

										};
										event_stone_info info_4[EVENT_STONE_MAX_INFO] =
										{ 
											{ 27001, 10,  0 },
											{ 27004, 10,  0 },
											{ 27002, 10,  0 },
											{ 27005, 10,  0 },
											{ 27100,  1,  0 },
											{ 27103,  1,  0 },
											{ 27101,  1,  0 },
											{ 27104,  1,  0 },
											{ 27999,  1, 25 },

											{ 25040,  1,  0 },

											{ 27410,  1,  0 },
											{ 27600,  1,  0 },
											{ 25100,  1, 15 },

											{ 50001,  1, 10 },
											{ 50003,  1, 50 },

										};

										{
											event_stone_info* info;
											if (item->get_socket(0) <= 4)
												info = info_4;
											else if (item->get_socket(0) <= 7)
												info = info_7;
											else
												info = info_10;

											int prob = number(1, 100);

											for (int i = 0; i < EVENT_STONE_MAX_INFO; ++i)
											{
												if (!info[i].prob)
													continue;

												if (prob <= info[i].prob)
												{
													if (info[i].vnum == 50001)
													{
														uint32_t * pdw = new uint32_t[2];

														pdw[0] = info[i].vnum;
														pdw[1] = info[i].count;

														db_manager::instance().return_query(QID_LOTTO, player_id(), pdw,
																"INSERT INTO lotto_list VALUES(0, 'server%s', %u, NOW())", 
																get_table_postfix(), player_id());
													}
													else
														auto_give_item(info[i].vnum, info[i].count);

													break;
												}
												prob -= info[i].prob;
											}
										}

										char chatbuf[CHAT_MAX_LEN + 1];
										int len = snprintf(chatbuf, sizeof(chatbuf), "StoneDetect %u 0 0", (uint32_t)get_vid());

										if (len < 0 || len >= (int) sizeof(chatbuf))
											len = sizeof(chatbuf) - 1;

										++len;

										packet_gc_chat pack_chat;
										pack_chat.header	= HEADER_GC_CHAT;
										pack_chat.size		= sizeof(packet_gc_chat) + len;
										pack_chat.type		= CHAT_TYPE_COMMAND;
										pack_chat.id		= 0;
										pack_chat.empire	= get_desc()->empire();

										temp_buffer buf;
										buf.write(&pack_chat, sizeof(packet_gc_chat));
										buf.write(chatbuf, len);

										packet_around(buf.read_peek(), buf.size());

										item_manager::instance().remove_item(item, "REMOVE (DETECT_EVENT_STONE) 1");
										return true;
									}
									else if (distance < 20000)
										dist = 1;
									else if (distance < 70000)
										dist = 2;
									else
										dist = 3;

									const int STONE_DETECT_MAX_TRY = 10;
									if (item->get_socket(0) >= STONE_DETECT_MAX_TRY)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("이벤트용 감지기가 흔적도 없이 사라집니다."));
										item_manager::instance().remove_item(item, "REMOVE (DETECT_EVENT_STONE) 0");
										auto_give_item(27002);
										return true;
									}

									if (dist)
									{
										char chatbuf[CHAT_MAX_LEN + 1];
										int len = snprintf(chatbuf, sizeof(chatbuf),
												"StoneDetect %u %d %d",
											   	(uint32_t)get_vid(), dist, (int)get_degree_from_position_xy(x(), item->get_socket(2), item->get_socket(1), y()));

										if (len < 0 || len >= (int) sizeof(chatbuf))
											len = sizeof(chatbuf) - 1;

										++len;

										packet_gc_chat pack_chat;
										pack_chat.header	= HEADER_GC_CHAT;
										pack_chat.size		= sizeof(packet_gc_chat) + len;
										pack_chat.type		= CHAT_TYPE_COMMAND;
										pack_chat.id		= 0;
										pack_chat.empire	= get_desc()->empire();

										temp_buffer buf;
										buf.write(&pack_chat, sizeof(packet_gc_chat));
										buf.write(chatbuf, len);

										packet_around(buf.read_peek(), buf.size());
									}

								}
								break;

							case 27989:
							case 76006:
								{
									sectree_map * pMap = sectree_manager::instance().map(map_index());

									if (pMap != nullptr)
									{
										item->set_socket(0, item->get_socket(0) + 1);

										f_find_stone f;

										pMap->for_each(f);

										if (f.stone.size() > 0)
										{
											std::map<uint32_t, character *>::iterator stone = f.stone.begin();

											uint32_t max = UINT_MAX;
											character * pTarget = stone->second;

											while (stone != f.stone.end())
											{
												uint32_t dist = (uint32_t)distance_sqrt(x()-stone->second->x(), y()-stone->second->y());

												if (dist != 0 && max > dist)
												{
													max = dist;
													pTarget = stone->second;
												}
												stone++;
											}

											if (pTarget != nullptr)
											{
												int val = 3;

												if (max < 10000) val = 2;
												else if (max < 70000) val = 1;

												chat_packet(CHAT_TYPE_COMMAND, "StoneDetect %u %d %d", (uint32_t)get_vid(), val,
														(int)get_degree_from_position_xy(x(), pTarget->y(), pTarget->x(), y()));
											}
											else
											{
												chat_packet(CHAT_TYPE_INFO, lc_text("감지기를 작용하였으나 감지되는 영석이 없습니다."));
											}
										}
										else
										{
											chat_packet(CHAT_TYPE_INFO, lc_text("감지기를 작용하였으나 감지되는 영석이 없습니다."));
										}

										if (item->get_socket(0) >= 6)
										{
											chat_packet(CHAT_TYPE_COMMAND, "StoneDetect %u 0 0", (uint32_t)get_vid());
											item_manager::instance().remove_item(item);
										}
									}
									break;
								}
								break;

							case 27996:
								item->set_count(item->count() - 1);
								break;

							case 27987:
								{
									item->set_count(item->count() - 1);

									int r = number(1, 100);

									if (r <= 50)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("조개에서 돌조각이 나왔습니다."));
										auto_give_item(27990);
									}
									else
									{
										const int prob_table[] =
										{
											95, 97, 99
										};

										if (r <= prob_table[0])
										{
											chat_packet(CHAT_TYPE_INFO, lc_text("조개가 흔적도 없이 사라집니다."));
										}
										else if (r <= prob_table[1])
										{
											chat_packet(CHAT_TYPE_INFO, lc_text("조개에서 백진주가 나왔습니다."));
											auto_give_item(27992);
										}
										else if (r <= prob_table[2])
										{
											chat_packet(CHAT_TYPE_INFO, lc_text("조개에서 청진주가 나왔습니다."));
											auto_give_item(27993);
										}
										else
										{
											chat_packet(CHAT_TYPE_INFO, lc_text("조개에서 피진주가 나왔습니다."));
											auto_give_item(27994);
										}
									}
								}
								break;

							case 71013:
								create_fly(number(FLY_FIREWORK1, FLY_FIREWORK6), this);
								item->set_count(item->count() - 1);
								break;

							case 50100:
							case 50101:
							case 50102:
							case 50103:
							case 50104:
							case 50105:
							case 50106:
								create_fly(item->vnum() - 50100 + FLY_FIREWORK1, this);
								item->set_count(item->count() - 1);
								break;

							case 50200:
								open_private_shop();
								break;

							case fishing::FISH_MIND_PILL_VNUM:
								add_affect(AFFECT_FISH_MIND_PILL, POINT_NONE, 0, AFF_FISH_MIND, 20*60, 0, true);
								item->set_count(item->count() - 1);
								break;

							case 50301:
							case 50302:
							case 50303:
								{
									if (is_polymorphed() == true)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("둔갑 중에는 능력을 올릴 수 없습니다."));
										return false;
									}

									int lv = skill_level(SKILL_LEADERSHIP);

									if (lv < item->value(0))
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("이 책은 너무 어려워 이해하기가 힘듭니다."));
										return false;
									}

									if (lv >= item->value(1))
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("이 책은 아무리 봐도 도움이 될 것 같지 않습니다."));
										return false;
									}

									if (learn_skill_by_book(SKILL_LEADERSHIP))
									{
										item_manager::instance().remove_item(item);

										int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
										if (metin2::config::game().distribution_test_server) iReadDelay /= 3;

										set_skill_next_read_time(SKILL_LEADERSHIP, get_global_time() + iReadDelay);
									}
								}
								break;

							case 50304:
							case 50305:
							case 50306:
								{
									if (is_polymorphed())
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("변신중에는 책을 읽을수 없습니다."));
										return false;

									}
									if (skill_level(SKILL_COMBO) == 0 && level() < 30)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("레벨 30이 되기 전에는 습득할 수 있을 것 같지 않습니다."));
										return false;
									}

									if (skill_level(SKILL_COMBO) == 1 && level() < 50)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("레벨 50이 되기 전에는 습득할 수 있을 것 같지 않습니다."));
										return false;
									}

									if (skill_level(SKILL_COMBO) >= 2)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("연계기는 더이상 수련할 수 없습니다."));
										return false;
									}

									int iPct = item->value(0);

									if (learn_skill_by_book(SKILL_COMBO, iPct))
									{
										item_manager::instance().remove_item(item);

										int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
										if (metin2::config::game().distribution_test_server) iReadDelay /= 3;

										set_skill_next_read_time(SKILL_COMBO, get_global_time() + iReadDelay);
									}
								}
								break;
							case 50311:
							case 50312:
							case 50313:
								{
									if (is_polymorphed())
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("변신중에는 책을 읽을수 없습니다."));
										return false;

									}
									uint32_t dwSkillVnum = item->value(0);
									int iPct = std::clamp(item->value(1), 0, 100);
									if (skill_level(dwSkillVnum)>=20 || dwSkillVnum-SKILL_LANGUAGE1+1 == empire())
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("이미 완벽하게 알아들을 수 있는 언어이다."));
										return false;
									}

									if (learn_skill_by_book(dwSkillVnum, iPct))
									{
										item_manager::instance().remove_item(item);

										int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
										if (metin2::config::game().distribution_test_server) iReadDelay /= 3;

										set_skill_next_read_time(dwSkillVnum, get_global_time() + iReadDelay);
									}
								}
								break;

							case 50061 :
								{
									if (is_polymorphed())
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("변신중에는 책을 읽을수 없습니다."));
										return false;

									}
									uint32_t dwSkillVnum = item->value(0);
									int iPct = std::clamp(item->value(1), 0, 100);

									if (skill_level(dwSkillVnum) >= 10)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("더 이상 수련할 수 없습니다."));
										return false;
									}

									if (learn_skill_by_book(dwSkillVnum, iPct))
									{
										item_manager::instance().remove_item(item);

										int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
										if (metin2::config::game().distribution_test_server) iReadDelay /= 3;

										set_skill_next_read_time(dwSkillVnum, get_global_time() + iReadDelay);
									}
								}
								break;

							case 50314: case 50315: case 50316:
							case 50323: case 50324:
							case 50325: case 50326:
								{
									if (is_polymorphed() == true)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("둔갑 중에는 능력을 올릴 수 없습니다."));
										return false;
									}

									int iSkillLevelLowLimit = item->value(0);
									int iSkillLevelHighLimit = item->value(1);
									int iPct = std::clamp(item->value(2), 0, 100);
									int iLevelLimit = item->value(3);
									uint32_t dwSkillVnum = 0;

									switch (item->vnum())
									{
										case 50314: case 50315: case 50316:
											dwSkillVnum = SKILL_POLYMORPH;
											break;

										case 50323: case 50324:
											dwSkillVnum = SKILL_ADD_HP;
											break;

										case 50325: case 50326:
											dwSkillVnum = SKILL_RESIST_PENETRATE;
											break;

										default:
											return false;
									}

									if (0 == dwSkillVnum)
										return false;

									if (level() < iLevelLimit)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("이 책을 읽으려면 레벨을 더 올려야 합니다."));
										return false;
									}

									if (skill_level(dwSkillVnum) >= 40)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("더 이상 수련할 수 없습니다."));
										return false;
									}

									if (skill_level(dwSkillVnum) < iSkillLevelLowLimit)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("이 책은 너무 어려워 이해하기가 힘듭니다."));
										return false;
									}

									if (skill_level(dwSkillVnum) >= iSkillLevelHighLimit)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("이 책으로는 더 이상 수련할 수 없습니다."));
										return false;
									}

									if (learn_skill_by_book(dwSkillVnum, iPct))
									{
										item_manager::instance().remove_item(item);

										int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
										if (metin2::config::game().distribution_test_server) iReadDelay /= 3;

										set_skill_next_read_time(dwSkillVnum, get_global_time() + iReadDelay);
									}
								}
								break;

							case 50902:
							case 50903:
							case 50904:
								{
									if (is_polymorphed())
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("변신중에는 책을 읽을수 없습니다."));
										return false;

									}
									uint32_t dwSkillVnum = SKILL_CREATE;
									int iPct = std::clamp(item->value(1), 0, 100);

									if (skill_level(dwSkillVnum)>=40)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("더 이상 수련할 수 없습니다."));
										return false;
									}

									if (learn_skill_by_book(dwSkillVnum, iPct))
									{
										item_manager::instance().remove_item(item);

										int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
										if (metin2::config::game().distribution_test_server) iReadDelay /= 3;

										set_skill_next_read_time(dwSkillVnum, get_global_time() + iReadDelay);

										if (metin2::config::test_server()) 
										{
											chat_packet(CHAT_TYPE_INFO, "[TEST_SERVER] Success to learn skill ");
										}
									}
									else
									{
										if (metin2::config::test_server()) 
										{
											chat_packet(CHAT_TYPE_INFO, "[TEST_SERVER] Failed to learn skill ");
										}
									}
								}
								break;

							case ITEM_MINING_SKILL_TRAIN_BOOK:
								{
									if (is_polymorphed())
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("변신중에는 책을 읽을수 없습니다."));
										return false;

									}
									uint32_t dwSkillVnum = SKILL_MINING;
									int iPct = std::clamp(item->value(1), 0, 100);

									if (skill_level(dwSkillVnum)>=40)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("더 이상 수련할 수 없습니다."));
										return false;
									}

									if (learn_skill_by_book(dwSkillVnum, iPct))
									{
										item_manager::instance().remove_item(item);

										int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
										if (metin2::config::game().distribution_test_server) iReadDelay /= 3;

										set_skill_next_read_time(dwSkillVnum, get_global_time() + iReadDelay);
									}
								}
								break;

							case ITEM_HORSE_SKILL_TRAIN_BOOK:
								{
									if (is_polymorphed())
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("변신중에는 책을 읽을수 없습니다."));
										return false;

									}
									uint32_t dwSkillVnum = SKILL_HORSE;
									int iPct = std::clamp(item->value(1), 0, 100);

									if (level() < 50)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("아직 승마 스킬을 수련할 수 있는 레벨이 아닙니다."));
										return false;
									}

									if (!metin2::config::test_server() && get_global_time() < skill_next_read_time(dwSkillVnum))
									{
										if (find_affect(AFFECT_SKILL_NO_BOOK_DELAY))
										{
											remove_affect(AFFECT_SKILL_NO_BOOK_DELAY);
											chat_packet(CHAT_TYPE_INFO, lc_text("주안술서를 통해 주화입마에서 빠져나왔습니다."));
										}
										else
										{
											skill_learn_wait_more_time_message(skill_next_read_time(dwSkillVnum) - get_global_time());
											return false;
										}
									}

									if (point(POINT_HORSE_SKILL) >= 20 || 
											skill_level(SKILL_HORSE_WILDATTACK) + skill_level(SKILL_HORSE_CHARGE) + skill_level(SKILL_HORSE_ESCAPE) >= 60 ||
											skill_level(SKILL_HORSE_WILDATTACK_RANGE) + skill_level(SKILL_HORSE_CHARGE) + skill_level(SKILL_HORSE_ESCAPE) >= 60)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("더 이상 승마 수련서를 읽을 수 없습니다."));
										return false;
									}

									if (number(1, 100) <= iPct)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("승마 수련서를 읽어 승마 스킬 포인트를 얻었습니다."));
										chat_packet(CHAT_TYPE_INFO, lc_text("얻은 포인트로는 승마 스킬의 레벨을 올릴 수 있습니다."));
										point_change(POINT_HORSE_SKILL, 1);

										int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
										if (metin2::config::game().distribution_test_server) iReadDelay /= 3;

										if (!metin2::config::test_server())
											set_skill_next_read_time(dwSkillVnum, get_global_time() + iReadDelay);
									}
									else
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("승마 수련서 이해에 실패하였습니다."));
									}

									item_manager::instance().remove_item(item);
								}
								break;

							case 70102:
							case 70103:
								{
									if (alignment() >= 0)
										return false;

									int delta = std::min(-alignment(), item->value(0));

									metin2::log::info("{}: alignment item, delta {:d}", name(), delta);

									update_alignment(delta);
									item->set_count(item->count() - 1);

									if (delta / 10 > 0)
									{
										chat_packet(CHAT_TYPE_TALKING, lc_text("마음이 맑아지는군. 가슴을 짓누르던 무언가가 좀 가벼워진 느낌이야."));
										chat_packet(CHAT_TYPE_INFO, lc_text("선악치가 %d 증가하였습니다."), delta/10);
									}
								}
								break;

							case 71107:
								{
									int val = item->value(0);
									int interval = item->value(1);
									quest::pc* pPC = quest::quest_manager::instance().get_pc(player_id());
									int last_use_time = pPC->flag("mythical_peach.last_use_time");

									if (get_global_time() - last_use_time < interval * 60 * 60)
									{
										if (metin2::config::test_server() == false)
										{
											chat_packet(CHAT_TYPE_INFO, lc_text("아직 사용할 수 없습니다."));
											return false;
										}
										else
										{
											chat_packet(CHAT_TYPE_INFO, lc_text("테스트 서버 시간제한 통과"));
										}
									}

									if (alignment() == 200000)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("선악치를 더 이상 올릴 수 없습니다."));
										return false;
									}

									if (200000 - alignment() < val * 10)
									{
										val = (200000 - alignment()) / 10;
									}

									int old_alignment = alignment() / 10;

									update_alignment(val*10);

									item->set_count(item->count()-1);
									pPC->set_flag("mythical_peach.last_use_time", get_global_time());

									chat_packet(CHAT_TYPE_TALKING, lc_text("마음이 맑아지는군. 가슴을 짓누르던 무언가가 좀 가벼워진 느낌이야."));
									chat_packet(CHAT_TYPE_INFO, lc_text("선악치가 %d 증가하였습니다."), val);

									char buf[256 + 1];
									snprintf(buf, sizeof(buf), "%d %d", old_alignment, alignment() / 10);
									log_manager::instance().char_log(this, val, "MYTHICAL_PEACH", buf);
								}
								break;

							case 71109:
							case 72719:
								{
									game::items::item * item2;

									if (!is_valid_item_position(dest_cell) || !(item2 = get_item(dest_cell)))
										return false;

									if (item2->is_exchanging() == true)
										return false;

									if (item2->socket_count() == 0)
										return false;

									switch( item2->type() )
									{
										case ITEM_WEAPON:
											break;
										case ITEM_ARMOR:
											switch (item2->sub_type())
											{
											case ARMOR_EAR:
											case ARMOR_WRIST:
											case ARMOR_NECK:
												chat_packet(CHAT_TYPE_INFO, lc_text("빼낼 영석이 없습니다"));
												return false;
											}
											break;

										default:
											return false;
									}

									std::stack<int32_t> socket;

									for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
										socket.push(item2->get_socket(i));

									int idx = ITEM_SOCKET_MAX_NUM - 1;

									while (socket.size() > 0)
									{
										if (socket.top() > 2 && socket.top() != ITEM_BROKEN_METIN_VNUM)
											break;

										idx--;
										socket.pop();
									}

									if (socket.size() == 0)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("빼낼 영석이 없습니다"));
										return false;
									}

									game::items::item * pItemReward = auto_give_item(socket.top());

									if (pItemReward != nullptr)
									{
										item2->set_socket(idx, 1);

										char buf[256+1];
										snprintf(buf, sizeof(buf), "%s(%u) %s(%u)", 
												item2->name(), item2->id(), pItemReward->name(), pItemReward->id());
										log_manager::instance().item_log(this, item, "USE_DETACHMENT_ONE", buf);

										item->set_count(item->count() - 1);
									}
								}
								break;

							case 70201:
							case 70202:
							case 70203:
							case 70204:
							case 70205:
							case 70206:
								{
									if (part(PART_HAIR) >= 1001)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("현재 헤어스타일에서는 염색과 탈색이 불가능합니다."));
									}
									else
									{
										quest::quest_manager& q = quest::quest_manager::instance();
										quest::pc* pPC = q.get_pc(player_id());

										if (pPC)
										{
											int last_dye_level = pPC->flag("dyeing_hair.last_dye_level");

											if (last_dye_level == 0 ||
													last_dye_level+3 <= level() ||
													item->vnum() == 70201)
											{
												set_part(PART_HAIR, item->vnum() - 70201);

												if (item->vnum() == 70201)
													pPC->set_flag("dyeing_hair.last_dye_level", 0);
												else
													pPC->set_flag("dyeing_hair.last_dye_level", level());

												item->set_count(item->count() - 1);
												update_packet();
											}
											else
											{
												chat_packet(CHAT_TYPE_INFO, lc_text("%d 레벨이 되어야 다시 염색하실 수 있습니다."), last_dye_level+3);
											}
										}
									}
								}
								break;

							case ITEM_NEW_YEAR_GREETING_VNUM:
								{
									uint32_t dwBoxVnum = ITEM_NEW_YEAR_GREETING_VNUM;
									std::vector <uint32_t> dwVnums;
									std::vector <uint32_t> dwCounts;
									std::vector <game::items::item *> item_gets;
									int count = 0;

									if (give_item_from_special_item_group(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
									{
										for (int i = 0; i < count; i++)
										{
											if (dwVnums[i] == special_item_group::GOLD)
												chat_packet(CHAT_TYPE_INFO, lc_text("돈 %d 냥을 획득했습니다."), dwCounts[i]);
										}

										item->set_count(item->count() - 1);
									}
								}
								break;

							case ITEM_VALENTINE_ROSE:
							case ITEM_VALENTINE_CHOCOLATE:
								{
									uint32_t dwBoxVnum = item->vnum();
									std::vector <uint32_t> dwVnums;
									std::vector <uint32_t> dwCounts;
									std::vector <game::items::item *> item_gets;
									int count = 0;

									if ((item->vnum() == ITEM_VALENTINE_ROSE && SEX_MALE==get_sex(this)) ||
										(item->vnum() == ITEM_VALENTINE_CHOCOLATE && SEX_FEMALE==get_sex(this)))
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("성별이 맞지않아 이 아이템을 열 수 없습니다."));
										return false;
									}

									if (give_item_from_special_item_group(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
										item->set_count(item->count()-1);
								}
								break;

							case ITEM_WHITEDAY_CANDY:
							case ITEM_WHITEDAY_ROSE:
								{
									uint32_t dwBoxVnum = item->vnum();
									std::vector <uint32_t> dwVnums;
									std::vector <uint32_t> dwCounts;
									std::vector <game::items::item *> item_gets;
									int count = 0;

									if ((item->vnum() == ITEM_WHITEDAY_CANDY && SEX_MALE==get_sex(this)) ||
										(item->vnum() == ITEM_WHITEDAY_ROSE && SEX_FEMALE==get_sex(this)))
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("성별이 맞지않아 이 아이템을 열 수 없습니다."));
										return false;
									}

									if (give_item_from_special_item_group(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
										item->set_count(item->count()-1);
								}
								break;

							case 50011:
								{
									uint32_t dwBoxVnum = 50011;
									std::vector <uint32_t> dwVnums;
									std::vector <uint32_t> dwCounts;
									std::vector <game::items::item *> item_gets;
									int count = 0;

									if (give_item_from_special_item_group(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
									{
										for (int i = 0; i < count; i++)
										{
											char buf[50 + 1];
											snprintf(buf, sizeof(buf), "%u %u", dwVnums[i], dwCounts[i]);
											log_manager::instance().item_log(this, item, "MOONLIGHT_GET", buf);

											item->set_count(item->count() - 1);

											switch (dwVnums[i])
											{
											case special_item_group::GOLD:
												chat_packet(CHAT_TYPE_INFO, lc_text("돈 %d 냥을 획득했습니다."), dwCounts[i]);
												break;

											case special_item_group::EXP:
												chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 부터 신비한 빛이 나옵니다."));
												chat_packet(CHAT_TYPE_INFO, lc_text("%d의 경험치를 획득했습니다."), dwCounts[i]);
												break;

											case special_item_group::MOB:
												chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 몬스터가 나타났습니다!"));
												break;

											case special_item_group::SLOW:
												chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 나온 빨간 연기를 들이마시자 움직이는 속도가 느려졌습니다!"));
												break;

											case special_item_group::DRAIN_HP:
												chat_packet(CHAT_TYPE_INFO, lc_text("상자가 갑자기 폭발하였습니다! 생명력이 감소했습니다."));
												break;

											case special_item_group::POISON:
												chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 나온 녹색 연기를 들이마시자 독이 온몸으로 퍼집니다!"));
												break;

											case special_item_group::MOB_GROUP:
												chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 몬스터가 나타났습니다!"));
												break;

											default:
												if (item_gets[i])
												{
													if (dwCounts[i] > 1)
														chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 %s 가 %d 개 나왔습니다."), item_gets[i]->name(), dwCounts[i]);
													else
														chat_packet(CHAT_TYPE_INFO, lc_text("상자에서 %s 가 나왔습니다."), item_gets[i]->name());
												}
												break;
											}
										}
									}
									else
									{
										chat_packet(CHAT_TYPE_TALKING, lc_text("아무것도 얻을 수 없었습니다."));
										return false;
									}
								}
								break;

							case ITEM_GIVE_STAT_RESET_COUNT_VNUM:
								{
									point_change(POINT_STAT_RESET_COUNT, 1);
									item->set_count(item->count()-1);
								}
								break;

							case 50107:
								{
									effect_packet(SE_CHINA_FIREWORK);
									add_affect(AFFECT_CHINA_FIREWORK, POINT_STUN_PCT, 30, AFF_CHINA_FIREWORK, 5*60, 0, true);
									item->set_count(item->count()-1);
								}
								break;

							case 50108:
								{
									if (arena_manager::instance().is_arena_map(map_index()) == true)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("대련 중에는 이용할 수 없는 물품입니다."));
										return false;
									}

									effect_packet(SE_SPIN_TOP);
									add_affect(AFFECT_CHINA_FIREWORK, POINT_STUN_PCT, 30, AFF_CHINA_FIREWORK, 5*60, 0, true);
									item->set_count(item->count()-1);
								}
								break;

							case ITEM_WONSO_BEAN_VNUM:
								point_change(POINT_HP, max_hp() - hp());
								item->set_count(item->count()-1);
								break;

							case ITEM_WONSO_SUGAR_VNUM:
								point_change(POINT_SP, max_sp() - sp());
								item->set_count(item->count()-1);
								break;

							case ITEM_WONSO_FRUIT_VNUM:
								point_change(POINT_STAMINA, max_stamina()-stamina());
								item->set_count(item->count()-1);
								break;

							case ITEM_ELK_VNUM:
								{
									int iGold = item->get_socket(0);
									item_manager::instance().remove_item(item);
									chat_packet(CHAT_TYPE_INFO, lc_text("돈 %d 냥을 획득했습니다."), iGold);
									point_change(POINT_GOLD, iGold);
								}
								break;

							case 70021:
								{
									int HealPrice = quest::quest_manager::instance().event_flag("MonarchHealGold");
									if (HealPrice == 0)
										HealPrice = 2000000;

									if (monarch::instance().heal_my_empire(this, HealPrice))
									{
										char szNotice[256];
										snprintf(szNotice, sizeof(szNotice), lc_text("군주의 축복으로 이지역 %s 유저는 HP,SP가 모두 채워집니다."), empire_name(empire()));
										send_notice_map(szNotice, map_index(), false);

										chat_packet(CHAT_TYPE_INFO, lc_text("군주의 축복을 사용하였습니다."));
									}
								}
								break;

							case 27995:
								{
								}
								break;

							case 71092 :
								{
									if (chr_target_ != nullptr)
									{
										if (chr_target_->is_polymorphed())
										{
											chr_target_->set_polymorph(0);
											chr_target_->remove_affect(AFFECT_POLYMORPH);
										}
									}
									else
									{
										if (is_polymorphed())
										{
											set_polymorph(0);
											remove_affect(AFFECT_POLYMORPH);
										}
									}
								}
								break;

							case 71051 :
								{

									game::items::item * item2;

									if (!is_valid_item_position(dest_cell) || !(item2 = inventory_item(wDestCell)))
										return false;

									if (item2->is_exchanging() == true)
										return false;

									if (item2->is_equipped() == true)
										return false;

									if (item2->type() == ITEM_COSTUME)
										return false;

									if (item2->attribute_set_index() == -1)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("속성을 변경할 수 없는 아이템입니다."));
										return false;
									}

									if (item2->add_rare_attribute() == true)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("성공적으로 속성이 추가 되었습니다"));

										int iAddedIdx = item2->rare_attr_count() + 4;
										char buf[21];
										snprintf(buf, sizeof(buf), "%u", item2->id());

										log_manager::instance().item_log(
												player_id(),
												item2->attribute_type(iAddedIdx),
												item2->attribute_value(iAddedIdx),
												item->id(),
												"ADD_RARE_ATTR",
												buf,
												get_desc()->host_name(),
												item->original_vnum());

										item->set_count(item->count() - 1);
									}
									else
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("더 이상 이 아이템으로 속성을 추가할 수 없습니다"));
									}
								}
								break;

							case 71052 :
								{

									game::items::item * item2;

									if (!is_valid_item_position(dest_cell) || !(item2 = get_item(dest_cell)))
										return false;

									if (item2->is_exchanging() == true)
										return false;

									if (item2->is_equipped() == true)
										return false;

									if (item2->type() == ITEM_COSTUME)
										return false;

									if (item2->attribute_set_index() == -1)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("속성을 변경할 수 없는 아이템입니다."));
										return false;
									}

									if (item2->change_rare_attribute() == true)
									{
										char buf[21];
										snprintf(buf, sizeof(buf), "%u", item2->id());
										log_manager::instance().item_log(this, item, "CHANGE_RARE_ATTR", buf);

										item->set_count(item->count() - 1);
									}
									else
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("변경 시킬 속성이 없습니다"));
									}
								}
								break;

							case ITEM_AUTO_HP_RECOVERY_S:
							case ITEM_AUTO_HP_RECOVERY_M:
							case ITEM_AUTO_HP_RECOVERY_L:
							case ITEM_AUTO_HP_RECOVERY_X:
							case ITEM_AUTO_SP_RECOVERY_S:
							case ITEM_AUTO_SP_RECOVERY_M:
							case ITEM_AUTO_SP_RECOVERY_L:
							case ITEM_AUTO_SP_RECOVERY_X:
							case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_XS: 
							case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_S: 
							case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_XS: 
							case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_S:
							case FUCKING_BRAZIL_ITEM_AUTO_SP_RECOVERY_S:
							case FUCKING_BRAZIL_ITEM_AUTO_HP_RECOVERY_S:
								{
									if (arena_manager::instance().is_arena_map(map_index()) == true)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("대련장에서 사용하실 수 없습니다."));
										return false;
									}

									affect_types type = AFFECT_NONE;
									bool isSpecialPotion = false;

									switch (item->vnum())
									{
										case ITEM_AUTO_HP_RECOVERY_X:
											isSpecialPotion = true;
											[[fallthrough]];

										case ITEM_AUTO_HP_RECOVERY_S:
										case ITEM_AUTO_HP_RECOVERY_M:
										case ITEM_AUTO_HP_RECOVERY_L:
										case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_XS:
										case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_S:
										case FUCKING_BRAZIL_ITEM_AUTO_HP_RECOVERY_S:
											type = AFFECT_AUTO_HP_RECOVERY;
											break;

										case ITEM_AUTO_SP_RECOVERY_X:
											isSpecialPotion = true;
											[[fallthrough]];

										case ITEM_AUTO_SP_RECOVERY_S:
										case ITEM_AUTO_SP_RECOVERY_M:
										case ITEM_AUTO_SP_RECOVERY_L:
										case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_XS:
										case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_S:
										case FUCKING_BRAZIL_ITEM_AUTO_SP_RECOVERY_S:
											type = AFFECT_AUTO_SP_RECOVERY;
											break;
									}

									if (AFFECT_NONE == type)
										break;

									if (item->count() > 1)
									{
										int pos = empty_inventory(item->size());

										if (-1 == pos)
										{
											chat_packet(CHAT_TYPE_INFO, lc_text("소지품에 빈 공간이 없습니다."));
											break;
										}

										item->set_count( item->count() - 1 );

										game::items::item * item2 = item_manager::instance().create_item( item->vnum(), 1 );
										item2->add_to_character(this, item_pos(INVENTORY, pos));

										if (item->get_socket(1) != 0)
										{
											item2->set_socket(1, item->get_socket(1));
										}

										item = item2;
									}

									affect* pAffect = find_affect( type );

									if (nullptr == pAffect)
									{
										point_types bonus = POINT_NONE;

										if (true == isSpecialPotion)
										{
											if (type == AFFECT_AUTO_HP_RECOVERY)
											{
												bonus = POINT_MAX_HP_PCT;
											}
											else if (type == AFFECT_AUTO_SP_RECOVERY)
											{
												bonus = POINT_MAX_SP_PCT;
											}
										}

										add_affect( type, bonus, 4, item->id(), INFINITE_AFFECT_DURATION, 0, true, false);

										item->lock(true);
										item->set_socket(0, true);

										auto_recovery_item_process( type );
									}
									else
									{
										if (item->id() == pAffect->flag)
										{
											remove_affect( pAffect );

											item->lock(false);
											item->set_socket(0, false);
										}
										else
										{
											game::items::item * old = find_item_by_id( pAffect->flag );

											if (nullptr != old)
											{
												old->lock(false);
												old->set_socket(0, false);
											}

											remove_affect( pAffect );

											point_types bonus = POINT_NONE;

											if (true == isSpecialPotion)
											{
												if (type == AFFECT_AUTO_HP_RECOVERY)
												{
													bonus = POINT_MAX_HP_PCT;
												}
												else if (type == AFFECT_AUTO_SP_RECOVERY)
												{
													bonus = POINT_MAX_SP_PCT;
												}
											}

											add_affect( type, bonus, 4, item->id(), INFINITE_AFFECT_DURATION, 0, true, false);

											item->lock(true);
											item->set_socket(0, true);

											auto_recovery_item_process( type );
										}
									}
								}
								break;
						}
						break;

					case USE_CLEAR:
						{
							remove_bad_affect();
							item->set_count(item->count() - 1);
						}
						break;

					case USE_INVISIBILITY:
						{
							if (item->vnum() == 70026)
							{
								quest::quest_manager& q = quest::quest_manager::instance();
								quest::pc* pPC = q.get_pc(player_id());

								if (pPC != nullptr)
								{
									int last_use_time = pPC->flag("mirror_of_disapper.last_use_time");

									if (get_global_time() - last_use_time < 10*60)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("아직 사용할 수 없습니다."));
										return false;
									}

									pPC->set_flag("mirror_of_disapper.last_use_time", get_global_time());
								}
							}

							add_affect(AFFECT_INVISIBILITY, POINT_NONE, 0, AFF_INVISIBILITY, 300, 0, true);
							item->set_count(item->count() - 1);
						}
						break;

					case USE_POTION_NODELAY:
						{
							if (arena_manager::instance().is_arena_map(map_index()) == true)
							{
								if (quest::quest_manager::instance().event_flag("arena_potion_limit") > 0)
								{
									chat_packet(CHAT_TYPE_INFO, lc_text("대련장에서 사용하실 수 없습니다."));
									return false;
								}

								switch (item->vnum())
								{
									case 70020 :
									case 71018 :
									case 71019 :
									case 71020 :
										if (quest::quest_manager::instance().event_flag("arena_potion_limit_count") < 10000)
										{
											if (potion_limit_ <= 0)
											{
												chat_packet(CHAT_TYPE_INFO, lc_text("사용 제한량을 초과하였습니다."));
												return false;
											}
										}
										break;

									default :
										chat_packet(CHAT_TYPE_INFO, lc_text("대련장에서 사용하실 수 없습니다."));
										return false;
								}
							}

							bool used = false;

							if (item->value(0) != 0)
							{
								if (hp() < max_hp())
								{
									point_change(POINT_HP, item->value(0) * (100 + point(POINT_POTION_BONUS)) / 100);
									effect_packet(SE_HPUP_RED);
									used = true;
								}
							}

							if (item->value(1) != 0)
							{
								if (sp() < max_sp())
								{
									point_change(POINT_SP, item->value(1) * (100 + point(POINT_POTION_BONUS)) / 100);
									effect_packet(SE_SPUP_BLUE);
									used = true;
								}
							}

							if (item->value(3) != 0)
							{
								if (hp() < max_hp())
								{
									point_change(POINT_HP, item->value(3) * max_hp() / 100);
									effect_packet(SE_HPUP_RED);
									used = true;
								}
							}

							if (item->value(4) != 0)
							{
								if (sp() < max_sp())
								{
									point_change(POINT_SP, item->value(4) * max_sp() / 100);
									effect_packet(SE_SPUP_BLUE);
									used = true;
								}
							}

							if (used)
							{
								if (item->vnum() == 50085 || item->vnum() == 50086)
								{
									if (metin2::config::test_server())
										chat_packet(CHAT_TYPE_INFO, lc_text("월병 또는 종자 를 사용하였습니다"));
								}
								if (get_dungeon())
									get_dungeon()->use_potion(this);

								if (get_war_map())
									get_war_map()->use_potion(this, item);

								potion_limit_--;

								item->set_count(item->count() - 1);
							}
						}
						break;

					case USE_POTION:
						if (arena_manager::instance().is_arena_map(map_index()) == true)
						{
							if (quest::quest_manager::instance().event_flag("arena_potion_limit") > 0)
							{
								chat_packet(CHAT_TYPE_INFO, lc_text("대련장에서 사용하실 수 없습니다."));
								return false;
							}

							switch (item->vnum())
							{
								case 27001 :
								case 27002 :
								case 27003 :
								case 27004 :
								case 27005 :
								case 27006 :
									if (quest::quest_manager::instance().event_flag("arena_potion_limit_count") < 10000)
									{
										if (potion_limit_ <= 0)
										{
											chat_packet(CHAT_TYPE_INFO, lc_text("사용 제한량을 초과하였습니다."));
											return false;
										}
									}
									break;

								default :
									chat_packet(CHAT_TYPE_INFO, lc_text("대련장에서 사용하실 수 없습니다."));
									return false;
							}
						}

						if (item->value(1) != 0)
						{
							if (point(POINT_SP_RECOVERY) + sp() >= max_sp())
							{
								return false;
							}

							point_change(POINT_SP_RECOVERY, item->value(1) * std::min(200, (100 + point(POINT_POTION_BONUS))) / 100);
							start_affect_event();
							effect_packet(SE_SPUP_BLUE);
						}

						if (item->value(0) != 0)
						{
							if (point(POINT_HP_RECOVERY) + hp() >= max_hp())
							{
								return false;
							}

							point_change(POINT_HP_RECOVERY, item->value(0) * std::min(200, (100 + point(POINT_POTION_BONUS))) / 100);
							start_affect_event();
							effect_packet(SE_HPUP_RED);
						}

						if (get_dungeon())
							get_dungeon()->use_potion(this);

						if (get_war_map())
							get_war_map()->use_potion(this, item);

						item->set_count(item->count() - 1);
						potion_limit_--;
						break;

					case USE_POTION_CONTINUE:
						{
							if (item->value(0) != 0)
							{
								add_affect(AFFECT_HP_RECOVER_CONTINUE, POINT_HP_RECOVER_CONTINUE, item->value(0), 0, item->value(2), 0, true);
							}
							else if (item->value(1) != 0)
							{
								add_affect(AFFECT_SP_RECOVER_CONTINUE, POINT_SP_RECOVER_CONTINUE, item->value(1), 0, item->value(2), 0, true);
							}
							else
								return false;
						}

						if (get_dungeon())
							get_dungeon()->use_potion(this);

						if (get_war_map())
							get_war_map()->use_potion(this, item);

						item->set_count(item->count() - 1);
						break;

					case USE_ABILITY_UP:
						{
							switch (item->value(0))
							{
								case APPLY_MOV_SPEED:
									add_affect(AFFECT_MOV_SPEED, POINT_MOV_SPEED, item->value(2), AFF_MOV_SPEED_POTION, item->value(1), 0, true);
									break;

								case APPLY_ATT_SPEED:
									add_affect(AFFECT_ATT_SPEED, POINT_ATT_SPEED, item->value(2), AFF_ATT_SPEED_POTION, item->value(1), 0, true);
									break;

								case APPLY_STR:
									add_affect(AFFECT_STR, POINT_ST, item->value(2), 0, item->value(1), 0, true);
									break;

								case APPLY_DEX:
									add_affect(AFFECT_DEX, POINT_DX, item->value(2), 0, item->value(1), 0, true);
									break;

								case APPLY_CON:
									add_affect(AFFECT_CON, POINT_HT, item->value(2), 0, item->value(1), 0, true);
									break;

								case APPLY_INT:
									add_affect(AFFECT_INT, POINT_IQ, item->value(2), 0, item->value(1), 0, true);
									break;

								case APPLY_CAST_SPEED:
									add_affect(AFFECT_CAST_SPEED, POINT_CASTING_SPEED, item->value(2), 0, item->value(1), 0, true);
									break;

								case APPLY_ATT_GRADE_BONUS:
									add_affect(AFFECT_ATT_GRADE, POINT_ATT_GRADE_BONUS, 
											item->value(2), 0, item->value(1), 0, true);
									break;

								case APPLY_DEF_GRADE_BONUS:
									add_affect(AFFECT_DEF_GRADE, POINT_DEF_GRADE_BONUS,
											item->value(2), 0, item->value(1), 0, true);
									break;
							}
						}

						if (get_dungeon())
							get_dungeon()->use_potion(this);

						if (get_war_map())
							get_war_map()->use_potion(this, item);

						item->set_count(item->count() - 1);
						break;

					case USE_TALISMAN:
						{
							const int TOWN_PORTAL	= 1;
							const int MEMORY_PORTAL = 2;

							if (map_index() == 200 || map_index() == 113)
							{
								chat_packet(CHAT_TYPE_INFO, lc_text("현재 위치에서 사용할 수 없습니다."));
								return false;
							}

							if (arena_manager::instance().is_arena_map(map_index()) == true)
							{
								chat_packet(CHAT_TYPE_INFO, lc_text("대련 중에는 이용할 수 없는 물품입니다."));
								return false;
							}

							if (warp_event)
							{
								chat_packet(CHAT_TYPE_INFO, lc_text("이동할 준비가 되어있음으로 귀환부를 사용할수 없습니다"));
								return false;
							}

							int consumeLife = calculate_consume(this);

							if (consumeLife < 0)
								return false;

							if (item->value(0) == TOWN_PORTAL)
							{
								if (item->get_socket(0) == 0)
								{
									if (!get_dungeon())
										if (!give_recall_item(item))
											return false;

									pixel_position posWarp;

									if (sectree_manager::instance().recall_position_by_empire(map_index(), empire(), posWarp))
									{
										point_change(POINT_HP, -consumeLife, false);

										warp_set(posWarp.x, posWarp.y);
									}
									else
									{
										metin2::log::error("Cannot find spawn position for {} near {:d} x {:d}", name(), x(), y());
									}
								}
								else
								{
									if (metin2::config::test_server())
										chat_packet(CHAT_TYPE_INFO, lc_text("원래 위치로 복귀"));	

									process_recall_item(item);
								}
							}
							else if (item->value(0) == MEMORY_PORTAL)
							{
								if (item->get_socket(0) == 0)
								{
									if (get_dungeon())
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("던전 안에서는 %s%s 사용할 수 없습니다."),
												item->name(),
												"");
										return false;
									}

									if (!give_recall_item(item))
										return false;
								}
								else
								{
									point_change(POINT_HP, -consumeLife, false);

									process_recall_item(item);
								}
							}
						}
						break;

					case USE_TUNING:
					case USE_DETACHMENT:
						{
							game::items::item * item2;

							if (!is_valid_item_position(dest_cell) || !(item2 = get_item(dest_cell)))
								return false;

							if (item2->is_exchanging())
								return false;

							if (item2->is_equipped())
								return false;

							if (item2->vnum() >= 28330 && item2->vnum() <= 28343)
							{
								chat_packet(CHAT_TYPE_INFO, lc_text("+3 영석은 이 아이템으로 개량할 수 없습니다"));
								return false;
							}

							if (item2->vnum() >= 28430 && item2->vnum() <= 28443)
							{
								if (item->vnum() == 71056)
								{
									refine_item(item, item2);
								}
								else
								{
									chat_packet(CHAT_TYPE_INFO, lc_text("영석은 이 아이템으로 개량할 수 없습니다"));
								}
							}
							else
							{
								refine_item(item, item2);
							}
						}
						break;

					case USE_PUT_INTO_BELT_SOCKET:
					case USE_PUT_INTO_RING_SOCKET:
					case USE_PUT_INTO_ACCESSORY_SOCKET:
					case USE_ADD_ACCESSORY_SOCKET:
					case USE_CLEAN_SOCKET:
					case USE_CHANGE_ATTRIBUTE:
					case USE_CHANGE_ATTRIBUTE2 :
					case USE_ADD_ATTRIBUTE:
					case USE_ADD_ATTRIBUTE2:
						{
							game::items::item * item2;
							if (!is_valid_item_position(dest_cell) || !(item2 = get_item(dest_cell)))
								return false;

							if (item2->is_equipped())
							{
								buff_on_attr_remove_buffs_from_item(item2);
							}

							if (ITEM_COSTUME == item2->type())
							{
								chat_packet(CHAT_TYPE_INFO, lc_text("속성을 변경할 수 없는 아이템입니다."));
								return false;
							}

							if (item2->is_exchanging())
								return false;

							if (item2->is_equipped())
								return false;

							switch (item->sub_type())
							{
								case USE_CLEAN_SOCKET:
									{
										int i;
										for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
										{
											if (item2->get_socket(i) == ITEM_BROKEN_METIN_VNUM)
												break;
										}

										if (i == ITEM_SOCKET_MAX_NUM)
										{
											chat_packet(CHAT_TYPE_INFO, lc_text("청소할 석이 박혀있지 않습니다."));
											return false;
										}

										int j = 0;

										for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
										{
											if (item2->get_socket(i) != ITEM_BROKEN_METIN_VNUM && item2->get_socket(i) != 0)
												item2->set_socket(j++, item2->get_socket(i));
										}

										for (; j < ITEM_SOCKET_MAX_NUM; ++j)
										{
											if (item2->get_socket(j) > 0)
												item2->set_socket(j, 1);
										}

										{
											char buf[21];
											snprintf(buf, sizeof(buf), "%u", item2->id());
											log_manager::instance().item_log(this, item, "CLEAN_SOCKET", buf);
										}

										item->set_count(item->count() - 1);

									}
									break;

								case USE_CHANGE_ATTRIBUTE :
									if (item2->attribute_set_index() == -1)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("속성을 변경할 수 없는 아이템입니다."));
										return false;
									}

									if (item2->attribute_count() == 0)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("변경할 속성이 없습니다."));
										return false;
									}

									if (GM_PLAYER == gm_level() && false == metin2::config::test_server())
									{

										uint32_t dwChangeItemAttrCycle = quest::quest_manager::instance().event_flag(change_item_attr_cycle_flag_);
										if (dwChangeItemAttrCycle < default_change_item_attr_cycle_)
											dwChangeItemAttrCycle = default_change_item_attr_cycle_;

										quest::pc* pPC = quest::quest_manager::instance().get_pc(player_id());

										if (pPC)
										{
											uint32_t dwNowMin = get_global_time() / 60;


											pPC->set_flag(last_change_item_attr_flag_, dwNowMin);
										}
									}

									if (item->sub_type() == USE_CHANGE_ATTRIBUTE2)
									{
										int aiChangeProb[ITEM_ATTRIBUTE_MAX_LEVEL] = 
										{
											0, 0, 30, 40, 3
										};

										item2->change_attribute(aiChangeProb);
									}
									else if (item->vnum() == 76014)
									{
										int aiChangeProb[ITEM_ATTRIBUTE_MAX_LEVEL] = 
										{
											0, 10, 50, 39, 1
										};

										item2->change_attribute(aiChangeProb);
									}

									else
									{
										if (item->vnum() == 71151 || item->vnum() == 76023)
										{
											if ((item2->type() == ITEM_WEAPON)
												|| (item2->type() == ITEM_ARMOR && item2->sub_type() == ARMOR_BODY))
											{
												bool bCanUse = true;
												for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
												{
													if (item2->limit_type(i) == LIMIT_LEVEL && item2->limit_value(i) > 40)
													{
														bCanUse = false;
														break;
													}
												}
												if (false == bCanUse)
												{
													chat_packet(CHAT_TYPE_INFO, lc_text("적용 레벨보다 높아 사용이 불가능합니다."));
													break;
												}
											}
											else
											{
												chat_packet(CHAT_TYPE_INFO, lc_text("무기와 갑옷에만 사용 가능합니다."));
												break;
											}
										}
										item2->change_attribute();
									}

									chat_packet(CHAT_TYPE_INFO, lc_text("속성을 변경하였습니다."));
									{
										char buf[21];
										snprintf(buf, sizeof(buf), "%u", item2->id());
										log_manager::instance().item_log(this, item, "CHANGE_ATTRIBUTE", buf);
									}

									item->set_count(item->count() - 1);
									break;

								case USE_ADD_ATTRIBUTE :
									if (item2->attribute_set_index() == -1)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("속성을 변경할 수 없는 아이템입니다."));
										return false;
									}

									if (item2->attribute_count() < 4)
									{
										if (item->vnum() == 71152 || item->vnum() == 76024)
										{
											if ((item2->type() == ITEM_WEAPON)
												|| (item2->type() == ITEM_ARMOR && item2->sub_type() == ARMOR_BODY))
											{
												bool bCanUse = true;
												for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
												{
													if (item2->limit_type(i) == LIMIT_LEVEL && item2->limit_value(i) > 40)
													{
														bCanUse = false;
														break;
													}
												}
												if (false == bCanUse)
												{
													chat_packet(CHAT_TYPE_INFO, lc_text("적용 레벨보다 높아 사용이 불가능합니다."));
													break;
												}
											}
											else
											{
												chat_packet(CHAT_TYPE_INFO, lc_text("무기와 갑옷에만 사용 가능합니다."));
												break;
											}
										}
										char buf[21];
										snprintf(buf, sizeof(buf), "%u", item2->id());

										if (number(1, 100) <= item_attribute_add_percent[item2->attribute_count()])
										{
											item2->add_attribute();
											chat_packet(CHAT_TYPE_INFO, lc_text("속성 추가에 성공하였습니다."));

											int iAddedIdx = item2->attribute_count() - 1;
											log_manager::instance().item_log(
													player_id(), 
													item2->attribute_type(iAddedIdx),
													item2->attribute_value(iAddedIdx),
													item->id(), 
													"ADD_ATTRIBUTE_SUCCESS",
													buf,
													get_desc()->host_name(),
													item->original_vnum());
										}
										else
										{
											chat_packet(CHAT_TYPE_INFO, lc_text("속성 추가에 실패하였습니다."));
											log_manager::instance().item_log(this, item, "ADD_ATTRIBUTE_FAIL", buf);
										}

										item->set_count(item->count() - 1);
									}
									else
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("더이상 이 아이템을 이용하여 속성을 추가할 수 없습니다."));
									}
									break;

								case USE_ADD_ATTRIBUTE2 :
									if (item2->attribute_set_index() == -1)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("속성을 변경할 수 없는 아이템입니다."));
										return false;
									}

									if (item2->attribute_count() == 4)
									{
										char buf[21];
										snprintf(buf, sizeof(buf), "%u", item2->id());

										if (number(1, 100) <= item_attribute_add_percent[item2->attribute_count()])
										{
											item2->add_attribute();
											chat_packet(CHAT_TYPE_INFO, lc_text("속성 추가에 성공하였습니다."));

											int iAddedIdx = item2->attribute_count() - 1;
											log_manager::instance().item_log(
													player_id(),
													item2->attribute_type(iAddedIdx),
													item2->attribute_value(iAddedIdx),
													item->id(),
													"ADD_ATTRIBUTE2_SUCCESS",
													buf,
													get_desc()->host_name(),
													item->original_vnum());
										}
										else
										{
											chat_packet(CHAT_TYPE_INFO, lc_text("속성 추가에 실패하였습니다."));
											log_manager::instance().item_log(this, item, "ADD_ATTRIBUTE2_FAIL", buf);
										}

										item->set_count(item->count() - 1);
									}
									else if (item2->attribute_count() == 5)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("더 이상 이 아이템을 이용하여 속성을 추가할 수 없습니다."));
									}
									else if (item2->attribute_count() < 4)
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("먼저 재가비서를 이용하여 속성을 추가시켜 주세요."));
									}
									else
									{
										metin2::log::error("Item has invalid attribute count {:d}", item2->attribute_count());
									}
									break;

								case USE_ADD_ACCESSORY_SOCKET:
									{
										char buf[21];
										snprintf(buf, sizeof(buf), "%u", item2->id());

										if (item2->is_accessory_for_socket())
										{
											if (item2->accessory_socket_max_grade() < ITEM_ACCESSORY_SOCKET_MAX_NUM)
											{
												if (number(1, 100) <= 50)
												{
													item2->set_accessory_socket_max_grade(item2->accessory_socket_max_grade() + 1);
													chat_packet(CHAT_TYPE_INFO, lc_text("소켓이 성공적으로 추가되었습니다."));
													log_manager::instance().item_log(this, item, "ADD_SOCKET_SUCCESS", buf);
												}
												else
												{
													chat_packet(CHAT_TYPE_INFO, lc_text("소켓 추가에 실패하였습니다."));
													log_manager::instance().item_log(this, item, "ADD_SOCKET_FAIL", buf);
												}

												item->set_count(item->count() - 1);
											}
											else
											{
												chat_packet(CHAT_TYPE_INFO, lc_text("이 액세서리에는 더이상 소켓을 추가할 공간이 없습니다."));
											}
										}
										else
										{
											chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템으로 소켓을 추가할 수 없는 아이템입니다."));
										}
									}
									break;

								case USE_PUT_INTO_BELT_SOCKET:
								case USE_PUT_INTO_ACCESSORY_SOCKET:
									if (item2->is_accessory_for_socket() && item->can_put_into(item2))
									{
										char buf[21];
										snprintf(buf, sizeof(buf), "%u", item2->id());

										if (item2->accessory_socket_grade() < item2->accessory_socket_max_grade())
										{
											if (number(1, 100) <= accessory_socket_put_pct[item2->accessory_socket_grade()])
											{
												item2->set_accessory_socket_grade(item2->accessory_socket_grade() + 1);
												chat_packet(CHAT_TYPE_INFO, lc_text("장착에 성공하였습니다."));
												log_manager::instance().item_log(this, item, "PUT_SOCKET_SUCCESS", buf);
											}
											else
											{
												chat_packet(CHAT_TYPE_INFO, lc_text("장착에 실패하였습니다."));
												log_manager::instance().item_log(this, item, "PUT_SOCKET_FAIL", buf);
											}

											item->set_count(item->count() - 1);
										}
										else
										{
											if (item2->accessory_socket_max_grade() == 0)
												chat_packet(CHAT_TYPE_INFO, lc_text("먼저 다이아몬드로 악세서리에 소켓을 추가해야합니다."));
											else if (item2->accessory_socket_max_grade() < ITEM_ACCESSORY_SOCKET_MAX_NUM)
											{
												chat_packet(CHAT_TYPE_INFO, lc_text("이 액세서리에는 더이상 장착할 소켓이 없습니다."));
												chat_packet(CHAT_TYPE_INFO, lc_text("다이아몬드로 소켓을 추가해야합니다."));
											}
											else
												chat_packet(CHAT_TYPE_INFO, lc_text("이 액세서리에는 더이상 보석을 장착할 수 없습니다."));
										}
									}
									else
									{
										chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템을 장착할 수 없습니다."));
									}
									break;
							}
							if (item2->is_equipped())
							{
								buff_on_attr_add_buffs_from_item(item2);
							}
						}
						break;

					case USE_BAIT:
						{

							if (fishing_event)
							{
								chat_packet(CHAT_TYPE_INFO, lc_text("낚시 중에 미끼를 갈아끼울 수 없습니다."));
								return false;
							}

							game::items::item * weapon = wear(WEAR_WEAPON);

							if (!weapon || weapon->type() != ITEM_ROD)
								return false;

							if (weapon->get_socket(2))
							{
								chat_packet(CHAT_TYPE_INFO, lc_text("이미 꽂혀있던 미끼를 빼고 %s를 끼웁니다."), item->name());
							}
							else
							{
								chat_packet(CHAT_TYPE_INFO, lc_text("낚시대에 %s를 미끼로 끼웁니다."), item->name());
							}

							weapon->set_socket(2, item->value(0));
							item->set_count(item->count() - 1);
						}
						break;

					case USE_MOVE:
					case USE_TREASURE_BOX:
					case USE_MONEYBAG:
						break;

					case USE_AFFECT :
						{
							if (find_affect(item->value(0), a_apply_info[item->value(1)].point_type))
							{
								chat_packet(CHAT_TYPE_INFO, lc_text("이미 효과가 걸려 있습니다."));
							}
							else
							{
								if (item->is_pc_bang_item() == true)
								{
									chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템은 PC방에서만 사용할 수 있습니다."));
									return false;
								}

								add_affect(item->value(0), a_apply_info[item->value(1)].point_type, item->value(2), 0, item->value(3), 0, false);
								item->set_count(item->count() - 1);
							}
						}
						break;

					case USE_CREATE_STONE:
						auto_give_item(number(28000, 28013));
						item->set_count(item->count() - 1);
						break;

					case USE_RECIPE :
						{
							game::items::item * pSource1 = find_specify_item(item->value(1));
							uint32_t dwSourceCount1 = item->value(2);

							game::items::item * pSource2 = find_specify_item(item->value(3));
							uint32_t dwSourceCount2 = item->value(4);

							if (dwSourceCount1 != 0)
							{
								if (pSource1 == nullptr)
								{
									chat_packet(CHAT_TYPE_INFO, lc_text("물약 조합을 위한 재료가 부족합니다."));
									return false;
								}
							}

							if (dwSourceCount2 != 0)
							{
								if (pSource2 == nullptr)
								{
									chat_packet(CHAT_TYPE_INFO, lc_text("물약 조합을 위한 재료가 부족합니다."));
									return false;
								}
							}

							if (pSource1 != nullptr)
							{
								if (pSource1->count() < dwSourceCount1)
								{
									chat_packet(CHAT_TYPE_INFO, lc_text("재료(%s)가 부족합니다."), pSource1->name());
									return false;
								}

								pSource1->set_count(pSource1->count() - dwSourceCount1);
							}

							if (pSource2 != nullptr)
							{
								if (pSource2->count() < dwSourceCount2)
								{
									chat_packet(CHAT_TYPE_INFO, lc_text("재료(%s)가 부족합니다."), pSource2->name());
									return false;
								}

								pSource2->set_count(pSource2->count() - dwSourceCount2);
							}

							game::items::item * pBottle = find_specify_item(50901);

							if (!pBottle || pBottle->count() < 1)
							{
								chat_packet(CHAT_TYPE_INFO, lc_text("빈 병이 모자릅니다."));
								return false;
							}

							pBottle->set_count(pBottle->count() - 1);

							if (number(1, 100) > item->value(5))
							{
								chat_packet(CHAT_TYPE_INFO, lc_text("물약 제조에 실패했습니다."));
								return false;
							}

							auto_give_item(item->value(0));
						}
						break;
				}
			}
			break;

		case ITEM_METIN:
			{
				game::items::item * item2;

				if (!is_valid_item_position(dest_cell) || !(item2 = get_item(dest_cell)))
					return false;

				if (item2->is_exchanging())
					return false;

				if (item2->is_equipped())
					return false;

				if (item2->type() == ITEM_PICK) return false;
				if (item2->type() == ITEM_ROD) return false;

				int i;

				for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
				{
					uint32_t dwVnum;   

					if ((dwVnum = item2->get_socket(i)) <= 2)
						continue;

					item_table * p = item_manager::instance().table(dwVnum);

					if (!p)
						continue;

					if (item->value(5) == p->values[5])
					{
						chat_packet(CHAT_TYPE_INFO, lc_text("같은 종류의 메틴석은 여러개 부착할 수 없습니다."));
						return false;
					}
				}

				if (item2->type() == ITEM_ARMOR)
				{
					if (!is_set(item->wear_flag(), WEARABLE_BODY) || !is_set(item2->wear_flag(), WEARABLE_BODY))
					{
						chat_packet(CHAT_TYPE_INFO, lc_text("이 메틴석은 장비에 부착할 수 없습니다."));
						return false;
					}
				}
				else if (item2->type() == ITEM_WEAPON)
				{
					if (!is_set(item->wear_flag(), WEARABLE_WEAPON))
					{
						chat_packet(CHAT_TYPE_INFO, lc_text("이 메틴석은 무기에 부착할 수 없습니다."));
						return false;
					}
				}
				else
				{
					chat_packet(CHAT_TYPE_INFO, lc_text("부착할 수 있는 슬롯이 없습니다."));
					return false;
				}

				for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
					if (item2->get_socket(i) >= 1 && item2->get_socket(i) <= 2 && item2->get_socket(i) >= item->value(2))
					{
						if (number(1, 100) <= 30)
						{
							chat_packet(CHAT_TYPE_INFO, lc_text("메틴석 부착에 성공하였습니다."));
							item2->set_socket(i, item->vnum());
						}
						else
						{
							chat_packet(CHAT_TYPE_INFO, lc_text("메틴석 부착에 실패하였습니다."));
							item2->set_socket(i, ITEM_BROKEN_METIN_VNUM);
						}

						log_manager::instance().item_log(this, item2, "SOCKET", item->name());
						item_manager::instance().remove_item(item, "REMOVE (METIN)");
						break;
					}

				if (i == ITEM_SOCKET_MAX_NUM)
					chat_packet(CHAT_TYPE_INFO, lc_text("부착할 수 있는 슬롯이 없습니다."));
			}
			break;

		case ITEM_AUTOUSE:
		case ITEM_MATERIAL:
		case ITEM_SPECIAL:
		case ITEM_TOOL:
		case ITEM_LOTTERY:
			break;

		case ITEM_TOTEM:
			{
				if (!item->is_equipped())
					equip_item(item);
			}
			break;

		case ITEM_BLEND:
			if (blend_item_find(item->vnum()))
			{
				int		affect_type		= AFFECT_BLEND;
				if (item->get_socket(0) >= static_cast<int32_t>(std::size(a_apply_info)))
				{
					metin2::log::error("Invalid blend item (id {:d}, vnum {:d}): apply type {:d}", item->id(), item->vnum(), item->get_socket(0));
					return false;
				}
				int		apply_type		= a_apply_info[item->get_socket(0)].point_type;
				int		apply_value		= item->get_socket(1);
				int		apply_duration	= item->get_socket(2);

				if (find_affect(affect_type, apply_type))
				{
					chat_packet(CHAT_TYPE_INFO, lc_text("이미 효과가 걸려 있습니다."));
				}
				else
				{
					if (find_affect(AFFECT_EXP_BONUS_EURO_FREE, POINT_RESIST_MAGIC))
					{
						chat_packet(CHAT_TYPE_INFO, lc_text("이미 효과가 걸려 있습니다."));
					}
					else
					{
						add_affect(affect_type, apply_type, apply_value, 0, apply_duration, 0, false);
						item->set_count(item->count() - 1);
					}
				}
			}
			break;
		case ITEM_EXTRACT:
			{
				game::items::item * pDestItem = get_item(dest_cell);
				if (nullptr == pDestItem)
				{
					return false;
				}
				switch (item->sub_type())
				{
				case EXTRACT_DRAGON_SOUL:
					if (pDestItem->is_dragon_soul())
					{
						return ds_manager::instance().pull_out(this, NPOS, pDestItem, item);
					}
					return false;
				case EXTRACT_DRAGON_HEART:
					if (pDestItem->is_dragon_soul())
					{
						return ds_manager::instance().extract_dragon_heart(this, pDestItem, item);
					}
					return false;
				default:
					return false;
				}
			}
			break;

		case ITEM_NONE:
			metin2::log::error("Item {} has type none", item->name());
			break;

		default:
			metin2::log::warn("Item {} has unknown type {:d}", item->name(), item->type());
			return false;
	}

	return true;
}

int portal_limit_time = 10;

auto character::use_item(item_pos cell_value, item_pos dest_cell) -> bool
{
	uint16_t wCell = cell_value.cell;
	uint8_t window_type = cell_value.window_type;
	item * item;

	if (!can_handle_item())
		return false;

	if (!is_valid_item_position(cell_value) || !(item = get_item(cell_value)))
			return false;

	metin2::log::info("{} uses item {} (window {:d}, cell {:d})", name(), item->name(), window_type, wCell);

	if (item->is_exchanging())
		return false;

	if (!item->can_used_by(this))
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("군직이 맞지않아 이 아이템을 사용할 수 없습니다."));
		return false;
	}

	if (is_stun())
		return false;

	if (false == fn_check_item_sex(this, item))
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("성별이 맞지않아 이 아이템을 사용할 수 없습니다."));
		return false;
	}

	if (is_summon_item(item->vnum()))
	{
		if (false == is_summonable_zone(map_index()))
		{
			chat_packet(CHAT_TYPE_INFO, lc_text("사용할수 없습니다."));
			return false;
		}


		if (three_way_war::instance().is_three_way_war_map_index(map_index()))
		{
			chat_packet(CHAT_TYPE_INFO, lc_text("삼거리 전투 참가중에는 귀환부,귀환기억부를 사용할수 없습니다."));
			return false;
		}
		int iPulse = thecore_pulse();

		if (iPulse - safebox_load_time() < passes_per_sec(portal_limit_time))
		{
			chat_packet(CHAT_TYPE_INFO, lc_text("창고를 연후 %d초 이내에는 귀환부,귀환기억부를 사용할 수 없습니다."), portal_limit_time);

			if (metin2::config::test_server())
				chat_packet(CHAT_TYPE_INFO, "[TestOnly]Pulse %d LoadTime %d PASS %d", iPulse, safebox_load_time(), passes_per_sec(portal_limit_time));
			return false; 
		}

		if (get_exchange() || my_shop() || shop_owner() || is_open_safebox() || is_cube_open())
		{
			chat_packet(CHAT_TYPE_INFO, lc_text("거래창,창고 등을 연 상태에서는 귀환부,귀환기억부 를 사용할수 없습니다."));
			return false;
		}

		{
			if (iPulse - get_refine_time() < passes_per_sec(portal_limit_time))
			{
				chat_packet(CHAT_TYPE_INFO, lc_text("아이템 개량후 %d초 이내에는 귀환부,귀환기억부를 사용할 수 없습니다."), portal_limit_time);
				return false;
			}
		}

		{
			if (iPulse - get_my_shop_time() < passes_per_sec(portal_limit_time))
			{
				chat_packet(CHAT_TYPE_INFO, lc_text("개인상점 사용후 %d초 이내에는 귀환부,귀환기억부를 사용할 수 없습니다."), portal_limit_time);
				return false;
			}

		}

		if (item->vnum() != 70302)
		{
			pixel_position posWarp;

			int x_value = 0;
			int y_value = 0;

			double nDist = 0;
			const double nDistant = 5000.0;
			if (item->vnum() == 22010)
			{
				x_value = item->get_socket(0) - x();
				y_value = item->get_socket(1) - y();
			}
			else if (item->vnum() == 22000) 
			{
				sectree_manager::instance().recall_position_by_empire(map_index(), empire(), posWarp);

				if (item->get_socket(0) == 0)
				{
					x_value = posWarp.x - x();
					y_value = posWarp.y - y();
				}
				else
				{
					x_value = item->get_socket(0) - x();
					y_value = item->get_socket(1) - y();
				}
			}

			nDist = sqrt(pow((float)x_value,2) + pow((float)y_value,2));

			if (nDistant > nDist)
			{
				chat_packet(CHAT_TYPE_INFO, lc_text("이동 되어질 위치와 너무 가까워 귀환부를 사용할수 없습니다."));				
				if (metin2::config::test_server())
					chat_packet(CHAT_TYPE_INFO, "PossibleDistant %f nNowDist %f", nDistant,nDist); 
				return false;
			}
		}

		if (iPulse - get_exchange_time()  < passes_per_sec(portal_limit_time))
		{
			chat_packet(CHAT_TYPE_INFO, lc_text("거래 후 %d초 이내에는 귀환부,귀환기억부등을 사용할 수 없습니다."), portal_limit_time);
			return false;
		}

	}

	if (item->vnum() == 50200 || item->vnum() == 71049)
	{
		if (get_exchange() || my_shop() || shop_owner() || is_open_safebox() || is_cube_open())
		{
			chat_packet(CHAT_TYPE_INFO, lc_text("거래창,창고 등을 연 상태에서는 보따리,비단보따리를 사용할수 없습니다."));
			return false;
		}

	}

	if (is_set(item->flag(), ITEM_FLAG_LOG))
	{
		uint32_t vid = item->get_vid();
		uint32_t oldCount = item->count();
		uint32_t vnum = item->vnum();

		char hint[PROTO_NAME_MAX_LEN + 32 + 1];
		int len = snprintf(hint, sizeof(hint) - 32, "%s", item->name());

		if (len < 0 || len >= (int) sizeof(hint) - 32)
			len = (sizeof(hint) - 32) - 1;

		bool ret = use_item_ex(item, dest_cell);

		if (nullptr == item_manager::instance().find_by_vid(vid))
		{
			log_manager::instance().item_log(this, vid, vnum, "REMOVE", hint);
		}
		else if (oldCount != item->count())
		{
			snprintf(hint + len, sizeof(hint) - len, " %u", oldCount - 1);
			log_manager::instance().item_log(this, vid, vnum, "USE_ITEM", hint);
		}
		return (ret);
	}
	else
		return use_item_ex(item, dest_cell);
}

auto character::drop_item(item_pos cell_value, uint8_t count_value) -> bool
{
	item * item = nullptr; 

	if (!can_handle_item())
	{
		if (nullptr != dragon_soul_refine_window_get_opener())
			chat_packet(CHAT_TYPE_INFO, lc_text("강화창을 연 상태에서는 아이템을 옮길 수 없습니다."));
		return false;
	}

	if (is_dead())
		return false;

	if (!is_valid_item_position(cell_value) || !(item = get_item(cell_value)))
		return false;

	if (item->is_exchanging())
		return false;

	if (true == item->is_locked())
		return false;

	if (quest::quest_manager::instance().pc_force(player_id())->is_running() == true)
		return false;

	if (is_set(item->anti_flag(), ITEM_ANTIFLAG_DROP | ITEM_ANTIFLAG_GIVE))
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("버릴 수 없는 아이템입니다."));
		return false;
	}

	if (count_value == 0 || count_value > item->count())
		count_value = item->count();

	sync_quickslot(QUICKSLOT_TYPE_ITEM, cell_value.cell, 255);

	game::items::item * pkItemToDrop;

	if (count_value == item->count())
	{
		item->remove_from_character();
		pkItemToDrop = item;
	}
	else
	{
		if (count_value == 0)
		{
			if (metin2::config::test_server())
				metin2::log::debug("Drop item: count is 0");
			return false;
		}


		item->set_count(item->count() - count_value);
		item_manager::instance().flush_delayed_save(item);

		pkItemToDrop = item_manager::instance().create_item(item->vnum(), count_value);

		fn_copy_item_socket(pkItemToDrop, item);

		char szBuf[51 + 1];
		snprintf(szBuf, sizeof(szBuf), "%u %u", pkItemToDrop->id(), pkItemToDrop->count());
		log_manager::instance().item_log(this, item, "ITEM_SPLIT", szBuf);
	}

	pixel_position pxPos = xyz();

	if (thecore_pulse() > last_drop_time + 25)
		count_drops = 0;

	if (thecore_pulse() < last_drop_time + 25 && count_drops >= 4)
	{
		count_drops = 0;
		metin2::log::warn("{} (pid {:d}) disconnected for drop hacking", name(), player_id());
		get_desc()->set_phase(PHASE_CLOSE);
		return false;
	}

	if (pkItemToDrop->add_to_ground(map_index(), pxPos))
	{

		chat_packet(CHAT_TYPE_INFO, lc_text("떨어진 아이템은 3분 후 사라집니다."));
		pkItemToDrop->start_destroy_event();

		item_manager::instance().flush_delayed_save(pkItemToDrop);

		char szHint[32 + 1];
		snprintf(szHint, sizeof(szHint), "%s %u %u", pkItemToDrop->name(), pkItemToDrop->count(), pkItemToDrop->original_vnum());
		log_manager::instance().item_log(this, pkItemToDrop, "DROP", szHint);

		last_drop_time = thecore_pulse();
		count_drops++;

	}

	return true;
}

auto character::drop_gold(int gold_value) -> bool
{
	if (gold_value <= 0 || gold_value > gold())
		return false;

	if (!can_handle_item())
		return false;

	if (0 != gold_drop_time_limit_value)
	{
		if (get_dword_time() < last_gold_drop_time_+gold_drop_time_limit_value)
		{
			chat_packet(CHAT_TYPE_INFO, lc_text("아직 골드를 버릴 수 없습니다."));
			return false;
		}
	}

	last_gold_drop_time_ = get_dword_time();

	item * item = item_manager::instance().create_item(1, gold_value);

	if (item)
	{
		pixel_position pos = xyz();

		if (item->add_to_ground(map_index(), pos))
		{
			point_change(POINT_GOLD, -gold_value, true);

			if (gold_value > 1000)
				log_manager::instance().char_log(this, gold_value, "DROP_GOLD", "");

			item->start_destroy_event(150);
			chat_packet(CHAT_TYPE_INFO, lc_text("떨어진 아이템은 %d분 후 사라집니다."), 150/60);
		}

		save();
		return true;
	}

	return false;
}

auto character::move_item(item_pos cell_value, item_pos dest_cell, uint8_t count) -> bool
{
	if (cell_value.is_same_position(dest_cell))
		return false;

	item * item = nullptr;

	if (!is_valid_item_position(cell_value))
		return false;

	if (!(item = get_item(cell_value)))
		return false;

	if (item->is_exchanging())
		return false;

	if (item->count() < count)
		return false;

	if (INVENTORY == cell_value.window_type && cell_value.cell >= INVENTORY_MAX_NUM && is_set(item->flag(), ITEM_FLAG_IRREMOVABLE))
		return false;

	if (true == item->is_locked())
		return false;

	if (!is_valid_item_position(dest_cell))
	{
		return false;
	}

	if (!can_handle_item())
	{
		if (nullptr != dragon_soul_refine_window_get_opener())
			chat_packet(CHAT_TYPE_INFO, lc_text("강화창을 연 상태에서는 아이템을 옮길 수 없습니다."));
		return false;
	}

	if (dest_cell.is_belt_inventory_position() && false == belt_inventory_helper::can_move_into_belt_inventory(item))
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템은 벨트 인벤토리로 옮길 수 없습니다."));			
		return false;
	}

	if (cell_value.is_equip_position() && !can_unequip_now(item))
		return false;

	if (dest_cell.is_equip_position())
	{
		if (get_item(dest_cell))
		{
			chat_packet(CHAT_TYPE_INFO, lc_text("이미 장비를 착용하고 있습니다."));

			return false;
		}

		equip_item(item, dest_cell.cell - INVENTORY_MAX_NUM);
	}
	else
	{
		if (item->is_dragon_soul())
		{
			if (item->is_equipped())
			{
				return ds_manager::instance().pull_out(this, dest_cell, item);
			}
			else
			{
				if (dest_cell.window_type != DRAGON_SOUL_INVENTORY)
				{
					return false;
				}

				if (!ds_manager::instance().is_valid_cell_for_this_item(item, dest_cell))
					return false;
			}
		}
		else if (DRAGON_SOUL_INVENTORY == dest_cell.window_type)
			return false;

		game::items::item * item2;

		if ((item2 = get_item(dest_cell)) && item != item2 && item2->is_stackable() &&
				!is_set(item2->anti_flag(), ITEM_ANTIFLAG_STACK) &&
				item2->vnum() == item->vnum())
		{
			for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
				if (item2->get_socket(i) != item->get_socket(i))
					return false;

			if (count == 0)
				count = item->count();

			metin2::log::debug("{}: stack {} from window {:d} cell {:d} to window {:d} cell {:d}, count {:d}", name(), item->name(), cell_value.window_type, cell_value.cell, 
				dest_cell.window_type, dest_cell.cell, count);

			count = std::min<int>(200 - item2->count(), count);

			item->set_count(item->count() - count);
			item2->set_count(item2->count() + count);
			return true;
		}

		if (!is_empty_item_grid(dest_cell, item->size(), cell_value.cell))
			return false;

		if (count == 0 || count >= item->count() || !item->is_stackable() || is_set(item->anti_flag(), ITEM_ANTIFLAG_STACK))
		{
			metin2::log::debug("{}: move {} from window {:d} cell {:d} to window {:d} cell {:d}, count {:d}", name(), item->name(), cell_value.window_type, cell_value.cell, 
				dest_cell.window_type, dest_cell.cell, count);

			item->remove_from_character();
			set_item(dest_cell, item);

			if (INVENTORY == cell_value.window_type && INVENTORY == dest_cell.window_type)
				sync_quickslot(QUICKSLOT_TYPE_ITEM, cell_value.cell, dest_cell.cell);
		}
		else if (count < item->count())
		{

			metin2::log::debug("{}: split {} from window {:d} cell {:d} to window {:d} cell {:d}, count {:d}", name(), item->name(), cell_value.window_type, cell_value.cell, 
				dest_cell.window_type, dest_cell.cell, count);

			item->set_count(item->count() - count);
			game::items::item * item2 = item_manager::instance().create_item(item->vnum(), count);

			fn_copy_item_socket(item2, item);

			item2->add_to_character(this, dest_cell);

			char szBuf[51+1];
			snprintf(szBuf, sizeof(szBuf), "%u %u %u %u ", item2->id(), item2->count(), item->count(), item->count() + item2->count());
			log_manager::instance().item_log(this, item, "ITEM_SPLIT", szBuf);
		}
	}

	return true;
}

namespace party_pickup_distribute
{
	struct f_find_ownership
	{
		game::items::item * item;
		character * owner;

		f_find_ownership(game::items::item * item) 
			: item(item), owner(nullptr)
		{
		}

		void operator () (character * ch)
		{
			if (item->is_ownership(ch))
				owner = ch;
		}
	};

	struct f_count_near_member
	{
		int		total;
		int		x, y;

		f_count_near_member(character * center )
			: total(0), x(center->x()), y(center->y())
		{
		}

		void operator () (character * ch)
		{
			if (distance_approx(ch->x() - x, ch->y() - y) <= PARTY_DEFAULT_RANGE)
				total += 1;
		}
	};

	struct f_money_distributor
	{
		int		total;
		character *	c;
		int		x, y;
		int		money;

		f_money_distributor(character * center, int money_value) 
			: total(0), c(center), x(center->x()), y(center->y()), money(money_value) 
		{
		}

		void operator ()(character * ch)
		{
			if (ch!=c)
				if (distance_approx(ch->x() - x, ch->y() - y) <= PARTY_DEFAULT_RANGE)
				{
					ch->point_change(POINT_GOLD, money, true);

					if (money > 1000)
						log_manager::instance().char_log(ch, money, "GET_GOLD", "");
				}
		}
	};
}

void character::give_gold(int amount)
{
	if (amount <= 0)
		return;

	metin2::log::info("{} receives {:d} gold", name(), amount);

	if (get_party())
	{
		party * pParty = get_party();

		uint32_t dwTotal = amount;
		uint32_t dwMyAmount = dwTotal;

		party_pickup_distribute::f_count_near_member funcCountNearMember(this);
		pParty->for_each_online_member(funcCountNearMember);

		if (funcCountNearMember.total > 1)
		{
			uint32_t dwShare = dwTotal / funcCountNearMember.total;
			dwMyAmount -= dwShare * (funcCountNearMember.total - 1);

			party_pickup_distribute::f_money_distributor funcMoneyDist(this, dwShare);

			pParty->for_each_online_member(funcMoneyDist);
		}

		point_change(POINT_GOLD, dwMyAmount, true);

		if (dwMyAmount > 1000)
			log_manager::instance().char_log(this, dwMyAmount, "GET_GOLD", "");
	}
	else
	{
		point_change(POINT_GOLD, amount, true);

		if (amount > 1000)
			log_manager::instance().char_log(this, amount, "GET_GOLD", "");
	}
}

auto character::pickup_item(uint32_t vid) -> bool
{
	item * item = item_manager::instance().find_by_vid(vid);

	if (is_observer_mode())
		return false;

	if (!item || !item->get_sectree())
		return false;

	if (item->distance_valid(this))
	{
		if (item->is_ownership(this))
		{
			if (item->type() == ITEM_ELK)
			{
				give_gold(item->count());
				item->remove_from_ground();

				item_manager::instance().destroy_item(item);

				save();
			}
			else
			{
				if (item->is_stackable() && !is_set(item->anti_flag(), ITEM_ANTIFLAG_STACK))
				{
					uint8_t bCount = item->count();

					for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
					{
						game::items::item * item2 = inventory_item(i);

						if (!item2)
							continue;

						if (item2->vnum() == item->vnum())
						{
							int j;

							for (j = 0; j < ITEM_SOCKET_MAX_NUM; ++j)
								if (item2->get_socket(j) != item->get_socket(j))
									break;

							if (j != ITEM_SOCKET_MAX_NUM)
								continue;

							uint8_t bCount2 = std::min<int>(200 - item2->count(), bCount);
							bCount -= bCount2;

							item2->set_count(item2->count() + bCount2);

							if (bCount == 0)
							{
								chat_packet(CHAT_TYPE_INFO, lc_text("아이템 획득: %s"), item2->name());
								item_manager::instance().destroy_item(item);
								if (item2->type() == ITEM_QUEST)
									quest::quest_manager::instance().pickup_item (player_id(), item2);
								return true;
							}
						}
					}

					item->set_count(bCount);
				}

				int iEmptyCell;
				if (item->is_dragon_soul())
				{
					if ((iEmptyCell = empty_dragon_soul_inventory(item)) == -1)
					{
						metin2::log::debug("No free dragon soul inventory slot for pid {:d} (item size {:d}, id {:d})", player_id(), item->size(), item->id());
						chat_packet(CHAT_TYPE_INFO, lc_text("소지하고 있는 아이템이 너무 많습니다."));
						return false;
					}
				}
				else
				{
					if ((iEmptyCell = empty_inventory(item->size())) == -1)
					{
						metin2::log::debug("No free inventory slot for pid {:d} (item size {:d}, id {:d})", player_id(), item->size(), item->id());
						chat_packet(CHAT_TYPE_INFO, lc_text("소지하고 있는 아이템이 너무 많습니다."));
						return false;
					}
				}

				item->remove_from_ground();

				if (item->is_dragon_soul())
					item->add_to_character(this, item_pos(DRAGON_SOUL_INVENTORY, iEmptyCell));
				else
					item->add_to_character(this, item_pos(INVENTORY, iEmptyCell));

				char szHint[32+1];
				snprintf(szHint, sizeof(szHint), "%s %u %u", item->name(), item->count(), item->original_vnum());
				log_manager::instance().item_log(this, item, "GET", szHint);
				chat_packet(CHAT_TYPE_INFO, lc_text("아이템 획득: %s"), item->name());

				if (item->type() == ITEM_QUEST)
					quest::quest_manager::instance().pickup_item (player_id(), item);
			}

			return true;
		}
		else if (!is_set(item->anti_flag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_DROP) && get_party())
		{
			party_pickup_distribute::f_find_ownership funcFindOwnership(item);

			get_party()->for_each_online_member(funcFindOwnership);

			character * owner = funcFindOwnership.owner;

			int iEmptyCell;

			if (item->is_dragon_soul())
			{
				if (!(owner && (iEmptyCell = owner->empty_dragon_soul_inventory(item)) != -1))
				{
					owner = this;

					if ((iEmptyCell = empty_dragon_soul_inventory(item)) == -1)
					{
						owner->chat_packet(CHAT_TYPE_INFO, lc_text("소지하고 있는 아이템이 너무 많습니다."));
						return false;
					}
				}
			}
			else
			{
				if (!(owner && (iEmptyCell = owner->empty_inventory(item->size())) != -1))
				{
					owner = this;

					if ((iEmptyCell = empty_inventory(item->size())) == -1)
					{
						owner->chat_packet(CHAT_TYPE_INFO, lc_text("소지하고 있는 아이템이 너무 많습니다."));
						return false;
					}
				}
			}

			item->remove_from_ground();

			if (item->is_dragon_soul())
				item->add_to_character(owner, item_pos(DRAGON_SOUL_INVENTORY, iEmptyCell));
			else
				item->add_to_character(owner, item_pos(INVENTORY, iEmptyCell));

			char szHint[32+1];
			snprintf(szHint, sizeof(szHint), "%s %u %u", item->name(), item->count(), item->original_vnum());
			log_manager::instance().item_log(owner, item, "GET", szHint);

			if (owner == this)
				chat_packet(CHAT_TYPE_INFO, lc_text("아이템 획득: %s"), item->name());
			else
			{
				owner->chat_packet(CHAT_TYPE_INFO, lc_text("아이템 획득: %s 님으로부터 %s"), name(), item->name());
				chat_packet(CHAT_TYPE_INFO, lc_text("아이템 전달: %s 님에게 %s"), owner->name(), item->name());
			}

			if (item->type() == ITEM_QUEST)
				quest::quest_manager::instance().pickup_item (owner->player_id(), item);

			return true;
		}
	}

	return false;
}

auto character::swap_item(uint8_t cell_value, uint8_t dest_cell) -> bool
{
	if (!can_handle_item())
		return false;

	item_pos srcCell(INVENTORY, cell_value), destCell(INVENTORY, dest_cell);

	if (srcCell.is_dragon_soul_equip_position() || destCell.is_dragon_soul_equip_position())
		return false;

	if (cell_value == dest_cell)
		return false;

	if (srcCell.is_equip_position() && destCell.is_equip_position())
		return false;

	item * item1;
	item * item2;

	if (srcCell.is_equip_position())
	{
		item1 = inventory_item(dest_cell);
		item2 = inventory_item(cell_value);
	}
	else
	{
		item1 = inventory_item(cell_value);
		item2 = inventory_item(dest_cell);
	}

	if (!item1 || !item2)
		return false;

	if (item1 == item2)
	{
	    metin2::log::warn("Possible hack by {}: swapping an item with itself (cells {:d}, {:d})", name_.c_str(), cell_value, dest_cell);
	    return false;
	}

	if (!is_empty_item_grid(item_pos (INVENTORY, item1->cell()), item2->size(), item1->cell()))
		return false;

	if (item_pos(EQUIPMENT, item2->cell()).is_equip_position())
	{
		uint8_t bEquipCell = item2->cell() - INVENTORY_MAX_NUM;
		uint8_t bInvenCell = item1->cell();

		if (false == can_unequip_now(item2) || false == can_equip_now(item1))
			return false;

		if (bEquipCell != item1->find_equip_cell(this))
			return false;

		item2->remove_from_character();

		if (item1->equip_to(this, bEquipCell))
			item2->add_to_character(this, item_pos(INVENTORY, bInvenCell));
		else
			metin2::log::error("Cannot equip {} when swapping with {}", item2->name(), item1->name());
	}
	else
	{
		uint8_t bCell1 = item1->cell();
		uint8_t bCell2 = item2->cell();

		item1->remove_from_character();
		item2->remove_from_character();

		item1->add_to_character(this, item_pos(INVENTORY, bCell2));
		item2->add_to_character(this, item_pos(INVENTORY, bCell1));
	}

	return true;
}

auto character::unequip_item(item * item) -> bool
{
	int pos;

	if (false == can_unequip_now(item))
		return false;

	if (item->is_dragon_soul())
		pos = empty_dragon_soul_inventory(item);
	else
		pos = empty_inventory(item->size());

	if (item->vnum() == UNIQUE_ITEM_HIDE_ALIGNMENT_TITLE)
		show_alignment(true);

	item->remove_from_character();
	if (item->is_dragon_soul())
	{
		item->add_to_character(this, item_pos(DRAGON_SOUL_INVENTORY, pos));
	}
	else
		item->add_to_character(this, item_pos(INVENTORY, pos));

	check_maximum_points();

	return true;
}

auto character::equip_item(item * item, int candidate_cell) -> bool
{
	if (item->is_exchanging())
		return false;

	if (false == item->is_equipable())
		return false;

	if (false == can_equip_now(item))
		return false;

	int iWearCell = item->find_equip_cell(this, candidate_cell);

	if (iWearCell < 0)
		return false;

	if (iWearCell == WEAR_BODY && is_riding() && (item->vnum() >= 11901 && item->vnum() <= 11904))
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("말을 탄 상태에서 예복을 입을 수 없습니다."));
		return false;
	}

	if (iWearCell != WEAR_ARROW && is_polymorphed())
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("둔갑 중에는 착용중인 장비를 변경할 수 없습니다."));
		return false;
	}

	if (fn_check_item_sex(this, item) == false)
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("성별이 맞지않아 이 아이템을 사용할 수 없습니다."));
		return false;
	}

	if(item->is_ride_item() && is_riding())
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("이미 탈것을 이용중입니다."));
		return false;
	}

	uint32_t dwCurTime = get_dword_time();

	if (iWearCell != WEAR_ARROW 
		&& (dwCurTime - last_attack_time() <= 1500 || dwCurTime - last_skill_time_ <= 1500))
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("가만히 있을 때만 착용할 수 있습니다."));
		return false;
	}

	if (item->is_dragon_soul())
	{
		if(inventory_item(INVENTORY_MAX_NUM + iWearCell))
		{
			chat_packet(CHAT_TYPE_INFO, "이미 같은 종류의 용혼석을 착용하고 있습니다.");
			return false;
		}

		if (!item->equip_to(this, iWearCell))
		{
			return false;
		}
	}
	else
	{
		if (wear(iWearCell) && !is_set(wear(iWearCell)->flag(), ITEM_FLAG_IRREMOVABLE))
		{
			if (item->wear_flag() == WEARABLE_ABILITY) 
				return false;

			if (false == swap_item(item->cell(), INVENTORY_MAX_NUM + iWearCell))
			{
				return false;
			}
		}
		else
		{
			uint8_t bOldCell = item->cell();

			if (item->equip_to(this, iWearCell))
			{
				sync_quickslot(QUICKSLOT_TYPE_ITEM, bOldCell, iWearCell);
			}
		}
	}

	if (true == item->is_equipped())
	{
		if (-1 != item->proto()->c_limit_real_time_first_use_index)
		{
			if (0 == item->get_socket(1))
			{
				int32_t duration = (0 != item->get_socket(0)) ? item->get_socket(0) : item->proto()->limits[static_cast<int>(item->proto()->c_limit_real_time_first_use_index)].value;

				if (0 == duration)
					duration = 60 * 60 * 24 * 7;

				item->set_socket(0, time(nullptr) + duration);
				item->start_real_time_expire_event();
			}

			item->set_socket(1, item->get_socket(1) + 1);
		}

		if (item->vnum() == UNIQUE_ITEM_HIDE_ALIGNMENT_TITLE)
			show_alignment(false);

		const uint32_t& dwVnum = item->vnum();

		if (true == item_vnum_helper::is_ramadan_moon_ring(dwVnum))
		{
			this->effect_packet(SE_EQUIP_RAMADAN_RING);
		}
		else if (true == item_vnum_helper::is_halloween_candy(dwVnum))
		{
			this->effect_packet(SE_EQUIP_HALLOWEEN_CANDY);
		}
		else if (true == item_vnum_helper::is_happiness_ring(dwVnum))
		{
			this->effect_packet(SE_EQUIP_HAPPINESS_RING);
		}
		else if (true == item_vnum_helper::is_love_pendant(dwVnum))
		{
			this->effect_packet(SE_EQUIP_LOVE_PENDANT);
		}
		else if (ITEM_UNIQUE == item->type() && 0 != item->sig_vnum())
		{
			const special_item_group* pGroup = item_manager::instance().get_special_item_group(item->sig_vnum());
			if (nullptr != pGroup)
			{
				const special_attr_group* pAttrGroup = item_manager::instance().get_special_attr_group(pGroup->attr_vnum(item->vnum()));
				if (nullptr != pAttrGroup)
				{
					const std::string& std = pAttrGroup->effect_file_name;
					specific_effect_packet(std.c_str());
				}
			}
		}

		if (UNIQUE_SPECIAL_RIDE == item->sub_type() && is_set(item->flag(), ITEM_FLAG_QUEST_USE))
		{
			quest::quest_manager::instance().use_item(player_id(), item, false);
		}
	}

	return true;
}

void character::buff_on_attr_add_buffs_from_item(item * item_value)
{
	for (size_t i = 0; i < std::size(buff_on_attr_points); i++)
	{
		map_buff_on_attrs::iterator it = map_buff_on_attrs_.find(buff_on_attr_points[i]);
		if (it != map_buff_on_attrs_.end())
		{
			it->second->add_buff_from_item(item_value);
		}
	}
}

void character::buff_on_attr_remove_buffs_from_item(item * item_value)
{
	for (size_t i = 0; i < std::size(buff_on_attr_points); i++)
	{
		map_buff_on_attrs::iterator it = map_buff_on_attrs_.find(buff_on_attr_points[i]);
		if (it != map_buff_on_attrs_.end())
		{
			it->second->remove_buff_from_item(item_value);
		}
	}
}

void character::buff_on_attr_clear_all()
{
	for (map_buff_on_attrs::iterator it = map_buff_on_attrs_.begin(); it != map_buff_on_attrs_.end(); it++)
	{
		buff_on_attributes* pBuff = it->second.get();
		if (pBuff)
		{
			pBuff->initialize();
		}
	}
}

void character::buff_on_attr_value_change(uint8_t type, uint8_t old_value, uint8_t new_value)
{
	map_buff_on_attrs::iterator it = map_buff_on_attrs_.find(type);

	if (0 == new_value)
	{
		if (map_buff_on_attrs_.end() == it)
			return;
		else
			it->second->off();
	}
	else if(0 == old_value)
	{
		buff_on_attributes* pBuff;
		if (map_buff_on_attrs_.end() == it)
		{
			std::unique_ptr<buff_on_attributes> pNewBuff;
			switch (type)
			{
			case POINT_ENERGY:
				{
					static uint8_t abSlot[] = { WEAR_BODY, WEAR_HEAD, WEAR_FOOTS, WEAR_WRIST, WEAR_WEAPON, WEAR_NECK, WEAR_EAR, WEAR_SHIELD };
					static std::vector <uint8_t> vec_slots (abSlot, abSlot + std::size(abSlot));
					pNewBuff = std::make_unique<buff_on_attributes>(this, type, &vec_slots);
				}
				break;
			case POINT_COSTUME_ATTR_BONUS:
				{
					static uint8_t abSlot[] = { WEAR_COSTUME_BODY, WEAR_COSTUME_HAIR };
					static std::vector <uint8_t> vec_slots (abSlot, abSlot + std::size(abSlot));
					pNewBuff = std::make_unique<buff_on_attributes>(this, type, &vec_slots);
				}
				break;
			default:
				break;
			}
			pBuff = pNewBuff.get();
			map_buff_on_attrs_.insert(map_buff_on_attrs::value_type(type, std::move(pNewBuff)));

		}
		else
			pBuff = it->second.get();

		pBuff->on(new_value);
	}
	else
	{
		if (map_buff_on_attrs_.end() == it)
			return;
		else
			it->second->change_buff_value(new_value);
	}
}

auto character::find_specify_item(uint32_t vnum) const -> item *
{
	for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
		if (inventory_item(i) && inventory_item(i)->vnum() == vnum)
			return inventory_item(i);

	return nullptr;
}

auto character::find_item_by_id(uint32_t id) const -> item *
{
	for (int i=0 ; i < INVENTORY_MAX_NUM ; ++i)
	{
		if (nullptr != inventory_item(i) && inventory_item(i)->id() == id)
			return inventory_item(i);
	}

	for (int i=BELT_INVENTORY_SLOT_START; i < BELT_INVENTORY_SLOT_END ; ++i)
	{
		if (nullptr != inventory_item(i) && inventory_item(i)->id() == id)
			return inventory_item(i);
	}

	return nullptr;
}

auto character::count_specify_item(uint32_t vnum) const -> int
{
	int	count = 0;
	item * item;

	for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
	{
		item = inventory_item(i);
		if (nullptr != item && item->vnum() == vnum)
		{
			if (my_shop_ && my_shop_->is_selling_item(item->id()))
			{
				continue;
			}
			else
			{
				count += item->count();
			}
		}
	}

	return count;
}

void character::remove_specify_item(uint32_t vnum, uint32_t count)
{
	if (0 == count)
		return;

	for (uint32_t i = 0; i < INVENTORY_MAX_NUM; ++i)
	{
		if (nullptr == inventory_item(i))
			continue;

		if (inventory_item(i)->vnum() != vnum)
			continue;

		if(my_shop_)
		{
			bool isItemSelling = my_shop_->is_selling_item(inventory_item(i)->id());
			if (isItemSelling)
				continue;
		}

		if (vnum >= 80003 && vnum <= 80007)
			log_manager::instance().gold_bar_log(player_id(), inventory_item(i)->id(), QUEST, "RemoveSpecifyItem");

		if (count >= inventory_item(i)->count())
		{
			count -= inventory_item(i)->count();
			inventory_item(i)->set_count(0);

			if (0 == count)
				return;
		}
		else
		{
			inventory_item(i)->set_count(inventory_item(i)->count() - count);
			return;
		}
	}

	if (count)
		metin2::log::warn("Cannot remove enough of item {:d}: {:d} still remaining", vnum, count);
}

auto character::count_specify_type_item(uint8_t type) const -> int
{
	int	count = 0;

	for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
	{
		item * pItem = inventory_item(i);
		if (pItem != nullptr && pItem->type() == type)
		{
			count += pItem->count();
		}
	}

	return count;
}

void character::remove_specify_type_item(uint8_t type, uint32_t count)
{
	if (0 == count)
		return;

	for (uint32_t i = 0; i < INVENTORY_MAX_NUM; ++i)
	{
		if (nullptr == inventory_item(i))
			continue;

		if (inventory_item(i)->type() != type)
			continue;

		if(my_shop_)
		{
			bool isItemSelling = my_shop_->is_selling_item(inventory_item(i)->id());
			if (isItemSelling)
				continue;
		}

		if (count >= inventory_item(i)->count())
		{
			count -= inventory_item(i)->count();
			inventory_item(i)->set_count(0);

			if (0 == count)
				return;
		}
		else
		{
			inventory_item(i)->set_count(inventory_item(i)->count() - count);
			return;
		}
	}
}

void character::auto_give_item(item * item, bool long_owner_ship)
{
	if (nullptr == item)
	{
		metin2::log::error("Cannot auto-give a null item");
		return;
	}
	if (item->owner())
	{
		metin2::log::error("Cannot auto-give item {:d}: it already has an owner", item->id());
		return;
	}

	int cell;
	if (item->is_dragon_soul())
	{
		cell = empty_dragon_soul_inventory(item);
	}
	else
	{
		cell = empty_inventory (item->size());
	}

	if (cell != -1)
	{
		if (item->is_dragon_soul())
			item->add_to_character(this, item_pos(DRAGON_SOUL_INVENTORY, cell));
		else
			item->add_to_character(this, item_pos(INVENTORY, cell));

		log_manager::instance().item_log(this, item, "SYSTEM", item->name());

		if (item->type() == ITEM_USE && item->sub_type() == USE_POTION)
		{
			quickslot * pSlot;

			if (get_quickslot(0, &pSlot) && pSlot->type == QUICKSLOT_TYPE_NONE)
			{
				quickslot slot;
				slot.type = QUICKSLOT_TYPE_ITEM;
				slot.pos = cell;
				set_quickslot(0, slot);
			}
		}
	}
	else
	{
		item->add_to_ground (map_index(), xyz());
		item->start_destroy_event();

		if (long_owner_ship)
			item->set_ownership (this, 300);
		else
			item->set_ownership (this, 60);
		log_manager::instance().item_log(this, item, "SYSTEM_DROP", item->name());
	}
}

auto character::auto_give_item(uint32_t item_vnum, uint8_t count_value, int rare_pct, bool msg) -> item *
{
	item_table * p = item_manager::instance().table(item_vnum);

	if (!p)
		return nullptr;

	db_manager::instance().send_money_log(MONEY_LOG_DROP, item_vnum, count_value);

	if (p->flags & ITEM_FLAG_STACKABLE && p->type != ITEM_BLEND) 
	{
		for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
		{
			item * item = inventory_item(i);

			if (!item)
				continue;

			if (item->vnum() == item_vnum && fn_check_item_socket(item))
			{
				if (is_set(p->flags, ITEM_FLAG_MAKECOUNT))
				{
					if (count_value < p->values[1])
						count_value = p->values[1];
				}

				uint8_t bCount2 = std::min<int>(200 - item->count(), count_value);
				count_value -= bCount2;

				item->set_count(item->count() + bCount2);

				if (count_value == 0)
				{
					if (msg)
						chat_packet(CHAT_TYPE_INFO, lc_text("아이템 획득: %s"), item->name());

					return item;
				}
			}
		}
	}

	game::items::item * item = item_manager::instance().create_item(item_vnum, count_value, 0, true);

	if (!item)
	{
		metin2::log::error("Cannot create item vnum {:d} for {}", item_vnum, name());
		return nullptr;
	}

	if (item->type() == ITEM_BLEND)
	{
		for (int i=0; i < INVENTORY_MAX_NUM; i++)
		{
			game::items::item * inv_item = inventory_item(i);

			if (inv_item == nullptr) continue;

			if (inv_item->type() == ITEM_BLEND)
			{
				if (inv_item->vnum() == item->vnum())
				{
					if (inv_item->get_socket(0) == item->get_socket(0) &&
							inv_item->get_socket(1) == item->get_socket(1) &&
							inv_item->get_socket(2) == item->get_socket(2) &&
							inv_item->count() < ITEM_MAX_COUNT)
					{
						inv_item->set_count(inv_item->count() + item->count());
						return inv_item;
					}
				}
			}
		}
	}

	int iEmptyCell;
	if (item->is_dragon_soul())
	{
		iEmptyCell = empty_dragon_soul_inventory(item);
	}
	else
		iEmptyCell = empty_inventory(item->size());

	if (iEmptyCell != -1)
	{
		if (msg)
			chat_packet(CHAT_TYPE_INFO, lc_text("아이템 획득: %s"), item->name());

		if (item->is_dragon_soul())
			item->add_to_character(this, item_pos(DRAGON_SOUL_INVENTORY, iEmptyCell));
		else
			item->add_to_character(this, item_pos(INVENTORY, iEmptyCell));
		log_manager::instance().item_log(this, item, "SYSTEM", item->name());

		if (item->type() == ITEM_USE && item->sub_type() == USE_POTION)
		{
			quickslot * pSlot;

			if (get_quickslot(0, &pSlot) && pSlot->type == QUICKSLOT_TYPE_NONE)
			{
				quickslot slot;
				slot.type = QUICKSLOT_TYPE_ITEM;
				slot.pos = iEmptyCell;
				set_quickslot(0, slot);
			}
		}
	}
	else
	{
		item->add_to_ground(map_index(), xyz());
		item->start_destroy_event();
		if (is_set(item->anti_flag(), ITEM_ANTIFLAG_DROP))
			item->set_ownership(this, 300);
		else
			item->set_ownership(this, 60);
		log_manager::instance().item_log(this, item, "SYSTEM_DROP", item->name());
	}

	metin2::log::debug("Auto-give item vnum {:d}, count {:d}", item_vnum, count_value);
	return item;
}

auto character::give_item(character * victim, item_pos cell) -> bool
{
	if (!can_handle_item())
		return false;

	item * item = get_item(cell);

	if (item && !item->is_exchanging())
	{
		if (victim->can_receive_item(this, item))
		{
			victim->receive_item(this, item);
			return true;
		}
	}

	return false;
}

auto character::can_receive_item(character * from, item * item) const -> bool
{
	if (is_pc())
		return false;

	if (distance_approx(x() - from->x(), y() - from->y()) > 2000)
		return false;

	switch (race_num())
	{
		case fishing::CAMPFIRE_MOB:
			if (item->type() == ITEM_FISH && 
					(item->sub_type() == FISH_ALIVE || item->sub_type() == FISH_DEAD))
				return true;
			break;

		case fishing::FISHER_MOB:
			if (item->type() == ITEM_ROD)
				return true;
			break;

		case BLACKSMITH_WEAPON_MOB:
		case DEVILTOWER_BLACKSMITH_WEAPON_MOB:
			if (item->type() == ITEM_WEAPON && 
					item->refined_vnum())
				return true;
			else
				return false;
			break;

		case BLACKSMITH_ARMOR_MOB:
		case DEVILTOWER_BLACKSMITH_ARMOR_MOB:
			if (item->type() == ITEM_ARMOR && 
					(item->sub_type() == ARMOR_BODY || item->sub_type() == ARMOR_SHIELD || item->sub_type() == ARMOR_HEAD) &&
					item->refined_vnum())
				return true;
			else
				return false;
			break;

		case BLACKSMITH_ACCESSORY_MOB:
		case DEVILTOWER_BLACKSMITH_ACCESSORY_MOB:
			if (item->type() == ITEM_ARMOR &&
					!(item->sub_type() == ARMOR_BODY || item->sub_type() == ARMOR_SHIELD || item->sub_type() == ARMOR_HEAD) &&
					item->refined_vnum())
				return true;
			else
				return false;
			break;

		case BLACKSMITH_MOB:
			if (item->refined_vnum() && item->refine_set() < 500)
			{
				return true;
			}
			else
			{
				return false;
			}

		case BLACKSMITH2_MOB:
			if (item->refine_set() >= 500)
			{
				return true;
			}
			else
			{
				return false;
			}

		case ALCHEMIST_MOB:
			if (item->refined_vnum())
				return true;
			break;

		case 20101:
		case 20102:
		case 20103:
			if (item->vnum() == ITEM_REVIVE_HORSE_1)
			{
				if (!is_dead())
				{
					from->chat_packet(CHAT_TYPE_INFO, lc_text("죽지 않은 말에게 선초를 먹일 수 없습니다."));
					return false;
				}
				return true;
			}
			else if (item->vnum() == ITEM_HORSE_FOOD_1)
			{
				if (is_dead())
				{
					from->chat_packet(CHAT_TYPE_INFO, lc_text("죽은 말에게 사료를 먹일 수 없습니다."));
					return false;
				}
				return true;
			}
			else if (item->vnum() == ITEM_HORSE_FOOD_2 || item->vnum() == ITEM_HORSE_FOOD_3)
			{
				return false;
			}
			break;
		case 20104:
		case 20105:
		case 20106:
			if (item->vnum() == ITEM_REVIVE_HORSE_2)
			{
				if (!is_dead())
				{
					from->chat_packet(CHAT_TYPE_INFO, lc_text("죽지 않은 말에게 선초를 먹일 수 없습니다."));
					return false;
				}
				return true;
			}
			else if (item->vnum() == ITEM_HORSE_FOOD_2)
			{
				if (is_dead())
				{
					from->chat_packet(CHAT_TYPE_INFO, lc_text("죽은 말에게 사료를 먹일 수 없습니다."));
					return false;
				}
				return true;
			}
			else if (item->vnum() == ITEM_HORSE_FOOD_1 || item->vnum() == ITEM_HORSE_FOOD_3)
			{
				return false;
			}
			break;
		case 20107:
		case 20108:
		case 20109:
			if (item->vnum() == ITEM_REVIVE_HORSE_3)
			{
				if (!is_dead())
				{
					from->chat_packet(CHAT_TYPE_INFO, lc_text("죽지 않은 말에게 선초를 먹일 수 없습니다."));
					return false;
				}
				return true;
			}
			else if (item->vnum() == ITEM_HORSE_FOOD_3)
			{
				if (is_dead())
				{
					from->chat_packet(CHAT_TYPE_INFO, lc_text("죽은 말에게 사료를 먹일 수 없습니다."));
					return false;
				}
				return true;
			}
			else if (item->vnum() == ITEM_HORSE_FOOD_1 || item->vnum() == ITEM_HORSE_FOOD_2)
			{
				return false;
			}
			break;
	}

	{
		return true;
	}

	return false;
}

void character::receive_item(character * from, item * item)
{
	if (is_pc())
		return;

	switch (race_num())
	{
		case fishing::CAMPFIRE_MOB:
			if (item->type() == ITEM_FISH && (item->sub_type() == FISH_ALIVE || item->sub_type() == FISH_DEAD))
				fishing::grill(from, item);
			else
			{
				from->set_quest_npcid(get_vid());
				quest::quest_manager::instance().take_item(from->player_id(), race_num(), item);
			}
			break;

		case DEVILTOWER_BLACKSMITH_WEAPON_MOB:
		case DEVILTOWER_BLACKSMITH_ARMOR_MOB:
		case DEVILTOWER_BLACKSMITH_ACCESSORY_MOB:
			if (item->refined_vnum() != 0 && item->refine_set() != 0 && item->refine_set() < 500)
			{
				from->set_refine_npc(this);
				from->refine_information(item->cell(), REFINE_TYPE_MONEY_ONLY);
			}
			else
			{
				from->chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템은 개량할 수 없습니다."));
			}
			break;

		case BLACKSMITH_MOB:
		case BLACKSMITH2_MOB:
		case BLACKSMITH_WEAPON_MOB:
		case BLACKSMITH_ARMOR_MOB:
		case BLACKSMITH_ACCESSORY_MOB:
			if (item->refined_vnum())
			{
				from->set_refine_npc(this);
				from->refine_information(item->cell(), REFINE_TYPE_NORMAL);
			}
			else
			{
				from->chat_packet(CHAT_TYPE_INFO, lc_text("이 아이템은 개량할 수 없습니다."));
			}
			break;

		case 20101:
		case 20102:
		case 20103:
		case 20104:
		case 20105:
		case 20106:
		case 20107:
		case 20108:
		case 20109:
			if (item->vnum() == ITEM_REVIVE_HORSE_1 ||
					item->vnum() == ITEM_REVIVE_HORSE_2 ||
					item->vnum() == ITEM_REVIVE_HORSE_3)
			{
				from->revive_horse();
				item->set_count(item->count()-1);
				from->chat_packet(CHAT_TYPE_INFO, lc_text("말에게 선초를 주었습니다."));
			}
			else if (item->vnum() == ITEM_HORSE_FOOD_1 ||
					item->vnum() == ITEM_HORSE_FOOD_2 ||
					item->vnum() == ITEM_HORSE_FOOD_3)
			{
				from->feed_horse();
				from->chat_packet(CHAT_TYPE_INFO, lc_text("말에게 사료를 주었습니다."));
				item->set_count(item->count()-1);
				effect_packet(SE_HPUP_RED);
			}
			break;

		default:
			metin2::log::debug("{} gives npc {:d} item {}", from->name(), race_num(), item->name());
			from->set_quest_npcid(get_vid());
			quest::quest_manager::instance().take_item(from->player_id(), race_num(), item);
			break;
	}
}

auto character::is_equip_unique_item(uint32_t item_vnum) const -> bool
{
	{
		item * u = wear(WEAR_UNIQUE1);

		if (u && u->vnum() == item_vnum)
			return true;
	}

	{
		item * u = wear(WEAR_UNIQUE2);

		if (u && u->vnum() == item_vnum)
			return true;
	}

	if (item_vnum == UNIQUE_ITEM_RING_OF_LANGUAGE)
		return is_equip_unique_item(UNIQUE_ITEM_RING_OF_LANGUAGE_SAMPLE);

	return false;
}

auto character::is_equip_unique_group(uint32_t group_vnum) const -> bool
{
	{
		item * u = wear(WEAR_UNIQUE1);

		if (u && u->special_group() == (int) group_vnum)
			return true;
	}

	{
		item * u = wear(WEAR_UNIQUE2);

		if (u && u->special_group() == (int) group_vnum)
			return true;
	}

	return false;
}

void character::set_refine_mode(int additional_cell)
{
	refine_additional_cell_ = additional_cell;
	under_refine_ = true;
}

void character::clear_refine_mode()
{
	under_refine_ = false;
	set_refine_npc( nullptr );
}

auto character::give_item_from_special_item_group(uint32_t group_num, std::vector<uint32_t> &item_vnums, 
											std::vector<uint32_t> &item_counts, std::vector <item *> &item_gets, int &count) -> bool
{
	const special_item_group* pGroup = item_manager::instance().get_special_item_group(group_num);

	if (!pGroup)
	{
		metin2::log::error("Special item group {:d} not found", group_num);
		return false;
	}

	std::vector <int> idxes;
	int n = pGroup->multi_index(idxes);

	bool bSuccess;

	for (int i = 0; i < n; i++)
	{
		bSuccess = false;
		int idx = idxes[i];
		uint32_t dwVnum = pGroup->get_vnum(idx);
		uint32_t dwCount = pGroup->count(idx);
		int	iRarePct = pGroup->rare_pct(idx);
		item * item_get = nullptr;
		switch (dwVnum)
		{
			case special_item_group::GOLD:
				point_change(POINT_GOLD, dwCount);
				log_manager::instance().char_log(this, dwCount, "TREASURE_GOLD", "");

				bSuccess = true;
				break;
			case special_item_group::EXP:
				{
					point_change(POINT_EXP, dwCount);
					log_manager::instance().char_log(this, dwCount, "TREASURE_EXP", "");

					bSuccess = true;
				}
				break;

			case special_item_group::MOB:
				{
					metin2::log::debug("Special item group: spawn {:d} mobs", dwCount);
					int x_value = x() + number(-500, 500);
					int y_value = y() + number(-500, 500);

					character * ch = character_manager::instance().spawn_mob(dwCount, map_index(), x_value, y_value, 0, true, -1);
					if (ch)
						ch->set_aggressive();
					bSuccess = true;
				}
				break;
			case special_item_group::SLOW:
				{
					metin2::log::debug("Special item group: slow {:d}", -(int)dwCount);
					add_affect(AFFECT_SLOW, POINT_MOV_SPEED, -(int)dwCount, AFF_SLOW, 300, 0, true);
					bSuccess = true;
				}
				break;
			case special_item_group::DRAIN_HP:
				{
					int iDropHP = max_hp()*dwCount/100;
					iDropHP = std::min(iDropHP, hp()-1);
					metin2::log::debug("Special item group: drain hp {:d}", -iDropHP);
					point_change(POINT_HP, -iDropHP);
					bSuccess = true;
				}
				break;
			case special_item_group::POISON:
				{
					attacked_by_poison(nullptr);
					bSuccess = true;
				}
				break;

			case special_item_group::MOB_GROUP:
				{
					int sx = x() - number(300, 500);
					int sy = y() - number(300, 500);
					int ex = x() + number(300, 500);
					int ey = y() + number(300, 500);
					character_manager::instance().spawn_group(dwCount, map_index(), sx, sy, ex, ey, nullptr, true);

					bSuccess = true;
				}
				break;
			default:
				{
					item_get = auto_give_item(dwVnum, dwCount, iRarePct);

					if (item_get)
					{
						bSuccess = true;
					}
				}
				break;
		}

		if (bSuccess)
		{
			item_vnums.push_back(dwVnum);
			item_counts.push_back(dwCount);
			item_gets.push_back(item_get);
			count++;

		}
		else
		{
			return false;
		}
	}
	return bSuccess;
}

auto character::item_process_hair(item * item, int dest_cell) -> bool
{
	if (item->check_item_use_level(level()) == false)
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("아직 이 머리를 사용할 수 없는 레벨입니다."));
		return false;
	}

	uint32_t hair = item->vnum();

	switch (job())
	{
		case JOB_WARRIOR :
			hair -= 72000;
			break;

		case JOB_ASSASSIN :
			hair -= 71250;
			break;

		case JOB_SURA :
			hair -= 70500;
			break;

		case JOB_SHAMAN :
			hair -= 69750;
			break;

		default :
			return false;
			break;
	}

	if (hair == part(PART_HAIR))
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("동일한 머리 스타일로는 교체할 수 없습니다."));
		return true;
	}

	item->set_count(item->count() - 1);

	set_part(PART_HAIR, hair);
	update_packet();

	return true;
}

auto character::item_process_polymorph(item * item) -> bool
{
	if (is_polymorphed())
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("이미 둔갑중인 상태입니다."));
		return false;
	}

	if (true == is_riding())
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("둔갑할 수 없는 상태입니다."));
		return false;
	}

	uint32_t dwVnum = item->get_socket(0);

	if (dwVnum == 0)
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("잘못된 둔갑 아이템입니다."));
		item->set_count(item->count()-1);
		return false;
	}

	const mob* pMob = mob_manager::instance().get(dwVnum);

	if (pMob == nullptr)
	{
		chat_packet(CHAT_TYPE_INFO, lc_text("잘못된 둔갑 아이템입니다."));
		item->set_count(item->count()-1);
		return false;
	}

	switch (item->vnum())
	{
		case 70104 :
		case 70105 :
		case 70106 :
		case 70107 :
		case 71093 :
			{
				metin2::log::info("Pid {:d} uses polymorph ball (vnum {:d})", player_id(), dwVnum);

				int iPolymorphLevelLimit = std::max(0, 20 - level() * 3 / 10);
				if (pMob->table.level >= level() + iPolymorphLevelLimit)
				{
					chat_packet(CHAT_TYPE_INFO, lc_text("나보다 너무 높은 레벨의 몬스터로는 변신 할 수 없습니다."));
					return false;
				}

				int iDuration = skill_level(POLYMORPH_SKILL_ID) == 0 ? 5 : (5 + (5 + skill_level(POLYMORPH_SKILL_ID)/40 * 25));
				iDuration *= 60;

				uint32_t dwBonus = 0;

				dwBonus = (2 + skill_level(POLYMORPH_SKILL_ID)/40) * 100;

				add_affect(AFFECT_POLYMORPH, POINT_POLYMORPH, dwVnum, AFF_POLYMORPH, iDuration, 0, true);
				add_affect(AFFECT_POLYMORPH, POINT_ATT_BONUS, dwBonus, AFF_POLYMORPH, iDuration, 0, false);

				item->set_count(item->count()-1);
			}
			break;

		case 50322:
			{

				metin2::log::info("{} (pid {:d}) uses polymorph book (vnum {:d})", name(), player_id(), dwVnum);

				if (polymorph_utils::instance().polymorph_character(this, item, pMob) == true)
				{
					polymorph_utils::instance().update_book_practice_grade(this, item);
				}
			}
			break;

		default :
			metin2::log::error("Invalid polymorph item vnum {:d} used by pid {:d}", item->original_vnum(), player_id());
			return false;
	}

	return true;
}

auto character::can_do_cube() const -> bool
{
	if (is_observer_)	return false;
	if (get_shop())		return false;
	if (my_shop())	return false;
	if (under_refine_)	return false;
	if (is_warping())	return false;

	return true;
}

auto character::un_equip_special_ride_unique_item() -> bool
{
	item * Unique1 = wear(WEAR_UNIQUE1);
	item * Unique2 = wear(WEAR_UNIQUE2);

	if( nullptr != Unique1 )
	{
		if( UNIQUE_GROUP_SPECIAL_RIDE == Unique1->special_group() )
		{
			return unequip_item(Unique1);
		}
	}

	if( nullptr != Unique2 )
	{
		if( UNIQUE_GROUP_SPECIAL_RIDE == Unique2->special_group() )
		{
			return unequip_item(Unique2);
		}
	}

	return true;
}

void character::auto_recovery_item_process(const affect_types type)
{
	if (true == is_dead() || true == is_stun())
		return;

	if (false == is_pc())
		return;

	if (AFFECT_AUTO_HP_RECOVERY != type && AFFECT_AUTO_SP_RECOVERY != type)
		return;

	if (nullptr != find_affect(AFFECT_STUN))
		return;

	{
		const uint32_t stunSkills[] = { SKILL_TANHWAN, SKILL_GEOMPUNG, SKILL_BYEURAK, SKILL_GIGUNG };

		for (size_t i=0 ; i < sizeof(stunSkills)/sizeof(uint32_t) ; ++i)
		{
			const affect* p = find_affect(stunSkills[i]);

			if (nullptr != p && AFF_STUN == p->flag)
				return;
		}
	}

	const affect* pAffect = find_affect(type);
	const size_t idx_of_amount_of_used = 1;
	const size_t idx_of_amount_of_full = 2;

	if (nullptr != pAffect)
	{
		item * pItem = find_item_by_id(pAffect->flag);

		if (nullptr != pItem && true == pItem->get_socket(0))
		{
			if (false == arena_manager::instance().is_arena_map(map_index()))
			{
				const int32_t amount_of_used = pItem->get_socket(idx_of_amount_of_used);
				const int32_t amount_of_full = pItem->get_socket(idx_of_amount_of_full);

				const int32_t avail = amount_of_full - amount_of_used;

				int32_t amount = 0;

				if (AFFECT_AUTO_HP_RECOVERY == type)
				{
					amount = max_hp() - (hp() + point(POINT_HP_RECOVERY));
				}
				else if (AFFECT_AUTO_SP_RECOVERY == type)
				{
					amount = max_sp() - (sp() + point(POINT_SP_RECOVERY));
				}

				if (amount > 0)
				{
					if (avail > amount)
					{
						const int pct_of_used = amount_of_used * 100 / amount_of_full;
						const int pct_of_will_used = (amount_of_used + amount) * 100 / amount_of_full;

						bool bLog = false;
						if ((pct_of_will_used / 10) - (pct_of_used / 10) >= 1)
							bLog = true;
						pItem->set_socket(idx_of_amount_of_used, amount_of_used + amount, bLog);
					}
					else
					{
						amount = avail;

						item_manager::instance().remove_item( pItem );
					}

					if (AFFECT_AUTO_HP_RECOVERY == type)
					{
						point_change( POINT_HP_RECOVERY, amount );
						effect_packet( SE_AUTO_HPUP );
					}
					else if (AFFECT_AUTO_SP_RECOVERY == type)
					{
						point_change( POINT_SP_RECOVERY, amount );
						effect_packet( SE_AUTO_SPUP );
					}
				}
			}
			else
			{
				pItem->lock(false);
				pItem->set_socket(0, false);
				remove_affect( const_cast<affect*>(pAffect) );
			}
		}
		else
		{
			remove_affect( const_cast<affect*>(pAffect) );
		}
	}
}

auto character::is_valid_item_position(item_pos pos) const -> bool
{
	uint8_t window_type = pos.window_type;
	uint16_t cell = pos.cell;

	switch (window_type)
	{
	case RESERVED_WINDOW:
		return false;

	case INVENTORY:
	case EQUIPMENT:
		return cell < (INVENTORY_AND_EQUIP_SLOT_MAX);

	case DRAGON_SOUL_INVENTORY:
		return cell < (DRAGON_SOUL_INVENTORY_MAX_NUM);

	case SAFEBOX:
		if (nullptr != safebox_)
			return safebox_->is_valid_position(cell);
		else
			return false;

	case MALL:
		if (nullptr != mall_)
			return mall_->is_valid_position(cell);
		else
			return false;
	default:
		return false;
	}
}

#define VERIFY_MSG(exp, msg)  \
	if (true == (exp)) { \
			chat_packet(CHAT_TYPE_INFO, lc_text(msg)); \
			return false; \
	}

auto character::can_equip_now(item * const item, const item_pos& src_cell, const item_pos& dest_cell) -> bool
{
	const item_table* itemTable = item->proto();

	switch (job())
	{
		case JOB_WARRIOR:
			if (item->anti_flag() & ITEM_ANTIFLAG_WARRIOR)
				return false;
			break;

		case JOB_ASSASSIN:
			if (item->anti_flag() & ITEM_ANTIFLAG_ASSASSIN)
				return false;
			break;

		case JOB_SHAMAN:
			if (item->anti_flag() & ITEM_ANTIFLAG_SHAMAN)
				return false;
			break;

		case JOB_SURA:
			if (item->anti_flag() & ITEM_ANTIFLAG_SURA)
				return false;
			break;
	}

	for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
	{
		int32_t limit = itemTable->limits[i].value;
		switch (itemTable->limits[i].type)
		{
			case LIMIT_LEVEL:
				if (level() < limit)
				{
					chat_packet(CHAT_TYPE_INFO, lc_text("레벨이 낮아 착용할 수 없습니다."));
					return false;
				}
				break;

			case LIMIT_STR:
				if (point(POINT_ST) < limit)
				{
					chat_packet(CHAT_TYPE_INFO, lc_text("근력이 낮아 착용할 수 없습니다."));
					return false;
				}
				break;

			case LIMIT_INT:
				if (point(POINT_IQ) < limit)
				{
					chat_packet(CHAT_TYPE_INFO, lc_text("지능이 낮아 착용할 수 없습니다."));
					return false;
				}
				break;

			case LIMIT_DEX:
				if (point(POINT_DX) < limit)
				{
					chat_packet(CHAT_TYPE_INFO, lc_text("민첩이 낮아 착용할 수 없습니다."));
					return false;
				}
				break;

			case LIMIT_CON:
				if (point(POINT_HT) < limit)
				{
					chat_packet(CHAT_TYPE_INFO, lc_text("체력이 낮아 착용할 수 없습니다."));
					return false;
				}
				break;
		}
	}

	if (item->wear_flag() & WEARABLE_UNIQUE)
	{
		if ((wear(WEAR_UNIQUE1) && wear(WEAR_UNIQUE1)->is_same_special_group(item)) ||
			(wear(WEAR_UNIQUE2) && wear(WEAR_UNIQUE2)->is_same_special_group(item)))
		{
			chat_packet(CHAT_TYPE_INFO, lc_text("같은 종류의 유니크 아이템 두 개를 동시에 장착할 수 없습니다."));
			return false;
		}

		if (marriage::manager::instance().is_marriage_unique_item(item->vnum()) && 
			!marriage::manager::instance().is_married(player_id()))
		{
			chat_packet(CHAT_TYPE_INFO, lc_text("결혼하지 않은 상태에서 예물을 착용할 수 없습니다."));
			return false;
		}

	}

	return true;
}

auto character::can_unequip_now(item * const item, const item_pos& src_cell, const item_pos& dest_cell) -> bool
{	

	if (ITEM_BELT == item->type())
		VERIFY_MSG(belt_inventory_helper::is_exist_item_in_belt_inventory(this), "벨트 인벤토리에 아이템이 존재하면 해제할 수 없습니다.");

	if (is_set(item->flag(), ITEM_FLAG_IRREMOVABLE))
		return false;

	{
		int pos = -1;

		if (item->is_dragon_soul())
			pos = empty_dragon_soul_inventory(item);
		else
			pos = empty_inventory(item->size());

		VERIFY_MSG( -1 == pos, "소지품에 빈 공간이 없습니다." );
	}

	return true;
}
}
