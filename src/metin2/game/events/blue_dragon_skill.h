#include <metin2/game/characters/affect.h>
#include <metin2/game/combat/battle.h>
#include <metin2/game/events/blue_dragon_binder.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/world/sectree.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/world/vector.h>

#include <metin2/proto/length.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <cstdint>

#pragma once

namespace metin2::game::inline events {

struct f_skill_breath
{
	jobs set1;
	jobs set2;
	sex gender;
	character * attacker;

	f_skill_breath(character * p)
	{
		attacker = p;

		set1 = static_cast<jobs>(number(0,3));
		set2 = static_cast<jobs>(number(0,3));
		gender = static_cast<sex>(number(0,2));
	}

	void operator()(entity * ent)
	{
		if (nullptr != ent)
		{
			if (true == ent->is_type(ENTITY_CHARACTER))
			{
				character * ch = static_cast<character *>(ent);

				if (true == ch->is_pc() && false == ch->is_dead())
				{
					if (nullptr != ch->find_affect(AFFECT_REVIVE_INVISIBLE, APPLY_NONE))
						return;

					if ((signed)blue_dragon_get_skill_factor(2, "Skill0", "damage_area") < distance_approx(attacker->x()-ch->x(), attacker->y()-ch->y()))
					{
						metin2::log::info( "BlueDragon: Breath too far ({:d})", distance_approx(attacker->x()-ch->x(), attacker->y()-ch->y()) );
						return;
					}

					int overlapDamageCount = 0;

					int pct = 0;
					if (ch->job() == set1)
					{
						const char* ptr = nullptr;

						switch ( set1 )
						{
							case JOB_WARRIOR:	ptr = "musa";	break;
							case JOB_ASSASSIN:	ptr = "assa";	break;
							case JOB_SURA:		ptr = "sura";	break;
							case JOB_SHAMAN:	ptr = "muda";	break;

							default:
							case JOB_MAX_NUM:	return;
						}


						int firstDamagePercent =  number(blue_dragon_get_skill_factor(4, "Skill0", "damage", ptr, "min"), blue_dragon_get_skill_factor(4, "Skill0", "damage", ptr, "max"));
						pct += firstDamagePercent;

						if (firstDamagePercent > 0)
							overlapDamageCount++;
					}

					if (ch->job() == set2)
					{
						const char* ptr = nullptr;

						switch ( set2 )
						{
							case JOB_WARRIOR:	ptr = "musa";	break;
							case JOB_ASSASSIN:	ptr = "assa";	break;
							case JOB_SURA:		ptr = "sura";	break;
							case JOB_SHAMAN:	ptr = "muda";	break;

							default:
							case JOB_MAX_NUM:	return;
						}

						int secondDamagePercent = number(blue_dragon_get_skill_factor(4, "Skill0", "damage", ptr, "min"), blue_dragon_get_skill_factor(4, "Skill0", "damage", ptr, "max"));
						pct += secondDamagePercent;

						if (secondDamagePercent > 0)
							overlapDamageCount++;
					}

					if (get_sex(ch) == gender)
					{
						const char* ptr = nullptr;

						switch (gender)
						{
							case SEX_MALE:		ptr = "male";	break;
							case SEX_FEMALE:	ptr = "female";	break;
							default:			return;
						}

						int thirdDamagePercent = number(blue_dragon_get_skill_factor(4, "Skill0", "gender", ptr, "min"), blue_dragon_get_skill_factor(4, "Skill0", "gender", ptr, "max"));
						pct += thirdDamagePercent;

						if (thirdDamagePercent > 0)
							overlapDamageCount++;
					}

					switch (overlapDamageCount)
					{
						case 1:
							ch->effect_packet(SE_PERCENT_DAMAGE1);
							break;
						case 2:
							ch->effect_packet(SE_PERCENT_DAMAGE2);
							break;
						case 3:
							ch->effect_packet(SE_PERCENT_DAMAGE3);
							break;
					}

					int addPct = blue_dragon_get_range_factor("hp_damage", attacker->hp_pct());

					pct += addPct;

					int dam = number(blue_dragon_get_skill_factor(3, "Skill0", "default_damage", "min"), blue_dragon_get_skill_factor(3, "Skill0", "default_damage", "max"));

					dam += (dam * addPct) / 100;
					dam += (ch->max_hp() * pct) / 100;

					ch->damage( attacker, dam, DAMAGE_TYPE_ICE );

					metin2::log::info( "BlueDragon: Breath to {} pct({:d}) dam({:d}) overlap({:d})", ch->name(), pct, dam, overlapDamageCount);
				}
			}
		}
	}
};

struct f_skill_weak_breath
{
	character * attacker;

	f_skill_weak_breath(character * p)
	{
		attacker = p;
	}

	void operator()(entity * ent)
	{
		if (nullptr != ent)
		{
			if (true == ent->is_type(ENTITY_CHARACTER))
			{
				character * ch = static_cast<character *>(ent);

				if (true == ch->is_pc() && false == ch->is_dead())
				{
					if (nullptr != ch->find_affect(AFFECT_REVIVE_INVISIBLE, APPLY_NONE))
						return;

					if ((signed)blue_dragon_get_skill_factor(2, "Skill1", "damage_area") < distance_approx(attacker->x()-ch->x(), attacker->y()-ch->y()))
					{
						metin2::log::info( "BlueDragon: Breath too far ({:d})", distance_approx(attacker->x()-ch->x(), attacker->y()-ch->y()) );
						return;
					}

					int addPct = blue_dragon_get_range_factor("hp_damage", attacker->hp_pct());

					int dam = number( blue_dragon_get_skill_factor(3, "Skill1", "default_damage", "min"), blue_dragon_get_skill_factor(3, "Skill1", "default_damage", "max") );
					dam += (dam * addPct) / 100;

					ch->damage( attacker, dam, DAMAGE_TYPE_ICE );

					metin2::log::info( "BlueDragon: WeakBreath to {} addPct({:d}) dam({:d})", ch->name(), addPct, dam);
				}
			}
		}
	}
};

struct f_skill_earth_quake
{
	jobs set1;
	jobs set2;
	sex gender;
	int32_t max_distance;
	character * attacker;
	character * farthest_char;

	f_skill_earth_quake(character * p)
	{
		attacker = p;

		max_distance = 0;
		farthest_char = nullptr;

		set1 = static_cast<jobs>(number(0,3));
		set2 = static_cast<jobs>(number(0,3));
		gender = static_cast<sex>(number(0,2));
	}

	void operator()(entity * ent)
	{
		if (nullptr != ent)
		{
			if (true == ent->is_type(ENTITY_CHARACTER))
			{
				character * ch = static_cast<character *>(ent);

				if (true == ch->is_pc() && false == ch->is_dead())
				{
					if (nullptr != ch->find_affect(AFFECT_REVIVE_INVISIBLE, APPLY_NONE))
						return;

					if ((signed)blue_dragon_get_skill_factor(2, "Skill2", "damage_area") < distance_approx(attacker->x()-ch->x(), attacker->y()-ch->y()))
					{
						metin2::log::info( "BlueDragon: Breath too far ({:d})", distance_approx(attacker->x()-ch->x(), attacker->y()-ch->y()) );
						return;
					}

					int sec = number(blue_dragon_get_skill_factor(4, "Skill2", "stun_time", "default", "min"), blue_dragon_get_skill_factor(4, "Skill2", "stun_time", "default", "max"));

					if (ch->job() == set1)
					{
						const char* ptr = nullptr;

						switch ( set1 )
						{
							case JOB_WARRIOR:	ptr = "musa";	break;
							case JOB_ASSASSIN:	ptr = "assa";	break;
							case JOB_SURA:		ptr = "sura";	break;
							case JOB_SHAMAN:	ptr = "muda";	break;

							default:
							case JOB_MAX_NUM:	return;
						}

						sec += number(blue_dragon_get_skill_factor(4, "Skill2", "stun_time", ptr, "min"), blue_dragon_get_skill_factor(4, "Skill2", "stun_time", ptr, "max"));
					}

					if (ch->job() == set2)
					{
						const char* ptr = nullptr;

						switch ( set2 )
						{
							case JOB_WARRIOR:	ptr = "musa";	break;
							case JOB_ASSASSIN:	ptr = "assa";	break;
							case JOB_SURA:		ptr = "sura";	break;
							case JOB_SHAMAN:	ptr = "muda";	break;

							default:
							case JOB_MAX_NUM:	return;
						}

						sec += number(blue_dragon_get_skill_factor(4, "Skill2", "stun_time", ptr, "min"), blue_dragon_get_skill_factor(4, "Skill2", "stun_time", ptr, "max"));
					}

					if (get_sex(ch) == gender)
					{
						const char* ptr = nullptr;

						switch (gender)
						{
							case SEX_MALE:		ptr = "male";	break;
							case SEX_FEMALE:	ptr = "female";	break;
							default:			return;
						}

						sec += number(blue_dragon_get_skill_factor(4, "Skill2", "gender", ptr, "min"), blue_dragon_get_skill_factor(4, "Skill2", "gender", ptr, "max"));
					}

					int addPct = blue_dragon_get_range_factor("hp_damage", attacker->hp_pct());

					int dam = number( blue_dragon_get_skill_factor(3, "Skill2", "default_damage", "min"), blue_dragon_get_skill_factor(3, "Skill2", "default_damage", "max") );
					dam += (dam * addPct) / 100;

					ch->damage( attacker, dam, DAMAGE_TYPE_ICE);

					skill_attack_affect( ch, 1000, IMMUNE_STUN, AFFECT_STUN, POINT_NONE, 0, AFF_STUN, sec, "BDRAGON_STUN" );

					metin2::log::info( "BlueDragon: EarthQuake to {} addPct({:d}) dam({:d}) sec({:d})", ch->name(), addPct, dam, sec);


					vector vec;
					vec.x = static_cast<float>(attacker->x() - ch->x());
					vec.y = static_cast<float>(attacker->y() - ch->y());
					vec.z = 0.0f;

					normalize( &vec, &vec );

					const int nFlyDistance = 1000;

					int32_t tx = ch->x() + vec.x * nFlyDistance;
					int32_t ty = ch->y() + vec.y * nFlyDistance;

					for (int i=0 ; i < 5 ; ++i)
					{
						if (true == sectree_manager::instance().is_movable_position( ch->map_index(), tx, ty ))
						{
							break;
						}

						switch( i )
						{
							case 0:
								tx = ch->x() + vec.x * nFlyDistance * -1;
								ty = ch->y() + vec.y * nFlyDistance * -1;
								break;
							case 1:
								tx = ch->x() + vec.x * nFlyDistance * -1;
								ty = ch->y() + vec.y * nFlyDistance;
								break;
							case 2:
								tx = ch->x() + vec.x * nFlyDistance;
								ty = ch->y() + vec.y * nFlyDistance * -1;
								break;
							case 3:
								tx = ch->x() + vec.x * number(1,100);
								ty = ch->y() + vec.y * number(1,100);
								break;
							case 4:
								tx = ch->x() + vec.x * number(1,10);
								ty = ch->y() + vec.y * number(1,10);
								break;
						}
					}

					ch->sync( tx , ty );
					ch->goto_( tx , ty );
					ch->calculate_move_duration();

					ch->sync_packet();

					int32_t dist = distance_approx( attacker->x() - ch->x(), attacker->y() - ch->y() );

					if (dist > max_distance)
					{
						max_distance = dist;
						farthest_char = ch;
					}
				}
			}
		}
	}
};
}
