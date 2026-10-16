#include <metin2/game/events/ox_event.h>

#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/empire/start_position.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/text/locale.hpp>

#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/chat_types.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <string>
#include <utility>

namespace metin2::game::inline events {

    auto ox_event_manager::initialize() -> bool {
        timed_event_ = nullptr;
        chars_.clear();
        attenders_.clear();
        quizzes_.clear();

        set_status(OXEVENT_FINISH);

        return true;
    }

    void ox_event_manager::destroy() {
        close_event();

        chars_.clear();
        attenders_.clear();
        quizzes_.clear();

        set_status(OXEVENT_FINISH);
    }

    auto ox_event_manager::status() -> ox_event_status {
        uint8_t ret = quest::get_quest_manager().event_flag("oxevent_status");

        switch (ret) {
        case 0:
            return OXEVENT_FINISH;

        case 1:
            return OXEVENT_OPEN;

        case 2:
            return OXEVENT_CLOSE;

        case 3:
            return OXEVENT_QUIZ;

        default:
            return OXEVENT_ERR;
        }

        return OXEVENT_ERR;
    }

    void ox_event_manager::set_status(ox_event_status status) {
        uint8_t val = 0;

        switch (status) {
        case OXEVENT_OPEN:
            val = 1;
            break;

        case OXEVENT_CLOSE:
            val = 2;
            break;

        case OXEVENT_QUIZ:
            val = 3;
            break;

        case OXEVENT_FINISH:
        case OXEVENT_ERR:
        default:
            val = 0;
            break;
        }
        quest::get_quest_manager().request_set_event_flag("oxevent_status", val);
    }

    auto ox_event_manager::enter(character *ch) -> bool {
        if (status() == OXEVENT_FINISH) {
            metin2::log::info("Ox event: player {} entered after the event finished", ch->name());
            return false;
        }

        pixel_position pos = ch->xyz();

        if (pos.x == 896500 && pos.y == 24600) {
            return enter_attender(ch);
        } else if (pos.x == 896300 && pos.y == 28900) {
            return enter_audience(ch);
        } else {
            metin2::log::warn("Ox event: player entered at unexpected position {:d} {:d}", pos.x, pos.y);
            return false;
        }

        return false;
    }

    auto ox_event_manager::enter_attender(character *ch) -> bool {
        uint32_t pid = ch->player_id();

        chars_.insert(std::make_pair(pid, pid));
        attenders_.insert(std::make_pair(pid, pid));

        return true;
    }

    auto ox_event_manager::enter_audience(character *ch) -> bool {
        uint32_t pid = ch->player_id();

        chars_.insert(std::make_pair(pid, pid));

        return true;
    }

    auto ox_event_manager::add_quiz(unsigned char level, const char *question, bool answer) -> bool {
        if (quizzes_.size() < (size_t)level + 1)
            quizzes_.resize(level + 1);

        struct tag_quiz tmp_quiz;

        tmp_quiz.level = level;
        tmp_quiz.quiz = question;
        tmp_quiz.answer = answer;

        quizzes_[level].push_back(tmp_quiz);
        return true;
    }

    auto ox_event_manager::show_quiz_list(character *ch) -> bool {
        int c = 0;

        for (size_t i = 0; i < quizzes_.size(); ++i) {
            for (size_t j = 0; j < quizzes_[i].size(); ++j, ++c) {
                ch->chat_packet(CHAT_TYPE_INFO, "%d %s %s", quizzes_[i][j].level, quizzes_[i][j].quiz.c_str(), quizzes_[i][j].answer ? lc_text("참") : lc_text("거짓"));
            }
        }

        ch->chat_packet(CHAT_TYPE_INFO, lc_text("총 퀴즈 수: %d"), c);
        return true;
    }

    void ox_event_manager::clear_quiz() {
        for (unsigned int i = 0; i < quizzes_.size(); ++i) {
            quizzes_[i].clear();
        }

        quizzes_.clear();
    }

    EVENTINFO(ox_event_info_data) {
        bool answer;

        ox_event_info_data()
            : answer(false) {
        }
    };

    EVENTFUNC(oxevent_timer) {
        static uint8_t flag = 0;
        auto *info = dynamic_cast<ox_event_info_data *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Ox event timer has no event info");
            return 0;
        }

        switch (flag) {
        case 0:
            send_notice_map(lc_text("10초뒤 판정하겠습니다."), OXEVENT_MAP_INDEX, true);
            flag++;
            return passes_per_sec(10);

        case 1:
            send_notice_map(lc_text("정답은"), OXEVENT_MAP_INDEX, true);

            if (info->answer == true) {
                get_ox_event_manager().check_answer(true);
                send_notice_map(lc_text("O 입니다"), OXEVENT_MAP_INDEX, true);
            } else {
                get_ox_event_manager().check_answer(false);
                send_notice_map(lc_text("X 입니다"), OXEVENT_MAP_INDEX, true);
            }

            send_notice_map(lc_text("5초 뒤 틀리신 분들을 바깥으로 이동 시키겠습니다."), OXEVENT_MAP_INDEX, true);

            flag++;
            return passes_per_sec(5);

        case 2:
            get_ox_event_manager().warp_to_audience();
            get_ox_event_manager().set_status(OXEVENT_CLOSE);
            send_notice_map(lc_text("다음 문제 준비해주세요."), OXEVENT_MAP_INDEX, true);
            flag = 0;
            break;
        }
        return 0;
    }

    auto ox_event_manager::quiz(unsigned char level, int timelimit) -> bool {
        if (quizzes_.size() == 0) return false;
        if (level > quizzes_.size()) level = quizzes_.size() - 1;
        if (quizzes_[level].size() <= 0) return false;

        if (timelimit < 0) timelimit = 30;

        int idx = number(0, quizzes_[level].size() - 1);

        send_notice_map(lc_text("문제 입니다."), OXEVENT_MAP_INDEX, true);
        send_notice_map(quizzes_[level][idx].quiz.c_str(), OXEVENT_MAP_INDEX, true);
        send_notice_map(lc_text("맞으면 O, 틀리면 X로 이동해주세요"), OXEVENT_MAP_INDEX, true);

        if (timed_event_ != nullptr) {
            event_cancel(&timed_event_);
        }

        auto *answer = alloc_event_info<ox_event_info_data>();

        answer->answer = quizzes_[level][idx].answer;

        timelimit -= 15;
        timed_event_ = event_create(oxevent_timer, answer, passes_per_sec(timelimit));

        set_status(OXEVENT_QUIZ);

        quizzes_[level].erase(quizzes_[level].begin() + idx);
        return true;
    }

    auto ox_event_manager::check_answer(bool answer) -> bool {
        if (attenders_.size() <= 0) return true;

        auto iter = attenders_.begin();
        decltype(attenders_.begin()) iter_tmp;

        misses_.clear();

        int rect[4];
        if (answer != true) {
            rect[0] = 892600;
            rect[1] = 22900;
            rect[2] = 896300;
            rect[3] = 26400;
        } else {
            rect[0] = 896600;
            rect[1] = 22900;
            rect[2] = 900300;
            rect[3] = 26400;
        }

        character *ch = nullptr;
        pixel_position pos;
        for (; iter != attenders_.end();) {
            ch = get_character_manager().find_by_pid(iter->second);
            if (ch != nullptr) {
                pos = ch->xyz();

                if (pos.x < rect[0] || pos.x > rect[2] || pos.y < rect[1] || pos.y > rect[3]) {
                    ch->effect_packet(SE_FAIL);
                    iter_tmp = iter;
                    iter++;
                    attenders_.erase(iter_tmp);
                    misses_.insert(std::make_pair(ch->player_id(), ch->player_id()));
                } else {
                    ch->chat_packet(CHAT_TYPE_INFO, lc_text("정답입니다!"));
                    const std::string text = std::format(
                        "{} {} {}", number(0, 1) == 1 ? "cheer1" : "cheer2", (uint32_t)ch->get_vid(), 0);

                    const int len = text.size() + 1;

                    packet_gc_chat pack_chat;
                    pack_chat.header = HEADER_GC_CHAT;
                    pack_chat.size = sizeof(packet_gc_chat) + len;
                    pack_chat.type = CHAT_TYPE_COMMAND;
                    pack_chat.id = 0;

                    temp_buffer buf;
                    buf.write(&pack_chat, sizeof(packet_gc_chat));
                    buf.write(text.c_str(), len);

                    ch->packet_around(buf.read_peek(), buf.size());
                    ch->effect_packet(SE_SUCCESS);

                    ++iter;
                }
            } else {
                auto err = chars_.find(iter->first);
                if (err != chars_.end()) chars_.erase(err);

                auto err2 = misses_.find(iter->first);
                if (err2 != misses_.end()) misses_.erase(err2);

                iter_tmp = iter;
                ++iter;
                attenders_.erase(iter_tmp);
            }
        }
        return true;
    }

    void ox_event_manager::warp_to_audience() {
        if (misses_.size() <= 0) return;

        auto iter = misses_.begin();
        character *ch = nullptr;

        for (; iter != misses_.end(); ++iter) {
            ch = get_character_manager().find_by_pid(iter->second);

            if (ch != nullptr) {
                switch (number(0, 3)) {
                case 0:  ch->show(OXEVENT_MAP_INDEX, 896300, 28900); break;
                case 1:  ch->show(OXEVENT_MAP_INDEX, 890900, 28100); break;
                case 2:  ch->show(OXEVENT_MAP_INDEX, 896600, 20500); break;
                case 3:  ch->show(OXEVENT_MAP_INDEX, 902500, 28100); break;
                default: ch->show(OXEVENT_MAP_INDEX, 896300, 28900); break;
                }
            }
        }

        misses_.clear();
    }

    auto ox_event_manager::close_event() -> bool {
        if (timed_event_ != nullptr) {
            event_cancel(&timed_event_);
        }

        auto iter = chars_.begin();

        character *ch = nullptr;
        for (; iter != chars_.end(); ++iter) {
            ch = get_character_manager().find_by_pid(iter->second);

            if (ch != nullptr)
                ch->warp_set(empire_start_x(ch->empire()), empire_start_y(ch->empire()));
        }

        chars_.clear();

        return true;
    }

    auto ox_event_manager::log_winner() -> bool {
        auto iter = attenders_.begin();

        for (; iter != attenders_.end(); ++iter) {
            character *ch = get_character_manager().find_by_pid(iter->second);

            if (ch)
                get_log_manager().char_log(ch, 0, "OXEVENT", "LastManStanding");
        }

        return true;
    }

    auto ox_event_manager::give_item_to_attender(uint32_t item_vnum, uint8_t count) -> bool {
        auto iter = attenders_.begin();

        for (; iter != attenders_.end(); ++iter) {
            character *ch = get_character_manager().find_by_pid(iter->second);

            if (ch) {
                ch->auto_give_item(item_vnum, count);
                get_log_manager().item_log(ch->player_id(), 0, count, item_vnum, "OXEVENT_REWARD", "", ch->get_desc()->host_name(), item_vnum);
            }
        }

        return true;
    }
}
