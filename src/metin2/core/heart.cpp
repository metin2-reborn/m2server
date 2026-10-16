#include <metin2/core/heart.h>

#include <metin2/log/log.h>

#include <chrono>
#include <thread>

namespace metin2::inline core {

    using clock_type = std::chrono::steady_clock;

    heart::heart(int fps)
        : opt_time_(std::chrono::duration_cast<clock_type::duration>(std::chrono::seconds(1)) / fps),
          last_time_(clock_type::now()),
          passes_per_sec_(fps) {
    }

    auto heart::idle() -> int {
        const clock_type::time_point now = clock_type::now();
        const clock_type::duration process_time = now - last_time_;

        int missed_pulse = 0;
        if (process_time >= opt_time_) {
            missed_pulse = static_cast<int>(process_time / opt_time_);
        }

        if (missed_pulse > 0) {
            last_time_ = now;
        } else {
            last_time_ += opt_time_;
            std::this_thread::sleep_until(last_time_);
        }

        ++missed_pulse;

        if (missed_pulse > (30 * passes_per_sec_)) {
            metin2::log::warn("Main loop is lagging, skipped {:d} seconds of pulses", missed_pulse / passes_per_sec_);
            missed_pulse = 30 * passes_per_sec_;
        }

        return missed_pulse;
    }
}
