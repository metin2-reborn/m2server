#pragma once

#include <metin2/game/characters/state.h>

namespace metin2::game::inline characters {

    class fsm {
      protected:
        state *current_state_;
        state *new_state_;
        state_template<fsm> state_initial_;

      public:
        fsm();

        virtual ~fsm() = default;

        virtual void update();

        auto is_state(state &state_value) const -> bool;
        auto goto_state(state &new_state) -> bool;

        virtual void begin_state_initial() {}
        virtual void state_initial() {}
        virtual void end_state_initial() {}
    };
}
