#include "main.h"
#include <libthecore/main.h>

#include "ani.h"
#include "arena.h"
#include "banword.h"
#include "BattleArena.h"
#include "blend_item.h"
#include "building.h"
#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "cube.h"
#include "db.h"
#include "desc.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "DragonLair.h"
#include "DragonSoul.h"
#include "dungeon.h"
#include "event.h"
#include "fishing.h"
#include "guild_manager.h"
#include "horsename_manager.h"
#include "input.h"
#include "item_addon.h"
#include "item_manager.h"
#include "log.h"
#include "login_sim.h"
#include "lzo_manager.h"
#include "MarkManager.h"
#include "marriage.h"
#include "messenger_manager.h"
#include "mob_manager.h"
#include "monarch.h"
#include "motion.h"
#include "over9refine.h"
#include "OXEvent.h"
#include "p2p.h"
#include "packet.h"
#include "party.h"
#include "polymorph.h"
#include "priv_manager.h"
#include "pvp.h"
#include "questmanager.h"
#include "refine.h"
#include "regen.h"
#include "sectree_manager.h"
#include "shop_manager.h"
#include "skill.h"
#include "skill_power.h"
#include "spam.h"
#include "SpeedServer.h"
#include "target.h"
#include "text_file_loader.h"
#include "threeway_war.h"
#include "TrafficProfiler.h"
#include "typedef.h"
#include "war_map.h"
#include "wedding.h"

#include <common/length.h>
#include <common/startup.h>
#include <common/tables.h>
#include <libthecore/buffer.h>
#include <libthecore/core_signal.h>
#include <libthecore/fdwatch.h>
#include <libthecore/heart.h>
#include <libthecore/log.h>
#include <libthecore/socket.h>
#include <libthecore/typedef.h>
#include <libthecore/utils.h>

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <sys/time.h>
#include <unistd.h>
#include <unistd.h>

// Always check the server key when testing on Windows

// TRAFFIC_PROFILER
static const uint32_t	TRAFFIC_PROFILE_FLUSH_CYCLE = 3600;	///< TrafficProfiler flush cycle. Every hour
// END_OF_TRAFFIC_PROFILER

// Socket connected to the game
volatile int	num_events_called = 0;
int             max_bytes_written = 0;
int             current_bytes_written = 0;
int             total_bytes_written = 0;

socket_t	tcp_socket = 0;
socket_t	p2p_socket = 0;

fdwatch *	main_fdw = nullptr;

int		io_loop(fdwatch * fdw);

int		start(int argc, char **argv);
int		idle();
void	destroy();

void 	test();

enum EProfile
{
	PROF_EVENT,
	PROF_CHR_UPDATE,
	PROF_IO,
	PROF_HEARTBEAT,
	PROF_MAX_NUM
};

static uint32_t s_dwProfiler[PROF_MAX_NUM];

int g_shutdown_disconnect_pulse;
int g_shutdown_disconnect_force_pulse;
bool g_bShutdown=false;

void ContinueOnFatalError()
{
	sys_err("FatalError");
}

namespace
{
	struct SendDisconnectFunc
	{
		void operator () (DESC * d)
		{
			if (d->GetCharacter())
			{
				if (d->GetCharacter()->GetGMLevel() == GM_PLAYER)
					d->GetCharacter()->ChatPacket(CHAT_TYPE_COMMAND, "quit Shutdown(SendDisconnectFunc)");
			}
		}
	};

	struct DisconnectFunc
	{
		void operator () (DESC * d)
		{
			if (d->GetType() == DESC_TYPE_CONNECTOR)
				return;

			if (d->IsPhase(PHASE_P2P))
				return;

			d->SetPhase(PHASE_CLOSE);
		}
	};
}

unsigned int save_idx = 0;

void heartbeat(heart * ht, int pulse) 
{
	uint32_t t;

	t = get_dword_time();
	num_events_called += event_process(pulse);
	s_dwProfiler[PROF_EVENT] += (get_dword_time() - t);

	t = get_dword_time();

	// Every second
	if (!(pulse % ht->passes_per_sec))
	{

		if (!g_bAuthServer)
		{
			TPlayerCountPacket pack;
			pack.dwCount = DESC_MANAGER::instance().GetLocalUserCount();
			db_clientdesc->DBPacket(HEADER_GD_PLAYER_COUNT, 0, &pack, sizeof(TPlayerCountPacket));
		}
		else
		{
			DESC_MANAGER::instance().ProcessExpiredLoginKey();
		}

		{
			int count = 0;
			auto it = g_sim.begin();

			while (it != g_sim.end())
			{
				if (!it->second->IsCheck())
				{
					it->second->SendLogin();

					if (++count > 50)
					{
						sys_log(0, "FLUSH_SENT");
						break;
					}
				}

				it++;
			}

			if (save_idx < g_vec_save.size())
			{
				count = std::min<int>(100, g_vec_save.size() - save_idx);

				for (int i = 0; i < count; ++i, ++save_idx)
					db_clientdesc->DBPacket(HEADER_GD_PLAYER_SAVE, 0, &g_vec_save[save_idx], sizeof(TPlayerTable));

				sys_log(0, "SAVE_FLUSH %d", count);
			}
		}
	}

	//
	// Assuming 25 PPS (pulses per second)
	//

	// About every 1.16 seconds
	if (!(pulse % (passes_per_sec + 4)))
		CHARACTER_MANAGER::instance().ProcessDelayedSave();

	//Every 4 seconds

	// About every 5.08 seconds
	if (!(pulse % (passes_per_sec * 5 + 2)))
	{
		ITEM_MANAGER::instance().Update();
		DESC_MANAGER::instance().UpdateLocalUserCount();
	}

	s_dwProfiler[PROF_HEARTBEAT] += (get_dword_time() - t);

	DBManager::instance().Process();
	AccountDB::instance().Process();
	CPVPManager::instance().Process();

	if (g_bShutdown)
	{
		if (thecore_pulse() > g_shutdown_disconnect_pulse)
		{
			const DESC_MANAGER::DESC_SET & c_set_desc = DESC_MANAGER::instance().GetClientSet();
			std::for_each(c_set_desc.begin(), c_set_desc.end(), ::SendDisconnectFunc());
			g_shutdown_disconnect_pulse = INT_MAX;
		}
		else if (thecore_pulse() > g_shutdown_disconnect_force_pulse)
		{
			const DESC_MANAGER::DESC_SET & c_set_desc = DESC_MANAGER::instance().GetClientSet();
			std::for_each(c_set_desc.begin(), c_set_desc.end(), ::DisconnectFunc());
		}
		else if (thecore_pulse() > g_shutdown_disconnect_force_pulse + PASSES_PER_SEC(5))
		{
			thecore_shutdown();
		}
	}
}

static void CleanUpForEarlyExit() {
	CancelReloadSpamEvent();
}

int main(int argc, char **argv)
{

	// // <Factor> start unit tests if option is set

	WriteVersion();

	SECTREE_MANAGER	sectree_manager;
	CHARACTER_MANAGER	char_manager;
	ITEM_MANAGER	item_manager;
	CShopManager	shop_manager;
	CMobManager		mob_manager;
	CMotionManager	motion_manager;
	CPartyManager	party_manager;
	CSkillManager	skill_manager;
	CPVPManager		pvp_manager;
	LZOManager		lzo_manager;
	DBManager		db_manager;
	AccountDB 		account_db;

	LogManager		log_manager;
	MessengerManager	messenger_manager;
	P2P_MANAGER		p2p_manager;
	CGuildManager	guild_manager;
	CGuildMarkManager mark_manager;
	CDungeonManager	dungeon_manager;
	CRefineManager	refine_manager;
	CBanwordManager	banword_manager;
	CPrivManager	priv_manager;
	CWarMapManager	war_map_manager;
	building::CManager	building_manager;
	CTargetManager	target_manager;
	marriage::CManager	marriage_manager;
	marriage::WeddingManager wedding_manager;
	CItemAddonManager	item_addon_manager;
	CArenaManager arena_manager;
	COXEventManager OXEvent_manager;
	CMonarch		Monarch;
	CHorseNameManager horsename_manager;

	DESC_MANAGER	desc_manager;

	TrafficProfiler	trafficProfiler;
	CTableBySkill SkillPowerByLevel;
	CPolymorphUtils polymorph_utils;
	CBattleArena	ba;
	COver9RefineManager	o9r;
	SpamManager		spam_mgr;
	CThreeWayWar	threeway_war;
	CDragonLairManager	dl_manager;

	CSpeedServerManager SSManager;
	DSManager dsManager;

	if (!start(argc, argv)) {
		CleanUpForEarlyExit();
		return 1;
	}

	quest::CQuestManager quest_manager;

	if (!quest_manager.Initialize()) {
		CleanUpForEarlyExit();
		return 1;
	}

	MessengerManager::instance().Initialize();
	CGuildManager::instance().Initialize();
	fishing::Initialize();
	OXEvent_manager.Initialize();
	if (speed_server)
		CSpeedServerManager::instance().Initialize();

	Cube_init();
	Blend_Item_init();
	ani_init();

	if ( g_bTrafficProfileOn )
		TrafficProfiler::instance().Initialize( TRAFFIC_PROFILE_FLUSH_CYCLE, "ProfileLog" );

	if (g_bAuthServer && !mark_server_ready()) thecore_shutdown();
	while (idle());

	sys_log(0, "<shutdown> Starting...");
	g_bShutdown = true;
	g_bNoMoreClient = true;

	if (g_bAuthServer)
	{
		int iLimit = DBManager::instance().CountQuery() / 50;
		int i = 0;

		do
		{
			uint32_t dwCount = DBManager::instance().CountQuery();
			sys_log(0, "Queries %u", dwCount);

			if (dwCount == 0)
				break;

			usleep(500000);

			if (++i >= iLimit)
				if (dwCount == DBManager::instance().CountQuery())
					break;
		} while (1);
	}

	sys_log(0, "<shutdown> Destroying CArenaManager...");
	arena_manager.Destroy();
	sys_log(0, "<shutdown> Destroying COXEventManager...");
	OXEvent_manager.Destroy();

	sys_log(0, "<shutdown> Disabling signal timer...");
	signal_timer_disable();

	sys_log(0, "<shutdown> Shutting down CHARACTER_MANAGER...");
	char_manager.GracefulShutdown();
	sys_log(0, "<shutdown> Shutting down ITEM_MANAGER...");
	item_manager.GracefulShutdown();

	sys_log(0, "<shutdown> Flushing db_clientdesc...");
	db_clientdesc->FlushOutput();
	sys_log(0, "<shutdown> Flushing p2p_manager...");
	p2p_manager.FlushOutput();

	sys_log(0, "<shutdown> Destroying CShopManager...");
	shop_manager.Destroy();
	sys_log(0, "<shutdown> Destroying CHARACTER_MANAGER...");
	char_manager.Destroy();
	sys_log(0, "<shutdown> Destroying ITEM_MANAGER...");
	item_manager.Destroy();
	sys_log(0, "<shutdown> Destroying DESC_MANAGER...");
	desc_manager.Destroy();
	sys_log(0, "<shutdown> Destroying quest::CQuestManager...");
	quest_manager.Destroy();
	sys_log(0, "<shutdown> Destroying building::CManager...");
	building_manager.Destroy();

	sys_log(0, "<shutdown> Flushing TrafficProfiler...");
	trafficProfiler.Flush();

	destroy();

	return 0;
}

void usage()
{
	printf("Option list\n"
			"-p <port>    : bind port number (port must be over 1024)\n"
			"-l <level>   : sets log level\n"
			"-v           : log to stdout\n"
			"-r           : do not load regen tables\n"
			"-t           : traffic proflie on\n");
}

int start(int argc, char **argv)
{
	std::string st_localeServiceName;

	bool bVerbose = false;
	int ch;

	while ((ch = getopt(argc, argv, "n:p:verl:tI:")) != -1)
	{
		char* ep = nullptr;

		switch (ch)
		{
			case 'I': // IP
				strlcpy(g_szPublicIP, optarg, sizeof(g_szPublicIP));

				printf("IP %s\n", g_szPublicIP);

				break;

			case 'p': // port
				mother_port = strtol(optarg, &ep, 10);

				if (mother_port <= 1024)
				{
					usage();
					return 0;
				}

				printf("port %d\n", mother_port);

				break;

			case 'l':
				{
					int32_t l = strtol(optarg, &ep, 10);

					log_set_level(l);

				}
				break;

				// LOCALE_SERVICE
			case 'n': 
				{
					if (optarg)
					{
						st_localeServiceName = optarg;
					}
				}
				break;
				// END_OF_LOCALE_SERVICE

			case 'v': // verbose
				bVerbose = true;
				break;

			case 'r':
				g_bNoRegen = true;
				break;

				// TRAFFIC_PROFILER
			case 't':
				g_bTrafficProfileOn = true;
				break;
				// END_OF_TRAFFIC_PROFILER
		}
	}

	// LOCALE_SERVICE
	config_init(st_localeServiceName);
	// END_OF_LOCALE_SERVICE

	if (!bVerbose)
		freopen("stdout", "a", stdout);

	bool is_thecore_initialized = thecore_init(25);

	if (!is_thecore_initialized)
	{
		fprintf(stderr, "Could not initialize thecore, check owner of pid, syslog\n");
		exit(0);
	}

	if (false == CThreeWayWar::instance().LoadSetting("forkedmapindex.txt"))
	{
		if (false == g_bAuthServer)
		{
			fprintf(stderr, "Could not Load ThreeWayWar Setting file");
			exit(0);
		}
	}

	signal_timer_disable();

	main_fdw = fdwatch_new(4096);

	if ((tcp_socket = socket_tcp_bind(g_szPublicIP, mother_port)) == INVALID_SOCKET)
	{
		perror("socket_tcp_bind: tcp_socket");
		return 0;
	}

	// if internal ip exists, p2p socket uses internal ip, if not use public ip
	if ((p2p_socket = socket_tcp_bind(g_szPublicIP, p2p_port)) == INVALID_SOCKET)
	{
		perror("socket_tcp_bind: p2p_socket");
		return 0;
	}

	fdwatch_add_fd(main_fdw, tcp_socket, nullptr, FDW_READ, false);
	fdwatch_add_fd(main_fdw, p2p_socket, nullptr, FDW_READ, false);

	db_clientdesc = DESC_MANAGER::instance().CreateConnectionDesc(main_fdw, db_addr, db_port, PHASE_DBCLIENT, true);
	if (!g_bAuthServer) {
		db_clientdesc->UpdateChannelStatus(0, true);
	}

	if (g_bAuthServer)
	{
		if (g_stAuthMasterIP.length() != 0)
		{
			fprintf(stderr, "SlaveAuth");
			g_pkAuthMasterDesc = DESC_MANAGER::instance().CreateConnectionDesc(main_fdw, g_stAuthMasterIP.c_str(), g_wAuthMasterPort, PHASE_P2P, true); 
			P2P_MANAGER::instance().RegisterConnector(g_pkAuthMasterDesc);
			g_pkAuthMasterDesc->SetP2P(g_stAuthMasterIP.c_str(), g_wAuthMasterPort);

		}
		else
		{
			fprintf(stderr, "MasterAuth");
		}
	}
	else
	{

		sys_log(0, "SPAM_CONFIG: duration %u score %u reload cycle %u\n",
				g_uiSpamBlockDuration, g_uiSpamBlockScore, g_uiSpamReloadCycle);

		LoadSpamDB();
	}

	signal_timer_enable(30);
	return 1;
}

void destroy()
{
	sys_log(0, "<shutdown> Canceling ReloadSpamEvent...");
	CancelReloadSpamEvent();

	sys_log(0, "<shutdown> regen_free()...");
	regen_free();

	sys_log(0, "<shutdown> Closing sockets...");
	socket_close(tcp_socket);
	socket_close(p2p_socket);

	sys_log(0, "<shutdown> fdwatch_delete()...");
	fdwatch_delete(main_fdw);

	sys_log(0, "<shutdown> event_destroy()...");
	event_destroy();

	sys_log(0, "<shutdown> CTextFileLoader::DestroySystem()...");
	CTextFileLoader::DestroySystem();

	sys_log(0, "<shutdown> thecore_destroy()...");
	thecore_destroy();
}

int idle()
{
	static struct timeval	pta = { 0, 0 };
	static int			process_time_count = 0;
	struct timeval		now;

	if (pta.tv_sec == 0)
		gettimeofday(&pta, (struct timezone *) nullptr);

	int passed_pulses;

	if (!(passed_pulses = thecore_idle()))
		return 0;

	assert(passed_pulses > 0);

	uint32_t t;

	while (passed_pulses--) {
		heartbeat(thecore_heart, ++thecore_heart->pulse);

		// To reduce the possibility of abort() in checkpointing
		thecore_tick();
	}

	t = get_dword_time();
	CHARACTER_MANAGER::instance().Update(thecore_heart->pulse);
	db_clientdesc->Update(t);
	s_dwProfiler[PROF_CHR_UPDATE] += (get_dword_time() - t);

	t = get_dword_time();
	if (!io_loop(main_fdw)) return 0;
	s_dwProfiler[PROF_IO] += (get_dword_time() - t);

	log_rotate();

	gettimeofday(&now, (struct timezone *) nullptr);
	++process_time_count;

	if (now.tv_sec - pta.tv_sec > 0)
	{
		pt_log("[%3d] event %5d/%-5d idle %-4d event %-4d heartbeat %-4d I/O %-4d chrUpate %-4d | WRITE: %-7d | PULSE: %d",
				process_time_count,
				num_events_called,
				event_count(),
				thecore_idle_time,
				s_dwProfiler[PROF_EVENT],
				s_dwProfiler[PROF_HEARTBEAT],
				s_dwProfiler[PROF_IO],
				s_dwProfiler[PROF_CHR_UPDATE],
				current_bytes_written,
				thecore_pulse());

		num_events_called = 0;
		current_bytes_written = 0;

		process_time_count = 0; 
		gettimeofday(&pta, (struct timezone *) nullptr);

		thecore_idle_time = 0;
		memset(&s_dwProfiler[0], 0, sizeof(s_dwProfiler));
	}

	return 1;
}

int io_loop(fdwatch * fdw)
{
	DESC *	d;
	int		num_events, event_idx;

	DESC_MANAGER::instance().DestroyClosed(); // Close connections in PHASE_CLOSE.
	DESC_MANAGER::instance().TryConnect();

	if ((num_events = fdwatch_wait(fdw, nullptr)) < 0)
		return 0;

	for (event_idx = 0; event_idx < num_events; ++event_idx)
	{
		d = (DESC *) fdwatch_get_client_data(fdw, event_idx);

		if (!d)
		{
			if (FDW_READ == fdwatch_check_event(fdw, tcp_socket, event_idx))
			{
				DESC_MANAGER::instance().AcceptDesc(fdw, tcp_socket);
				fdwatch_clear_event(fdw, tcp_socket, event_idx);
			}
			else if (FDW_READ == fdwatch_check_event(fdw, p2p_socket, event_idx))
			{
				DESC_MANAGER::instance().AcceptP2PDesc(fdw, p2p_socket);
				fdwatch_clear_event(fdw, p2p_socket, event_idx);
			}
			continue; 
		}

		int iRet = fdwatch_check_event(fdw, d->GetSocket(), event_idx);

		switch (iRet)
		{
			case FDW_READ:
				if (db_clientdesc == d)
				{
					int size = d->ProcessInput();

					if (size)
						sys_log(1, "DB_BYTES_READ: %d", size);

					if (size < 0)
					{
						d->SetPhase(PHASE_CLOSE);
					}
				}
				else if (d->ProcessInput() < 0)
				{
					d->SetPhase(PHASE_CLOSE);
				}
				break;

			case FDW_WRITE:
				if (db_clientdesc == d)
				{
					int buf_size = buffer_size(d->GetOutputBuffer());
					int sock_buf_size = fdwatch_get_buffer_size(fdw, d->GetSocket());

					int ret = d->ProcessOutput();

					if (ret < 0)
					{
						d->SetPhase(PHASE_CLOSE);
					}

					if (buf_size)
						sys_log(1, "DB_BYTES_WRITE: size %d sock_buf %d ret %d", buf_size, sock_buf_size, ret);
				}
				else if (d->ProcessOutput() < 0)
				{
					d->SetPhase(PHASE_CLOSE);
				}
				break;

			case FDW_EOF:
				{
					d->SetPhase(PHASE_CLOSE);
				}
				break;

			default:
				sys_err("fdwatch_check_event returned unknown %d", iRet);
				d->SetPhase(PHASE_CLOSE);
				break;
		}
	}

	return 1;
}

