#pragma once

#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>
#include <metin2/proto/length.h>

#include <cstdint>
#include <memory>

namespace metin2::game::inline characters {
    class character;
}

namespace metin2::game::inline items {

    class item;

    class dragon_soul_table;

    class ds_manager : public singleton<ds_manager> {
      public:
        ds_manager();
        ~ds_manager() override;
        auto read_dragon_soul_table_file(const char *file_name) -> bool;

        void dragon_soul_info(uint32_t vnum, uint8_t &type, uint8_t &grade, uint8_t &step, uint8_t &strength) const;
        auto base_position(item *const item_value) const -> uint16_t;
        auto is_valid_cell_for_this_item(item *const item_value, const item_pos &cell_value) const -> bool;
        auto duration(item *const item_value) const -> int;

        auto extract_dragon_heart(character *ch, item *item_value, item *extractor = nullptr) -> bool;

        auto pull_out(character *ch, item_pos dest_cell, item *&item_value, item *extractor = nullptr) -> bool;

        auto do_refine_grade(character *ch, item_pos (&item_poses)[DRAGON_SOUL_REFINE_GRID_SIZE]) -> bool;
        auto do_refine_step(character *ch, item_pos (&item_poses)[DRAGON_SOUL_REFINE_GRID_SIZE]) -> bool;
        auto do_refine_strength(character *ch, item_pos (&item_poses)[DRAGON_SOUL_REFINE_GRID_SIZE]) -> bool;

        auto dragon_soul_item_initialize(item *item_value) -> bool;

        auto is_time_left_dragon_soul(item *item_value) const -> bool;
        auto left_time(item *item_value) const -> int;
        auto activate_dragon_soul(item *item_value) -> bool;
        auto deactivate_dragon_soul(item *item_value, bool skip_refresh_owner_active_state = false) -> bool;
        auto is_active_dragon_soul(item *item_value) const -> bool;

      private:
        void send_refine_result_packet(character *ch, uint8_t sub_header, const item_pos &pos);

        void refresh_dragon_soul_state(character *ch);

        auto make_dragon_soul_vnum(uint8_t type, uint8_t grade, uint8_t step, uint8_t refine) -> uint32_t;
        auto put_attributes(item *ds) -> bool;
        auto refresh_item_attributes(item *ds) -> bool;

        std::unique_ptr<dragon_soul_table> table_;
    };
}
