#pragma once

#include <metin2/config/config.h>

#include <cstdint>
#include <set>
#include <string>

namespace metin2::game {

// Game and auth process setup from metin2::config (server.toml): database connections, locale, data
// files. The settings themselves are read through metin2::config::server(), game(), auth(), ...; the
// variables below are runtime state derived from them.
void config_init();

// Pulses in sec seconds.
inline auto PASSES_PER_SEC(auto sec) { return sec * metin2::config::server().passes_per_sec; }

// Address the listeners bind to: server.bind_ip, or detected.
extern char g_szPublicIP[16];
// Address told to the clients instead of g_szPublicIP (server.proxy_ip, resolved).
extern std::string g_stProxyIP;

// New clients are refused (game.refuse_clients at start, set at shutdown).
extern bool g_bNoMoreClient;

// Thresholds of the user count shown as full/busy; state_user_count may change them at run time.
extern int g_iFullUserCount;
extern int g_iBusyUserCount;
void LoadStateUserCount();

// The locale's quest directories plus game.quest.
extern std::string g_stQuestDir;
extern std::set<std::string> g_setQuestObjectDir;

extern std::string g_stLocale;
extern std::string g_stLocaleFilename;

extern std::string g_stClientVersion;
extern bool g_bCheckClientVersion;
void CheckClientVersion();

// View = VIEW_RANGE + VIEW_BONUS_RANGE: a 500 cm margin, since an exact match with the client's range causes problems.
inline constexpr int VIEW_BONUS_RANGE = 500;

extern uint32_t g_GoldDropTimeLimitValue;

bool map_allow_find(int index);
void map_allow_copy(int32_t * pl, int size);
void map_allow_log();

const char * get_table_postfix();
}
