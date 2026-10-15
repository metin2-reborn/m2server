/*
 *    Filename: event.c
 * Description: Events (timed event)
 *
 *      Author: Kim Han-ju (aka. Biyeop, Cronan), Song Young-jin (aka. myevan, Bitjaru)
 */
#include <metin2/game/app/event.h>

#include <metin2/game/app/event_queue.h>
#include <metin2/game/app/main.h>

#include <metin2/log/log.h>
#include <metin2/core/main.h>

#include <cassert>
#include <cstdint>

namespace metin2::game::inline app {



static event_queue cxx_q;

/* Creates and returns an event */
event_ptr event_create(event_func func, event_info_data* info, int32_t when)
{
	event_ptr new_event = nullptr;

	/* Must be called only after at least the next pulse has passed. */
	if (when < 1)
		when = 1;

	new_event = new scheduled_event;

	assert(nullptr != new_event);

	new_event->func = func;
	new_event->info	= info;
	new_event->q_el	= cxx_q.enqueue(new_event, when, thecore_heart->pulse);
	new_event->is_processing = false;
	new_event->is_force_to_end = false;

	return (new_event);
}

/* Removes an event from the system */
void event_cancel(event_ptr * ppevent)
{
	event_ptr event;

	if (!ppevent)
	{
		metin2::log::error("null pointer");
		return;
	}

	if (!(event = *ppevent))
		return;

	if (event->is_processing)
	{
		event->is_force_to_end = true;

		if (event->q_el)
			event->q_el->cancel = true;

		*ppevent = nullptr;
		return;
	}

	// Already cancelled?
	if (!event->q_el)
	{
		*ppevent = nullptr;
		return;
	}

	if (event->q_el->cancel)
	{
		*ppevent = nullptr;
		return;
	}

	event->q_el->cancel = true;

	*ppevent = nullptr;
}

void event_reset_time(event_ptr event, int32_t when)
{
	if (!event->is_processing)
	{
		if (event->q_el)
			event->q_el->cancel = true;

		event->q_el = cxx_q.enqueue(event, when, thecore_heart->pulse);
	}
}

/* Runs the events whose execution time has arrived */
int event_process(int pulse)
{
	int32_t	new_time;
	int		num_events = 0;

	// If the current pulse is less than the time of the head of event_q (the event queue), the loop 
	// does not run.
	while (pulse >= cxx_q.top_key())
	{
		queue_element * pElem = cxx_q.dequeue();

		if (pElem->cancel)
		{
			cxx_q.remove(pElem);
			continue;
		}

		new_time = pElem->key;

		event_ptr the_event = pElem->pv_data;
		int32_t processing_time = event_processing_time(the_event);
		cxx_q.remove(pElem);

		/*
		 * The return value is the new time; if it is greater than 0 the event is re-added. 
		 * When returning a value above 0, be careful not to free the memory allocated
		 * to the event.
		 */
		the_event->is_processing = true;

		if (!the_event->info)
		{
			the_event->q_el = nullptr;
			continue_on_fatal_error();
		}
		else
		{
			new_time = (the_event->func) (the_event, processing_time);
			
			if (new_time <= 0 || the_event->is_force_to_end)
			{
				the_event->q_el = nullptr;
			}
			else
			{
				the_event->q_el = cxx_q.enqueue(the_event, new_time, pulse);
				the_event->is_processing = false;
			}
		}

		++num_events;
	}

	return num_events;
}

/* Returns the event's execution time in pulses */
int32_t event_processing_time(event_ptr event)
{
	int32_t start_time;

	if (!event->q_el)
		return 0;

	start_time = event->q_el->start_time;
	return (thecore_heart->pulse - start_time);
}

/* Returns the event's remaining time in pulses */
int32_t event_time(event_ptr event)
{
	int32_t when;

	if (!event->q_el)
		return 0;

	when = event->q_el->key;
	return (when - thecore_heart->pulse);
}

/* Removes all events */
void event_destroy(void)
{
	queue_element * pElem;

	while ((pElem = cxx_q.dequeue()))
	{
		event_ptr the_event = (event_ptr) pElem->pv_data;

		if (!pElem->cancel)
		{
			// no op here
		}

		cxx_q.remove(pElem);
	}
}

int event_count()
{
	return cxx_q.size();
}

void event_add_ref(scheduled_event* p) {
	++(p->ref_count);
}

void event_release(scheduled_event* p) {
	if ( --(p->ref_count) == 0 ) {
		delete p;
	}
}
}
