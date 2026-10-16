#include <metin2/config/config.h>
#include <metin2/game/characters/affect.h>
#include <metin2/game/events/blue_dragon.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/world/entity.h>
#include <metin2/game/trade/exchange.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/characters/motion.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/world/vector.h>
#include <metin2/game/guilds/war_map.h>
#include <metin2/game/events/xmas_event.h>

#include <metin2/proto/length.h>
#include <metin2/log/log.h>
#include <metin2/core/main.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdio>

namespace metin2::game::inline characters {


namespace
{
	class func_find_chr_for_flag
	{
		public:
			func_find_chr_for_flag(character * chr_value) :
				chr(chr_value), chr_find(nullptr), min_distance(INT_MAX)
				{
				}

			void operator () (entity * ent)
			{
				if (!ent->is_type(ENTITY_CHARACTER))
					return;

				if (ent->is_observer_mode())
					return;

				character * pkChr = (character *) ent;

				if (!pkChr->is_pc())
					return;

				if (!pkChr->get_guild())
					return;

				if (pkChr->is_dead())
					return;

				int iDist = distance_approx(pkChr->x()-chr->x(), pkChr->y()-chr->y());

				if (iDist <= 500 && min_distance > iDist &&
						!pkChr->is_affect_flag(AFF_WAR_FLAG1) &&
						!pkChr->is_affect_flag(AFF_WAR_FLAG2) &&
						!pkChr->is_affect_flag(AFF_WAR_FLAG3))
				{
					if ((uint32_t) chr->point(POINT_STAT) == pkChr->get_guild()->id())
					{
						war_map * pMap = pkChr->get_war_map();
						uint8_t idx;

						if (!pMap || !pMap->team_index(pkChr->get_guild()->id(), idx))
							return;

						if (!pMap->is_flag_on_base(idx))
						{
							chr_find = pkChr;
							min_distance = iDist;
						}
					}
					else
					{
						chr_find = pkChr;
						min_distance = iDist;
					}
				}
			}

			character *	chr;
			character * chr_find;
			int		min_distance;
	};

	class func_find_chr_for_flag_base
	{
		public:
			func_find_chr_for_flag_base(character * chr_value) : chr(chr_value)
			{
			}

			void operator () (entity * ent)
			{
				if (!ent->is_type(ENTITY_CHARACTER))
					return;

				if (ent->is_observer_mode())
					return;

				character * pkChr = (character *) ent;

				if (!pkChr->is_pc())
					return;

				guild * pkGuild = pkChr->get_guild();

				if (!pkGuild)
					return;

				int iDist = distance_approx(pkChr->x()-chr->x(), pkChr->y()-chr->y());

				if (iDist <= 500 &&
						(pkChr->is_affect_flag(AFF_WAR_FLAG1) || 
						 pkChr->is_affect_flag(AFF_WAR_FLAG2) ||
						 pkChr->is_affect_flag(AFF_WAR_FLAG3)))
				{
					affect * pkAff = pkChr->find_affect(AFFECT_WAR_FLAG);

					metin2::log::debug("War flag base: {} at distance {:d}, affect {}, flag guild {:d}, player guild {:d}",
							pkChr->name(), iDist, static_cast<const void *>(pkAff), chr->point(POINT_STAT),
							pkChr->get_guild()->id());

					if (pkAff)
					{
						if ((uint32_t) chr->point(POINT_STAT) == pkGuild->id() &&
								chr->point(POINT_STAT) != pkAff->apply_value)
						{
							war_map * pMap = pkChr->get_war_map();
							uint8_t idx;

							if (!pMap || !pMap->team_index(pkGuild->id(), idx))
								return;

							{
								uint8_t idx_opp = idx == 0 ? 1 : 0;

								send_guild_war_score(chr->point(POINT_STAT), pkAff->apply_value, 1);

								pMap->reset_flag();

								char buf[256];
								snprintf(buf, sizeof(buf), lc_text("%s 길드가 %s 길드의 깃발을 빼앗았습니다!"), pMap->get_guild(idx)->name(), pMap->get_guild(idx_opp)->name());
								pMap->notice(buf);
							}
						}
					}
				}
			}

			character * chr;
	};

	class func_find_guard_victim
	{
		public:
			func_find_guard_victim(character * chr, int max_distance) :
			min_distance_(INT_MAX),
			max_distance_(max_distance),
			lx_(chr->x()),
			ly_(chr->y()),
			chr_victim_(nullptr)
			{
			};

			void operator () (entity * ent)
			{
				if (!ent->is_type(ENTITY_CHARACTER))
					return;

				character * pkChr = (character *) ent;

				if (pkChr->is_pc())
					return;

				if (pkChr->is_npc() && !pkChr->is_monster())
					return;

				if (pkChr->is_dead())
					return;

				if (pkChr->is_affect_flag(AFF_EUNHYUNG) || 
						pkChr->is_affect_flag(AFF_INVISIBILITY) ||
						pkChr->is_affect_flag(AFF_REVIVE_INVISIBLE))
					return;

				if (pkChr->race_num() == 5001)
					return;

				int iDistance = distance_approx(lx_ - pkChr->x(), ly_ - pkChr->y());

				if (iDistance < min_distance_ && iDistance <= max_distance_)
				{
					chr_victim_ = pkChr;
					min_distance_ = iDistance;
				}
			}

			auto victim() -> character *
			{
				return (chr_victim_);
			}

		private:
			int		min_distance_;
			int		max_distance_;
			int32_t	lx_;
			int32_t	ly_;

			character *	chr_victim_;
	};

}

auto character::is_aggressive() const -> bool
{
	return is_set(points_instant_.ai_flag, AIFLAG_AGGRESSIVE);
}

void character::set_aggressive()
{
	set_bit(points_instant_.ai_flag, AIFLAG_AGGRESSIVE);
}

auto character::is_coward() const -> bool
{
	return is_set(points_instant_.ai_flag, AIFLAG_COWARD);
}

void character::set_coward()
{
	set_bit(points_instant_.ai_flag, AIFLAG_COWARD);
}

auto character::is_berserker() const -> bool
{
	return is_set(points_instant_.ai_flag, AIFLAG_BERSERK);
}

auto character::is_stone_skinner() const -> bool
{
	return is_set(points_instant_.ai_flag, AIFLAG_STONESKIN);
}

auto character::is_god_speeder() const -> bool
{
	return is_set(points_instant_.ai_flag, AIFLAG_GODSPEED);
}

auto character::is_death_blower() const -> bool
{
	return is_set(points_instant_.ai_flag, AIFLAG_DEATHBLOW);
}

auto character::is_reviver() const -> bool
{
	return is_set(points_instant_.ai_flag, AIFLAG_REVIVE);
}

void character::coward_escape()
{
	int iDist[4] = {500, 1000, 3000, 5000};

	for (int iDistIdx = 2; iDistIdx >= 0; --iDistIdx)
		for (int iTryCount = 0; iTryCount < 8; ++iTryCount)
		{
			set_rotation(number(0, 359));

			float fx, fy;
			float fDist = number(iDist[iDistIdx], iDist[iDistIdx+1]);

			get_delta_by_degree(rotation(), fDist, &fx, &fy);

			bool bIsWayBlocked = false;
			for (int j = 1; j <= 100; ++j)
			{
				if (!sectree_manager::instance().is_movable_position(map_index(), x() + (int) fx*j/100, y() + (int) fy*j/100))
				{
					bIsWayBlocked = true;
					break;
				}
			}

			if (bIsWayBlocked)
				continue;

			state_duration_ = passes_per_sec(1);

			int iDestX = x() + (int) fx;
			int iDestY = y() + (int) fy;

			if (go_to(iDestX, iDestY))
				send_move_packet(FUNC_WAIT, 0, 0, 0, 0);

			metin2::log::debug("Waegu moves to {:d} {:d} (far)", iDestX, iDestY);
			return;
		}
}

auto character::is_no_attack_shinsu() const -> bool
{
	return is_set(points_instant_.ai_flag, AIFLAG_NOATTACKSHINSU);
}

auto character::is_no_attack_chunjo() const -> bool
{
	return is_set(points_instant_.ai_flag, AIFLAG_NOATTACKCHUNJO);
}

auto character::is_no_attack_jinno() const -> bool
{
	return is_set(points_instant_.ai_flag, AIFLAG_NOATTACKJINNO);
}

auto character::is_attack_mob() const -> bool
{
	return is_set(points_instant_.ai_flag, AIFLAG_ATTACKMOB);
}

void character::state_idle()
{
	if (is_stone())
	{
		state_idle_stone();
		return;
	}
	else if (is_warp() || is_goto())
	{
		state_duration_ = 60 * metin2::config::server().passes_per_sec;
		return;
	}

	if (is_pc())
		return;

	if (!is_monster())
	{
		state_idle_npc();
		return;
	}

	state_idle_monster();
}

void character::state_idle_stone()
{
	state_duration_ = passes_per_sec(1);

	int iPercent = (hp() * 100) / max_hp();
	uint32_t dwVnum = number(std::min<int>(get_mob_table().s_attack_speed, get_mob_table().s_moving_speed ), std::max<int>(get_mob_table().s_attack_speed, get_mob_table().s_moving_speed));

	if (iPercent <= 10 && max_sp() < 10)
	{
		set_max_sp(10);
		send_move_packet(FUNC_ATTACK, 0, x(), y(), 0);

		character_manager::instance().select_stone(this);
		character_manager::instance().spawn_group(dwVnum, map_index(), x() - 500, y() - 500, x() + 500, y() + 500);
		character_manager::instance().spawn_group(dwVnum, map_index(), x() - 1000, y() - 1000, x() + 1000, y() + 1000);
		character_manager::instance().spawn_group(dwVnum, map_index(), x() - 1500, y() - 1500, x() + 1500, y() + 1500);
		character_manager::instance().select_stone(nullptr);
	}
	else if (iPercent <= 20 && max_sp() < 9)
	{
		set_max_sp(9);
		send_move_packet(FUNC_ATTACK, 0, x(), y(), 0);

		character_manager::instance().select_stone(this);
		character_manager::instance().spawn_group(dwVnum, map_index(), x() - 500, y() - 500, x() + 500, y() + 500);
		character_manager::instance().spawn_group(dwVnum, map_index(), x() - 1000, y() - 1000, x() + 1000, y() + 1000);
		character_manager::instance().spawn_group(dwVnum, map_index(), x() - 1500, y() - 1500, x() + 1500, y() + 1500);
		character_manager::instance().select_stone(nullptr);
	}
	else if (iPercent <= 30 && max_sp() < 8)
	{
		set_max_sp(8);
		send_move_packet(FUNC_ATTACK, 0, x(), y(), 0);

		character_manager::instance().select_stone(this);
		character_manager::instance().spawn_group(dwVnum, map_index(), x() - 500, y() - 500, x() + 500, y() + 500);
		character_manager::instance().spawn_group(dwVnum, map_index(), x() - 1000, y() - 1000, x() + 1000, y() + 1000);
		character_manager::instance().spawn_group(dwVnum, map_index(), x() - 1000, y() - 1000, x() + 1000, y() + 1000);
		character_manager::instance().select_stone(nullptr);
	}
	else if (iPercent <= 40 && max_sp() < 7)
	{
		set_max_sp(7);
		send_move_packet(FUNC_ATTACK, 0, x(), y(), 0);

		character_manager::instance().select_stone(this);
		character_manager::instance().spawn_group(dwVnum, map_index(), x() - 1000, y() - 1000, x() + 1000, y() + 1000);
		character_manager::instance().spawn_group(dwVnum, map_index(), x() - 1000, y() - 1000, x() + 1000, y() + 1000);
		character_manager::instance().spawn_group(dwVnum, map_index(), x() - 1000, y() - 1000, x() + 1000, y() + 1000);
		character_manager::instance().select_stone(nullptr);
	}
	else if (iPercent <= 50 && max_sp() < 6)
	{
		set_max_sp(6);
		send_move_packet(FUNC_ATTACK, 0, x(), y(), 0);

		character_manager::instance().select_stone(this);
		character_manager::instance().spawn_group(dwVnum, map_index(), x() - 1000, y() - 1000, x() + 1000, y() + 1000);
		character_manager::instance().spawn_group(dwVnum, map_index(), x() - 1000, y() - 1000, x() + 1000, y() + 1000);
		character_manager::instance().select_stone(nullptr);
	}
	else if (iPercent <= 60 && max_sp() < 5)
	{
		set_max_sp(5);
		send_move_packet(FUNC_ATTACK, 0, x(), y(), 0);

		character_manager::instance().select_stone(this);
		character_manager::instance().spawn_group(dwVnum, map_index(), x() - 1000, y() - 1000, x() + 1000, y() + 1000);
		character_manager::instance().spawn_group(dwVnum, map_index(), x() - 500, y() - 500, x() + 500, y() + 500);
		character_manager::instance().select_stone(nullptr);
	}
	else if (iPercent <= 70 && max_sp() < 4)
	{
		set_max_sp(4);
		send_move_packet(FUNC_ATTACK, 0, x(), y(), 0);

		character_manager::instance().select_stone(this);
		character_manager::instance().spawn_group(dwVnum, map_index(), x() - 500, y() - 500, x() + 500, y() + 500);
		character_manager::instance().spawn_group(dwVnum, map_index(), x() - 1000, y() - 1000, x() + 1000, y() + 1000);
		character_manager::instance().select_stone(nullptr);
	}
	else if (iPercent <= 80 && max_sp() < 3)
	{
		set_max_sp(3);
		send_move_packet(FUNC_ATTACK, 0, x(), y(), 0);

		character_manager::instance().select_stone(this);
		character_manager::instance().spawn_group(dwVnum, map_index(), x() - 1000, y() - 1000, x() + 1000, y() + 1000);
		character_manager::instance().spawn_group(dwVnum, map_index(), x() - 1000, y() - 1000, x() + 1000, y() + 1000);
		character_manager::instance().select_stone(nullptr);
	}
	else if (iPercent <= 90 && max_sp() < 2)
	{
		set_max_sp(2);
		send_move_packet(FUNC_ATTACK, 0, x(), y(), 0);

		character_manager::instance().select_stone(this);
		character_manager::instance().spawn_group(dwVnum, map_index(), x() - 500, y() - 500, x() + 500, y() + 500);
		character_manager::instance().select_stone(nullptr);
	}
	else if (iPercent <= 99 && max_sp() < 1)
	{
		set_max_sp(1);
		send_move_packet(FUNC_ATTACK, 0, x(), y(), 0);

		character_manager::instance().select_stone(this);
		character_manager::instance().spawn_group(dwVnum, map_index(), x() - 1000, y() - 1000, x() + 1000, y() + 1000);
		character_manager::instance().select_stone(nullptr);
	}
	else
		return;

	update_packet();
	return;
}

void character::state_idle_npc()
{
	monster_chat(MONSTER_CHAT_WAIT);
	state_duration_ = passes_per_sec(5);

	if (is_pet())
		return;
	else if (is_guard_npc())
	{
		if (!quest::quest_manager::instance().event_flag("noguard"))
		{
			func_find_guard_victim f(this, 50000);

			if (get_sectree())
				get_sectree()->for_each_around(f);

			character * victim = f.victim();

			if (victim)
			{
				state_duration_ = metin2::config::server().passes_per_sec/2;

				if (can_begin_fight())
					begin_fight(victim);
			}
		}
	}
	else
	{
		if (race_num() == xmas::MOB_SANTA_VNUM)
		{
			if (get_dword_time() > play_start_time_)
			{
				int	next_warp_time = 2 * 1000;

				play_start_time_ = get_dword_time() + next_warp_time;

				const int WARP_MAP_INDEX_NUM = 7;
				static const int32_t c_lWarpMapIndexs[WARP_MAP_INDEX_NUM] = { 61, 62, 63, 64, 3, 23, 43 };
				int32_t lNextMapIndex;
				lNextMapIndex = c_lWarpMapIndexs[number(1, WARP_MAP_INDEX_NUM) - 1];

				if (map_allow_find(lNextMapIndex))
				{
					character_manager::instance().destroy_character(this);
					int iNextSpawnDelay = 0;
					iNextSpawnDelay = 50 * 60;

					xmas::spawn_santa(lNextMapIndex, iNextSpawnDelay);
				}
				else
				{
					packet_gg_xmas_warp_santa p;
					p.header   = HEADER_GG_XMAS_WARP_SANTA;
					p.channel  = metin2::config::server().channel;
					p.map_index = lNextMapIndex;
					p2_p_manager::instance().send(&p, sizeof(packet_gg_xmas_warp_santa));
				}
				return;
			}
		}

		if (!is_set(points_instant_.ai_flag, AIFLAG_NOMOVE))
		{
			character * pkChrProtege = protege();

			if (pkChrProtege)
			{
				if (distance_approx(x() - pkChrProtege->x(), y() - pkChrProtege->y()) > 500)
				{
					if (follow(pkChrProtege, number(100, 300)))
						return;
				}
			}

			if (!number(0, 6))
			{
				set_rotation(number(0, 359));

				float fx, fy;
				float fDist = number(200, 400);

				get_delta_by_degree(rotation(), fDist, &fx, &fy);

				if (!(sectree_manager::instance().is_movable_position(map_index(), x() + (int) fx, y() + (int) fy) 
					&& sectree_manager::instance().is_movable_position(map_index(), x() + (int) fx / 2, y() + (int) fy / 2)))
					return;

				set_now_walking(true);

				if (go_to(x() + (int) fx, y() + (int) fy))
					send_move_packet(FUNC_WAIT, 0, 0, 0, 0);

				return;
			}
		}
	}
}

void character::state_idle_monster()
{
	if (is_stun())
		return;

	if (!can_move())
		return;

	if (is_coward())
	{
		if (!is_dead())
			coward_escape();

		return;
	}

	if (is_berserker())
		if (is_berserk())
			set_berserk(false);

	if (is_god_speeder())
		if (is_god_speed())
			set_god_speed(false);

	character * victim_value = victim();

	if (!victim_value || victim_value->is_dead())
	{
		set_victim(nullptr);
		victim_value = nullptr;
		state_duration_ = passes_per_sec(1);
	}

	if (!victim_value || victim_value->is_building())
	{
		if (chr_stone_)
		{
			victim_value = chr_stone_->nearest_victim(chr_stone_);
		}
		else if (!metin2::config::game().no_wander && is_aggressive())
		{
			if (map_index() == 61 && quest::quest_manager::instance().event_flag("xmas_tree"));
			else
				victim_value = find_victim(this, mob_data->table.aggressive_sight);
		}
	}

	if (victim_value && !victim_value->is_dead())
	{
		if (can_begin_fight())
			begin_fight(victim_value);

		return;
	}

	if (is_aggressive() && !victim_value)
		state_duration_ = passes_per_sec(number(1, 3));
	else
		state_duration_ = passes_per_sec(number(3, 5));

	character * pkChrProtege = protege();

	if (pkChrProtege)
	{
		if (distance_approx(x() - pkChrProtege->x(), y() - pkChrProtege->y()) > 1000)
		{
			if (follow(pkChrProtege, number(150, 400)))
			{
				monster_log("[IDLE] 리더로부터 너무 멀리 떨어졌다! 복귀한다.");
				return;
			}
		}
	}

	if (!metin2::config::game().no_wander && !is_set(points_instant_.ai_flag, AIFLAG_NOMOVE))
	{
		if (!number(0, 6))
		{
			set_rotation(number(0, 359));

			float fx, fy;
			float fDist = number(300, 700);

			get_delta_by_degree(rotation(), fDist, &fx, &fy);

			if (!(sectree_manager::instance().is_movable_position(map_index(), x() + (int) fx, y() + (int) fy) 
						&& sectree_manager::instance().is_movable_position(map_index(), x() + (int) fx/2, y() + (int) fy/2)))
				return;

			if (metin2::config::test_server())
			{
				if (number(0, 100) < 60)
					set_now_walking(false);
				else
					set_now_walking(true);
			}

			if (go_to(x() + (int) fx, y() + (int) fy))
				send_move_packet(FUNC_WAIT, 0, 0, 0, 0);

			return;
		}
	}

	monster_chat(MONSTER_CHAT_WAIT);
}

auto character_goto_near_target(character * self, character * victim) -> bool
{
	if (is_set(self->ai_flag(), AIFLAG_NOMOVE))
		return false;

	switch (self->mob_battle_type())
	{
		case BATTLE_TYPE_RANGE:
		case BATTLE_TYPE_MAGIC:
			if (self->follow(victim, self->mob_attack_range() * 8 / 10))
				return true;
			break;

		default:
			if (self->follow(victim, self->mob_attack_range() * 9 / 10))
				return true;
	}

	return false;
}

void character::state_move()
{
	uint32_t dwElapsedTime = get_dword_time() - move_start_time_;
	float fRate = (float) dwElapsedTime / (float) move_duration_;

	if (fRate > 1.0f)
		fRate = 1.0f;

	int x_value = (int) ((float) (pos_dest_.x - pos_start_.x) * fRate + pos_start_.x);
	int y_value = (int) ((float) (pos_dest_.y - pos_start_.y) * fRate + pos_start_.y);

	move(x_value, y_value);

	if (is_pc() && (thecore_pulse() & 15) == 0)
	{
		update_sectree();

		if (get_exchange())
		{
			character * victim_value = get_exchange()->company()->owner();
			int iDist = distance_approx(x() - victim_value->x(), y() - victim_value->y());

			if (iDist >= EXCHANGE_MAX_DISTANCE)
			{
				get_exchange()->cancel();
			}
		}
	}

	if (is_pc())
	{
		if (is_walking() && stamina() < max_stamina())
		{
			if (get_dword_time() - walk_start_time() > 5000)
				point_change(POINT_STAMINA, max_stamina() / 1);
		}

		if (!is_walking() && !is_riding())
		{
			if ((get_dword_time() - last_attack_time()) < 20000)
			{
				start_affect_event();

				if (is_stamina_half_consume())
				{
					if (thecore_pulse()&1)
						point_change(POINT_STAMINA, -STAMINA_PER_STEP);
				}
				else
					point_change(POINT_STAMINA, -STAMINA_PER_STEP);

				start_stamina_consume();

				if (stamina() <= 0)
				{
					set_stamina(0);
					set_now_walking(true);
					stop_stamina_consume();
				}
			}
			else if (is_stamina_consume())
			{
				stop_stamina_consume();
			}
		}
	}
	else
	{
		if (is_monster() && victim())
		{
			character * victim_value = victim();
			update_aggr_point(victim_value, DAMAGE_TYPE_NORMAL, -(victim_value->level() / 3 + 1));

			if (metin2::config::test_server())
			{
				set_now_walking(false);
			}
		}

		if (is_monster() && get_mob_rank() >= MOB_RANK_BOSS && victim())
		{
			character * victim_value = victim();

			if (race_num() == 2191 && number(1, 20) == 1 && get_dword_time() - mob_inst->last_warp_time > 1000)
			{
				float fx, fy;
				get_delta_by_degree(victim_value->rotation(), 400, &fx, &fy);
				int32_t new_x = victim_value->x() + (int32_t)fx;
				int32_t new_y = victim_value->y() + (int32_t)fy;
				set_rotation(get_degree_from_position_xy(new_x, new_y, victim_value->x(), victim_value->y()));
				show(victim_value->map_index(), new_x, new_y, 0, true);
				goto_state(state_battle_);
				state_duration_ = 1;
				reset_mob_skill_cooltime();
				mob_inst->last_warp_time = get_dword_time();
				return;
			}

			if (number(0, 3) == 0)
			{
				if (character_goto_near_target(this, victim_value))
					return;
			}
		}
	}

	if (1.0f == fRate)
	{
		if (is_pc())
		{
			metin2::log::debug("{} arrived at {:d} {:d}", name(), x_value, y_value);
			goto_state(state_idle_);
			stop_stamina_consume();
		}
		else
		{
			if (victim() && !is_coward())
			{
				if (!is_state(state_battle_))
					monster_log("[BATTLE] 근처에 왔으니 공격시작 %s", victim()->name());

				goto_state(state_battle_);
				state_duration_ = 1;
			}
			else
			{
				if (!is_state(state_idle_))
					monster_log("[IDLE] 대상이 없으니 쉬자");

				goto_state(state_idle_);

				state_duration_ = passes_per_sec(number(1, 3));
			}
		}
	}
}

void character::state_battle()
{
	if (is_stone())
	{
		metin2::log::error("Stone {} must not enter battle state", name());
		return;
	}

	if (is_pc())
		return; 

	if (!can_move())
		return;

	if (is_stun())
		return;

	character * victim_value = victim();

	if (is_coward())
	{
		if (is_dead())
			return;

		set_victim(nullptr);

		if (number(1, 50) != 1)
		{
			goto_state(state_idle_);
			state_duration_ = 1;
		}
		else
			coward_escape();

		return;
	}

	if (!victim_value || (victim_value->is_stun() && is_guard_npc()) || victim_value->is_dead())
	{
		if (victim_value && victim_value->is_dead() &&
				!metin2::config::game().no_wander && is_aggressive() && (!get_party() || get_party()->leader() == this))
		{
			character * new_victim = find_victim(this, mob_data->table.aggressive_sight);

			set_victim(new_victim);
			state_duration_ = passes_per_sec(1);

			if (!new_victim)
			{
				switch (mob_battle_type())
				{
					case BATTLE_TYPE_MELEE:
					case BATTLE_TYPE_SUPER_POWER:
					case BATTLE_TYPE_SUPER_TANKER:
					case BATTLE_TYPE_POWER:
					case BATTLE_TYPE_TANKER:
						{
							float fx, fy;
							float fDist = number(400, 1500);

							get_delta_by_degree(number(0, 359), fDist, &fx, &fy);

							if (sectree_manager::instance().is_movable_position(victim_value->map_index(),
										victim_value->x() + (int) fx, 
										victim_value->y() + (int) fy) && 
									sectree_manager::instance().is_movable_position(victim_value->map_index(),
										victim_value->x() + (int) fx/2,
										victim_value->y() + (int) fy/2))
							{
								float dx = victim_value->x() + fx;
								float dy = victim_value->y() + fy;

								set_rotation(get_degree_from_position(dx, dy));

								if (go_to((int32_t) dx, (int32_t) dy))
								{
									metin2::log::debug("{}: kill and go, distance {:.1f}", name(), fDist);
									send_move_packet(FUNC_WAIT, 0, 0, 0, 0);
								}
							}
						}
				}
			}
			return;
		}

		set_victim(nullptr);

		if (is_guard_npc())
			return_home();

		state_duration_ = passes_per_sec(1);
		return;
	}

	if (is_summon_monster() && !is_dead() && !is_stun())
	{
		if (!get_party())
		{
			party_manager::instance().create_party(this);
		}

		party * pParty = get_party();
		bool bPct = !number(0, 3);

		if (bPct && pParty->count_member_by_vnum(summon_vnum()) < SUMMON_MONSTER_COUNT)
		{
			monster_log("부하 몬스터 소환!");
			int sx = x() - 300;
			int sy = y() - 300;
			int ex = x() + 300;
			int ey = y() + 300;

			character * tch = character_manager::instance().spawn_mob_range(summon_vnum(), map_index(), sx, sy, ex, ey, true, true);

			if (tch)
			{
				pParty->join(tch->get_vid());
				pParty->link(tch);
			}
		}
	}

	character * pkChrProtege = protege();

	float fDist = distance_approx(x() - victim_value->x(), y() - victim_value->y());

	if (fDist >= 4000.0f)
	{
		monster_log("타겟이 멀어서 포기");
		set_victim(nullptr);

		if (pkChrProtege)
			if (distance_approx(x() - pkChrProtege->x(), y() - pkChrProtege->y()) > 1000)
				follow(pkChrProtege, number(150, 400));

		return;
	}

	if (fDist >= mob_attack_range() * 1.15)
	{
		character_goto_near_target(this, victim_value);
		return;
	}

	if (party_)
		party_->send_message(this, PM_ATTACKED_BY, 0, 0);

	if (2493 == mob_data->table.vnum)
	{
		state_duration_ = blue_dragon_state_battle(this);
		return;
	}

	uint32_t dwCurTime = get_dword_time();
	uint32_t dwDuration = calculate_duration(limit_point(POINT_ATT_SPEED), 2000);

	if ((dwCurTime - last_attack_time_) < dwDuration)
	{
		state_duration_ = std::max<int>(1, (metin2::config::server().passes_per_sec * (dwDuration - (dwCurTime - last_attack_time_)) / 1000));
		return;
	}

	if (is_berserker() == true)
		if (hp_pct() < mob_data->table.berserk_point)
			if (is_berserk() != true)
				set_berserk(true);

	if (is_god_speeder() == true)
		if (hp_pct() < mob_data->table.god_speed_point)
			if (is_god_speed() != true)
				set_god_speed(true);

	if (has_mob_skill())
	{
		for (unsigned int iSkillIdx = 0; iSkillIdx < MOB_SKILL_MAX_NUM; ++iSkillIdx)
		{
			if (can_use_mob_skill(iSkillIdx))
			{
				set_rotation_to_xy(victim_value->x(), victim_value->y());

				if (use_mob_skill(iSkillIdx))
				{
					send_move_packet(FUNC_MOB_SKILL, iSkillIdx, x(), y(), 0, dwCurTime);

					float fDuration = motion_manager::instance().motion_duration(race_num(), make_motion_key(MOTION_MODE_GENERAL, MOTION_SPECIAL_1 + iSkillIdx));
					state_duration_ = (uint32_t) (fDuration == 0.0f ? passes_per_sec(2) : passes_per_sec(fDuration));

					if (metin2::config::test_server())
						metin2::log::debug("Mob {} uses skill index {:d} (motion {:d}, duration {:.0f})", name(), iSkillIdx, MOTION_SPECIAL_1 + iSkillIdx, fDuration);

					return;
				}
			}
		}
	}

	if (!attack(victim_value))
		state_duration_ = metin2::config::server().passes_per_sec / 2;
	else
	{
		set_rotation_to_xy(victim_value->x(), victim_value->y());

		send_move_packet(FUNC_ATTACK, 0, x(), y(), 0, dwCurTime);

		float fDuration = motion_manager::instance().motion_duration(race_num(), make_motion_key(MOTION_MODE_GENERAL, MOTION_NORMAL_ATTACK));
		state_duration_ = (uint32_t) (fDuration == 0.0f ? passes_per_sec(2) : passes_per_sec(fDuration));
	}
}

void character::state_flag()
{
	state_duration_ = (uint32_t) passes_per_sec(0.5);

	war_map * pMap = get_war_map();

	if (!pMap)
		return;

	func_find_chr_for_flag f(this);
	get_sectree()->for_each_around(f);

	if (!f.chr_find)
		return;

	if (nullptr == f.chr_find->get_guild())
		return;

	char buf[256];
	uint8_t idx;

	if (!pMap->team_index(point(POINT_STAT), idx))
		return;

	f.chr_find->add_affect(AFFECT_WAR_FLAG, POINT_NONE, point(POINT_STAT), idx == 0 ? AFF_WAR_FLAG1 : AFF_WAR_FLAG2, INFINITE_AFFECT_DURATION, 0, false);
	f.chr_find->add_affect(AFFECT_WAR_FLAG, POINT_MOV_SPEED, 50 - f.chr_find->point(POINT_MOV_SPEED), 0, INFINITE_AFFECT_DURATION, 0, false);

	pMap->remove_flag(idx);

	snprintf(buf, sizeof(buf), lc_text("%s 길드의 깃발을 %s 님이 획득하였습니다."), pMap->get_guild(idx)->name(), f.chr_find->name());
	pMap->notice(buf);
}

void character::state_flag_base()
{
	state_duration_ = (uint32_t) passes_per_sec(0.5);

	func_find_chr_for_flag_base f(this);
	get_sectree()->for_each_around(f);
}

void character::state_horse()
{
	float	START_FOLLOW_DISTANCE = 400.0f;
	float	START_RUN_DISTANCE = 700.0f;
	int		MIN_APPROACH = 150;
	int		MAX_APPROACH = 300;

	uint32_t	STATE_DURATION = (uint32_t)passes_per_sec(0.5);

	bool bDoMoveAlone = true;
	bool bRun = true;

	if (is_dead())
		return;

	state_duration_ = STATE_DURATION;

	character * victim = rider();

	if (!victim)
	{
		character_manager::instance().destroy_character(this);
		return;
	}

	mob_inst->pos_last_attacked = xyz();

	float fDist = distance_approx(x() - victim->x(), y() - victim->y());

	if (fDist >= START_FOLLOW_DISTANCE)
	{
		if (fDist > START_RUN_DISTANCE)
			set_now_walking(!bRun);

		follow(victim, number(MIN_APPROACH, MAX_APPROACH));

		state_duration_ = STATE_DURATION;
	}
	else if (bDoMoveAlone && (get_dword_time() > last_attack_time_))
	{
		last_attack_time_ = get_dword_time() + number(5000, 12000);

		set_rotation(number(0, 359));

		float fx, fy;
		float fDist = number(200, 400);

		get_delta_by_degree(rotation(), fDist, &fx, &fy);

		if (!(sectree_manager::instance().is_movable_position(map_index(), x() + (int) fx, y() + (int) fy) 
					&& sectree_manager::instance().is_movable_position(map_index(), x() + (int) fx/2, y() + (int) fy/2)))
			return;

		set_now_walking(true);

		if (go_to(x() + (int) fx, y() + (int) fy))
			send_move_packet(FUNC_WAIT, 0, 0, 0, 0);
	}
}
}
