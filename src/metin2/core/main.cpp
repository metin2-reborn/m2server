#include <metin2/core/main.h>

#include <metin2/core/internal.h>

#include <metin2/core/heart.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>

#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>

#include <unistd.h>

namespace metin2::inline core {

    heart *thecore_heart = nullptr;

    volatile std::sig_atomic_t shutdowned = false;
    unsigned int thecore_idle_time = 0;

    namespace {
        auto pid_init() -> bool {
            std::ofstream pid("pid");
            if (!pid) {
                metin2::log::error("Cannot write pid file ./pid");
                return false;
            }

            pid << getpid();
            metin2::log::info("Process started (pid {:d})", getpid());
            return true;
        }

        void pid_deinit() {
            std::remove("./pid");
            metin2::log::info("Process stopping, pid file removed");
        }
    }

    auto thecore_init(int fps) -> bool {
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
        ticks.fetch_add(1, std::memory_order_relaxed);
    }
}
