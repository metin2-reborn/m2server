#pragma once
#include <metin2/game/app/typedef.h>

#include <metin2/core/utils.h>
#include <metin2/proto/mob_types.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace metin2::game::inline characters {

    struct mob_splash_attack_info {
        uint32_t timing;
        uint32_t hit_distance;

        mob_splash_attack_info(uint32_t timing_value, uint32_t hit_distance_value)
            : timing(timing_value), hit_distance(hit_distance_value) {}
    };

    struct mob_skill_info {
        uint32_t skill_vnum;
        uint8_t skill_level;
        std::vector<mob_splash_attack_info> splash_attack;
    };

    class mob {
      public:
        mob();

        ~mob() = default;

        mob_table table;
        game::characters::mob_skill_info mob_skill_info[MOB_SKILL_MAX_NUM];

        void add_skill_splash(int index, uint32_t timing, uint32_t hit_distance);
    };

    class mob_instance {
      public:
        mob_instance();

        pixel_position pos_last_attacked;
        uint32_t last_attacked_time;
        uint32_t last_warp_time;

        bool is_berserk;
        bool is_god_speed;
        bool is_revive;
    };

    class mob_group_group {
      public:
        mob_group_group(uint32_t vnum_value) {
            vnum = vnum_value;
        }

        void add_member(uint32_t vnum, int prob = 1) {
            if (prob == 0)
                return;

            if (!probs.empty())
                prob += probs.back();

            probs.push_back(prob);
            member_vnums.push_back(vnum);
        }

        auto member() -> uint32_t {
            if (member_vnums.empty())
                return 0;

            int n = number(1, probs.back());
            auto it = std::ranges::lower_bound(probs, n);

            return member_vnums[std::distance(probs.begin(), it)];
        }

        uint32_t vnum;
        std::vector<uint32_t> member_vnums;

        std::vector<int> probs;
    };

    class mob_group {
      public:
        void create(uint32_t vnum, std::string &name) {
            vnum_ = vnum;
            name_ = name;
        }

        auto member_vector() -> const std::vector<uint32_t> & {
            return member_vnums_;
        }

        void add_member(uint32_t vnum) {
            member_vnums_.push_back(vnum);
        }

      protected:
        uint32_t vnum_;
        std::string name_;
        std::vector<uint32_t> member_vnums_;
    };

    class mob_manager {
      public:
        using iterator = std::map<uint32_t, std::unique_ptr<mob>>::iterator;

        mob_manager();
        ~mob_manager();

        auto initialize(mob_table *table_value, int size) -> bool;
        void destroy();

        auto load_group(const char *file_name) -> bool;
        auto load_group_group(const char *file_name) -> bool;
        auto group(uint32_t vnum) -> mob_group *;
        auto group_from_group_group(uint32_t vnum) -> uint32_t;

        auto get(uint32_t vnum) -> const mob *;
        auto get(const char *name, bool is_abbrev) -> const mob *;

        auto begin() -> const iterator { return mobs_by_vnum_.begin(); }
        auto end() -> const iterator { return mobs_by_vnum_.end(); }

        void rebind_mob_proto(character *ch);

        void inc_regen_count(uint8_t regen_type, uint32_t vnum, int count, int time);
        void dump_regen_count(const char *filename);

      private:
        std::map<uint32_t, std::unique_ptr<mob>> mobs_by_vnum_;
        std::map<std::string, mob *> mobs_by_name_;
        std::map<uint32_t, std::unique_ptr<mob_group>> mob_groups_;
        std::map<uint32_t, std::unique_ptr<mob_group_group>> mob_group_groups_;
        std::vector<std::unique_ptr<mob>> retired_mobs_;

        std::map<uint32_t, double> regen_count_;
    };

    inline auto get_mob_manager() -> mob_manager & {
        static mob_manager instance;
        return instance;
    }
}
