#include <metin2/core/main.h>
#include <metin2/game/app/main.h>

#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/ani.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/characters/horsename_manager.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/characters/motion.h>
#include <metin2/game/characters/polymorph.h>
#include <metin2/game/combat/pvp.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/combat/skill_power.h>
#include <metin2/game/data/text_file_loader.h>
#include <metin2/game/empire/monarch.h>
#include <metin2/game/empire/priv_manager.h>
#include <metin2/game/empire/threeway_war.h>
#include <metin2/game/events/arena.h>
#include <metin2/game/events/battle_arena.h>
#include <metin2/game/events/dragon_lair.h>
#include <metin2/game/events/fishing.h>
#include <metin2/game/events/ox_event.h>
#include <metin2/game/events/speed_server.h>
#include <metin2/game/guilds/building.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/guilds/mark_manager.h>
#include <metin2/game/guilds/war_map.h>
#include <metin2/game/items/blend_item.h>
#include <metin2/game/items/cube.h>
#include <metin2/game/items/dragon_soul.h>
#include <metin2/game/items/item_addon.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/items/over9refine.h>
#include <metin2/game/items/refine.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/protocol/lzo_manager.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/quest/target.h>
#include <metin2/game/social/marriage.h>
#include <metin2/game/social/messenger_manager.h>
#include <metin2/game/social/wedding.h>
#include <metin2/game/text/banword.h>
#include <metin2/game/text/spam.h>
#include <metin2/game/trade/shop_manager.h>
#include <metin2/game/world/dungeon.h>
#include <metin2/game/world/regen.h>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/common/startup.h>
#include <metin2/config/config.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/net/io.h>
#include <metin2/net/listener.h>
#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include <sys/time.h>
#include <unistd.h>

namespace metin2::game::inline app {

    volatile int num_events_called = 0;

    std::shared_ptr<net::listener> client_listener;
    std::shared_ptr<net::listener> p2p_listener;

    auto io_loop() -> int;

    auto start(int argc, char **argv) -> int;
    auto idle() -> int;
    void destroy();

    void test();

    enum profile {
        PROF_EVENT,
        PROF_CHR_UPDATE,
        PROF_IO,
        PROF_HEARTBEAT,
        PROF_MAX_NUM
    };

    static uint32_t profiler[PROF_MAX_NUM];

    int shutdown_disconnect_pulse;
    int shutdown_disconnect_force_pulse;
    bool shutting_down = false;

    void continue_on_fatal_error() {
        metin2::log::error("Fatal error, continuing");
    }

    namespace {
        struct send_disconnect_func {
            void operator()(desc *d) {
                if (d->get_character()) {
                    if (d->get_character()->gm_level() == GM_PLAYER)
                        d->get_character()->chat_packet(CHAT_TYPE_COMMAND, "quit Shutdown(SendDisconnectFunc)");
                }
            }
        };

        struct disconnect_func {
            void operator()(desc *d) {
                if (d->type() == DESC_TYPE_CONNECTOR)
                    return;

                if (d->is_phase(PHASE_P2P))
                    return;

                d->set_phase(PHASE_CLOSE);
            }
        };
    }

    void heartbeat(int pulse) {
        uint32_t t;

        t = get_dword_time();
        num_events_called += event_process(pulse);
        profiler[PROF_EVENT] += (get_dword_time() - t);

        t = get_dword_time();

        if (!(pulse % thecore_passes_per_sec())) {

            if (!metin2::config::is_auth()) {
                player_count_packet pack;
                pack.count = desc_manager::instance().local_user_count();
                db_clientdesc->db_packet(HEADER_GD_PLAYER_COUNT, 0, &pack, sizeof(player_count_packet));
            } else {
                desc_manager::instance().process_expired_login_key();
            }
        }

        if (!(pulse % (metin2::config::server().passes_per_sec + 4)))
            character_manager::instance().process_delayed_save();

        if (!(pulse % (metin2::config::server().passes_per_sec * 5 + 2))) {
            item_manager::instance().update();
            desc_manager::instance().update_local_user_count();
        }

        profiler[PROF_HEARTBEAT] += (get_dword_time() - t);

        db_manager::instance().process();
        account_db::instance().process();
        pvp_manager::instance().process();

        if (shutting_down) {
            if (thecore_pulse() > shutdown_disconnect_pulse) {
                const desc_manager::desc_set &c_set_desc = desc_manager::instance().client_set();
                std::for_each(c_set_desc.begin(), c_set_desc.end(), metin2::game::send_disconnect_func());
                shutdown_disconnect_pulse = INT_MAX;
            } else if (thecore_pulse() > shutdown_disconnect_force_pulse) {
                const desc_manager::desc_set &c_set_desc = desc_manager::instance().client_set();
                std::for_each(c_set_desc.begin(), c_set_desc.end(), metin2::game::disconnect_func());
            } else if (thecore_pulse() > shutdown_disconnect_force_pulse + passes_per_sec(5)) {
                thecore_shutdown();
            }
        }
    }

    static void clean_up_for_early_exit() {
        cancel_reload_spam_event();
    }

    auto run(int argc, char **argv) -> int {

        sectree_manager sectree_manager;
        character_manager char_manager;
        item_manager item_manager;
        shop_manager shop_manager;
        mob_manager mob_manager;
        motion_manager motion_manager;
        party_manager party_manager;
        skill_manager skill_manager;
        pvp_manager pvp_manager;
        lzo_manager lzo_manager;
        db_manager db_manager;
        account_db account_db;

        log_manager log_manager;
        messenger_manager messenger_manager;
        p2p_manager p2p_manager;
        guild_manager guild_manager;
        guild_mark_manager mark_manager;
        dungeon_manager dungeon_manager;
        refine_manager refine_manager;
        banword_manager banword_manager;
        priv_manager priv_manager;
        war_map_manager war_map_manager;
        building::manager building_manager;
        target_manager target_manager;
        marriage::manager marriage_manager;
        marriage::wedding_manager wedding_manager;
        item_addon_manager item_addon_manager;
        arena_manager arena_manager;
        ox_event_manager OXEvent_manager;
        monarch Monarch;
        horse_name_manager horsename_manager;

        desc_manager desc_manager;

        table_by_skill SkillPowerByLevel;
        polymorph_utils polymorph_utils;
        battle_arena ba;
        over9_refine_manager o9r;
        spam_manager spam_mgr;
        three_way_war threeway_war;
        dragon_lair_manager dl_manager;

        speed_server_manager SSManager;
        ds_manager dsManager;

        if (!start(argc, argv)) {
            clean_up_for_early_exit();
            return 1;
        }

        quest::quest_manager quest_manager;

        if (!quest_manager.initialize()) {
            clean_up_for_early_exit();
            return 1;
        }

        game::social::messenger_manager::instance().initialize();
        game::guilds::guild_manager::instance().initialize();
        fishing::initialize();
        OXEvent_manager.initialize();
        if (metin2::config::game().speed_server)
            speed_server_manager::instance().initialize();

        cube_init();
        blend_item_init();
        ani_init();

        if (metin2::config::is_auth() && !mark_server_ready()) thecore_shutdown();
        while (idle())
            ;

        metin2::log::info("Shutting down");
        shutting_down = true;
        no_more_client = true;

        if (metin2::config::is_auth()) {
            int iLimit = game::persistence::db_manager::instance().count_query() / 50;
            int i = 0;

            do {
                uint32_t dwCount = game::persistence::db_manager::instance().count_query();
                metin2::log::debug("Shutdown: {:d} queries pending", dwCount);

                if (dwCount == 0)
                    break;

                usleep(500000);

                if (++i >= iLimit)
                    if (dwCount == game::persistence::db_manager::instance().count_query())
                        break;
            } while (1);
        }

        metin2::log::debug("Shutdown: destroying arenas");
        arena_manager.destroy();
        metin2::log::debug("Shutdown: destroying ox event");
        OXEvent_manager.destroy();

        metin2::log::debug("Shutdown: disabling signal timer");
        signal_timer_disable();

        metin2::log::debug("Shutdown: saving characters");
        char_manager.graceful_shutdown();
        metin2::log::debug("Shutdown: saving items");
        item_manager.graceful_shutdown();

        metin2::log::debug("Shutdown: flushing db connection");
        db_clientdesc->flush_output();
        metin2::log::debug("Shutdown: flushing peer connections");
        p2p_manager.flush_output();

        metin2::log::debug("Shutdown: destroying shops");
        shop_manager.destroy();
        metin2::log::debug("Shutdown: destroying characters");
        char_manager.destroy();
        metin2::log::debug("Shutdown: destroying items");
        item_manager.destroy();
        metin2::log::debug("Shutdown: closing connections");
        desc_manager.destroy();
        metin2::log::debug("Shutdown: destroying quests");
        quest_manager.destroy();
        metin2::log::debug("Shutdown: destroying buildings");
        building_manager.destroy();

        destroy();

        return 0;
    }

    void usage() {
        printf("Option list\n"
               "-l <level>   : sets log level (above 0: debug messages)\n"
               "-v           : log to stdout\n");
    }

    auto start(int argc, char **argv) -> int {
        bool bVerbose = false;
        int ch;

        while ((ch = getopt(argc, argv, "vl:")) != -1) {
            char *ep = nullptr;

            switch (ch) {
            case 'l':
                if (strtol(optarg, &ep, 10) > 0)
                    metin2::log::set_level(metin2::log::level::debug);
                break;

            case 'v':
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

        if (!is_thecore_initialized) {
            fprintf(stderr, "Could not initialize thecore, check owner of pid, syslog\n");
            exit(0);
        }

        if (false == three_way_war::instance().load_setting("forkedmapindex.txt")) {
            if (false == metin2::config::is_auth()) {
                fprintf(stderr, "Could not Load ThreeWayWar Setting file");
                exit(0);
            }
        }

        signal_timer_disable();

        client_listener = net::listener::open(public_ip, metin2::config::server().port, [](net::tcp::socket socket) { desc_manager::instance().accept_desc(std::move(socket)); });

        if (!client_listener)
            return 0;

        p2p_listener = net::listener::open(public_ip, metin2::config::server().p2p_port, [](net::tcp::socket socket) { desc_manager::instance().accept_p2p_desc(std::move(socket)); });

        if (!p2p_listener)
            return 0;

        db_clientdesc = desc_manager::instance().create_connection_desc(metin2::config::server().db_host.c_str(), metin2::config::server().db_port, PHASE_DBCLIENT, true);
        if (!metin2::config::is_auth()) {
            db_clientdesc->update_channel_status(0, true);
        }

        if (metin2::config::is_auth()) {
            if (metin2::config::auth().master_host.length() != 0) {
                fprintf(stderr, "SlaveAuth");
                auth_master_desc = desc_manager::instance().create_connection_desc(metin2::config::auth().master_host.c_str(), metin2::config::auth().master_port, PHASE_P2P, true);
                p2p_manager::instance().register_connector(auth_master_desc);
                auth_master_desc->set_p2p(metin2::config::auth().master_host.c_str(), metin2::config::auth().master_port);

            } else {
                fprintf(stderr, "MasterAuth");
            }
        } else {

            metin2::log::info("Spam filter: block duration {:d}, block score {:d}, reload cycle {:d}",
                metin2::config::game().spam.block_duration, metin2::config::game().spam.block_score, metin2::config::game().spam.reload_cycle);

            load_spam_db();
        }

        signal_timer_enable(30);
        return 1;
    }

    void destroy() {
        metin2::log::debug("Shutdown: cancelling spam reload");
        cancel_reload_spam_event();

        metin2::log::debug("Shutdown: freeing regens");
        regen_free();

        metin2::log::debug("Shutdown: closing listeners");
        client_listener->close();
        p2p_listener->close();

        metin2::log::debug("Shutdown: destroying events");
        event_destroy();

        metin2::log::debug("Shutdown: releasing text file loader");
        text_file_loader::destroy_system();

        metin2::log::debug("Shutdown: releasing core");
        thecore_destroy();
    }

    auto idle() -> int {
        static struct timeval pta = {0, 0};
        static int process_time_count = 0;
        struct timeval now;

        if (pta.tv_sec == 0)
            gettimeofday(&pta, (struct timezone *)nullptr);

        int passed_pulses;

        if (!(passed_pulses = thecore_idle()))
            return 0;

        assert(passed_pulses > 0);

        uint32_t t;

        while (passed_pulses--) {
            heartbeat(thecore_next_pulse());

            thecore_tick();
        }

        t = get_dword_time();
        character_manager::instance().update(thecore_pulse());
        db_clientdesc->update(t);
        profiler[PROF_CHR_UPDATE] += (get_dword_time() - t);

        t = get_dword_time();
        if (!io_loop()) return 0;
        profiler[PROF_IO] += (get_dword_time() - t);

        metin2::log::rotate();

        gettimeofday(&now, (struct timezone *)nullptr);
        ++process_time_count;

        if (now.tv_sec - pta.tv_sec > 0) {
            static uint64_t s_qwBytesWritten = 0;
            const uint64_t qwBytesWritten = net::bytes_written();

            metin2::log::debug("Perf: {:d} loops, events {:d}/{:d}, idle {:d}, event {:d}, heartbeat {:d}, io {:d}, character update {:d}, written {:d} bytes, pulse {:d}",
                process_time_count,
                num_events_called,
                event_count(),
                thecore_idle_time,
                profiler[PROF_EVENT],
                profiler[PROF_HEARTBEAT],
                profiler[PROF_IO],
                profiler[PROF_CHR_UPDATE],
                (unsigned long long)(qwBytesWritten - s_qwBytesWritten),
                thecore_pulse());

            num_events_called = 0;
            s_qwBytesWritten = qwBytesWritten;

            process_time_count = 0;
            gettimeofday(&pta, (struct timezone *)nullptr);

            thecore_idle_time = 0;
            memset(&profiler[0], 0, sizeof(profiler));
        }

        return 1;
    }

    auto io_loop() -> int {
        desc_manager::instance().destroy_closed();
        desc_manager::instance().try_connect();

        net::io().poll();
        return 1;
    }
}
