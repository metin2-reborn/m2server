#include <metin2/config/config.h>
#include <metin2/log/log.h>

#include <filesystem>
#include <fstream>
#include <utility>
#include <vector>

namespace metin2::db::inline app {
    auto run() -> int;
}

namespace metin2::game::inline app {
    auto run(int argc, char **argv) -> int;
}

namespace {
    auto load_settings() -> bool {
        std::vector<std::filesystem::path> files = {"server.toml"};
        if (std::filesystem::exists("runtime.toml"))
            files.emplace_back("runtime.toml");

        auto settings = metin2::config::load(files);
        if (!settings) {
            metin2::log::error("invalid configuration:\n{}", settings.error());
            return false;
        }

        metin2::config::set_current(std::move(*settings));
        metin2::log::set_keep_days(metin2::config::current().log.keep_days);
        metin2::log::set_level(metin2::config::current().log.level);
        return true;
    }
}

auto main(int argc, char **argv) -> int {
    if (!load_settings())
        return 1;

    const metin2::config::role role = metin2::config::current_role();
    std::ofstream("version.txt") << "metin2 server " << M2_VERSION << " (" << metin2::config::role_name(role) << ")\n";

    if (role == metin2::config::role::db)
        return metin2::db::run();
    return metin2::game::run(argc, argv);
}
