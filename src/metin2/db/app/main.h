#pragma once

#include <string>

namespace metin2::db::inline app {
    auto start() -> int;

    extern std::string locale_name_column;

    extern int logout_seconds;
}
