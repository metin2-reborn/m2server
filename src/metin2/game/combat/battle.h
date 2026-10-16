#pragma once

#include <metin2/config/config.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>

#include <metin2/core/utils.h>
#include <metin2/proto/length.h>

#include <cstdint>

namespace metin2::game::inline combat {

    enum battle_types {
        BATTLE_NONE,
        BATTLE_DAMAGE,
        BATTLE_DEFENSE,
        BATTLE_DEAD
    };

    extern auto calc_att_bonus(character *attacker, character *victim, int atk) -> int;
    extern auto calc_battle_damage(int dam, int attacker_lev, int victim_lev) -> int;
    extern auto calc_melee_damage(character *attacker, character *victim, bool ignore_defense = false, bool ignore_target_rating = false) -> int;
    extern auto calc_magic_damage(character *attacker, character *victim) -> int;
    extern auto calc_arrow_damage(character *attacker, character *victim, item *bow, item *arrow, bool ignore_defense = false) -> int;
    extern auto calc_attack_rating(character *attacker, character *victim, bool ignore_target_rating = false) -> float;

    extern auto battle_is_attackable(character *ch, character *victim) -> bool;
    extern auto battle_melee_attack(character *ch, character *victim) -> int;
    extern void battle_end(character *ch);

    extern void normal_attack_affect(character *attacker, character *victim);

    inline void attack_affect(character *attacker,
        character *victim,
        uint8_t att_point,
        uint32_t immune_flag,
        uint32_t affect_idx,
        uint8_t affect_point,
        int32_t affect_amount,
        uint32_t affect_flag,
        int time,
        const char *name) {
        if (attacker->point(att_point) && !victim->is_affect_flag(affect_flag)) {
            if (prob(attacker->point(att_point)) && !victim->is_immune(immune_flag)) {
                victim->add_affect(affect_idx, affect_point, affect_amount, affect_flag, time, 0, true);

                if (metin2::config::test_server()) {
                    victim->chat_packet(CHAT_TYPE_PARTY, "%s %s(%d%%) SUCCESS", attacker->name(), name, attacker->point(att_point));
                }
            } else if (metin2::config::test_server()) {
                victim->chat_packet(CHAT_TYPE_PARTY, "%s %s(%d%%) FAIL", attacker->name(), name, attacker->point(att_point));
            }
        }
    }

    inline void skill_attack_affect(character *victim,
        int success_pct,
        uint32_t immune_flag,
        uint32_t affect_idx,
        uint8_t affect_point,
        int32_t affect_amount,
        uint32_t affect_flag,
        int time,
        const char *name) {
        if (success_pct && !victim->is_affect_flag(affect_flag)) {
            if (number(1, 1000) <= success_pct && !victim->is_immune(immune_flag)) {
                victim->add_affect(affect_idx, affect_point, affect_amount, affect_flag, time, 0, true);

                if (metin2::config::test_server())
                    victim->chat_packet(CHAT_TYPE_PARTY,
                        "%s(%d%%) -> %s SUCCESS", name, success_pct, name);
            } else if (metin2::config::test_server()) {
                victim->chat_packet(CHAT_TYPE_PARTY, "%s(%d%%) -> %s FAIL", name, success_pct, name);
            }
        }
    }

    auto get_attack_speed(character *ch) -> uint32_t;
    void set_attack_time(character *ch, character *victim, uint32_t current_time);
    void set_attacked_time(character *ch, character *victim, uint32_t current_time);
    auto is_speed_hack(character *ch, character *victim, uint32_t current_time) -> bool;
}
