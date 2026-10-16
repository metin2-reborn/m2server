#pragma once

#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/quest/quest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace metin2::game::inline characters {
    class character;
}

namespace metin2::game {

    namespace quest {
        struct reward_data {
            enum reward_type {
                REWARD_TYPE_NONE,
                REWARD_TYPE_EXP,
                REWARD_TYPE_ITEM,
            } type;

            uint32_t value1;
            int value2;

            reward_data(reward_type t, uint32_t value1, int value2 = 0) : type(t),
                                                                          value1(value1),
                                                                          value2(value2) {
            }
        };

        class pc {
          public:
            enum {
                QUEST_SEND_ISBEGIN = (1 << 0),
                QUEST_SEND_TITLE = (1 << 1),
                QUEST_SEND_CLOCK_NAME = (1 << 2),
                QUEST_SEND_CLOCK_VALUE = (1 << 3),
                QUEST_SEND_COUNTER_NAME = (1 << 4),
                QUEST_SEND_COUNTER_VALUE = (1 << 5),
                QUEST_SEND_ICON_FILE = (1 << 6),
            };

            using quest_info = std::map<unsigned int, quest_state>;
            using quest_info_iterator = quest_info::iterator;

            pc();
            ~pc();

            void destroy();
            void set_id(uint32_t id);
            auto id() -> uint32_t { return id_; }

            inline auto quest_begin() -> quest_info_iterator;
            inline auto quest_end() -> quest_info_iterator;
            inline auto quest_find(uint32_t quest_index) -> quest_info_iterator;

            inline auto is_running() -> bool;

            void end_running();
            void cancel_running();

            inline auto running_quest_state() -> quest_state *;

            void set_quest(const std::string &quest_name, quest_state &qs);
            void set_current_quest_state_name(const std::string &state_name);
            void set_quest_state(const std::string &quest_name, const std::string &state_name);
            void set_quest_state(const std::string &quest_name, int new_state_index);

            void clear_quest(const std::string &quest_name);

          private:
            void add_quest_state_change(const std::string &quest_name, int prev_state, int next_state);
            void do_quest_state_change();

            struct quest_state_change_info {
                uint32_t quest_idx;
                int prev_state;
                int next_state;

                quest_state_change_info(uint32_t quest_idx_value, int prev_state_value, int next_state_value) : quest_idx(quest_idx_value),
                                                                                                                prev_state(prev_state_value),
                                                                                                                next_state(next_state_value) {
                }
            };

            std::vector<quest_state_change_info> quest_state_change_;

          public:
            void set_flag(const std::string &name, int value, bool skip_save = false);
            auto flag(const std::string &name) -> int;
            auto delete_flag(const std::string &name) -> bool;

            auto current_quest_name() const -> const std::string &;
            auto current_quest_index() -> int;

            void remove_timer(const std::string &name);
            void remove_timer_not_cancel(const std::string &name);
            void add_timer(const std::string &name, event_ptr event);
            void clear_timer();

            void set_current_quest_start_flag();
            void set_current_quest_done_flag();

            void set_quest_title(const std::string &quest, const std::string &title);

            void set_current_quest_title(const std::string &title);
            void set_current_quest_clock_name(const std::string &name);
            void set_current_quest_clock_value(int value);
            void set_current_quest_counter_name(const std::string &name);
            void set_current_quest_counter_value(int value);
            void set_current_quest_icon_file(const std::string &icon_file);

            auto is_loaded() const -> bool { return loaded_; }
            void set_loaded() { loaded_ = true; }
            void build();
            void save();

            auto has_reward() -> bool { return !reward_data_.empty() || is_given_reward_; }
            void reward(character *ch);

            void give_item(const std::string &label, uint32_t vnum, int count);
            void give_exp(const std::string &label, uint32_t exp);

            void set_send_done_flag() { should_send_done_ = true; }
            auto and_reset_done_flag() -> bool {
                bool temp = should_send_done_;
                should_send_done_ = false;
                return temp;
            }

            void send_flag_list(character *ch);

            void set_quest_state(const char *quest_name, const char *state_name);

            void set_confirm_wait(uint32_t pid) {
                confirm_wait_ = true;
                wait_confirm_from_pid_ = pid;
            }
            void clear_confirm_wait() { confirm_wait_ = false; }
            auto is_confirm_wait() const -> bool { return confirm_wait_; }
            auto is_confirm_wait(uint32_t pid) const -> bool { return confirm_wait_ && pid == wait_confirm_from_pid_; }

          private:
            void set_send_flag(int idx);
            void save_flag(const std::string &name, int value);

            void clear_current_quest_begin_flag();
            void set_current_quest_begin_flag();
            auto current_quest_begin_flag() -> int;

            void send_quest_info_pakcet();

          private:
            std::vector<reward_data> reward_data_;
            bool is_given_reward_;

            bool should_send_done_;

            uint32_t id_;

            quest_info quest_info_;

            quest_state *running_quest_state_;
            std::string cur_quest_;

            using flag_map = std::map<std::string, int>;
            flag_map flag_map_;

            flag_map flag_save_map_;

            using timer_map = std::map<std::string, event_ptr>;
            timer_map timer_map_;

            int send_to_client_;
            bool loaded_;

            int last_state_;

            uint32_t wait_confirm_from_pid_;
            bool confirm_wait_;
        };

        inline auto pc::quest_begin() -> pc::quest_info_iterator {
            return quest_info_.begin();
        }

        inline auto pc::quest_end() -> pc::quest_info_iterator {
            return quest_info_.end();
        }

        inline auto pc::quest_find(uint32_t quest_index) -> pc::quest_info_iterator {
            return quest_info_.find(quest_index);
        }

        inline auto pc::is_running() -> bool {
            return running_quest_state_ != nullptr;
        }

        inline auto pc::running_quest_state() -> quest_state * {
            return running_quest_state_;
        }
    }
}
