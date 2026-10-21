
#include <metin2/game/characters/fsm.h>

#include <metin2/game/characters/state.h>

#include <cassert>

namespace metin2::game::inline characters {

    fsm::fsm() {
        state_initial_.set(this, &fsm::begin_state_initial, &fsm::state_initial, &fsm::end_state_initial);

        current_state_ = static_cast<state *>(&state_initial_);
        new_state_ = nullptr;
    }

    void fsm::update() {
        if (new_state_) {
            if (nullptr != current_state_) {
                current_state_->execute_end_state();
            }

            current_state_ = new_state_;
            new_state_ = nullptr;

            current_state_->execute_begin_state();
        }

        current_state_->execute_state();
    }

    auto fsm::is_state(state &state_value) const -> bool {
        return (current_state_ == &state_value);
    }

    auto fsm::goto_state(state &new_state) -> bool {
        if (is_state(new_state) && new_state_ == &new_state)
            return true;

        new_state_ = &new_state;
        return true;
    }
}
