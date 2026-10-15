#include <metin2/game/characters/affect.h>
#include <metin2/game/combat/battle.h>
#include <metin2/game/events/BlueDragon_Binder.h>
#include <metin2/game/characters/char.h>
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

struct FSkillBreath
{
	EJobs Set1;
	EJobs Set2;
	ESex gender;
	CHARACTER * pAttacker;

	FSkillBreath(CHARACTER * p)
	{
		pAttacker = p;

		Set1 = static_cast<EJobs>(number(0,3));
		Set2 = static_cast<EJobs>(number(0,3));
		gender = static_cast<ESex>(number(0,2));
	}

	void operator()(CEntity * ent)
	{
		if (nullptr != ent)
		{
			if (true == ent->IsType(ENTITY_CHARACTER))
			{
				CHARACTER * ch = static_cast<CHARACTER *>(ent);

				if (true == ch->IsPC() && false == ch->IsDead())
				{
					if (nullptr != ch->FindAffect(AFFECT_REVIVE_INVISIBLE, APPLY_NONE))
						return;

					if ((signed)BlueDragon_GetSkillFactor(2, "Skill0", "damage_area") < DISTANCE_APPROX(pAttacker->GetX()-ch->GetX(), pAttacker->GetY()-ch->GetY()))
					{
						metin2::log::info( "BlueDragon: Breath too far ({:d})", DISTANCE_APPROX(pAttacker->GetX()-ch->GetX(), pAttacker->GetY()-ch->GetY()) );
						return;
					}

					int overlapDamageCount = 0;

					int pct = 0;
					if (ch->GetJob() == Set1)
					{
						const char* ptr = nullptr;

						switch ( Set1 )
						{
							case JOB_WARRIOR:	ptr = "musa";	break;
							case JOB_ASSASSIN:	ptr = "assa";	break;
							case JOB_SURA:		ptr = "sura";	break;
							case JOB_SHAMAN:	ptr = "muda";	break;

							default:
							case JOB_MAX_NUM:	return;
						}


						int firstDamagePercent =  number(BlueDragon_GetSkillFactor(4, "Skill0", "damage", ptr, "min"), BlueDragon_GetSkillFactor(4, "Skill0", "damage", ptr, "max"));
						pct += firstDamagePercent;

						if (firstDamagePercent > 0)
							overlapDamageCount++;
					}

					if (ch->GetJob() == Set2)
					{
						const char* ptr = nullptr;

						switch ( Set2 )
						{
							case JOB_WARRIOR:	ptr = "musa";	break;
							case JOB_ASSASSIN:	ptr = "assa";	break;
							case JOB_SURA:		ptr = "sura";	break;
							case JOB_SHAMAN:	ptr = "muda";	break;

							default:
							case JOB_MAX_NUM:	return;
						}

						int secondDamagePercent = number(BlueDragon_GetSkillFactor(4, "Skill0", "damage", ptr, "min"), BlueDragon_GetSkillFactor(4, "Skill0", "damage", ptr, "max"));
						pct += secondDamagePercent;

						if (secondDamagePercent > 0)
							overlapDamageCount++;
					}

					if (GET_SEX(ch) == gender)
					{
						const char* ptr = nullptr;

						switch (gender)
						{
							case SEX_MALE:		ptr = "male";	break;
							case SEX_FEMALE:	ptr = "female";	break;
							default:			return;
						}

						int thirdDamagePercent = number(BlueDragon_GetSkillFactor(4, "Skill0", "gender", ptr, "min"), BlueDragon_GetSkillFactor(4, "Skill0", "gender", ptr, "max"));
						pct += thirdDamagePercent;

						if (thirdDamagePercent > 0)
							overlapDamageCount++;
					}

					switch (overlapDamageCount)
					{
						case 1:
							ch->EffectPacket(SE_PERCENT_DAMAGE1);
							break;
						case 2:
							ch->EffectPacket(SE_PERCENT_DAMAGE2);
							break;
						case 3:
							ch->EffectPacket(SE_PERCENT_DAMAGE3);
							break;
					}

					int addPct = BlueDragon_GetRangeFactor("hp_damage", pAttacker->GetHPPct());

					pct += addPct;

					int dam = number(BlueDragon_GetSkillFactor(3, "Skill0", "default_damage", "min"), BlueDragon_GetSkillFactor(3, "Skill0", "default_damage", "max"));

					dam += (dam * addPct) / 100;
					dam += (ch->GetMaxHP() * pct) / 100;

					ch->Damage( pAttacker, dam, DAMAGE_TYPE_ICE );

					metin2::log::info( "BlueDragon: Breath to {} pct({:d}) dam({:d}) overlap({:d})", ch->GetName(), pct, dam, overlapDamageCount);
				}
			}
		}
	}
};

struct FSkillWeakBreath
{
	CHARACTER * pAttacker;

	FSkillWeakBreath(CHARACTER * p)
	{
		pAttacker = p;
	}

	void operator()(CEntity * ent)
	{
		if (nullptr != ent)
		{
			if (true == ent->IsType(ENTITY_CHARACTER))
			{
				CHARACTER * ch = static_cast<CHARACTER *>(ent);

				if (true == ch->IsPC() && false == ch->IsDead())
				{
					if (nullptr != ch->FindAffect(AFFECT_REVIVE_INVISIBLE, APPLY_NONE))
						return;

					if ((signed)BlueDragon_GetSkillFactor(2, "Skill1", "damage_area") < DISTANCE_APPROX(pAttacker->GetX()-ch->GetX(), pAttacker->GetY()-ch->GetY()))
					{
						metin2::log::info( "BlueDragon: Breath too far ({:d})", DISTANCE_APPROX(pAttacker->GetX()-ch->GetX(), pAttacker->GetY()-ch->GetY()) );
						return;
					}

					int addPct = BlueDragon_GetRangeFactor("hp_damage", pAttacker->GetHPPct());

					int dam = number( BlueDragon_GetSkillFactor(3, "Skill1", "default_damage", "min"), BlueDragon_GetSkillFactor(3, "Skill1", "default_damage", "max") );
					dam += (dam * addPct) / 100;

					ch->Damage( pAttacker, dam, DAMAGE_TYPE_ICE );

					metin2::log::info( "BlueDragon: WeakBreath to {} addPct({:d}) dam({:d})", ch->GetName(), addPct, dam);
				}
			}
		}
	}
};

struct FSkillEarthQuake
{
	EJobs Set1;
	EJobs Set2;
	ESex gender;
	int32_t MaxDistance;
	CHARACTER * pAttacker;
	CHARACTER * pFarthestChar;

	FSkillEarthQuake(CHARACTER * p)
	{
		pAttacker = p;

		MaxDistance = 0;
		pFarthestChar = nullptr;

		Set1 = static_cast<EJobs>(number(0,3));
		Set2 = static_cast<EJobs>(number(0,3));
		gender = static_cast<ESex>(number(0,2));
	}

	void operator()(CEntity * ent)
	{
		if (nullptr != ent)
		{
			if (true == ent->IsType(ENTITY_CHARACTER))
			{
				CHARACTER * ch = static_cast<CHARACTER *>(ent);

				if (true == ch->IsPC() && false == ch->IsDead())
				{
					if (nullptr != ch->FindAffect(AFFECT_REVIVE_INVISIBLE, APPLY_NONE))
						return;

					if ((signed)BlueDragon_GetSkillFactor(2, "Skill2", "damage_area") < DISTANCE_APPROX(pAttacker->GetX()-ch->GetX(), pAttacker->GetY()-ch->GetY()))
					{
						metin2::log::info( "BlueDragon: Breath too far ({:d})", DISTANCE_APPROX(pAttacker->GetX()-ch->GetX(), pAttacker->GetY()-ch->GetY()) );
						return;
					}

					int sec = number(BlueDragon_GetSkillFactor(4, "Skill2", "stun_time", "default", "min"), BlueDragon_GetSkillFactor(4, "Skill2", "stun_time", "default", "max"));

					if (ch->GetJob() == Set1)
					{
						const char* ptr = nullptr;

						switch ( Set1 )
						{
							case JOB_WARRIOR:	ptr = "musa";	break;
							case JOB_ASSASSIN:	ptr = "assa";	break;
							case JOB_SURA:		ptr = "sura";	break;
							case JOB_SHAMAN:	ptr = "muda";	break;

							default:
							case JOB_MAX_NUM:	return;
						}

						sec += number(BlueDragon_GetSkillFactor(4, "Skill2", "stun_time", ptr, "min"), BlueDragon_GetSkillFactor(4, "Skill2", "stun_time", ptr, "max"));
					}

					if (ch->GetJob() == Set2)
					{
						const char* ptr = nullptr;

						switch ( Set2 )
						{
							case JOB_WARRIOR:	ptr = "musa";	break;
							case JOB_ASSASSIN:	ptr = "assa";	break;
							case JOB_SURA:		ptr = "sura";	break;
							case JOB_SHAMAN:	ptr = "muda";	break;

							default:
							case JOB_MAX_NUM:	return;
						}

						sec += number(BlueDragon_GetSkillFactor(4, "Skill2", "stun_time", ptr, "min"), BlueDragon_GetSkillFactor(4, "Skill2", "stun_time", ptr, "max"));
					}

					if (GET_SEX(ch) == gender)
					{
						const char* ptr = nullptr;

						switch (gender)
						{
							case SEX_MALE:		ptr = "male";	break;
							case SEX_FEMALE:	ptr = "female";	break;
							default:			return;
						}

						sec += number(BlueDragon_GetSkillFactor(4, "Skill2", "gender", ptr, "min"), BlueDragon_GetSkillFactor(4, "Skill2", "gender", ptr, "max"));
					}

					int addPct = BlueDragon_GetRangeFactor("hp_damage", pAttacker->GetHPPct());

					int dam = number( BlueDragon_GetSkillFactor(3, "Skill2", "default_damage", "min"), BlueDragon_GetSkillFactor(3, "Skill2", "default_damage", "max") );
					dam += (dam * addPct) / 100;

					ch->Damage( pAttacker, dam, DAMAGE_TYPE_ICE);

					SkillAttackAffect( ch, 1000, IMMUNE_STUN, AFFECT_STUN, POINT_NONE, 0, AFF_STUN, sec, "BDRAGON_STUN" );

					metin2::log::info( "BlueDragon: EarthQuake to {} addPct({:d}) dam({:d}) sec({:d})", ch->GetName(), addPct, dam, sec);


					VECTOR vec;
					vec.x = static_cast<float>(pAttacker->GetX() - ch->GetX());
					vec.y = static_cast<float>(pAttacker->GetY() - ch->GetY());
					vec.z = 0.0f;

					Normalize( &vec, &vec );

					const int nFlyDistance = 1000;

					int32_t tx = ch->GetX() + vec.x * nFlyDistance;
					int32_t ty = ch->GetY() + vec.y * nFlyDistance;

					for (int i=0 ; i < 5 ; ++i)
					{
						if (true == SECTREE_MANAGER::instance().IsMovablePosition( ch->GetMapIndex(), tx, ty ))
						{
							break;
						}

						switch( i )
						{
							case 0:
								tx = ch->GetX() + vec.x * nFlyDistance * -1;
								ty = ch->GetY() + vec.y * nFlyDistance * -1;
								break;
							case 1:
								tx = ch->GetX() + vec.x * nFlyDistance * -1;
								ty = ch->GetY() + vec.y * nFlyDistance;
								break;
							case 2:
								tx = ch->GetX() + vec.x * nFlyDistance;
								ty = ch->GetY() + vec.y * nFlyDistance * -1;
								break;
							case 3:
								tx = ch->GetX() + vec.x * number(1,100);
								ty = ch->GetY() + vec.y * number(1,100);
								break;
							case 4:
								tx = ch->GetX() + vec.x * number(1,10);
								ty = ch->GetY() + vec.y * number(1,10);
								break;
						}
					}

					ch->Sync( tx , ty );
					ch->Goto( tx , ty );
					ch->CalculateMoveDuration();

					ch->SyncPacket();

					int32_t dist = DISTANCE_APPROX( pAttacker->GetX() - ch->GetX(), pAttacker->GetY() - ch->GetY() );

					if (dist > MaxDistance)
					{
						MaxDistance = dist;
						pFarthestChar = ch;
					}
				}
			}
		}
	}
};
}
