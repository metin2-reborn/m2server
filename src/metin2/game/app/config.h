#pragma once

#include <metin2/config/config.h>

#include <cstdint>
#include <set>
#include <string>

namespace metin2::game::inline app {

// Game and auth process setup from metin2::config (server.toml): database connections, locale, data
// files. The settings themselves are read through metin2::config::server(), game(), auth(), ...; the
// variables below are runtime state derived from them.
void config_init();

// Pulses in sec seconds.
inline auto passes_per_sec(auto sec) { return sec * metin2::config::server().passes_per_sec; }

// Address the listeners bind to: server.bind_ip, or detected.
extern char public_ip[16];
// Address told to the clients instead of g_szPublicIP (server.proxy_ip, resolved).
extern std::string proxy_ip;

// New clients are refused (game.refuse_clients at start, set at shutdown).
extern bool no_more_client;

// Thresholds of the user count shown as full/busy; state_user_count may change them at run time.
extern int full_user_count;
extern int busy_user_count;
void load_state_user_count();

// The locale's quest directories plus game.quest.
extern std::string quest_dir;
extern std::set<std::string> quest_object_dir;


extern std::string client_version;
extern bool check_client_version_enabled;
void check_client_version();

// View = VIEW_RANGE + VIEW_BONUS_RANGE: a 500 cm margin, since an exact match with the client's range causes problems.
inline constexpr int VIEW_BONUS_RANGE = 500;

extern uint32_t gold_drop_time_limit_value;

bool map_allow_find(int index);
void map_allow_copy(int32_t * pl, int size);
void map_allow_log();

const char * get_table_postfix();
}
