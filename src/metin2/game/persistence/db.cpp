#include <metin2/config/config.h>
#include <metin2/game/persistence/db.h>

#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/items/item.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/protocol/input.h>
#include <metin2/game/protocol/login_data.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/text/spam.h>

#include <metin2/common/utils.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <sys/types.h>

namespace metin2::game::inline persistence {

    db_manager::db_manager() : is_connect_(false) {
    }

    db_manager::~db_manager() {
    }

    auto db_manager::connect(const char *host, const int port, const char *user, const char *pwd, const char *db) -> bool {
        if (sql_.setup(host, user, pwd, db, locale.c_str(), false, port))
            is_connect_ = true;

        if (!sql_direct_.setup(host, user, pwd, db, locale.c_str(), true, port))
            metin2::log::error("Cannot open direct sql connection to {}", host);

        if (is_connect_ && !metin2::config::is_auth()) {
            load_db_string();
        }

        return is_connect_;
    }

    void db_manager::query(const std::string &query) {
        sql_.async_query(query.c_str());
    }

    auto db_manager::direct_query(const std::string &query) -> std::unique_ptr<sql_msg> {
        return sql_direct_.direct_query(query.c_str());
    }

    auto db_manager::is_connected() -> bool {
        return is_connect_;
    }

    auto db_manager::pop_result() -> std::unique_ptr<sql_msg> {
        return sql_.pop_result();
    }

    void db_manager::process() {
        while (auto msg = pop_result())
            if (msg->on_result)
                msg->on_result(*msg);
    }

    void db_manager::func_query(sql_callback f, const std::string &query) {
        sql_.return_query(query.c_str(), std::move(f));
    }

    void db_manager::func_after_query(std::move_only_function<void()> f, const std::string &query) {
        sql_.return_query(query.c_str(), [f = std::move(f)](sql_msg &) mutable { f(); });
    }

    auto db_manager::get_login_data(uint32_t key) -> login_data * {
        auto it = map_login_data_.find(key);

        if (it == map_login_data_.end())
            return nullptr;

        return it->second.get();
    }

    void db_manager::insert_login_data(std::unique_ptr<login_data> ld) {
        const uint32_t dwKey = ld->key();
        map_login_data_.insert(std::make_pair(dwKey, std::move(ld)));
    }

    void db_manager::delete_login_data(login_data *ld) {
        auto it = map_login_data_.find(ld->key());

        if (it == map_login_data_.end())
            return;

        metin2::log::debug("Login data of account {} released ({})", ld->login(), static_cast<const void *>(ld));

        map_login_data_.erase(it);
    }

    void db_manager::send_auth_login(desc *d) {
        const account_table &r = d->get_account_table();

        login_data *pkLD = get_login_data(d->get_login_key());

        if (!pkLD)
            return;

        packet_gd_auth_login ptod;
        ptod.id = r.id;

        trim_and_lower(r.login, ptod.login);
        strlcpy(ptod.social_id, r.social_id, sizeof(ptod.social_id));
        ptod.login_key = d->get_login_key();

        std::memcpy(ptod.premium_times, pkLD->premium_ptr(), sizeof(ptod.premium_times));
        std::memcpy(&ptod.client_key, pkLD->client_key(), sizeof(uint32_t) * 4);

        db_clientdesc->db_packet(HEADER_GD_AUTH_LOGIN, d->handle(), &ptod, sizeof(packet_gd_auth_login));
        metin2::log::debug("Auth login sent to db: account {} (id {:d})", ptod.login, ptod.id);

        delete_login_data(pkLD);
    }

    void db_manager::login_prepare(desc *d, uint32_t *client_key, int *pai_premium_times) {
        const account_table &r = d->get_account_table();

        auto pkLD = std::make_unique<login_data>();

        pkLD->set_key(d->get_login_key());
        pkLD->set_login(r.login);
        pkLD->set_client_key(client_key);

        if (pai_premium_times)
            pkLD->set_premium(pai_premium_times);

        insert_login_data(std::move(pkLD));

        send_auth_login(d);
    }

    void give_lotto_ticket_result(sql_msg &msg, uint32_t pid, uint32_t vnum, uint32_t count, uint32_t round) {
        character *ch = character_manager::instance().find_by_pid(pid);

        if (!ch)
            return;

        if (msg.get()->affected_rows == 0 || msg.get()->affected_rows == (uint32_t)-1) {
            metin2::log::warn("Lotto: cannot record ticket for pid {:d}", ch->player_id());
            return;
        }

        item *pkItem = ch->auto_give_item(vnum, count);

        if (pkItem) {
            metin2::log::info("Lotto: ticket given to player {} (pid {:d})", ch->name(), pid);

            pkItem->set_socket(0, msg.get()->insert_id);
            pkItem->set_socket(1, round);
        } else
            metin2::log::warn("Lotto: cannot give ticket item to pid {:d}", ch->player_id());
    }

    void db_manager::load_db_string() {
        func_query([this](sql_msg &msg) {
            map_dbstring_.clear();
            vec_greet_message_.clear();

            for (uint i = 0; i < msg.get()->num_rows; ++i) {
                MYSQL_ROW row = mysql_fetch_row(msg.get()->res.get());
                if (row[0] && row[1]) {
                    map_dbstring_.insert(make_pair(std::string(row[0]), std::string(row[1])));
                    metin2::log::debug("Db string {} = \"{}\"", row[0], row[1]);
                }
            }
            if (map_dbstring_.find("GREET") != map_dbstring_.end()) {
                std::istringstream is(map_dbstring_["GREET"]);
                while (!is.eof()) {
                    std::string str;
                    getline(is, str);
                    vec_greet_message_.push_back(str);
                }
            }
        },
            sql::format("SELECT name, text FROM string{}", sql::raw(get_table_postfix())));
    }

    auto db_manager::greet_message() -> const std::vector<std::string> & {
        return vec_greet_message_;
    }

    void db_manager::send_money_log(uint8_t type, uint32_t vnum, int gold) {
        if (!gold)
            return;
        packet_money_log p;
        p.type = type;
        p.vnum = vnum;
        p.gold = gold;
        db_clientdesc->db_packet(HEADER_GD_MONEY_LOG, 0, &p, sizeof(p));
    }

    account_db::account_db() : is_connect_(false) {
    }

    auto account_db::is_connected() -> bool {
        return is_connect_;
    }

    auto account_db::connect(const char *host, const int port, const char *user, const char *pwd, const char *db) -> bool {
        is_connect_ = sql_direct_.setup(host, user, pwd, db, "", true, port);

        if (false == is_connect_) {
            fprintf(stderr, "cannot open direct sql connection to host: %s user: %s db: %s\n", host, user, db);
            return false;
        }

        return is_connect_;
    }

    auto account_db::connect_async(const char *host, const int port, const char *user, const char *pwd, const char *db, const char *locale) -> bool {
        sql_.setup(host, user, pwd, db, locale, false, port);
        return true;
    }

    void account_db::set_locale(const std::string &locale) {
        sql_direct_.set_locale(locale);
        sql_direct_.query_locale_set();
    }

    auto account_db::direct_query(const char *query) -> std::unique_ptr<sql_msg> {
        return sql_direct_.direct_query(query);
    }

    void account_db::func_query(sql_callback f, const std::string &query) {
        sql_.return_query(query.c_str(), std::move(f));
    }

    auto account_db::pop_result() -> std::unique_ptr<sql_msg> {
        return sql_.pop_result();
    }

    void account_db::process() {
        while (auto msg = pop_result())
            if (msg->on_result)
                msg->on_result(*msg);
    }

    static event_ptr reload_spam_event_handle = nullptr;

    static void query_spam_db() {
        account_db::instance().func_query([](sql_msg &msg) {
            if (msg.get()->num_rows > 0) {
                MYSQL_ROW row;

                spam_manager::instance().clear();

                while ((row = mysql_fetch_row(msg.get()->res.get())))
                    spam_manager::instance().insert(row[0], atoi(row[1]));
            }
        },
            "SELECT word, score FROM spam_db WHERE type='SPAM'");
    }

    EVENTINFO(reload_spam_event_info) {
        uint32_t empty;
    };

    EVENTFUNC(reload_spam_event) {
        query_spam_db();
        return passes_per_sec(metin2::config::game().spam.reload_cycle);
    }

    void load_spam_db() {
        query_spam_db();

        if (nullptr == reload_spam_event_handle) {
            reload_spam_event_info *info = alloc_event_info<reload_spam_event_info>();
            reload_spam_event_handle = event_create(reload_spam_event, info, passes_per_sec(metin2::config::game().spam.reload_cycle));
        }
    }

    void cancel_reload_spam_event() {
        event_cancel(&reload_spam_event_handle);
    }
}
