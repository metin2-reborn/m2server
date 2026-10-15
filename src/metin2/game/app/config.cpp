#include <metin2/game/app/config.h>

#include <metin2/game/characters/char.h>
#include <metin2/game/admin/cmd.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/protocol/ip_ban.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/combat/skill_power.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/guilds/war_map.h>

#include <metin2/proto/length.h>
#include <metin2/common/utils.h>
#include <metin2/sql/AsyncSQL.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>

#include <mysql.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <set>
#include <string>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/socket.h>

namespace metin2::game::inline app {

char		g_szPublicIP[16] = "0";
std::string	g_stProxyIP;
bool		g_bNoMoreClient = false;
int			g_iFullUserCount = 1200;
int			g_iBusyUserCount = 650;

bool		g_bCheckClientVersion = true;
std::string	g_stClientVersion = "1215955205";

uint32_t	g_GoldDropTimeLimitValue = 0;

std::string g_stQuestDir = "./quest";
std::set<std::string> g_setQuestObjectDir;

static std::set<int> s_set_map_allows;

bool map_allow_find(int index)
{
	if (metin2::config::is_auth())
		return false;

	if (s_set_map_allows.find(index) == s_set_map_allows.end())
		return false;

	return true;
}

void map_allow_log()
{
	std::set<int>::iterator i;

	for (i = s_set_map_allows.begin(); i != s_set_map_allows.end(); ++i)
		metin2::log::info( "MAP_ALLOW: {:d}", *i);
}


void map_allow_copy(int32_t * pl, int size)
{
	int iCount = 0;
	std::set<int>::iterator it = s_set_map_allows.begin();

	if (s_set_map_allows.size() > static_cast<size_t>(size))
	{
		metin2::log::error("too many maps for the internal routing capacity ({:d})", size);
		exit(1);
	}
	while (it != s_set_map_allows.end() && iCount < size)
	{
		*(pl++) = *(it++);
		++iCount;
	}
}

static bool GetIPInfo()
{
	char szInternalIP[16] = "0";
	struct ifaddrs* ifaddrp = nullptr;

	if (0 != getifaddrs(&ifaddrp))
		return false;

	for( struct ifaddrs* ifap=ifaddrp ; nullptr != ifap ; ifap = ifap->ifa_next )
	{
		if (!ifap->ifa_addr || ifap->ifa_addr->sa_family != AF_INET) continue;
		struct sockaddr_in * sai = (struct sockaddr_in *) ifap->ifa_addr;

		if (!ifap->ifa_netmask ||  // ignore if no netmask
				sai->sin_addr.s_addr == 0 || // ignore if address is 0.0.0.0
				sai->sin_addr.s_addr == 16777343) // ignore if address is 127.0.0.1
			continue;

		char * netip = inet_ntoa(sai->sin_addr);

		if (!strncmp(netip, "192.168", 7)) // ignore if address is starting with 192
		{
			strlcpy(szInternalIP, netip, sizeof(szInternalIP));
		}
		else if (!strncmp(netip, "10.", 3))
		{
			strlcpy(szInternalIP, netip, sizeof(szInternalIP));
		}
		else if (g_szPublicIP[0] == '0')
		{
			strlcpy(g_szPublicIP, netip, sizeof(g_szPublicIP));
		}
	}

	freeifaddrs( ifaddrp );

	if (g_szPublicIP[0] != '0')
	{
		return true;
	}
	else
	{
		if (szInternalIP[0] == '0')
		{
			return false;
		}
		else
		{
			strlcpy(g_szPublicIP, szInternalIP, sizeof(g_szPublicIP));
			return true;
		}
	}
}


void config_init()
{
	const metin2::config::server_settings & server = metin2::config::server();
	const metin2::config::database_settings & db = metin2::config::database();

	if (!server.bind_ip.empty())
		strlcpy(g_szPublicIP, server.bind_ip.c_str(), sizeof(g_szPublicIP));
	else if (!GetIPInfo())
	{
		metin2::log::error("cannot detect the address to bind to; set server.bind_ip");
		exit(1);
	}

	if (!server.proxy_ip.empty())
	{
		in_addr address{};
		if (inet_pton(AF_INET, server.proxy_ip.c_str(), &address) == 1)
			g_stProxyIP = server.proxy_ip;
		else if (const hostent * host = gethostbyname(server.proxy_ip.c_str()); host && host->h_addr_list[0])
		{
			g_stProxyIP = inet_ntoa(*reinterpret_cast<const in_addr *>(host->h_addr_list[0]));
			metin2::log::info("server.proxy_ip {} resolves to {}", server.proxy_ip, g_stProxyIP);
		}
		else
		{
			metin2::log::error("cannot resolve server.proxy_ip {}", server.proxy_ip);
			exit(1);
		}
	}

	g_bNoMoreClient = metin2::config::game().refuse_clients;

	for (long index : metin2::config::game().map_allow)
		s_set_map_allows.insert(static_cast<int>(index));

	if (metin2::config::game().max_level < 1 || metin2::config::game().max_level > PLAYER_MAX_LEVEL_CONST)
	{
		metin2::log::error("game.max_level must be from 1 to {:d}", PLAYER_MAX_LEVEL_CONST);
		exit(1);
	}

	// The Common DB holds the locale info, so connect to it first.
	AccountDB::instance().Connect(db.host.c_str(), db.port, db.user.c_str(), db.password.c_str(), db.common.c_str());

	if (false == AccountDB::instance().IsConnected())
	{
		metin2::log::error( "cannot start server while no common sql connected");
		exit(1);
	}

	metin2::log::info( "CommonSQL connected");

	// Fetch the locale info 
	// <WARNING> Never add a condition (WHERE) to this query. (It may cause problems in other regions)
	{
		char szQuery[512];
		snprintf(szQuery, sizeof(szQuery), "SELECT mKey, mValue FROM locale");

		std::unique_ptr<SQLMsg> pMsg(AccountDB::instance().DirectQuery(szQuery));

		if (pMsg->Get()->uiNumRows == 0)
		{
			metin2::log::error( "COMMON_SQL: DirectQuery failed : {}", szQuery);
			exit(1);
		}

		MYSQL_ROW row; 

		while (nullptr != (row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
		{
			// Locale setup
			if (strcasecmp(row[0], "LOCALE") == 0)
			{
				if (LocaleService_Init(row[1]) == false)
				{
					metin2::log::error( "COMMON_SQL: invalid locale key {}", row[1]);
					exit(1);
				}
			}
		}
	}

	// Set the locale info on the COMMON SQL.
	// Note: g_stLocale is set inside LocaleService_Init().
	metin2::log::info( "Setting DB to locale {}", g_stLocale.c_str());

	AccountDB::instance().SetLocale(g_stLocale);

	AccountDB::instance().ConnectAsync(db.host.c_str(), db.port, db.user.c_str(), db.password.c_str(), db.common.c_str(), g_stLocale.c_str());

	// Connect to Player DB
	// The auth server reads accounts where the game reads players.
	const std::string & player_db = metin2::config::is_auth() ? db.account : db.player;
	DBManager::instance().Connect(db.host.c_str(), db.port, db.user.c_str(), db.password.c_str(), player_db.c_str());

	if (!DBManager::instance().IsConnected())
	{
		metin2::log::error( "PlayerSQL.ConnectError");
		exit(1);
	}

	metin2::log::info( "PlayerSQL connected");

	if (!metin2::config::is_auth())
	{
		// Connect to Log DB
		LogManager::instance().Connect(db.host.c_str(), db.port, db.user.c_str(), db.password.c_str(), db.log.c_str());

		if (!LogManager::instance().IsConnected())
		{
			metin2::log::error( "LogSQL.ConnectError");
			exit(1);
		}

		metin2::log::info( "LogSQL connected");

		LogManager::instance().BootLog(server.hostname.c_str(), server.channel);
	}

	// SKILL_POWER_BY_LEVEL
	// Due to string comparison issues, do this after AccountDB::instance().SetLocale(g_stLocale).
	// Of course this is not much of a problem domestically (overseas is the problem)
	{
		char szQuery[256];
		snprintf(szQuery, sizeof(szQuery), "SELECT mValue FROM locale WHERE mKey='SKILL_POWER_BY_LEVEL'");
		std::unique_ptr<SQLMsg> pMsg(AccountDB::instance().DirectQuery(szQuery));

		if (pMsg->Get()->uiNumRows == 0)
		{
			metin2::log::error( "[SKILL_PERCENT] Query failed: {}", szQuery);
			exit(1);
		}

		MYSQL_ROW row; 

		row = mysql_fetch_row(pMsg->Get()->pSQLResult);

		const char * p = row[0];
		int cnt = 0;
		char num[128];
		int aiBaseSkillPowerByLevelTable[SKILL_MAX_LEVEL+1];

		metin2::log::info( "SKILL_POWER_BY_LEVEL {}", p);
		while (*p != '\0' && cnt < (SKILL_MAX_LEVEL + 1))
		{
			p = one_argument(p, num, sizeof(num));
			aiBaseSkillPowerByLevelTable[cnt++] = atoi(num);

			if (*p == '\0')
			{
				if (cnt != (SKILL_MAX_LEVEL + 1))
				{
					metin2::log::error( "[SKILL_PERCENT] locale table has not enough skill information! (count: {:d} query: {})", cnt, szQuery);
					exit(1);
				}

				metin2::log::info( "SKILL_POWER_BY_LEVEL: Done! (count {:d})", cnt);
				break;
			}
		}

		// Per-race skill setup
		for (int job = 0; job < JOB_MAX_NUM * 2; ++job)
		{
			snprintf(szQuery, sizeof(szQuery), "SELECT mValue from locale where mKey='SKILL_POWER_BY_LEVEL_TYPE%d' ORDER BY CAST(mValue AS unsigned)", job);
			std::unique_ptr<SQLMsg> pMsg(AccountDB::instance().DirectQuery(szQuery));

			// Use the default table if not set.
			if (pMsg->Get()->uiNumRows == 0)
			{
				CTableBySkill::instance().SetSkillPowerByLevelFromType(job, aiBaseSkillPowerByLevelTable);
				continue;
			}

			row = mysql_fetch_row(pMsg->Get()->pSQLResult);
			cnt = 0;
			p = row[0];
			int aiSkillTable[SKILL_MAX_LEVEL + 1];

			metin2::log::info( "SKILL_POWER_BY_JOB {:d} {}", job, p);
			while (*p != '\0' && cnt < (SKILL_MAX_LEVEL + 1))
			{			
				p = one_argument(p, num, sizeof(num));
				aiSkillTable[cnt++] = atoi(num);

				if (*p == '\0')
				{
					if (cnt != (SKILL_MAX_LEVEL + 1))
					{
						metin2::log::error( "[SKILL_PERCENT] locale table has not enough skill information! (count: {:d} query: {})", cnt, szQuery);
						exit(1);
					}

					metin2::log::info( "SKILL_POWER_BY_JOB: Done! (job: {:d} count: {:d})", job, cnt);
					break;
				}
			}

			CTableBySkill::instance().SetSkillPowerByLevelFromType(job, aiSkillTable);
		}		
	}
	// END_SKILL_POWER_BY_LEVEL


	if (!metin2::config::game().quest.dir.empty())
		g_stQuestDir = metin2::config::game().quest.dir;
	for (const std::string & dir : metin2::config::game().quest.object_dirs)
		g_setQuestObjectDir.insert(dir);

	// LOCALE_SERVICE
	LocaleService_LoadLocaleStringFile();
	LocaleService_TransferDefaultSetting();
	LocaleService_LoadEmpireTextConvertTables();
	// END_OF_LOCALE_SERVICE

	if (FILE * fp = fopen("CMD", "r"))
	{
		char buf[256];
		while (fgets(buf, 256, fp))
		{
			char cmd[32], levelname[32];
			int level;

			two_arguments(buf, cmd, sizeof(cmd), levelname, sizeof(levelname));

			if (!*cmd || !*levelname)
			{
				metin2::log::error( "CMD syntax error: <cmd> <DISABLE | LOW_WIZARD | WIZARD | HIGH_WIZARD | GOD>");
				exit(1);
			}

			if (!strcasecmp(levelname, "LOW_WIZARD"))
				level = GM_LOW_WIZARD;
			else if (!strcasecmp(levelname, "WIZARD"))
				level = GM_WIZARD;
			else if (!strcasecmp(levelname, "HIGH_WIZARD"))
				level = GM_HIGH_WIZARD;
			else if (!strcasecmp(levelname, "GOD"))
				level = GM_GOD;
			else if (!strcasecmp(levelname, "IMPLEMENTOR"))
				level = GM_IMPLEMENTOR;
			else if (!strcasecmp(levelname, "DISABLE"))
				level = GM_IMPLEMENTOR + 1;
			else
			{
				metin2::log::error( "CMD syntax error: <cmd> <DISABLE | LOW_WIZARD | WIZARD | HIGH_WIZARD | GOD>");
				exit(1);
			}

			interpreter_set_privilege(cmd, level);
		}

		fclose(fp);
	}

	LoadStateUserCount();

	CWarMapManager::instance().LoadWarMapInfo(nullptr);
}

const char* get_table_postfix()
{
	return metin2::config::database().table_postfix.c_str();
}

void CheckClientVersion()
{
	g_bCheckClientVersion = true;

	const DESC_MANAGER::DESC_SET & set = DESC_MANAGER::instance().GetClientSet();
	DESC_MANAGER::DESC_SET::const_iterator it = set.begin();

	while (it != set.end())
	{
		DESC * d = *(it++);

		if (!d->GetCharacter())
			continue;

		int version = atoi(g_stClientVersion.c_str());
		int date	= atoi(d->GetClientVersion() );

		if (version != date) // Fix
		{
			d->GetCharacter()->ChatPacket(CHAT_TYPE_NOTICE, LC_TEXT("클라이언트 버전이 틀려 로그아웃 됩니다. 정상적으로 패치 후 접속하세요."));
			d->DelayedDisconnect(10);
		}
	}
}

void LoadStateUserCount()
{
	FILE * fp = fopen("state_user_count", "r");

	if (!fp)
		return;

	fscanf(fp, " %d %d ", &g_iFullUserCount, &g_iBusyUserCount);

	fclose(fp);
}
}
