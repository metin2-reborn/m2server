

#include <metin2/game/combat/skill_power.h>

#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>

#include <cstring>
#include <memory>
#include <utility>

namespace metin2::game::inline combat {

    auto table_by_skill::check() const -> bool {
        for (int job = 0; job < (JOB_MAX_NUM * 2); ++job) {
            if (!skill_power_by_level_from_type_[job]) {
                metin2::log::error("No skill power table for job group {:d}", job);
                return false;
            }
        }

        return true;
    }

    auto table_by_skill::skill_power_by_level_from_type(int job, int skillgroup, int skilllevel, bool mob) const -> int {
        if (mob) {
            return skill_power_by_level_from_type_[0][skilllevel];
        }

        if (job >= JOB_MAX_NUM || skillgroup == 0)
            return 0;

        int idx = (job * 2) + (skillgroup - 1);

        return skill_power_by_level_from_type_[idx][skilllevel];
    }

    void table_by_skill::set_skill_power_by_level_from_type(int idx, const int *table) {
        delete_skill_power_by_level_from_type(idx);

        auto ai_skill_table = std::make_unique_for_overwrite<int[]>(SKILL_MAX_LEVEL + 1);

        memcpy(ai_skill_table.get(), table, sizeof(int) * (SKILL_MAX_LEVEL + 1));
        skill_power_by_level_from_type_[idx] = std::move(ai_skill_table);
    }

    void table_by_skill::delete_skill_power_by_level_from_type(int idx) {
        skill_power_by_level_from_type_[idx].reset();
    }
}
