#include <metin2/config/config.h>
#include <metin2/game/persistence/log.h>
#include <metin2/log/log.h>

#include <metin2/game/app/config.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/items/item.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/text/locale_service.h>

#include <metin2/proto/length.h>
#include <metin2/sql/query.h>

#include <cstdint>
#include <string>

namespace metin2::game::inline persistence {

    namespace {
        auto or_empty(const char *text) -> const char * {
            return text ? text : "";
        }
    }

    log_manager::log_manager() : is_connect_(false) {
    }

    log_manager::~log_manager() {
    }

    auto log_manager::connect(const char *host, const int port, const char *user, const char *pwd, const char *db) -> bool {
        if (sql_.setup(host, user, pwd, db, locale.c_str(), false, port))
            is_connect_ = true;

        return is_connect_;
    }

    void log_manager::query(const std::string &query) {
        if (metin2::config::test_server())
            metin2::log::debug("Log query: {}", query);

        sql_.async_query(query.c_str());
    }

    auto log_manager::is_connected() -> bool {
        return is_connect_;
    }

    void log_manager::item_log(uint32_t pid, uint32_t x, uint32_t y, uint32_t item_id, const char *text, const char *hint, const char *ip, uint32_t vnum) {
        query(sql::format("INSERT DELAYED INTO log{} (type, time, who, x, y, what, how, hint, ip, vnum) VALUES('ITEM', NOW(), {}, {}, {}, {}, {}, {}, {}, {})",
            sql::raw(get_table_postfix()), pid, x, y, item_id, or_empty(text), hint, or_empty(ip), vnum));
    }

    void log_manager::item_log(character *ch, item *item, const char *text, const char *hint) {
        if (nullptr == ch || nullptr == item) {
            metin2::log::error("Item log without character or item (character {}, item {}, text {})", static_cast<const void *>(ch), static_cast<const void *>(item), text);
            return;
        }

        item_log(ch->player_id(), ch->x(), ch->y(), item->id(),
            nullptr == text ? "" : text,
            hint, ch->get_desc() ? ch->get_desc()->host_name() : "",
            item->original_vnum());
    }

    void log_manager::item_log(character *ch, int item_id, int item_vnum, const char *text, const char *hint) {
        item_log(ch->player_id(), ch->x(), ch->y(), item_id, text, hint, ch->get_desc() ? ch->get_desc()->host_name() : "", item_vnum);
    }

    void log_manager::char_log(uint32_t pid, uint32_t x, uint32_t y, uint32_t value, const char *text, const char *hint, const char *ip) {
        query(sql::format("INSERT DELAYED INTO log{} (type, time, who, x, y, what, how, hint, ip) VALUES('CHARACTER', NOW(), {}, {}, {}, {}, {}, {}, {})",
            sql::raw(get_table_postfix()), pid, x, y, value, or_empty(text), hint, or_empty(ip)));
    }

    void log_manager::char_log(character *ch, uint32_t dw, const char *text, const char *hint) {
        if (ch)
            char_log(ch->player_id(), ch->x(), ch->y(), dw, text, hint, ch->get_desc() ? ch->get_desc()->host_name() : "");
        else
            char_log(0, 0, 0, dw, text, hint, "");
    }

    void log_manager::money_log(uint8_t type, uint32_t vnum, int gold) {
        if (type == MONEY_LOG_RESERVED || type >= MONEY_LOG_TYPE_MAX_NUM) {
            metin2::log::error("Money log with invalid type {:d} (vnum {:d}, gold {:d})", type, vnum, gold);
            return;
        }

        query(sql::format("INSERT DELAYED INTO money_log{} VALUES (NOW(), {}, {}, {})", sql::raw(get_table_postfix()), type, vnum, gold));
    }

    void log_manager::hack_log(const char *hack_name, const char *login, const char *name, const char *ip) {
        query(sql::format("INSERT INTO hack_log (time, login, name, ip, server, why) VALUES(NOW(), {}, {}, {}, {}, {})", login, name, ip, metin2::config::server().hostname, hack_name));
    }

    void log_manager::hack_log(const char *hack_name, character *ch) {
        if (ch->get_desc()) {
            hack_log(hack_name,
                ch->get_desc()->get_account_table().login,
                ch->name(),
                ch->get_desc()->host_name());
        }
    }

    void log_manager::gold_bar_log(uint32_t pid, uint32_t item_id, goldbar_how how, const char *hint) {
        const char *szHow;

        switch (how) {
        case PERSONAL_SHOP_BUY:
            szHow = "BUY";
            break;

        case PERSONAL_SHOP_SELL:
            szHow = "SELL";
            break;

        case SHOP_BUY:
            szHow = "SHOP_BUY";
            break;

        case SHOP_SELL:
            szHow = "SHOP_SELL";
            break;

        case EXCHANGE_TAKE:
            szHow = "EXCHANGE_TAKE";
            break;

        case EXCHANGE_GIVE:
            szHow = "EXCHANGE_GIVE";
            break;

        case QUEST:
            szHow = "QUEST";
            break;

        default:
            szHow = "";
            break;
        }

        query(sql::format("INSERT DELAYED INTO goldlog{} (date, time, pid, what, how, hint) VALUES(CURDATE(), CURTIME(), {}, {}, {}, {})",
            sql::raw(get_table_postfix()), pid, item_id, szHow, hint));
    }

    void log_manager::cube_log(uint32_t pid, uint32_t x, uint32_t y, uint32_t item_vnum, uint32_t item_uid, int item_count, bool success) {
        query(sql::format("INSERT DELAYED INTO cube{} (pid, time, x, y, item_vnum, item_uid, item_count, success) "
                          "VALUES({}, NOW(), {}, {}, {}, {}, {}, {})",
            sql::raw(get_table_postfix()), pid, x, y, item_vnum, item_uid, item_count, success ? 1 : 0));
    }

    void log_manager::speed_hack_log(uint32_t pid, uint32_t x, uint32_t y, int hack_count) {
        query(sql::format("INSERT INTO speed_hack{} (pid, time, x, y, hack_count) "
                          "VALUES({}, NOW(), {}, {}, {})",
            sql::raw(get_table_postfix()), pid, x, y, hack_count));
    }

    void log_manager::change_name_log(uint32_t pid, const char *old_name, const char *new_name, const char *ip) {
        query(sql::format("INSERT DELAYED INTO change_name{} (pid, old_name, new_name, time, ip) "
                          "VALUES({}, {}, {}, NOW(), {}) ",
            sql::raw(get_table_postfix()), pid, old_name, new_name, ip));
    }

    void log_manager::gm_command_log(uint32_t pid, const char *name, const char *ip, uint8_t channel, const char *command) {
        query(sql::format("INSERT DELAYED INTO command_log{} (userid, server, ip, port, username, command, date ) "
                          "VALUES({}, 999, {}, {}, {}, {}, NOW()) ",
            sql::raw(get_table_postfix()), pid, ip, channel, name, command));
    }

    void log_manager::refine_log(uint32_t pid, const char *item_name, uint32_t item_id, int item_refine_level, int is_success, const char *how) {
        query(sql::format("INSERT INTO refinelog{} (pid, item_name, item_id, step, time, is_success, setType) VALUES({}, {}, {}, {}, NOW(), {}, {})",
            sql::raw(get_table_postfix()), pid, item_name, item_id, item_refine_level, is_success, how));
    }

    void log_manager::shout_log(uint8_t channel, uint8_t empire, const char *text) {
        query(sql::format("INSERT INTO shout_log{} VALUES(NOW(), {}, {},{})", sql::raw(get_table_postfix()), channel, empire, text));
    }

    void log_manager::level_log(character *ch, unsigned int level, unsigned int playhour) {
        uint32_t aid = 0;

        if (nullptr != ch->get_desc()) {
            aid = ch->get_desc()->get_account_table().id;
        }

        query(sql::format("REPLACE INTO levellog{} (name, level, time, account_id, pid, playtime) VALUES({}, {}, NOW(), {}, {}, {})",
            sql::raw(get_table_postfix()), ch->name(), level, aid, ch->player_id(), playhour));
    }

    void log_manager::boot_log(const char *host_name, uint8_t channel) {
        query(sql::format("INSERT INTO bootlog (time, hostname, channel) VALUES(NOW(), {}, {})",
            host_name, channel));
    }

    void log_manager::fish_log(uint32_t pid, int prob_idx, int fish_id, int fish_level, uint32_t miliseconds, uint32_t vnum, uint32_t value) {
        query(sql::format("INSERT INTO fish_log{} VALUES(NOW(), {}, {}, {}, {}, {}, {}, {})",
            sql::raw(get_table_postfix()),
            pid,
            prob_idx,
            fish_id,
            fish_level,
            miliseconds,
            vnum,
            value));
    }

    void log_manager::quest_reward_log(const char *quest_name, uint32_t pid, uint32_t level, int value1, int value2) {
        query(sql::format("INSERT INTO quest_reward_log{} VALUES({},{},{},2,{},{},NOW())",
            sql::raw(get_table_postfix()),
            quest_name,
            pid,
            level,
            value1,
            value2));
    }

    void log_manager::detail_login_log(bool is_login, character *ch) {
        if (nullptr == ch->get_desc())
            return;

        if (true == is_login) {
            query(sql::format("INSERT INTO loginlog2(type, is_gm, login_time, channel, account_id, pid, ip, client_version) "
                              "VALUES('INVALID', {}, NOW(), {}, {}, {}, inet_aton({}), {})",
                ch->is_gm() == true ? "Y" : "N",
                metin2::config::server().channel,
                ch->get_desc()->get_account_table().id,
                ch->player_id(),
                ch->get_desc()->host_name(),
                ch->get_desc()->client_version()));
        } else {
            query(sql::format("SET @i = (SELECT MAX(id) FROM loginlog2 WHERE account_id={} AND pid={})",
                ch->get_desc()->get_account_table().id,
                ch->player_id()));

            query("UPDATE loginlog2 SET type='VALID', logout_time=NOW(), playtime=TIMEDIFF(logout_time,login_time) WHERE id=@i");
        }
    }

    void log_manager::dragon_slay_log(uint32_t guild_id, uint32_t dragon_vnum, uint32_t start_time, uint32_t end_time) {
        query(sql::format("INSERT INTO dragon_slay_log{} VALUES( {}, {}, FROM_UNIXTIME({}), FROM_UNIXTIME({}) )",
            sql::raw(get_table_postfix()),
            guild_id, dragon_vnum, start_time, end_time));
    }

}
