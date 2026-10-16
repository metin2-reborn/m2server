
#include <metin2/game/events/blue_dragon.h>

#include <metin2/game/app/config.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/motion.h>
#include <metin2/game/events/blue_dragon_binder.h>
#include <metin2/game/events/blue_dragon_skill.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/core/utils.h>
#include <metin2/log/log.h>

#include <cstddef>
#include <cstdint>
#include <ctime>

namespace metin2::game::inline events {

    auto use_blue_dragon_skill(character *ch, unsigned int idx) -> time_t {
        sectree_map *sec_map = sectree_manager::instance().map(ch->map_index());

        if (nullptr == sec_map)
            return 0;

        int next_using_time = 0;

        switch (idx) {
        case 0:
            {
                metin2::log::debug("Blue dragon: using breath");

                blue_dragon_skill_breath(ch, sec_map);

                next_using_time = number(blue_dragon_get_skill_factor(3, "Skill0", "period", "min"), blue_dragon_get_skill_factor(3, "Skill0", "period", "max"));
            }
            break;

        case 1:
            {
                metin2::log::debug("Blue dragon: using weak breath");

                blue_dragon_skill_weak_breath(ch, sec_map);

                next_using_time = number(blue_dragon_get_skill_factor(3, "Skill1", "period", "min"), blue_dragon_get_skill_factor(3, "Skill1", "period", "max"));
            }
            break;

        case 2:
            {
                metin2::log::debug("Blue dragon: using earthquake");

                character *farthest_char = blue_dragon_skill_earth_quake(ch, sec_map);

                next_using_time = number(blue_dragon_get_skill_factor(3, "Skill2", "period", "min"), blue_dragon_get_skill_factor(3, "Skill2", "period", "max"));

                if (nullptr != farthest_char) {
                    ch->begin_fight(farthest_char);
                }
            }
            break;

        default:
            metin2::log::error("Blue dragon: unknown skill index {:d}", idx);
            return 0;
        }

        int add_pct = blue_dragon_get_range_factor("hp_period", ch->hp_pct());

        next_using_time += (next_using_time * add_pct) / 100;

        return next_using_time;
    }

    auto blue_dragon_state_battle(character *ch) -> int {
        if (ch->hp_pct() > 98)
            return passes_per_sec(1);

        const int skill_count = 3;
        int skill_priority[skill_count];
        static time_t time_skill_can_use_time[skill_count];

        if (ch->hp_pct() > 76) {
            skill_priority[0] = 1;
            skill_priority[1] = 0;
            skill_priority[2] = 2;
        } else if (ch->hp_pct() > 31) {
            skill_priority[0] = 0;
            skill_priority[1] = 1;
            skill_priority[2] = 2;
        } else {
            skill_priority[0] = 0;
            skill_priority[1] = 2;
            skill_priority[2] = 1;
        }

        auto time_now = static_cast<time_t>(get_dword_time());

        for (int i = 0; i < skill_count; ++i) {
            const int skill_index = skill_priority[i];

            if (time_skill_can_use_time[skill_index] < time_now) {
                int skill_using_duration =
                    static_cast<int>(motion_manager::instance().motion_duration(ch->race_num(), make_motion_key(MOTION_MODE_GENERAL, MOTION_SPECIAL_1 + skill_index)));

                time_skill_can_use_time[skill_index] = time_now + (use_blue_dragon_skill(ch, skill_index) * 1000) + skill_using_duration + 3000;

                ch->send_move_packet(FUNC_MOB_SKILL, skill_index, ch->x(), ch->y(), 0, time_now);

                return 0 == skill_using_duration ? passes_per_sec(1) : passes_per_sec(skill_using_duration);
            }
        }

        return passes_per_sec(1);
    }

    auto blue_dragon_damage(character *me, character *attacker, int dam) -> int {
        if (nullptr == me || nullptr == attacker)
            return dam;

        if (true == attacker->is_monster() && 2493 == attacker->get_mob_table().vnum) {
            for (int i = 1; i <= 4; ++i) {
                if (ATK_BONUS == blue_dragon_get_index_factor("DragonStone", i, "effect_type")) {
                    uint32_t dragon_stone_id = blue_dragon_get_index_factor("DragonStone", i, "vnum");
                    size_t val = blue_dragon_get_index_factor("DragonStone", i, "val");
                    size_t cnt = sectree_manager::instance().monster_count_in_map(attacker->map_index(), dragon_stone_id);

                    dam += (dam * (val * cnt)) / 100;

                    break;
                }
            }
        }

        if (true == me->is_monster() && 2493 == me->get_mob_table().vnum) {
            for (int i = 1; i <= 4; ++i) {
                if (DEF_BONUS == blue_dragon_get_index_factor("DragonStone", i, "effect_type")) {
                    uint32_t dragon_stone_id = blue_dragon_get_index_factor("DragonStone", i, "vnum");
                    size_t val = blue_dragon_get_index_factor("DragonStone", i, "val");
                    size_t cnt = sectree_manager::instance().monster_count_in_map(me->map_index(), dragon_stone_id);

                    dam -= (dam * (val * cnt)) / 100;

                    if (dam <= 0)
                        dam = 1;

                    break;
                }
            }
        }

        if (true == me->is_stone() && 0 != attacker->get_mount_vnum()) {
            for (int i = 1; i <= 4; ++i) {
                if (me->get_mob_table().vnum == blue_dragon_get_index_factor("DragonStone", i, "vnum")) {
                    if (attacker->get_mount_vnum() == blue_dragon_get_index_factor("DragonStone", i, "enemy")) {
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
