#include <metin2/game/characters/affect.h>
#include <metin2/game/events/arena.h>
#include <metin2/game/combat/battle.h>
#include <metin2/game/events/battle_arena.h>
#include <metin2/game/events/blue_dragon.h>
#include <metin2/game/empire/castle.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/items/cube.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/events/dragon_lair.h>
#include <metin2/game/world/dungeon.h>
#include <metin2/game/app/event.h>
#include <metin2/game/trade/exchange.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/persistence/log.h>
#include <metin2/game/social/marriage.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/empire/monarch.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/empire/priv_manager.h>
#include <metin2/game/combat/pvp.h>
#include <metin2/game/quest/quest.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/world/regen.h>
#include <metin2/game/world/sectree.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/trade/shop_manager.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/empire/start_position.h>
#include <metin2/game/empire/threeway_war.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/world/vector.h>
#include <metin2/game/world/vid.h>

#include <metin2/proto/length.h>
#include <metin2/log/log.h>
#include <metin2/core/main.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <queue>
#include <random>
#include <utility>
#include <vector>

#include <unistd.h>

namespace metin2::game::inline characters {

uint32_t adjust_exp_by_level(character * const ch, const uint32_t exp)
{
	if (PLAYER_EXP_TABLE_MAX < ch->level())
	{
		double ret = 0.95;
		double factor = 0.1;

		for (ssize_t i=0 ; i < ch->level()-100 ; ++i)
		{
			if ( (i%10) == 0)
				factor /= 2.0;

			ret *= 1.0 - factor;
		}

		ret = ret * static_cast<double>(exp);

		if (ret < 1.0)
			return 1;

		return static_cast<uint32_t>(ret);
	}

	return exp;
}

bool character::can_begin_fight() const
{
	if (!can_move())
		return false;

	return points_instant_.position == POS_STANDING && !is_dead() && !is_stun();
}

void character::begin_fight(character * pkVictim)
{
	set_victim(pkVictim);
	set_position(POS_FIGHTING);
	set_next_state_pulse(1);
}

void character::create_fly(uint8_t bType, character * pkVictim)
{
	packet_gc_create_fly packFly;

	packFly.header         = HEADER_GC_CREATE_FLY;
	packFly.type           = bType;
	packFly.start_vid      = get_vid();
	packFly.end_vid        = pkVictim->get_vid();

	packet_around(&packFly, sizeof(packet_gc_create_fly));
}

void character::distribute_sp(character * pkKiller, int iMethod)
{
	if (pkKiller->sp() >= pkKiller->max_sp())
		return;

	bool bAttacking = (get_dword_time() - last_attack_time()) < 3000;
	bool bMoving = (get_dword_time() - last_move_time()) < 3000;

	if (iMethod == 1)
	{
		int num = number(0, 3);

		if (!num)
		{
			int iLvDelta = level() - pkKiller->level();
			int iAmount = 0;

			if (iLvDelta >= 5)
				iAmount = 10;
			else if (iLvDelta >= 0)
				iAmount = 6;
			else if (iLvDelta >= -3)
				iAmount = 2;

			if (iAmount != 0)
			{
				iAmount += (iAmount * pkKiller->point(POINT_SP_REGEN)) / 100;

				if (iAmount >= 11)
					create_fly(FLY_SP_BIG, pkKiller);
				else if (iAmount >= 7)
					create_fly(FLY_SP_MEDIUM, pkKiller);
				else
					create_fly(FLY_SP_SMALL, pkKiller);

				pkKiller->point_change(POINT_SP, iAmount);
			}
		}
	}
	else
	{
		if (pkKiller->job() == JOB_SHAMAN || (pkKiller->job() == JOB_SURA && pkKiller->skill_group() == 2))
		{
			int iAmount;

			if (bAttacking)
				iAmount = 2 + max_sp() / 100;
			else if (bMoving)
				iAmount = 3 + max_sp() * 2 / 100;
			else
				iAmount = 10 + max_sp() * 3 / 100; // Normal state

			iAmount += (iAmount * pkKiller->point(POINT_SP_REGEN)) / 100;
			pkKiller->point_change(POINT_SP, iAmount);
		}
		else
		{
			int iAmount;

			if (bAttacking)
				iAmount = 2 + pkKiller->max_sp() / 200;
			else if (bMoving)
				iAmount = 2 + pkKiller->max_sp() / 100;
			else
			{
				// Normal state
				if (pkKiller->hp() < pkKiller->max_hp())
					iAmount = 2 + (pkKiller->max_sp() / 100); // When HP is not full
				else
					iAmount = 9 + (pkKiller->max_sp() / 100); // Default
			}

			iAmount += (iAmount * pkKiller->point(POINT_SP_REGEN)) / 100;
			pkKiller->point_change(POINT_SP, iAmount);
		}
	}
}


bool character::attack(character * pkVictim, uint8_t bType)
{
	if (metin2::config::test_server())
		metin2::log::info( "[TEST_SERVER] Attack : {} type {:d}, MobBattleType {:d}", name(), bType, !mob_battle_type() ? 0 : mob_attack_range());
	if (!can_move())
		return false;

	// CASTLE
	if (is_castle_map(map_index()) && false == castle_can_attack(this, pkVictim))
		return false;
	// CASTLE

	uint32_t dwCurrentTime = get_dword_time();

	if (is_pc())
	{
		if (is_speed_hack(this, pkVictim, dwCurrentTime))
			return false;

		if (bType == 0 && dwCurrentTime < get_skip_combo_attack_by_time())
			return false;
	}
	else
	{
		monster_chat(MONSTER_CHAT_ATTACK);
	}

	pkVictim->set_sync_owner(this);

	if (pkVictim->can_begin_fight())
		pkVictim->begin_fight(this);

	int iRet;

	if (bType == 0)
	{
		//
		// Normal attack
		//
		switch (mob_battle_type())
		{
			case BATTLE_TYPE_MELEE:
			case BATTLE_TYPE_POWER:
			case BATTLE_TYPE_TANKER:
			case BATTLE_TYPE_SUPER_POWER:
			case BATTLE_TYPE_SUPER_TANKER:
				iRet = battle_melee_attack(this, pkVictim);
				break;

			case BATTLE_TYPE_RANGE:
				fly_target(pkVictim->get_vid(), pkVictim->x(), pkVictim->y(), HEADER_CG_FLY_TARGETING);
				iRet = shoot(0) ? BATTLE_DAMAGE : BATTLE_NONE;
				break;

			case BATTLE_TYPE_MAGIC:
				fly_target(pkVictim->get_vid(), pkVictim->x(), pkVictim->y(), HEADER_CG_FLY_TARGETING);
				iRet = shoot(1) ? BATTLE_DAMAGE : BATTLE_NONE;
				break;

			default:
				metin2::log::error("Unhandled battle type {:d}", mob_battle_type());
				iRet = BATTLE_NONE;
				break;
		}
	}
	else
	{
		if (is_pc() == true)
		{
			if (dwCurrentTime - last_skill_time_ > 1500)
			{
				metin2::log::debug( "HACK: Too long skill using term. Name({}) PID({:d}) delta({:d})",
						name(), player_id(), (dwCurrentTime - last_skill_time_));
				return false;
			}
		}

		metin2::log::debug( "Attack call ComputeSkill {:d} {}", bType, pkVictim?pkVictim->name():"");
		iRet = compute_skill(bType, pkVictim);
	}

	if (iRet == BATTLE_DAMAGE || iRet == BATTLE_DEAD)
	{
		on_move(true);
		pkVictim->on_move();

		// only pc sets victim null. For npc, state machine will reset this.
		if (BATTLE_DEAD == iRet && is_pc())
			set_victim(nullptr);

		return true;
	}

	return false;
}

void character::death_penalty(uint8_t bTown)
{
	metin2::log::debug( "DEATH_PERNALY_CHECK({}) town({:d})", name(), bTown);

	cube_close(this);

	if (battle_arena::instance().is_battle_arena_map(map_index()) == true)
	{
		return;
	}
	
	if (level() < 10)
	{
		metin2::log::info( "NO_DEATH_PENALTY_LESS_LV10({})", name());
		chat_packet(CHAT_TYPE_INFO, lc_text("용신의 가호로 경험치가 떨어지지 않았습니다."));
		return;
	}

   	if (number(0, 2))
	{
		metin2::log::info( "NO_DEATH_PENALTY_LUCK({})", name());
		chat_packet(CHAT_TYPE_INFO, lc_text("용신의 가호로 경험치가 떨어지지 않았습니다."));
		return;
	}

	if (is_set(points_instant_.instant_flag, INSTANT_FLAG_DEATH_PENALTY))
	{
		remove_bit(points_instant_.instant_flag, INSTANT_FLAG_DEATH_PENALTY);

		// NO_DEATH_PENALTY_BUG_FIX 
		if (!bTown) // In the international version, Dragon God's Protection is used only when reviving in place. (No experience penalty when returning to town)
		{
			if (find_affect(AFFECT_NO_DEATH_PENALTY))
			{
				metin2::log::info( "NO_DEATH_PENALTY_AFFECT({})", name());
				chat_packet(CHAT_TYPE_INFO, lc_text("용신의 가호로 경험치가 떨어지지 않았습니다."));
				remove_affect(AFFECT_NO_DEATH_PENALTY);
				return;
			}
		}
		// END_OF_NO_DEATH_PENALTY_BUG_FIX

		int iLoss = ((next_exp() * exp_loss_percents[std::clamp(level(), 1, PLAYER_EXP_TABLE_MAX)]) / 100);

		iLoss = std::min(800000, iLoss);

		if (bTown)
		{
			iLoss = 0;
		}

		if (is_equip_unique_item(UNIQUE_ITEM_TEARDROP_OF_GODNESS))
			iLoss /= 2;

		metin2::log::info( "DEATH_PENALTY({}) EXP_LOSS: {:d} percent {:d}%", name(), iLoss, exp_loss_percents[std::min(metin2::config::game().max_level, level())]);

		point_change(POINT_EXP, -iLoss, true);
	}
}

bool character::is_stun() const
{
	if (is_set(points_instant_.instant_flag, INSTANT_FLAG_STUN))
		return true;

	return false;
}

EVENTFUNC(stun_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "StunEvent> <Factor> Null pointer" );
		return 0;
	}

	character * ch = info->ch;

	if (ch == nullptr) { // <Factor>
		return 0;
	}
	ch->stun_event = nullptr;
	ch->dead();
	return 0;
}

void character::stun()
{
	if (is_stun())
		return;

	if (is_dead())
		return;

	if (!is_pc() && party_)
	{
		party_->send_message(this, PM_ATTACKED_BY, 0, 0);
	}

	metin2::log::debug( "{}: Stun {}", name(), static_cast<const void *>(this));

	point_change(POINT_HP_RECOVERY, -point(POINT_HP_RECOVERY));
	point_change(POINT_SP_RECOVERY, -point(POINT_SP_RECOVERY));

	close_my_shop();

	event_cancel(&recovery_event); // Kill the recovery event.

	packet_gc_stun pack;
	pack.header	= HEADER_GC_STUN;
	pack.vid	= vid_;
	packet_around(&pack, sizeof(pack));

	set_bit(points_instant_.instant_flag, INSTANT_FLAG_STUN);

	if (stun_event)
		return;

	char_event_info* info = alloc_event_info<char_event_info>();

	info->ch = this;

	stun_event = event_create(game::characters::stun_event, info, passes_per_sec(3));
}

EVENTINFO(char_dead_event_info)
{
	bool is_pc;
	uint32_t id;

	char_dead_event_info()
	: is_pc(0)
	, id(0)
	{
	}
};

EVENTFUNC(dead_event)
{
	const char_dead_event_info* info = dynamic_cast<char_dead_event_info*>(event->info);

	if ( info == nullptr )
	{
		metin2::log::error( "dead_event> <Factor> Null pointer" );
		return 0;
	}

	character * ch = nullptr;

	if (true == info->is_pc)
	{
		ch = character_manager::instance().find_by_pid( info->id );
	}
	else
	{
		ch = character_manager::instance().find( info->id );
	}

	if (nullptr == ch)
	{
		metin2::log::error("DEAD_EVENT: cannot find char pointer with {} id({:d})", info->is_pc ? "PC" : "MOB", info->id );
		return 0;
	}

	ch->dead_event = nullptr;

	if (ch->get_desc())
	{
		ch->get_desc()->set_phase(PHASE_GAME);

		ch->set_position(POS_STANDING);

		pixel_position pos;

		if (sectree_manager::instance().recall_position_by_empire(ch->map_index(), ch->empire(), pos))
			ch->warp_set(pos.x, pos.y);
		else
		{
			metin2::log::error("cannot find spawn position (name {})", ch->name());
			ch->warp_set(empire_start_x(ch->empire()), empire_start_y(ch->empire()));
		}

		ch->point_change(POINT_HP, (ch->max_hp() / 2) - ch->hp(), true);

		ch->death_penalty(0);

		ch->start_recovery_event();

		ch->chat_packet(CHAT_TYPE_COMMAND, "CloseRestartWindow");
	}
	else
	{
		if (ch->is_monster() == true)
		{
			if (ch->is_revive() == false && ch->has_reviver_in_party() == true)
			{
				ch->set_position(POS_STANDING);
				ch->set_hp(ch->max_hp());

				ch->view_reencode();

				ch->set_aggressive();
				ch->set_revive(true);

				return 0;
			}
		}

		character_manager::instance().destroy_character(ch);
	}

	return 0;
}

bool character::is_dead() const
{
	if (points_instant_.position == POS_DEAD)
		return true;

	return false;
}

static int get_gold_multipler() { return metin2::config::game().distribution_test_server ? 3 : 1; }

void character::reward_gold(character * pkAttacker)
{
	// ADD_PREMIUM
	bool isAutoLoot = 
		(pkAttacker->premium_remain_seconds(PREMIUM_AUTOLOOT) > 0 ||
		 pkAttacker->is_equip_unique_group(UNIQUE_GROUP_AUTOLOOT))
		? true : false; // Third Hand
	// END_OF_ADD_PREMIUM

	pixel_position pos;

	if (!isAutoLoot)
		if (!sectree_manager::instance().movable_position(map_index(), x(), y(), pos))
			return;

	int iTotalGold = 0;
	//
	// --------- Gold drop chance calculation ----------
	//
	int iGoldPercent = mob_rank_stats[get_mob_rank()].gold_percent;

	if (pkAttacker->is_pc())
		iGoldPercent = iGoldPercent * (100 + priv_manager::instance().priv(pkAttacker, PRIV_GOLD_DROP)) / 100;

	if (pkAttacker->point(POINT_MALL_GOLDBONUS))
		iGoldPercent += (iGoldPercent * pkAttacker->point(POINT_MALL_GOLDBONUS) / 100);

	iGoldPercent = iGoldPercent * character_manager::instance().mob_gold_drop_rate(pkAttacker) / 100;

	// ADD_PREMIUM
	if (pkAttacker->premium_remain_seconds(PREMIUM_GOLD) > 0 ||
			pkAttacker->is_equip_unique_group(UNIQUE_GROUP_LUCKY_GOLD))
		iGoldPercent += iGoldPercent;
	// END_OF_ADD_PREMIUM

	if (iGoldPercent > 100) 
		iGoldPercent = 100;

	int iPercent;

	if (get_mob_rank() >= MOB_RANK_BOSS)
		iPercent = ((iGoldPercent * percent_lvdelta_boss(pkAttacker->level(), level())) / 100);
	else
		iPercent = ((iGoldPercent * percent_lvdelta(pkAttacker->level(), level())) / 100);

	if (number(1, 100) > iPercent)
		return;

	int iGoldMultipler = get_gold_multipler();

	if (1 == number(1, 50000)) // 1/50000 chance of 10x gold
		iGoldMultipler *= 10;
	else if (1 == number(1, 10000)) // 1/10000 chance of 5x gold
		iGoldMultipler *= 5;

	// Personal modifier
	if (pkAttacker->point(POINT_GOLD_DOUBLE_BONUS))
		if (number(1, 100) <= pkAttacker->point(POINT_GOLD_DOUBLE_BONUS))
			iGoldMultipler *= 2;

	//
	// --------- Determine gold drop multiplier ----------
	//
	if (metin2::config::test_server())
		pkAttacker->chat_packet(CHAT_TYPE_PARTY, "gold_mul %d rate %d", iGoldMultipler, character_manager::instance().mob_gold_amount_rate(pkAttacker));

	//
	// --------- Actual drop handling -------------
	// 
	item * item;

	int iGold10DropPct = 100;
	iGold10DropPct = (iGold10DropPct * 100) / (100 + priv_manager::instance().priv(pkAttacker, PRIV_GOLD10_DROP));

	// If MOB_RANK is higher than BOSS, always a gold bomb
	if (get_mob_rank() >= MOB_RANK_BOSS && !is_stone() && get_mob_table().gold_max != 0)
	{
		if (1 == number(1, iGold10DropPct))
			iGoldMultipler *= 10; // 1% chance of 10x gold

		int iSplitCount = number(25, 35);

		for (int i = 0; i < iSplitCount; ++i)
		{
			int iGold = number(get_mob_table().gold_min, get_mob_table().gold_max) / iSplitCount;
			if (metin2::config::test_server())
				metin2::log::info( "iGold {:d}", iGold);
			iGold = iGold * character_manager::instance().mob_gold_amount_rate(pkAttacker) / 100;
			iGold *= iGoldMultipler;

			if (iGold == 0)
			{
				continue ;
			}

			if (metin2::config::test_server())
			{
				metin2::log::info( "Drop Moeny MobGoldAmountRate {:d} {:d}", character_manager::instance().mob_gold_amount_rate(pkAttacker), iGoldMultipler);
				metin2::log::info( "Drop Money gold {:d} GoldMin {:d} GoldMax {:d}", iGold, get_mob_table().gold_max, get_mob_table().gold_max);
			}

			// NOTE: gold bombs do not go through Third Hand handling
			if ((item = item_manager::instance().create_item(1, iGold)))
			{
				pos.x = x() + ((number(-14, 14) + number(-14, 14)) * 23);
				pos.y = y() + ((number(-14, 14) + number(-14, 14)) * 23);

				item->add_to_ground(map_index(), pos);
				item->start_destroy_event();

				iTotalGold += iGold; // Total gold
			}
		}
	}
	// 1% chance to drop 10 piles of gold. (10x drop)
	else if (1 == number(1, iGold10DropPct))
	{
		//
		// Gold-bomb style drop
		//
		for (int i = 0; i < 10; ++i)
		{
			int iGold = number(get_mob_table().gold_min, get_mob_table().gold_max);
			iGold = iGold * character_manager::instance().mob_gold_amount_rate(pkAttacker) / 100;
			iGold *= iGoldMultipler;

			if (iGold == 0)
			{
				continue;
			}

			// NOTE: gold bombs do not go through Third Hand handling
			if ((item = item_manager::instance().create_item(1, iGold)))
			{
				pos.x = x() + (number(-7, 7) * 20);
				pos.y = y() + (number(-7, 7) * 20);

				item->add_to_ground(map_index(), pos);
				item->start_destroy_event();

				iTotalGold += iGold; // Total gold
			}
		}
	}
	else
	{
		//
		// Normal gold drop
		//
		int iGold = number(get_mob_table().gold_min, get_mob_table().gold_max);
		iGold = iGold * character_manager::instance().mob_gold_amount_rate(pkAttacker) / 100;
		iGold *= iGoldMultipler;

		int iSplitCount;

		if (iGold >= 3) 
			iSplitCount = number(1, 3);
		else if (get_mob_rank() >= MOB_RANK_BOSS)
		{
			iSplitCount = number(3, 10);

			if ((iGold / iSplitCount) == 0)
				iSplitCount = 1;
		}
		else
			iSplitCount = 1;

		if (iGold != 0)
		{
			iTotalGold += iGold; // Total gold

			for (int i = 0; i < iSplitCount; ++i)
			{
				if (isAutoLoot)
				{
					pkAttacker->give_gold(iGold / iSplitCount);
				}
				else if ((item = item_manager::instance().create_item(1, iGold / iSplitCount)))
				{
					pos.x = x() + (number(-7, 7) * 20);
					pos.y = y() + (number(-7, 7) * 20);

					item->add_to_ground(map_index(), pos);
					item->start_destroy_event();
				}
			}
		}
	}

	db_manager::instance().send_money_log(MONEY_LOG_MONSTER, race_num(), iTotalGold);
}

void character::reward(bool bItemDrop)
{
	if (race_num() == 5001) // Pirates (Waegu) always drop gold
	{
		pixel_position pos;

		if (!sectree_manager::instance().movable_position(map_index(), x(), y(), pos))
			return;

		item * item;
		int iGold = number(get_mob_table().gold_min, get_mob_table().gold_max);
		iGold = iGold * character_manager::instance().mob_gold_amount_rate(nullptr) / 100;
		iGold *= get_gold_multipler();
		int iSplitCount = number(25, 35);

		metin2::log::info( "WAEGU Dead gold {:d} split {:d}", iGold, iSplitCount);

		for (int i = 1; i <= iSplitCount; ++i)
		{
			if ((item = item_manager::instance().create_item(1, iGold / iSplitCount)))
			{
				if (i != 0)
				{
					pos.x = number(-7, 7) * 20;
					pos.y = number(-7, 7) * 20;

					pos.x += x();
					pos.y += y();
				}

				item->add_to_ground(map_index(), pos);
				item->start_destroy_event();
			}
		}
		return;
	}

   	character * pkAttacker = distribute_exp();

	if (!pkAttacker)
		return;

	if (pkAttacker->is_pc())
	{
		if (level() - pkAttacker->level() >= -10)
		{
			if (pkAttacker->real_alignment() < 0)
			{
				if (pkAttacker->is_equip_unique_item(UNIQUE_ITEM_FASTER_ALIGNMENT_UP_BY_KILL))
					pkAttacker->update_alignment(14);
				else
					pkAttacker->update_alignment(7);
			}
			else
				pkAttacker->update_alignment(2);
		}

		pkAttacker->set_quest_npcid(get_vid());
		quest::quest_manager::instance().kill(pkAttacker->player_id(), race_num());
		character_manager::instance().kill_log(race_num());

		if (!number(0, 9))
		{
			if (pkAttacker->point(POINT_KILL_HP_RECOVERY))
			{
				int iHP = pkAttacker->max_hp() * pkAttacker->point(POINT_KILL_HP_RECOVERY) / 100;
				pkAttacker->point_change(POINT_HP, iHP);
				create_fly(FLY_HP_SMALL, pkAttacker);
			}

			if (pkAttacker->point(POINT_KILL_SP_RECOVER))
			{
				int iSP = pkAttacker->max_sp() * pkAttacker->point(POINT_KILL_SP_RECOVER) / 100;
				pkAttacker->point_change(POINT_SP, iSP);
				create_fly(FLY_SP_SMALL, pkAttacker);
			}
		}
	}

	if (!bItemDrop)
		return;

	pixel_position pos = xyz();

	if (!sectree_manager::instance().movable_position(map_index(), pos.x, pos.y, pos))
		return;

	//
	// Gold drop
	//
	if (metin2::config::test_server())
		metin2::log::info( "Drop money : Attacker {}", pkAttacker->name());
	reward_gold(pkAttacker);

	//
	// Item drop
	//
	game::items::item * item;

	static std::vector<game::items::item *> s_vec_item;
	s_vec_item.clear();

	if (item_manager::instance().create_drop_item(this, pkAttacker, s_vec_item))
	{
		if (s_vec_item.size() == 0);
		else if (s_vec_item.size() == 1)
		{
			item = s_vec_item[0];
			item->add_to_ground(map_index(), pos);

			if (battle_arena::instance().is_battle_arena_map(pkAttacker->map_index()) == false)
			{
				item->set_ownership(pkAttacker);
			}

			item->start_destroy_event();

			pos.x = number(-7, 7) * 20;
			pos.y = number(-7, 7) * 20;
			pos.x += x();
			pos.y += y();

			metin2::log::info( "DROP_ITEM: {} {:d} {:d} from {}", item->name(), pos.x, pos.y, name());
		}
		else
		{
			int iItemIdx = s_vec_item.size() - 1;

			std::priority_queue<std::pair<int, character *> > pq;

			int total_dam = 0;

			for (damage_map::iterator it = map_k_damage_.begin(); it != map_k_damage_.end(); ++it)
			{
				int iDamage = it->second.total_damage;
				if (iDamage > 0)
				{
					character * ch = character_manager::instance().find(it->first);

					if (ch)
					{
						pq.push(std::make_pair(iDamage, ch));
						total_dam += iDamage;
					}
				}
			}

			std::vector<character *> v;

			while (!pq.empty() && pq.top().first * 10 >= total_dam)
			{
				v.push_back(pq.top().second);
				pq.pop();
			}

			if (v.empty())
			{
				// Nobody dealt notably more damage, so no ownership
				while (iItemIdx >= 0)
				{
					item = s_vec_item[iItemIdx--];

					if (!item)
					{
						metin2::log::error("item null in vector idx {:d}", iItemIdx + 1);
						continue;
					}

					item->add_to_ground(map_index(), pos);
					// No ownership among those who dealt 10% damage or less
					item->start_destroy_event();

					pos.x = number(-7, 7) * 20;
					pos.y = number(-7, 7) * 20;
					pos.x += x();
					pos.y += y();

					metin2::log::info( "DROP_ITEM: {} {:d} {:d} by {}", item->name(), pos.x, pos.y, name());
				}
			}
			else
			{
				// Ownership is shared only among those who dealt a lot of damage
				std::vector<character *>::iterator it = v.begin();

				while (iItemIdx >= 0)
				{
					item = s_vec_item[iItemIdx--];

					if (!item)
					{
						metin2::log::error("item null in vector idx {:d}", iItemIdx + 1);
						continue;
					}

					item->add_to_ground(map_index(), pos);

					character * ch = *it;

					if (ch->get_party())
						ch = ch->get_party()->next_ownership(ch, x(), y());

					++it;

					if (it == v.end())
						it = v.begin();

					if (battle_arena::instance().is_battle_arena_map(ch->map_index()) == false)
					{
						item->set_ownership(ch);
					}

					item->start_destroy_event();

					pos.x = number(-7, 7) * 20;
					pos.y = number(-7, 7) * 20;
					pos.x += x();
					pos.y += y();

					metin2::log::info( "DROP_ITEM: {} {:d} {:d} by {}", item->name(), pos.x, pos.y, name());
				}
			}
		}
	}

	map_k_damage_.clear();
}

struct item_drop_penalty
{
	int inventory_pct;		// Range: 1 ~ 1000
	int inventory_qty;		// Range: --
	int equipment_pct;		// Range: 1 ~ 100
	int equipment_qty;		// Range: --
};

item_drop_penalty item_drop_penalty_kor[9] =
{
	{   0,   0,  0,  0 },	// Noble King
	{   0,   0,  0,  0 },	// Hero
	{   0,   0,  0,  0 },	// Saint
	{   0,   0,  0,  0 },	// Wise
	{   0,   0,  0,  0 },	// Citizen
	{  25,   1,  5,  1 },	// Ronin
	{  50,   2, 10,  1 },	// Villain
	{  75,   4, 15,  1 },	// Demon Lord
	{ 100,   8, 20,  1 },	// Tyrant
};

void character::item_drop_penalty(character * pkKiller)
{
	// Do not drop items while a private shop is open.
	if (my_shop())
		return;

	if (level() < 50)
		return;
	
	if (battle_arena::instance().is_battle_arena_map(map_index()) == true)
	{
		return;
	}

	struct game::characters::item_drop_penalty * table = &item_drop_penalty_kor[0];

	if (level() < 10)
		return;

	int iAlignIndex;

	if (real_alignment() >= 120000)
		iAlignIndex = 0;
	else if (real_alignment() >= 80000)
		iAlignIndex = 1;
	else if (real_alignment() >= 40000)
		iAlignIndex = 2;
	else if (real_alignment() >= 10000)
		iAlignIndex = 3;
	else if (real_alignment() >= 0)
		iAlignIndex = 4;
	else if (real_alignment() > -40000)
		iAlignIndex = 5;
	else if (real_alignment() > -80000)
		iAlignIndex = 6;
	else if (real_alignment() > -120000)
		iAlignIndex = 7;
	else
		iAlignIndex = 8;

	std::vector<std::pair<item *, int> > vec_item;
	item * pkItem;
	int	i;
	bool isDropAllEquipments = false;

	game::characters::item_drop_penalty & r = table[iAlignIndex];
	metin2::log::info( "{} align {:d} inven_pct {:d} equip_pct {:d}", name(), iAlignIndex, r.inventory_pct, r.equipment_pct);

	bool bDropInventory = r.inventory_pct >= number(1, 1000);
	bool bDropEquipment = r.equipment_pct >= number(1, 100);
	bool bDropAntiDropUniqueItem = false;

	if ((bDropInventory || bDropEquipment) && is_equip_unique_item(UNIQUE_ITEM_SKIP_ITEM_DROP_PENALTY))
	{
		bDropInventory = false;
		bDropEquipment = false;
		bDropAntiDropUniqueItem = true;
	}

	if (bDropInventory) // Drop Inventory
	{
		std::vector<uint8_t> vec_bSlots;

		for (i = 0; i < INVENTORY_MAX_NUM; ++i)
			if (inventory_item(i))
				vec_bSlots.push_back(i);

		if (!vec_bSlots.empty())
		{
			std::random_device rd;
			std::mt19937 g(rd());
			std::shuffle(vec_bSlots.begin(), vec_bSlots.end(), g);

			int iQty = std::min<int>(vec_bSlots.size(), r.inventory_qty);

			if (iQty)
				iQty = number(1, iQty);

			for (i = 0; i < iQty; ++i)
			{
				pkItem = inventory_item(vec_bSlots[i]);

				if (is_set(pkItem->anti_flag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_PKDROP))
					continue;

				sync_quickslot(QUICKSLOT_TYPE_ITEM, vec_bSlots[i], 255);
				vec_item.push_back(std::make_pair(pkItem->remove_from_character(), INVENTORY));
			}
		}
		else if (iAlignIndex == 8)
			isDropAllEquipments = true;
	}

	if (bDropEquipment) // Drop Equipment
	{
		std::vector<uint8_t> vec_bSlots;

		for (i = 0; i < WEAR_MAX_NUM; ++i)
			if (wear(i))
				vec_bSlots.push_back(i);

		if (!vec_bSlots.empty())
		{
			std::random_device rd;
			std::mt19937 g(rd());
			std::shuffle(vec_bSlots.begin(), vec_bSlots.end(), g);
			int iQty;

			if (isDropAllEquipments)
				iQty = vec_bSlots.size();
			else
				iQty = std::min<int>(vec_bSlots.size(), number(1, r.equipment_qty));

			if (iQty)
				iQty = number(1, iQty);

			for (i = 0; i < iQty; ++i)
			{
				pkItem = wear(vec_bSlots[i]);

				if (is_set(pkItem->anti_flag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_PKDROP))
					continue;

				sync_quickslot(QUICKSLOT_TYPE_ITEM, vec_bSlots[i], 255);
				vec_item.push_back(std::make_pair(pkItem->remove_from_character(), EQUIPMENT));
			}
		}
	}

	if (bDropAntiDropUniqueItem)
	{
		item * pkItem;

		pkItem = wear(WEAR_UNIQUE1);

		if (pkItem && pkItem->vnum() == UNIQUE_ITEM_SKIP_ITEM_DROP_PENALTY)
		{
			sync_quickslot(QUICKSLOT_TYPE_ITEM, WEAR_UNIQUE1, 255);
			vec_item.push_back(std::make_pair(pkItem->remove_from_character(), EQUIPMENT));
		}

		pkItem = wear(WEAR_UNIQUE2);

		if (pkItem && pkItem->vnum() == UNIQUE_ITEM_SKIP_ITEM_DROP_PENALTY)
		{
			sync_quickslot(QUICKSLOT_TYPE_ITEM, WEAR_UNIQUE2, 255);
			vec_item.push_back(std::make_pair(pkItem->remove_from_character(), EQUIPMENT));
		}
	}

	{
		pixel_position pos;
		pos.x = x();
		pos.y = y();

		unsigned int i;

		for (i = 0; i < vec_item.size(); ++i)
		{
			item * item = vec_item[i].first;
			int window = vec_item[i].second;

			item->add_to_ground(map_index(), pos);
			item->start_destroy_event();

			metin2::log::info( "DROP_ITEM_PK: {} {:d} {:d} from {}", item->name(), pos.x, pos.y, name());
			log_manager::instance().item_log(this, item, "DEAD_DROP", (window == INVENTORY) ? "INVENTORY" : ((window == EQUIPMENT) ? "EQUIPMENT" : ""));

			pos.x = x() + number(-7, 7) * 20;
			pos.y = y() + number(-7, 7) * 20;
		}
	}
}

class f_party_alignment_compute
{
	public:
		f_party_alignment_compute(int iAmount, int x, int y)
		{
			amount = iAmount;
			count = 0;
			step = 0;
			killer_x = x;
			killer_y = y;
		}

		void operator () (character * pkChr)
		{
			if (distance_approx(pkChr->x() - killer_x, pkChr->y() - killer_y) < PARTY_DEFAULT_RANGE)
			{
				if (step == 0)
				{
					++count;
				}
				else
				{
					pkChr->update_alignment(amount / count);
				}
			}
		}

		int amount;
		int count;
		int step;

		int killer_x;
		int killer_y;
};



void character::dead(character * pkKiller, bool bImmediateDead)
{
	if (is_dead())
		return;

	{
		if (is_horse_riding())
		{
			stop_riding();
		}
		else if (get_mount_vnum())
		{
			remove_affect(AFFECT_MOUNT_BONUS);
			mount_vnum_ = 0;
			un_equip_special_ride_unique_item();

			update_packet();
		}
	}

	if (!pkKiller && killer_pid_)
		pkKiller = character_manager::instance().find_by_pid(killer_pid_);

	killer_pid_ = 0; // Must be initialized DO NOT DELETE THIS LINE UNLESS YOU ARE 1000000% SURE

	bool isAgreedPVP = false;
	bool isUnderGuildWar = false;
	bool isDuel = false;
	bool isForked = false;

	if (pkKiller && pkKiller->is_pc())
	{
		if (pkKiller->chr_target_ == this)
			pkKiller->set_target(nullptr);

		if (!is_pc() && pkKiller->get_dungeon())
			pkKiller->get_dungeon()->inc_kill_count(pkKiller, this);

		isAgreedPVP = pvp_manager::instance().dead(this, pkKiller->player_id());
		isDuel = arena_manager::instance().on_dead(pkKiller, this);

		if (is_pc())
		{
			guild * g1 = get_guild();
			guild * g2 = pkKiller->get_guild();

			if (g1 && g2)
				if (g1->under_war(g2->id()))
					isUnderGuildWar = true;

			pkKiller->set_quest_npcid(get_vid());
			quest::quest_manager::instance().kill(pkKiller->player_id(), quest::QUEST_NO_NPC);
			guild_manager::instance().kill(pkKiller, this);
		}
	}

	//CHECK_FORKEDROAD_WAR
	if (is_pc())
	{
		if (three_way_war::instance().is_three_way_war_map_index(map_index()))
			isForked = true;
	}
	//END_CHECK_FORKEDROAD_WAR

	if (pkKiller &&
			!isAgreedPVP &&
			!isUnderGuildWar &&
			is_pc() &&
			!isDuel &&
			!isForked &&
			!is_castle_map(map_index()))
	{
		if (gm_level() == GM_PLAYER || metin2::config::test_server())
		{
			item_drop_penalty(pkKiller);
		}
	}

	// CASTLE_SIEGE
	if (is_castle_map(map_index()))
	{
		if (CASTLE_FROG_VNUM == race_num())
			castle_frog_die(this, pkKiller);
		else if (castle_is_guard_vnum(race_num()))
			castle_guard_die(this, pkKiller);
		else if (castle_is_tower_vnum(race_num()))
			castle_tower_die(this, pkKiller);
	}
	// CASTLE_SIEGE

	if (true == isForked)
	{
		three_way_war::instance().on_dead( this, pkKiller );
	}

	set_position(POS_DEAD);
	clear_affect(true);

	if (pkKiller && is_pc())
	{
		if (!pkKiller->is_pc())
		{
			if (!isForked)
			{
				metin2::log::debug( "DEAD: {} {} WITH PENALTY", name(), static_cast<const void *>(this));
				set_bit(points_instant_.instant_flag, INSTANT_FLAG_DEATH_PENALTY);
				log_manager::instance().char_log(this, pkKiller->race_num(), "DEAD_BY_NPC", pkKiller->name());
			}
		}
		else
		{
			metin2::log::debug( "DEAD_BY_PC: {} {} KILLER {} {}", name(), static_cast<const void *>(this), pkKiller->name(), static_cast<const void *>(pkKiller));
			remove_bit(points_instant_.instant_flag, INSTANT_FLAG_DEATH_PENALTY);

			if (empire() != pkKiller->empire())
			{
				int iEP = std::min(point(POINT_EMPIRE_POINT), pkKiller->point(POINT_EMPIRE_POINT));

				point_change(POINT_EMPIRE_POINT, -(iEP / 10));
				pkKiller->point_change(POINT_EMPIRE_POINT, iEP / 5);

				if (point(POINT_EMPIRE_POINT) < 10)
				{
					// TODO : need to add code that sends them to the entrance.
				}

				char buf[256];
				snprintf(buf, sizeof(buf),
						"%d %d %d %s %d %d %d %s",
						empire(), alignment(), pk_mode(), name(),
						pkKiller->empire(), pkKiller->alignment(), pkKiller->pk_mode(), pkKiller->name());

				log_manager::instance().char_log(this, pkKiller->player_id(), "DEAD_BY_PC", buf);
			}
			else
			{
				if (!isAgreedPVP && !isUnderGuildWar && !is_killer_mode() && alignment() >= 0 && !isDuel && !isForked)
				{
					int iNoPenaltyProb = 0;

					if (pkKiller->alignment() >= 0)	// 1/3 percent down
						iNoPenaltyProb = 33;
					else				// 4/5 percent down
						iNoPenaltyProb = 20;

					if (number(1, 100) < iNoPenaltyProb)
						pkKiller->chat_packet(CHAT_TYPE_INFO, lc_text("용신의 보호로 아이템이 떨어지지 않았습니다."));
					else
					{
						if (pkKiller->get_party())
						{
							f_party_alignment_compute f(-20000, pkKiller->x(), pkKiller->y());
							pkKiller->get_party()->for_each_online_member(f);

							if (f.count == 0)
								pkKiller->update_alignment(-20000);
							else
							{
								metin2::log::info( "ALIGNMENT PARTY count {:d} amount {:d}", f.count, f.amount);

								f.step = 1;
								pkKiller->get_party()->for_each_online_member(f);
							}
						}
						else
							pkKiller->update_alignment(-20000);
					}
				}

				char buf[256];
				snprintf(buf, sizeof(buf),
						"%d %d %d %s %d %d %d %s",
						empire(), alignment(), pk_mode(), name(),
						pkKiller->empire(), pkKiller->alignment(), pkKiller->pk_mode(), pkKiller->name());

				log_manager::instance().char_log(this, pkKiller->player_id(), "DEAD_BY_PC", buf);
			}
		}
	}
	else
	{
		metin2::log::debug( "DEAD: {} {}", name(), static_cast<const void *>(this));
		remove_bit(points_instant_.instant_flag, INSTANT_FLAG_DEATH_PENALTY);
	}

	clear_sync();

	event_cancel(&stun_event); // Kill the stun event.

	if (is_pc())
	{
		last_dead_time_ = get_dword_time();
		set_killer_mode(false);
		get_desc()->set_phase(PHASE_DEAD);
	}
	else
	{
		// Monsters attacked by guards must give no reward.
		if (!is_set(points_instant_.instant_flag, INSTANT_FLAG_NO_REWARD))
		{
			if (!(pkKiller && pkKiller->is_pc() && pkKiller->get_guild() && pkKiller->get_guild()->under_any_war(GUILD_WAR_TYPE_FIELD)))
			{
				// Reviving monsters give no reward.
				if (get_mob_table().resurrection_vnum)
				{
					// DUNGEON_MONSTER_REBIRTH_BUG_FIX
					character * chResurrect = character_manager::instance().spawn_mob(get_mob_table().resurrection_vnum, map_index(), x(), y(), z(), true, (int) rotation());
					if (get_dungeon() && chResurrect)
					{
						chResurrect->set_dungeon(get_dungeon());
					}
					// END_OF_DUNGEON_MONSTER_REBIRTH_BUG_FIX

					reward(false);
				}
				else if (is_revive() == true)
				{
					reward(false);
				}
				else
				{
					reward(true); // Drops gold, item, etc..
				}
			}
			else
			{
				if (pkKiller->under_guild_war_info_message_time_ < get_dword_time())
				{
					pkKiller->under_guild_war_info_message_time_ = get_dword_time() + 60000;
					pkKiller->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드전중에는 사냥에 따른 이익이 없습니다."));
				}
			}
		}
	}

	// BOSS_KILL_LOG
	if (get_mob_rank() >= MOB_RANK_BOSS && pkKiller && pkKiller->is_pc())
	{
		char buf[51];
		snprintf(buf, sizeof(buf), "%d %d", metin2::config::server().channel, pkKiller->map_index());
		if (is_stone())
			log_manager::instance().char_log(pkKiller, race_num(), "STONE_KILL", buf);
		else
			log_manager::instance().char_log(pkKiller, race_num(), "BOSS_KILL", buf);
	}
	// END_OF_BOSS_KILL_LOG

	packet_gc_dead pack;
	pack.header	= HEADER_GC_DEAD;
	pack.vid	= vid_;
	packet_around(&pack, sizeof(pack));

	remove_bit(points_instant_.instant_flag, INSTANT_FLAG_STUN);

	// If player character
	if (get_desc() != nullptr) {
		//
		// Resend the effect packet to the client.
		//
		auto it = list_pk_affect_.begin();

		while (it != list_pk_affect_.end())
			send_affect_add_packet(get_desc(), *it++);
	}

	//
	// Create the Dead event,
	//
	// For monsters, the Dead event destroys them after a few seconds,
	// for PCs, it makes them come out in town after 3 minutes. Within 3 minutes the user
	// decides whether to restart in town or here.
	if (isDuel == false)
	{
		if (dead_event)
		{
			metin2::log::debug( "DEAD_EVENT_CANCEL: {} {} {}", name(), static_cast<const void *>(this), static_cast<const void *>(dead_event.get()));
			event_cancel(&dead_event);
		}

		if (is_stone())
			clear_stone();

		if (get_dungeon())
		{
			get_dungeon()->dead_character(this);
		}

		char_dead_event_info* pEventInfo = alloc_event_info<char_dead_event_info>();

		if (is_pc())
		{
			pEventInfo->is_pc = true;
			pEventInfo->id = this->player_id();

			dead_event = event_create(game::characters::dead_event, pEventInfo, passes_per_sec(180));
		}
		else
		{
			pEventInfo->is_pc = false;
			pEventInfo->id = this->get_vid();

			if (is_revive() == false && has_reviver_in_party() == true)
			{
				dead_event = event_create(game::characters::dead_event, pEventInfo, bImmediateDead ? 1 : passes_per_sec(3));
			}
			else
			{
				dead_event = event_create(game::characters::dead_event, pEventInfo, bImmediateDead ? 1 : passes_per_sec(10));
			}
		}

		metin2::log::debug( "DEAD_EVENT_CREATE: {} {} {}", name(), static_cast<const void *>(this), static_cast<const void *>(dead_event.get()));
	}

	if (exchange_ != nullptr)
	{
		exchange_->cancel();
	}

	if (is_cube_open() == true)
	{
		cube_close(this);
	}
	
	shop_manager::instance().stop_shopping(this);
	close_my_shop();
	close_safebox();

	if (true == is_monster() && 2493 == get_mob_table().vnum)
	{
		if (nullptr != pkKiller && nullptr != pkKiller->get_guild())
		{
			dragon_lair_manager::instance().on_dragon_dead( this, pkKiller->get_guild()->id() );
		}
		else
		{
			metin2::log::error("DragonLair: Dragon killed by nobody");
		}
	}
}


void character::set_last_attacked(uint32_t dwTime)
{
	assert(mob_inst != nullptr);

	mob_inst->last_attacked_time = dwTime;
	mob_inst->pos_last_attacked = xyz();
}

void character::send_damage_packet(character * pAttacker, int Damage, uint8_t DamageFlag)
{
	if (is_pc() == true || (pAttacker->is_pc() == true && pAttacker->target() == this))
	{
		packet_gc_damage_info damageInfo;
		memset(&damageInfo, 0, sizeof(packet_gc_damage_info));

		damageInfo.header = HEADER_GC_DAMAGE_INFO;
		damageInfo.vid = (uint32_t)get_vid();
		damageInfo.flag = DamageFlag;
		damageInfo.damage = Damage;

		if (get_desc() != nullptr)
		{
			get_desc()->packet(&damageInfo, sizeof(packet_gc_damage_info));
		}

		if (pAttacker->get_desc() != nullptr)
		{
			pAttacker->get_desc()->packet(&damageInfo, sizeof(packet_gc_damage_info));
		}
	}
}

//
// CHARACTER::Damage method makes this take damage.
//
// Arguments
//    pAttacker		: attacker
//    dam		: damage
//    EDamageType	: what kind of attack?
//    
// Return value
//    true		: dead
//    false		: not dead yet
// 
bool character::damage(character * pAttacker, int dam, damage_type type) // returns true if dead
{
	if (DAMAGE_TYPE_MAGIC == type)
	{
		dam = (int)((float)dam * (100 + (pAttacker->point(POINT_MAGIC_ATT_BONUS_PER) + pAttacker->point(POINT_MELEE_MAGIC_ATT_BONUS_PER))) / 100.f + 0.5f);
	}
	if (race_num() == 5001)
	{
		bool bDropMoney = false;
		int iPercent = (hp() * 100) / max_hp();

		if (iPercent <= 10 && max_sp() < 5)
		{
			set_max_sp(5);
			bDropMoney = true;
		}
		else if (iPercent <= 20 && max_sp() < 4)
		{
			set_max_sp(4);
			bDropMoney = true;
		}
		else if (iPercent <= 40 && max_sp() < 3)
		{
			set_max_sp(3);
			bDropMoney = true;
		}
		else if (iPercent <= 60 && max_sp() < 2)
		{
			set_max_sp(2);
			bDropMoney = true;
		}
		else if (iPercent <= 80 && max_sp() < 1)
		{
			set_max_sp(1);
			bDropMoney = true;
		}

		if (bDropMoney)
		{
			uint32_t dwGold = 1000;
			int iSplitCount = number(10, 13);

			metin2::log::info( "WAEGU DropGoldOnHit {:d} times", max_sp());

			for (int i = 1; i <= iSplitCount; ++i)
			{
				pixel_position pos;
				item * item;

				if ((item = item_manager::instance().create_item(1, dwGold / iSplitCount)))
				{
					if (i != 0)
					{
						pos.x = (number(-14, 14) + number(-14, 14)) * 20;
						pos.y = (number(-14, 14) + number(-14, 14)) * 20;

						pos.x += x();
						pos.y += y();
					}

					item->add_to_ground(map_index(), pos);
					item->start_destroy_event();
				}
			}
		}
	}

	// Handle fear when not a normal attack
	if (type != DAMAGE_TYPE_NORMAL && type != DAMAGE_TYPE_NORMAL_RANGE)
	{
		if (is_affect_flag(AFF_TERROR))
		{
			int pct = skill_power(SKILL_TERROR) / 400;

			if (number(1, 100) <= pct)
				return false;
		}
	}

	int iCurHP = hp();
	int iCurSP = sp();

	bool IsCritical = false;
	bool IsPenetrate = false;
	bool IsDeathBlow = false;

	enum damage_flag
	{
		DAMAGE_NORMAL	= (1 << 0),
		DAMAGE_POISON	= (1 << 1),
		DAMAGE_DODGE	= (1 << 2),
		DAMAGE_BLOCK	= (1 << 3),
		DAMAGE_PENETRATE= (1 << 4),
		DAMAGE_CRITICAL = (1 << 5),
	};

	//
	// Magic and ranged skills (archer/assassin) get critical and penetration calculation.
	// Originally this shouldn't be done, but since a nerf (down-balance) patch wasn't possible, critical and
	// penetration are applied at /2 or more rather than their original values.
	// 
	// Melee skills added too, due to lots of complaints about warriors
	//
	// 20091109 : concluded that warriors ended up hugely overpowered; warrior ratio in Germany near 70%
	//
	if (type == DAMAGE_TYPE_MELEE || type == DAMAGE_TYPE_RANGE || type == DAMAGE_TYPE_MAGIC)
	{
		if (pAttacker)
		{
			// Critical
			int iCriticalPct = pAttacker->point(POINT_CRITICAL_PCT);

			if (!is_pc())
				iCriticalPct += pAttacker->marriage_bonus(UNIQUE_ITEM_MARRIAGE_CRITICAL_BONUS);

			if (iCriticalPct)
			{
				if (iCriticalPct >= 10) // Above 10: 5% + (1% per 4), so a value of 50 gives 20%
					iCriticalPct = 5 + (iCriticalPct - 10) / 4;
				else // Below 10: simply halved, 10 = 5%
					iCriticalPct /= 2;

				//Apply critical resistance.
				iCriticalPct -= point(POINT_RESIST_CRITICAL);

				if (number(1, 100) <= iCriticalPct)
				{
					IsCritical = true;
					dam *= 2;
					effect_packet(SE_CRITICAL);

					if (is_affect_flag(AFF_MANASHIELD))
					{
						remove_affect(AFF_MANASHIELD);
					}
				}
			}

			// Penetration
			int iPenetratePct = pAttacker->point(POINT_PENETRATE_PCT);

			if (!is_pc())
				iPenetratePct += pAttacker->marriage_bonus(UNIQUE_ITEM_MARRIAGE_PENETRATE_BONUS);


			if (iPenetratePct)
			{
				{
					skill_proto* pkSk = skill_manager::instance().get(SKILL_RESIST_PENETRATE);

					if (nullptr != pkSk)
					{
						pkSk->set_point_var("k", 1.0f * skill_power(SKILL_RESIST_PENETRATE) / 100.0f);

						iPenetratePct -= static_cast<int>(pkSk->point_poly.eval());
					}
				}

				if (iPenetratePct >= 10)
				{
					// Above 10: 5% + (1% per 4), so a value of 50 gives 20%
					iPenetratePct = 5 + (iPenetratePct - 10) / 4;
				}
				else
				{
					// Below 10: simply halved, 10 = 5%
					iPenetratePct /= 2;
				}

				//Apply penetration resistance.
				iPenetratePct -= point(POINT_RESIST_PENETRATE);

				if (number(1, 100) <= iPenetratePct)
				{
					IsPenetrate = true;

					if (metin2::config::test_server())
						chat_packet(CHAT_TYPE_INFO, lc_text("관통 추가 데미지 %d"), point(POINT_DEF_GRADE) * (100 + point(POINT_DEF_BONUS)) / 100);

					dam += point(POINT_DEF_GRADE) * (100 + point(POINT_DEF_BONUS)) / 100;

					if (is_affect_flag(AFF_MANASHIELD))
					{
						remove_affect(AFF_MANASHIELD);
					}
				}
			}
		}
	}
	// 
	// Attribute values are calculated only for combo and bow attacks, i.e. normal attacks.
	//
	else if (type == DAMAGE_TYPE_NORMAL || type == DAMAGE_TYPE_NORMAL_RANGE)
	{
		if (type == DAMAGE_TYPE_NORMAL)
		{
			// Melee normal attacks can be blocked
			if (point(POINT_BLOCK) && number(1, 100) <= point(POINT_BLOCK))
			{
				if (metin2::config::test_server())
				{
					pAttacker->chat_packet(CHAT_TYPE_INFO, lc_text("%s 블럭! (%d%%)"), name(), point(POINT_BLOCK));
					chat_packet(CHAT_TYPE_INFO, lc_text("%s 블럭! (%d%%)"), name(), point(POINT_BLOCK));
				}

				send_damage_packet(pAttacker, 0, DAMAGE_BLOCK);
				return false;
			}
		}
		else if (type == DAMAGE_TYPE_NORMAL_RANGE)
		{
			// Ranged normal attacks can be dodged
			if (point(POINT_DODGE) && number(1, 100) <= point(POINT_DODGE))
			{
				if (metin2::config::test_server())
				{
					pAttacker->chat_packet(CHAT_TYPE_INFO, lc_text("%s 회피! (%d%%)"), name(), point(POINT_DODGE));
					chat_packet(CHAT_TYPE_INFO, lc_text("%s 회피! (%d%%)"), name(), point(POINT_DODGE));
				}

				send_damage_packet(pAttacker, 0, DAMAGE_DODGE);
				return false;
			}
		}

		if (is_affect_flag(AFF_JEONGWIHON))
			dam = (int) (dam * (100 + skill_power(SKILL_JEONGWI) * 25 / 100) / 100);

		if (is_affect_flag(AFF_TERROR))
			dam = (int) (dam * (95 - skill_power(SKILL_TERROR) / 5) / 100);

		if (is_affect_flag(AFF_HOSIN))
			dam = dam * (100 - point(POINT_RESIST_NORMAL_DAMAGE)) / 100;

		//
		// Apply attacker attributes
		//
		if (pAttacker)
		{
			if (type == DAMAGE_TYPE_NORMAL)
			{
				// Reflect
				if (point(POINT_REFLECT_MELEE))
				{
					int reflectDamage = dam * point(POINT_REFLECT_MELEE) / 100;

					// NOTE: design requested that if the attacker has IMMUNE_REFLECT, instead of not reflecting 
					// at all, the damage is fixed at 1/3.
					if (pAttacker->is_immune(IMMUNE_REFLECT))
						reflectDamage = int(reflectDamage / 3.0f + 0.5f);

					pAttacker->damage(this, reflectDamage, DAMAGE_TYPE_SPECIAL);
				}
			}

			// Critical
			int iCriticalPct = pAttacker->point(POINT_CRITICAL_PCT);

			if (!is_pc())
				iCriticalPct += pAttacker->marriage_bonus(UNIQUE_ITEM_MARRIAGE_CRITICAL_BONUS);

			if (iCriticalPct)
			{
				//Apply critical resistance.
				iCriticalPct -= point(POINT_RESIST_CRITICAL);

				if (number(1, 100) <= iCriticalPct)
				{
					IsCritical = true;
					dam *= 2;
					effect_packet(SE_CRITICAL);
				}
			}

			// Penetration
			int iPenetratePct = pAttacker->point(POINT_PENETRATE_PCT);

			if (!is_pc())
				iPenetratePct += pAttacker->marriage_bonus(UNIQUE_ITEM_MARRIAGE_PENETRATE_BONUS);

			{
				skill_proto* pkSk = skill_manager::instance().get(SKILL_RESIST_PENETRATE);

				if (nullptr != pkSk)
				{
					pkSk->set_point_var("k", 1.0f * skill_power(SKILL_RESIST_PENETRATE) / 100.0f);

					iPenetratePct -= static_cast<int>(pkSk->point_poly.eval());
				}
			}


			if (iPenetratePct)
			{
				
				//Apply penetration resistance.
				iPenetratePct -= point(POINT_RESIST_PENETRATE);

				if (number(1, 100) <= iPenetratePct)
				{
					IsPenetrate = true;

					if (metin2::config::test_server())
						chat_packet(CHAT_TYPE_INFO, lc_text("관통 추가 데미지 %d"), point(POINT_DEF_GRADE) * (100 + point(POINT_DEF_BONUS)) / 100);
					dam += point(POINT_DEF_GRADE) * (100 + point(POINT_DEF_BONUS)) / 100;
				}
			}

			// HP steal
			if (pAttacker->point(POINT_STEAL_HP))
			{
				int pct = 1;

				if (number(1, 10) <= pct)
				{
					int iHP = std::min(dam, std::max(0, iCurHP)) * pAttacker->point(POINT_STEAL_HP) / 100;

					if (iHP > 0 && hp() >= iHP)
					{
						create_fly(FLY_HP_SMALL, pAttacker);
						pAttacker->point_change(POINT_HP, iHP);
						point_change(POINT_HP, -iHP);
					}
				}
			}

			// SP steal
			if (pAttacker->point(POINT_STEAL_SP))
			{
				int pct = 1;

				if (number(1, 10) <= pct)
				{
					int iCur;

					if (is_pc())
						iCur = iCurSP;
					else
						iCur = iCurHP;

					int iSP = std::min(dam, std::max(0, iCur)) * pAttacker->point(POINT_STEAL_SP) / 100;

					if (iSP > 0 && iCur >= iSP)
					{
						create_fly(FLY_SP_SMALL, pAttacker);
						pAttacker->point_change(POINT_SP, iSP);

						if (is_pc())
							point_change(POINT_SP, -iSP);
					}
				}
			}

			// Gold steal
			if (pAttacker->point(POINT_STEAL_GOLD))
			{
				if (number(1, 100) <= pAttacker->point(POINT_STEAL_GOLD))
				{
					int iAmount = number(1, level());
					pAttacker->point_change(POINT_GOLD, iAmount);
					db_manager::instance().send_money_log(MONEY_LOG_MISC, 1, iAmount);
				}
			}

			// HP recovery on every hit
			if (pAttacker->point(POINT_HIT_HP_RECOVERY) && number(0, 4) > 0) // 80% chance
			{
				int i = ((iCurHP>=0)?std::min(dam, iCurHP):dam) * pAttacker->point(POINT_HIT_HP_RECOVERY) / 100; // Fix

				if (i)
				{
					create_fly(FLY_HP_SMALL, pAttacker);
					pAttacker->point_change(POINT_HP, i);
				}
			}

			// SP recovery on every hit
			if (pAttacker->point(POINT_HIT_SP_RECOVERY) && number(0, 4) > 0) // 80% chance
			{
				int i = ((iCurHP>=0)?std::min(dam, iCurHP):dam) * pAttacker->point(POINT_HIT_SP_RECOVERY) / 100; // Fix

				if (i)
				{
					create_fly(FLY_SP_SMALL, pAttacker);
					pAttacker->point_change(POINT_SP, i);
				}
			}

			// Remove the target's mana.
			if (pAttacker->point(POINT_MANA_BURN_PCT))
			{
				if (number(1, 100) <= pAttacker->point(POINT_MANA_BURN_PCT))
					point_change(POINT_SP, -50);
			}
		}
	}

	//
	// Bonus damage/defense from normal attacks or skills
	// 
	switch (type)
	{
		case DAMAGE_TYPE_NORMAL:
		case DAMAGE_TYPE_NORMAL_RANGE:
			if (pAttacker)
				if (pAttacker->point(POINT_NORMAL_HIT_DAMAGE_BONUS))
					dam = dam * (100 + pAttacker->point(POINT_NORMAL_HIT_DAMAGE_BONUS)) / 100;

			dam = dam * (100 - std::min(99, point(POINT_NORMAL_HIT_DEFEND_BONUS))) / 100;
			break;

		case DAMAGE_TYPE_MELEE:
		case DAMAGE_TYPE_RANGE:
		case DAMAGE_TYPE_FIRE:
		case DAMAGE_TYPE_ICE:
		case DAMAGE_TYPE_ELEC:
		case DAMAGE_TYPE_MAGIC:
			if (pAttacker)
				if (pAttacker->point(POINT_SKILL_DAMAGE_BONUS))
					dam = dam * (100 + pAttacker->point(POINT_SKILL_DAMAGE_BONUS)) / 100;

			dam = dam * (100 - std::min(99, point(POINT_SKILL_DEFEND_BONUS))) / 100;
			break;

		default:
			break;
	}

	//
	// Mana shield (Dark Protection)
	//
	if (is_affect_flag(AFF_MANASHIELD))
	{
		// The smaller POINT_MANASHIELD is, the better
		int iDamageSPPart = dam / 3;
		int iDamageToSP = iDamageSPPart * point(POINT_MANASHIELD) / 100;
		int iSP = sp();

		// If there is SP, damage is always halved
		if (iDamageToSP <= iSP)
		{
			point_change(POINT_SP, -iDamageToSP);
			dam -= iDamageSPPart;
		}
		else
		{
			// When there isn't enough SP and more HP must be taken
			point_change(POINT_SP, -sp());
			dam -= iSP * 100 / std::max(point(POINT_MANASHIELD), 1);
		}
	}

	//
	// Overall defense increase (mall item)
	// 
	if (point(POINT_MALL_DEFBONUS) > 0)
	{
		int dec_dam = std::min(200, dam * point(POINT_MALL_DEFBONUS) / 100);
		dam -= dec_dam;
	}

	if (pAttacker)
	{
		//
		// Overall attack increase (mall item)
		//
		if (pAttacker->point(POINT_MALL_ATTBONUS) > 0)
		{
			int add_dam = std::min(300, dam * pAttacker->limit_point(POINT_MALL_ATTBONUS) / 100);
			dam += add_dam;
		}

		//
		// Empire bonus (old Korean version only)
		//
		int iEmpire = empire();
		int32_t lMapIndex = map_index();
		int iMapEmpire = sectree_manager::instance().empire_from_map_index(lMapIndex);


		if (pAttacker->is_pc())
		{
			iEmpire = pAttacker->empire();
			lMapIndex = pAttacker->map_index();
			iMapEmpire = sectree_manager::instance().empire_from_map_index(lMapIndex);

			// 10% less damage against people of another empire
			if (iEmpire && iMapEmpire && iEmpire != iMapEmpire)
			{
				int percent = 10;
				
				if (184 <= lMapIndex && lMapIndex <= 189)
				{
					percent = 9;
				}
				else
				{
					percent = 9;
				}

				dam = dam * percent / 10;
			}

			if (!is_pc() && monster_drain_sp_point())
			{
				int iDrain = monster_drain_sp_point();

				if (iDrain <= pAttacker->sp())
					pAttacker->point_change(POINT_SP, -iDrain);
				else
				{
					int iSP = pAttacker->sp();
					pAttacker->point_change(POINT_SP, -iSP);
				}
			}

		}
		else if (pAttacker->is_guard_npc())
		{
			set_bit(points_instant_.instant_flag, INSTANT_FLAG_NO_REWARD);
			stun();
			return true;
		}

		//
		// Monarch's Diamond Fist & Lion's Roar 
		//
		if (pAttacker->is_pc() && monarch::instance().is_power_up(pAttacker->empire()))
		{
			// 10% more damage
			dam += dam / 10;
		}

		if (is_pc() && monarch::instance().is_defence_up(empire()))
		{
			// 10% less damage
			dam -= dam / 10;
		}
	}

	if (!get_sectree() || get_sectree()->is_attr(x(), y(), ATTR_BANPK))
		return false;

	if (!is_pc())
	{
		if (party_ && party_->leader())
			party_->leader()->set_last_attacked(get_dword_time());
		else
			set_last_attacked(get_dword_time());

		// Monster dialogue: when hit
		monster_chat(MONSTER_CHAT_ATTACKED);
	}

	if (is_stun())
	{
		dead(pAttacker);
		return true;
	}

	if (is_dead())
		return true;

	// Prevent dying from poison attacks.
	if (type == DAMAGE_TYPE_POISON)
	{
		if (hp() - dam <= 0)
		{
			dam = hp() - 1;
		}
	}

	// ------------------------
	// German premium mode 
	// -----------------------

	// STONE SKIN : damage halved
	if (is_monster() && is_stone_skinner())
	{
		if (hp_pct() < get_mob_table().stone_skin_point)
			dam /= 2;
	}

	if (pAttacker)
	{
		// DEATH BLOW : chance of 4x damage (!? currently only used by event or siege monsters)
		if (pAttacker->is_monster() && pAttacker->is_death_blower())
		{
			if (pAttacker->is_death_blow())
			{
				//if (number(1, 4) == GetJob())
				if (number(JOB_WARRIOR, JOB_MAX_NUM - 1) == job())
				{
					IsDeathBlow = true;
					dam = dam * 4;
				}
			}
		}

		dam = blue_dragon_damage(this, pAttacker, dam);

		uint8_t damageFlag = 0;

		if (type == DAMAGE_TYPE_POISON)
			damageFlag = DAMAGE_POISON;
		else
			damageFlag = DAMAGE_NORMAL;

		if (IsCritical == true)
			damageFlag |= DAMAGE_CRITICAL;

		if (IsPenetrate == true)
			damageFlag |= DAMAGE_PENETRATE;


		//Final damage adjustment
		float damMul = this->dam_mul();
		float tempDam = dam;
		dam = tempDam * damMul + 0.5f;


		if (pAttacker)
			send_damage_packet(pAttacker, dam, damageFlag);

		if (metin2::config::test_server())
		{
			if(pAttacker)
			{
				pAttacker->chat_packet(CHAT_TYPE_INFO, "-> %s, DAM %d HP %d(%d%%) %s%s",
						name(), 
						dam, 
						hp(),
						(hp() * 100) / max_hp(),
						IsCritical ? "crit " : "",
						IsPenetrate ? "pene " : "",
						IsDeathBlow ? "deathblow " : "");
			}

			chat_packet(CHAT_TYPE_PARTY, "<- %s, DAM %d HP %d(%d%%) %s%s",
					pAttacker ? pAttacker->name() : nullptr,
					dam, 
					hp(),
					(hp() * 100) / max_hp(),
					IsCritical ? "crit " : "",
					IsPenetrate ? "pene " : "",
					IsDeathBlow ? "deathblow " : "");
		}

		if (detail_log_)
		{
			chat_packet(CHAT_TYPE_INFO, lc_text("%s[%d]가 공격 위치: %d %d"), pAttacker->name(), (uint32_t) pAttacker->get_vid(), pAttacker->x(), pAttacker->y());
		}
	}

	//
	// !!!!!!!!! Part that actually reduces HP !!!!!!!!!
	//
	if (!cannot_dead_)
	{
		point_change(POINT_HP, -dam, false);
	}

	if (pAttacker && dam > 0 && is_npc())
	{
		damage_map::iterator it = map_k_damage_.find(pAttacker->get_vid());

		if (it == map_k_damage_.end())
		{
			map_k_damage_.insert(damage_map::value_type(pAttacker->get_vid(), battle_info(dam, 0)));
			it = map_k_damage_.find(pAttacker->get_vid());
		}
		else
		{
			it->second.total_damage += dam;
		}

		start_recovery_event(); // Monsters start recovering when they take damage.

		update_aggr_point_ex(pAttacker, type, dam, it->second);
	}

	if (hp() <= 0)
	{
		stun();

		if (pAttacker && !pAttacker->is_npc())
			killer_pid_ = pAttacker->player_id();
		else
			killer_pid_ = 0;
	}

	return false;
}

void character::distribute_hp(character * pkKiller)
{
	if (pkKiller->get_dungeon()) // Dumplings don't appear inside dungeons
		return;
}

static void give_exp(character * from, character * to, int iExp)
{
	// Experience adjustment ratio by level difference
	iExp = calculate_value_lvdelta(to->level(), from->level(), iExp);

	// 3x experience bonus on external test server
	if (metin2::config::game().distribution_test_server)
		iExp *= 3;

	int iBaseExp = iExp;

	// Apply fortune-telling and company experience events
	iExp = iExp * (100 + priv_manager::instance().priv(to, PRIV_EXP_PCT)) / 100;

	// Default in-game experience bonus
	{
		// Labor Day medal
		if (to->is_equip_unique_item(UNIQUE_ITEM_LARBOR_MEDAL))
			iExp += iExp * 20 /100;

		// Demon Tower experience bonus
		if (to->map_index() >= 660000 && to->map_index() < 670000) 
			iExp += iExp * 20 / 100; // 1.2x (20%)

		// Item double-experience attribute
		if (to->point(POINT_EXP_DOUBLE_BONUS))
			if (number(1, 100) <= to->point(POINT_EXP_DOUBLE_BONUS))
				iExp += iExp * 30 / 100; // 1.3x (30%)

		// Ring of Experience (2-hour)
		if (to->is_equip_unique_item(UNIQUE_ITEM_DOUBLE_EXP))
			iExp += iExp * 50 / 100;

		switch (to->get_mount_vnum())
		{
			case 20110:
			case 20111:
			case 20112:
			case 20113:
				if (to->is_equip_unique_item(71115) || to->is_equip_unique_item(71117) || to->is_equip_unique_item(71119) ||
						to->is_equip_unique_item(71121) )
				{
					iExp += iExp * 10 / 100;
				}
				break;

			case 20114:
			case 20120:
			case 20121:
			case 20122:
			case 20123:
			case 20124:
			case 20125:
				// White Lion experience bonus
				iExp += iExp * 30 / 100;
				break;
		}
	}

	// Item mall experience bonus
	// Item mall: experience purchase
	if (to->premium_remain_seconds(PREMIUM_EXP) > 0)
	{
		iExp += (iExp * 50 / 100);
	}

	if (to->is_equip_unique_group(UNIQUE_GROUP_RING_OF_EXP) == true)
	{
		iExp += (iExp * 50 / 100);
	}

	// Marriage bonus
	iExp += iExp * to->marriage_bonus(UNIQUE_ITEM_MARRIAGE_EXP_BONUS) / 100;

	iExp += (iExp * to->point(POINT_RAMADAN_CANDY_BONUS_EXP)/100);
	iExp += (iExp * to->point(POINT_MALL_EXPBONUS)/100);
	iExp += (iExp * to->point(POINT_EXP)/100);

	if (metin2::config::test_server())
	{
		metin2::log::info( "Bonus Exp : Ramadan Candy: {:d} MallExp: {:d} PointExp: {:d}", 
				to->point(POINT_RAMADAN_CANDY_BONUS_EXP),
				to->point(POINT_MALL_EXPBONUS),
				to->point(POINT_EXP)
			   );
	}

	// Design-side adjustment value, 85% as of 2005.04.21
	iExp = iExp * character_manager::instance().mob_exp_rate(to) / 100;

	// Limit on experience gained at once
	iExp = std::min<int>(to->next_exp() / 10, iExp);

	if (metin2::config::test_server())
	{
		if (quest::quest_manager::instance().event_flag("exp_bonus_log") && iBaseExp>0)
			to->chat_packet(CHAT_TYPE_INFO, "exp bonus %d%%", (iExp-iBaseExp)*100/iBaseExp);
	}

	iExp = adjust_exp_by_level(to, iExp);

	to->point_change(POINT_EXP, iExp, true);
	from->create_fly(FLY_EXP, to);

	{
		character * you = to->marry_partner();
		// Love points rise when a married couple are in a party together
		if (you)
		{
			// 100 million is 100%
			uint32_t dwUpdatePoint = 2000*iExp/to->level()/to->level()/3;

			if (to->premium_remain_seconds(PREMIUM_MARRIAGE_FAST) > 0 || 
					you->premium_remain_seconds(PREMIUM_MARRIAGE_FAST) > 0)
				dwUpdatePoint = (uint32_t)(dwUpdatePoint * 3);

			marriage::marriage* pMarriage = marriage::manager::instance().get(to->player_id());

			// DIVORCE_NULL_BUG_FIX
			if (pMarriage && pMarriage->is_near())
				pMarriage->update(dwUpdatePoint);
			// END_OF_DIVORCE_NULL_BUG_FIX
		}
	}
}

namespace NPartyExpDistribute
{
	struct f_party_totaler
	{
		int		total;
		int		member_count;
		int		x, y;

		f_party_totaler(character * center)
			: total(0), member_count(0), x(center->x()), y(center->y())
		{};

		void operator () (character * ch)
		{
			if (distance_approx(ch->x() - x, ch->y() - y) <= PARTY_DEFAULT_RANGE)
			{
				total += party_exp_distribute_table[ch->level()];

				++member_count;
			}
		}
	};

	struct f_party_distributor
	{
		int		total;
		character *	c;
		int		x, y;
		uint32_t		i_exp;
		int		mode;
		int		member_count;

		f_party_distributor(character * center, int member_count_value, int total, uint32_t iExp, int iMode) 
			: total(total), c(center), x(center->x()), y(center->y()), i_exp(iExp), mode(iMode), member_count(member_count_value)
			{
				if (member_count == 0)
					member_count = 1;
			};

		void operator () (character * ch)
		{
			if (distance_approx(ch->x() - x, ch->y() - y) <= PARTY_DEFAULT_RANGE)
			{
				uint32_t iExp2 = 0;

				switch (mode)
				{
					case PARTY_EXP_DISTRIBUTION_NON_PARITY:
						iExp2 = (uint32_t) (i_exp * (float) party_exp_distribute_table[ch->level()] / total);
						break;

					case PARTY_EXP_DISTRIBUTION_PARITY:
						iExp2 = i_exp / member_count;
						break;

					default:
						metin2::log::error("Unknown party exp distribution mode {:d}", mode);
						return;
				}

				give_exp(c, ch, iExp2);
			}
		}
	};
}

struct damage_info {
	int dam;
	character * attacker;
	game::parties::party * party;

	void clear()
	{
		attacker = nullptr;
		party = nullptr;
	}

	inline void distribute(character * ch, int iExp)
	{
		if (attacker)
			give_exp(ch, attacker, iExp);
		else if (party)
		{
			NPartyExpDistribute::f_party_totaler f(ch);
			party->for_each_online_member(f);

			if (party->is_position_near_leader(ch))
				iExp = iExp * (100 + party->exp_bonus_percent()) / 100;

			if (metin2::config::test_server())
			{
				if (quest::quest_manager::instance().event_flag("exp_bonus_log") && party->exp_bonus_percent())
					party->chat_packet_to_all_member(CHAT_TYPE_INFO, "exp party bonus %d%%", party->exp_bonus_percent());
			}

			// Experience funneling (5% of party experience is taken and given first)
			if (party->exp_centralize_character())
			{
				character * tch = party->exp_centralize_character();

				if (distance_approx(ch->x() - tch->x(), ch->y() - tch->y()) <= PARTY_DEFAULT_RANGE)
				{
					int iExpCenteralize = (int) (iExp * 0.05f);
					iExp -= iExpCenteralize;

					give_exp(ch, party->exp_centralize_character(), iExpCenteralize);
				}
			}

			NPartyExpDistribute::f_party_distributor fDist(ch, f.member_count, f.total, iExp, party->exp_distribution_mode());
			party->for_each_online_member(fDist);
		}
	}
};

character * character::distribute_exp()
{
	int iExpToDistribute = get_exp();

	if (iExpToDistribute <= 0)
		return nullptr;

	int	iTotalDam = 0;
	character * pkChrMostAttacked = nullptr;
	int iMostDam = 0;

	typedef std::vector<damage_info> damage_info_list;
	damage_info_list damage_info_table;
	std::map<party *, damage_info> map_party_damage;

	damage_info_table.reserve(map_k_damage_.size());

	damage_map::iterator it = map_k_damage_.begin();

	// First filter out those not nearby. (50m)
	while (it != map_k_damage_.end())
	{
		const vid & c_VID = it->first;
		int iDam = it->second.total_damage;

		++it;

		character * pAttacker = character_manager::instance().find(c_VID);

		// Do NPCs even hit? -.-;
		if (!pAttacker || pAttacker->is_npc() || distance_approx(x()-pAttacker->x(), y()-pAttacker->y())>5000)
			continue;

		iTotalDam += iDam;
		if (!pkChrMostAttacked || iDam > iMostDam)
		{
			pkChrMostAttacked = pAttacker;
			iMostDam = iDam;
		}

		if (pAttacker->get_party())
		{
			std::map<party *, damage_info>::iterator it = map_party_damage.find(pAttacker->get_party());
			if (it == map_party_damage.end())
			{
				damage_info di;
				di.dam = iDam;
				di.attacker = nullptr;
				di.party = pAttacker->get_party();
				map_party_damage.insert(std::make_pair(di.party, di));
			}
			else
			{
				it->second.dam += iDam;
			}
		}
		else
		{
			damage_info di;

			di.dam = iDam;
			di.attacker = pAttacker;
			di.party = nullptr;

			damage_info_table.push_back(di);
		}
	}

	for (std::map<party *, damage_info>::iterator it = map_party_damage.begin(); it != map_party_damage.end(); ++it)
	{
		damage_info_table.push_back(it->second);
	}

	set_exp(0);

	if (iTotalDam == 0)	// Return if damage dealt is 0
		return nullptr;

	if (chr_stone_)	// If there is a stone, give half of the experience to the stone.
	{
		int iExp = iExpToDistribute >> 1;
		chr_stone_->set_exp(chr_stone_->get_exp() + iExp);
		iExpToDistribute -= iExp;
	}

	metin2::log::debug( "{} total exp: {:d}, damage_info_table.size() == {:d}, TotalDam {:d}",
			name(), iExpToDistribute, damage_info_table.size(), iTotalDam);

	if (damage_info_table.empty())
		return nullptr;

	// The one who dealt the most damage recovers HP.
	distribute_hp(pkChrMostAttacked);	// Dumpling system

	{
		// The person or party that dealt the most damage gets 20% of total experience + experience proportional to their damage.
		damage_info_list::iterator di = damage_info_table.begin();
		{
			damage_info_list::iterator it;

			for (it = damage_info_table.begin(); it != damage_info_table.end();++it)
			{
				if (it->dam > di->dam)
					di = it;
			}
		}

		int	iExp = iExpToDistribute / 5;
		iExpToDistribute -= iExp;

		float fPercent = (float) di->dam / iTotalDam;

		if (fPercent > 1.0f)
		{
			metin2::log::error("DistributeExp percent over 1.0 (fPercent {:f} name {})", fPercent, di->attacker->name());
			fPercent = 1.0f;
		}

		iExp += (int) (iExpToDistribute * fPercent);

		di->distribute(this, iExp);

		// Return if all 100% has been taken.
		if (fPercent == 1.0f)
			return pkChrMostAttacked;

		di->clear();
	}

	{
		// Distribute the remaining 80% of experience.
		damage_info_list::iterator it;

		for (it = damage_info_table.begin(); it != damage_info_table.end(); ++it)
		{
			damage_info & di = *it;

			float fPercent = (float) di.dam / iTotalDam;

			if (fPercent > 1.0f)
			{
				metin2::log::error("DistributeExp percent over 1.0 (fPercent {:f} name {})", fPercent, di.attacker->name());
				fPercent = 1.0f;
			}

			di.distribute(this, (int) (iExpToDistribute * fPercent));
		}
	}

	return pkChrMostAttacked;
}

// Returns the arrow count
int character::arrow_and_bow(item * * ppkBow, item * * ppkArrow, int iArrowCount/* = 1 */)
{
	item * pkBow;

	if (!(pkBow = wear(WEAR_WEAPON)) || pkBow->proto()->sub_type != WEAPON_BOW)
	{
		return 0;
	}

	item * pkArrow;

	if (!(pkArrow = wear(WEAR_ARROW)) || pkArrow->type() != ITEM_WEAPON ||
			pkArrow->proto()->sub_type != WEAPON_ARROW)
	{
		return 0;
	}

	iArrowCount = std::min<int>(iArrowCount, pkArrow->count());

	*ppkBow = pkBow;
	*ppkArrow = pkArrow;

	return iArrowCount;
}

void character::use_arrow(item * pkArrow, uint32_t dwArrowCount)
{
	int iCount = pkArrow->count();
	uint32_t dwVnum = pkArrow->vnum();
	iCount = iCount - std::min<int>(iCount, dwArrowCount);
	pkArrow->set_count(iCount);

	if (iCount == 0)
	{
		item * pkNewArrow = find_specify_item(dwVnum);

		metin2::log::info( "UseArrow : FindSpecifyItem {:d} {}", dwVnum, static_cast<const void *>(pkNewArrow));

		if (pkNewArrow)
			equip_item(pkNewArrow);
	}
}

class func_shoot
{
	public:
		character *	me;
		uint8_t		type;
		bool		succeed;

		func_shoot(character * ch, uint8_t bType) : me(ch), type(bType), succeed(false)
		{
		}

		void operator () (uint32_t dwTargetVID)
		{
			if (type > 1)
			{
				if (metin2::config::game().skill_disable)
					return;

				me->skill_use_info_[type].set_main_target_vid(dwTargetVID);
			}

			character * pkVictim = character_manager::instance().find(dwTargetVID);

			if (!pkVictim)
				return;

			// Cannot attack
			if (!battle_is_attackable(me, pkVictim))
				return;

			if (me->is_npc())
			{
				if (distance_approx(me->x() - pkVictim->x(), me->y() - pkVictim->y()) > 5000)
					return;
			}

			item * pkBow;
			item * pkArrow;

			switch (type)
			{
				case 0: // Normal bow
					{
						int iDam = 0;

						if (me->is_pc())
						{
							if (me->job() != JOB_ASSASSIN)
								return;

							if (0 == me->arrow_and_bow(&pkBow, &pkArrow))
								return;

							if (me->skill_group() != 0)
								if (!me->is_npc() && me->skill_group() != 2)
								{
									if (me->sp() < 5)
										return;

									me->point_change(POINT_SP, -5);
								}

							iDam = calc_arrow_damage(me, pkVictim, pkBow, pkArrow);
							me->use_arrow(pkArrow, 1);

							// check speed hack
							uint32_t	dwCurrentTime	= get_dword_time();
							if (is_speed_hack(me, pkVictim, dwCurrentTime))
								iDam	= 0;
						}
						else
							iDam = calc_melee_damage(me, pkVictim);

						normal_attack_affect(me, pkVictim);

						// Damage calculation
						iDam = iDam * (100 - pkVictim->point(POINT_RESIST_BOW)) / 100;

						me->on_move(true);
						pkVictim->on_move();

						if (pkVictim->can_begin_fight())
							pkVictim->begin_fight(me);

						pkVictim->damage(me, iDam, DAMAGE_TYPE_NORMAL_RANGE);
						// End of hit damage calculation
					}
					break;

				case 1: // Normal magic
					{
						int iDam;

						if (me->is_pc())
							return;

						iDam = calc_magic_damage(me, pkVictim);

						normal_attack_affect(me, pkVictim);

						// Damage calculation
						iDam = iDam * (100 - pkVictim->point(POINT_RESIST_MAGIC)) / 100;

						me->on_move(true);
						pkVictim->on_move();

						if (pkVictim->can_begin_fight())
							pkVictim->begin_fight(me); 

						pkVictim->damage(me, iDam, DAMAGE_TYPE_MAGIC);
						// End of hit damage calculation
					}
					break;

				case SKILL_YEONSA:	// Rapid fire
					{
						int iUseArrow = 1;

						// When only computing the total
						{
							if (iUseArrow == me->arrow_and_bow(&pkBow, &pkArrow, iUseArrow))
							{
								me->on_move(true);
								pkVictim->on_move();

								if (pkVictim->can_begin_fight())
									pkVictim->begin_fight(me); 

								me->compute_skill(type, pkVictim);
								me->use_arrow(pkArrow, iUseArrow);

								if (pkVictim->is_dead())
									break;

							}
							else
								break;
						}
					}
					break;


				case SKILL_KWANKYEOK:
					{
						int iUseArrow = 1;

						if (iUseArrow == me->arrow_and_bow(&pkBow, &pkArrow, iUseArrow))
						{
							me->on_move(true);
							pkVictim->on_move();

							if (pkVictim->can_begin_fight())
								pkVictim->begin_fight(me); 

							metin2::log::info( "{} kwankeyok {}", me->name(), pkVictim->name());
							me->compute_skill(type, pkVictim);
							me->use_arrow(pkArrow, iUseArrow);
						}
					}
					break;

				case SKILL_GIGUNG:
					{
						int iUseArrow = 1;
						if (iUseArrow == me->arrow_and_bow(&pkBow, &pkArrow, iUseArrow))
						{
							me->on_move(true);
							pkVictim->on_move();

							if (pkVictim->can_begin_fight())
								pkVictim->begin_fight(me);

							metin2::log::info( "{} gigung {}", me->name(), pkVictim->name());
							me->compute_skill(type, pkVictim);
							me->use_arrow(pkArrow, iUseArrow);
						}
					}

					break;
				case SKILL_HWAJO:
					{
						int iUseArrow = 1;
						if (iUseArrow == me->arrow_and_bow(&pkBow, &pkArrow, iUseArrow))
						{
							me->on_move(true);
							pkVictim->on_move();

							if (pkVictim->can_begin_fight())
								pkVictim->begin_fight(me);

							metin2::log::info( "{} hwajo {}", me->name(), pkVictim->name());
							me->compute_skill(type, pkVictim);
							me->use_arrow(pkArrow, iUseArrow);
						}
					}

					break;

				case SKILL_HORSE_WILDATTACK_RANGE:
					{
						int iUseArrow = 1;
						if (iUseArrow == me->arrow_and_bow(&pkBow, &pkArrow, iUseArrow))
						{
							me->on_move(true);
							pkVictim->on_move();

							if (pkVictim->can_begin_fight())
								pkVictim->begin_fight(me);

							metin2::log::info( "{} horse_wildattack {}", me->name(), pkVictim->name());
							me->compute_skill(type, pkVictim);
							me->use_arrow(pkArrow, iUseArrow);
						}
					}

					break;

				case SKILL_MARYUNG:
					//case SKILL_GUMHWAN:
				case SKILL_TUSOK:
				case SKILL_BIPABU:
				case SKILL_NOEJEON:
				case SKILL_GEOMPUNG:
				case SKILL_SANGONG:
				case SKILL_MAHWAN:
				case SKILL_PABEOB:
					//case SKILL_CURSE:
					{
						me->on_move(true);
						pkVictim->on_move();

						if (pkVictim->can_begin_fight())
							pkVictim->begin_fight(me);

						metin2::log::info( "{} - Skill {:d} -> {}", me->name(), type, pkVictim->name());
						me->compute_skill(type, pkVictim);
					}
					break;

				case SKILL_CHAIN:
					{
						me->on_move(true);
						pkVictim->on_move();

						if (pkVictim->can_begin_fight())
							pkVictim->begin_fight(me); 

						metin2::log::info( "{} - Skill {:d} -> {}", me->name(), type, pkVictim->name());
						me->compute_skill(type, pkVictim);

						// TODO hit multiple targets whoosh whoosh whoosh
					}
					break;

				case SKILL_YONGBI:
					{
						me->on_move(true);
					}
					break;

				default:
					metin2::log::error("CFuncShoot: I don't know this type [{:d}] of range attack.", (int) type);
					break;
			}

			succeed = true;
		}
};

bool character::shoot(uint8_t bType)
{
	metin2::log::debug( "Shoot {} type {:d} flyTargets.size {:d}", name(), bType, vec_dw_fly_targets_.size());

	if (!can_move())
	{
		return false;
	}	

	func_shoot f(this, bType);

	if (fly_target_id_ != 0)
	{
		f(fly_target_id_);
		fly_target_id_ = 0;
	}

	f = std::for_each(vec_dw_fly_targets_.begin(), vec_dw_fly_targets_.end(), f);
	vec_dw_fly_targets_.clear();

	return f.succeed;
}

void character::fly_target(uint32_t dwTargetVID, int32_t x, int32_t y, uint8_t bHeader)
{
	character * pkVictim = character_manager::instance().find(dwTargetVID);
	packet_gc_fly_targeting pack;

	pack.header	= (bHeader == HEADER_CG_FLY_TARGETING) ? HEADER_GC_FLY_TARGETING : HEADER_GC_ADD_FLY_TARGETING;
	pack.shooter_vid	= get_vid();

	if (pkVictim)
	{
		pack.target_vid = pkVictim->get_vid();
		pack.x = pkVictim->x();
		pack.y = pkVictim->y();

		if (bHeader == HEADER_CG_FLY_TARGETING)
			fly_target_id_ = dwTargetVID;
		else
			vec_dw_fly_targets_.push_back(dwTargetVID);
	}
	else
	{
		pack.target_vid = 0;
		pack.x = x;
		pack.y = y;
	}

	metin2::log::debug( "FlyTarget {} vid {:d} x {:d} y {:d}", name(), pack.target_vid, pack.x, pack.y);
	packet_around(&pack, sizeof(pack), this);
}

character * character::nearest_victim(character * pkChr)
{
	if (nullptr == pkChr)
		pkChr = this;

	float fMinDist = 99999.0f;
	character * pkVictim = nullptr;

	damage_map::iterator it = map_k_damage_.begin();

	// First filter out those not nearby.
	while (it != map_k_damage_.end())
	{
		const vid & c_VID = it->first;
		++it;

		character * pAttacker = character_manager::instance().find(c_VID);

		if (!pAttacker)
			continue;

		if (pAttacker->is_affect_flag(AFF_EUNHYUNG) || 
				pAttacker->is_affect_flag(AFF_INVISIBILITY) ||
				pAttacker->is_affect_flag(AFF_REVIVE_INVISIBLE))
			continue;

		float fDist = distance_approx(pAttacker->x() - pkChr->x(), pAttacker->y() - pkChr->y());

		if (fDist < fMinDist)
		{
			pkVictim = pAttacker;
			fMinDist = fDist;
		}
	}

	return pkVictim;
}

void character::set_victim(character * pkVictim)
{
	if (!pkVictim)
	{
		if (0 != (uint32_t)vid_victim)
			monster_log("공격 대상을 해제");

		vid_victim.reset();
		battle_end(this);
	}
	else
	{
		if (vid_victim != pkVictim->get_vid())
			monster_log("공격 대상을 설정: %s", pkVictim->name());

		vid_victim = pkVictim->get_vid();
		last_victim_set_time_ = get_dword_time();
	}
}

character * character::victim() const
{
	return character_manager::instance().find(vid_victim);
}

character * character::protege() const // Returns the target to protect
{
	if (chr_stone_)
		return chr_stone_;

	if (party_)
		return party_->leader();

	return nullptr;
}

int character::alignment() const
{
	return alignment_;
}

int character::real_alignment() const
{
	return real_alignment_;
}

void character::show_alignment(bool bShow)
{
	if (bShow)
	{
		if (alignment_ != real_alignment_)
		{
			alignment_ = real_alignment_;
			update_packet();
		}
	}
	else
	{
		if (alignment_ != 0)
		{
			alignment_ = 0;
			update_packet();
		}
	}
}

void character::update_alignment(int iAmount)
{
	bool bShow = false;

	if (alignment_ == real_alignment_)
		bShow = true;

	int i = alignment_ / 10;

	real_alignment_ = std::clamp(real_alignment_ + iAmount, -200000, 200000);

	if (bShow)
	{
		alignment_ = real_alignment_;

		if (i != alignment_ / 10)
			update_packet();
	}
}

void character::set_killer_mode(bool isOn)
{
	if ((isOn ? ADD_CHARACTER_STATE_KILLER : 0) == is_set(add_chr_state_, ADD_CHARACTER_STATE_KILLER))
		return;

	if (isOn)
		set_bit(add_chr_state_, ADD_CHARACTER_STATE_KILLER);
	else
		remove_bit(add_chr_state_, ADD_CHARACTER_STATE_KILLER);

	killer_mode_pulse_ = thecore_pulse();
	update_packet();
	metin2::log::info( "SetKillerMode Update {}[{:d}]", name(), player_id());
}

bool character::is_killer_mode() const
{
	return is_set(add_chr_state_, ADD_CHARACTER_STATE_KILLER);
}

void character::update_killer_mode()
{
	if (!is_killer_mode())
		return;

	int iKillerSeconds = 30;

	if (thecore_pulse() - killer_mode_pulse_ >= passes_per_sec(iKillerSeconds))
		set_killer_mode(false);
}

void character::set_pk_mode(uint8_t bPKMode)
{
	if (bPKMode >= PK_MODE_MAX_NUM)
		return;

	if (pk_mode_ == bPKMode)
		return;

	if (bPKMode == PK_MODE_GUILD && !get_guild())
		bPKMode = PK_MODE_FREE;

	pk_mode_ = bPKMode;
	update_packet();

	metin2::log::info( "PK_MODE: {} {:d}", name(), pk_mode_);
}

uint8_t character::pk_mode() const
{
	return pk_mode_;
}

struct func_forget_my_attacker
{
	character * ch;
	func_forget_my_attacker(character * ch_value)
	{
		ch = ch_value;
	}
	void operator()(entity * ent)
	{
		if (ent->is_type(ENTITY_CHARACTER))
		{
			character * ch_value = (character *) ent;
			if (ch_value->is_pc())
				return;
			if (ch_value->vid_victim == ch->get_vid())
				ch_value->set_victim(nullptr);
		}
	}
};

struct func_aggregate_monster
{
	character * ch;
	func_aggregate_monster(character * ch_value)
	{
		ch = ch_value;
	}
	void operator()(entity * ent)
	{
		if (ent->is_type(ENTITY_CHARACTER))
		{
			character * ch_value = (character *) ent;
			if (ch_value->is_pc())
				return;
			if (!ch_value->is_monster())
				return;
			if (ch_value->victim())
				return;

			if (number(1, 100) <= 50) // Temporarily pull enemies with 50% chance
				if (distance_approx(ch_value->x() - ch->x(), ch_value->y() - ch->y()) < 5000)
					if (ch_value->can_begin_fight())
						ch_value->begin_fight(ch);
		}
	}
};

struct func_attract_ranger
{
	character * ch;
	func_attract_ranger(character * ch_value)
	{
		ch = ch_value;
	}

	void operator()(entity * ent)
	{
		if (ent->is_type(ENTITY_CHARACTER))
		{
			character * ch_value = (character *) ent;
			if (ch_value->is_pc())
				return;
			if (!ch_value->is_monster())
				return;
			if (ch_value->victim() && ch_value->victim() != ch)
				return;
			if (ch_value->mob_attack_range() > 150)
			{
				int iNewRange = 150;//(int)(ch->GetMobAttackRange() * 0.2);
				if (iNewRange < 150)
					iNewRange = 150;

				ch_value->add_affect(AFFECT_BOW_DISTANCE, POINT_BOW_DISTANCE, iNewRange - ch_value->mob_attack_range(), AFF_NONE, 3*60, 0, false);
			}
		}
	}
};

struct func_pull_monster
{
	character * ch;
	int length;
	func_pull_monster(character * ch_value, int iLength = 300)
	{
		ch = ch_value;
		length = iLength;
	}

	void operator()(entity * ent)
	{
		if (ent->is_type(ENTITY_CHARACTER))
		{
			character * ch_value = (character *) ent;
			if (ch_value->is_pc())
				return;
			if (!ch_value->is_monster())
				return;
			float fDist = distance_approx(ch->x() - ch_value->x(), ch->y() - ch_value->y());
			if (fDist > 3000 || fDist < 100)
				return;

			float fNewDist = fDist - length;
			if (fNewDist < 100) 
				fNewDist = 100;

			float degree = get_degree_from_position_xy(ch_value->x(), ch_value->y(), ch->x(), ch->y());
			float fx;
			float fy;

			get_delta_by_degree(degree, fDist - fNewDist, &fx, &fy);
			int32_t tx = (int32_t)(ch_value->x() + fx);
			int32_t ty = (int32_t)(ch_value->y() + fy);

			ch_value->sync(tx, ty);
			ch_value->goto_(tx, ty);
			ch_value->calculate_move_duration();

			ch_value->sync_packet();
		}
	}
};

void character::forget_my_attacker()
{
	sectree * pSec = get_sectree();
	if (pSec)
	{
		func_forget_my_attacker f(this);
		pSec->for_each_around(f);
	}
	revive_invisible(5);
}

void character::aggregate_monster()
{
	sectree * pSec = get_sectree();
	if (pSec)
	{
		func_aggregate_monster f(this);
		pSec->for_each_around(f);
	}
}

void character::attract_ranger()
{
	sectree * pSec = get_sectree();
	if (pSec)
	{
		func_attract_ranger f(this);
		pSec->for_each_around(f);
	}
}

void character::pull_monster()
{
	sectree * pSec = get_sectree();
	if (pSec)
	{
		func_pull_monster f(this);
		pSec->for_each_around(f);
	}
}

void character::update_aggr_point_ex(character * pAttacker, damage_type type, int dam, character::battle_info & info)
{
	// Increases further depending on attack type
	switch (type)
	{
		case DAMAGE_TYPE_NORMAL_RANGE:
			dam = (int) (dam*1.2f);
			break;

		case DAMAGE_TYPE_RANGE:
			dam = (int) (dam*1.5f);
			break;

		case DAMAGE_TYPE_MAGIC:
			dam = (int) (dam*1.2f);
			break;

		default:
			break;
	}

	// Give a bonus if the attacker is the current target.
	if (pAttacker == victim())
		dam = (int) (dam * 1.2f);

	info.aggro += dam;

	if (info.aggro < 0)
		info.aggro = 0;

	if (get_party() && dam > 0 && type != DAMAGE_TYPE_SPECIAL)
	{
		party * pParty = get_party();

		// Leaders have somewhat stronger influence
		int iPartyAggroDist = dam;

		if (pParty->leader_pid() == get_vid())
			iPartyAggroDist /= 2;
		else
			iPartyAggroDist /= 3;

		pParty->send_message(this, PM_AGGRO_INCREASE, iPartyAggroDist, pAttacker->get_vid());
	}

	change_victim_by_aggro(info.aggro, pAttacker);
}

void character::update_aggr_point(character * pAttacker, damage_type type, int dam)
{
	if (is_dead() || is_stun())
		return;

	damage_map::iterator it = map_k_damage_.find(pAttacker->get_vid());

	if (it == map_k_damage_.end())
	{
		map_k_damage_.insert(damage_map::value_type(pAttacker->get_vid(), battle_info(0, dam)));
		it = map_k_damage_.find(pAttacker->get_vid());
	}

	update_aggr_point_ex(pAttacker, type, dam, it->second);
}

void character::change_victim_by_aggro(int iNewAggro, character * pNewVictim)
{
	if (get_dword_time() - last_victim_set_time_ < 3000) // Must wait 3 seconds
		return;

	if (pNewVictim == victim())
	{
		if (max_aggro_ < iNewAggro)
		{
			max_aggro_ = iNewAggro;
			return;
		}

		// When aggro has decreased
		damage_map::iterator it;
		damage_map::iterator itFind = map_k_damage_.end();

		for (it = map_k_damage_.begin(); it != map_k_damage_.end(); ++it)
		{
			if (it->second.aggro > iNewAggro)
			{
				character * ch = character_manager::instance().find(it->first);

				if (ch && !ch->is_dead() && distance_approx(ch->x() - x(), ch->y() - y()) < 5000)
				{
					itFind = it;
					iNewAggro = it->second.aggro;
				}
			}
		}

		if (itFind != map_k_damage_.end())
		{
			max_aggro_ = iNewAggro;
			set_victim(character_manager::instance().find(itFind->first));
			state_duration_ = 1;
		}
	}
	else
	{
		if (max_aggro_ < iNewAggro)
		{
			max_aggro_ = iNewAggro;
			set_victim(pNewVictim);
			state_duration_ = 1;
		}
	}
}
}
