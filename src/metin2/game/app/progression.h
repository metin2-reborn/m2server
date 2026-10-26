#pragma once

#include <cstdint>

// Character progression curves: exp per level, exp per kill, level-difference damage and
// refine chances. Pure functions so the curves can be unit tested.
namespace metin2::game::inline app {

    inline constexpr int PROGRESSION_MAX_LEVEL = 100;

    // Exp needed at `level` to reach the next one; levels past the cap get the last step's value.
    auto level_exp(int level) -> uint32_t;

    // Exp a monster of `level` and `rank` (mob_rank) is worth before the level-delta and bonuses.
    auto kill_exp(int level, int rank) -> uint32_t;

    // Damage in percent when one side of a fight is a monster.
    auto level_damage_percent(int attacker_level, int victim_level) -> int;

    // Success chance in percent of refining to `target_level` (+1..+9) on a chain whose last step
    // (+8 to +9) has `final_prob`.
    auto refine_chance(int target_level, int final_prob) -> int;

}
