
#include <metin2/db/app/main.h>
#include <metin2/db/caches/cache.h>
#include <metin2/db/clients/client_manager.h>
#include <metin2/db/clients/peer.h>
#include <metin2/db/login/login_data.h>
#include <metin2/db/storage/db_manager.h>

#include <metin2/common/utils.h>
#include <metin2/config/config.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/limits.h>
#include <metin2/proto/tables.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include <strings.h>

namespace metin2::db::inline clients {

    auto client_manager::insert_logon_account(const char *login, uint32_t handle) -> bool {
        char login_value[LOGIN_MAX_LEN + 1];
        trim_and_lower(login, login_value);

        auto it = map_logon_account_.find(login_value);

        if (map_logon_account_.end() != it)
            return false;

        login_data *ld = get_login_data_by_login(login);

        if (!ld)
            return false;

        ld->set_connected_peer_handle(handle);

        map_logon_account_.insert(logon_account_map::value_type(login_value, ld));
        return true;
    }

    auto client_manager::delete_logon_account(const char *login, uint32_t handle) -> bool {
        char login_value[LOGIN_MAX_LEN + 1];
        trim_and_lower(login, login_value);

        auto it = map_logon_account_.find(login_value);

        if (it == map_logon_account_.end())
            return false;

        login_data *ld = it->second;

        if (ld->connected_peer_handle() != handle) {
            metin2::log::warn("Account {} logout from peer handle {:d}, but it is logged in on handle {:d}", login_value, handle, ld->connected_peer_handle());
            return false;
        }

        ld->set_play(false);

        if (ld->is_deleted()) {
            delete ld;
        }

        map_logon_account_.erase(it);
        return true;
    }

    auto client_manager::find_logon_account(const char *login) -> bool {
        char login_value[LOGIN_MAX_LEN + 1];
        trim_and_lower(login, login_value);

        if (map_logon_account_.end() == map_logon_account_.find(login_value))
            return false;

        return true;
    }

    void client_manager::query_login_by_key(peer *peer_value, uint32_t handle_value, packet_gd_login_by_key *p) {

        login_data *login_data = get_login_data(p->login_key);
        char login_value[LOGIN_MAX_LEN + 1];
        trim_and_lower(p->login, login_value);

        if (!login_data) {
            metin2::log::info("Login: account {} used unknown login key {:d}", login_value, p->login_key);
            peer_value->encode_return(HEADER_DG_LOGIN_NOT_EXIST, handle_value);
            return;
        }

        account_table &r = login_data->account_ref();

        if (find_logon_account(r.login)) {
            metin2::log::info("Login: account {} (key {:d}) is already logged in", r.login, p->login_key);
            packet_dg_login_already ptog;
            strlcpy(ptog.login, login_value, sizeof(ptog.login));
            peer_value->encode_header(HEADER_DG_LOGIN_ALREADY, handle_value, sizeof(packet_dg_login_already));
            peer_value->encode(&ptog, sizeof(packet_dg_login_already));
            return;
        }

        if (strcasecmp(r.login, login_value)) {
            metin2::log::warn("Login: key {1:d} belongs to account {0}, but {2} tried to use it", r.login, p->login_key, login_value);
            peer_value->encode_return(HEADER_DG_LOGIN_NOT_EXIST, handle_value);
            return;
        }

        if (memcmp(login_data->client_key(), p->client_key, sizeof(uint32_t) * 4)) {
            const uint32_t *client_key = login_data->client_key();

            metin2::log::warn("Login: client key mismatch for account {}: expected {:d} {:d} {:d} {:d}, got {:d} {:d} {:d} {:d}",
                r.login,
                p->client_key[0], p->client_key[1], p->client_key[2], p->client_key[3],
                client_key[0], client_key[1], client_key[2], client_key[3]);

            peer_value->encode_return(HEADER_DG_LOGIN_NOT_EXIST, handle_value);
            return;
        }

        auto tab = std::make_unique<account_table>();
        memset(tab.get(), 0, sizeof(account_table));

        tab->id = r.id;
        trim_and_lower(r.login, tab->login);
        strlcpy(tab->passwd, r.passwd, sizeof(tab->passwd));
        strlcpy(tab->social_id, r.social_id, sizeof(tab->social_id));
        strlcpy(tab->status, "OK", sizeof(tab->status));

        auto info = std::make_unique<client_handle_info>(handle_value);
        info->account_table = std::move(tab);
        strlcpy(info->ip, p->ip, sizeof(info->ip));

        metin2::log::info("Login: account {} logged in with key {:d} from {}", r.login, p->login_key, info->ip);
        const std::string query = sql::format("SELECT pid1, pid2, pid3, pid4, empire FROM player_index{} WHERE id={}", sql::raw(get_table_postfix()), r.id);
        db_manager::instance().return_query(query, [this, handle = peer_value->handle(), info = std::move(info)](sql_msg &msg) mutable {
            if (auto *peer_value = get_peer(handle))
                result_login_by_key(peer_value, msg, std::move(info));
        });
    }

    void client_manager::result_login_by_key(peer *peer, sql_msg &msg, std::unique_ptr<client_handle_info> info) {
        if (msg.sql_errno != 0) {
            peer->encode_return(HEADER_DG_LOGIN_NOT_EXIST, info->handle);
            return;
        }

        if (msg.get()->num_rows == 0) {
            uint32_t account_id = info->account_table->id;
            const std::string query = sql::format("SELECT pid1, pid2, pid3, pid4, empire FROM player_index{} WHERE id={}", sql::raw(get_table_postfix()), account_id);
            auto msg_value = db_manager::instance().direct_query(query, SQL_PLAYER);

            metin2::log::debug("Login: account {:d} has no players loaded, checking player_index", account_id);

            if (msg_value->get()->num_rows == 0) {
                metin2::log::info("Login: no player_index row for account {:d}, creating one", account_id);

                const std::string insert_query = sql::format("INSERT INTO player_index{} (id) VALUES({})", sql::raw(get_table_postfix()), info->account_table->id);
                db_manager::instance().return_query(insert_query, [this, handle = peer->handle(), info = std::move(info)](sql_msg &msg) mutable {
                    if (auto *peer_value = get_peer(handle))
                        result_player_index_create(peer_value, msg, std::move(info));
                });
            }
            return;
        }

        MYSQL_ROW row = mysql_fetch_row(msg.get()->res.get());

        int col = 0;

        for (; col < PLAYER_PER_ACCOUNT; ++col)
            str_to_number(info->account_table->players[col].id, row[col]);

        str_to_number(info->account_table->empire, row[col++]);

        const std::string query = sql::format(
            "SELECT id, name, job, level, playtime, st, ht, dx, iq, part_main, part_hair, x, y, skill_group, change_name FROM player{} WHERE account_id={}",
            sql::raw(get_table_postfix()), info->account_table->id);

        db_manager::instance().return_query(query, [this, handle = peer->handle(), info = std::move(info)](sql_msg &msg) mutable {
            if (auto *peer_value = get_peer(handle))
                result_login(peer_value, msg, std::move(info));
        });
    }

    void client_manager::result_player_index_create(peer *peer_value, sql_msg &msg, std::unique_ptr<client_handle_info> info) {
        const std::string query = sql::format("SELECT pid1, pid2, pid3, pid4, empire FROM player_index{} WHERE id={}", sql::raw(get_table_postfix()),
            info->account_table->id);
        db_manager::instance().return_query(query, [this, handle = peer_value->handle(), info = std::move(info)](sql_msg &msg) mutable {
            if (auto *peer_value = get_peer(handle))
                result_login_by_key(peer_value, msg, std::move(info));
        });
    }

    void create_account_player_data_from_res(MYSQL_RES *res, account_table *tab) {
        if (!res)
            return;

        for (uint32_t i = 0; i < mysql_num_rows(res); ++i) {
            MYSQL_ROW row = mysql_fetch_row(res);
            int col = 0;

            uint32_t player_id = 0;
            !row[col++] ? 0 : str_to_number(player_id, row[col - 1]);

            if (!player_id)
                continue;

            int j;

            for (j = 0; j < PLAYER_PER_ACCOUNT; ++j) {
                if (tab->players[j].id == player_id) {
                    player_table_cache *pc = client_manager::instance().player_cache(player_id);
                    player_table *pt = pc ? pc->get(false) : nullptr;

                    if (pt) {
                        strlcpy(tab->players[j].name, pt->name, sizeof(tab->players[j].name));

                        tab->players[j].job = pt->job;
                        tab->players[j].level = pt->level;
                        tab->players[j].play_minutes = pt->playtime;
                        tab->players[j].st = pt->st;
                        tab->players[j].ht = pt->ht;
                        tab->players[j].dx = pt->dx;
                        tab->players[j].iq = pt->iq;
                        tab->players[j].main_part = pt->parts[PART_MAIN];
                        tab->players[j].hair_part = pt->parts[PART_HAIR];
                        tab->players[j].x = pt->x;
                        tab->players[j].y = pt->y;
                        tab->players[j].skill_group = pt->skill_group;
                        tab->players[j].change_name = 0;
                    } else {
                        if (!row[col++])
                            *tab->players[j].name = '\0';
                        else
                            strlcpy(tab->players[j].name, row[col - 1], sizeof(tab->players[j].name));

                        tab->players[j].job = 0;
                        tab->players[j].level = 0;
                        tab->players[j].play_minutes = 0;
                        tab->players[j].st = 0;
                        tab->players[j].ht = 0;
                        tab->players[j].dx = 0;
                        tab->players[j].iq = 0;
                        tab->players[j].main_part = 0;
                        tab->players[j].hair_part = 0;
                        tab->players[j].x = 0;
                        tab->players[j].y = 0;
                        tab->players[j].skill_group = 0;
                        tab->players[j].change_name = 0;

                        str_to_number(tab->players[j].job, row[col++]);
                        str_to_number(tab->players[j].level, row[col++]);
                        str_to_number(tab->players[j].play_minutes, row[col++]);
                        str_to_number(tab->players[j].st, row[col++]);
                        str_to_number(tab->players[j].ht, row[col++]);
                        str_to_number(tab->players[j].dx, row[col++]);
                        str_to_number(tab->players[j].iq, row[col++]);
                        str_to_number(tab->players[j].main_part, row[col++]);
                        str_to_number(tab->players[j].hair_part, row[col++]);
                        str_to_number(tab->players[j].x, row[col++]);
                        str_to_number(tab->players[j].y, row[col++]);
                        str_to_number(tab->players[j].skill_group, row[col++]);
                        str_to_number(tab->players[j].change_name, row[col++]);
                    }

                    metin2::log::debug("Login: player {} at ({:d}, {:d}), hair {:d}",
                        tab->players[j].name, tab->players[j].x, tab->players[j].y, tab->players[j].hair_part);
                    break;
                }
            }
        }
    }

    void client_manager::result_login(peer *peer, sql_msg &msg, std::unique_ptr<client_handle_info> info) {
        if (!insert_logon_account(info->account_table->login, peer->handle())) {
            metin2::log::info("Login: account {} is already logged in", info->account_table->login);

            packet_dg_login_already p;
            strlcpy(p.login, info->account_table->login, sizeof(p.login));

            peer->encode_header(HEADER_DG_LOGIN_ALREADY, info->handle, sizeof(packet_dg_login_already));
            peer->encode(&p, sizeof(p));
        } else {
            metin2::log::info("Login: account {} logged in ({:d} players)", info->account_table->login, msg.get()->num_rows);

            if (msg.get()->num_rows > 0)
                create_account_player_data_from_res(msg.get()->res.get(), info->account_table.get());

            login_data *p = get_login_data_by_login(info->account_table->login);
            memcpy(&p->account_ref(), info->account_table.get(), sizeof(account_table));

            peer->encode_header(HEADER_DG_LOGIN_SUCCESS, info->handle, sizeof(account_table));
            peer->encode(info->account_table.get(), sizeof(account_table));
        }
    }

    void client_manager::query_logout(peer *peer, uint32_t handle_value, const char *data) {
        auto *packet = (logout_packet *)data;

        if (!*packet->login)
            return;

        login_data *login_data = get_login_data_by_login(packet->login);

        if (login_data == nullptr)
            return;

        int pid[PLAYER_PER_ACCOUNT];

        for (int n = 0; n < PLAYER_PER_ACCOUNT; ++n) {
            if (login_data->account_ref().players[n].id == 0) {
                if (metin2::config::test_server())
                    metin2::log::debug("Logout: account {} has empty slot (pid {:d})", packet->login, login_data->account_ref().players[n].id);
                continue;
            }

            pid[n] = login_data->account_ref().players[n].id;

            if (metin2::config::db().login_log)
                metin2::log::info("Logout: account {}, player pid {:d}", packet->login, pid[n]);

            insert_logout_player(pid[n]);
        }

        if (delete_logon_account(packet->login, peer->handle())) {
            if (metin2::config::db().login_log)
                metin2::log::info("Logout: account {}", packet->login);
        }
    }

    void client_manager::query_change_name(peer *peer, uint32_t handle, packet_gd_change_name *p) {
        std::string query = sql::format(
            "SELECT COUNT(*) as count FROM player{} WHERE name={} AND id <> {}", sql::raw(get_table_postfix()), p->name, p->pid);

        auto msg = db_manager::instance().direct_query(query, SQL_PLAYER);

        if (msg->get()->num_rows) {
            if (!msg->get()->res.get()) {
                peer->encode_header(HEADER_DG_PLAYER_CREATE_FAILED, handle, 0);
                return;
            }

            MYSQL_ROW row = mysql_fetch_row(msg->get()->res.get());

            if (*row[0] != '0') {
                peer->encode_header(HEADER_DG_PLAYER_CREATE_ALREADY, handle, 0);
                return;
            }
        } else {
            peer->encode_header(HEADER_DG_PLAYER_CREATE_FAILED, handle, 0);
            return;
        }

        query = sql::format(
            "UPDATE player{} SET name={},change_name=0 WHERE id={}", sql::raw(get_table_postfix()), p->name, p->pid);

        auto msg0 = db_manager::instance().direct_query(query, SQL_PLAYER);

        packet_dg_change_name pdg;
        peer->encode_header(HEADER_DG_CHANGE_NAME, handle, sizeof(packet_dg_change_name));
        pdg.pid = p->pid;
        strlcpy(pdg.name, p->name, sizeof(pdg.name));
        peer->encode(&pdg, sizeof(packet_dg_change_name));
    }
}
