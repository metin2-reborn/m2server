#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>


namespace metin2::game::inline app {
struct event_info_data 
{
	event_info_data() {}
	virtual ~event_info_data() {}

};
	
struct scheduled_event;

void event_add_ref(scheduled_event * p);
void event_release(scheduled_event * p);

class event_ptr {
  public:
    event_ptr() = default;
    event_ptr(std::nullptr_t) {}
    event_ptr(scheduled_event *p) : p_(p) {
        if (p_) {
            event_add_ref(p_);
        }
    }
    event_ptr(const event_ptr &other) : event_ptr(other.p_) {}
    event_ptr(event_ptr &&other) noexcept : p_(std::exchange(other.p_, nullptr)) {}
    ~event_ptr() {
        if (p_) {
            event_release(p_);
        }
    }

    auto operator=(event_ptr other) noexcept -> event_ptr & {
        std::swap(p_, other.p_);
        return *this;
    }

    void reset() { event_ptr().swap(*this); }
    void swap(event_ptr &other) noexcept { std::swap(p_, other.p_); }

    [[nodiscard]] auto get() const -> scheduled_event * { return p_; }
    auto operator->() const -> scheduled_event * { return p_; }
    auto operator*() const -> scheduled_event & { return *p_; }
    explicit operator bool() const { return p_ != nullptr; }

    friend auto operator==(const event_ptr &a, const event_ptr &b) -> bool { return a.p_ == b.p_; }
    friend auto operator==(const event_ptr &a, const scheduled_event *b) -> bool { return a.p_ == b; }
    friend auto operator==(const event_ptr &a, std::nullptr_t) -> bool { return a.p_ == nullptr; }

  private:
    scheduled_event *p_ = nullptr;
};

typedef int32_t (*event_func) (event_ptr event, int32_t processing_time);

#define EVENTFUNC(name)	int32_t (name) (event_ptr event, int32_t processing_time)
#define EVENTINFO(name) struct name : public event_info_data

struct queue_element;

struct scheduled_event
{
	scheduled_event() : func(nullptr), info(nullptr), q_el(nullptr), ref_count(0) {}
	~scheduled_event() {
		if (info != nullptr) {
			delete info;
		}
	}
	event_func			func;
	event_info_data* 	info;
	queue_element *		q_el;
	char				is_force_to_end;
	char				is_processing;

	size_t ref_count;
};

template<class T>
T* alloc_event_info() {
	return new T;
}

extern void		event_destroy();
extern int		event_process(int pulse);
extern int		event_count();

extern event_ptr	event_create(event_func func, event_info_data* info, int32_t when);
extern void		event_cancel(event_ptr * ppevent);
extern int32_t		event_processing_time(event_ptr event);
extern int32_t		event_time(event_ptr event);
extern void		event_reset_time(event_ptr event, int32_t when);
}
