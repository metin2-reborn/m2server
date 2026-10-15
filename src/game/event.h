/*
 *    Filename: event.h
 * Description: Events (timed event)
 *
 *      Author: Kim Han-ju (aka. Biyeop, Cronan), Song Young-jin (aka. myevan, Bitjaru)
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

/**
 * Base class for all event info data
 */

namespace metin2::game {
struct event_info_data 
{
	event_info_data() {}
	virtual ~event_info_data() {}

};
	
typedef struct event EVENT;

void event_add_ref(EVENT * p);
void event_release(EVENT * p);

// Shared ownership of an event through its intrusive reference count.
class event_ptr {
  public:
    event_ptr() = default;
    event_ptr(std::nullptr_t) {}
    event_ptr(EVENT *p) : m_p(p) {
        if (m_p) {
            event_add_ref(m_p);
        }
    }
    event_ptr(const event_ptr &other) : event_ptr(other.m_p) {}
    event_ptr(event_ptr &&other) noexcept : m_p(std::exchange(other.m_p, nullptr)) {}
    ~event_ptr() {
        if (m_p) {
            event_release(m_p);
        }
    }

    auto operator=(event_ptr other) noexcept -> event_ptr & {
        std::swap(m_p, other.m_p);
        return *this;
    }

    void reset() { event_ptr().swap(*this); }
    void swap(event_ptr &other) noexcept { std::swap(m_p, other.m_p); }

    [[nodiscard]] auto get() const -> EVENT * { return m_p; }
    auto operator->() const -> EVENT * { return m_p; }
    auto operator*() const -> EVENT & { return *m_p; }
    explicit operator bool() const { return m_p != nullptr; }

    friend auto operator==(const event_ptr &a, const event_ptr &b) -> bool { return a.m_p == b.m_p; }
    friend auto operator==(const event_ptr &a, const EVENT *b) -> bool { return a.m_p == b; }
    friend auto operator==(const event_ptr &a, std::nullptr_t) -> bool { return a.m_p == nullptr; }

  private:
    EVENT *m_p = nullptr;
};

typedef event_ptr LPEVENT;
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
}
