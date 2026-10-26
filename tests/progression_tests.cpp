#include "check.h"

#include <metin2/game/app/progression.h>
#include <metin2/proto/mob_types.h>

#include <cstdint>
#include <limits>

using namespace metin2;
using namespace metin2::game;

TEST(level_exp_matches_the_tier_design) {
    CHECK_EQ(level_exp(1), 240u);
    CHECK_EQ(level_exp(10), 3850u);
    CHECK_EQ(level_exp(30), 64100u);
    CHECK_EQ(level_exp(31), 93400u);
    CHECK_EQ(level_exp(50), 470000u);
    CHECK_EQ(level_exp(51), 627000u);
    CHECK_EQ(level_exp(75), 2650000u);
    CHECK_EQ(level_exp(76), 3470000u);
    CHECK_EQ(level_exp(90), 7590000u);
    CHECK_EQ(level_exp(91), 14900000u);
    CHECK_EQ(level_exp(99), 144000000u);
}

TEST(level_exp_grows_and_fits_the_points_packet) {
    for (int level = 2; level < PROGRESSION_MAX_LEVEL; ++level)
        CHECK(level_exp(level) > level_exp(level - 1));
    CHECK(level_exp(PROGRESSION_MAX_LEVEL - 1) < static_cast<uint32_t>(std::numeric_limits<int32_t>::max()));
    CHECK_EQ(level_exp(105), level_exp(99));
    CHECK_EQ(level_exp(0), level_exp(1));
}

TEST(level_exp_steps_up_at_each_tier) {
    for (int first : {31, 51, 76, 91}) {
        const double step = static_cast<double>(level_exp(first)) / level_exp(first - 1);
        const double before = static_cast<double>(level_exp(first - 1)) / level_exp(first - 2);
        CHECK(step > before);
    }
}

TEST(kill_exp_scales_with_rank) {
    CHECK_EQ(kill_exp(1, MOB_RANK_PAWN), 16u);
    CHECK_EQ(kill_exp(10, MOB_RANK_PAWN), 135u);
    CHECK_EQ(kill_exp(10, MOB_RANK_S_PAWN), 203u);
    CHECK_EQ(kill_exp(10, MOB_RANK_KNIGHT), 338u);
    CHECK_EQ(kill_exp(10, MOB_RANK_S_KNIGHT), 540u);
    CHECK_EQ(kill_exp(10, MOB_RANK_BOSS), 5400u);
    CHECK_EQ(kill_exp(10, MOB_RANK_KING), 16200u);
}

TEST(level_damage_percent_is_capped) {
    CHECK_EQ(level_damage_percent(50, 50), 100);
    CHECK_EQ(level_damage_percent(52, 50), 106);
    CHECK_EQ(level_damage_percent(80, 50), 115);
    CHECK_EQ(level_damage_percent(48, 50), 94);
    CHECK_EQ(level_damage_percent(1, 50), 70);
}

TEST(refine_chance_follows_the_curve) {
    const int to_30[] = {100, 100, 100, 95, 85, 75, 60, 45, 30};
    const int to_10[] = {100, 100, 100, 95, 85, 70, 50, 30, 10};
    for (int level = 1; level <= 9; ++level) {
        CHECK_EQ(refine_chance(level, 30), to_30[level - 1]);
        CHECK_EQ(refine_chance(level, 10), to_10[level - 1]);
    }
}
