#include "Main.h"

#include "ClientManager.h"
#include "DBManager.h"
#include "GuildManager.h"
#include "ItemAwardManager.h"
#include "ItemIDRangeManager.h"
#include "Marriage.h"
#include "Monarch.h"
#include "MoneyLog.h"
#include "PrivManager.h"

#include <common/startup.h>
#include <common/utils.h>
#include <libthecore/core_signal.h>
#include <metin2/config/config.h>
#include <metin2/log/log.h>
#include <libthecore/main.h>

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
void SetPlayerDBName(const char* c_pszPlayerDBName);
void SetTablePostfix(const char* c_pszTablePostfix);
int Start();

std::string g_stTablePostfix;
std::string g_stLocaleNameColumn = "name";
std::string g_stPlayerDBName = "";

//g_iLogoutSeconds must be longer than the player and item cache flush intervals.
int g_iLogoutSeconds = 60*10;


void emergency_sig(int sig)
{
	if (sig == SIGSEGV)
		metin2::log::info( "SIGNAL: SIGSEGV");
	else if (sig == SIGUSR1)
		metin2::log::info( "SIGNAL: SIGUSR1");

	if (sig == SIGSEGV)
		abort();
}

int main()
{
	WriteVersion();



	// Removes the pid file and closes the logs once everything below is destroyed.
	struct CoreCleanup
	{
		~CoreCleanup() { thecore_destroy(); }
	} coreCleanup;

	CDBManager DBManager; 
	CClientManager ClientManager;
	CGuildManager GuildManager;
	CPrivManager PrivManager;
	CMoneyLog MoneyLog;
	ItemAwardManager ItemAwardManager;
	marriage::CManager MarriageManager;
	CMonarch Monarch;
	CItemIDRangeManager ItemIDRangeManager;
	if (!Start())
		return 1;

	GuildManager.Initialize();
	MarriageManager.Initialize();
	ItemIDRangeManager.Build();
	metin2::log::info( "Metin2DBCacheServer Start");

	if (!mark_server_ready()) return 1;
	CClientManager::instance().MainLoop();

	signal_timer_disable();

	DBManager.Quit();
	int iCount;

	while (1)
	{
		iCount = 0;

		iCount += CDBManager::instance().CountReturnQuery(SQL_PLAYER);
		iCount += CDBManager::instance().CountAsyncQuery(SQL_PLAYER);

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
// server.toml with runtime.toml (written by the launcher) merged over it.
static bool load_settings()
{
	std::vector<std::filesystem::path> files = {"server.toml"};
	if (std::filesystem::exists("runtime.toml"))
		files.emplace_back("runtime.toml");

	auto settings = metin2::config::load(files);
	if (!settings)
	{
		metin2::log::error("invalid configuration:\n{}", settings.error());
		return false;
	}
	if (settings->role != metin2::config::role::db)
	{
		metin2::log::error("this executable runs role \"db\", the configuration says \"{}\"", metin2::config::role_name(settings->role));
		return false;
	}

	metin2::config::set_current(std::move(*settings));
	metin2::log::set_keep_days(metin2::config::current().log.keep_days);
	metin2::log::set_level(metin2::config::current().log.level);
	return true;
}

// Connects one database slot, retrying while MariaDB is still starting.
static bool connect_database(int slot, const std::string & name)
{
	const metin2::config::database_settings & database = metin2::config::database();

	for (int attempt = 1; attempt <= 6; ++attempt)
	{
		if (CDBManager::instance().Connect(slot, database.host.c_str(), database.port, name.c_str(), database.user.c_str(), database.password.c_str()))
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

int Start()
{
	if (!load_settings())
		return false;

	const metin2::config::db_settings & db = metin2::config::db();

	if (!thecore_init(db.heart_fps))
		return false;
	signal_timer_enable(60);

	SetTablePostfix(metin2::config::database().table_postfix.c_str());
	CClientManager::instance().SetCacheFlushCountLimit(db.cache_flush_limit_per_second);
	CClientManager::instance().SetPlayerIDStart(db.player_id_start);
	g_stLocaleNameColumn = db.name_column;

	const metin2::config::database_settings & database = metin2::config::database();
	if (!connect_database(SQL_PLAYER, database.player) || !connect_database(SQL_ACCOUNT, database.account) ||
		!connect_database(SQL_COMMON, database.common))
		return false;
	SetPlayerDBName(database.player.c_str());

	metin2::log::info( "ClientManager initialization.. ");

	if (!CClientManager::instance().Initialize())
	{
		metin2::log::info( "   failed"); 
		return false;
	}

	metin2::log::info( "   OK");

	signal(SIGUSR1, emergency_sig);
	signal(SIGSEGV, emergency_sig);
	return true;
}

void SetTablePostfix(const char* c_pszTablePostfix)
{
	if (!c_pszTablePostfix || !*c_pszTablePostfix)
		g_stTablePostfix = "";
	else
		g_stTablePostfix = c_pszTablePostfix;
}

const char * GetTablePostfix()
{
	return g_stTablePostfix.c_str();
}

void SetPlayerDBName(const char* c_pszPlayerDBName)
{
	if (! c_pszPlayerDBName || ! *c_pszPlayerDBName)
		g_stPlayerDBName = "";
	else
	{
		g_stPlayerDBName = c_pszPlayerDBName;
		g_stPlayerDBName += ".";
	}
}

const char * GetPlayerDBName()
{
	return g_stPlayerDBName.c_str();
}

