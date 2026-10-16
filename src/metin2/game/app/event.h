#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace metin2::game::inline app {
    struct event_info_data {
        virtual ~event_info_data() = default;
    };

    struct scheduled_event;
    using event_ptr = std::shared_ptr<scheduled_event>;

    typedef int32_t (*event_func)(event_ptr event, int32_t processing_time);

#define EVENTFUNC(name) auto(name)(event_ptr event, int32_t processing_time)->int32_t
#define EVENTINFO(name) struct name : public event_info_data

    struct queue_element;

    struct scheduled_event {
        event_func func = nullptr;
        std::unique_ptr<event_info_data> info;
        queue_element *q_el = nullptr;
        bool is_force_to_end = false;
        bool is_processing = false;
    };

    template <class T>
    auto alloc_event_info() -> T * {
        return new T;
    }

    extern void event_destroy();
    extern auto event_process(int pulse) -> int;
    extern auto event_count() -> int;

    extern auto event_create(event_func func, event_info_data *info, int32_t when) -> event_ptr;
    extern void event_cancel(event_ptr *ppevent);
    extern auto event_processing_time(event_ptr event) -> int32_t;
    extern auto event_time(event_ptr event) -> int32_t;
    extern void event_reset_time(event_ptr event, int32_t when);
}
