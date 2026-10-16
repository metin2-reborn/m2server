#include <metin2/config/config.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/protocol/input.h>
#include <metin2/game/protocol/packet.h>

#include <metin2/common/utils.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/limits.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <cctype>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>
#include <utility>

namespace metin2::game::inline protocol {

    auto fn_is_valid_login_string(const char *str) -> bool {
        const char *tmp;

        if (!str || !*str)
            return false;

        if (strlen(str) < 2)
            return false;

        for (tmp = str; *tmp; ++tmp) {
            if (isdigit((unsigned char)*tmp) || isalpha((unsigned char)*tmp))
                continue;

            return false;
        }

        return true;
    }

    auto login_is_in_channel_service(const char *login) -> bool {
        if (login[0] == '[')
            return true;
        return false;
    }

    static void auth_login_result(sql_msg &msg, uint32_t login_key, packet_cg_login3 &info) {
        desc *d = get_desc_manager().find_by_login_key(login_key);

        if (!d)
            return;

        d->set_login(info.login);

        metin2::log::debug("Auth login query result: key {:d}, connection {}", login_key, static_cast<const void *>(d));

        if (msg.get()->num_rows == 0) {
            metin2::log::info("Auth login refused: account {} not found", info.login);
            login_failure(d, "NOID");
            return;
        }

        MYSQL_ROW row = mysql_fetch_row(msg.get()->res.get());
        int col = 0;

        char encryt_password[45 + 1];
        char password_value[45 + 1];
        char social_id[SOCIAL_ID_MAX_LEN + 1];
        char status_value[ACCOUNT_STATUS_MAX_LEN + 1];
        uint32_t id_value = 0;

        if (!row[col]) {
            metin2::log::error("Auth login: column {:d} is null for account {}", col, info.login);
            return;
        }

        strlcpy(encryt_password, row[col++], sizeof(encryt_password));

        if (!row[col]) {
            metin2::log::error("Auth login: column {:d} is null for account {}", col, info.login);
            return;
        }

        strlcpy(password_value, row[col++], sizeof(password_value));

        if (!row[col]) {
            metin2::log::error("Auth login: column {:d} is null for account {}", col, info.login);
            return;
        }

        strlcpy(social_id, row[col++], sizeof(social_id));

        if (!row[col]) {
            metin2::log::error("Auth login: column {:d} is null for account {}", col, info.login);
            return;
        }

        str_to_number(id_value, row[col++]);

        if (!row[col]) {
            metin2::log::error("Auth login: column {:d} is null for account {}", col, info.login);
            return;
        }

        strlcpy(status_value, row[col++], sizeof(status_value));

        uint8_t not_avail = 0;
        str_to_number(not_avail, row[col++]);

        int ai_premium_times[PREMIUM_MAX_NUM];
        memset(&ai_premium_times, 0, sizeof(ai_premium_times));

        char create_date[256] = "00000000";

        str_to_number(ai_premium_times[PREMIUM_EXP], row[col++]);
        str_to_number(ai_premium_times[PREMIUM_ITEM], row[col++]);
        str_to_number(ai_premium_times[PREMIUM_SAFEBOX], row[col++]);
        str_to_number(ai_premium_times[PREMIUM_AUTOLOOT], row[col++]);
        str_to_number(ai_premium_times[PREMIUM_FISH_MIND], row[col++]);
        str_to_number(ai_premium_times[PREMIUM_MARRIAGE_FAST], row[col++]);
        str_to_number(ai_premium_times[PREMIUM_GOLD], row[col++]);

        int32_t ret_value = 0;
        str_to_number(ret_value, row[col]);

        time_t create_time = ret_value;
        struct tm *tm1;
        tm1 = localtime(&create_time);
        strftime(create_date, 255, "%Y%m%d", tm1);

        metin2::log::debug("Auth login: account created at {:d} ({})", ret_value, create_date);
        metin2::log::debug("Auth login: creation date vs block date: {:d}", strncmp(create_date, metin2::config::auth().block_login_date.c_str(), 8));

        int password_diff = strcmp(encryt_password, password_value);

        if (password_diff) {
            login_failure(d, "WRONGPWD");
            metin2::log::info("Auth login refused: wrong password for account {}", info.login);
        } else if (not_avail) {
            login_failure(d, "NOTAVAIL");
            metin2::log::info("Auth login refused: account {} not available", info.login);
        } else if (get_desc_manager().find_by_login_name(info.login)) {
            login_failure(d, "ALREADY");
            metin2::log::info("Auth login refused: account {} already logged in", info.login);
        } else if (strcmp(status_value, "OK")) {
            login_failure(d, status_value);
            metin2::log::info("Auth login refused: account {} has status {}", info.login, status_value);
        } else {
            if (strncmp(create_date, metin2::config::auth().block_login_date.c_str(), 8) >= 0) {
                login_failure(d, "BLKLOGIN");
                metin2::log::info("Auth login refused: account {} created after the login block date", info.login);
                return;
            }

            auto update = get_db_manager().direct_query(sql::format("UPDATE account SET last_play=NOW() WHERE id={}", id_value));

            account_table &r = d->get_account_table();

            r.id = id_value;
            trim_and_lower(info.login, r.login);
            strlcpy(r.passwd, info.passwd, sizeof(r.passwd));
            strlcpy(r.social_id, social_id, sizeof(r.social_id));
            get_desc_manager().connect_account(r.login, d);

            get_db_manager().login_prepare(d, info.client_key, ai_premium_times);
            metin2::log::info("Auth login: account {} accepted", info.login);
        }
    }

    input_auth::input_auth() = default;

    void input_auth::login(desc *d, const char *data) {

        auto *pinfo = (packet_cg_login3 *)data;

        if (!metin2::config::is_auth()) {
            metin2::log::warn("Auth packet from {} on a game server, disconnecting",
                d->host_name());
            d->delayed_disconnect(5);
            return;
        }

        // One login per connection: another would replace the key of a query in flight and allow
        // password guessing at line rate.
        if (d->get_login_key() || d->get_account_table().id) {
            metin2::log::warn("Second login attempt on one connection from {}, closing", d->host_name());
            d->set_phase(PHASE_CLOSE);
            return;
        }

        char login_value[sizeof(pinfo->login)];
        copy_packet_string(login_value, pinfo->login);

        char login[LOGIN_MAX_LEN + 1];
        trim_and_lower(login_value, login);

        char passwd[PASSWD_MAX_LEN + 1];
        copy_packet_string(passwd, pinfo->passwd);

        metin2::log::debug("Auth login: account {} (length {:d}), connection {}",
            login, strlen(login), static_cast<const void *>(d));

        if (false == fn_is_valid_login_string(login)) {
            metin2::log::info("Auth login refused: invalid account name {} (connection {})",
                login, static_cast<const void *>(d));
            login_failure(d, "NOID");
            return;
        }

        if (no_more_client) {
            packet_gc_login_failure failure_packet;

            failure_packet.header = HEADER_GC_LOGIN_FAILURE;
            strlcpy(failure_packet.status, "SHUTDOWN", sizeof(failure_packet.status));

            d->packet(&failure_packet, sizeof(failure_packet));
            return;
        }

        if (get_desc_manager().find_by_login_name(login)) {
            login_failure(d, "ALREADY");
            return;
        }

        uint32_t key_value = get_desc_manager().create_login_key(d);
        metin2::log::info("Auth login: key {:d} for account {}", key_value, login);

        auto p = std::make_unique<packet_cg_login3>();
        std::memcpy(p.get(), pinfo, sizeof(packet_cg_login3));

        p->login[sizeof(p->login) - 1] = '\0';
        p->passwd[sizeof(p->passwd) - 1] = '\0';

        auto on_result = [key_value, p = std::move(p)](sql_msg &msg) { auth_login_result(msg, key_value, *p); };

        if (login_is_in_channel_service(login)) {
            metin2::log::debug("Auth login: account {} uses channel service", login);

            get_db_manager().func_query(std::move(on_result), sql::format("SELECT {},password,social_id,id,status,availDt - NOW() > 0,"
                                                                          "UNIX_TIMESTAMP(silver_expire),"
                                                                          "UNIX_TIMESTAMP(gold_expire),"
                                                                          "UNIX_TIMESTAMP(safebox_expire),"
                                                                          "UNIX_TIMESTAMP(autoloot_expire),"
                                                                          "UNIX_TIMESTAMP(fish_mind_expire),"
                                                                          "UNIX_TIMESTAMP(marriage_fast_expire),"
                                                                          "UNIX_TIMESTAMP(money_drop_rate_expire),"
                                                                          "UNIX_TIMESTAMP(create_time)"
                                                                          " FROM account WHERE login={}",
                                                                  passwd, login));
        } else {
            get_db_manager().func_query(std::move(on_result), sql::format("SELECT PASSWORD({}),password,social_id,id,status,availDt - NOW() > 0,"
                                                                          "UNIX_TIMESTAMP(silver_expire),"
                                                                          "UNIX_TIMESTAMP(gold_expire),"
                                                                          "UNIX_TIMESTAMP(safebox_expire),"
                                                                          "UNIX_TIMESTAMP(autoloot_expire),"
                                                                          "UNIX_TIMESTAMP(fish_mind_expire),"
                                                                          "UNIX_TIMESTAMP(marriage_fast_expire),"
                                                                          "UNIX_TIMESTAMP(money_drop_rate_expire),"
                                                                          "UNIX_TIMESTAMP(create_time)"
                                                                          " FROM account WHERE login={}",
                                                                  passwd, login));
        }
    }

    auto input_auth::analyze(desc *d, uint8_t header, const char *data) -> int {

        if (!metin2::config::is_auth()) {
            metin2::log::warn("Auth packet from {} on a game server, disconnecting",
                d->host_name());
            d->delayed_disconnect(5);
            return 0;
        }

        int extra_len = 0;

        if (metin2::config::test_server())
            metin2::log::debug("Auth packet header {:d}", header);

        switch (header) {
        case HEADER_CG_PONG:
            pong(d);
            break;

        case HEADER_CG_LOGIN3:
            login(d, data);
            break;

        case HEADER_CG_HANDSHAKE:
            break;

        default:
            metin2::log::warn("Unhandled packet {:d} ({:#x}) in auth phase", header, header);
            break;
        }

        return extra_len;
    }
}
