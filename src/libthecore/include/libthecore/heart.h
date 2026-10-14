#ifndef __INC_LIBTHECORE_HEART_H__
#define __INC_LIBTHECORE_HEART_H__

#include <chrono>

// The main loop's clock: thecore_idle waits for the next of passes_per_sec pulses a second.
struct heart
{
    std::chrono::steady_clock::duration		opt_time;	// one pulse
    std::chrono::steady_clock::time_point	last_time;	// when the current pulse is due
    int						passes_per_sec;
    int						pulse;
};


#endif
