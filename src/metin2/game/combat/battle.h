#pragma once

#include <metin2/game/characters/character.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/core/utils.h>

#include <cstdint>

namespace metin2::game::inline combat {

enum battle_types       // Relative to the opponent
{
	BATTLE_NONE,
	BATTLE_DAMAGE,
	BATTLE_DEFENSE,
	BATTLE_DEAD
};

extern int	calc_att_bonus(character * pkAttacker, character * pkVictim, int iAtk);
extern int	calc_battle_damage(int iDam, int iAttackerLev, int iVictimLev);
extern int	calc_melee_damage(character * pAttacker, character * pVictim, bool bIgnoreDefense = false, bool bIgnoreTargetRating = false);
extern int	calc_magic_damage(character * pAttacker, character * pVictim);
extern int	calc_arrow_damage(character * pkAttacker, character * pkVictim, item * pkBow, item * pkArrow, bool bIgnoreDefense = false);
extern float	calc_attack_rating(character * pkAttacker, character * pkVictim, bool bIgnoreTargetRating = false);

extern bool	battle_is_attackable(character * ch, character * victim);
extern int	battle_melee_attack(character * ch, character * victim);
extern void	battle_end(character * ch);

extern void	normal_attack_affect(character * pkAttacker, character * pkVictim);

// Special attack
inline void attack_affect(character * pkAttacker,
		character * pkVictim,
		uint8_t att_point,
		uint32_t immune_flag,
		uint32_t affect_idx,
		uint8_t affect_point,
		int32_t affect_amount,
		uint32_t affect_flag,
		int time,
		const char* name)
{
	if (pkAttacker->point(att_point) && !pkVictim->is_affect_flag(affect_flag))
	{
		if (number(1, 100) <= pkAttacker->point(att_point) && !pkVictim->is_immune(immune_flag))
		{
			pkVictim->add_affect(affect_idx, affect_point, affect_amount, affect_flag, time, 0, true);

			if (metin2::config::test_server())
			{
				pkVictim->chat_packet(CHAT_TYPE_PARTY, "%s %s(%d%%) SUCCESS", pkAttacker->name(), name, pkAttacker->point(att_point));
			}
		}
		else if (metin2::config::test_server())
		{
			pkVictim->chat_packet(CHAT_TYPE_PARTY, "%s %s(%d%%) FAIL", pkAttacker->name(), name, pkAttacker->point(att_point));
		}
	}
}

inline void skill_attack_affect(character * pkVictim,
		int success_pct,
		uint32_t immune_flag,
		uint32_t affect_idx,
		uint8_t affect_point,
		int32_t affect_amount,
		uint32_t affect_flag,
		int time,
		const char* name)
{
	if (success_pct && !pkVictim->is_affect_flag(affect_flag))
	{
		if (number(1, 1000) <= success_pct && !pkVictim->is_immune(immune_flag))
		{
			pkVictim->add_affect(affect_idx, affect_point, affect_amount, affect_flag, time, 0, true);

			// SKILL_ATTACK_NO_LOG_TARGET_NAME_FIX
			if (metin2::config::test_server())
				pkVictim->chat_packet(CHAT_TYPE_PARTY, 
						"%s(%d%%) -> %s SUCCESS", name, success_pct, name);
			// END_OF_SKILL_ATTACK_LOG_NO_TARGET_NAME_FIX
		}
		else if (metin2::config::test_server())
		{
			// SKILL_ATTACK_NO_LOG_TARGET_NAME_FIX
			pkVictim->chat_packet(CHAT_TYPE_PARTY, "%s(%d%%) -> %s FAIL", name, success_pct, name);
			// END_OF_SKILL_ATTACK_LOG_NO_TARGET_NAME_FIX
		}
	}
} 

uint32_t	get_attack_speed(character * ch);
void	set_attack_time(character * ch, character * victim, uint32_t current_time);
void	set_attacked_time(character * ch, character * victim, uint32_t current_time);
bool	is_speed_hack(character * ch, character * victim, uint32_t current_time);
}
