#include <metin2/core/main.h>

#include <metin2/core/utils.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <format>
#include <mutex>
#include <stop_token>
#include <thread>

#include <sys/wait.h>
#include <unistd.h>

namespace metin2::inline core {

    namespace {
        std::atomic<unsigned> ticks{0};
        volatile std::sig_atomic_t shutdowned = false;
        std::atomic<int> watchdog_seconds{0};
        volatile std::sig_atomic_t dump_requested = 0;

        void write_stderr(std::string_view text) {
            (void)write(STDERR_FILENO, text.data(), text.size());
        }

        void watch(std::stop_token stop) {
            using namespace std::chrono;

            std::mutex mutex;
            std::condition_variable_any wake;
            auto last_tick = ticks.load(std::memory_order_relaxed);
            auto last_change = steady_clock::now();

            while (!stop.stop_requested()) {
                {
                    std::unique_lock lock(mutex);
                    wake.wait_for(lock, stop, 1s, [] -> bool { return false; });
                }

                if (dump_requested) {
                    dump_requested = 0;
                    core_dump();
                }

                const auto now = steady_clock::now();
                const auto tick = ticks.load(std::memory_order_relaxed);
                const int limit = watchdog_seconds.load(std::memory_order_relaxed);

                if (tick != last_tick || limit == 0 || tick == 0) {
                    last_tick = tick;
                    last_change = now;
                    continue;
                }

                if (now - last_change > seconds(limit)) {
                    // The main loop may hold the log mutex, so report on stderr only.
                    write_stderr(std::format("Watchdog: main loop stalled for more than {} s, aborting\n", limit));
                    std::abort();
                }
            }
        }

        std::jthread watchdog;

        void reap(int) {
            while (waitpid(-1, nullptr, WNOHANG) > 0)
                ;
        }

        void hupsig(int) {
            shutdowned = true;
            write_stderr("shutdown signal received\n");
        }

        void usrsig(int) {
            dump_requested = 1;
        }
    }

    void thecore_shutdown() {
        shutdowned = true;
    }

    auto thecore_is_shutdowned() -> bool {
        return shutdowned;
    }

    void thecore_tick() {
        ticks.fetch_add(1, std::memory_order_relaxed);
    }

    void signal_timer_disable() {
        watchdog_seconds = 0;
    }

    void signal_timer_enable(int timeout_seconds) {
        watchdog_seconds = timeout_seconds;
    }

    void signal_setup() {
        signal_timer_enable(30);
        if (!watchdog.joinable())
            watchdog = std::jthread(watch);

        std::signal(SIGHUP, hupsig);
        std::signal(SIGCHLD, reap);
        std::signal(SIGINT, hupsig);
        std::signal(SIGTERM, hupsig);
        std::signal(SIGPIPE, SIG_IGN);
        std::signal(SIGALRM, SIG_IGN);
        std::signal(SIGUSR1, usrsig);
    }
}
