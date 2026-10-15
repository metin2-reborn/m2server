#pragma once



#include <cassert>

namespace metin2::game::inline characters {

class state
{
	public:
		virtual ~state()
		{
		}

		virtual void execute_begin_state() = 0;
		virtual void execute_state() = 0;
		virtual void execute_end_state() = 0;
};


template <class T>
class state_template : public state
{
	public:
		typedef void (T::*pfnstate) (void);

	protected:
		T *             instance_;
		pfnstate        pfn_begin_state_;
		pfnstate        pfn_state_;
		pfnstate        pfn_end_state_;

	public:
		state_template() : instance_(nullptr), pfn_begin_state_(nullptr), pfn_state_(nullptr), pfn_end_state_(nullptr)
		{
		}

		void set(T *instance, pfnstate begin_state, pfnstate state, pfnstate end_state)
		{
			assert(instance);
			instance_ = instance;

			assert(begin_state);
			pfn_begin_state_ = begin_state;

			assert(state);
			pfn_state_ = state;

			assert(end_state);
			pfn_end_state_ = end_state;
		}

		void execute_begin_state() override
		{
			assert(instance_ && pfn_begin_state_);
			(instance_->*pfn_begin_state_)();
		}

		void execute_state() override
		{
			assert(instance_ && pfn_state_);
			(instance_->*pfn_state_)();
		}

		void execute_end_state() override
		{
			assert(instance_ && pfn_end_state_);
			(instance_->*pfn_end_state_)();
		}
};
}
