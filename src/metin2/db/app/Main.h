#pragma once

#include <string>

namespace metin2::db::inline app {

int	start();
void end();
const char * get_table_postfix();
const char * get_player_db_name();

// The proto name column (db.name_column, or from the locale).
extern std::string locale_name_column;

// Seconds after logout before a player's cache is dropped; longer than the cache flush intervals.
extern int logout_seconds;
}
