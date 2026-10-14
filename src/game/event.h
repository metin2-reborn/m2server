/*
 *    Filename: event.h
 * Description: Events (timed event)
 *
 *      Author: Kim Han-ju (aka. Biyeop, Cronan), Song Young-jin (aka. myevan, Bitjaru)
 */
#ifndef __INC_LIBTHECORE_EVENT_H__
#define __INC_LIBTHECORE_EVENT_H__


#include <boost/smart_ptr/intrusive_ptr.hpp>

#include <cstddef>
#include <cstdint>


/**
 * Base class for all event info data
 */
struct event_info_data 
{
	event_info_data() {}
	virtual ~event_info_data() {}

};
	
typedef struct event EVENT;
typedef boost::intrusive_ptr<EVENT> LPEVENT;
typedef int32_t (*TEVENTFUNC) (LPEVENT event, int32_t processing_time);

#define EVENTFUNC(name)	int32_t (name) (LPEVENT event, int32_t processing_time)
#define EVENTINFO(name) struct name : public event_info_data

struct TQueueElement;

struct event
{
	event() : func(nullptr), info(nullptr), q_el(nullptr), ref_count(0) {}
	~event() {
		if (info != nullptr) {
			delete info;
		}
	}
	TEVENTFUNC			func;
	event_info_data* 	info;
	TQueueElement *		q_el;
	char				is_force_to_end;
	char				is_processing;

	size_t ref_count;
};

extern void intrusive_ptr_add_ref(EVENT* p);
extern void intrusive_ptr_release(EVENT* p);

template<class T> // T should be a subclass of event_info_data
T* AllocEventInfo() {
	return new T;
}

extern void		event_destroy();
extern int		event_process(int pulse);
extern int		event_count();

extern LPEVENT	event_create(TEVENTFUNC func, event_info_data* info, int32_t when);
extern void		event_cancel(LPEVENT * event);			// Cancel event
extern int32_t		event_processing_time(LPEVENT event);	// Return execution time
extern int32_t		event_time(LPEVENT event);			// Return remaining time
extern void		event_reset_time(LPEVENT event, int32_t when);	// Reset execution time


#endif
