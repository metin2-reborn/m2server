#pragma once

#include <metin2/proto/tables.h>

#include <cstdint>

namespace metin2::db::inline items {

    // Game servers get whole ranges of 2^32 ids, numbered from 1 up; ids below 2^32 (range 0) are the db's own
    // configured range and older items. A range is never handed out twice: the next number is kept in
    // item_id_state, so a restart of the db or of a game server cannot hand out ids that are already in use.
    class item_id_range_manager {
      public:
        static constexpr uint64_t RANGE_SIZE = uint64_t{1} << 32;

        auto build() -> bool;
        auto build_range(uint64_t min_value, uint64_t max_value, item_id_range_table &range) -> bool;

        auto range() -> item_id_range_table;

      private:
        static constexpr uint64_t minimum_remain_count_ = 10000;

        uint64_t next_range_ = 1;
    };

    inline auto get_item_id_range_manager() -> item_id_range_manager & {
        static item_id_range_manager instance;
        return instance;
    }
}
