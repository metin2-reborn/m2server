#pragma once

#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/log/log.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace metin2::game::inline parties {
    class party;
}

namespace metin2::game::inline world {

    class dungeon {
        using party_map = std::unordered_map<party *, int>;
        using unique_mob_map = std::map<std::string, character *>;

      public:
        using id_type = uint32_t;

        ~dungeon();

        auto id() const -> id_type { return id_; }

        void notice(const char *msg);

        void join_party(party *party_value);
        void quit_party(party *party_value);

        void join(character *ch);

        void inc_member(character *ch);
        void dec_member(character *ch);

        void purge();
        void kill_all();

        void inc_monster() {
            monster_count_++;
            metin2::log::debug("Dungeon: monster count {:d}", monster_count_);
        }
        void dec_monster() {
            monster_count_--;
            check_eliminated();
        }
        auto count_monster() -> int { return monster_count_; }

        void inc_party_member(party *party_value, character *ch);
        void dec_party_member(party *party_value, character *ch);

        void inc_kill_count(character *killer, character *victim);
        auto kill_mob_count() -> int;
        auto kill_stone_count() -> int;
        auto is_use_potion() -> bool;
        auto is_use_revive() -> bool;
        void use_potion(character *ch);
        void use_revive(character *ch);

        auto map_index() -> int32_t { return map_index_; }

        void spawn(uint32_t vnum, const char *pos);
        auto spawn_mob(uint32_t vnum, int x, int y, int dir = 0) -> character *;
        auto spawn_mob_ac_dir(uint32_t vnum, int x, int y, int dir = 0) -> character *;
        auto spawn_group(uint32_t vnum, int32_t x, int32_t y, float radius, bool aggressive = false, int count = 1) -> character *;

        void spawn_name_mob(uint32_t vnum, int x, int y, const char *name);
        void spawn_goto_mob(int32_t from_x, int32_t from_y, int32_t to_x, int32_t to_y);

        void spawn_regen(const char *filename, bool once = true);
        void add_regen(regen *regen);
        void clear_regen();
        auto is_valid_regen(regen *regen, size_t regen_id) -> bool;

        void set_unique(const char *key, uint32_t vid);
        void spawn_move_unique(const char *key, uint32_t vnum, const char *pos_from, const char *pos_to);
        void spawn_move_group(uint32_t vnum, const char *pos_from, const char *pos_to, int count = 1);
        void spawn_unique(const char *key, uint32_t vnum, const char *pos);
        void spawn_stone_door(const char *key, const char *pos);
        void spawn_wooden_door(const char *key, const char *pos);
        void kill_unique(const std::string &key);
        void purge_unique(const std::string &key);
        auto is_unique_dead(const std::string &key) -> bool;
        auto unique_hp_perc(const std::string &key) -> float;
        auto unique_vid(const std::string &key) -> uint32_t;

        void dead_character(character *ch);

        void unique_set_max_hp(const std::string &key, int max_hp);
        void unique_set_hp(const std::string &key, int hp);
        void unique_set_def_grade(const std::string &key, int grade);

        void send_dest_position_to_party(party *party_value, int32_t x, int32_t y);

        void check_eliminated();

        void jump_all(int32_t from_map_index, int x, int y);
        void warp_all(int32_t from_map_index, int x, int y);
        void jump_party(party *party_value, int32_t from_map_index, int x, int y);

        void exit_all();
        void exit_all_to_start_position();
        void jump_to_eliminate_location();
        void set_exit_all_at_eliminate(int32_t time);
        void set_warp_at_eliminate(int32_t time, int32_t map_index, int x, int y, const char *regen_file);

        auto flag(std::string name) -> int;
        void set_flag(std::string name, int value);
        void set_warp_location(int32_t map_index, int x, int y);

        using item_group = std::vector<std::pair<uint32_t, int>>;
        void create_item_group(std::string &group_name, item_group &item_group);
        auto get_item_group(std::string &group_name) -> const item_group *;

        template <class Func>
        auto for_each_member(Func f) -> Func;

        auto is_all_pc_near_to(int x, int y, int dist) -> bool;

      protected:
        dungeon(id_type id, int32_t original_map_index, int32_t map_index);

        void initialize();

      private:
        id_type id_;
        uint32_t map_index_;

        character_set characters_;
        std::map<std::string, int> flags_;
        using item_group_map = std::map<std::string, item_group>;
        item_group_map item_groups_;
        party_map parties_;
        area_map &areas_;
        unique_mob_map unique_mobs_;

        auto find_unique(const std::string &key, bool may_be_dead) -> unique_mob_map::iterator;

        int mob_kill_;
        int stone_kill_;
        bool use_potion_;
        bool use_revive_;

        int monster_count_;

        bool exit_all_at_eliminate_;
        bool warp_at_eliminate_;

        int warp_delay_;
        int32_t warp_map_index_;
        int32_t warp_x_;
        int32_t warp_y_;
        std::string regen_file_;

        std::vector<regen *> regen_;

        event_ptr dead_event_;
        event_ptr exit_all_event_;
        event_ptr jump_to_event_;
        size_t regen_id_;

        friend class dungeon_manager;
        friend EVENTFUNC(dungeon_dead_event);
        friend EVENTFUNC(dungeon_exit_all_event);
        friend EVENTFUNC(dungeon_jump_to_event);

        party *party_;

      public:
        void set_party_null();
    };

    class dungeon_manager {
        using dungeon_map = std::map<dungeon::id_type, std::unique_ptr<dungeon>>;
        using dungeon_by_map = std::map<int32_t, dungeon *>;

      public:
        dungeon_manager();
        ~dungeon_manager();

        auto create(int32_t original_map_index) -> dungeon *;
        void destroy(dungeon::id_type dungeon_id);
        auto find(dungeon::id_type dungeon_id) -> dungeon *;
        auto find_by_map_index(int32_t map_index) -> dungeon *;

      private:
        dungeon_map dungeons_;
        dungeon_by_map dungeons_by_map_;

        dungeon::id_type next_id_;
    };

    inline auto get_dungeon_manager() -> dungeon_manager & {
        static dungeon_manager instance;
        return instance;
    }
}
