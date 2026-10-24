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
#include <metin2/game/protocol/ip_ban.h>
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
#include <metin2/proto/character_types.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdint>
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

    auto start() -> bool;
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
                pack.count = get_desc_manager().local_user_count();
                db_clientdesc->db_packet(HEADER_GD_PLAYER_COUNT, 0, &pack, sizeof(player_count_packet));
            } else {
                get_desc_manager().process_expired_login_key();
            }
        }

        if (!(pulse % (metin2::config::server().passes_per_sec + 4)))
            get_character_manager().process_delayed_save();

        if (!(pulse % (metin2::config::server().passes_per_sec * 5 + 2))) {
            get_item_manager().update();
            get_desc_manager().update_local_user_count();
        }

        profiler[PROF_HEARTBEAT] += (get_dword_time() - t);

        get_db_manager().process();
        get_account_db().process();
        get_pvp_manager().process();

        if (shutting_down) {
            if (thecore_pulse() > shutdown_disconnect_pulse) {
                const desc_manager::desc_set &descs = get_desc_manager().client_set();
                std::ranges::for_each(descs, metin2::game::send_disconnect_func());
                shutdown_disconnect_pulse = INT_MAX;
            } else if (thecore_pulse() > shutdown_disconnect_force_pulse) {
                const desc_manager::desc_set &descs = get_desc_manager().client_set();
                std::ranges::for_each(descs, metin2::game::disconnect_func());
            } else if (thecore_pulse() > shutdown_disconnect_force_pulse + passes_per_sec(5)) {
                thecore_shutdown();
            }
        }
    }

    static void clean_up_for_early_exit() {
        cancel_reload_spam_event();
    }

    auto run() -> int {

        get_sectree_manager();
        get_character_manager();
        get_item_manager();
        get_shop_manager();
        get_mob_manager();
        get_motion_manager();
        get_party_manager();
        get_skill_manager();
        get_pvp_manager();
        get_lzo_manager();
        get_db_manager();
        get_account_db();

        get_log_manager();
        get_messenger_manager();
        get_p2p_manager();
        get_guild_manager();
        get_guild_mark_manager();
        get_dungeon_manager();
        get_refine_manager();
        get_banword_manager();
        get_priv_manager();
        get_war_map_manager();
        building::get_manager();
        get_target_manager();
        marriage::get_manager();
        marriage::get_wedding_manager();
        get_item_addon_manager();
        get_arena_manager();
        get_ox_event_manager();
        get_monarch();
        get_horse_name_manager();

        get_desc_manager();

        get_table_by_skill();
        get_polymorph_utils();
        get_battle_arena();
        get_over9_refine_manager();
        get_spam_manager();
        get_three_way_war();
        get_dragon_lair_manager();

        get_speed_server_manager();
        get_ds_manager();

        if (!start()) {
            clean_up_for_early_exit();
            return 1;
        }

        quest::get_quest_manager();

        if (!quest::get_quest_manager().initialize()) {
            clean_up_for_early_exit();
            return 1;
        }

        game::social::get_messenger_manager().initialize();
        game::guilds::get_guild_manager().initialize();
        fishing::initialize();
        get_ox_event_manager().initialize();
        if (metin2::config::game().speed_server)
            get_speed_server_manager().initialize();

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
            int limit = game::persistence::get_db_manager().count_query() / 50;
            int i = 0;

            do {
                uint32_t count = game::persistence::get_db_manager().count_query();
                metin2::log::debug("Shutdown: {:d} queries pending", count);

                if (count == 0)
                    break;

                usleep(500000);

                if (++i >= limit)
                    if (count == game::persistence::get_db_manager().count_query())
                        break;
            } while (1);
        }

        metin2::log::debug("Shutdown: destroying arenas");
        get_arena_manager().destroy();
        metin2::log::debug("Shutdown: destroying ox event");
        get_ox_event_manager().destroy();

        metin2::log::debug("Shutdown: disabling signal timer");
        signal_timer_disable();

        metin2::log::debug("Shutdown: saving characters");
        get_character_manager().graceful_shutdown();
        metin2::log::debug("Shutdown: saving items");
        get_item_manager().graceful_shutdown();

        metin2::log::debug("Shutdown: flushing db connection");
        db_clientdesc->flush_output();
        metin2::log::debug("Shutdown: flushing peer connections");
        get_p2p_manager().flush_output();

        metin2::log::debug("Shutdown: destroying shops");
        get_shop_manager().destroy();
        metin2::log::debug("Shutdown: destroying characters");
        get_character_manager().destroy();
        metin2::log::debug("Shutdown: destroying items");
        get_item_manager().destroy();
        metin2::log::debug("Shutdown: closing connections");
        get_desc_manager().destroy();
        metin2::log::debug("Shutdown: destroying quests");
        quest::get_quest_manager().destroy();
        metin2::log::debug("Shutdown: destroying buildings");
        building::get_manager().destroy();

        destroy();

        return 0;
    }

    auto start() -> bool {
        config_init();

        if (!thecore_init(metin2::config::server().passes_per_sec)) {
            return false;
        }

        if (!get_three_way_war().load_setting("forkedmapindex.txt") && !metin2::config::is_auth()) {
            metin2::log::error("Cannot load the three-way war settings (forkedmapindex.txt)");
            return false;
        }

        signal_timer_disable();

        client_listener = net::listener::open(public_ip, metin2::config::server().port, [](net::tcp::socket socket) { get_desc_manager().accept_desc(std::move(socket)); });

        if (!client_listener)
            return false;

        p2p_listener = net::listener::open(public_ip, metin2::config::server().p2p_port, [](net::tcp::socket socket) { get_desc_manager().accept_p2p_desc(std::move(socket)); });

        if (!p2p_listener)
            return false;

        db_clientdesc = get_desc_manager().create_connection_desc(metin2::config::server().db_host.c_str(), metin2::config::server().db_port, PHASE_DBCLIENT, true);
        if (!metin2::config::is_auth()) {
            db_clientdesc->update_channel_status(0, true);
        }

        if (metin2::config::is_auth()) {
            load_ban_ip("BANIP");

            if (metin2::config::auth().master_host.length() != 0) {
                metin2::log::info("Auth: slave of {}:{:d}", metin2::config::auth().master_host, metin2::config::auth().master_port);
                auth_master_desc = get_desc_manager().create_connection_desc(metin2::config::auth().master_host.c_str(), metin2::config::auth().master_port, PHASE_P2P, true);
                get_p2p_manager().register_connector(auth_master_desc);
                auth_master_desc->set_p2p(metin2::config::auth().master_host.c_str(), metin2::config::auth().master_port);
            } else {
                metin2::log::info("Auth: master");
            }
        } else {

            metin2::log::info("Spam filter: block duration {:d}, block score {:d}, reload cycle {:d}",
                metin2::config::game().spam.block_duration, metin2::config::game().spam.block_score, metin2::config::game().spam.reload_cycle);

            load_spam_db();
        }

        signal_timer_enable(30);
        return true;
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

        // Input that arrived while the loop slept is handled before the pulse simulates it, so an
        // action waits for at most the rest of this pass instead of a whole pulse; the poll after the
        // update then sends what the pulse produced.
        if (!io_loop()) return 0;

        while (passed_pulses--) {
            heartbeat(thecore_next_pulse());

            thecore_tick();
        }

        t = get_dword_time();
        get_character_manager().update(thecore_pulse());
        db_clientdesc->update(t);
        profiler[PROF_CHR_UPDATE] += (get_dword_time() - t);

        t = get_dword_time();
        if (!io_loop()) return 0;
        profiler[PROF_IO] += (get_dword_time() - t);

        metin2::log::rotate();

        gettimeofday(&now, (struct timezone *)nullptr);
        ++process_time_count;

        if (now.tv_sec - pta.tv_sec > 0) {
            static uint64_t qw_bytes_written_value = 0;
            const uint64_t qw_bytes_written = net::bytes_written();

            metin2::log::debug("Perf: {:d} loops, events {:d}/{:d}, idle {:d}, event {:d}, heartbeat {:d}, io {:d}, character update {:d}, written {:d} bytes, pulse {:d}",
                process_time_count,
                num_events_called,
                event_count(),
                thecore_idle_time,
                profiler[PROF_EVENT],
                profiler[PROF_HEARTBEAT],
                profiler[PROF_IO],
                profiler[PROF_CHR_UPDATE],
                (unsigned long long)(qw_bytes_written - qw_bytes_written_value),
                thecore_pulse());

            num_events_called = 0;
            qw_bytes_written_value = qw_bytes_written;

            process_time_count = 0;
            gettimeofday(&pta, (struct timezone *)nullptr);

            thecore_idle_time = 0;
            memset(&profiler[0], 0, sizeof(profiler));
        }

        return 1;
    }

    auto io_loop() -> int {
        get_desc_manager().destroy_closed();
        get_desc_manager().try_connect();

        net::io().poll();
        return 1;
    }
}
