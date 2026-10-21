#include <metin2/config/config.h>
#include <metin2/game/guilds/guild_manager.h>

#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/guilds/mark_manager.h>
#include <metin2/game/guilds/war_map.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>

#include <metin2/log/log.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/guild_types.h>
#include <metin2/proto/tables.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>

namespace metin2::game::inline guilds {

    guild_manager::guild_manager() = default;

    guild_manager::~guild_manager() {
        guild_.clear();
    }

    auto guild_manager::disband_delay() -> int {
        return quest::get_quest_manager().event_flag("guild_disband_delay") * (metin2::config::test_server() ? 60 : 86400);
    }

    auto guild_manager::withdraw_delay() -> int {
        return quest::get_quest_manager().event_flag("guild_withdraw_delay") * (metin2::config::test_server() ? 60 : 86400);
    }

    auto guild_manager::create_guild(guild_create_parameter &gcp) -> uint32_t {
        if (!gcp.master)
            return 0;

        if (!check_name(gcp.name)) {
            gcp.master->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드 이름이 적합하지 않습니다."));
            return 0;
        }

        std::unique_ptr<sql_msg> pmsg(get_db_manager().direct_query(sql::format("SELECT COUNT(*) FROM guild WHERE name = {}", gcp.name)));

        if (pmsg->get()->num_rows > 0) {
            MYSQL_ROW row = mysql_fetch_row(pmsg->get()->res.get());

            if (!(row[0] && row[0][0] == '0')) {
                gcp.master->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 이미 같은 이름의 길드가 있습니다."));
                return 0;
            }
        } else {
            gcp.master->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드를 생성할 수 없습니다."));
            return 0;
        }

        auto pg = std::make_unique<guild>(gcp);
        const uint32_t guild_id = pg->id();
        guild_.insert(std::make_pair(guild_id, std::move(pg)));
        return guild_id;
    }

    void guild_manager::unlink(uint32_t pid) {
        guilds_by_pid_.erase(pid);
    }

    auto guild_manager::linked_guild(uint32_t pid) -> guild * {
        auto it = guilds_by_pid_.find(pid);

        if (it == guilds_by_pid_.end())
            return nullptr;

        return it->second;
    }

    void guild_manager::link(uint32_t pid, guild *guild) {
        if (guilds_by_pid_.find(pid) == guilds_by_pid_.end())
            guilds_by_pid_.insert(std::make_pair(pid, guild));
    }

    void guild_manager::p2p_logout_member(uint32_t pid) {
        auto it = guilds_by_pid_.find(pid);

        if (it != guilds_by_pid_.end()) {
            it->second->p2p_logout_member(pid);
        }
    }

    void guild_manager::p2p_login_member(uint32_t pid) {
        auto it = guilds_by_pid_.find(pid);

        if (it != guilds_by_pid_.end()) {
            it->second->p2p_login_member(pid);
        }
    }

    void guild_manager::login_member(character *ch) {
        auto it = guilds_by_pid_.find(ch->player_id());

        if (it != guilds_by_pid_.end()) {
            it->second->login_member(ch);
        }
    }

    auto guild_manager::touch_guild(uint32_t guild_id) -> guild * {
        auto it = guild_.find(guild_id);

        if (it == guild_.end()) {
            guild_.insert(std::make_pair(guild_id, std::make_unique<guild>(guild_id)));
            it = guild_.find(guild_id);

            send_guild_name_to_all(guild_id, it->second->name());
        }

        return it->second.get();
    }

    auto guild_manager::find_guild(uint32_t guild_id) -> guild * {
        auto it = guild_.find(guild_id);
        if (it == guild_.end()) {
            return nullptr;
        }
        return it->second.get();
    }

    auto guild_manager::find_guild_by_name(const std::string guild_name) -> guild * {
        decltype(guild_.begin()) it;
        for (it = guild_.begin(); it != guild_.end(); ++it) {
            if (it->second->name() == guild_name)
                return it->second.get();
        }
        return nullptr;
    }

    // The marks that come with the data (mark_seed, mounted read-only) are copied once, before any mark exists.
    void guild_manager::seed_marks() {
        namespace fs = std::filesystem;
        std::error_code error;
        if (fs::exists("mark/mark_index", error) || !fs::is_directory("mark_seed", error)) {
            return;
        }

        for (const auto &entry : fs::directory_iterator("mark_seed", error)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            if (!fs::copy_file(entry.path(), fs::path("mark") / entry.path().filename(), fs::copy_options::skip_existing, error) && error) {
                metin2::log::error("Guild marks: cannot copy {}: {}", entry.path().string(), error.message());
                return;
            }
        }
        metin2::log::info("Guild marks: seeded from mark_seed");
    }

    void guild_manager::initialize() {
        metin2::log::info("Loading guilds");

        if (metin2::config::is_auth()) {
            metin2::log::info("Guilds are not loaded on the auth server");
            return;
        }

        auto pmsg = get_db_manager().direct_query(sql::format("SELECT id FROM guild"));

        for (uint i = 0; i < pmsg->get()->num_rows; i++) {
            MYSQL_ROW row = mysql_fetch_row(pmsg->get()->res.get());
            uint32_t guild_id = strtoul(row[0], (char **)nullptr, 10);
            load_guild(guild_id);
        }

        guild_mark_manager &mark_mgr = get_guild_mark_manager();

        mark_mgr.set_mark_path_prefix("mark");
        mkdir("mark", S_IRWXU);
        seed_marks();

        mark_mgr.load_mark_index();
        mark_mgr.load_mark_images();
        mark_mgr.load_symbol(GUILD_SYMBOL_FILENAME);
    }

    void guild_manager::load_guild(uint32_t guild_id) {
        auto it = guild_.find(guild_id);

        if (it == guild_.end()) {
            guild_.insert(std::make_pair(guild_id, std::make_unique<guild>(guild_id)));
        }
    }

    void guild_manager::disband_guild(uint32_t guild_id) {
        auto it = guild_.find(guild_id);

        if (it == guild_.end())
            return;

        it->second->disband();

        guild_.erase(it);

        get_guild_mark_manager().delete_mark(guild_id);
    }

    void guild_manager::skill_recharge() {
        for (auto it = guild_.begin(); it != guild_.end(); ++it) {
            it->second->skill_recharge();
        }
    }

    auto guild_manager::rank(guild *g) -> int {
        int point = g->ladder_point();
        int rank = 1;

        for (auto it = guild_.begin(); it != guild_.end(); ++it) {
            guild *pg = it->second.get();

            if (pg->ladder_point() > point)
                rank++;
        }

        return rank;
    }

    namespace {
        auto guild_rank_less(guild *g1, guild *g2) -> bool {
            return std::tuple(g1->ladder_point(), g1->guild_war_win_count(), g1->guild_war_loss_count(), std::string_view(g2->name())) < std::tuple(g2->ladder_point(), g2->guild_war_win_count(), g2->guild_war_loss_count(), std::string_view(g1->name()));
        }

        void append_rank_line(std::string &text, guild *g, int rank, uint32_t my_guild, bool separator) {
            if (my_guild == g->id()) {
                text += "[COLOR r;255|g;255|b;0]";
            }

            if (separator) {
                text += "[ENTER]";
            }

            std::format_to(std::back_inserter(text), "{:3} | {:<12} | {:<8} | {:4} | {:4} | {:4}",
                rank,
                g->name(),
                g->ladder_point(),
                g->guild_war_win_count(),
                g->guild_war_draw_count(),
                g->guild_war_loss_count());

            if (g->id() == my_guild) {
                text += "[/COLOR]";
            }
        }

        void send_war_request(uint8_t war, uint32_t guild_id1, uint32_t guild_id2) {
            packet_guild_war p;
            p.war = war;
            p.guild_from = guild_id1;
            p.guild_to = guild_id2;
            db_clientdesc->db_packet(HEADER_GD_GUILD_WAR, 0, &p, sizeof(p));
        }
    }

    auto guild_manager::sorted_by_rank() -> std::vector<guild *> {
        std::vector<guild *> v;

        for (auto it = guild_.begin(); it != guild_.end(); ++it) {
            if (it->second)
                v.push_back(it->second.get());
        }

        std::ranges::sort(v, guild_rank_less);
        return v;
    }

    void guild_manager::high_rank_string(uint32_t my_guild, char *buffer, size_t buflen) {
        std::vector<guild *> v = sorted_by_rank();
        int n = v.size();
        std::string text;

        for (int i = 0; i < 8; ++i) {
            if (n - i - 1 < 0)
                break;

            guild *g = v[n - i - 1];

            if (!g)
                continue;

            if (g->ladder_point() <= 0)
                break;

            append_rank_line(text, g, rank(g), my_guild, i != 0);
        }

        strlcpy(buffer, text.c_str(), buflen);
    }

    void guild_manager::around_rank_string(uint32_t my_guild, char *buffer, size_t buflen) {
        std::vector<guild *> v = sorted_by_rank();
        int n = v.size();
        int idx;

        for (idx = 0; idx < (int)v.size(); ++idx)
            if (v[idx]->id() == my_guild)
                break;

        std::string text;
        int count = 0;

        for (int i = -3; i <= 3; ++i) {
            if (idx - i < 0)
                continue;

            if (idx - i >= n)
                continue;

            guild *g = v[idx - i];

            if (!g)
                continue;

            append_rank_line(text, g, rank(g), my_guild, count != 0);
            ++count;
        }

        strlcpy(buffer, text.c_str(), buflen);
    }

    void guild_manager::request_cancel_war(uint32_t guild_id1, uint32_t guild_id2) {
        metin2::log::info("Guild war: requesting cancel of war between guild {:d} and {:d}", guild_id1, guild_id2);
        send_war_request(GUILD_WAR_CANCEL, guild_id1, guild_id2);
    }

    void guild_manager::request_end_war(uint32_t guild_id1, uint32_t guild_id2) {
        metin2::log::info("Guild war: requesting end of war between guild {:d} and {:d}", guild_id1, guild_id2);
        send_war_request(GUILD_WAR_END, guild_id1, guild_id2);
    }

    void guild_manager::request_war_over(uint32_t guild1, uint32_t guild2, uint32_t guild_winner, int32_t reward) {
        guild *g1 = touch_guild(guild1);
        guild *g2 = touch_guild(guild2);

        if (g1->get_guild_war_state(g2->id()) != GUILD_WAR_ON_WAR) {
            metin2::log::info("Guild war: war over requested but guild {:d} and {:d} are not at war", guild1, guild2);
            request_end_war(guild1, guild2);
            return;
        }

        packet_guild_war p;

        p.war = GUILD_WAR_OVER;
        p.war_price = 0;
        p.type = guild_winner == 0 ? 1 : 0;

        if (guild_winner == 0) {
            p.guild_from = guild1;
            p.guild_to = guild2;
        } else {
            p.guild_from = guild_winner;
            p.guild_to = guild_winner == guild1 ? guild2 : guild1;
        }

        db_clientdesc->db_packet(HEADER_GD_GUILD_WAR, 0, &p, sizeof(p));
        metin2::log::info("Guild war: war over, winner {:d}, loser {:d}, draw {:d}, bet {:d}", p.guild_from, p.guild_to, p.type, p.war_price);
    }

    void guild_manager::declare_war(uint32_t guild_id1, uint32_t guild_id2, uint8_t type) {
        if (guild_id1 == guild_id2)
            return;

        guild *g1 = find_guild(guild_id1);
        guild *g2 = find_guild(guild_id2);

        if (!g1 || !g2)
            return;

        if (g1->declare_war(guild_id2, type, GUILD_WAR_SEND_DECLARE) &&
            g2->declare_war(guild_id1, type, GUILD_WAR_RECV_DECLARE)) {
            char buf[256];
            snprintf(buf, sizeof(buf), lc_text("%s 길드가 %s 길드에 선전포고를 하였습니다!"), touch_guild(guild_id1)->name(), touch_guild(guild_id2)->name());
            send_notice(buf);
        }
    }

    void guild_manager::refuse_war(uint32_t guild_id1, uint32_t guild_id2) {
        guild *g1 = find_guild(guild_id1);
        guild *g2 = find_guild(guild_id2);

        if (g1 && g2) {
            if (g2->master_character())
                g2->master_character()->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> %s 길드가 길드전을 거부하였습니다."), g1->name());
        }

        if (g1 != nullptr)
            g1->refuse_war(guild_id2);

        if (g2 != nullptr && g1 != nullptr)
            g2->refuse_war(g1->id());
    }

    void guild_manager::wait_start_war(uint32_t guild_id1, uint32_t guild_id2) {
        guild *g1 = find_guild(guild_id1);
        guild *g2 = find_guild(guild_id2);

        if (!g1 || !g2) {
            metin2::log::warn("Cannot prepare war between guild {:d} and {:d}, guild not found", guild_id1, guild_id2);
            return;
        }

        if (g1->wait_start_war(guild_id2) || g2->wait_start_war(guild_id1)) {
            char buf[256];
            snprintf(buf, sizeof(buf), lc_text("%s 길드와 %s 길드가 잠시 후 전쟁을 시작합니다!"), g1->name(), g2->name());
            send_notice(buf);
        }
    }

    namespace {
        void send_war_list(uint8_t subheader, uint32_t guild_id1, uint32_t guild_id2) {
            packet_gc_guild p;
            p.header = HEADER_GC_GUILD;
            p.size = sizeof(p) + sizeof(uint32_t) * 2;
            p.subheader = subheader;

            get_character_manager().for_each_pc([&p, guild_id1, guild_id2](character *ch) {
                desc *d = ch->get_desc();

                if (d) {
                    d->buffered_packet(&p, sizeof(p));
                    d->buffered_packet(&guild_id1, sizeof(uint32_t));
                    d->packet(&guild_id2, sizeof(uint32_t));
                }
            });
        }
    }

    void guild_manager::start_war(uint32_t guild_id1, uint32_t guild_id2) {
        guild *g1 = find_guild(guild_id1);
        guild *g2 = find_guild(guild_id2);

        if (!g1 || !g2)
            return;

        if (!g1->check_start_war(guild_id2) || !g2->check_start_war(guild_id1))
            return;

        g1->start_war(guild_id2);
        g2->start_war(guild_id1);

        char buf[256];
        snprintf(buf, sizeof(buf), lc_text("%s 길드와 %s 길드가 전쟁을 시작하였습니다!"), g1->name(), g2->name());
        send_notice(buf);

        if (guild_id1 > guild_id2)
            std::swap(guild_id1, guild_id2);

        send_war_list(GUILD_SUBHEADER_GC_GUILD_WAR_LIST, guild_id1, guild_id2);
        guild_war_.insert(std::make_pair(guild_id1, guild_id2));
    }

    void send_guild_war_over_notice(guild *g1, guild *g2, bool draw) {
        if (g1 && g2) {
            char buf[256];

            if (draw) {
                snprintf(buf, sizeof(buf), lc_text("%s 길드와 %s 길드 사이의 전쟁이 무승부로 끝났습니다."), g1->name(), g2->name());
            } else {
                if (g1->war_score_against_to(g2->id()) > g2->war_score_against_to(g1->id())) {
                    snprintf(buf, sizeof(buf), lc_text("%s 길드가 %s 길드와의 전쟁에서 승리 했습니다."), g1->name(), g2->name());
                } else {
                    snprintf(buf, sizeof(buf), lc_text("%s 길드가 %s 길드와의 전쟁에서 승리 했습니다."), g2->name(), g1->name());
                }
            }

            send_notice(buf);
        }
    }

    auto guild_manager::end_war(uint32_t guild_id1, uint32_t guild_id2) -> bool {
        if (guild_id1 > guild_id2)
            std::swap(guild_id1, guild_id2);

        guild *g1 = find_guild(guild_id1);
        guild *g2 = find_guild(guild_id2);

        std::pair<uint32_t, uint32_t> k = std::make_pair(guild_id1, guild_id2);

        auto it = guild_war_.find(k);

        if (guild_war_.end() == it) {
            metin2::log::warn("Cannot end war between guild {:d} and {:d}, no such war", guild_id1, guild_id2);
            return false;
        }

        if (g1 && g2) {
            if (g1->get_guild_war_type(guild_id2) == GUILD_WAR_TYPE_FIELD) {
                send_guild_war_over_notice(g1, g2, g1->war_score_against_to(guild_id2) == g2->war_score_against_to(guild_id1));
            }
        } else {
            return false;
        }

        if (g1)
            g1->end_war(guild_id2);

        if (g2)
            g2->end_war(guild_id1);

        guild_war_end_time_[k] = get_global_time();
        send_war_list(GUILD_SUBHEADER_GC_GUILD_WAR_END_LIST, guild_id1, guild_id2);
        guild_war_.erase(it);

        return true;
    }

    void guild_manager::war_over(uint32_t guild_id1, uint32_t guild_id2, bool draw) {
        guild *g1 = find_guild(guild_id1);
        guild *g2 = find_guild(guild_id2);

        if (guild_id1 > guild_id2)
            std::swap(guild_id1, guild_id2);

        std::pair<uint32_t, uint32_t> k = std::make_pair(guild_id1, guild_id2);

        auto it = guild_war_.find(k);

        if (guild_war_.end() == it)
            return;

        send_guild_war_over_notice(g1, g2, draw);

        end_war(guild_id1, guild_id2);
    }

    void guild_manager::cancel_war(uint32_t guild_id1, uint32_t guild_id2) {
        if (!end_war(guild_id1, guild_id2))
            return;

        guild *g1 = find_guild(guild_id1);
        guild *g2 = find_guild(guild_id2);

        if (g1) {
            character *master1 = g1->master_character();

            if (master1)
                master1->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드전이 취소 되었습니다."));
        }

        if (g2) {
            character *master2 = g2->master_character();

            if (master2)
                master2->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드전이 취소 되었습니다."));
        }

        if (g1 && g2) {
            char buf[256 + 1];
            snprintf(buf, sizeof(buf), lc_text("%s 길드와 %s 길드 사이의 전쟁이 취소되었습니다."), g1->name(), g2->name());
            send_notice(buf);
        }
    }

    void guild_manager::reserve_war(uint32_t guild1, uint32_t guild2, uint8_t type) {
        metin2::log::info("Guild war: reserving war between guild {:d} and {:d}", guild1, guild2);

        guild *g1 = find_guild(guild1);
        guild *g2 = find_guild(guild2);

        if (!g1 || !g2)
            return;

        g1->reserve_war(guild2, type);
        g2->reserve_war(guild1, type);
    }

    void guild_manager::show_guild_war_list(character *ch) {
        for (auto it = guild_war_.begin(); it != guild_war_.end(); ++it) {
            guild *a = touch_guild(it->first);
            guild *b = touch_guild(it->second);

            if (a && b) {
                ch->chat_packet(CHAT_TYPE_NOTICE, "%s[%d] vs %s[%d] time %u sec.",
                    a->name(), a->id(),
                    b->name(), b->id(),
                    get_global_time() - a->war_start_time(b->id()));
            }
        }
    }

    void guild_manager::send_guild_war(character *ch) {
        if (!ch->get_desc())
            return;

        temp_buffer buf;
        packet_gc_guild p;
        p.header = HEADER_GC_GUILD;
        p.subheader = GUILD_SUBHEADER_GC_GUILD_WAR_LIST;
        p.size = sizeof(p) + (sizeof(uint32_t) * 2) * guild_war_.size();
        buf.write(&p, sizeof(p));

        decltype(guild_war_.begin()) it;

        for (it = guild_war_.begin(); it != guild_war_.end(); ++it) {
            buf.write(&it->first, sizeof(uint32_t));
            buf.write(&it->second, sizeof(uint32_t));
        }

        ch->get_desc()->packet(buf.read_peek(), buf.size());
    }

    void send_guild_war_score(uint32_t guild, uint32_t guild_opp, int delta, int bet_score_delta) {
        packet_guild_war_score p;

        p.guild_gain_point = guild;
        p.guild_opponent = guild_opp;
        p.score = delta;
        p.bet_score = bet_score_delta;

        db_clientdesc->db_packet(HEADER_GD_GUILD_WAR_SCORE, 0, &p, sizeof(packet_guild_war_score));
        metin2::log::debug("Guild war: score change for guild {:d} against guild {:d}: {:d}", guild, guild_opp, delta);
    }

    void guild_manager::kill(character *killer, character *victim) {
        if (!killer->is_pc())
            return;

        if (!victim->is_pc())
            return;

        if (killer->get_war_map()) {
            killer->get_war_map()->on_kill(killer, victim);
            return;
        }

        guild *g_attack = killer->get_guild();
        guild *g_defend = victim->get_guild();

        if (!g_attack || !g_defend)
            return;

        if (g_attack->get_guild_war_type(g_defend->id()) != GUILD_WAR_TYPE_FIELD)
            return;

        if (!g_attack->under_war(g_defend->id()))
            return;

        send_guild_war_score(g_attack->id(), g_defend->id(), victim->level());
    }

    void guild_manager::stop_all_guild_war() {
        for (auto it = guild_war_.begin(); it != guild_war_.end(); ++it) {
            guild *g = get_guild_manager().touch_guild(it->first);
            guild *pg = get_guild_manager().touch_guild(it->second);
            g->end_war(it->second);
            pg->end_war(it->first);
        }

        guild_war_.clear();
    }

    void guild_manager::reserve_war_add(guild_war_reserve *p) {
        auto it = reserve_wars_.find(p->id);

        guild_war_reserve_for_game *reserve;

        if (it != reserve_wars_.end())
            reserve = it->second.get();
        else {
            auto new_reserve = std::make_unique<guild_war_reserve_for_game>();
            reserve = new_reserve.get();

            reserve_wars_.insert(std::make_pair(p->id, std::move(new_reserve)));
            reserve_war_list_.push_back(reserve);
        }

        std::memcpy(&reserve->data, p, sizeof(guild_war_reserve));

        metin2::log::info("Guild war: added reserved war {:d}, guild {:d} (power {:d}) vs guild {:d} (power {:d}), handicap {:d}",
            reserve->data.id, p->guild_from, p->power_from, p->guild_to, p->power_to, p->handicap);
    }

    void guild_manager::reserve_war_bet(packet_gd_guild_war_bet *p) {
        auto it = reserve_wars_.find(p->war_id);

        if (it == reserve_wars_.end())
            return;

        it->second->bet.insert(std::make_pair(p->login, std::make_pair(p->guild, p->gold)));
    }

    auto guild_manager::is_bet(uint32_t id, const char *login) -> bool {
        auto it = reserve_wars_.find(id);

        if (it == reserve_wars_.end())
            return true;

        return it->second->bet.end() != it->second->bet.find(login);
    }

    void guild_manager::reserve_war_delete(uint32_t id) {
        metin2::log::info("Guild war: deleted reserved war {:d}", id);
        auto it = reserve_wars_.find(id);

        if (it == reserve_wars_.end())
            return;

        auto it_vec = reserve_war_list_.begin();

        while (it_vec != reserve_war_list_.end()) {
            if (*it_vec == it->second.get()) {
                it_vec = reserve_war_list_.erase(it_vec);
                break;
            } else
                ++it_vec;
        }

        reserve_wars_.erase(it);
    }

    auto guild_manager::reserve_war_ref() -> std::vector<guild_war_reserve_for_game *> & {
        return reserve_war_list_;
    }

    void guild_manager::change_master(uint32_t gid) {
        auto iter = guild_.find(gid);

        if (iter == guild_.end())
            return;

        iter->second->load(gid);

        get_db_manager().func_query([gid](sql_msg &msg) {
            if (guild *g = get_guild_manager().find_guild(gid))
                g->send_guild_data_update_to_all_member(msg);
        },
            "SELECT 1");
    }
}
