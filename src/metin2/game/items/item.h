#pragma once

#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/world/entity.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>

#include <cstdint>
#include <cstring>

namespace metin2::game::inline protocol {
    class input_db;
}

namespace metin2::game::inline items {

    class item : public entity {
      protected:
        void encode_insert_packet(entity *ent) override;
        void encode_remove_packet(entity *ent) override;

      public:
        item(uint32_t vnum);
        ~item() override;

        auto level_limit() -> int;

        auto check_item_use_level(int level) -> bool;

        auto is_pc_bang_item() -> bool;

        auto is_stackable() -> bool { return (flag() & ITEM_FLAG_STACKABLE) ? true : false; }

        void initialize();
        void destroy();

        void save();

        void set_window(uint8_t b) { window_ = b; }
        auto window() -> uint8_t { return window_; }

        void set_id(uint32_t id) { id_ = id; }
        auto id() -> uint32_t { return id_; }

        void set_proto(const item_table *table);
        auto proto() -> item_table const * { return proto_; }

        auto shop_buy_price() -> int;
        auto name() -> const char * { return proto_ ? proto_->locale_name : nullptr; }
        auto base_name() -> const char * { return proto_ ? proto_->name : nullptr; }
        auto size() -> uint8_t { return proto_ ? proto_->size : 0; }

        void set_flag(int32_t flag) { flag_ = flag; }
        auto flag() -> int32_t { return flag_; }

        auto wear_flag() -> uint32_t { return proto_ ? proto_->wear_flags : 0; }
        auto anti_flag() -> uint32_t { return proto_ ? proto_->anti_flags : 0; }
        auto immune_flag() -> uint32_t { return proto_ ? proto_->immune_flag : 0; }

        void set_vid(uint32_t vid) { vid_ = vid; }
        auto get_vid() -> uint32_t { return vid_; }

        auto set_count(uint32_t count) -> bool;
        auto count() -> uint32_t;

        // vnum() can be masked (ori_to_new.txt); original_vnum() is the real one, for the client,
        // logs and saving.
        auto vnum() const -> uint32_t { return mask_vnum_ ? mask_vnum_ : vnum_; }
        auto original_vnum() const -> uint32_t { return vnum_; }
        auto type() const -> uint8_t { return proto_ ? proto_->type : 0; }
        auto sub_type() const -> uint8_t { return proto_ ? proto_->sub_type : 0; }
        auto limit_type(uint32_t idx) const -> uint8_t { return proto_ ? proto_->limits[idx].type : 0; }
        auto limit_value(uint32_t idx) const -> int32_t { return proto_ ? proto_->limits[idx].value : 0; }

        auto value(uint32_t idx) -> int32_t;

        void set_cell(character *ch, uint16_t pos) { owner_ = ch, cell_ = pos; }
        auto cell() -> uint16_t { return cell_; }

        auto remove_from_character() -> item *;
        auto add_to_character(character *ch, item_pos cell_value) -> bool;
        auto owner() -> character * { return owner_; }

        auto remove_from_ground() -> item *;
        auto add_to_ground(int32_t map_index, const pixel_position &pos, bool skip_owner_check = false) -> bool;

        auto find_equip_cell(character *ch, int candidate_cell = -1) -> int;
        auto is_equipped() const -> bool { return equipped_; }
        auto equip_to(character *ch, uint8_t wear_cell) -> bool;
        auto is_equipable() const -> bool;

        auto can_used_by(character *ch) -> bool;

        auto distance_valid(character *ch) -> bool;

        void update_packet();

        void set_exchanging(bool on = true);
        auto is_exchanging() -> bool { return exchanging_; }

        void modify_points(bool add);

        auto sockets() -> const int32_t * { return &sockets_[0]; }
        auto get_socket(int i) -> int32_t { return sockets_[i]; }

        void set_sockets(const int32_t *al);
        void set_socket(int i, int32_t v, bool log = true);

        auto socket_count() -> int;
        auto add_socket() -> bool;

        auto attributes() -> const player_item_attribute * { return attr_; }
        auto get_attribute(int i) -> const player_item_attribute & { return attr_[i]; }

        auto attribute_type(int i) -> uint8_t { return attr_[i].type; }
        auto attribute_value(int i) -> short { return attr_[i].s_value; }

        void set_attributes(const player_item_attribute *attribute);

        auto find_attribute(uint8_t type) -> int;
        auto remove_attribute_type(uint8_t type) -> bool;

        auto has_attr(uint8_t apply) -> bool;
        auto has_rare_attr(uint8_t apply) -> bool;

        void set_destroy_event(event_ptr event);
        void start_destroy_event(int sec = 300);

        auto refined_vnum() -> uint32_t { return proto_ ? proto_->refined_vnum : 0; }
        auto refine_from_vnum() -> uint32_t;
        auto refine_level() -> int;

        void set_skip_save(bool b) { skip_save_ = b; }
        auto skip_save() -> bool { return skip_save_; }

        auto is_ownership(character *ch) -> bool;
        void set_ownership(character *ch, int sec = 10);
        void set_ownership_event(event_ptr event);

        auto last_owner_pid() -> uint32_t { return last_owner_pid_; }

        auto attribute_set_index() -> int;
        void alter_to_magic_item();
        void alter_to_socket_item(int socket_count);

        auto refine_set() -> uint16_t { return proto_ ? proto_->refine_set : 0; }

        void start_unique_expire_event();
        void set_unique_expire_event(event_ptr event);

        void start_timer_based_on_wear_expire_event();
        void set_timer_based_on_wear_expire_event(event_ptr event);

        void start_real_time_expire_event();
        auto is_real_time_item() -> bool;

        void stop_unique_expire_event();
        void stop_timer_based_on_wear_expire_event();
        void stop_accessory_socket_expire_event();

        auto duration() -> int;

        auto attribute_count() -> int;
        void clear_attribute();
        void change_attribute(const int *change_prob = nullptr);
        void add_attribute();
        void add_attribute(uint8_t apply, short s_value);

        void apply_addon(int addon_type);

        auto special_group() const -> int;
        auto is_same_special_group(item *const item) const -> bool;

        auto is_accessory_for_socket() -> bool;

        auto accessory_socket_grade() -> int;
        auto accessory_socket_max_grade() -> int;
        auto accessory_socket_down_grade_time() -> int;

        void set_accessory_socket_grade(int grade);
        void set_accessory_socket_max_grade(int max_grade);
        void set_accessory_socket_down_grade_time(uint32_t time);

        void accessory_socket_degrade();

        void start_accessory_socket_expire_event();
        void set_accessory_socket_expire_event(event_ptr event);

        auto can_put_into(item *item) -> bool;

        void copy_attribute_to(item *item_value);
        void copy_socket_to(item *item_value);

        auto rare_attr_count() -> int;
        auto add_rare_attribute() -> bool;
        auto change_rare_attribute() -> bool;

        void attr_log();

        void lock(bool f) { is_locked_ = f; }
        auto is_locked() const -> bool { return is_locked_; }

      private:
        void set_attribute(int i, uint8_t type_value, short s_value_value);

      public:
        void set_force_attribute(int i, uint8_t type_value, short s_value_value);

      protected:
        auto unequip() -> bool;

        void add_attr(uint8_t apply, uint8_t level);
        void put_attribute(const int *attr_percent_table);
        void put_attribute_with_level(uint8_t level);

      protected:
        friend class protocol::input_db;
        auto on_after_created_item() -> bool;

      public:
        auto is_ride_item() -> bool;

        void clear_mount_attribute_and_affect();

        void set_mask_vnum(uint32_t vnum) { mask_vnum_ = vnum; }

        auto is_dragon_soul() -> bool;
        auto give_more_time_per(float percent) -> int;
        auto give_more_time_fix(uint32_t time) -> int;

      private:
        item_table const *proto_;

        uint32_t vnum_;
        character *owner_;

        uint8_t window_;
        uint32_t id_;
        bool equipped_;
        uint32_t vid_;
        uint16_t cell_;
        uint32_t count_;
        int32_t flag_;
        uint32_t last_owner_pid_;

        bool exchanging_;

        int32_t sockets_[ITEM_SOCKET_MAX_NUM];
        player_item_attribute attr_[ITEM_ATTRIBUTE_MAX_NUM];

        event_ptr destroy_event_;
        event_ptr unique_expire_event_;
        event_ptr timer_based_on_wear_expire_event_;
        event_ptr real_time_expire_event_;
        event_ptr accessory_socket_expire_event_;
        event_ptr ownership_event_;

        uint32_t ownership_pid_;

        bool skip_save_;

        bool is_locked_;

        uint32_t mask_vnum_;
        uint32_t sig_vnum_;

      public:
        void set_sig_vnum(uint32_t sig) {
            sig_vnum_ = sig;
        }
        auto sig_vnum() const -> uint32_t {
            return sig_vnum_;
        }
    };

    EVENTINFO(item_event_info) {
        game::items::item *item;
        char owner_name[CHARACTER_NAME_MAX_LEN];

        item_event_info()
            : item(nullptr) {
            ::memset(owner_name, 0, CHARACTER_NAME_MAX_LEN);
        }
    };

    EVENTINFO(item_vid_event_info) {
        uint32_t item_vid;

        item_vid_event_info()
            : item_vid(0) {
        }
    };
}
