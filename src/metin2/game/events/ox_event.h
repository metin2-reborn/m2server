#pragma once

#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace metin2::game::inline events {

    inline constexpr int OXEVENT_MAP_INDEX = 113;
    struct tag_quiz {
        char level;
        std::string quiz;
        bool answer;
    };

    enum ox_event_status {
        OXEVENT_FINISH = 0,
        OXEVENT_OPEN = 1,
        OXEVENT_CLOSE = 2,
        OXEVENT_QUIZ = 3,

        OXEVENT_ERR = 0xff
    };

    class ox_event_manager {
      private:
        std::map<uint32_t, uint32_t> chars_;
        std::map<uint32_t, uint32_t> attenders_;
        std::map<uint32_t, uint32_t> misses_;

        std::vector<std::vector<tag_quiz>> quizzes_;

        event_ptr timed_event_;

      protected:
        auto check_answer() -> bool;

        auto enter_audience(character *ch) -> bool;
        auto enter_attender(character *ch) -> bool;

      public:
        auto initialize() -> bool;
        void destroy();

        auto status() -> ox_event_status;
        void set_status(ox_event_status status);

        auto enter(character *ch) -> bool;

        auto close_event() -> bool;

        void clear_quiz();
        auto add_quiz(unsigned char level, const char *question, bool answer) -> bool;
        auto show_quiz_list(character *ch) -> bool;

        auto quiz(unsigned char level, int timelimit) -> bool;
        auto give_item_to_attender(uint32_t item_vnum, uint8_t count) -> bool;

        auto check_answer(bool answer) -> bool;
        void warp_to_audience();

        auto log_winner() -> bool;

        auto attender_count() -> uint32_t { return attenders_.size(); }
    };

    inline auto get_ox_event_manager() -> ox_event_manager & {
        static ox_event_manager instance;
        return instance;
    }
}
