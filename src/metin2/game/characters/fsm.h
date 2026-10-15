#pragma once

// Local Includes
#include <metin2/game/characters/state.h>

namespace metin2::game::inline characters {

// FSM Class
class fsm
{
	protected:
		state *		current_state_;	// Current State
		state *		new_state_;		// New State
		state_template<fsm>	state_initial_;		// Initial State

	public:
		// Constructor
		fsm();

		// Destructor
		virtual ~fsm() {}

		// Global Functions
		virtual void update();

		// State Functions
		bool is_state(state &state_value) const;
		bool goto_state(state &new_state);

		virtual void begin_state_initial() {}
		virtual void state_initial() {}
		virtual void end_state_initial() {}
};
}
