#pragma once

#include <string>

namespace metin2::db::inline app {

int	start();
void end();
const char * get_table_postfix();
const char * get_player_db_name();

extern std::string locale_name_column;

extern int logout_seconds;
}
