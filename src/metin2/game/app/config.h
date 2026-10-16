#pragma once

#include <metin2/config/config.h>

#include <cstdint>
#include <set>
#include <string>

namespace metin2::game::inline app {

    void config_init();

    inline auto passes_per_sec(auto sec) { return sec * metin2::config::server().passes_per_sec; }

    extern char public_ip[16];
    extern std::string proxy_ip;

    extern bool no_more_client;

    extern int full_user_count;
    extern int busy_user_count;
    void load_state_user_count();

    extern std::string quest_dir;
    extern std::set<std::string> quest_object_dir;

    extern std::string client_version;
    extern bool check_client_version_enabled;
    void check_client_version();

    // A 500 cm margin over the view range: matching the client exactly causes problems.
    inline constexpr int VIEW_BONUS_RANGE = 500;

    extern uint32_t gold_drop_time_limit_value;

    auto map_allow_find(int index) -> bool;
    void map_allow_copy(int32_t *pl, int size);
    void map_allow_log();

    auto get_table_postfix() -> const char *;
}
