#pragma once

#include <metin2/game/app/typedef.h>

#include <cstdint>
#include <unordered_map>

namespace metin2::game::inline world {

    class sectree;

    class entity {
      public:
        using entity_map = std::unordered_map<entity *, int>;

      public:
        entity();
        virtual ~entity();

        virtual void encode_insert_packet(entity *entity) = 0;
        virtual void encode_remove_packet(entity *entity) = 0;

      protected:
        void initialize(int type = -1);
        void destroy();

      public:
        auto type() const -> int;
        auto is_type(int type) const -> bool;

        void view_cleanup();
        void view_insert(entity *entity, bool recursive = true);
        void view_remove(entity *entity, bool recursive = true);
        void view_reencode();

        auto x() const -> int32_t { return pos_.x; }
        auto y() const -> int32_t { return pos_.y; }
        auto z() const -> int32_t { return pos_.z; }
        auto xyz() const -> const pixel_position & { return pos_; }

        void set_xyz(int32_t x, int32_t y, int32_t z) { pos_.x = x, pos_.y = y, pos_.z = z; }

        auto get_sectree() const -> sectree * { return sectree_; }
        void set_sectree(sectree *tree) { sectree_ = tree; }

        void update_sectree();
        // What a nearby player's own update_sectree would do for this entity: it enters the view of
        // every player in range. Moving monsters call it, since a standing player never refreshes.
        void announce_to_players_around();
        void packet_around(const void *data, int bytes, entity *except = nullptr);
        void packet_view(const void *data, int bytes, entity *except = nullptr);

        void bind_desc(desc *d) { desc_ = d; }
        auto get_desc() const -> desc * { return desc_; }

        void set_map_index(int32_t l) { map_index_ = l; }
        auto map_index() const -> int32_t { return map_index_; }

        void set_observer_mode(bool flag);
        auto is_observer_mode() const -> bool { return is_observer_; }

      protected:
        bool is_observer_;
        bool observer_mode_change_;
        entity_map views_;
        int32_t map_index_;

      private:
        desc *desc_;

        int type_;
        bool is_destroyed_;

        pixel_position pos_;

        int view_age_;

        sectree *sectree_;
    };
}
