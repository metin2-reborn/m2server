#include <metin2/db/app/Main.h>

#include <metin2/db/clients/ClientManager.h>
#include <metin2/db/storage/DBManager.h>
#include <metin2/db/guilds/GuildManager.h>
#include <metin2/db/items/ItemAwardManager.h>
#include <metin2/db/items/ItemIDRangeManager.h>
#include <metin2/db/social/Marriage.h>
#include <metin2/db/empire/Monarch.h>
#include <metin2/db/logs/MoneyLog.h>
#include <metin2/db/empire/PrivManager.h>

#include <metin2/common/startup.h>
#include <metin2/common/utils.h>
#include <metin2/core/core_signal.h>
#include <metin2/config/config.h>
#include <metin2/log/log.h>
#include <metin2/core/main.h>

#include <algorithm>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <unistd.h>

#undef __FreeBSD__

namespace metin2::db::inline app {
void set_player_db_name(const char* c_pszPlayerDBName);
void set_table_postfix(const char* c_pszTablePostfix);
int start();

std::string table_postfix;
std::string locale_name_column = "name";
std::string player_db_name = "";

//g_iLogoutSeconds must be longer than the player and item cache flush intervals.
int logout_seconds = 60*10;


void emergency_sig(int sig)
{
	if (sig == SIGSEGV)
		metin2::log::info( "SIGNAL: SIGSEGV");
	else if (sig == SIGUSR1)
		metin2::log::info( "SIGNAL: SIGUSR1");

	if (sig == SIGSEGV)
		abort();
}

int run()
{



	// Removes the pid file and closes the logs once everything below is destroyed.
	struct core_cleanup
	{
		~core_cleanup() { thecore_destroy(); }
	} coreCleanup;

	db_manager DBManager; 
	client_manager ClientManager;
	guild_manager GuildManager;
	priv_manager PrivManager;
	money_log MoneyLog;
	item_award_manager ItemAwardManager;
	marriage::manager MarriageManager;
	monarch Monarch;
	item_id_range_manager ItemIDRangeManager;
	if (!start())
		return 1;

	GuildManager.initialize();
	MarriageManager.initialize();
	ItemIDRangeManager.build();
	metin2::log::info( "Metin2DBCacheServer Start");

	if (!mark_server_ready()) return 1;
	client_manager::instance().main_loop();

	signal_timer_disable();

	DBManager.quit();
	int iCount;

	while (1)
	{
		iCount = 0;

		iCount += db_manager::instance().count_return_query(SQL_PLAYER);
		iCount += db_manager::instance().count_async_query(SQL_PLAYER);

		if (iCount == 0)
			break;

		usleep(1000);
		metin2::log::info( "WAITING_QUERY_COUNT {:d}", iCount);
	}

	return 0;
}

//
// @version	05/06/13 Bang2ni - Added item price info cache flush timeout setting.
//
// Connects one database slot, retrying while MariaDB is still starting.
static bool connect_database(int slot, const std::string & name)
{
	const metin2::config::database_settings & database = metin2::config::database();

	for (int attempt = 1; attempt <= 6; ++attempt)
	{
		if (db_manager::instance().connect(slot, database.host.c_str(), database.port, name.c_str(), database.user.c_str(), database.password.c_str()))
		{
			metin2::log::info("connected to database {}", name);
			return true;
		}

		metin2::log::warn("cannot connect to database {} (attempt {:d}), retrying in 5 seconds", name, attempt);
		sleep(5);
	}

	metin2::log::error("cannot connect to database {}", name);
	return false;
}

int start()
{
	const metin2::config::db_settings & db = metin2::config::db();

	if (!thecore_init(db.heart_fps))
		return false;
	signal_timer_enable(60);

	set_table_postfix(metin2::config::database().table_postfix.c_str());
	client_manager::instance().set_cache_flush_count_limit(db.cache_flush_limit_per_second);
	client_manager::instance().set_player_id_start(db.player_id_start);
	locale_name_column = db.name_column;

	const metin2::config::database_settings & database = metin2::config::database();
	if (!connect_database(SQL_PLAYER, database.player) || !connect_database(SQL_ACCOUNT, database.account) ||
		!connect_database(SQL_COMMON, database.common))
		return false;
	set_player_db_name(database.player.c_str());

	metin2::log::info( "ClientManager initialization.. ");

	if (!client_manager::instance().initialize())
	{
		metin2::log::info( "   failed"); 
		return false;
	}

	metin2::log::info( "   OK");

	signal(SIGUSR1, emergency_sig);
	signal(SIGSEGV, emergency_sig);
	return true;
}

void set_table_postfix(const char* c_pszTablePostfix)
{
	if (!c_pszTablePostfix || !*c_pszTablePostfix)
		table_postfix = "";
	else
		table_postfix = c_pszTablePostfix;
}

const char * get_table_postfix()
{
	return table_postfix.c_str();
}

void set_player_db_name(const char* c_pszPlayerDBName)
{
	if (! c_pszPlayerDBName || ! *c_pszPlayerDBName)
		player_db_name = "";
	else
	{
		player_db_name = c_pszPlayerDBName;
		player_db_name += ".";
	}
}

const char * get_player_db_name()
{
	return player_db_name.c_str();
}
}
