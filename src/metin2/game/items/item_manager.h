#pragma once
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>

#include <metin2/core/utils.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include <sys/types.h>

namespace metin2::game::inline items {

    class special_attr_group {
      public:
        special_attr_group(uint32_t vnum_value)
            : vnum(vnum_value) {}
        struct special_attr_info {
            special_attr_info(uint32_t apply_type_value, uint32_t apply_value_value)
                : apply_type(apply_type_value), apply_value(apply_value_value) {}
            uint32_t apply_type;
            uint32_t apply_value;
        };
        uint32_t vnum;
        std::string effect_file_name;
        std::vector<special_attr_info> attrs;
    };

    class special_item_group {
      public:
        enum give_type {
            NONE,
            GOLD,
            EXP,
            MOB,
            SLOW,
            DRAIN_HP,
            POISON,
            MOB_GROUP,
        };

        enum sig_type { NORMAL,
            PCT,
            QUEST,
            SPECIAL };

        struct special_item_info {
            uint32_t vnum;
            int count;
            int rare;

            special_item_info(uint32_t vnum_value, int count_value, int rare_value)
                : vnum(vnum_value), count(count_value), rare(rare_value) {}
        };

        special_item_group(uint32_t vnum_value, uint8_t type_value = 0)
            : vnum(vnum_value), type(type_value) {}

        void add_item(uint32_t vnum, int count, int prob, int rare) {
            if (!prob)
                return;
            if (!probs.empty())
                prob += probs.back();
            probs.push_back(prob);
            items.push_back(special_item_info(vnum, count, rare));
        }

        auto multi_index(std::vector<int> &idx_vec) const -> int {
            idx_vec.clear();
            if (type == PCT) {
                int count = 0;
                if (prob(probs[0])) {
                    idx_vec.push_back(0);
                    count++;
                }
                for (uint i = 1; i < probs.size(); i++) {
                    if (prob(probs[i] - probs[i - 1])) {
                        idx_vec.push_back(i);
                        count++;
                    }
                }
                return count;
            } else {
                idx_vec.push_back(one_index());
                return 1;
            }
        }

        auto one_index() const -> int {
            int n = number(1, probs.back());
            auto it = std::ranges::lower_bound(probs, n);
            return std::distance(probs.begin(), it);
        }

        auto get_vnum(int idx) const -> int {
            return items[idx].vnum;
        }

        auto count(int idx) const -> int {
            return items[idx].count;
        }

        auto rare_pct(int idx) const -> int {
            return items[idx].rare;
        }

        auto contains(uint32_t vnum_value) const -> bool {
            for (uint32_t i = 0; i < items.size(); i++) {
                if (items[i].vnum == vnum_value)
                    return true;
            }
            return false;
        }

        auto attr_vnum(uint32_t vnum_value) const -> uint32_t {
            if (special_item_group::SPECIAL != type)
                return 0;
            for (auto it = items.begin(); it != items.end(); it++) {
                if (it->vnum == vnum_value) {
                    return it->count;
                }
            }
            return 0;
        }

        uint32_t vnum;
        uint8_t type;
        std::vector<int> probs;
        std::vector<special_item_info> items;
    };

    class mob_item_group {
      public:
        struct mob_item_group_info {
            uint32_t item_vnum;
            int count;
            int rare_pct;

            mob_item_group_info(uint32_t item_vnum_value, int count_value, int rare_pct_value)
                : item_vnum(item_vnum_value),
                  count(count_value),
                  rare_pct(rare_pct_value) {
            }
        };

        explicit mob_item_group(int kill_drop) : kill_drop_(kill_drop) {
        }

        auto kill_per_drop() const -> int {
            return kill_drop_;
        }

        void add_item(uint32_t item_vnum, int count, int part_pct, int rare_pct) {
            if (!probs_.empty())
                part_pct += probs_.back();
            probs_.push_back(part_pct);
            items_.push_back(mob_item_group_info(item_vnum, count, rare_pct));
        }

        auto is_empty() const -> bool {
            return probs_.empty();
        }

        auto one_index() const -> int {
            int n = number(1, probs_.back());
            auto it = std::ranges::lower_bound(probs_, n);
            return std::distance(probs_.begin(), it);
        }

        auto one() const -> const mob_item_group_info & {
            return items_[one_index()];
        }

      private:
        int kill_drop_;
        std::vector<int> probs_;
        std::vector<mob_item_group_info> items_;
    };

    class drop_item_group {
        struct drop_item_group_info {
            uint32_t vnum;
            uint32_t pct;
            int count;

            drop_item_group_info(uint32_t vnum_value, uint32_t pct_value, int count_value)
                : vnum(vnum_value), pct(pct_value), count(count_value) {}
        };

      public:
        drop_item_group() = default;

        auto get_vector() -> const std::vector<drop_item_group_info> & {
            return items_;
        }

        void add_item(uint32_t item_vnum, uint32_t pct, int count) {
            items_.push_back(drop_item_group_info(item_vnum, pct, count));
        }

      private:
        std::vector<drop_item_group_info> items_;
    };

    class level_item_group {
        struct level_item_group_info {
            uint32_t v_num;
            uint32_t pct;
            int count;

            level_item_group_info(uint32_t vnum, uint32_t pct_value, int count_value)
                : v_num(vnum), pct(pct_value), count(count_value) {}
        };

      private:
        uint32_t level_limit_;
        std::vector<level_item_group_info> items_;

      public:
        level_item_group(uint32_t level_limit)
            : level_limit_(level_limit) {}

        auto level_limit() -> uint32_t { return level_limit_; }

        void add_item(uint32_t item_vnum, uint32_t pct, int count) {
            items_.push_back(level_item_group_info(item_vnum, pct, count));
        }

        auto get_vector() -> const std::vector<level_item_group_info> & {
            return items_;
        }
    };

    class buyer_thief_gloves_item_group {
        struct thief_group_info {
            uint32_t vnum;
            uint32_t pct;
            int count;

            thief_group_info(uint32_t vnum_value, uint32_t pct_value, int count_value)
                : vnum(vnum_value), pct(pct_value), count(count_value) {}
        };

      public:
        buyer_thief_gloves_item_group() = default;

        auto get_vector() -> const std::vector<thief_group_info> & {
            return items_;
        }

        void add_item(uint32_t item_vnum, uint32_t pct, int count) {
            items_.push_back(thief_group_info(item_vnum, pct, count));
        }

      private:
        std::vector<thief_group_info> items_;
    };

    class item_manager {
      public:
        item_manager();
        ~item_manager();

        auto initialize(item_table *table_value, int size) -> bool;
        void destroy();
        void update();
        void graceful_shutdown();

        auto new_id() -> uint32_t;
        auto set_max_item_id(item_id_range_table range) -> bool;
        auto set_max_spare_item_id(item_id_range_table range) -> bool;

        void delayed_save(item *item);
        void flush_delayed_save(item *item);
        void save_single_item(item *item);

        auto create_item(uint32_t vnum, uint32_t count = 1, uint32_t id = 0, bool try_magic = false, int rare_pct = -1, bool skip_save = false) -> item *;
        void destroy_item(item *item);
        void remove_item(item *item, const char *reason = nullptr);

        auto find(uint32_t id) -> item *;
        auto find_by_vid(uint32_t vid) -> item *;
        auto table(uint32_t vnum) -> item_table *;
        auto vnum(const char *name, uint32_t &vnum) -> bool;
        auto vnum_by_original_name(const char *name_value, uint32_t &vnum) -> bool;

        auto drop_pct(character *chr, character *killer, int &delta_percent, int &rand_range) -> bool;
        auto create_drop_item(character *chr, character *killer, std::vector<item *> &items) -> bool;

        auto read_common_drop_item_file(const char *file_name) -> bool;
        auto read_etc_drop_item_file(const char *file_name) -> bool;
        auto read_drop_item_group(const char *file_name) -> bool;
        auto read_monster_drop_item_group(const char *file_name) -> bool;
        auto read_special_drop_item_file(const char *file_name) -> bool;

        auto conv_special_drop_item_file() -> bool;

        auto refine_from_vnum(uint32_t vnum) -> uint32_t;

        static void copy_all_attr_to(item *old_item, item *new_item);

        auto get_special_item_group(uint32_t vnum) -> const special_item_group *;
        auto get_special_attr_group(uint32_t vnum) -> const special_attr_group *;

        auto special_group_from_item(uint32_t vnum) const -> int {
            auto it = item_to_special_group_.find(vnum);
            return (it == item_to_special_group_.end()) ? 0 : it->second;
        }

      protected:
        auto real_number(uint32_t vnum) -> int;
        void create_quest_drop_item(character *chr, character *killer, std::vector<item *> &items, int delta_percent, int rand_range);

      protected:
        using item_vid_map = std::map<uint32_t, item *>;

        std::vector<item_table> prototypes_;
        std::vector<item_table *> item_vnum_range_info_;
        std::map<uint32_t, uint32_t> item_refine_from_;

        item_vid_map vid_map_;
        uint32_t vid_count_;
        uint32_t current_id_;
        item_id_range_table item_id_range_;
        item_id_range_table item_id_spare_range_;

        std::unordered_set<item *> items_for_delayed_save_;
        std::map<uint32_t, item *> items_by_id_;
        std::map<uint32_t, uint32_t> etc_item_drop_probs_;
        std::map<uint32_t, std::unique_ptr<drop_item_group>> drop_item_groups_;
        std::map<uint32_t, std::unique_ptr<special_item_group>> special_item_groups_;
        std::map<uint32_t, std::unique_ptr<special_item_group>> quest_item_groups_;
        std::map<uint32_t, std::unique_ptr<special_attr_group>> special_attr_groups_;
        std::map<uint32_t, std::unique_ptr<mob_item_group>> mob_item_groups_;
        std::map<uint32_t, std::unique_ptr<level_item_group>> level_item_groups_;
        std::map<uint32_t, std::unique_ptr<buyer_thief_gloves_item_group>> glove_item_groups_;

        std::map<uint32_t, int> item_to_special_group_;

      private:
        using new_vnum_map = std::map<uint32_t, uint32_t>;
        new_vnum_map new_to_ori_;

      public:
        auto mask_vnum(uint32_t vnum) -> uint32_t;
        std::map<uint32_t, item_table> vids;

        const static int max_norm_attr_num = 5;
        const static int max_rare_attr_num = 2;
        auto read_item_vnum_mask_table(const char *file_name) -> bool;

      private:
    };

    inline auto get_item_manager() -> item_manager & {
        static item_manager instance;
        return instance;
    }

    extern auto drop_event_refine_box_set_value(const std::string &name, int value) -> bool;
    extern auto drop_event_char_stone_set_value(const std::string &name, int value) -> bool;
}
