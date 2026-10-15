#pragma once

#include <metin2/game/app/event.h>

#include <map>

namespace metin2::game::inline app {

struct queue_element
{
	event_ptr	pv_data;
	int		start_time;
	int		key;		// the pulse the event is due
	bool	cancel;
};

// Pending events ordered by due pulse; events due on the same pulse run in the order queued.
class event_queue
{
	public:
		event_queue() = default;
		~event_queue();

		event_queue(const event_queue &) = delete;
		event_queue & operator=(const event_queue &) = delete;

		queue_element *	enqueue(event_ptr data, int duration, int pulse);
		queue_element *	dequeue();
		void		remove(queue_element * elem);
		int		top_key();
		int		size();

	private:
		std::multimap<int, queue_element *> queue_;
};
}
