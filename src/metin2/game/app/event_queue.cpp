#include <metin2/game/app/event_queue.h>

#include <metin2/game/app/event.h>

#include <climits>

namespace metin2::game::inline app {

    event_queue::~event_queue() {
        for (auto &[key, pElem] : queue_)
            remove(pElem);
    }

    auto event_queue::enqueue(event_ptr data, int duration, int pulse) -> queue_element * {
        queue_element *pElem = new queue_element;

        pElem->pv_data = data;
        pElem->start_time = pulse;
        pElem->key = duration + pulse;
        pElem->cancel = false;

        queue_.emplace(pElem->key, pElem);
        return pElem;
    }

    auto event_queue::dequeue() -> queue_element * {
        if (queue_.empty())
            return nullptr;

        const auto first = queue_.begin();
        queue_element *pElem = first->second;
        queue_.erase(first);
        return pElem;
    }

    void event_queue::remove(queue_element *elem) {
        delete elem;
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
