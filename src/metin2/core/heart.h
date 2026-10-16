#pragma once

#include <chrono>

namespace metin2::inline core {

    class heart {
      public:
        explicit heart(int fps);

        auto idle() -> int;

        auto pulse() const -> int { return pulse_; }
        auto next_pulse() -> int { return ++pulse_; }
        auto passes_per_sec() const -> int { return passes_per_sec_; }

      private:
        std::chrono::steady_clock::duration opt_time_;
        std::chrono::steady_clock::time_point last_time_;
        int passes_per_sec_;
        int pulse_ = 0;
    };
}
