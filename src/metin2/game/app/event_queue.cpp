/*
 *    Filename: queue.c
 * Description: Queue handling
 *
 *      Author: Kim Han-ju (aka. Biyeop, Cronan), Song Young-jin (aka. myevan, Bitjaru)
 */
#include <metin2/game/app/event_queue.h>

#include <metin2/game/app/event.h>

#include <climits>

namespace metin2::game::inline app {

event_queue::~event_queue()
{
	for (auto & [key, pElem] : queue_)
		delete_(pElem);
}

queue_element * event_queue::enqueue(event_ptr pvData, int duration, int pulse)
{
	queue_element * pElem = new queue_element;

	pElem->pv_data = pvData;
	pElem->start_time = pulse;
	pElem->key = duration + pulse;
	pElem->cancel = false;

	queue_.emplace(pElem->key, pElem);
	return pElem;
}

queue_element * event_queue::dequeue()
{
	if (queue_.empty())
		return nullptr;

	const auto first = queue_.begin();
	queue_element * pElem = first->second;
	queue_.erase(first);
	return pElem;
}

void event_queue::delete_(queue_element * pElem)
{
	delete pElem;
}

int event_queue::top_key()
{
	if (queue_.empty())
		return INT_MAX;

	return queue_.begin()->first;
}

int event_queue::size()
{
	return static_cast<int>(queue_.size());
}
}
