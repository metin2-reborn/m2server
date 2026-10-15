#include <metin2/core/main.h>

#include <metin2/core/internal.h>

#include <metin2/core/heart.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>

#include <sys/random.h>
#include <unistd.h>

namespace metin2::inline core {

heart *thecore_heart = nullptr;

volatile std::sig_atomic_t shutdowned = false;
volatile std::sig_atomic_t tics = 0;
unsigned int thecore_idle_time = 0;

namespace {
    auto pid_init() -> bool {
        std::ofstream pid("pid");
        if (!pid) {
            metin2::log::error("cannot write the pid file");
            return false;
        }

        pid << getpid();
        metin2::log::info("start of pid {:d}", getpid());
        return true;
    }

    void pid_deinit() {
        std::remove("./pid");
        metin2::log::info("end of pid");
    }
}

auto thecore_init(int fps) -> bool {
    unsigned int seed;
    if (getrandom(&seed, sizeof(seed), 0) != sizeof(seed)) {
        metin2::log::error("getrandom failed: {}", std::strerror(errno));
        return false;
    }

    srandom(seed);
    signal_setup();

    if (!metin2::log::init(".") || !pid_init()) {
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
    metin2::log::shutdown();
}

auto thecore_pulse() -> int {
    return thecore_heart->pulse;
}

auto thecore_is_shutdowned() -> bool {
    return shutdowned;
}

void thecore_tick() {
    tics = tics + 1;
}
}
