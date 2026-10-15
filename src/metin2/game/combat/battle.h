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

extern int	calc_att_bonus(character * attacker, character * victim, int atk);
extern int	calc_battle_damage(int dam, int attacker_lev, int victim_lev);
extern int	calc_melee_damage(character * attacker, character * victim, bool ignore_defense = false, bool ignore_target_rating = false);
extern int	calc_magic_damage(character * attacker, character * victim);
extern int	calc_arrow_damage(character * attacker, character * victim, item * bow, item * arrow, bool ignore_defense = false);
extern float	calc_attack_rating(character * attacker, character * victim, bool ignore_target_rating = false);

extern bool	battle_is_attackable(character * ch, character * victim);
extern int	battle_melee_attack(character * ch, character * victim);
extern void	battle_end(character * ch);

extern void	normal_attack_affect(character * attacker, character * victim);

// Special attack
inline void attack_affect(character * attacker,
		character * victim,
		uint8_t att_point,
		uint32_t immune_flag,
		uint32_t affect_idx,
		uint8_t affect_point,
		int32_t affect_amount,
		uint32_t affect_flag,
		int time,
		const char* name)
{
	if (attacker->point(att_point) && !victim->is_affect_flag(affect_flag))
	{
		if (number(1, 100) <= attacker->point(att_point) && !victim->is_immune(immune_flag))
		{
			victim->add_affect(affect_idx, affect_point, affect_amount, affect_flag, time, 0, true);

			if (metin2::config::test_server())
			{
				victim->chat_packet(CHAT_TYPE_PARTY, "%s %s(%d%%) SUCCESS", attacker->name(), name, attacker->point(att_point));
			}
		}
		else if (metin2::config::test_server())
		{
			victim->chat_packet(CHAT_TYPE_PARTY, "%s %s(%d%%) FAIL", attacker->name(), name, attacker->point(att_point));
		}
	}
}

inline void skill_attack_affect(character * victim,
		int success_pct,
		uint32_t immune_flag,
		uint32_t affect_idx,
		uint8_t affect_point,
		int32_t affect_amount,
		uint32_t affect_flag,
		int time,
		const char* name)
{
	if (success_pct && !victim->is_affect_flag(affect_flag))
	{
		if (number(1, 1000) <= success_pct && !victim->is_immune(immune_flag))
		{
			victim->add_affect(affect_idx, affect_point, affect_amount, affect_flag, time, 0, true);

			// SKILL_ATTACK_NO_LOG_TARGET_NAME_FIX
			if (metin2::config::test_server())
				victim->chat_packet(CHAT_TYPE_PARTY, 
						"%s(%d%%) -> %s SUCCESS", name, success_pct, name);
			// END_OF_SKILL_ATTACK_LOG_NO_TARGET_NAME_FIX
		}
		else if (metin2::config::test_server())
		{
			// SKILL_ATTACK_NO_LOG_TARGET_NAME_FIX
			victim->chat_packet(CHAT_TYPE_PARTY, "%s(%d%%) -> %s FAIL", name, success_pct, name);
			// END_OF_SKILL_ATTACK_LOG_NO_TARGET_NAME_FIX
		}
	}
} 

uint32_t	get_attack_speed(character * ch);
void	set_attack_time(character * ch, character * victim, uint32_t current_time);
void	set_attacked_time(character * ch, character * victim, uint32_t current_time);
bool	is_speed_hack(character * ch, character * victim, uint32_t current_time);
}
