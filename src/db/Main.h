#pragma once

#include <string>

namespace metin2::db {

int	Start();
void End();
const char * GetTablePostfix();
const char * GetPlayerDBName();

// The proto name column (db.name_column, or from the locale).
extern std::string g_stLocaleNameColumn;

// Seconds after logout before a player's cache is dropped; longer than the cache flush intervals.
extern int g_iLogoutSeconds;
}
