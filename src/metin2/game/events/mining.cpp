#include <metin2/game/events/mining.h>

#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/app/event.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/persistence/log.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <cstdint>

namespace metin2::game::inline events {

namespace mining
{
	enum
	{
		MAX_ORE = 18,
		MAX_FRACTION_COUNT = 9,
		ORE_COUNT_FOR_REFINE = 100,
	};

	struct mining_info
	{
		uint32_t load_vnum;
		uint32_t raw_ore_vnum;
		uint32_t refine_vnum;
	};

	mining_info info[MAX_ORE] =
	{
		{ 20047, 50601, 50621 },
		{ 20048, 50602, 50622 },
		{ 20049, 50603, 50623 },
		{ 20050, 50604, 50624 },
		{ 20051, 50605, 50625 },
		{ 20052, 50606, 50626 },
		{ 20053, 50607, 50627 },
		{ 20054, 50608, 50628 },
		{ 20055, 50609, 50629 },
		{ 20056, 50610, 50630 },
		{ 20057, 50611, 50631 },
		{ 20058, 50612, 50632 },
		{ 20059, 50613, 50633 },
		{ 30301, 50614, 50634 },
		{ 30302, 50615, 50635 },
		{ 30303, 50616, 50636 },
		{ 30304, 50617, 50637 },
		{ 30305, 50618, 50638 },
	};

	int fraction_info[MAX_FRACTION_COUNT][3] =
	{
		{ 20,  1, 10 },
		{ 30, 11, 20 },
		{ 20, 21, 30 },
		{ 15, 31, 40 },
		{  5, 41, 50 },
		{  4, 51, 60 },
		{  3, 61, 70 },
		{  2, 71, 80 },
		{  1, 81, 90 },
	};

	int pick_grade_add_pct[10] =
	{
		3, 5, 8, 11, 15, 20, 26, 32, 40, 50
	};

	int skill_level_add_pct[SKILL_MAX_LEVEL + 1] =
	{
		0,
		1, 1, 1, 1,
		2, 2, 2, 2,
		3, 3, 3, 3,
		4, 4, 4, 4,
		5, 5, 5, 5,
		6, 6, 6, 6,
		7, 7, 7, 7,
		8, 8, 8, 8,
		9, 9, 9, 9,
		10, 10, 10,
		11,
	};

	auto get_raw_ore_from_load(uint32_t load_vnum_value) -> uint32_t
	{
		for (int i = 0; i < MAX_ORE; ++i)
		{
			if (info[i].load_vnum == load_vnum_value)
				return info[i].raw_ore_vnum;
		}
		return 0;
	}

	auto get_refine_from_raw_ore(uint32_t raw_ore_vnum_value) -> uint32_t
	{
		for (int i = 0; i < MAX_ORE; ++i)
		{
			if (info[i].raw_ore_vnum == raw_ore_vnum_value)
				return info[i].refine_vnum;
		}
		return 0;
	}

	auto get_fraction_count() -> int
	{
		int r = number(1, 100);

		for (int i = 0; i < MAX_FRACTION_COUNT; ++i)
		{
			if (r <= fraction_info[i][0])
				return number(fraction_info[i][1], fraction_info[i][2]);
			else
				r -= fraction_info[i][0];
		}

		return 0; 
	}

	void ore_drop(character * ch, uint32_t load_vnum)
	{
		uint32_t dwRawOreVnum = get_raw_ore_from_load(load_vnum);

		int iFractionCount = get_fraction_count();

		if (iFractionCount == 0)
		{
			metin2::log::error("Wrong ore fraction count");
			return;
		}

		item * item = item_manager::instance().create_item(dwRawOreVnum, get_fraction_count());

		if (!item)
		{
			metin2::log::error("cannot create item vnum {:d}", dwRawOreVnum);
			return;
		}

		pixel_position pos;
		pos.x = ch->x() + number(-200, 200);
		pos.y = ch->y() + number(-200, 200);

		item->add_to_ground(ch->map_index(), pos);
		item->start_destroy_event();
		item->set_ownership(ch, 15);

		db_manager::instance().send_money_log(MONEY_LOG_DROP, item->vnum(), item->count());
	}

	auto get_ore_pct(character * ch) -> int
	{
		int defaultPct = 20;
		int iSkillLevel = ch->skill_level(SKILL_MINING);

		item * pick = ch->wear(WEAR_WEAPON);

		if (!pick || pick->type() != ITEM_PICK)
			return 0;

		return defaultPct + skill_level_add_pct[std::clamp(iSkillLevel, 0, 40)] + pick_grade_add_pct[std::clamp(pick->refine_level(), 0, 9)];
	}

	EVENTINFO(mining_event_info)
	{
		uint32_t pid;
		uint32_t vid_load;

		mining_event_info() 
		: pid( 0 )
		, vid_load( 0 )
		{
		}
	};

	auto pick_check(item& item) -> bool
	{
		if (item.type() != ITEM_PICK)    
			return false;

		return true;
	}

	auto pick_get_max_exp(item& pick) -> int
	{
		return pick.value(2);
	}

	auto pick_get_cur_exp(item& pick) -> int
	{
		return pick.get_socket(0);
	}

	void pick_inc_cur_exp(item& pick)
	{
		int cur = pick_get_cur_exp(pick);
		pick.set_socket(0, cur + 1);
	}

	void pick_max_cur_exp(item& pick)
	{
		int max = pick_get_max_exp(pick);
		pick.set_socket(0, max);
	}

	auto pick_refinable(item& item) -> bool
	{
		if (pick_get_cur_exp(item) < pick_get_max_exp(item))
			return false;

		return true;
	}

	auto pick_is_practice_success(item& pick) -> bool
	{
		return (number(1,pick.value(1))==1);
	}
	
	auto pick_is_refine_success(item& pick) -> bool
	{
		return (number(1,100) <= pick.value(3));
	}

	auto real_refine_pick(character * ch, item * item) -> int
	{
		if (!ch || !item)
			return 2;

		log_manager& rkLogMgr = log_manager::instance();
		item_manager& rkItemMgr = item_manager::instance();

		if (!pick_check(*item))
		{
			metin2::log::error("REFINE_PICK_HACK pid({:d}) item({}:{:d}) type({:d})", ch->player_id(), item->name(), item->id(), item->type());
			rkLogMgr.refine_log(ch->player_id(), item->name(), item->id(), -1, 1, "PICK_HACK");
			return 2;
		}

		game::items::item& rkOldPick = *item;

		if (!pick_refinable(rkOldPick))
			return 2;

		int iAdv = rkOldPick.value(0) / 10;

		if (rkOldPick.is_equipped() == true)
			return 2;

		if (pick_is_refine_success(rkOldPick))
		{
			rkLogMgr.refine_log(ch->player_id(), rkOldPick.name(), rkOldPick.id(), iAdv, 1, "PICK");

			game::items::item * pkNewPick = item_manager::instance().create_item(rkOldPick.refined_vnum(), 1);
			if (pkNewPick)
			{
				uint8_t bCell = rkOldPick.cell();
				rkItemMgr.remove_item(item, "REMOVE (REFINE PICK)");
				pkNewPick->add_to_character(ch, item_pos(INVENTORY, bCell));
				log_manager::instance().item_log(ch, pkNewPick, "REFINE PICK SUCCESS", pkNewPick->name());
				return 1;
			}

			return 2;
		}
		else
		{
			rkLogMgr.refine_log(ch->player_id(), rkOldPick.name(), rkOldPick.id(), iAdv, 0, "PICK");

			game::items::item * pkNewPick = item_manager::instance().create_item(rkOldPick.value(4), 1);

			if (pkNewPick)
			{
				uint8_t bCell = rkOldPick.cell();
				rkItemMgr.remove_item(item, "REMOVE (REFINE PICK)");
				pkNewPick->add_to_character(ch, item_pos(INVENTORY, bCell));
				rkLogMgr.item_log(ch, pkNewPick, "REFINE PICK FAIL", pkNewPick->name());
				return 0;
			}

			return 2;
		}
	}

	void cheat_max_pick(character * ch, item * item)
	{
		if (!item)
			return;

		if (!pick_check(*item))
			return;

		game::items::item& pick = *item;
		pick_max_cur_exp(pick);

		ch->chat_packet(CHAT_TYPE_INFO, lc_text("곡괭이 수련도가 최대(%d)가 되었습니다."), pick_get_cur_exp(pick));
	}

	void practice_pick(character * ch, item * item)
	{
		if (!item)
			return;

		if (!pick_check(*item))
			return;

		game::items::item& pick = *item;
		if (pick.refined_vnum()<=0)
			return;

		if (pick_is_practice_success(pick))
		{

			if (pick_refinable(pick))
			{
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("곡괭이가 최대 수련도에 도달하였습니다."));
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("나무꾼를 통해 다음 레벨의 곡괭이로 업그레이드 할 수 있습니다."));
			}
			else
			{
				pick_inc_cur_exp(pick);	

				ch->chat_packet(CHAT_TYPE_INFO, lc_text("곡괭이의 수련도가 증가하였습니다! (%d/%d)"),
						pick_get_cur_exp(pick), pick_get_max_exp(pick));

				if (pick_refinable(pick))
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("곡괭이가 최대 수련도에 도달하였습니다."));
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("나무꾼를 통해 다음 레벨의 곡괭이로 업그레이드 할 수 있습니다."));
				}
			}
		}
	}

	EVENTFUNC(mining_event)
	{
		mining_event_info* info = dynamic_cast<mining_event_info*>( event->info );

		if ( info == nullptr )
		{
			metin2::log::error( "mining_event_info> <Factor> Null pointer" );
			return 0;
		}

		character * ch = character_manager::instance().find_by_pid(info->pid);
		character * load = character_manager::instance().find(info->vid_load);

		if (!ch)
			return 0;

		ch->mining_take();

		item * pick = ch->wear(WEAR_WEAPON);

		if (!pick || !pick_check(*pick))
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("곡괭이를 들고 있지 않아서 캘 수 없습니다."));
			return 0;
		}

		if (!load)
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("더이상 캐낼 수 없습니다."));
			return 0;
		}

		int iPct = get_ore_pct(ch);

		if (number(1, 100) <= iPct)
		{
			ore_drop(ch, load->race_num());
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("채광에 성공하였습니다."));
		}
		else
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("채광에 실패하였습니다."));
		}

		practice_pick(ch, pick);

		return 0;
	}

	auto create_mining_event(character * ch, character * load, int count) -> event_ptr
	{
		mining_event_info* info = alloc_event_info<mining_event_info>();
		info->pid = ch->player_id();
		info->vid_load = load->get_vid();

		return event_create(mining_event, info, passes_per_sec(2 * count));
	}

	auto ore_refine(character * ch, character * npc, item * ore, int cost, int pct, game::items::item * metinstone_item) -> bool
	{
		if (!ch || !npc)
			return false;

		if (ore->owner() != ch)
		{
			metin2::log::error("wrong owner");
			return false;
		}

		if (ore->count() < ORE_COUNT_FOR_REFINE)
		{
			metin2::log::error("not enough count");
			return false;
		}

		uint32_t dwRefinedVnum = get_refine_from_raw_ore(ore->vnum());

		if (dwRefinedVnum == 0)
			return false;

		ch->set_refine_npc(npc);
		ore->set_count(ore->count() - ORE_COUNT_FOR_REFINE);
		int iCost = ch->compute_refine_fee(cost, 1);

		if (ch->gold() < iCost)
			return false;

		ch->pay_refine_fee(iCost);

		if (metinstone_item)
			item_manager::instance().remove_item(metinstone_item, "REMOVE (MELT)");

		if (number(1, 100) <= pct)
		{
			ch->auto_give_item(dwRefinedVnum, 1);
			return true;
		}

		return false;
	}

	auto is_vein_of_ore (uint32_t vnum) -> bool
	{
		for (int i = 0; i < MAX_ORE; i++)
		{
			if (info[i].load_vnum == vnum)
				return true;
		}
		return false;
	}
}
}
