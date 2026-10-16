#pragma once
#include <metin2/game/app/typedef.h>

#include <metin2/common/grid.h>
#include <metin2/proto/length.h>

#include <cstdint>
#include <memory>

namespace metin2::game::inline trade {

    enum exchange_values {
        EXCHANGE_ITEM_MAX_NUM = 12,
        EXCHANGE_MAX_DISTANCE = 1000
    };

    class exchange {
      public:
        exchange(character *owner);
        ~exchange();

        auto accept(bool accept = true) -> bool;
        void cancel();

        auto add_gold(int32_t gold) -> bool;
        auto add_item(item_pos item_pos, uint8_t display_pos) -> bool;
        auto remove_item(uint8_t pos) -> bool;

        auto owner() -> character * { return owner_; }
        auto company() -> exchange * { return company_; }

        auto accept_status() -> bool { return accept_; }

        void set_company(exchange *exchange_value) { company_ = exchange_value; }

      private:
        auto done() -> bool;
        auto check(int *item_count_value) -> bool;
        auto check_space() -> bool;
        auto validate() -> bool;

      private:
        exchange *company_;

        character *owner_;

        item_pos item_pos_[EXCHANGE_ITEM_MAX_NUM];
        item *ap_items_[EXCHANGE_ITEM_MAX_NUM];
        uint8_t item_display_pos_[EXCHANGE_ITEM_MAX_NUM];

        bool accept_;
        int32_t gold_;

        std::unique_ptr<grid> grid_;
    };
}
