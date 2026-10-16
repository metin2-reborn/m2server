#pragma once

#include <ctime>

namespace metin2::inline common {

    template <typename T>
    class cache {
      public:
        cache() = default;

        virtual ~cache() = default;

        auto get(bool update_time = true) -> T * {
            if (update_time) {
                last_update_time_ = std::time(nullptr);
            }
            return &data_;
        }

        void put(const T *data, bool skip_query = false) {
            data_ = *data;
            last_update_time_ = std::time(nullptr);

            if (!skip_query) {
                need_query_ = true;
            }
        }

        auto check_flush_timeout() const -> bool {
            return need_query_ && std::time(nullptr) - last_flush_time_ > expire_time_;
        }

        auto check_timeout() const -> bool {
            return std::time(nullptr) - last_update_time_ > expire_time_;
        }

        void flush() {
            if (!need_query_) {
                return;
            }

            on_flush();

            need_query_ = false;
            last_flush_time_ = std::time(nullptr);
        }

        virtual void on_flush() = 0;

      protected:
        T data_{};
        bool need_query_{false};
        std::time_t expire_time_{600};
        std::time_t last_update_time_{0};
        std::time_t last_flush_time_{std::time(nullptr)};
    };
}
