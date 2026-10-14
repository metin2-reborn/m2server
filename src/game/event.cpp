/*
 *    Filename: event.c
 * Description: Events (timed event)
 *
 *      Author: Kim Han-ju (aka. Biyeop, Cronan), Song Young-jin (aka. myevan, Bitjaru)
 */
#include "event.h"

#include "event_queue.h"
#include "main.h"

#include <libthecore/log.h>
#include <libthecore/main.h>

#include <cassert>
#include <cstdint>



static CEventQueue cxx_q;

/* Creates and returns an event */
LPEVENT event_create(TEVENTFUNC func, event_info_data* info, int32_t when)
{
	LPEVENT new_event = nullptr;

	/* Must be called only after at least the next pulse has passed. */
	if (when < 1)
		when = 1;

	new_event = new event;

	assert(nullptr != new_event);

	new_event->func = func;
	new_event->info	= info;
	new_event->q_el	= cxx_q.Enqueue(new_event, when, thecore_heart->pulse);
	new_event->is_processing = false;
	new_event->is_force_to_end = false;

	return (new_event);
}

/* Removes an event from the system */
void event_cancel(LPEVENT * ppevent)
{
	LPEVENT event;

	if (!ppevent)
	{
		sys_err("null pointer");
		return;
	}

	if (!(event = *ppevent))
		return;

	if (event->is_processing)
	{
		event->is_force_to_end = true;

		if (event->q_el)
			event->q_el->bCancel = true;

		*ppevent = nullptr;
		return;
	}

	// Already cancelled?
	if (!event->q_el)
	{
		*ppevent = nullptr;
		return;
	}

	if (event->q_el->bCancel)
	{
		*ppevent = nullptr;
		return;
	}

	event->q_el->bCancel = true;

	*ppevent = nullptr;
}

void event_reset_time(LPEVENT event, int32_t when)
{
	if (!event->is_processing)
	{
		if (event->q_el)
			event->q_el->bCancel = true;

		event->q_el = cxx_q.Enqueue(event, when, thecore_heart->pulse);
	}
}

/* Runs the events whose execution time has arrived */
int event_process(int pulse)
{
	int32_t	new_time;
	int		num_events = 0;

	// If the current pulse is less than the time of the head of event_q (the event queue), the loop 
	// does not run.
	while (pulse >= cxx_q.GetTopKey())
	{
		TQueueElement * pElem = cxx_q.Dequeue();

		if (pElem->bCancel)
		{
			cxx_q.Delete(pElem);
			continue;
		}

		new_time = pElem->iKey;

		LPEVENT the_event = pElem->pvData;
		int32_t processing_time = event_processing_time(the_event);
		cxx_q.Delete(pElem);

		/*
		 * The return value is the new time; if it is greater than 0 the event is re-added. 
		 * When returning a value above 0, be careful not to free the memory allocated
		 * to the event.
		 */
		the_event->is_processing = true;

		if (!the_event->info)
		{
			the_event->q_el = nullptr;
			ContinueOnFatalError();
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
				the_event->q_el = cxx_q.Enqueue(the_event, new_time, pulse);
				the_event->is_processing = false;
			}
		}

		++num_events;
	}

	return num_events;
}

/* Returns the event's execution time in pulses */
int32_t event_processing_time(LPEVENT event)
{
	int32_t start_time;

	if (!event->q_el)
		return 0;

	start_time = event->q_el->iStartTime;
	return (thecore_heart->pulse - start_time);
}

/* Returns the event's remaining time in pulses */
int32_t event_time(LPEVENT event)
{
	int32_t when;

	if (!event->q_el)
		return 0;

	when = event->q_el->iKey;
	return (when - thecore_heart->pulse);
}

/* Removes all events */
void event_destroy(void)
{
	TQueueElement * pElem;

	while ((pElem = cxx_q.Dequeue()))
	{
		LPEVENT the_event = (LPEVENT) pElem->pvData;

		if (!pElem->bCancel)
		{
			// no op here
		}

		cxx_q.Delete(pElem);
	}
}

int event_count()
{
	return cxx_q.Size();
}

void intrusive_ptr_add_ref(EVENT* p) {
	++(p->ref_count);
}

void intrusive_ptr_release(EVENT* p) {
	if ( --(p->ref_count) == 0 ) {
		delete p;
	}
}
