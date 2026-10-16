#pragma once
#include <metin2/game/app/typedef.h>

#include <metin2/common/grid.h>
#include <metin2/proto/tables.h>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace metin2::game::inline trade {

    enum {
        SHOP_MAX_DISTANCE = 1000
    };

    class shop {
      public:
        struct shop_item {
            uint32_t vnum;
            int32_t price;
            uint8_t count;

            game::items::item *item;
            int itemid;

            shop_item() {
                vnum = 0;
                price = 0;
                count = 0;
                itemid = 0;
                item = nullptr;
            }
        };

        shop();
        virtual ~shop();

        auto create(uint32_t vnum_value, uint32_t npc_vnum, shop_item_table *table) -> bool;
        void set_shop_items(shop_item_table *table_value, uint8_t item_count);

        virtual void set_pc_shop(character *ch);
        virtual auto is_pc_shop() -> bool { return pc_ ? true : false; }

        virtual auto add_guest(character *ch, uint32_t owner_vid, bool other_empire) -> bool;
        void remove_guest(character *ch);

        virtual auto buy(character *ch, uint8_t pos) -> int;

        void broadcast_update_item(uint8_t pos);

        auto number_by_vnum(uint32_t vnum_value) -> int;

        virtual auto is_selling_item(uint32_t item_id) -> bool;

        auto vnum() -> uint32_t { return vnum_; }

      protected:
        void broadcast(const void *data, int bytes);

      protected:
        uint32_t vnum_;

        std::unique_ptr<grid> grid_;

        using guest_map_type = std::unordered_map<character *, bool>;
        guest_map_type map_guest_;
        std::vector<shop_item> item_vector_;

        character *pc_;
    };
}
