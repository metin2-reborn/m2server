
#pragma once
#include <metin2/proto/character_types.h>

#include <memory>

namespace metin2::game::inline combat {

    class table_by_skill {
      public:
        table_by_skill() = default;
        ~table_by_skill() = default;

        auto check() const -> bool;

        void delete_all();

        auto skill_power_by_level_from_type(int job, int skillgroup, int skilllevel, bool mob) const -> int;
        void set_skill_power_by_level_from_type(int idx, const int *table);
        void delete_skill_power_by_level_from_type(int idx);

        void delete_skill_damage_by_level_table();

      private:
        std::unique_ptr<int[]> skill_power_by_level_from_type_[JOB_MAX_NUM * 2];
        std::unique_ptr<int[]> skill_damage_by_level_;
    };

    inline auto get_table_by_skill() -> table_by_skill & {
        static table_by_skill instance;
        return instance;
    }
}
