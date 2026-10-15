#include <libthecore/main.h>

#include "internal.h"

#include <libthecore/heart.h>
#include <libthecore/log.h>
#include <libthecore/utils.h>

#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

#include <sys/random.h>
#include <unistd.h>

heart *thecore_heart = nullptr;

volatile std::sig_atomic_t shutdowned = false;
volatile std::sig_atomic_t tics = 0;
unsigned int thecore_idle_time = 0;

namespace {
    auto pid_init() -> bool {
        std::ofstream pid("pid");
        if (!pid) {
            std::printf("pid_init(): could not open file for writing. (filename: ./pid)");
            sys_err("\nError writing pid file\n");
            return false;
        }

        pid << getpid();
        sys_err("\nStart of pid: %d\n", getpid());
        return true;
    }

    void pid_deinit() {
        std::remove("./pid");
        sys_err("\nEnd of pid\n");
    }
}

auto thecore_init(int fps) -> bool {
    unsigned int seed;
    if (getrandom(&seed, sizeof(seed), 0) != sizeof(seed)) {
        sys_err("getrandom failed: %s", std::strerror(errno));
        return false;
    }

    srandom(seed);
    signal_setup();

    if (!log_init() || !pid_init()) {
        return false;
    }

    thecore_heart = heart_new(fps);
    return true;
}

void thecore_shutdown() {
    shutdowned = true;
}

auto thecore_idle() -> int {
    thecore_tick();

    if (shutdowned) {
        return 0;
    }

    const uint32_t t = get_dword_time();
    const int pulses = heart_idle(thecore_heart);

    thecore_idle_time += get_dword_time() - t;

    return pulses;
}

void thecore_destroy() {
    pid_deinit();
    log_destroy();
}

auto thecore_pulse() -> int {
    return thecore_heart->pulse;
}

bool thecore_is_shutdowned() {
    return shutdowned;
}

void thecore_tick() {
    tics = tics + 1;
}
