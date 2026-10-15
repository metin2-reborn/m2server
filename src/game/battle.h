#pragma once

#include "char.h"
#include "config.h"
#include "typedef.h"

#include <common/length.h>
#include <libthecore/utils.h>

#include <cstdint>

enum EBattleTypes       // Relative to the opponent
{
	BATTLE_NONE,
	BATTLE_DAMAGE,
	BATTLE_DEFENSE,
	BATTLE_DEAD
};

extern int	CalcAttBonus(CHARACTER * pkAttacker, CHARACTER * pkVictim, int iAtk);
extern int	CalcBattleDamage(int iDam, int iAttackerLev, int iVictimLev);
extern int	CalcMeleeDamage(CHARACTER * pAttacker, CHARACTER * pVictim, bool bIgnoreDefense = false, bool bIgnoreTargetRating = false);
extern int	CalcMagicDamage(CHARACTER * pAttacker, CHARACTER * pVictim);
extern int	CalcArrowDamage(CHARACTER * pkAttacker, CHARACTER * pkVictim, CItem * pkBow, CItem * pkArrow, bool bIgnoreDefense = false);
extern float	CalcAttackRating(CHARACTER * pkAttacker, CHARACTER * pkVictim, bool bIgnoreTargetRating = false);

extern bool	battle_is_attackable(CHARACTER * ch, CHARACTER * victim);
extern int	battle_melee_attack(CHARACTER * ch, CHARACTER * victim);
extern void	battle_end(CHARACTER * ch);

extern void	NormalAttackAffect(CHARACTER * pkAttacker, CHARACTER * pkVictim);

// Special attack
inline void AttackAffect(CHARACTER * pkAttacker,
		CHARACTER * pkVictim,
		uint8_t att_point,
		uint32_t immune_flag,
		uint32_t affect_idx,
		uint8_t affect_point,
		int32_t affect_amount,
		uint32_t affect_flag,
		int time,
		const char* name)
{
	if (pkAttacker->GetPoint(att_point) && !pkVictim->IsAffectFlag(affect_flag))
	{
		if (number(1, 100) <= pkAttacker->GetPoint(att_point) && !pkVictim->IsImmune(immune_flag))
		{
			pkVictim->AddAffect(affect_idx, affect_point, affect_amount, affect_flag, time, 0, true);

			if (test_server)
			{
				pkVictim->ChatPacket(CHAT_TYPE_PARTY, "%s %s(%d%%) SUCCESS", pkAttacker->GetName(), name, pkAttacker->GetPoint(att_point));
			}
		}
		else if (test_server)
		{
			pkVictim->ChatPacket(CHAT_TYPE_PARTY, "%s %s(%d%%) FAIL", pkAttacker->GetName(), name, pkAttacker->GetPoint(att_point));
		}
	}
}

inline void SkillAttackAffect(CHARACTER * pkVictim,
		int success_pct,
		uint32_t immune_flag,
		uint32_t affect_idx,
		uint8_t affect_point,
		int32_t affect_amount,
		uint32_t affect_flag,
		int time,
		const char* name)
{
	if (success_pct && !pkVictim->IsAffectFlag(affect_flag))
	{
		if (number(1, 1000) <= success_pct && !pkVictim->IsImmune(immune_flag))
		{
			pkVictim->AddAffect(affect_idx, affect_point, affect_amount, affect_flag, time, 0, true);

			// SKILL_ATTACK_NO_LOG_TARGET_NAME_FIX
			if (test_server)
				pkVictim->ChatPacket(CHAT_TYPE_PARTY, 
						"%s(%d%%) -> %s SUCCESS", name, success_pct, name);
			// END_OF_SKILL_ATTACK_LOG_NO_TARGET_NAME_FIX
		}
		else if (test_server)
		{
			// SKILL_ATTACK_NO_LOG_TARGET_NAME_FIX
			pkVictim->ChatPacket(CHAT_TYPE_PARTY, "%s(%d%%) -> %s FAIL", name, success_pct, name);
			// END_OF_SKILL_ATTACK_LOG_NO_TARGET_NAME_FIX
		}
	}
} 

uint32_t	GET_ATTACK_SPEED(CHARACTER * ch);
void	SET_ATTACK_TIME(CHARACTER * ch, CHARACTER * victim, uint32_t current_time);
void	SET_ATTACKED_TIME(CHARACTER * ch, CHARACTER * victim, uint32_t current_time);
bool	IS_SPEED_HACK(CHARACTER * ch, CHARACTER * victim, uint32_t current_time);
