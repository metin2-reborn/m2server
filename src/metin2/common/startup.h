#pragma once

#include <metin2/log/log.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace metin2::inline common {
    inline auto mark_server_ready() -> bool {
        std::ofstream file("ready");
        if (!file) {
            metin2::log::error("Cannot create readiness marker: {}", std::strerror(errno));
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
