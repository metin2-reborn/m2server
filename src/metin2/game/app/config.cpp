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

char		public_ip[16] = "0";
std::string	proxy_ip;
bool		no_more_client = false;
int			full_user_count = 1200;
int			busy_user_count = 650;

bool		check_client_version_enabled = true;
std::string	client_version = "1215955205";

uint32_t	gold_drop_time_limit_value = 0;

std::string quest_dir = "./quest";
std::set<std::string> quest_object_dir;

static std::set<int> set_map_allows;

bool map_allow_find(int index)
{
	if (metin2::config::is_auth())
		return false;

	if (set_map_allows.find(index) == set_map_allows.end())
		return false;

	return true;
}

void map_allow_log()
{
	std::set<int>::iterator i;

	for (i = set_map_allows.begin(); i != set_map_allows.end(); ++i)
		metin2::log::info( "MAP_ALLOW: {:d}", *i);
}


void map_allow_copy(int32_t * pl, int size)
{
	int iCount = 0;
	std::set<int>::iterator it = set_map_allows.begin();

	if (set_map_allows.size() > static_cast<size_t>(size))
	{
		metin2::log::error("too many maps for the internal routing capacity ({:d})", size);
		exit(1);
	}
	while (it != set_map_allows.end() && iCount < size)
	{
		*(pl++) = *(it++);
		++iCount;
	}
}

static bool get_ip_info()
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
		else if (public_ip[0] == '0')
		{
			strlcpy(public_ip, netip, sizeof(public_ip));
		}
	}

	freeifaddrs( ifaddrp );

	if (public_ip[0] != '0')
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
			strlcpy(public_ip, szInternalIP, sizeof(public_ip));
			return true;
		}
	}
}


void config_init()
{
	const metin2::config::server_settings & server = metin2::config::server();
	const metin2::config::database_settings & db = metin2::config::database();

	if (!server.bind_ip.empty())
		strlcpy(public_ip, server.bind_ip.c_str(), sizeof(public_ip));
	else if (!get_ip_info())
	{
		metin2::log::error("cannot detect the address to bind to; set server.bind_ip");
		exit(1);
	}

	if (!server.proxy_ip.empty())
	{
		in_addr address{};
		if (inet_pton(AF_INET, server.proxy_ip.c_str(), &address) == 1)
			proxy_ip = server.proxy_ip;
		else if (const hostent * host = gethostbyname(server.proxy_ip.c_str()); host && host->h_addr_list[0])
		{
			proxy_ip = inet_ntoa(*reinterpret_cast<const in_addr *>(host->h_addr_list[0]));
			metin2::log::info("server.proxy_ip {} resolves to {}", server.proxy_ip, proxy_ip);
		}
		else
		{
			metin2::log::error("cannot resolve server.proxy_ip {}", server.proxy_ip);
			exit(1);
		}
	}

	no_more_client = metin2::config::game().refuse_clients;

	for (long index : metin2::config::game().map_allow)
		set_map_allows.insert(static_cast<int>(index));

	if (metin2::config::game().max_level < 1 || metin2::config::game().max_level > PLAYER_MAX_LEVEL_CONST)
	{
		metin2::log::error("game.max_level must be from 1 to {:d}", PLAYER_MAX_LEVEL_CONST);
		exit(1);
	}

	// The Common DB holds the locale info, so connect to it first.
	account_db::instance().connect(db.host.c_str(), db.port, db.user.c_str(), db.password.c_str(), db.common.c_str());

	if (false == account_db::instance().is_connected())
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

		std::unique_ptr<ql_msg> pMsg(account_db::instance().direct_query(szQuery));

		if (pMsg->get()->num_rows == 0)
		{
			metin2::log::error( "COMMON_SQL: DirectQuery failed : {}", szQuery);
			exit(1);
		}

		MYSQL_ROW row; 

		while (nullptr != (row = mysql_fetch_row(pMsg->get()->sql_result)))
		{
			// Locale setup
			if (strcasecmp(row[0], "LOCALE") == 0)
			{
				if (locale_service_init(row[1]) == false)
				{
					metin2::log::error( "COMMON_SQL: invalid locale key {}", row[1]);
					exit(1);
				}
			}
		}
	}

	// Set the locale info on the COMMON SQL.
	// Note: g_stLocale is set inside LocaleService_Init().
	metin2::log::info( "Setting DB to locale {}", locale.c_str());

	account_db::instance().set_locale(locale);

	account_db::instance().connect_async(db.host.c_str(), db.port, db.user.c_str(), db.password.c_str(), db.common.c_str(), locale.c_str());

	// Connect to Player DB
	// The auth server reads accounts where the game reads players.
	const std::string & player_db = metin2::config::is_auth() ? db.account : db.player;
	db_manager::instance().connect(db.host.c_str(), db.port, db.user.c_str(), db.password.c_str(), player_db.c_str());

	if (!db_manager::instance().is_connected())
	{
		metin2::log::error( "PlayerSQL.ConnectError");
		exit(1);
	}

	metin2::log::info( "PlayerSQL connected");

	if (!metin2::config::is_auth())
	{
		// Connect to Log DB
		log_manager::instance().connect(db.host.c_str(), db.port, db.user.c_str(), db.password.c_str(), db.log.c_str());

		if (!log_manager::instance().is_connected())
		{
			metin2::log::error( "LogSQL.ConnectError");
			exit(1);
		}

		metin2::log::info( "LogSQL connected");

		log_manager::instance().boot_log(server.hostname.c_str(), server.channel);
	}

	// SKILL_POWER_BY_LEVEL
	// Due to string comparison issues, do this after AccountDB::instance().SetLocale(g_stLocale).
	// Of course this is not much of a problem domestically (overseas is the problem)
	{
		char szQuery[256];
		snprintf(szQuery, sizeof(szQuery), "SELECT mValue FROM locale WHERE mKey='SKILL_POWER_BY_LEVEL'");
		std::unique_ptr<ql_msg> pMsg(account_db::instance().direct_query(szQuery));

		if (pMsg->get()->num_rows == 0)
		{
			metin2::log::error( "[SKILL_PERCENT] Query failed: {}", szQuery);
			exit(1);
		}

		MYSQL_ROW row; 

		row = mysql_fetch_row(pMsg->get()->sql_result);

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
			std::unique_ptr<ql_msg> pMsg(account_db::instance().direct_query(szQuery));

			// Use the default table if not set.
			if (pMsg->get()->num_rows == 0)
			{
				table_by_skill::instance().set_skill_power_by_level_from_type(job, aiBaseSkillPowerByLevelTable);
				continue;
			}

			row = mysql_fetch_row(pMsg->get()->sql_result);
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

			table_by_skill::instance().set_skill_power_by_level_from_type(job, aiSkillTable);
		}		
	}
	// END_SKILL_POWER_BY_LEVEL


	if (!metin2::config::game().quest.dir.empty())
		quest_dir = metin2::config::game().quest.dir;
	for (const std::string & dir : metin2::config::game().quest.object_dirs)
		quest_object_dir.insert(dir);

	// LOCALE_SERVICE
	locale_service_load_locale_string_file();
	locale_service_transfer_default_setting();
	locale_service_load_empire_text_convert_tables();
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

	load_state_user_count();

	war_map_manager::instance().load_war_map_info(nullptr);
}

const char* get_table_postfix()
{
	return metin2::config::database().table_postfix.c_str();
}

void check_client_version()
{
	check_client_version_enabled = true;

	const desc_manager::desc_set & set = desc_manager::instance().client_set();
	desc_manager::desc_set::const_iterator it = set.begin();

	while (it != set.end())
	{
		desc * d = *(it++);

		if (!d->get_character())
			continue;

		int version = atoi(client_version.c_str());
		int date	= atoi(d->client_version() );

		if (version != date) // Fix
		{
			d->get_character()->chat_packet(CHAT_TYPE_NOTICE, lc_text("클라이언트 버전이 틀려 로그아웃 됩니다. 정상적으로 패치 후 접속하세요."));
			d->delayed_disconnect(10);
		}
	}
}

void load_state_user_count()
{
	FILE * fp = fopen("state_user_count", "r");

	if (!fp)
		return;

	fscanf(fp, " %d %d ", &full_user_count, &busy_user_count);

	fclose(fp);
}
}
