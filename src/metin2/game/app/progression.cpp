#include <metin2/game/app/progression.h>

#include <metin2/proto/mob_types.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace metin2::game::inline app {

    namespace {

        // Same-level pawn exp; a level takes kills_per_level(level) of those.
        auto reference_kill_exp(int level) -> double {
            return std::round(15.0 + 1.2 * level * level);
        }

        // The tiers: 1-30 easy, then a step up at 31, 51, 76 and a steep last tier from 91.
        auto kills_per_level(int level) -> double {
            if (level <= 30)
                return 15.0 + 1.5 * (level - 1);
            if (level <= 50)
                return 80.0 + 4.0 * (level - 31);
            if (level <= 75)
                return 200.0 + 8.0 * (level - 51);
            if (level <= 90)
                return 500.0 + 20.0 * (level - 76);
            return 1500.0 * std::pow(1.3, level - 91);
        }

        auto round_to_three_digits(double value) -> uint32_t {
            const double scale = std::pow(10.0, std::max(0, static_cast<int>(std::log10(value)) - 2));
            return static_cast<uint32_t>(std::round(value / scale) * scale);
        }

        const std::array<uint32_t, PROGRESSION_MAX_LEVEL> level_exps = [] {
            std::array<uint32_t, PROGRESSION_MAX_LEVEL> table{};
            for (int level = 1; level < PROGRESSION_MAX_LEVEL; ++level)
                table[level] = round_to_three_digits(kills_per_level(level) * reference_kill_exp(level));
            return table;
        }();

        // Tenths of a pawn kill, by mob_rank.
        constexpr std::array<int, MOB_RANK_MAX_NUM> rank_exp_tenths = {10, 15, 25, 40, 400, 1200};

    }

    auto level_exp(int level) -> uint32_t {
        return level_exps[std::clamp(level, 1, PROGRESSION_MAX_LEVEL - 1)];
    }

    auto kill_exp(int level, int rank) -> uint32_t {
        const int tenths = rank_exp_tenths[std::clamp(rank, 0, MOB_RANK_MAX_NUM - 1)];
        return static_cast<uint32_t>(std::lround(reference_kill_exp(std::max(level, 1)) * tenths / 10.0));
    }

    auto level_damage_percent(int attacker_level, int victim_level) -> int {
        return 100 + std::clamp(3 * (attacker_level - victim_level), -30, 15);
    }

    auto refine_chance(int target_level, int final_prob) -> int {
        if (target_level <= 3)
            return 100;
        if (target_level >= 9)
            return final_prob;
        const double fall = std::pow((target_level - 3) / 6.0, 1.5);
        return static_cast<int>(std::lround((100.0 - (100 - final_prob) * fall) / 5.0)) * 5;
    }

}
