#pragma once
#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>
#include <metin2/proto/tables.h>

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace metin2::game::inline trade {

    class shop;

    class shop_manager : public singleton<shop_manager> {
      public:
        using shop_map = std::map<uint32_t, shop *>;
        using owned_shop_map = std::map<uint32_t, std::unique_ptr<shop>>;

      public:
        shop_manager();
        ~shop_manager() override;

        auto initialize(shop_table *table, int size) -> bool;
        void destroy();

        auto get(uint32_t vnum) -> shop *;
        auto by_npc_vnum(uint32_t vnum) -> shop *;

        auto start_shopping(character *chr, character *chr_shop_keeper, int shop_vnum = 0) -> bool;
        void stop_shopping(character *ch);

        void buy(character *ch, uint8_t pos);
        void sell(character *ch, uint8_t cell, uint8_t count_value = 0);

        auto create_pc_shop(character *ch, shop_item_table *table, uint8_t item_count) -> shop *;
        auto find_pc_shop(uint32_t vid) -> shop *;
        void destroy_pc_shop(character *ch);

      private:
        std::vector<std::unique_ptr<shop>> shops_;
        shop_map map_shop_;
        shop_map map_shop_by_npc_vnum_;
        owned_shop_map map_shop_by_pc_;

        auto read_shop_table_ex(const char *file_name) -> bool;
    };
}
