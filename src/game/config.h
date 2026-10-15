
#pragma once

#include <cstdint>
#include <ctime>
#include <set>
#include <string>
#include <vector>

enum
{
	ADDRESS_MAX_LEN = 15
};

void config_init(const std::string& st_localeServiceName); // default "" is CONFIG

extern char sql_addr[256];

extern uint16_t mother_port;
extern uint16_t p2p_port;

extern char db_addr[ADDRESS_MAX_LEN + 1];
extern uint16_t db_port;

extern int passes_per_sec;
inline auto PASSES_PER_SEC(auto sec) { return sec * passes_per_sec; }
extern int save_event_second_cycle;
extern int ping_event_second_cycle;
extern int test_server;
extern bool	guild_mark_server;
extern uint8_t guild_mark_min_level;
extern bool	distribution_test_server;

extern bool	g_bNoMoreClient;
extern bool	g_bNoRegen;

extern bool	g_bTrafficProfileOn;		///< If true, enables the TrafficProfiler.

extern uint8_t	g_bChannel;

extern bool	map_allow_find(int index);
extern void	map_allow_copy(int32_t * pl, int size);
extern bool	no_wander;

extern int	g_iUserLimit;
extern time_t	g_global_time;

const char *	get_table_postfix();

extern std::string	g_stHostname;
extern std::string	g_stLocale;
extern std::string	g_stLocaleFilename;

extern char		g_szPublicIP[16];
extern char		g_szInternalIP[16];

extern std::string	g_stProxyIP;

extern bool		g_bSkillDisable;

extern int		g_iFullUserCount;
extern int		g_iBusyUserCount;
extern void		LoadStateUserCount();

extern bool	g_bEmpireWhisper;

extern uint8_t	g_bAuthServer;

extern uint8_t	PK_PROTECT_LEVEL;

extern void	LoadValidCRCList();

extern std::string	g_stAuthMasterIP;
extern uint16_t		g_wAuthMasterPort;

extern std::string	g_stClientVersion;
extern bool		g_bCheckClientVersion;
extern void		CheckClientVersion();

extern std::string	g_stQuestDir;
extern std::set<std::string> g_setQuestObjectDir;

extern std::vector<std::string>	g_stAdminPageIP;
extern std::string	g_stAdminPagePassword;

extern int	SPEEDHACK_LIMIT_COUNT;
extern int 	SPEEDHACK_LIMIT_BONUS;

extern int g_iSyncHackLimitCount;

extern int g_server_id;
extern std::string g_strWebMallURL;

extern int VIEW_RANGE;
extern int VIEW_BONUS_RANGE;

extern bool g_bCheckMultiHack;
extern bool g_protectNormalPlayer;      // Outlaws cannot attack normal users in "peace mode"
extern bool g_noticeBattleZone;         // Show an info message when entering a neutral zone

extern uint32_t g_GoldDropTimeLimitValue;

extern int gPlayerMaxLevel;

extern bool g_BlockCharCreation;

extern int speed_server;
extern unsigned int g_uiSpamBlockDuration;
extern unsigned int g_uiSpamBlockScore;
extern unsigned int g_uiSpamReloadCycle;
extern std::string g_stBlockDate;
extern int openid_server;
extern char openid_host[256];
extern char openid_uri[256];
extern int g_iSpamBlockMaxLevel;
extern void map_allow_log();
