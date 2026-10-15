#include "internal.h"

#include <libthecore/core_signal.h>
#include <metin2/log/log.h>
#include <libthecore/utils.h>

#include <csignal>
#include <cstdlib>

#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
    void reap(int) {
        while (waitpid(-1, nullptr, WNOHANG) > 0)
            ;
        std::signal(SIGCHLD, reap);
    }

    // The virtual timer fires every 30s of CPU time; no tick since the last one means the
    // main loop is stuck.
    void checkpointing(int) {
        if (!tics) {
            metin2::log::error("CHECKPOINT shutdown: tics did not updated.");
            std::abort();
        }
        tics = 0;
    }

    void hupsig(int) {
        shutdowned = true;
        constexpr char message[] = "shutdown signal received\n";
        (void)write(STDERR_FILENO, message, sizeof(message) - 1);
    }

    void usrsig(int) {
        core_dump();
    }

    void set_virtual_timer(int seconds) {
        itimerval itime{};
        itime.it_interval.tv_sec = seconds;
        itime.it_value.tv_sec = seconds;
        setitimer(ITIMER_VIRTUAL, &itime, nullptr);
    }
}

void signal_timer_disable() {
    set_virtual_timer(0);
}

void signal_timer_enable(int sec) {
    set_virtual_timer(sec);
}

void signal_setup() {
    signal_timer_enable(30);

    std::signal(SIGVTALRM, checkpointing);

    // just to be on the safe side:
    std::signal(SIGHUP, hupsig);
    std::signal(SIGCHLD, reap);
    std::signal(SIGINT, hupsig);
    std::signal(SIGTERM, hupsig);
    std::signal(SIGPIPE, SIG_IGN);
    std::signal(SIGALRM, SIG_IGN);
    std::signal(SIGUSR1, usrsig);
}
