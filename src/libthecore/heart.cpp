/*
 *    Filename: heart.cpp
 * Description: the "heart", called once per fps tick.
 *
 *      Author: Biyeop aka. Cronan
 */
#include <libthecore/heart.h>

#include "internal.h"

#include <libthecore/log.h>

#include <chrono>
#include <thread>

using clock_type = std::chrono::steady_clock;

auto heart_new(int fps) -> heart * {
    auto ht = new heart{};
    ht->opt_time = std::chrono::duration_cast<clock_type::duration>(std::chrono::seconds(1)) / fps;
    ht->passes_per_sec = fps;
    ht->last_time = clock_type::now();
    return ht;
}

auto heart_idle(heart *ht) -> int {
    const clock_type::time_point now = clock_type::now();
    const clock_type::duration process_time = now - ht->last_time;

    // When the last pass took longer than a pulse, count the missed pulses and carry on
    // without sleeping; otherwise sleep until the next pulse is due.
    int missed_pulse = 0;
    if (process_time >= ht->opt_time) {
        missed_pulse = static_cast<int>(process_time / ht->opt_time);
    }

    // So busy we even missed a pulse, no time to sleep...
    // Pulse fps is off anyway; matching it exactly isn't important.
    if (missed_pulse > 0) {
        ht->last_time = now;
    } else {
        ht->last_time += ht->opt_time;
        std::this_thread::sleep_until(ht->last_time);
    }

    ++missed_pulse;

    if (missed_pulse > (30 * ht->passes_per_sec)) {
        sys_err("losing %d seconds. (lag occured)", missed_pulse / ht->passes_per_sec);
        missed_pulse = 30 * ht->passes_per_sec;
    }

    return missed_pulse;
}
