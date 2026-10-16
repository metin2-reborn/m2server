#pragma once

#include <string>

namespace metin2::db::inline app {

    auto start() -> int;
    auto get_table_postfix() -> const char *;
    auto get_player_db_name() -> const char *;

    extern std::string locale_name_column;

    extern int logout_seconds;
}
