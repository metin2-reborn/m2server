#include <metin2/config/config.h>
#include <metin2/game/quest/questpc.h>

#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/quest.h>
#include <metin2/game/quest/questevent.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/text/locale.hpp>

#include <metin2/log/log.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/tables.h>

#include <cassert>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace metin2::game {

    namespace quest {
        pc::pc() : is_given_reward_(false),
                   should_send_done_(false),
                   id_(0),
                   running_quest_state_(nullptr),
                   send_to_client_(0),
                   loaded_(false),
                   last_state_(0),
                   wait_confirm_from_pid_(0),
                   confirm_wait_(false) {
        }

        pc::~pc() {
            destroy();
        }

        void pc::destroy() {
            clear_timer();
        }

        void pc::set_id(uint32_t id) {
            id_ = id;
            should_send_done_ = false;
        }

        auto pc::current_quest_name() const -> const std::string & {
            return cur_quest_;
        }

        auto pc::current_quest_index() -> int {
            return get_quest_manager().quest_index_by_name(current_quest_name());
        }

        void pc::set_flag(const std::string &name, int value, bool skip_save) {
            if (metin2::config::test_server())
                metin2::log::info("Quest: set flag {} = {:d}", name, value);
            else
                metin2::log::debug("Quest: set flag {} = {:d}", name, value);

            if (value == 0) {
                delete_flag(name);
                return;
            }

            auto it = flag_map_.find(name);

            if (it == flag_map_.end())
                flag_map_.insert(make_pair(name, value));
            else if (it->second != value)
                it->second = value;
            else
                skip_save = true;

            if (!skip_save)
                save_flag(name, value);
        }

        auto pc::delete_flag(const std::string &name) -> bool {
            auto it = flag_map_.find(name);

            if (it != flag_map_.end()) {
                flag_map_.erase(it);
                save_flag(name, 0);
                return true;
            }

            return false;
        }

        auto pc::flag(const std::string &name) -> int {
            auto it = flag_map_.find(name);

            if (it != flag_map_.end()) {
                metin2::log::debug("Quest: get flag {} = {:d}", name, it->second);
                return it->second;
            }
            return 0;
        }

        void pc::save_flag(const std::string &name, int value) {
            auto it = flag_save_map_.find(name);

            if (it == flag_save_map_.end())
                flag_save_map_.insert(make_pair(name, value));
            else if (it->second != value)
                it->second = value;
        }

        void pc::set_current_quest_state_name(const std::string &state_name) {
            set_flag(cur_quest_ + ".__status", get_quest_manager().quest_state_index(cur_quest_, state_name));
        }

        void pc::set_quest_state(const std::string &quest_name, const std::string &state_name) {
            set_quest_state(quest_name, get_quest_manager().quest_state_index(quest_name, state_name));
        }

        void pc::set_quest_state(const std::string &quest_name, int new_state_index) {
            int now_state = flag(quest_name + ".__status");

            if (now_state != new_state_index)
                add_quest_state_change(quest_name, now_state, new_state_index);
        }

        void pc::add_quest_state_change(const std::string &quest_name, int prev_state, int next_state) {
            uint32_t quest_index = get_quest_manager().quest_index_by_name(quest_name);
            metin2::log::debug("Quest: queue state change of quest {} ({:d}) from {:d} to {:d}", quest_name, quest_index, prev_state, next_state);
            quest_state_change_.push_back(quest_state_change_info(quest_index, prev_state, next_state));
        }

        void pc::set_quest(const std::string &quest_name, quest_state &qs) {
            unsigned int qi = get_quest_manager().quest_index_by_name(quest_name);
            auto it = quest_info_.find(qi);

            if (it == quest_info_.end())
                quest_info_.insert(std::make_pair(qi, qs));
            else
                it->second = qs;

            cur_quest_ = quest_name;
            running_quest_state_ = &quest_info_[qi];
            send_to_client_ = 0;

            last_state_ = qs.st;
            set_flag(quest_name + ".__status", qs.st);

            running_quest_state_->index = qi;
            should_send_done_ = false;
        }

        void pc::add_timer(const std::string &name, event_ptr event) {
            remove_timer(name);
            timer_map_.insert(make_pair(name, event));
            metin2::log::debug("Quest: add timer {}, {:d} timers", static_cast<const void *>(event.get()), timer_map_.size());
        }

        void pc::remove_timer_not_cancel(const std::string &name) {
            auto it = timer_map_.find(name);

            if (it != timer_map_.end()) {
                metin2::log::debug("Quest: remove timer {} without cancelling", static_cast<const void *>(it->second.get()));
                timer_map_.erase(it);
            }

            metin2::log::debug("Quest: {:d} timers left", timer_map_.size());
        }

        void pc::remove_timer(const std::string &name) {
            auto it = timer_map_.find(name);

            if (it != timer_map_.end()) {
                metin2::log::debug("Quest: remove timer {}", static_cast<const void *>(it->second.get()));
                cancel_timer_event(&it->second);
                timer_map_.erase(it);
            }

            metin2::log::debug("Quest: {:d} timers left", timer_map_.size());
        }

        void pc::clear_timer() {
            metin2::log::debug("Quest: clear {:d} timers", timer_map_.size());
            auto it = timer_map_.begin();

            while (it != timer_map_.end()) {
                cancel_timer_event(&it->second);
                ++it;
            }

            timer_map_.clear();
        }

        void pc::set_current_quest_start_flag() {
            if (!current_quest_begin_flag()) {
                set_current_quest_begin_flag();
            }
        }

        void pc::set_current_quest_done_flag() {
            if (current_quest_begin_flag()) {
                clear_current_quest_begin_flag();
            }
        }

        void pc::send_quest_info_pakcet() {
            assert(send_to_client_);
            assert(running_quest_state_);

            packet_quest_info qi;

            qi.header = HEADER_GC_QUEST_INFO;
            qi.size = sizeof(struct packet_quest_info);
            qi.index = running_quest_state_->index;
            qi.flag = send_to_client_;

            temp_buffer buf;
            buf.write(&qi, sizeof(qi));

            if (send_to_client_ & QUEST_SEND_ISBEGIN) {
                uint8_t temp = running_quest_state_->start ? 1 : 0;
                buf.write(&temp, 1);
                qi.size += 1;

                metin2::log::debug("Quest info: begin flag {:d}", (int)temp);
            }
            if (send_to_client_ & QUEST_SEND_TITLE) {
                running_quest_state_->title.reserve(30 + 1);
                buf.write(running_quest_state_->title.c_str(), 30 + 1);
                qi.size += 30 + 1;

                metin2::log::debug("Quest info: title {}", running_quest_state_->title);
            }
            if (send_to_client_ & QUEST_SEND_CLOCK_NAME) {
                running_quest_state_->clock_name.reserve(16 + 1);
                buf.write(running_quest_state_->clock_name.c_str(), 16 + 1);
                qi.size += 16 + 1;

                metin2::log::debug("Quest info: clock name {}", running_quest_state_->clock_name);
            }
            if (send_to_client_ & QUEST_SEND_CLOCK_VALUE) {
                buf.write(&running_quest_state_->clock_value, sizeof(int));
                qi.size += 4;

                metin2::log::debug("Quest info: clock value {:d}", running_quest_state_->clock_value);
            }
            if (send_to_client_ & QUEST_SEND_COUNTER_NAME) {
                running_quest_state_->counter_name.reserve(16 + 1);
                buf.write(running_quest_state_->counter_name.c_str(), 16 + 1);
                qi.size += 16 + 1;

                metin2::log::debug("Quest info: counter name {}", running_quest_state_->counter_name);
            }
            if (send_to_client_ & QUEST_SEND_COUNTER_VALUE) {
                buf.write(&running_quest_state_->counter_value, sizeof(int));
                qi.size += 4;

                metin2::log::debug("Quest info: counter value {:d}", running_quest_state_->counter_value);
            }
            if (send_to_client_ & QUEST_SEND_ICON_FILE) {
                running_quest_state_->icon_file.reserve(24 + 1);
                buf.write(running_quest_state_->icon_file.c_str(), 24 + 1);
                qi.size += 24 + 1;

                metin2::log::debug("Quest info: icon file {}", running_quest_state_->icon_file);
            }

            get_quest_manager().current_character_ptr()->get_desc()->packet(buf.read_peek(), buf.size());

            send_to_client_ = 0;
        }

        void pc::end_running() {
            {
                character *npc = get_quest_manager().current_npc_character_ptr();
                character *ch = get_quest_manager().current_character_ptr();
                if (npc && !npc->is_pc()) {
                    if (ch->player_id() == npc->quest_npcid()) {
                        npc->set_quest_npcid(0);
                        metin2::log::warn("Quest npc was still locked by player (pid {:d}), unlocking", ch->player_id());
                        get_quest_manager().write_running_state_to_syserr();
                    }
                }
            }

            if (has_reward()) {
                save();

                character *ch = get_quest_manager().current_character_ptr();
                if (ch != nullptr) {
                    reward(ch);
                    ch->save();
                }
            }
            is_given_reward_ = false;

            if (send_to_client_) {
                metin2::log::debug("Quest: end running, send info {:d}", send_to_client_);
                send_quest_info_pakcet();
            }

            if (running_quest_state_ == nullptr) {
                metin2::log::error("No running quest state at end of quest run");
                return;
            }
            quest_state *old_state = running_quest_state_;
            int now_state = running_quest_state_->st;

            running_quest_state_ = nullptr;

            if (last_state_ != now_state) {
                character *ch = get_quest_manager().current_character_ptr();
                uint32_t quest_index = get_quest_manager().quest_index_by_name(cur_quest_);
                if (ch) {
                    set_flag(cur_quest_ + ".__status", last_state_);
                    get_quest_manager().leave_state(ch->player_id(), quest_index, last_state_);
                    old_state->st = now_state;
                    set_flag(cur_quest_ + ".__status", now_state);
                    get_quest_manager().enter_state(ch->player_id(), quest_index, now_state);
                    if (flag(cur_quest_ + ".__status") == now_state)
                        get_quest_manager().letter(ch->player_id(), quest_index, now_state);
                }
            }

            do_quest_state_change();
        }

        void pc::do_quest_state_change() {
            character *ch = get_quest_manager().current_character_ptr();

            std::vector<quest_state_change_info> quest_state_change;
            quest_state_change_.swap(quest_state_change);

            for (uint32_t i = 0; i < quest_state_change.size(); ++i) {
                const quest_state_change_info &info = quest_state_change[i];
                if (info.quest_idx == 0)
                    continue;

                uint32_t quest_idx = info.quest_idx;
                auto it = quest_find(quest_idx);
                const std::string quest_name = get_quest_manager().quest_name_by_index(quest_idx);

                if (it == quest_end()) {
                    quest_state qs;
                    qs.st = 0;

                    quest_info_.insert(std::make_pair(quest_idx, qs));
                    set_flag(quest_name + ".__status", 0);

                    it = quest_find(quest_idx);
                }

                metin2::log::debug("Quest: apply queued state change of quest {:d} from {:d} to {:d} (current {:d}, expected {:d})", quest_idx, info.prev_state, info.next_state, it->second.st, info.prev_state);

                assert(it->second.st == info.prev_state);

                get_quest_manager().leave_state(ch->player_id(), quest_idx, info.prev_state);
                it->second.st = info.next_state;
                set_flag(quest_name + ".__status", info.next_state);

                get_quest_manager().enter_state(ch->player_id(), quest_idx, info.next_state);

                if (flag(quest_name + ".__status") == info.next_state)
                    get_quest_manager().letter(ch->player_id(), quest_idx, info.next_state);
            }
        }

        void pc::cancel_running() {
            running_quest_state_ = nullptr;
            send_to_client_ = 0;
            should_send_done_ = false;
        }

        void pc::set_send_flag(int idx) {
            send_to_client_ |= idx;
        }

        void pc::clear_current_quest_begin_flag() {
            set_send_flag(QUEST_SEND_ISBEGIN);
            running_quest_state_->start = false;
        }

        void pc::set_current_quest_begin_flag() {
            quest_manager &q = get_quest_manager();
            int quest_index = q.quest_index_by_name(cur_quest_);
            running_quest_state_->start = true;
            running_quest_state_->index = quest_index;

            set_send_flag(QUEST_SEND_ISBEGIN);
        }

        auto pc::current_quest_begin_flag() -> int {
            return running_quest_state_ ? running_quest_state_->index : 0;
        }

        void pc::set_current_quest_title(const std::string &title) {
            set_send_flag(QUEST_SEND_TITLE);
            running_quest_state_->title = title;
        }

        void pc::set_quest_title(const std::string &quest, const std::string &title) {
            auto it = quest_info_.find(get_quest_manager().quest_index_by_name(quest));

            if (it != quest_info_.end()) {
                quest_state *old = running_quest_state_;
                int old2 = send_to_client_;
                std::string oldquestname = cur_quest_;
                cur_quest_ = quest;
                running_quest_state_ = &it->second;
                send_to_client_ = QUEST_SEND_TITLE;
                running_quest_state_->index = current_quest_begin_flag();

                set_current_quest_title(title);

                send_quest_info_pakcet();

                cur_quest_ = oldquestname;
                running_quest_state_ = old;
                send_to_client_ = old2;
            }
        }

        void pc::set_current_quest_clock_name(const std::string &name) {
            set_send_flag(QUEST_SEND_CLOCK_NAME);
            running_quest_state_->clock_name = name;
        }

        void pc::set_current_quest_clock_value(int value) {
            set_send_flag(QUEST_SEND_CLOCK_VALUE);
            running_quest_state_->clock_value = value;
        }

        void pc::set_current_quest_counter_name(const std::string &name) {
            set_send_flag(QUEST_SEND_COUNTER_NAME);
            running_quest_state_->counter_name = name;
        }

        void pc::set_current_quest_counter_value(int value) {
            set_send_flag(QUEST_SEND_COUNTER_VALUE);
            running_quest_state_->counter_value = value;
        }

        void pc::set_current_quest_icon_file(const std::string &icon_file) {
            set_send_flag(QUEST_SEND_ICON_FILE);
            running_quest_state_->icon_file = icon_file;
        }

        void pc::save() {
            if (flag_save_map_.empty())
                return;

            static std::vector<quest_table> s_table;
            s_table.resize(flag_save_map_.size());

            int i = 0;

            auto it = flag_save_map_.begin();

            while (it != flag_save_map_.end()) {
                const std::string &comp = it->first;
                int32_t value = it->second;

                ++it;

                int pos = comp.find(".");

                if (pos < 0) {
                    metin2::log::error("Quest flag {} has no '.' separator", comp);
                    continue;
                }

                std::string name_value;
                name_value.assign(comp, 0, pos);

                std::string state_value;
                state_value.assign(comp, pos + 1, comp.length());

                if (name_value.length() == 0 || state_value.length() == 0) {
                    metin2::log::error("Quest flag {} has an empty quest or state name", comp);
                    continue;
                }

                metin2::log::debug("Quest: save flag {}.{} = {:d} ({:d})", name_value, state_value, value, i);

                if (name_value.length() >= QUEST_NAME_MAX_LEN) {
                    metin2::log::error("Quest name {} is too long to save", name_value);
                    continue;
                }

                if (state_value.length() >= QUEST_STATE_MAX_LEN) {
                    metin2::log::error("Quest state {} is too long to save", state_value);
                    continue;
                }

                quest_table &r = s_table[i++];

                r.pid = id_;
                strlcpy(r.name, name_value.c_str(), sizeof(r.name));
                strlcpy(r.state, state_value.c_str(), sizeof(r.state));
                r.value = value;
            }

            if (i > 0) {
                metin2::log::debug("Quest: saving {:d} flags", i);
                db_clientdesc->db_packet_header(HEADER_GD_QUEST_SAVE, 0, sizeof(quest_table) * i);
                db_clientdesc->packet(&s_table[0], sizeof(quest_table) * i);
            }

            flag_save_map_.clear();
        }

        void pc::give_item(const std::string &label, uint32_t vnum, int count) {
            metin2::log::debug("Quest: give item {} (vnum {:d}, count {:d})", label, vnum, count);
            if (!flag(cur_quest_ + "." + label)) {
                reward_data_.push_back(reward_data(reward_data::REWARD_TYPE_ITEM, vnum, count));
            } else
                is_given_reward_ = true;
        }

        void pc::give_exp(const std::string &label, uint32_t exp) {
            metin2::log::debug("Quest: give exp {} ({:d})", label, exp);

            if (!flag(cur_quest_ + "." + label)) {
                reward_data_.push_back(reward_data(reward_data::REWARD_TYPE_EXP, exp));
            } else
                is_given_reward_ = true;
        }

        void pc::reward(character *ch) {
            if (is_given_reward_) {
                ch->chat_packet(CHAT_TYPE_INFO, lc_text("<퀘스트> 이전에 같은 보상을 받은 적이 있어 다시 받지 않습니다."));
                is_given_reward_ = false;
            }

            for (auto it = reward_data_.begin(); it != reward_data_.end(); ++it) {
                switch (it->type) {
                case reward_data::REWARD_TYPE_EXP:
                    metin2::log::debug("Quest reward: exp {:d} + {:d}, next level at {:d}", ch->get_exp(), it->value1, ch->next_exp());

                    if (ch->get_exp() + it->value1 > ch->next_exp())
                        ch->point_change(POINT_EXP, ch->next_exp() - 1 - ch->get_exp());
                    else
                        ch->point_change(POINT_EXP, it->value1);

                    break;

                case reward_data::REWARD_TYPE_ITEM:
                    ch->auto_give_item(it->value1, it->value2);
                    break;

                case reward_data::REWARD_TYPE_NONE:
                default:
                    metin2::log::error("Unknown quest reward type {:d}", static_cast<int>(it->type));
                    break;
                }
            }

            reward_data_.clear();
        }

        void pc::build() {
            decltype(flag_map_.begin()) it;
            for (it = flag_map_.begin(); it != flag_map_.end(); ++it) {
                if (it->first.size() > 9 && it->first.compare(it->first.size() - 9, 9, ".__status") == 0) {
                    uint32_t quest_index = get_quest_manager().quest_index_by_name(it->first.substr(0, it->first.size() - 9));
                    int state = it->second;
                    quest_state qs;
                    qs.st = state;

                    quest_info_.insert(std::make_pair(quest_index, qs));
                }
            }
        }

        void pc::clear_quest(const std::string &quest_name) {
            std::string quest_name_with_dot = quest_name + '.';
            for (auto it = flag_map_.begin(); it != flag_map_.end();) {
                auto now = it++;
                if (now->second != 0 && now->first.compare(0, quest_name_with_dot.size(), quest_name_with_dot) == 0) {
                    set_flag(now->first, 0);
                }
            }

            clear_timer();

            auto it = quest_begin();
            unsigned int questindex = quest::get_quest_manager().quest_index_by_name(quest_name);

            while (it != quest_end()) {
                if (it->first == questindex) {
                    it->second.st = 0;
                    break;
                }

                ++it;
            }
        }

        void pc::send_flag_list(character *ch) {
            for (auto it = flag_map_.begin(); it != flag_map_.end(); ++it) {
                if (it->first.size() > 9 && it->first.compare(it->first.size() - 9, 9, ".__status") == 0) {
                    const std::string quest_name = it->first.substr(0, it->first.size() - 9);
                    const char *state_name = get_quest_manager().quest_state_name(quest_name, it->second);
                    ch->chat_packet(CHAT_TYPE_INFO, "%s %s (%d)", quest_name.c_str(), state_name, it->second);
                } else {
                    ch->chat_packet(CHAT_TYPE_INFO, "%s %d", it->first.c_str(), it->second);
                }
            }
        }
    }
}
