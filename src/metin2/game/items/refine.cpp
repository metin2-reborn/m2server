#include <metin2/game/items/refine.h>

#include <metin2/log/log.h>
#include <metin2/proto/tables.h>

#include <cstdint>
#include <utility>

namespace metin2::game::inline items {

    refine_manager::refine_manager() = default;

    refine_manager::~refine_manager() = default;

    auto refine_manager::initialize(refine_table *table, int size) -> bool {
        for (int i = 0; i < size; ++i, ++table) {
            metin2::log::debug("Refine: recipe {:d}, chance {:d}, cost {:d}", table->id, table->prob, table->cost);
            map_refine_recipe_.insert(std::make_pair(table->id, *table));
        }

        metin2::log::info("Refine: loaded {:d} recipes", map_refine_recipe_.size());
        return true;
    }

    auto refine_manager::refine_recipe(uint32_t vnum) -> const refine_table * {
        if (vnum == 0)
            return nullptr;

        auto it = map_refine_recipe_.find(vnum);
        metin2::log::debug("Refine: recipe {:d} {}", vnum, it == map_refine_recipe_.end() ? "not found" : "found");

        if (it == map_refine_recipe_.end()) {
            return nullptr;
        }

        return &it->second;
    }
}
