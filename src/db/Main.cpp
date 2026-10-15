#include "Main.h"

#include "ClientManager.h"
#include "Config.h"
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
#include <metin2/log/log.h>
#include <libthecore/main.h>

#include <algorithm>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <unistd.h>

#undef __FreeBSD__
void SetPlayerDBName(const char* c_pszPlayerDBName);
void SetTablePostfix(const char* c_pszTablePostfix);
int Start();

std::string g_stTablePostfix;
std::string g_stLocaleNameColumn = "name";
std::string g_stPlayerDBName = "";


bool g_test_server = false;

//in seconds
int g_iPlayerCacheFlushSeconds = 60*7;
int g_iItemCacheFlushSeconds = 60*5;

//g_iLogoutSeconds must be longer than g_iPlayerCacheFlushSeconds and g_iItemCacheFlushSeconds.
int g_iLogoutSeconds = 60*10;

int g_log = 1;


// MYSHOP_PRICE_LIST
int g_iItemPriceListTableCacheFlushSeconds = 540;
// END_OF_MYSHOP_PRICE_LIST



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


	CConfig Config;

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
int Start()
{
	if (!CConfig::instance().LoadFile("conf.txt"))
	{
		fprintf(stderr, "Loading conf.txt failed.\n");
		return false;
	}

	int test_server = 0;
	if (!CConfig::instance().GetValue("TEST_SERVER", &test_server))
	{
		fprintf(stderr, "Real Server\n");
	}
	else
		fprintf(stderr, "Test Server\n");
	g_test_server = test_server != 0;

	if (!CConfig::instance().GetValue("LOG", &g_log))
	{
		fprintf(stderr, "Log Off");
		g_log= 0;
	}
	else
	{
		g_log = 1;
		fprintf(stderr, "Log On");
	}
	
	
	int tmpValue;

	int heart_beat = 50;
	if (!CConfig::instance().GetValue("CLIENT_HEART_FPS", &heart_beat))
	{
		fprintf(stderr, "Cannot find CLIENT_HEART_FPS configuration.\n");
		return false;
	}

	metin2::log::set_keep_days(3);

	if (CConfig::instance().GetValue("LOG_KEEP_DAYS", &tmpValue))
	{
		tmpValue = std::clamp(tmpValue, 3, 30);
		metin2::log::set_keep_days(tmpValue);
		fprintf(stderr, "Setting log keeping days to %d\n", tmpValue);
	}

	thecore_init(heart_beat);
	signal_timer_enable(60);

	char szBuf[256+1];

	if (!CConfig::instance().GetValue("TABLE_POSTFIX", szBuf, 256))
	{
		metin2::log::error("TABLE_POSTFIX not configured use default");
		szBuf[0] = '\0';
	}

	SetTablePostfix(szBuf);

	if (CConfig::instance().GetValue("PLAYER_CACHE_FLUSH_SECONDS", szBuf, 256))
	{
		str_to_number(g_iPlayerCacheFlushSeconds, szBuf);
		metin2::log::info( "PLAYER_CACHE_FLUSH_SECONDS: {:d}", g_iPlayerCacheFlushSeconds);
	}

	if (CConfig::instance().GetValue("ITEM_CACHE_FLUSH_SECONDS", szBuf, 256))
	{
		str_to_number(g_iItemCacheFlushSeconds, szBuf);
		metin2::log::info( "ITEM_CACHE_FLUSH_SECONDS: {:d}", g_iItemCacheFlushSeconds);
	}

	// MYSHOP_PRICE_LIST
	if (CConfig::instance().GetValue("ITEM_PRICELIST_CACHE_FLUSH_SECONDS", szBuf, 256)) 
	{
		str_to_number(g_iItemPriceListTableCacheFlushSeconds, szBuf);
		metin2::log::info( "ITEM_PRICELIST_CACHE_FLUSH_SECONDS: {:d}", g_iItemPriceListTableCacheFlushSeconds);
	}
	// END_OF_MYSHOP_PRICE_LIST
	//
	if (CConfig::instance().GetValue("CACHE_FLUSH_LIMIT_PER_SECOND", szBuf, 256))
	{
		uint32_t dwVal = 0; str_to_number(dwVal, szBuf);
		CClientManager::instance().SetCacheFlushCountLimit(dwVal);
	}

	int iIDStart;
	if (!CConfig::instance().GetValue("PLAYER_ID_START", &iIDStart))
	{
		metin2::log::error("PLAYER_ID_START not configured");
		return false;
	}

	CClientManager::instance().SetPlayerIDStart(iIDStart);

	if (CConfig::instance().GetValue("NAME_COLUMN", szBuf, 256))
	{
		fprintf(stderr, "%s %s", g_stLocaleNameColumn.c_str(), szBuf);
		g_stLocaleNameColumn = szBuf;
	}

	char szAddr[64], szDB[64], szUser[64], szPassword[64];
	int iPort;
	char line[256+1];

	if (CConfig::instance().GetValue("SQL_PLAYER", line, 256))
	{
		sscanf(line, " %63s %63s %63s %63s %d ", szAddr, szDB, szUser, szPassword, &iPort);
		metin2::log::info( "connecting to MySQL server (player)");

		int iRetry = 5;

		do
		{
			if (CDBManager::instance().Connect(SQL_PLAYER, szAddr, iPort, szDB, szUser, szPassword))
			{
				metin2::log::info( "   OK");
				break;
			}

			metin2::log::info( "   failed, retrying in 5 seconds");
			fprintf(stderr, "   failed, retrying in 5 seconds");
			sleep(5);
		} while (iRetry--);
		fprintf(stderr, "Success PLAYER\n");
		SetPlayerDBName(szDB);
	}
	else
	{
		metin2::log::error("SQL_PLAYER not configured");
		return false;
	}

	if (CConfig::instance().GetValue("SQL_ACCOUNT", line, 256))
	{
		sscanf(line, " %63s %63s %63s %63s %d ", szAddr, szDB, szUser, szPassword, &iPort);
		metin2::log::info( "connecting to MySQL server (account)");

		int iRetry = 5;

		do
		{
			if (CDBManager::instance().Connect(SQL_ACCOUNT, szAddr, iPort, szDB, szUser, szPassword))
			{
				metin2::log::info( "   OK");
				break;
			}

			metin2::log::info( "   failed, retrying in 5 seconds");
			fprintf(stderr, "   failed, retrying in 5 seconds");
			sleep(5);
		} while (iRetry--);
		fprintf(stderr, "Success ACCOUNT\n");
	}
	else
	{
		metin2::log::error("SQL_ACCOUNT not configured");
		return false;
	}

	if (CConfig::instance().GetValue("SQL_COMMON", line, 256))
	{
		sscanf(line, " %63s %63s %63s %63s %d ", szAddr, szDB, szUser, szPassword, &iPort);
		metin2::log::info( "connecting to MySQL server (common)");

		int iRetry = 5;

		do
		{
			if (CDBManager::instance().Connect(SQL_COMMON, szAddr, iPort, szDB, szUser, szPassword))
			{
				metin2::log::info( "   OK");
				break;
			}

			metin2::log::info( "   failed, retrying in 5 seconds");
			fprintf(stderr, "   failed, retrying in 5 seconds");
			sleep(5);
		} while (iRetry--);
		fprintf(stderr, "Success COMMON\n");
	}
	else
	{
		metin2::log::error("SQL_COMMON not configured");
		return false;
	}

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

