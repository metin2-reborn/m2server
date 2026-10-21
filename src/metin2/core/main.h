#pragma once

namespace metin2::inline core {

    extern unsigned int thecore_idle_time;

    auto thecore_init(int fps) -> bool;
    auto thecore_idle() -> int;
    void thecore_shutdown();
    void thecore_destroy();
    auto thecore_pulse() -> int;
    auto thecore_next_pulse() -> int;
    auto thecore_passes_per_sec() -> int;
    auto thecore_is_shutdowned() -> bool;
    void thecore_tick();

    void signal_setup();
    void signal_timer_disable();
    void signal_timer_enable(int timeout_seconds);
}
