#pragma once

#include <metin2/game/app/typedef.h>
#include <metin2/game/world/vid.h>

#include <metin2/common/singleton.h>
#include <metin2/proto/character_types.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <unordered_map>

namespace metin2::game::inline world {
    class dungeon;
}

namespace metin2::game::inline characters {

    class character;
    class character_vector_interactor;

    class character_manager : public singleton<character_manager> {
      public:
        using name_map = std::unordered_map<std::string, character *>;

        character_manager();
        ~character_manager() override;

        void destroy();

        void graceful_shutdown();

        auto alloc_vid() -> uint32_t;

        auto create_character(const char *name, uint32_t pid = 0) -> character *;
        void destroy_character(character *ch);

        void update(int pulse);

        auto spawn_mob(uint32_t vnum, int32_t map_index, int32_t x, int32_t y, int32_t z, bool spawn_motion = false, int rot = -1, bool show_value = true) -> character *;
        auto spawn_mob_range(uint32_t vnum, int32_t map_index, int sx, int sy, int ex, int ey, bool is_exception = false, bool spawn_motion = false, bool aggressive = false) -> character *;
        auto spawn_group(uint32_t vnum, int32_t map_index, int sx, int sy, int ex, int ey, regen *regen_value = nullptr, bool aggressive = false, dungeon *dungeon_value = nullptr) -> character *;
        auto spawn_group_group(uint32_t vnum, int32_t map_index, int sx, int sy, int ex, int ey, regen *regen_value = nullptr, bool aggressive = false, dungeon *dungeon_value = nullptr) -> bool;
        auto spawn_move_group(uint32_t vnum, int32_t map_index, int sx, int sy, int ex, int ey, int tx, int ty, regen *regen_value = nullptr, bool aggressive = false) -> bool;
        auto spawn_mob_random_position(uint32_t vnum, int32_t map_index) -> character *;

        void select_stone(character *chr);

        auto pc_map() -> name_map & { return map_pc_chr_; }

        auto find(uint32_t vid) -> character *;
        auto find(const vid &vid) -> character *;
        auto find_pc(const char *name) -> character *;
        auto find_by_pid(uint32_t pid) -> character *;

        auto add_to_state_list(character *ch) -> bool;
        void remove_from_state_list(character *ch);

        void delayed_save(character *ch);
        auto flush_delayed_save(character *ch) -> bool;
        void process_delayed_save();

        template <class Func>
        auto for_each_pc(Func f) -> Func;

        void register_for_monster_log(character *ch);
        void unregister_for_monster_log(character *ch);
        void packet_monster_log(character *ch, const void *buf, int size);

        void kill_log(uint32_t vnum);

        void register_race_num(uint32_t vnum);
        void register_race_num_map(character *ch);
        void unregister_race_num_map(character *ch);
        auto characters_by_race_num(uint32_t race_num, character_vector_interactor &i) -> bool;

        auto find_specify_pc(unsigned int job_flag, int32_t map_index_value, character *except = nullptr, int min_level = 1, int max_level = PLAYER_MAX_LEVEL_CONST) -> character *;

        void set_mob_item_rate(int value) { mob_item_rate_ = value; }
        void set_mob_damage_rate(int value) { mob_damage_rate_ = value; }
        void set_mob_gold_amount_rate(int value) { mob_gold_amount_rate_ = value; }
        void set_mob_gold_drop_rate(int value) { mob_gold_drop_rate_ = value; }
        void set_mob_exp_rate(int value) { mob_exp_rate_ = value; }

        void set_mob_item_rate_premium(int value) { mob_item_rate_premium_ = value; }
        void set_mob_gold_amount_rate_premium(int value) { mob_gold_amount_rate_premium_ = value; }
        void set_mob_gold_drop_rate_premium(int value) { mob_gold_drop_rate_premium_ = value; }
        void set_mob_exp_rate_premium(int value) { mob_exp_rate_premium_ = value; }

        auto mob_item_rate(character *ch) -> int;
        auto mob_damage_rate(character *ch) -> int;
        auto mob_gold_amount_rate(character *ch) -> int;
        auto mob_gold_drop_rate(character *ch) -> int;
        auto mob_exp_rate(character *ch) -> int;

        void send_script_to_map(int32_t map_index, const std::string &s);

        auto begin_pending_destroy() -> bool;
        void flush_pending_destroy();

      private:
        int mob_item_rate_;
        int mob_damage_rate_;
        int mob_gold_amount_rate_;
        int mob_gold_drop_rate_;
        int mob_exp_rate_;

        int mob_item_rate_premium_;
        int mob_gold_amount_rate_premium_;
        int mob_gold_drop_rate_premium_;
        int mob_exp_rate_premium_;

        int vid_count_;

        std::unordered_map<uint32_t, character *> map_chr_by_vid_;
        std::unordered_map<uint32_t, character *> map_chr_by_pid_;
        name_map map_pc_chr_;

        character_set set_chr_state_;
        character_set set_chr_for_delayed_save_;
        character_set set_chr_monster_log_;

        character *chr_selected_stone_;

        std::map<uint32_t, uint32_t> map_mob_kill_count_;

        std::set<uint32_t> set_registered_race_num_;
        std::map<uint32_t, character_set> map_chr_by_race_num_;

        bool use_pending_destroy_;
        character_set set_chr_pending_destroy_;
    };

    template <class Func>
    auto character_manager::for_each_pc(Func f) -> Func {
        std::unordered_map<uint32_t, character *>::iterator it;

        for (it = map_chr_by_pid_.begin(); it != map_chr_by_pid_.end(); ++it)
            f(it->second);

        return f;
    }

    class character_vector_interactor : public character_vector {
      public:
        character_vector_interactor() : my_begin_(false) {}

        character_vector_interactor(const character_set &r);
        virtual ~character_vector_interactor();

      private:
        bool my_begin_;
    };
}
