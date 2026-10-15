#pragma once

#include <metin2/log/log.h>

#include <cstdint>
#include <string>
#include <vector>

namespace metin2::config {
    enum class role : std::uint8_t {
        db,
        auth,
        game,
    };

    struct log_settings {
        log::level level = log::level::info;
        unsigned int keep_days = 3;
    };

    struct database_settings {
        std::string host = "127.0.0.1";
        std::uint16_t port = 3306;
        std::string user;
        std::string password;
        std::string table_postfix;

        std::string account = "account";
        std::string common = "common";
        std::string player = "player";
        std::string log = "log";
    };

    struct db_settings {
        std::string bind_ip = "127.0.0.1";
        std::uint16_t port = 15000;
        std::vector<std::string> allowed_peers;
        int heart_fps = 10;
        bool login_log = false;

        int player_id_start = 0;
        std::uint32_t item_id_min = 0;
        std::uint32_t item_id_max = 0;
        int player_delete_level_limit = 0;
        int player_delete_level_limit_lower = 0;
        std::string name_column = "name";

        int player_cache_flush_seconds = 420;
        int item_cache_flush_seconds = 300;
        int pricelist_cache_flush_seconds = 540;
        unsigned int cache_flush_limit_per_second = 200;

        std::string guild_war_power;
        std::string guild_war_handicap;
    };

    struct admin_settings {
        std::vector<std::string> ips;
        std::string password;
    };

    struct server_settings {
        std::string hostname;
        std::uint8_t channel = 0;
        std::string bind_ip;
        std::string proxy_ip;
        std::uint16_t port = 13000;
        std::uint16_t p2p_port = 14000;
        std::vector<std::string> p2p_allowed_hosts;
        std::string db_host = "127.0.0.1";
        std::uint16_t db_port = 15000;
        int passes_per_sec = 25;
        int ping_seconds = 60;
        int user_limit = 32768;
        admin_settings admin;
    };

    struct auth_settings {
        bool master = true;
        std::string master_host;
        std::uint16_t master_port = 0;
        std::string block_login_date = "30000705";
    };

    struct mark_settings {
        bool server = true;
        std::uint8_t min_level = 3;
    };

    struct quest_settings {
        std::string dir;
        std::vector<std::string> object_dirs;
    };

    struct spam_settings {
        unsigned int block_duration = 900;
        unsigned int block_score = 100;
        unsigned int reload_cycle = 600;
        int block_max_level = 10;
    };

    struct anti_cheat_settings {
        int sync_hack_limit = 20;
        int speed_hack_limit = 50;
        int speed_hack_bonus = 80;
        bool check_multihack = true;
    };

    struct game_settings {
        std::vector<long> map_allow;
        int save_seconds = 120;
        int view_range = 5000;
        int max_level = 99;
        std::uint8_t pk_protect_level = 15;

        bool refuse_clients = false;
        bool no_regen = false;
        bool no_wander = false;
        bool speed_server = false;
        bool distribution_test_server = false;
        bool skill_disable = false;
        bool empire_whisper = true;
        bool protect_normal_player = false;
        bool notice_battle_zone = false;
        bool block_char_creation = false;

        int server_id = 0;
        std::string mall_url = "www.metin2.de";

        mark_settings mark;
        quest_settings quest;
        spam_settings spam;
        anti_cheat_settings anti_cheat;
    };

    struct settings {
        config::role role = config::role::game;
        bool test_server = false;
        log_settings log;
        database_settings database;
        db_settings db;
        server_settings server;
        auth_settings auth;
        game_settings game;
    };
}
