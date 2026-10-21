#include <metin2/core/main.h>

#include <metin2/core/heart.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <optional>

#include <unistd.h>

namespace metin2::inline core {

    unsigned int thecore_idle_time = 0;

    namespace {
        std::optional<heart> the_heart;

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

        if (!metin2::log::init("log") || !pid_init()) {
            return false;
        }

        the_heart.emplace(fps);
        return true;
    }

    auto thecore_idle() -> int {
        thecore_tick();

        if (thecore_is_shutdowned()) {
            return 0;
        }

        const uint32_t t = get_dword_time();
        const int pulses = the_heart->idle();

        thecore_idle_time += get_dword_time() - t;

        return pulses;
    }

    void thecore_destroy() {
        pid_deinit();
        metin2::log::shutdown();
    }

    auto thecore_pulse() -> int {
        return the_heart->pulse();
    }

    auto thecore_next_pulse() -> int {
        return the_heart->next_pulse();
    }

    auto thecore_passes_per_sec() -> int {
        return the_heart->passes_per_sec();
    }
}
