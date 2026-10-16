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
#include <metin2/proto/length.h>
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
        desc *d = desc_manager::instance().find_by_login_key(login_key);

        if (!d)
            return;

        d->set_login(info.login);

        metin2::log::debug("Auth login query result: key {:d}, connection {}", login_key, static_cast<const void *>(d));

        if (msg.get()->num_rows == 0) {
            metin2::log::info("Auth login refused: account {} not found", info.login);
            login_failure(d, "NOID");
            return;
        }

        MYSQL_ROW row = mysql_fetch_row(msg.get()->res);
        int col = 0;

        char szEncrytPassword[45 + 1];
        char szPassword[45 + 1];
        char szSocialID[SOCIAL_ID_MAX_LEN + 1];
        char szStatus[ACCOUNT_STATUS_MAX_LEN + 1];
        uint32_t dwID = 0;

        if (!row[col]) {
            metin2::log::error("Auth login: column {:d} is null for account {}", col, info.login);
            return;
        }

        strlcpy(szEncrytPassword, row[col++], sizeof(szEncrytPassword));

        if (!row[col]) {
            metin2::log::error("Auth login: column {:d} is null for account {}", col, info.login);
            return;
        }

        strlcpy(szPassword, row[col++], sizeof(szPassword));

        if (!row[col]) {
            metin2::log::error("Auth login: column {:d} is null for account {}", col, info.login);
            return;
        }

        strlcpy(szSocialID, row[col++], sizeof(szSocialID));

        if (!row[col]) {
            metin2::log::error("Auth login: column {:d} is null for account {}", col, info.login);
            return;
        }

        str_to_number(dwID, row[col++]);

        if (!row[col]) {
            metin2::log::error("Auth login: column {:d} is null for account {}", col, info.login);
            return;
        }

        strlcpy(szStatus, row[col++], sizeof(szStatus));

        uint8_t bNotAvail = 0;
        str_to_number(bNotAvail, row[col++]);

        int aiPremiumTimes[PREMIUM_MAX_NUM];
        memset(&aiPremiumTimes, 0, sizeof(aiPremiumTimes));

        char szCreateDate[256] = "00000000";

        str_to_number(aiPremiumTimes[PREMIUM_EXP], row[col++]);
        str_to_number(aiPremiumTimes[PREMIUM_ITEM], row[col++]);
        str_to_number(aiPremiumTimes[PREMIUM_SAFEBOX], row[col++]);
        str_to_number(aiPremiumTimes[PREMIUM_AUTOLOOT], row[col++]);
        str_to_number(aiPremiumTimes[PREMIUM_FISH_MIND], row[col++]);
        str_to_number(aiPremiumTimes[PREMIUM_MARRIAGE_FAST], row[col++]);
        str_to_number(aiPremiumTimes[PREMIUM_GOLD], row[col++]);

        int32_t retValue = 0;
        str_to_number(retValue, row[col]);

        time_t create_time = retValue;
        struct tm *tm1;
        tm1 = localtime(&create_time);
        strftime(szCreateDate, 255, "%Y%m%d", tm1);

        metin2::log::debug("Auth login: account created at {:d} ({})", retValue, szCreateDate);
        metin2::log::debug("Auth login: creation date vs block date: {:d}", strncmp(szCreateDate, metin2::config::auth().block_login_date.c_str(), 8));

        int nPasswordDiff = strcmp(szEncrytPassword, szPassword);

        if (nPasswordDiff) {
            login_failure(d, "WRONGPWD");
            metin2::log::info("Auth login refused: wrong password for account {}", info.login);
        } else if (bNotAvail) {
            login_failure(d, "NOTAVAIL");
            metin2::log::info("Auth login refused: account {} not available", info.login);
        } else if (desc_manager::instance().find_by_login_name(info.login)) {
            login_failure(d, "ALREADY");
            metin2::log::info("Auth login refused: account {} already logged in", info.login);
        } else if (strcmp(szStatus, "OK")) {
            login_failure(d, szStatus);
            metin2::log::info("Auth login refused: account {} has status {}", info.login, szStatus);
        } else {
            if (strncmp(szCreateDate, metin2::config::auth().block_login_date.c_str(), 8) >= 0) {
                login_failure(d, "BLKLOGIN");
                metin2::log::info("Auth login refused: account {} created after the login block date", info.login);
                return;
            }

            auto update = db_manager::instance().direct_query(sql::format("UPDATE account SET last_play=NOW() WHERE id={}", dwID));

            account_table &r = d->get_account_table();

            r.id = dwID;
            trim_and_lower(info.login, r.login, sizeof(r.login));
            strlcpy(r.passwd, info.passwd, sizeof(r.passwd));
            strlcpy(r.social_id, szSocialID, sizeof(r.social_id));
            desc_manager::instance().connect_account(r.login, d);

            db_manager::instance().login_prepare(d, info.client_key, aiPremiumTimes);
            metin2::log::info("Auth login: account {} accepted", info.login);
        }
    }

    input_auth::input_auth() {
    }

    void input_auth::login(desc *d, const char *data) {

        packet_cg_login3 *pinfo = (packet_cg_login3 *)data;

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

        char szLogin[sizeof(pinfo->login)];
        copy_packet_string(szLogin, pinfo->login);

        char login[LOGIN_MAX_LEN + 1];
        trim_and_lower(szLogin, login, sizeof(login));

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
            packet_gc_login_failure failurePacket;

            failurePacket.header = HEADER_GC_LOGIN_FAILURE;
            strlcpy(failurePacket.status, "SHUTDOWN", sizeof(failurePacket.status));

            d->packet(&failurePacket, sizeof(failurePacket));
            return;
        }

        if (desc_manager::instance().find_by_login_name(login)) {
            login_failure(d, "ALREADY");
            return;
        }

        uint32_t dwKey = desc_manager::instance().create_login_key(d);
        metin2::log::info("Auth login: key {:d} for account {}", dwKey, login);

        auto p = std::make_unique<packet_cg_login3>();
        std::memcpy(p.get(), pinfo, sizeof(packet_cg_login3));

        p->login[sizeof(p->login) - 1] = '\0';
        p->passwd[sizeof(p->passwd) - 1] = '\0';

        auto on_result = [dwKey, p = std::move(p)](sql_msg &msg) { auth_login_result(msg, dwKey, *p); };

        if (login_is_in_channel_service(login)) {
            metin2::log::debug("Auth login: account {} uses channel service", login);

            db_manager::instance().func_query(std::move(on_result), sql::format("SELECT {},password,social_id,id,status,availDt - NOW() > 0,"
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
            db_manager::instance().func_query(std::move(on_result), sql::format("SELECT PASSWORD({}),password,social_id,id,status,availDt - NOW() > 0,"
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

        int iExtraLen = 0;

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

        return iExtraLen;
    }
}
