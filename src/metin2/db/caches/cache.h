#pragma once

#include <metin2/common/cache.h>
#include <metin2/proto/tables.h>

#include <chrono>
#include <cstdint>

namespace metin2::db::inline caches {

    class item_cache : public cache<player_item> {
      public:
        item_cache();

        void remove();
        void on_flush() override;
    };

    class player_table_cache : public cache<player_table> {
      public:
        player_table_cache();

        void on_flush() override;

        auto seconds_since_update() const -> uint32_t {
            return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(clock::now() - last_update_time_).count());
        }
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
