#include <metin2/game/app/event.h>

#include <metin2/game/app/event_queue.h>
#include <metin2/game/app/main.h>

#include <metin2/core/main.h>
#include <metin2/log/log.h>

#include <cassert>
#include <cstdint>

namespace metin2::game::inline app {

    static event_queue cxx_q;

    auto event_create(event_func func, event_info_data *info, int32_t when) -> event_ptr {
        if (when < 1)
            when = 1;

        auto new_event = std::make_shared<scheduled_event>();
        new_event->func = func;
        new_event->info.reset(info);
        new_event->q_el = cxx_q.enqueue(new_event, when, thecore_pulse());
        new_event->is_processing = false;
        new_event->is_force_to_end = false;

        return (new_event);
    }

    void event_cancel(event_ptr *ppevent) {
        event_ptr event;

        if (!ppevent) {
            metin2::log::error("Cannot cancel event: null event pointer");
            return;
        }

        if (!(event = *ppevent))
            return;

        if (event->is_processing) {
            event->is_force_to_end = true;

            if (event->q_el)
                event->q_el->cancel = true;

            *ppevent = nullptr;
            return;
        }

        if (!event->q_el) {
            *ppevent = nullptr;
            return;
        }

        if (event->q_el->cancel) {
            *ppevent = nullptr;
            return;
        }

        event->q_el->cancel = true;

        *ppevent = nullptr;
    }

    void event_reset_time(event_ptr event, int32_t when) {
        if (!event->is_processing) {
            if (event->q_el)
                event->q_el->cancel = true;

            event->q_el = cxx_q.enqueue(event, when, thecore_pulse());
        }
    }

    auto event_process(int pulse) -> int {
        int32_t new_time;
        int num_events = 0;

        while (pulse >= cxx_q.top_key()) {
            event_queue::element elem = cxx_q.dequeue();
            queue_element *elem_value = &elem.mapped();

            if (elem_value->cancel)
                continue;

            new_time = elem_value->key;

            event_ptr the_event = elem_value->data;
            int32_t processing_time = event_processing_time(the_event);
            elem = {};
            the_event->q_el = nullptr;

            // A positive return value re-adds the event; its memory must not be freed then.
            the_event->is_processing = true;

            if (!the_event->info) {
                the_event->q_el = nullptr;
                continue_on_fatal_error();
            } else {
                new_time = (the_event->func)(the_event, processing_time);

                if (new_time <= 0 || the_event->is_force_to_end) {
                    the_event->q_el = nullptr;
                } else {
                    the_event->q_el = cxx_q.enqueue(the_event, new_time, pulse);
                    the_event->is_processing = false;
                }
            }

            ++num_events;
        }

        return num_events;
    }

    auto event_processing_time(event_ptr event) -> int32_t {
        int32_t start_time;

        if (!event->q_el)
            return 0;

        start_time = event->q_el->start_time;
        return (thecore_pulse() - start_time);
    }

    auto event_time(event_ptr event) -> int32_t {
        int32_t when;

        if (!event->q_el)
            return 0;

        when = event->q_el->key;
        return (when - thecore_pulse());
    }

    void event_destroy(void) {
        while (event_queue::element elem = cxx_q.dequeue()) {
            queue_element &el = elem.mapped();

            if (el.data && el.data->q_el == &el)
                el.data->q_el = nullptr;
        }
    }

    auto event_count() -> int {
        return cxx_q.size();
    }
}
