#include <metin2/config/config.h>
#include <metin2/log/log.h>
#include <metin2/net/io.h>

#include <filesystem>
#include <fstream>
#include <utility>

namespace metin2::db::inline app {
    auto run() -> int;
}

namespace metin2::game::inline app {
    auto run() -> int;
}

namespace {
    auto load_settings() -> bool {
        auto settings = metin2::config::load("server.toml");
        if (!settings) {
            metin2::log::error("Invalid configuration:\n{}", settings.error());
            return false;
        }

        metin2::config::set_current(std::move(*settings));
        metin2::log::set_keep_days(metin2::config::current().log.keep_days);
        metin2::log::set_level(metin2::config::current().log.level);
        return true;
    }
}

auto main() -> int {
    // The managers are function-local statics that may own sockets; creating the io_context first makes it
    // outlive them, since statics are destroyed in reverse order of construction.
    metin2::net::io();

    std::filesystem::remove("ready");

    if (!load_settings()) {
        return 1;
    }

    const auto role = metin2::config::current_role();

    metin2::log::info("Metin2 server {} starting as {}", M2_VERSION, metin2::config::role_name(role));

    if (role == metin2::config::role::db) {
        return metin2::db::run();
    }

    return metin2::game::run();
}
