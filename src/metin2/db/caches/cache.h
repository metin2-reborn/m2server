#pragma once

#include <metin2/common/cache.h>
#include <metin2/proto/tables.h>

#include <cstdint>

namespace metin2::db::inline caches {

    class item_cache : public cache<player_item> {
      public:
        item_cache();

        ~item_cache() override;

        void remove();
        void on_flush() override;
    };

    class player_table_cache : public cache<player_table> {
      public:
        player_table_cache();

        ~player_table_cache() override;

        void on_flush() override;

        auto last_update_time() -> uint32_t { return last_update_time_; }
    };

    class item_price_list_table_cache : public cache<item_price_list_table> {
      public:
        item_price_list_table_cache(void);

        void update_list(const item_price_list_table *update_list);

        void on_flush(void) override;

      private:
        static const int min_flush_sec_;
    };
}
