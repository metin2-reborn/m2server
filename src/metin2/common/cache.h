#pragma once

#include <chrono>

namespace metin2::inline common {

    template <typename T>
    class cache {
      public:
        using clock = std::chrono::steady_clock;

        cache() = default;

        virtual ~cache() = default;

        auto get(bool update_time = true) -> T * {
            if (update_time) {
                last_update_time_ = clock::now();
            }
            return &data_;
        }

        void put(const T *data, bool skip_query = false) {
            data_ = *data;
            last_update_time_ = clock::now();

            if (!skip_query) {
                need_query_ = true;
            }
        }

        auto check_flush_timeout() const -> bool {
            return need_query_ && last_flush_time_ < clock::now() - expire_time_;
        }

        auto check_timeout() const -> bool {
            return last_update_time_ < clock::now() - expire_time_;
        }

        void flush() {
            if (!need_query_) {
                return;
            }

            on_flush();

            need_query_ = false;
            last_flush_time_ = clock::now();
        }

        virtual void on_flush() = 0;

      protected:
        T data_{};
        bool need_query_{false};
        std::chrono::seconds expire_time_{600};
        clock::time_point last_update_time_{clock::time_point::min()};
        clock::time_point last_flush_time_{clock::now()};
    };
}
