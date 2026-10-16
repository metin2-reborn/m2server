#pragma once

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace metin2::inline common {

    inline auto mark_server_ready() -> bool {
        std::ofstream file("ready");
        if (!file) {
            std::fprintf(stderr, "cannot create readiness marker: %s\n", std::strerror(errno));
            return false;
        }

        file << "ready\n";
        file.close();

        if (!file) {
            std::remove("ready");
            return false;
        }

        return true;
    }
}
