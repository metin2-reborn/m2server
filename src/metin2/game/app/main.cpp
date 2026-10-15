#include <metin2/game/app/main.h>
#include <metin2/core/main.h>

#include <metin2/game/characters/ani.h>
#include <metin2/game/events/arena.h>
#include <metin2/game/text/banword.h>
#include <metin2/game/events/BattleArena.h>
#include <metin2/game/items/blend_item.h>
#include <metin2/game/guilds/building.h>
#include <metin2/game/characters/char.h>
#include <metin2/game/characters/char_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/items/cube.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/events/DragonLair.h>
#include <metin2/game/items/DragonSoul.h>
#include <metin2/game/world/dungeon.h>
#include <metin2/game/app/event.h>
#include <metin2/game/events/fishing.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/characters/horsename_manager.h>
#include <metin2/game/protocol/input.h>
#include <metin2/game/items/item_addon.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/lzo_manager.h>
#include <metin2/game/guilds/MarkManager.h>
#include <metin2/game/social/marriage.h>
#include <metin2/game/social/messenger_manager.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/empire/monarch.h>
#include <metin2/game/characters/motion.h>
#include <metin2/game/items/over9refine.h>
#include <metin2/game/events/OXEvent.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/characters/polymorph.h>
#include <metin2/game/empire/priv_manager.h>
#include <metin2/game/combat/pvp.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/items/refine.h>
#include <metin2/game/world/regen.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/trade/shop_manager.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/combat/skill_power.h>
#include <metin2/game/text/spam.h>
#include <metin2/game/events/SpeedServer.h>
#include <metin2/game/quest/target.h>
#include <metin2/game/data/text_file_loader.h>
#include <metin2/game/empire/threeway_war.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/guilds/war_map.h>
#include <metin2/game/social/wedding.h>

#include <metin2/proto/length.h>
#include <metin2/common/startup.h>
#include <metin2/proto/tables.h>
#include <metin2/net/io.h>
#include <metin2/net/listener.h>
#include <metin2/core/core_signal.h>
#include <metin2/core/heart.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>
#include <metin2/config/config.h>

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <sys/time.h>
#include <unistd.h>

namespace metin2::game::inline app {

// Always check the server key when testing on Windows


volatile int	num_events_called = 0;

// Accept the game clients and the other servers' P2P connections.
std::shared_ptr<net::listener>	client_listener;
std::shared_ptr<net::listener>	p2p_listener;

int		io_loop();

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
	metin2::log::error("FatalError");
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

		if (!metin2::config::is_auth())
		{
			TPlayerCountPacket pack;
			pack.dwCount = DESC_MANAGER::instance().GetLocalUserCount();
			db_clientdesc->DBPacket(HEADER_GD_PLAYER_COUNT, 0, &pack, sizeof(TPlayerCountPacket));
		}
		else
		{
			DESC_MANAGER::instance().ProcessExpiredLoginKey();
		}
	}

	//
	// Assuming 25 PPS (pulses per second)
	//

	// About every 1.16 seconds
	if (!(pulse % (metin2::config::server().passes_per_sec + 4)))
		CHARACTER_MANAGER::instance().ProcessDelayedSave();

	//Every 4 seconds

	// About every 5.08 seconds
	if (!(pulse % (metin2::config::server().passes_per_sec * 5 + 2)))
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
			std::for_each(c_set_desc.begin(), c_set_desc.end(), metin2::game::SendDisconnectFunc());
			g_shutdown_disconnect_pulse = INT_MAX;
		}
		else if (thecore_pulse() > g_shutdown_disconnect_force_pulse)
		{
			const DESC_MANAGER::DESC_SET & c_set_desc = DESC_MANAGER::instance().GetClientSet();
			std::for_each(c_set_desc.begin(), c_set_desc.end(), metin2::game::DisconnectFunc());
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

int run(int argc, char **argv)
{

	// // <Factor> start unit tests if option is set


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
	if (metin2::config::game().speed_server)
		CSpeedServerManager::instance().Initialize();

	Cube_init();
	Blend_Item_init();
	ani_init();


	if (metin2::config::is_auth() && !mark_server_ready()) thecore_shutdown();
	while (idle());

	metin2::log::info( "<shutdown> Starting...");
	g_bShutdown = true;
	g_bNoMoreClient = true;

	if (metin2::config::is_auth())
	{
		int iLimit = DBManager::instance().CountQuery() / 50;
		int i = 0;

		do
		{
			uint32_t dwCount = DBManager::instance().CountQuery();
			metin2::log::info( "Queries {:d}", dwCount);

			if (dwCount == 0)
				break;

			usleep(500000);

			if (++i >= iLimit)
				if (dwCount == DBManager::instance().CountQuery())
					break;
		} while (1);
	}

	metin2::log::info( "<shutdown> Destroying CArenaManager...");
	arena_manager.Destroy();
	metin2::log::info( "<shutdown> Destroying COXEventManager...");
	OXEvent_manager.Destroy();

	metin2::log::info( "<shutdown> Disabling signal timer...");
	signal_timer_disable();

	metin2::log::info( "<shutdown> Shutting down CHARACTER_MANAGER...");
	char_manager.GracefulShutdown();
	metin2::log::info( "<shutdown> Shutting down ITEM_MANAGER...");
	item_manager.GracefulShutdown();

	metin2::log::info( "<shutdown> Flushing db_clientdesc...");
	db_clientdesc->FlushOutput();
	metin2::log::info( "<shutdown> Flushing p2p_manager...");
	p2p_manager.FlushOutput();

	metin2::log::info( "<shutdown> Destroying CShopManager...");
	shop_manager.Destroy();
	metin2::log::info( "<shutdown> Destroying CHARACTER_MANAGER...");
	char_manager.Destroy();
	metin2::log::info( "<shutdown> Destroying ITEM_MANAGER...");
	item_manager.Destroy();
	metin2::log::info( "<shutdown> Destroying DESC_MANAGER...");
	desc_manager.Destroy();
	metin2::log::info( "<shutdown> Destroying quest::CQuestManager...");
	quest_manager.Destroy();
	metin2::log::info( "<shutdown> Destroying building::CManager...");
	building_manager.Destroy();

	destroy();

	return 0;
}

void usage()
{
	printf("Option list\n"
			"-l <level>   : sets log level (above 0: debug messages)\n"
			"-v           : log to stdout\n");
}

int start(int argc, char **argv)
{
	bool bVerbose = false;
	int ch;

	while ((ch = getopt(argc, argv, "vl:")) != -1)
	{
		char* ep = nullptr;

		switch (ch)
		{
			case 'l':
				// Any level above 0 turns on debug messages.
				if (strtol(optarg, &ep, 10) > 0)
					metin2::log::set_level(metin2::log::level::debug);
				break;

			case 'v': // verbose
				bVerbose = true;
				break;

			default:
				usage();
				return 0;
		}
	}

	config_init();

	if (!bVerbose)
		freopen("stdout", "a", stdout);

	bool is_thecore_initialized = thecore_init(metin2::config::server().passes_per_sec);

	if (!is_thecore_initialized)
	{
		fprintf(stderr, "Could not initialize thecore, check owner of pid, syslog\n");
		exit(0);
	}

	if (false == CThreeWayWar::instance().LoadSetting("forkedmapindex.txt"))
	{
		if (false == metin2::config::is_auth())
		{
			fprintf(stderr, "Could not Load ThreeWayWar Setting file");
			exit(0);
		}
	}

	signal_timer_disable();

	client_listener = net::listener::open(g_szPublicIP, metin2::config::server().port, [](net::tcp::socket socket) { DESC_MANAGER::instance().AcceptDesc(std::move(socket)); });

	if (!client_listener)
		return 0;

	// if internal ip exists, p2p socket uses internal ip, if not use public ip
	p2p_listener = net::listener::open(g_szPublicIP, metin2::config::server().p2p_port, [](net::tcp::socket socket) { DESC_MANAGER::instance().AcceptP2PDesc(std::move(socket)); });

	if (!p2p_listener)
		return 0;

	db_clientdesc = DESC_MANAGER::instance().CreateConnectionDesc(metin2::config::server().db_host.c_str(), metin2::config::server().db_port, PHASE_DBCLIENT, true);
	if (!metin2::config::is_auth()) {
		db_clientdesc->UpdateChannelStatus(0, true);
	}

	if (metin2::config::is_auth())
	{
		if (metin2::config::auth().master_host.length() != 0)
		{
			fprintf(stderr, "SlaveAuth");
			g_pkAuthMasterDesc = DESC_MANAGER::instance().CreateConnectionDesc(metin2::config::auth().master_host.c_str(), metin2::config::auth().master_port, PHASE_P2P, true); 
			P2P_MANAGER::instance().RegisterConnector(g_pkAuthMasterDesc);
			g_pkAuthMasterDesc->SetP2P(metin2::config::auth().master_host.c_str(), metin2::config::auth().master_port);

		}
		else
		{
			fprintf(stderr, "MasterAuth");
		}
	}
	else
	{

		metin2::log::info( "SPAM_CONFIG: duration {:d} score {:d} reload cycle {:d}",
				metin2::config::game().spam.block_duration, metin2::config::game().spam.block_score, metin2::config::game().spam.reload_cycle);

		LoadSpamDB();
	}

	signal_timer_enable(30);
	return 1;
}

void destroy()
{
	metin2::log::info( "<shutdown> Canceling ReloadSpamEvent...");
	CancelReloadSpamEvent();

	metin2::log::info( "<shutdown> regen_free()...");
	regen_free();

	metin2::log::info( "<shutdown> Closing listeners...");
	client_listener->close();
	p2p_listener->close();

	metin2::log::info( "<shutdown> event_destroy()...");
	event_destroy();

	metin2::log::info( "<shutdown> CTextFileLoader::DestroySystem()...");
	CTextFileLoader::DestroySystem();

	metin2::log::info( "<shutdown> thecore_destroy()...");
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
	if (!io_loop()) return 0;
	s_dwProfiler[PROF_IO] += (get_dword_time() - t);

	metin2::log::rotate();

	gettimeofday(&now, (struct timezone *) nullptr);
	++process_time_count;

	if (now.tv_sec - pta.tv_sec > 0)
	{
		static uint64_t s_qwBytesWritten = 0;
		const uint64_t qwBytesWritten = net::bytes_written();

		metin2::log::debug("[perf] [{:3d}] event {:5d}/{:<5d} idle {:<4d} event {:<4d} heartbeat {:<4d} I/O {:<4d} chrUpate {:<4d} | WRITE: {:<7d} | PULSE: {:d}",
				process_time_count,
				num_events_called,
				event_count(),
				thecore_idle_time,
				s_dwProfiler[PROF_EVENT],
				s_dwProfiler[PROF_HEARTBEAT],
				s_dwProfiler[PROF_IO],
				s_dwProfiler[PROF_CHR_UPDATE],
				(unsigned long long) (qwBytesWritten - s_qwBytesWritten),
				thecore_pulse());

		num_events_called = 0;
		s_qwBytesWritten = qwBytesWritten;

		process_time_count = 0; 
		gettimeofday(&pta, (struct timezone *) nullptr);

		thecore_idle_time = 0;
		memset(&s_dwProfiler[0], 0, sizeof(s_dwProfiler));
	}

	return 1;
}

int io_loop()
{
	DESC_MANAGER::instance().DestroyClosed(); // Close connections in PHASE_CLOSE.
	DESC_MANAGER::instance().TryConnect();

	// Runs the network handlers that are ready (input, accepts, queued output); never blocks.
	net::io().poll();
	return 1;
}
}
