#include <metin2/db/app/main.h>

#include <metin2/db/clients/client_manager.h>
#include <metin2/db/empire/monarch.h>
#include <metin2/db/empire/priv_manager.h>
#include <metin2/db/guilds/guild_manager.h>
#include <metin2/db/items/item_award_manager.h>
#include <metin2/db/items/item_id_range_manager.h>
#include <metin2/db/logs/money_log.h>
#include <metin2/db/social/marriage.h>
#include <metin2/db/storage/db_manager.h>

#include <metin2/common/startup.h>
#include <metin2/config/config.h>
#include <metin2/core/main.h>
#include <metin2/log/log.h>

#include <csignal>
#include <string>

#include <unistd.h>

#undef __FreeBSD__

namespace metin2::db::inline app {
    std::string locale_name_column = "name";

    // Must be longer than the player and item cache flush intervals.
    int logout_seconds = 60 * 10;

    void emergency_sig(int sig) {
        if (sig == SIGSEGV) {
            metin2::log::error("Received SIGSEGV");
        } else if (sig == SIGUSR1) {
            metin2::log::info("Received SIGUSR1");
        }

        if (sig == SIGSEGV) {
            abort();
        }
    }

    auto run() -> int {
        struct core_cleanup {
            ~core_cleanup() { thecore_destroy(); }
        } core_cleanup;

        get_db_manager();
        get_client_manager();
        get_guild_manager();
        get_priv_manager();
        get_money_log();
        get_item_award_manager();
        marriage::get_manager();
        get_monarch();
        get_item_id_range_manager();

        if (!start()) {
            return 1;
        }

        get_guild_manager().initialize();
        marriage::get_manager().initialize();
        if (!get_item_id_range_manager().build()) {
            return 1;
        }
        metin2::log::info("Db cache server starting");

        if (!mark_server_ready()) return 1;
        get_client_manager().main_loop();

        signal_timer_disable();

        get_db_manager().quit();
        int count;

        while (1) {
            count = 0;

            count += get_db_manager().return_stats(SQL_PLAYER).queued;
            count += get_db_manager().async_stats(SQL_PLAYER).queued;

            if (count == 0) {
                break;
            }

            usleep(1000);
            metin2::log::info("Waiting for {:d} pending queries to finish", count);
        }

        return 0;
    }

    static auto connect_database(int slot, const std::string &name) -> bool {
        const metin2::config::database_settings &database = metin2::config::database();

        for (int attempt = 1; attempt <= 6; ++attempt) {
            if (get_db_manager().connect(slot, database.host.c_str(), database.port, name.c_str(), database.user.c_str(), database.password.c_str())) {
                metin2::log::info("Connected to database {}", name);
                return true;
            }

            metin2::log::warn("Cannot connect to database {} (attempt {:d}), retrying in 5 seconds", name, attempt);
            sleep(5);
        }

        metin2::log::error("Cannot connect to database {}", name);
        return false;
    }

    auto start() -> int {
        const auto &db = metin2::config::db();

        if (!thecore_init(db.heart_fps)) {
            return false;
        }

        signal_timer_enable(60);

        get_client_manager().set_cache_flush_count_limit(db.cache_flush_limit_per_second);
        get_client_manager().set_player_id_start(db.player_id_start);
        locale_name_column = db.name_column;

        const auto &database = metin2::config::database();
        if (!connect_database(SQL_PLAYER, database.player) ||
            !connect_database(SQL_ACCOUNT, database.account) ||
            !connect_database(SQL_COMMON, database.common)) {
            return false;
        }

        metin2::log::info("Initialising client manager");

        if (!get_client_manager().initialize()) {
            metin2::log::error("Client manager initialisation failed");
            return false;
        }

        metin2::log::info("Client manager initialised");

        signal(SIGUSR1, emergency_sig);
        signal(SIGSEGV, emergency_sig);
        return true;
    }
}
