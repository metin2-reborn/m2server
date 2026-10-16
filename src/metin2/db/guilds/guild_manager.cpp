#include <metin2/db/guilds/guild_manager.h>

#include <metin2/config/config.h>
#include <metin2/db/app/main.h>
#include <metin2/db/clients/client_manager.h>
#include <metin2/db/clients/peer.h>
#include <metin2/db/storage/db_manager.h>

#include <metin2/common/utils.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/guild_types.h>
#include <metin2/proto/item_types.h>
#include <metin2/proto/limits.h>
#include <metin2/proto/tables.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <format>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

namespace metin2::db::inline guilds {

    const int GUILD_RANK_MAX_NUM = 20;

    auto get_guild_war_wait_start_duration() -> uint32_t {
        return 5;
    }

    auto get_guild_war_reserve_seconds() -> uint32_t {
        return 10;
    }

    namespace {
        auto send_peer_war(uint8_t type_value, uint8_t war_value, uint32_t gid1, uint32_t gid2) {
            if (number(0, 1))
                std::swap(gid1, gid2);

            packet_guild_war p;
            memset(&p, 0, sizeof(packet_guild_war));

            p.war = war_value;
            p.type = type_value;
            p.guild_from = gid1;
            p.guild_to = gid2;

            return [p](peer *peer) {
                if (peer->channel() == 0)
                    return;

                peer->encode_header(HEADER_DG_GUILD_WAR, 0, sizeof(packet_guild_war));
                peer->encode(&p, sizeof(packet_guild_war));
            };
        }

        auto send_guild_war_score(uint32_t guild_gain, uint32_t opp_gid, int score_value, int bet_score_value) {
            packet_guild_war_score pck;
            pck.guild_gain_point = guild_gain;
            pck.guild_opponent = opp_gid;
            pck.score = score_value;
            pck.bet_score = bet_score_value;

            return [pck](peer *peer) {
                if (peer->channel() == 0)
                    return;

                peer->encode_header(HEADER_DG_GUILD_WAR_SCORE, 0, sizeof(pck));
                peer->encode(&pck, sizeof(pck));
            };
        }
    }

    guild_manager::guild_manager() = default;

    guild_manager::~guild_manager() = default;

    auto guild_manager::touch_guild(uint32_t gid) -> guild & {
        auto it = map_guild_.find(gid);

        if (it != map_guild_.end())
            return it->second;

        guild info;
        map_guild_.insert(std::map<uint32_t, guild>::value_type(gid, info));
        return map_guild_[gid];
    }

    void guild_manager::parse_result(sql_result *res) {
        MYSQL_ROW row;

        while ((row = mysql_fetch_row(res->res.get()))) {
            uint32_t GID = strtoul(row[0], nullptr, 10);

            guild &r_info = touch_guild(GID);

            r_info.name = row[1];
            str_to_number(r_info.ladder_point, row[2]);
            str_to_number(r_info.win, row[3]);
            str_to_number(r_info.draw, row[4]);
            str_to_number(r_info.loss, row[5]);
            str_to_number(r_info.gold, row[6]);
            str_to_number(r_info.level, row[7]);

            metin2::log::debug(
                "Guild {}: ladder {:d}, wins {:d}, draws {:d}, losses {:d}",
                r_info.name,
                r_info.ladder_point,
                r_info.win,
                r_info.draw,
                r_info.loss);
        }
    }

    void guild_manager::initialize() {
        const std::string query = sql::format("SELECT id, name, ladder_point, win, draw, loss, gold, level FROM guild{}", sql::raw(get_table_postfix()));
        auto pmsg = db_manager::instance().direct_query(query);

        if (pmsg->get()->num_rows)
            parse_result(pmsg->get());

        const auto load_formula = [](poly::expression &formula, const std::string &text, std::string_view what) {
            if (text.empty())
                metin2::log::info("Guild war: no {} formula set, it evaluates to 0", what);
            else if (!formula.analyze(text.c_str()))
                metin2::log::error("Guild war: invalid {} formula: {}", what, text);
            else
                metin2::log::info("Guild war: {} formula {}", what, text);
        };
        load_formula(poly_power_, metin2::config::db().guild_war_power, "power");
        load_formula(poly_handicap_, metin2::config::db().guild_war_handicap, "handicap");

        query_ranking();
    }

    void guild_manager::load(uint32_t guild_id) {
        const std::string query = sql::format("SELECT id, name, ladder_point, win, draw, loss, gold, level FROM guild{} WHERE id={}", sql::raw(get_table_postfix()), guild_id);
        auto pmsg = db_manager::instance().direct_query(query);

        if (pmsg->get()->num_rows)
            parse_result(pmsg->get());
    }

    void guild_manager::query_ranking() {
        const std::string query = sql::format("SELECT id,name,ladder_point FROM guild{} ORDER BY ladder_point DESC LIMIT 20", sql::raw(get_table_postfix()));

        db_manager::instance().return_query(query, [this](sql_msg &msg) { result_ranking(msg.get()->res.get()); });
    }

    auto guild_manager::ranking(uint32_t gid) -> int {
        auto it = map_ladder_point_ranking_by_gid_.find(gid);

        if (it == map_ladder_point_ranking_by_gid_.end())
            return GUILD_RANK_MAX_NUM;

        return std::clamp(it->second, 0, GUILD_RANK_MAX_NUM);
    }

    void guild_manager::result_ranking(MYSQL_RES *res) {
        if (!res)
            return;

        int iLastLadderPoint = -1;
        int iRank = 0;

        map_ladder_point_ranking_by_gid_.clear();

        MYSQL_ROW row;

        while ((row = mysql_fetch_row(res))) {
            uint32_t dwGID = 0;
            str_to_number(dwGID, row[0]);
            int iLadderPoint = 0;
            str_to_number(iLadderPoint, row[2]);

            if (iLadderPoint != iLastLadderPoint)
                ++iRank;

            metin2::log::debug("Guild rank: {} rank {:d}, ladder {:d}", row[1], iRank, iLadderPoint);

            map_ladder_point_ranking_by_gid_.insert(std::make_pair(dwGID, iRank));
        }
    }

    void guild_manager::update() {
        process_reserve_war();

        time_t now = client_manager::instance().current_time();

        if (!pq_on_war_.empty()) {

            while (!pq_on_war_.empty() && (pq_on_war_.top().first <= now || (pq_on_war_.top().second && pq_on_war_.top().second->end))) {
                auto e = pq_on_war_.top().second;

                pq_on_war_.pop();

                if (e) {
                    if (!e->end)
                        war_end(e->gid[0], e->gid[1], false);
                }
            }
        }

        while (!pq_skill_.empty() && pq_skill_.top().first <= now) {
            const guild_skill_used &s = pq_skill_.top().second;
            client_manager::instance().send_guild_skill_usable(s.gid, s.skill_vnum, true);
            pq_skill_.pop();
        }

        while (!pq_wait_start_.empty() && pq_wait_start_.top().first <= now) {
            const guild_wait_start_info &ws = pq_wait_start_.top().second;
            pq_wait_start_.pop();

            start_war(ws.type, ws.gid[0], ws.gid[1], ws.reserve);

            if (ws.initial_score) {
                update_score(ws.gid[0], ws.gid[1], ws.initial_score, 0);
                update_score(ws.gid[1], ws.gid[0], ws.initial_score, 0);
            }

            packet_guild_war p;

            p.type = ws.type;
            p.war = GUILD_WAR_ON_WAR;
            p.guild_from = ws.gid[0];
            p.guild_to = ws.gid[1];

            client_manager::instance().forward_packet(HEADER_DG_GUILD_WAR, &p, sizeof(p));
            metin2::log::info("Guild war: starting waiting war between guild {:d} and guild {:d}", ws.gid[0], ws.gid[1]);
        }
    }

    void guild_manager::on_setup(peer *peer) {
        for (const auto &[key, war] : war_map_) {
            guild_war_pq_element *p = war.element.get();

            if (!p || p->end)
                continue;

            send_peer_war(p->type, GUILD_WAR_ON_WAR, key.first, key.second)(peer);
            send_guild_war_score(p->gid[0], p->gid[1], p->score[0], p->bet_score[0])(peer);
            send_guild_war_score(p->gid[1], p->gid[0], p->score[1], p->bet_score[1])(peer);
        }

        for (const auto &declare : declare_map_) {
            send_peer_war(declare.type, GUILD_WAR_SEND_DECLARE, declare.guild_id[0], declare.guild_id[1])(peer);
        }

        for (const auto &[id, reserve] : map_war_reserve_) {
            reserve->on_setup(peer);
        }
    }

    void guild_manager::count_war_result(uint32_t gid, int guild::*counter, const char *column) {
        auto it = map_guild_.find(gid);

        if (it == map_guild_.end())
            return;

        const int count = ++(it->second.*counter);

        const std::string query = sql::format("UPDATE guild{} SET {}={} WHERE id={}", sql::raw(get_table_postfix()), sql::raw(column), count, gid);
        db_manager::instance().async_query(query);
    }

    auto guild_manager::find_war(uint32_t gid1, uint32_t gid2) -> guild_war_info * {
        const auto it = war_map_.find(make_war_key(gid1, gid2));
        return it == war_map_.end() ? nullptr : &it->second;
    }

    auto guild_manager::is_half_win_ladder_point(uint32_t guild_winner, uint32_t guild_loser) -> bool {
        const auto it = guild_war_end_time_.find(make_war_key(guild_winner, guild_loser));

        return it != guild_war_end_time_.end() &&
               it->second + GUILD_WAR_LADDER_HALF_PENALTY_TIME > client_manager::instance().current_time();
    }

    void guild_manager::process_draw(uint32_t guild_id1, uint32_t guild_id2) {
        metin2::log::info("Guild war: war between guild {:d} and guild {:d} ended in a draw", guild_id1, guild_id2);

        count_war_result(guild_id1, &guild::draw, "draw");
        count_war_result(guild_id2, &guild::draw, "draw");
        change_ladder_point(guild_id1, 0);
        change_ladder_point(guild_id2, 0);

        query_ranking();
    }

    void guild_manager::process_win_lose(uint32_t guild_winner, uint32_t guild_loser) {
        count_war_result(guild_winner, &guild::win, "win");
        count_war_result(guild_loser, &guild::loss, "loss");
        metin2::log::info("Guild war: guild {:d} won against guild {:d}", guild_winner, guild_loser);

        int iPoint = ladder_point(guild_loser);
        int gain = (int)(iPoint * 0.05);
        int loss = (int)(iPoint * 0.07);

        if (is_half_win_ladder_point(guild_winner, guild_loser))
            gain /= 2;

        metin2::log::info("Guild war: ladder gain {:d}, loss {:d}", gain, loss);

        change_ladder_point(guild_winner, gain);
        change_ladder_point(guild_loser, -loss);

        query_ranking();
    }

    void guild_manager::remove_war(uint32_t gid1, uint32_t gid2) {
        metin2::log::debug("Guild war: removing war between guild {:d} and guild {:d}", gid1, gid2);

        const war_key key = make_war_key(gid1, gid2);
        const auto it = war_map_.find(key);

        if (it == war_map_.end())
            return;

        if (it->second.element)
            it->second.element->end = true;

        guild_war_end_time_[key] = client_manager::instance().current_time();

        war_map_.erase(it);
    }

    void guild_manager::war_end(uint32_t gid1, uint32_t gid2, bool force_draw) {
        std::tie(gid1, gid2) = make_war_key(gid1, gid2);

        metin2::log::info("Guild war: ending war between guild {:d} and guild {:d}", gid1, gid2);

        const guild_war_info *war = find_war(gid1, gid2);

        if (!war) {
            metin2::log::warn("Guild war: cannot end war between guild {:d} and guild {:d}, no such war", gid1, gid2);
            return;
        }

        const guild_war_info gwi = *war;
        guild_war_pq_element *pData = gwi.element.get();

        if (!pData || pData->end) {
            metin2::log::warn("Guild war: cannot end war between guild {:d} and guild {:d}, already ended", gid1, gid2);
            return;
        }

        uint32_t win_guild = pData->gid[0];
        uint32_t lose_guild = pData->gid[1];

        bool bDraw = false;

        if (!force_draw) {
            if (pData->score[0] > pData->score[1]) {
                win_guild = pData->gid[0];
                lose_guild = pData->gid[1];
            } else if (pData->score[1] > pData->score[0]) {
                win_guild = pData->gid[1];
                lose_guild = pData->gid[0];
            } else
                bDraw = true;
        } else
            bDraw = true;

        if (bDraw)
            process_draw(win_guild, lose_guild);
        else
            process_win_lose(win_guild, lose_guild);

        // The db ends some wars on its own, so the packet must still be sent.
        client_manager::instance().for_each_peer(send_peer_war(0, GUILD_WAR_END, gid1, gid2));

        remove_war(gid1, gid2);
    }

    void guild_manager::recv_war_over(uint32_t guild_winner, uint32_t guild_loser, bool draw_value, int32_t war_price) {
        metin2::log::info("Guild war: war over, winner guild {:d}, loser guild {:d}, draw {:d}, war price {:d}", guild_winner, guild_loser, draw_value ? 1 : 0, war_price);

        guild_war_info *war = find_war(guild_winner, guild_loser);

        if (!war)
            return;

        guild_war_info &gw = *war;

        if (draw_value) {
            deposit_money(guild_winner, war_price / 2);
            deposit_money(guild_loser, war_price / 2);
            process_draw(guild_winner, guild_loser);
        } else {
            deposit_money(guild_winner, war_price);
            process_win_lose(guild_winner, guild_loser);
        }

        if (gw.reserve) {
            if (draw_value || !gw.element)
                gw.reserve->draw();
            else if (gw.element->type == GUILD_WAR_TYPE_BATTLE)
                gw.reserve->end(gw.element->bet_score[0], gw.element->bet_score[1]);
        }

        const auto [gid1, gid2] = make_war_key(guild_winner, guild_loser);
        remove_war(gid1, gid2);
    }

    void guild_manager::recv_war_end(uint32_t gid1, uint32_t gid2) {
        metin2::log::info("Guild war: war between guild {:d} and guild {:d} ended", gid1, gid2);
        war_end(gid1, gid2, true);
    }

    void guild_manager::start_war(uint8_t type, uint32_t gid1, uint32_t gid2, guild_war_reservation *reserve_value) {
        metin2::log::info("Guild war: starting war type {:d} between guild {:d} and guild {:d}", type, gid1, gid2);

        std::tie(gid1, gid2) = make_war_key(gid1, gid2);

        guild_war_info &gw = war_map_[{gid1, gid2}];

        if (type == GUILD_WAR_TYPE_FIELD)
            gw.t_end_time = client_manager::instance().current_time() + GUILD_WAR_DURATION;
        else
            gw.t_end_time = client_manager::instance().current_time() + 172800;

        gw.element = std::make_shared<guild_war_pq_element>(type, gid1, gid2);
        gw.reserve = reserve_value;

        pq_on_war_.push(std::make_pair(gw.t_end_time, gw.element));
    }

    void guild_manager::update_score(uint32_t gain_gid, uint32_t opp_gid, int score_delta, int bet_score_delta) {
        if (guild_war_info *war = find_war(gain_gid, opp_gid)) {
            guild_war_pq_element *p = war->element.get();

            if (!p || p->end) {
                metin2::log::warn("Guild war: score update for guild {:d}, but its war against guild {:d} has already ended", gain_gid, opp_gid);
                return;
            }

            int iNewScore = 0;
            int iNewBetScore = 0;

            if (p->gid[0] == gain_gid) {
                p->score[0] += score_delta;
                p->bet_score[0] += bet_score_delta;

                iNewScore = p->score[0];
                iNewBetScore = p->bet_score[0];
            } else {
                p->score[1] += score_delta;
                p->bet_score[1] += bet_score_delta;

                iNewScore = p->score[1];
                iNewBetScore = p->bet_score[1];
            }

            metin2::log::debug("Guild war: guild {:d} war type {:d} score {:+d}, bet score {:+d}, now {:d} and {:d}",
                gain_gid, p->type, score_delta, bet_score_delta, iNewScore, iNewBetScore);

            client_manager::instance().for_each_peer(send_guild_war_score(gain_gid, opp_gid, iNewScore, iNewBetScore));
        }
    }

    void guild_manager::add_declare(uint8_t type, uint32_t guild_from, uint32_t guild_to) {
        guild_declare_info di(type, guild_from, guild_to);

        if (declare_map_.find(di) == declare_map_.end())
            declare_map_.insert(di);

        metin2::log::info("Guild war: war type {:d} declared by guild {:d} on guild {:d}", type, guild_from, guild_to);
    }

    void guild_manager::remove_declare(uint32_t guild_from, uint32_t guild_to) {
        __typeof(declare_map_.begin()) it = declare_map_.find(guild_declare_info(0, guild_from, guild_to));

        if (it != declare_map_.end())
            declare_map_.erase(it);

        it = declare_map_.find(guild_declare_info(0, guild_to, guild_from));

        if (it != declare_map_.end())
            declare_map_.erase(it);

        metin2::log::info("Guild war: declaration by guild {:d} on guild {:d} removed", guild_from, guild_to);
    }

    auto guild_manager::take_bet_price(uint32_t guild_to, uint32_t guild_from, int32_t war_price) -> bool {
        auto it_from = map_guild_.find(guild_from);
        auto it_to = map_guild_.find(guild_to);

        if (it_from == map_guild_.end() || it_to == map_guild_.end()) {
            metin2::log::warn("Guild war: cannot take war price, guild {:d} or guild {:d} not found",
                guild_from, guild_to);
            return false;
        }

        if (it_from->second.gold < war_price || it_to->second.gold < war_price) {
            metin2::log::info("Guild war: cannot take war price, guild {:d} has {:d} gold, guild {:d} has {:d} gold",
                guild_from, it_from->second.gold, guild_to, it_to->second.gold);
            return false;
        }

        it_from->second.gold -= war_price;
        it_to->second.gold -= war_price;

        money_change(guild_from, it_from->second.gold);
        money_change(guild_to, it_to->second.gold);
        return true;
    }

    auto guild_manager::ladder_point(uint32_t gid) -> int {
        auto it = map_guild_.find(gid);

        if (it == map_guild_.end())
            return 0;

        return it->second.ladder_point;
    }

    void guild_manager::change_ladder_point(uint32_t gid, int change) {
        auto it = map_guild_.find(gid);

        if (it == map_guild_.end())
            return;

        guild &r = it->second;

        r.ladder_point += change;

        if (r.ladder_point < 0)
            r.ladder_point = 0;

        const std::string query = sql::format("UPDATE guild{} SET ladder_point={} WHERE id={}", sql::raw(get_table_postfix()), r.ladder_point, gid);
        db_manager::instance().async_query(query);

        metin2::log::info("Guild {:d}: ladder points now {:d}", gid, r.ladder_point);
        packet_guild_ladder p;

        p.guild = gid;
        p.ladder_point = r.ladder_point;
        p.win = r.win;
        p.draw = r.draw;
        p.loss = r.loss;

        client_manager::instance().forward_packet(HEADER_DG_GUILD_LADDER, &p, sizeof(packet_guild_ladder));
    }

    void guild_manager::use_skill(uint32_t gid, uint32_t skill_vnum, uint32_t cooltime) {
        metin2::log::debug("Guild {:d}: used skill {:d}, cooldown {:d} s (until {:d})", gid, skill_vnum, cooltime, client_manager::instance().current_time() + cooltime);
        pq_skill_.push(std::make_pair(client_manager::instance().current_time() + cooltime, guild_skill_used(gid, skill_vnum)));
    }

    void guild_manager::money_change(uint32_t guild_value, uint32_t gold) {
        metin2::log::info("Guild {:d}: gold changed to {:d}", guild_value, gold);

        packet_dg_guild_money_change p;
        p.guild = guild_value;
        p.total_gold = gold;
        client_manager::instance().forward_packet(HEADER_DG_GUILD_MONEY_CHANGE, &p, sizeof(p));

        const std::string query = sql::format("UPDATE guild{} SET gold={} WHERE id = {}", sql::raw(get_table_postfix()), gold, guild_value);
        db_manager::instance().async_query(query);
    }

    void guild_manager::deposit_money(uint32_t guild, int32_t gold_value) {
        if (gold_value <= 0)
            return;

        auto it = map_guild_.find(guild);

        if (it == map_guild_.end()) {
            metin2::log::warn("Guild {:d} not found, cannot deposit gold", guild);
            return;
        }

        it->second.gold += gold_value;
        metin2::log::info("Guild {:d}: deposited {:d} gold, total {:d}", guild, gold_value, it->second.gold);

        money_change(guild, it->second.gold);
    }

    void guild_manager::withdraw_money(peer *peer, uint32_t guild_value, int32_t gold_value) {
        auto it = map_guild_.find(guild_value);

        if (it == map_guild_.end()) {
            metin2::log::warn("Guild {:d} not found, cannot withdraw gold", guild_value);
            return;
        }

        if (it->second.gold >= gold_value) {
            it->second.gold -= gold_value;
            metin2::log::info("Guild {:d}: withdrawing {:d} gold, total {:d}", guild_value, gold_value, it->second.gold);

            packet_dg_guild_money_withdraw p;
            p.guild = guild_value;
            p.change_gold = gold_value;

            peer->encode_header(HEADER_DG_GUILD_WITHDRAW_MONEY_GIVE, 0, sizeof(packet_dg_guild_money_withdraw));
            peer->encode(&p, sizeof(packet_dg_guild_money_withdraw));
        }
    }

    void guild_manager::withdraw_money_reply(uint32_t guild, uint8_t give_success, int32_t gold_value) {
        auto it = map_guild_.find(guild);

        if (it == map_guild_.end())
            return;

        metin2::log::info("Guild {:d}: withdrawal reply, success {:d}, gold {:d}", guild, give_success, gold_value);

        if (!give_success)
            it->second.gold += gold_value;
        else
            money_change(guild, it->second.gold);
    }

    const int score_by_level[GUILD_MAX_LEVEL + 1] =
        {
            500,
            500,
            1000,
            2000,
            3000,
            4000,
            6000,
            8000,
            10000,
            12000,
            15000,
            18000,
            21000,
            24000,
            28000,
            32000,
            36000,
            40000,
            45000,
            50000,
            55000,
    };

    const int score_by_ranking[GUILD_RANK_MAX_NUM + 1] =
        {
            0,
            55000,
            50000,
            45000,
            40000,
            36000,
            32000,
            28000,
            24000,
            21000,
            18000,
            15000,
            12000,
            10000,
            8000,
            6000,
            4000,
            3000,
            2000,
            1000,
            500};

    void guild_manager::boot_reserve_war() {
        const char *c_apszQuery[2] =
            {
                "SELECT id, guild1, guild2, UNIX_TIMESTAMP(time), type, warprice, initscore, bet_from, bet_to, power1, power2, handicap FROM guild_war_reservation WHERE started=1 AND winner=-1",
                "SELECT id, guild1, guild2, UNIX_TIMESTAMP(time), type, warprice, initscore, bet_from, bet_to, power1, power2, handicap FROM guild_war_reservation WHERE started=0"};

        for (int i = 0; i < 2; ++i) {
            auto pmsg = db_manager::instance().direct_query(c_apszQuery[i]);

            if (pmsg->get()->num_rows == 0)
                continue;

            MYSQL_ROW row;

            while ((row = mysql_fetch_row(pmsg->get()->res.get()))) {
                int col = 0;

                guild_war_reserve t;

                str_to_number(t.id, row[col++]);
                str_to_number(t.guild_from, row[col++]);
                str_to_number(t.guild_to, row[col++]);
                str_to_number(t.time, row[col++]);
                str_to_number(t.type, row[col++]);
                str_to_number(t.war_price, row[col++]);
                str_to_number(t.initial_score, row[col++]);
                str_to_number(t.bet_from, row[col++]);
                str_to_number(t.bet_to, row[col++]);
                str_to_number(t.power_from, row[col++]);
                str_to_number(t.power_to, row[col++]);
                str_to_number(t.handicap, row[col++]);
                t.started = 0;

                auto pkReserve = std::make_unique<guild_war_reservation>(t);

                const std::string buf = std::format("GuildWar: BootReserveWar : step {} id {} GID1 {} GID2 {}", i, t.id, t.guild_from, t.guild_to);
                if (i == 0 || (int)t.time - client_manager::instance().current_time() < 0) {
                    if (i == 0)
                        metin2::log::info("{}: server was shut down during this war, cancelling it", buf);
                    else
                        metin2::log::info("{}: start time already passed, cancelling it", buf);

                    pkReserve->draw();
                } else {
                    metin2::log::info("{}: restored", buf);
                    map_war_reserve_.insert(std::make_pair(t.id, std::move(pkReserve)));
                }
            }
        }
    }

    auto get_average_guild_member_level(uint32_t gid) -> int {
        const std::string query = sql::format(
            "SELECT AVG(level) FROM guild_member{0}, player{0} AS p WHERE guild_id={1} AND guild_member{0}.pid=p.id",
            sql::raw(get_table_postfix()), gid);

        auto msg = db_manager::instance().direct_query(query);

        MYSQL_ROW row;
        row = mysql_fetch_row(msg->get()->res.get());

        int nAverageLevel = 0;
        str_to_number(nAverageLevel, row[0]);
        return nAverageLevel;
    }

    auto get_guild_member_count(uint32_t gid) -> int {
        const std::string query = sql::format("SELECT COUNT(*) FROM guild_member{} WHERE guild_id={}", sql::raw(get_table_postfix()), gid);

        auto msg = db_manager::instance().direct_query(query);

        MYSQL_ROW row;
        row = mysql_fetch_row(msg->get()->res.get());

        uint32_t dwCount = 0;
        str_to_number(dwCount, row[0]);
        return dwCount;
    }

    auto guild_manager::reserve_war(packet_guild_war *p) -> bool {
        uint32_t GID1 = p->guild_from;
        uint32_t GID2 = p->guild_to;

        if (GID1 > GID2)
            std::swap(GID1, GID2);

        if (p->war_price > 0)
            if (!take_bet_price(GID1, GID2, p->war_price))
                return false;

        guild_war_reserve t;
        memset(&t, 0, sizeof(guild_war_reserve));

        t.guild_from = GID1;
        t.guild_to = GID2;
        t.time = client_manager::instance().current_time() + get_guild_war_reserve_seconds();
        t.type = p->type;
        t.war_price = p->war_price;
        t.initial_score = p->initial_score;

        int lvp, rkp, alv, mc;

        guild &k1 = touch_guild(GID1);

        lvp = score_by_level[std::min(GUILD_MAX_LEVEL, k1.level)];
        rkp = score_by_ranking[ranking(GID1)];
        alv = get_average_guild_member_level(GID1);
        mc = get_guild_member_count(GID1);

        poly_power_.set_var("lvp", lvp);
        poly_power_.set_var("rkp", rkp);
        poly_power_.set_var("alv", alv);
        poly_power_.set_var("mc", mc);

        t.power_from = (int32_t)poly_power_.eval();
        metin2::log::debug("Guild war: guild {:d} level points {:d}, rank points {:d}, average level {:d}, members {:d}, power {:d}", GID1, lvp, rkp, alv, mc, t.power_from);

        guild &k2 = touch_guild(GID2);

        lvp = score_by_level[std::min(GUILD_MAX_LEVEL, k2.level)];
        rkp = score_by_ranking[ranking(GID2)];
        alv = get_average_guild_member_level(GID2);
        mc = get_guild_member_count(GID2);

        poly_power_.set_var("lvp", lvp);
        poly_power_.set_var("rkp", rkp);
        poly_power_.set_var("alv", alv);
        poly_power_.set_var("mc", mc);

        t.power_to = (int32_t)poly_power_.eval();
        metin2::log::debug("Guild war: guild {:d} level points {:d}, rank points {:d}, average level {:d}, members {:d}, power {:d}", GID2, lvp, rkp, alv, mc, t.power_to);

        if (t.power_to > t.power_from) {
            poly_handicap_.set_var("pA", t.power_to);
            poly_handicap_.set_var("pB", t.power_from);
        } else {
            poly_handicap_.set_var("pA", t.power_from);
            poly_handicap_.set_var("pB", t.power_to);
        }

        t.handicap = (int32_t)poly_handicap_.eval();
        metin2::log::debug("Guild war: handicap {:d}", t.handicap);

        const std::string query = sql::format(
            "INSERT INTO guild_war_reservation (guild1, guild2, time, type, warprice, initscore, power1, power2, handicap) "
            "VALUES({}, {}, DATE_ADD(NOW(), INTERVAL 180 SECOND), {}, {}, {}, {}, {}, {})",
            GID1, GID2, p->type, p->war_price, p->initial_score, t.power_from, t.power_to, t.handicap);

        auto pmsg = db_manager::instance().direct_query(query);

        if (pmsg->get()->affected_rows == 0 || pmsg->get()->insert_id == 0 || pmsg->get()->affected_rows == (uint32_t)-1) {
            metin2::log::error("Guild war: cannot reserve war between guild {:d} and guild {:d}", GID1, GID2);
            return false;
        }

        t.id = pmsg->get()->insert_id;

        map_war_reserve_.insert(std::make_pair(t.id, std::make_unique<guild_war_reservation>(t)));

        client_manager::instance().forward_packet(HEADER_DG_GUILD_WAR_RESERVE_ADD, &t, sizeof(guild_war_reserve));
        return true;
    }

    void guild_manager::process_reserve_war() {
        uint32_t dwCurTime = client_manager::instance().current_time();

        auto it = map_war_reserve_.begin();

        while (it != map_war_reserve_.end()) {
            auto it2 = it++;

            guild_war_reservation *pk = it2->second.get();
            guild_war_reserve &r = pk->data_ref();

            if (!r.started && r.time - 1800 <= dwCurTime) {
                int iMin = (int)ceil((int)(r.time - dwCurTime) / 60.0);

                metin2::log::info("Guild war: reserved war between guild {:d} and guild {:d} is starting (started {:d}, {:d} s overdue, {:d} min left)", r.guild_from, r.guild_to, r.started, dwCurTime - r.time, iMin);

                if (iMin <= 0) {
                    const std::string query = sql::format("UPDATE guild_war_reservation SET started=1 WHERE id={}", r.id);
                    db_manager::instance().async_query(query);

                    client_manager::instance().forward_packet(HEADER_DG_GUILD_WAR_RESERVE_DEL, &r.id, sizeof(uint32_t));

                    r.started = true;

                    guild_wait_start_info info(r.type, r.guild_from, r.guild_to, r.war_price, r.initial_score, pk);
                    pq_wait_start_.push(std::make_pair(dwCurTime + get_guild_war_wait_start_duration(), info));

                    packet_guild_war pck;

                    pck.type = r.type;
                    pck.war = GUILD_WAR_WAIT_START;
                    pck.guild_from = r.guild_from;
                    pck.guild_to = r.guild_to;
                    pck.war_price = r.war_price;
                    pck.initial_score = r.initial_score;

                    client_manager::instance().forward_packet(HEADER_DG_GUILD_WAR, &pck, sizeof(packet_guild_war));
                } else {
                    if (iMin != pk->last_notice_min()) {
                        pk->set_last_notice_min(iMin);
                    }
                }
            }
        }
    }

    auto guild_manager::bet(uint32_t id, const char *login, uint32_t gold, uint32_t guild) -> bool {
        auto it = map_war_reserve_.find(id);

        if (it == map_war_reserve_.end()) {
            metin2::log::warn("Guild war bet: reserved war {:d} not found, refunding", id);
            const std::string query = sql::format("INSERT INTO item_award (login, vnum, socket0, given_time) VALUES({}, {}, {}, NOW())",
                login, ITEM_ELK_VNUM, gold);
            db_manager::instance().async_query(query);
            return false;
        }

        if (!it->second->bet(login, gold, guild)) {
            metin2::log::info("Guild war bet: bet on war {:d} by account {} ({:d} gold on guild {:d}) rejected, refunding", id, login, gold, guild);
            const std::string query = sql::format("INSERT INTO item_award (login, vnum, socket0, given_time) VALUES({}, {}, {}, NOW())",
                login, ITEM_ELK_VNUM, gold);
            db_manager::instance().async_query(query);
            return false;
        }

        return true;
    }

    void guild_manager::cancel_war(uint32_t gid1, uint32_t gid2) {
        remove_declare(gid1, gid2);
        remove_war(gid1, gid2);
    }

    auto guild_manager::change_master(uint32_t gid, uint32_t from, uint32_t to) -> bool {
        auto iter = map_guild_.find(gid);

        if (iter == map_guild_.end())
            return false;

        db_manager::instance().direct_query(sql::format("UPDATE guild{} SET master={} WHERE id={}", sql::raw(get_table_postfix()), to, gid));
        db_manager::instance().direct_query(sql::format("UPDATE guild_member{} SET grade=1 WHERE pid={}", sql::raw(get_table_postfix()), to));
        db_manager::instance().direct_query(sql::format("UPDATE guild_member{} SET grade=15 WHERE pid={}", sql::raw(get_table_postfix()), from));

        return true;
    }

    guild_war_reservation::guild_war_reservation(const guild_war_reserve &table) {
        std::memcpy(&data_, &table, sizeof(guild_war_reserve));
        last_notice_min_ = -1;

        initialize();
    }

    void guild_war_reservation::initialize() {
        const std::string query = sql::format("SELECT login, guild, gold FROM guild_war_bet WHERE war_id={}", data_.id);

        auto msgbet = db_manager::instance().direct_query(query);

        if (msgbet->get()->num_rows) {
            MYSQL_RES *res = msgbet->get()->res.get();
            MYSQL_ROW row;

            char szLogin[LOGIN_MAX_LEN + 1];
            uint32_t dwGuild;
            uint32_t dwGold;

            while ((row = mysql_fetch_row(res))) {
                dwGuild = dwGold = 0;
                strlcpy(szLogin, row[0], sizeof(szLogin));
                str_to_number(dwGuild, row[1]);
                str_to_number(dwGold, row[2]);

                bet_.insert(std::make_pair(szLogin, std::make_pair(dwGuild, dwGold)));
            }
        }
    }

    void guild_war_reservation::on_setup(peer *peer) {
        if (data_.started)
            return;

        send_peer_war(data_.type, GUILD_WAR_RESERVE, data_.guild_from, data_.guild_to)(peer);

        peer->encode_header(HEADER_DG_GUILD_WAR_RESERVE_ADD, 0, sizeof(guild_war_reserve));
        peer->encode(&data_, sizeof(guild_war_reserve));

        packet_gd_guild_war_bet pckBet;
        pckBet.war_id = data_.id;

        auto it = bet_.begin();

        while (it != bet_.end()) {
            strlcpy(pckBet.login, it->first.c_str(), sizeof(pckBet.login));
            pckBet.guild = it->second.first;
            pckBet.gold = it->second.second;

            peer->encode_header(HEADER_DG_GUILD_WAR_BET, 0, sizeof(packet_gd_guild_war_bet));
            peer->encode(&pckBet, sizeof(packet_gd_guild_war_bet));

            ++it;
        }
    }

    auto guild_war_reservation::bet(const char *login_value, uint32_t gold_value, uint32_t guild_value) -> bool {
        if (data_.guild_from != guild_value && data_.guild_to != guild_value) {
            metin2::log::warn("Guild war bet: guild {:d} is not part of war {:d}", guild_value, data_.id);
            return false;
        }

        if (data_.started) {
            metin2::log::info("Guild war bet: war {:d} has already started", data_.id);
            return false;
        }

        if (bet_.find(login_value) != bet_.end()) {
            metin2::log::info("Guild war bet: account {} already bet on war {:d}", login_value, data_.id);
            return false;
        }

        std::string query = sql::format(
            "INSERT INTO guild_war_bet (war_id, login, gold, guild) VALUES({}, {}, {}, {})",
            data_.id, login_value, gold_value, guild_value);

        auto pmsg = db_manager::instance().direct_query(query);

        if (pmsg->get()->affected_rows == 0 || pmsg->get()->affected_rows == (uint32_t)-1) {
            metin2::log::error("Guild war bet: cannot record bet of account {} on war {:d}", login_value, data_.id);
            return false;
        }

        if (data_.guild_from == guild_value)
            data_.bet_from += gold_value;
        else
            data_.bet_to += gold_value;

        client_manager::instance().forward_packet(HEADER_DG_GUILD_WAR_RESERVE_ADD, &data_, sizeof(guild_war_reserve));

        query = sql::format("UPDATE guild_war_reservation SET bet_from={},bet_to={} WHERE id={}",
            data_.bet_from, data_.bet_to, data_.id);

        db_manager::instance().async_query(query);

        metin2::log::info("Guild war bet: account {} bet on guild {:d} in war {:d}, totals {:d} : {:d}", login_value, guild_value, data_.id, data_.bet_from, data_.bet_to);
        bet_.insert(std::make_pair(login_value, std::make_pair(guild_value, gold_value)));

        packet_gd_guild_war_bet pckBet;
        pckBet.war_id = data_.id;
        strlcpy(pckBet.login, login_value, sizeof(pckBet.login));
        pckBet.guild = guild_value;
        pckBet.gold = gold_value;

        client_manager::instance().forward_packet(HEADER_DG_GUILD_WAR_BET, &pckBet, sizeof(packet_gd_guild_war_bet));
        return true;
    }

    namespace {
        class elk_award_batch {
          public:
            void add(const std::string &login, uint32_t gold) {
                query_ += rows_++ == 0 ? "INSERT INTO item_award (login, vnum, socket0, given_time) VALUES" : ",";
                query_ += sql::format("({}, {}, {}, NOW())", login, ITEM_ELK_VNUM, gold);

                if (query_.size() > 384)
                    flush();
            }

            void flush() {
                if (rows_ > 0) {
                    metin2::log::debug("Guild war reward: {}", query_);
                    db_manager::instance().async_query(query_);
                }
                query_.clear();
                rows_ = 0;
            }

          private:
            std::string query_;
            int rows_ = 0;
        };
    }

    void guild_war_reservation::draw() {
        std::string query = sql::format("UPDATE guild_war_reservation SET started=1,winner=0 WHERE id={}", data_.id);
        db_manager::instance().async_query(query);

        if (bet_.empty())
            return;

        metin2::log::info("Guild war reward: war {:d} ended in a draw, refunding bets", data_.id);

        elk_award_batch awards;
        for (const auto &[login, bet] : bet_)
            awards.add(login, bet.second);
        awards.flush();
    }

    void guild_war_reservation::end(int score_from, int score_to) {
        uint32_t dwWinner;

        metin2::log::info("Guild war reward: war ended, guild {:d} scored {:d}, guild {:d} scored {:d}", data_.guild_from, score_from, data_.guild_to, score_to);

        if (data_.power_from > data_.power_to) {
            if (data_.handicap > score_from - score_to) {
                metin2::log::info("Guild war reward: stronger guild {:d} did not overcome the handicap, guild {:d} wins", data_.guild_from, data_.guild_to);
                dwWinner = data_.guild_to;
            } else {
                metin2::log::info("Guild war reward: stronger guild {:d} overcame the handicap and wins", data_.guild_from);
                dwWinner = data_.guild_from;
            }
        } else {
            if (data_.handicap > score_to - score_from) {
                metin2::log::info("Guild war reward: stronger guild {:d} did not overcome the handicap, guild {:d} wins", data_.guild_to, data_.guild_from);
                dwWinner = data_.guild_from;
            } else {
                metin2::log::info("Guild war reward: stronger guild {:d} overcame the handicap and wins", data_.guild_to);
                dwWinner = data_.guild_to;
            }
        }

        std::string query = sql::format("UPDATE guild_war_reservation SET started=1,winner={},result1={},result2={} WHERE id={}",
            dwWinner, score_from, score_to, data_.id);
        db_manager::instance().async_query(query);

        if (bet_.empty())
            return;

        uint32_t dwTotalBet = data_.bet_from + data_.bet_to;
        uint32_t dwWinnerBet = 0;

        if (dwWinner == data_.guild_from)
            dwWinnerBet = data_.bet_from;
        else if (dwWinner == data_.guild_to)
            dwWinnerBet = data_.bet_to;
        else {
            metin2::log::error("Guild war reward: winner guild {:d} is not part of war {:d}", dwWinner, data_.id);
            return;
        }

        if (dwWinnerBet == 0) {
            metin2::log::info("Guild war reward: nobody bet on winner guild {:d} in war {:d}", dwWinner, data_.id);
            return;
        }

        metin2::log::info("Guild war reward: total bet {:d}, bet on winner {:d}", dwTotalBet, dwWinnerBet);

        elk_award_batch awards;
        for (const auto &[login, bet] : bet_) {
            const auto [guild_id, gold] = bet;
            if (guild_id != dwWinner)
                continue;

            double ratio = (double)gold / dwWinnerBet;

            metin2::log::info("Guild war reward: account {} bet {:d}, share {:f}", login, gold, ratio);

            awards.add(login, (uint32_t)(dwTotalBet * ratio * 0.9));
        }
        awards.flush();
    }
}
