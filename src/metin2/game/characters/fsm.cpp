// Local Includes

#include <metin2/game/characters/fsm.h>

#include <metin2/game/characters/state.h>

#include <cassert>

namespace metin2::game::inline characters {

// Constructor
fsm::fsm()
{
	// Initialize States
	state_initial_.set(this, &fsm::begin_state_initial, &fsm::state_initial, &fsm::end_state_initial);

	// Initialize State Machine
	current_state_ = static_cast<state *>(&state_initial_);
	new_state_ = nullptr;
}

//======================================================================================================
// Global Functions

// Update
void fsm::update()
{
	// Check New State
	if (new_state_)
	{
		if (nullptr != current_state_)
		{
			current_state_->execute_end_state();
		}

		// Set New State
		current_state_ = new_state_;
		new_state_ = nullptr;

		// Execute Begin State
		current_state_->execute_begin_state();
	}

	// Execute State
	current_state_->execute_state();
}

//======================================================================================================
// State Functions

// Is State
bool fsm::is_state(state & State) const
{
	return (current_state_ == &State);
}

// Goto State
bool fsm::goto_state(state & NewState)
{
	if (is_state(NewState) && new_state_ == &NewState)
		return true;

	// Set New State
	new_state_ = &NewState;
	return true;
}
}
