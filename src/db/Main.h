#pragma once

#include <string>

int	Start();
void End();
const char * GetTablePostfix();
const char * GetPlayerDBName();
void WriteVersion();

// Configuration read by Start() (Main.cpp).
extern bool g_test_server;
extern int g_log;
extern std::string g_stLocaleNameColumn;

// Cache flush intervals in seconds; g_iLogoutSeconds must exceed the player and item ones.
extern int g_iPlayerCacheFlushSeconds;
extern int g_iItemCacheFlushSeconds;
extern int g_iLogoutSeconds;
extern int g_iItemPriceListTableCacheFlushSeconds;
