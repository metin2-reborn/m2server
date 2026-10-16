#include <metin2/db/app/main.h>
#include <metin2/db/clients/client_manager.h>
#include <metin2/db/clients/peer.h>
#include <metin2/db/guilds/guild_manager.h>
#include <metin2/db/storage/db_manager.h>

#include <metin2/common/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/guild_types.h>
#include <metin2/proto/tables.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <cstdint>
#include <memory>
#include <string>

namespace metin2::db::inline clients {

    void client_manager::guild_create(peer *peer, uint32_t guild_id) {
        metin2::log::info("Guild: created guild {:d}", guild_id);
        forward_packet(HEADER_DG_GUILD_LOAD, &guild_id, sizeof(uint32_t));

        guild_manager::instance().load(guild_id);
    }

    void client_manager::guild_change_grade(peer *peer, packet_guild *p) {
        metin2::log::info("Guild: guild {:d} changed grade {:d}", p->guild, p->info);
        forward_packet(HEADER_DG_GUILD_CHANGE_GRADE, p, sizeof(packet_guild));
    }

    void client_manager::guild_add_member(peer *peer, packet_gd_guild_add_member *p) {
        guild_manager::instance().touch_guild(p->guild);
        metin2::log::info("Guild: guild {:d} added member pid {:d}", p->guild, p->pid);

        const std::string insert_query = sql::format(
            "INSERT INTO guild_member{} VALUES({}, {}, {}, 0, 0)",
            sql::raw(get_table_postfix()), p->pid, p->guild, p->grade);

        auto pmsg_insert = db_manager::instance().direct_query(insert_query);

        const std::string query = sql::format(
            "SELECT pid, grade, is_general, offer, level, job, name FROM guild_member{0}, player{0} WHERE guild_id = {1} and pid = id and pid = {2}", sql::raw(get_table_postfix()), p->guild, p->pid);

        auto pmsg = db_manager::instance().direct_query(query);

        if (pmsg->get()->num_rows == 0) {
            metin2::log::error("Guild: cannot load guild member data: {}", pmsg->query);
            return;
        }

        MYSQL_ROW row = mysql_fetch_row(pmsg->get()->res.get());

        if (!row[0] || !row[1])
            return;

        packet_dg_guild_member dg;

        dg.guild = p->guild;
        str_to_number(dg.pid, row[0]);
        str_to_number(dg.grade, row[1]);
        str_to_number(dg.is_general, row[2]);
        str_to_number(dg.offer, row[3]);
        str_to_number(dg.level, row[4]);
        str_to_number(dg.job, row[5]);
        strlcpy(dg.name, row[6], sizeof(dg.name));

        forward_packet(HEADER_DG_GUILD_ADD_MEMBER, &dg, sizeof(packet_dg_guild_member));
    }

    void client_manager::guild_remove_member(peer *peer, packet_guild *p) {
        metin2::log::info("Guild: guild {:d} removed member pid {:d}", p->guild, p->info);

        db_manager::instance().async_query(sql::format("DELETE FROM guild_member{} WHERE pid={} and guild_id={}", sql::raw(get_table_postfix()), p->info, p->guild));

        db_manager::instance().async_query(sql::format("REPLACE INTO quest{} (dwPID, szName, szState, lValue) VALUES({}, 'guild_manage', 'withdraw_time', {})", sql::raw(get_table_postfix()), p->info, (uint32_t)current_time()));

        forward_packet(HEADER_DG_GUILD_REMOVE_MEMBER, p, sizeof(packet_guild));
    }

    void client_manager::guild_skill_update(peer *peer, packet_guild_skill_update *p) {
        metin2::log::debug("Guild: skill update, amount {:d}", p->amount);
        forward_packet(HEADER_DG_GUILD_SKILL_UPDATE, p, sizeof(packet_guild_skill_update));
    }

    void client_manager::guild_exp_update(peer *peer, packet_guild_exp_update *p) {
        metin2::log::debug("Guild: exp update, amount {:d}", p->amount);
        forward_packet(HEADER_DG_GUILD_EXP_UPDATE, p, sizeof(packet_guild_exp_update), 0, peer);
    }

    void client_manager::guild_change_member_data(peer *peer, packet_guild_change_member_data *p) {
        metin2::log::debug("Guild: member pid {:d} offer {:d} level {:d} grade {:d}", p->pid, p->offer, p->level, p->grade);
        forward_packet(HEADER_DG_GUILD_CHANGE_MEMBER_DATA, p, sizeof(packet_guild_change_member_data), 0, peer);
    }

    void client_manager::guild_disband(peer *peer, packet_guild *p) {
        metin2::log::info("Guild: disbanded guild {:d}", p->guild);

        db_manager::instance().async_query(sql::format("DELETE FROM guild{} WHERE id={}", sql::raw(get_table_postfix()), p->guild));

        db_manager::instance().async_query(sql::format("DELETE FROM guild_grade{} WHERE guild_id={}", sql::raw(get_table_postfix()), p->guild));

        db_manager::instance().async_query(sql::format("REPLACE INTO quest{0} (dwPID, szName, szState, lValue) SELECT pid, 'guild_manage', 'withdraw_time', {1} FROM guild_member{0} WHERE guild_id = {2}", sql::raw(get_table_postfix()), (uint32_t)current_time(), p->guild));

        db_manager::instance().async_query(sql::format("DELETE FROM guild_member{} WHERE guild_id={}", sql::raw(get_table_postfix()), p->guild));

        db_manager::instance().async_query(sql::format("DELETE FROM guild_comment{} WHERE guild_id={}", sql::raw(get_table_postfix()), p->guild));

        forward_packet(HEADER_DG_GUILD_DISBAND, p, sizeof(packet_guild));
    }

    auto get_war_type(int n) -> const char * {
        switch (n) {
        case 0:
            return "패왕";
        case 1:
            return "맹장";
        case 2:
            return "수호";
        default:
            return "없는 번호";
        }
    }

    void client_manager::guild_war(peer *peer, packet_guild_war *p) {
        switch (p->war) {
        case GUILD_WAR_SEND_DECLARE:
            metin2::log::info("Guild war (type {}): declared by guild {:d} on guild {:d}", get_war_type(p->type), p->guild_from, p->guild_to);
            guild_manager::instance().add_declare(p->type, p->guild_from, p->guild_to);
            break;

        case GUILD_WAR_REFUSE:
            metin2::log::info("Guild war (type {}): between guild {:d} and guild {:d} refused", get_war_type(p->type), p->guild_from, p->guild_to);
            guild_manager::instance().remove_declare(p->guild_from, p->guild_to);
            break;

        case GUILD_WAR_WAIT_START:
            metin2::log::info("Guild war (type {}): between guild {:d} and guild {:d} waiting to start", get_war_type(p->type), p->guild_from, p->guild_to);
            [[fallthrough]];
        case GUILD_WAR_RESERVE:
            if (p->war != GUILD_WAR_WAIT_START)
                metin2::log::info("Guild war (type {}): between guild {:d} and guild {:d} reserved", get_war_type(p->type), p->guild_from, p->guild_to);
            guild_manager::instance().remove_declare(p->guild_from, p->guild_to);

            if (!guild_manager::instance().reserve_war(p))
                p->war = GUILD_WAR_CANCEL;
            else
                p->war = GUILD_WAR_RESERVE;

            break;

        case GUILD_WAR_ON_WAR:
            metin2::log::info("Guild war (type {}): between guild {:d} and guild {:d} started", get_war_type(p->type), p->guild_from, p->guild_to);
            guild_manager::instance().remove_declare(p->guild_from, p->guild_to);
            guild_manager::instance().start_war(p->type, p->guild_from, p->guild_to);
            break;

        case GUILD_WAR_OVER:
            metin2::log::info("Guild war (type {}): between guild {:d} and guild {:d} is over", get_war_type(p->type), p->guild_from, p->guild_to);
            guild_manager::instance().recv_war_over(p->guild_from, p->guild_to, p->type, p->war_price);
            break;

        case GUILD_WAR_END:
            metin2::log::info("Guild war (type {}): between guild {:d} and guild {:d} ended", get_war_type(p->type), p->guild_from, p->guild_to);
            guild_manager::instance().recv_war_end(p->guild_from, p->guild_to);
            return; // recv_war_end already broadcasts the packet.

        case GUILD_WAR_CANCEL:
            metin2::log::info("Guild war (type {}): between guild {:d} and guild {:d} cancelled", get_war_type(p->type), p->guild_from, p->guild_to);
            guild_manager::instance().cancel_war(p->guild_from, p->guild_to);
            break;
        }

        forward_packet(HEADER_DG_GUILD_WAR, p, sizeof(packet_guild_war));
    }

    void client_manager::guild_war_score(peer *peer, packet_guild_war_score *p) {
        guild_manager::instance().update_score(p->guild_gain_point, p->guild_opponent, p->score, p->bet_score);
    }

    void client_manager::guild_change_ladder_point(packet_guild_ladder_point *p) {
        metin2::log::debug("Guild: guild {:d} ladder points changed by {:d}", p->guild, p->change);
        guild_manager::instance().change_ladder_point(p->guild, p->change);
    }

    void client_manager::guild_use_skill(packet_guild_use_skill *p) {
        metin2::log::debug("Guild: guild {:d} used skill {:d}", p->guild, p->skill_vnum);
        guild_manager::instance().use_skill(p->guild, p->skill_vnum, p->cooltime);
        send_guild_skill_usable(p->guild, p->skill_vnum, false);
    }

    void client_manager::send_guild_skill_usable(uint32_t guild_id, uint32_t skill_vnum_value, bool usable_value) {
        metin2::log::debug("Guild: guild {:d} skill {:d} usable: {}", guild_id, skill_vnum_value, usable_value ? "true" : "false");

        packet_guild_skill_usable_change p;

        p.guild = guild_id;
        p.skill_vnum = skill_vnum_value;
        p.usable = usable_value;

        forward_packet(HEADER_DG_GUILD_SKILL_USABLE_CHANGE, &p, sizeof(packet_guild_skill_usable_change));
    }

    void client_manager::guild_change_master(packet_change_guild_master *p) {
        if (guild_manager::instance().change_master(p->guild_id, p->id_from, p->id_to) == true) {
            packet_change_guild_master packet;
            packet.guild_id = p->guild_id;
            packet.id_from = 0;
            packet.id_to = 0;

            forward_packet(HEADER_DG_ACK_CHANGE_GUILD_MASTER, &packet, sizeof(packet));
        }
    }
}
