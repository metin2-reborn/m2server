
#include <metin2/game/events/blue_dragon.h>

#include <metin2/game/events/blue_dragon_binder.h>
#include <metin2/game/events/blue_dragon_skill.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/app/config.h>
#include <metin2/game/characters/motion.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/app/typedef.h>

#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <cstddef>
#include <cstdint>
#include <ctime>

namespace metin2::game::inline events {

auto use_blue_dragon_skill(character * ch, unsigned int idx) -> time_t
{
	sectree_map * pSecMap = sectree_manager::instance().map( ch->map_index() );

	if (nullptr == pSecMap)
		return 0;

	int nextUsingTime = 0;

	switch (idx)
	{
		case 0:
			{
				metin2::log::debug( "Blue dragon: using breath");

				f_skill_breath f(ch);

				pSecMap->for_each( f );

				nextUsingTime = number(blue_dragon_get_skill_factor(3, "Skill0", "period", "min"), blue_dragon_get_skill_factor(3, "Skill0", "period", "max"));
			}
			break;

		case 1:
			{
				metin2::log::debug( "Blue dragon: using weak breath");

				f_skill_weak_breath f(ch);

				pSecMap->for_each( f );

				nextUsingTime = number(blue_dragon_get_skill_factor(3, "Skill1", "period", "min"), blue_dragon_get_skill_factor(3, "Skill1", "period", "max"));
			}
			break;

		case 2:
			{
				metin2::log::debug( "Blue dragon: using earthquake");

				f_skill_earth_quake f(ch);

				pSecMap->for_each( f );

				nextUsingTime = number(blue_dragon_get_skill_factor(3, "Skill2", "period", "min"), blue_dragon_get_skill_factor(3, "Skill2", "period", "max"));

				if (nullptr != f.farthest_char)
				{
					ch->begin_fight( f.farthest_char );
				}
			}
			break;

		default:
			metin2::log::error("Blue dragon: unknown skill index {:d}", idx);
			return 0;
	}

	int addPct = blue_dragon_get_range_factor("hp_period", ch->hp_pct());

	nextUsingTime += (nextUsingTime * addPct) / 100;

	return nextUsingTime;
}

auto blue_dragon_state_battle(character * ch) -> int
{
	if (ch->hp_pct() > 98)
		return passes_per_sec(1);

	const int SkillCount = 3;
	int SkillPriority[SkillCount];
	static time_t timeSkillCanUseTime[SkillCount];

	if (ch->hp_pct() > 76)
	{
		SkillPriority[0] = 1;
		SkillPriority[1] = 0;
		SkillPriority[2] = 2;
	}
	else if (ch->hp_pct() > 31)
	{
		SkillPriority[0] = 0;
		SkillPriority[1] = 1;
		SkillPriority[2] = 2;
	}
	else
	{
		SkillPriority[0] = 0;
		SkillPriority[1] = 2;
		SkillPriority[2] = 1;
	}

	time_t timeNow = static_cast<time_t>(get_dword_time());

	for (int i=0 ; i < SkillCount ; ++i)
	{
		const int SkillIndex = SkillPriority[i];

		if (timeSkillCanUseTime[SkillIndex] < timeNow)
		{
			int SkillUsingDuration =
				static_cast<int>(motion_manager::instance().motion_duration( ch->race_num(), make_motion_key(MOTION_MODE_GENERAL, MOTION_SPECIAL_1 + SkillIndex) ));

			timeSkillCanUseTime[SkillIndex] = timeNow + (use_blue_dragon_skill( ch, SkillIndex ) * 1000) + SkillUsingDuration + 3000;

			ch->send_move_packet(FUNC_MOB_SKILL, SkillIndex, ch->x(), ch->y(), 0, timeNow);

			return 0 == SkillUsingDuration ? passes_per_sec(1) : passes_per_sec(SkillUsingDuration);
		}
	}

	return passes_per_sec(1);
}

auto blue_dragon_damage (character * me, character * attacker, int dam) -> int
{
	if (nullptr == me || nullptr == attacker)
		return dam;

	if (true == attacker->is_monster() && 2493 == attacker->get_mob_table().vnum)
	{
		for (int i=1 ; i <= 4 ; ++i)
		{
			if (ATK_BONUS == blue_dragon_get_index_factor("DragonStone", i, "effect_type"))
			{
				uint32_t dwDragonStoneID = blue_dragon_get_index_factor("DragonStone", i, "vnum");
				size_t val = blue_dragon_get_index_factor("DragonStone", i, "val");
				size_t cnt = sectree_manager::instance().monster_count_in_map( attacker->map_index(), dwDragonStoneID );

				dam += (dam * (val*cnt))/100;

				break;
			}
		}
	}

	if (true == me->is_monster() && 2493 == me->get_mob_table().vnum)
	{
		for (int i=1 ; i <= 4 ; ++i)
		{
			if (DEF_BONUS == blue_dragon_get_index_factor("DragonStone", i, "effect_type"))
			{
				uint32_t dwDragonStoneID = blue_dragon_get_index_factor("DragonStone", i, "vnum");
				size_t val = blue_dragon_get_index_factor("DragonStone", i, "val");
				size_t cnt = sectree_manager::instance().monster_count_in_map( me->map_index(), dwDragonStoneID );

				dam -= (dam * (val*cnt))/100;

				if (dam <= 0)
					dam = 1;

				break;
			}
		}
	}

	if (true == me->is_stone() && 0 != attacker->get_mount_vnum())
	{
		for (int i=1 ; i <= 4 ; ++i)
		{
			if (me->get_mob_table().vnum == blue_dragon_get_index_factor("DragonStone", i, "vnum"))
			{
				if (attacker->get_mount_vnum() == blue_dragon_get_index_factor("DragonStone", i, "enemy"))
				{
					size_t val = blue_dragon_get_index_factor("DragonStone", i, "enemy_val");

					dam *= val;

					break;
				}
			}
		}
	}

	return dam;
}
}
