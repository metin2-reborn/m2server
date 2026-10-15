#pragma once

// System Includes

//==================================================================================================
// CState

// CState Class
#include <cassert>

namespace metin2::game::inline characters {

class state
{
	public:
		// Destructor
		virtual ~state()
		{
		}

		// State Functions
		virtual void execute_begin_state() = 0;
		virtual void execute_state() = 0;
		virtual void execute_end_state() = 0;
};

//==================================================================================================
// CStateTemplate

// CStateTemplate Class
template <class T>
class state_template : public state
{
	public:
		typedef void (T::*pfnstate) (void);

	protected:
		T *             instance_;            // Instance Pointer
		pfnstate        pfn_begin_state_;        // State Function Pointer
		pfnstate        pfn_state_;             // State Function Pointer
		pfnstate        pfn_end_state_;          // State Function Pointer

	public:
		// Constructor
		state_template() : instance_(nullptr), pfn_begin_state_(nullptr), pfn_state_(nullptr), pfn_end_state_(nullptr)
		{
		}

		// Initialize Functions
		void set(T *instance, pfnstate begin_state, pfnstate state, pfnstate end_state)
		{
			// Set Instance
			assert(instance);
			instance_ = instance;

			// Set Function Pointers
			assert(begin_state);
			pfn_begin_state_ = begin_state;

			assert(state);
			pfn_state_ = state;

			assert(end_state);
			pfn_end_state_ = end_state;
		}

		// State Functions
		void execute_begin_state() override
		{
			// Begin State
			assert(instance_ && pfn_begin_state_);
			(instance_->*pfn_begin_state_)();
		}

		void execute_state() override
		{
			// State
			assert(instance_ && pfn_state_);
			(instance_->*pfn_state_)();
		}

		void execute_end_state() override
		{
			// End State
			assert(instance_ && pfn_end_state_);
			(instance_->*pfn_end_state_)();
		}
};
}
