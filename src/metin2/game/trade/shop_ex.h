#pragma once
#include <metin2/game/app/typedef.h>
#include <metin2/game/trade/shop.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace metin2::game::inline data {
    class group_node;
}

namespace metin2::game::inline trade {

    struct shop_table_ex : shop_table {
        std::string name;
        shop_coin_type coin_type;
    };

    // Tabs are sent to the client in blocks of 45 positions.
    class shop_ex : public shop {
      public:
        auto create(uint32_t vnum, uint32_t npc_vnum) -> bool;
        auto add_shop_table(shop_table_ex &shop_table) -> bool;

        auto add_guest(character *ch, uint32_t owner_vid, bool other_empire) -> bool override;
        void set_pc_shop(character *ch) override { return; }
        auto is_pc_shop() -> bool override { return false; }
        auto buy(character *ch, uint8_t pos) -> int override;
        auto is_selling_item(uint32_t item_id) -> bool override { return false; }

        auto tab_count() -> size_t { return vec_shop_tabs_.size(); }

      private:
        std::vector<shop_table_ex> vec_shop_tabs_;
    };
}
