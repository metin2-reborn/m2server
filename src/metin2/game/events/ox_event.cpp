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
#include <metin2/proto/length.h>

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
        map_char_.clear();
        map_attender_.clear();
        vec_quiz_.clear();

        set_status(OXEVENT_FINISH);

        return true;
    }

    void ox_event_manager::destroy() {
        close_event();

        map_char_.clear();
        map_attender_.clear();
        vec_quiz_.clear();

        set_status(OXEVENT_FINISH);
    }

    auto ox_event_manager::status() -> ox_event_status {
        uint8_t ret = quest::quest_manager::instance().event_flag("oxevent_status");

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
        quest::quest_manager::instance().request_set_event_flag("oxevent_status", val);
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

        map_char_.insert(std::make_pair(pid, pid));
        map_attender_.insert(std::make_pair(pid, pid));

        return true;
    }

    auto ox_event_manager::enter_audience(character *ch) -> bool {
        uint32_t pid = ch->player_id();

        map_char_.insert(std::make_pair(pid, pid));

        return true;
    }

    auto ox_event_manager::add_quiz(unsigned char level, const char *question, bool answer) -> bool {
        if (vec_quiz_.size() < (size_t)level + 1)
            vec_quiz_.resize(level + 1);

        struct tag_quiz tmpQuiz;

        tmpQuiz.level = level;
        tmpQuiz.quiz = question;
        tmpQuiz.answer = answer;

        vec_quiz_[level].push_back(tmpQuiz);
        return true;
    }

    auto ox_event_manager::show_quiz_list(character *ch) -> bool {
        int c = 0;

        for (size_t i = 0; i < vec_quiz_.size(); ++i) {
            for (size_t j = 0; j < vec_quiz_[i].size(); ++j, ++c) {
                ch->chat_packet(CHAT_TYPE_INFO, "%d %s %s", vec_quiz_[i][j].level, vec_quiz_[i][j].quiz.c_str(), vec_quiz_[i][j].answer ? lc_text("참") : lc_text("거짓"));
            }
        }

        ch->chat_packet(CHAT_TYPE_INFO, lc_text("총 퀴즈 수: %d"), c);
        return true;
    }

    void ox_event_manager::clear_quiz() {
        for (unsigned int i = 0; i < vec_quiz_.size(); ++i) {
            vec_quiz_[i].clear();
        }

        vec_quiz_.clear();
    }

    EVENTINFO(ox_event_info_data) {
        bool answer;

        ox_event_info_data()
            : answer(false) {
        }
    };

    EVENTFUNC(oxevent_timer) {
        static uint8_t flag = 0;
        ox_event_info_data *info = dynamic_cast<ox_event_info_data *>(event->info);

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
                ox_event_manager::instance().check_answer(true);
                send_notice_map(lc_text("O 입니다"), OXEVENT_MAP_INDEX, true);
            } else {
                ox_event_manager::instance().check_answer(false);
                send_notice_map(lc_text("X 입니다"), OXEVENT_MAP_INDEX, true);
            }

            send_notice_map(lc_text("5초 뒤 틀리신 분들을 바깥으로 이동 시키겠습니다."), OXEVENT_MAP_INDEX, true);

            flag++;
            return passes_per_sec(5);

        case 2:
            ox_event_manager::instance().warp_to_audience();
            ox_event_manager::instance().set_status(OXEVENT_CLOSE);
            send_notice_map(lc_text("다음 문제 준비해주세요."), OXEVENT_MAP_INDEX, true);
            flag = 0;
            break;
        }
        return 0;
    }

    auto ox_event_manager::quiz(unsigned char level, int timelimit) -> bool {
        if (vec_quiz_.size() == 0) return false;
        if (level > vec_quiz_.size()) level = vec_quiz_.size() - 1;
        if (vec_quiz_[level].size() <= 0) return false;

        if (timelimit < 0) timelimit = 30;

        int idx = number(0, vec_quiz_[level].size() - 1);

        send_notice_map(lc_text("문제 입니다."), OXEVENT_MAP_INDEX, true);
        send_notice_map(vec_quiz_[level][idx].quiz.c_str(), OXEVENT_MAP_INDEX, true);
        send_notice_map(lc_text("맞으면 O, 틀리면 X로 이동해주세요"), OXEVENT_MAP_INDEX, true);

        if (timed_event_ != nullptr) {
            event_cancel(&timed_event_);
        }

        ox_event_info_data *answer = alloc_event_info<ox_event_info_data>();

        answer->answer = vec_quiz_[level][idx].answer;

        timelimit -= 15;
        timed_event_ = event_create(oxevent_timer, answer, passes_per_sec(timelimit));

        set_status(OXEVENT_QUIZ);

        vec_quiz_[level].erase(vec_quiz_[level].begin() + idx);
        return true;
    }

    auto ox_event_manager::check_answer(bool answer) -> bool {
        if (map_attender_.size() <= 0) return true;

        auto iter = map_attender_.begin();
        decltype(map_attender_.begin()) iter_tmp;

        map_miss_.clear();

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

        character *pkChar = nullptr;
        pixel_position pos;
        for (; iter != map_attender_.end();) {
            pkChar = character_manager::instance().find_by_pid(iter->second);
            if (pkChar != nullptr) {
                pos = pkChar->xyz();

                if (pos.x < rect[0] || pos.x > rect[2] || pos.y < rect[1] || pos.y > rect[3]) {
                    pkChar->effect_packet(SE_FAIL);
                    iter_tmp = iter;
                    iter++;
                    map_attender_.erase(iter_tmp);
                    map_miss_.insert(std::make_pair(pkChar->player_id(), pkChar->player_id()));
                } else {
                    pkChar->chat_packet(CHAT_TYPE_INFO, lc_text("정답입니다!"));
                    const std::string text = std::format(
                        "{} {} {}", number(0, 1) == 1 ? "cheer1" : "cheer2", (uint32_t)pkChar->get_vid(), 0);

                    const int len = text.size() + 1;

                    packet_gc_chat pack_chat;
                    pack_chat.header = HEADER_GC_CHAT;
                    pack_chat.size = sizeof(packet_gc_chat) + len;
                    pack_chat.type = CHAT_TYPE_COMMAND;
                    pack_chat.id = 0;

                    temp_buffer buf;
                    buf.write(&pack_chat, sizeof(packet_gc_chat));
                    buf.write(text.c_str(), len);

                    pkChar->packet_around(buf.read_peek(), buf.size());
                    pkChar->effect_packet(SE_SUCCESS);

                    ++iter;
                }
            } else {
                auto err = map_char_.find(iter->first);
                if (err != map_char_.end()) map_char_.erase(err);

                auto err2 = map_miss_.find(iter->first);
                if (err2 != map_miss_.end()) map_miss_.erase(err2);

                iter_tmp = iter;
                ++iter;
                map_attender_.erase(iter_tmp);
            }
        }
        return true;
    }

    void ox_event_manager::warp_to_audience() {
        if (map_miss_.size() <= 0) return;

        auto iter = map_miss_.begin();
        character *pkChar = nullptr;

        for (; iter != map_miss_.end(); ++iter) {
            pkChar = character_manager::instance().find_by_pid(iter->second);

            if (pkChar != nullptr) {
                switch (number(0, 3)) {
                case 0:  pkChar->show(OXEVENT_MAP_INDEX, 896300, 28900); break;
                case 1:  pkChar->show(OXEVENT_MAP_INDEX, 890900, 28100); break;
                case 2:  pkChar->show(OXEVENT_MAP_INDEX, 896600, 20500); break;
                case 3:  pkChar->show(OXEVENT_MAP_INDEX, 902500, 28100); break;
                default: pkChar->show(OXEVENT_MAP_INDEX, 896300, 28900); break;
                }
            }
        }

        map_miss_.clear();
    }

    auto ox_event_manager::close_event() -> bool {
        if (timed_event_ != nullptr) {
            event_cancel(&timed_event_);
        }

        auto iter = map_char_.begin();

        character *pkChar = nullptr;
        for (; iter != map_char_.end(); ++iter) {
            pkChar = character_manager::instance().find_by_pid(iter->second);

            if (pkChar != nullptr)
                pkChar->warp_set(empire_start_x(pkChar->empire()), empire_start_y(pkChar->empire()));
        }

        map_char_.clear();

        return true;
    }

    auto ox_event_manager::log_winner() -> bool {
        auto iter = map_attender_.begin();

        for (; iter != map_attender_.end(); ++iter) {
            character *pkChar = character_manager::instance().find_by_pid(iter->second);

            if (pkChar)
                log_manager::instance().char_log(pkChar, 0, "OXEVENT", "LastManStanding");
        }

        return true;
    }

    auto ox_event_manager::give_item_to_attender(uint32_t item_vnum, uint8_t count) -> bool {
        auto iter = map_attender_.begin();

        for (; iter != map_attender_.end(); ++iter) {
            character *pkChar = character_manager::instance().find_by_pid(iter->second);

            if (pkChar) {
                pkChar->auto_give_item(item_vnum, count);
                log_manager::instance().item_log(pkChar->player_id(), 0, count, item_vnum, "OXEVENT_REWARD", "", pkChar->get_desc()->host_name(), item_vnum);
            }
        }

        return true;
    }
}
