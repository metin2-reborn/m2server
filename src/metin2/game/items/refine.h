#pragma once

#include <metin2/proto/tables.h>

#include <cstdint>
#include <map>

namespace metin2::game::inline items {

    enum {
        BLACKSMITH_MOB = 20016,
        ALCHEMIST_MOB = 20001,

        BLACKSMITH_WEAPON_MOB = 20044,
        BLACKSMITH_ARMOR_MOB = 20045,
        BLACKSMITH_ACCESSORY_MOB = 20046,

        DEVILTOWER_BLACKSMITH_WEAPON_MOB = 20074,
        DEVILTOWER_BLACKSMITH_ARMOR_MOB = 20075,
        DEVILTOWER_BLACKSMITH_ACCESSORY_MOB = 20076,

        BLACKSMITH2_MOB = 20091,
    };

    class refine_manager {
        using refine_recipe_map = std::map<uint32_t, refine_table>;

      public:
        refine_manager();
        ~refine_manager();

        auto initialize(refine_table *table, int size) -> bool;
        auto refine_recipe(uint32_t vnum) -> const refine_table *;

      private:
        refine_recipe_map map_refine_recipe_;
    };

    inline auto get_refine_manager() -> refine_manager & {
        static refine_manager instance;
        return instance;
    }
}
