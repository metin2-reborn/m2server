#include <metin2/game/app/event_queue.h>

#include <metin2/game/app/event.h>

#include <climits>
#include <utility>

namespace metin2::game::inline app {

    auto event_queue::enqueue(event_ptr data, int duration, int pulse) -> queue_element * {
        const int key = duration + pulse;
        auto it = queue_.emplace(key, queue_element{std::move(data), pulse, key, false});
        return &it->second;
    }

    auto event_queue::dequeue() -> element {
        if (queue_.empty())
            return {};

        return queue_.extract(queue_.begin());
    }

    auto event_queue::top_key() -> int {
        if (queue_.empty())
            return INT_MAX;

        return queue_.begin()->first;
    }

    auto event_queue::size() -> int {
        return static_cast<int>(queue_.size());
    }
}
