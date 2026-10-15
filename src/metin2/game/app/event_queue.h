#pragma once

#include <metin2/game/app/event.h>

#include <map>

namespace metin2::game::inline app {

struct queue_element
{
	event_ptr	pv_data;
	int		start_time;
	int		key;
	bool	cancel;
};

// Pending events ordered by due pulse; events due on the same pulse run in the order queued.
class event_queue
{
	public:
		event_queue() = default;
		~event_queue();

		event_queue(const event_queue &) = delete;
		auto operator=(const event_queue &) -> event_queue & = delete;

		auto	enqueue(event_ptr data, int duration, int pulse) -> queue_element *;
		auto	dequeue() -> queue_element *;
		void		remove(queue_element * elem);
		auto		top_key() -> int;
		auto		size() -> int;

	private:
		std::multimap<int, queue_element *> queue_;
};
}
