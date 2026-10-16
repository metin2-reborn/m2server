
#include <metin2/db/caches/cache.h>
#include <metin2/db/clients/client_manager.h>
#include <metin2/db/storage/db_manager.h>
#include <metin2/db/login/login_data.h>
#include <metin2/db/app/main.h>
#include <metin2/db/clients/peer.h>
#include <metin2/db/clients/qid.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/common/utils.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>
#include <metin2/config/config.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <mysql.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include <strings.h>

namespace metin2::db::inline clients {


auto client_manager::insert_logon_account(const char * login, uint32_t handle) -> bool
{
	char szLogin[LOGIN_MAX_LEN + 1];
	trim_and_lower(login, szLogin, sizeof(szLogin));

	auto it = map_k_logon_account_.find(szLogin);

	if (map_k_logon_account_.end() != it)
		return false;

	login_data * pkLD = get_login_data_by_login(login);

	if (!pkLD)
		return false;

	pkLD->set_connected_peer_handle(handle);

	map_k_logon_account_.insert(logon_account_map::value_type(szLogin, pkLD));
	return true;
}

auto client_manager::delete_logon_account(const char * login, uint32_t handle) -> bool
{
	char szLogin[LOGIN_MAX_LEN + 1];
	trim_and_lower(login, szLogin, sizeof(szLogin));

	auto it = map_k_logon_account_.find(szLogin);

	if (it == map_k_logon_account_.end())
		return false;

	login_data * pkLD = it->second;

	if (pkLD->connected_peer_handle() != handle)
	{
		metin2::log::warn("Account {} logout from peer handle {:d}, but it is logged in on handle {:d}", szLogin, handle, pkLD->connected_peer_handle());
		return false;
	}

	pkLD->set_play(false);

	if (pkLD->is_deleted())
	{
		delete pkLD;
	}

	map_k_logon_account_.erase(it);
	return true;
}

auto client_manager::find_logon_account(const char * login) -> bool
{
	char szLogin[LOGIN_MAX_LEN + 1];
	trim_and_lower(login, szLogin, sizeof(szLogin));

	if (map_k_logon_account_.end() == map_k_logon_account_.find(szLogin))
		return false;

	return true;
}

void client_manager::query_login_by_key(peer * peer_value, uint32_t handle_value, packet_gd_login_by_key * p)
{

	login_data * pkLoginData = get_login_data(p->login_key);
	char szLogin[LOGIN_MAX_LEN + 1];
	trim_and_lower(p->login, szLogin, sizeof(szLogin));

	if (!pkLoginData)
	{
		metin2::log::info("Login: account {} used unknown login key {:d}", szLogin, p->login_key);
		peer_value->encode_return(HEADER_DG_LOGIN_NOT_EXIST, handle_value);
		return;
	}

	account_table & r = pkLoginData->account_ref();

	if (find_logon_account(r.login))
	{
		metin2::log::info("Login: account {} (key {:d}) is already logged in", r.login, p->login_key);
		packet_dg_login_already ptog;
		strlcpy(ptog.login, szLogin, sizeof(ptog.login));
		peer_value->encode_header(HEADER_DG_LOGIN_ALREADY, handle_value, sizeof(packet_dg_login_already));
		peer_value->encode(&ptog, sizeof(packet_dg_login_already));
		return;
	}

	if (strcasecmp(r.login, szLogin))
	{
		metin2::log::warn("Login: key {1:d} belongs to account {0}, but {2} tried to use it", r.login, p->login_key, szLogin);
		peer_value->encode_return(HEADER_DG_LOGIN_NOT_EXIST, handle_value);
		return;
	}

	if (memcmp(pkLoginData->client_key(), p->client_key, sizeof(uint32_t) * 4))
	{
		const uint32_t * pdwClientKey = pkLoginData->client_key();

		metin2::log::warn("Login: client key mismatch for account {}: expected {:d} {:d} {:d} {:d}, got {:d} {:d} {:d} {:d}",
				r.login,
				p->client_key[0], p->client_key[1], p->client_key[2], p->client_key[3],
				pdwClientKey[0], pdwClientKey[1], pdwClientKey[2], pdwClientKey[3]);

		peer_value->encode_return(HEADER_DG_LOGIN_NOT_EXIST, handle_value);
		return;
	}

	auto pkTab = std::make_unique<account_table>();
	memset(pkTab.get(), 0, sizeof(account_table));

	pkTab->id = r.id;
	trim_and_lower(r.login, pkTab->login, sizeof(pkTab->login));
	strlcpy(pkTab->passwd, r.passwd, sizeof(pkTab->passwd));
	strlcpy(pkTab->social_id, r.social_id, sizeof(pkTab->social_id));
	strlcpy(pkTab->status, "OK", sizeof(pkTab->status));

	client_handle_info * info = new client_handle_info(handle_value);
	info->account_table = std::move(pkTab);
	strlcpy(info->ip, p->ip, sizeof(info->ip));

	metin2::log::info("Login: account {} logged in with key {:d} from {}", r.login, p->login_key, info->ip);
	char szQuery[QUERY_MAX_LEN];
	snprintf(szQuery, sizeof(szQuery), "SELECT pid1, pid2, pid3, pid4, empire FROM player_index%s WHERE id=%u", get_table_postfix(), r.id);
	db_manager::instance().return_query(szQuery, QID_LOGIN_BY_KEY, peer_value->handle(), info);
}

void client_manager::result_login_by_key(peer * peer, sql_msg * msg)
{
	query_info * qi = (query_info *) msg->user_data;
	client_handle_info * info = (client_handle_info *) qi->pv_data;

	if (msg->sql_errno != 0)
	{
		peer->encode_return(HEADER_DG_LOGIN_NOT_EXIST, info->handle);
		delete info;
		return;
	}

	char szQuery[QUERY_MAX_LEN];

	if (msg->get()->num_rows == 0)
	{
		uint32_t account_id = info->account_table->id;
		char szQuery[QUERY_MAX_LEN];
		snprintf(szQuery, sizeof(szQuery), "SELECT pid1, pid2, pid3, pid4, empire FROM player_index%s WHERE id=%u", get_table_postfix(), account_id);
		std::unique_ptr<sql_msg> pMsg(db_manager::instance().direct_query(szQuery, SQL_PLAYER));
		
		metin2::log::debug("Login: account {:d} has no players loaded, checking player_index", account_id);

		if (pMsg->get()->num_rows == 0)
		{
			metin2::log::info("Login: no player_index row for account {:d}, creating one", account_id);

			snprintf(szQuery, sizeof(szQuery), "INSERT INTO player_index%s (id) VALUES(%u)", get_table_postfix(), info->account_table->id);
			db_manager::instance().return_query(szQuery, QID_PLAYER_INDEX_CREATE, peer->handle(), info);
		}
		return;
	}

	MYSQL_ROW row = mysql_fetch_row(msg->get()->res);

	int col = 0;

	for (; col < PLAYER_PER_ACCOUNT; ++col)
		str_to_number(info->account_table->players[col].id, row[col]);

	str_to_number(info->account_table->empire, row[col++]);
	info->account_index = 1;

	snprintf(szQuery, sizeof(szQuery),
			"SELECT id, name, job, level, playtime, st, ht, dx, iq, part_main, part_hair, x, y, skill_group, change_name FROM player%s WHERE account_id=%u",
			get_table_postfix(), info->account_table->id);

	db_manager::instance().return_query(szQuery, QID_LOGIN, peer->handle(), info);
}

void client_manager::result_player_index_create(peer * peer_value, sql_msg * msg)
{
	query_info * qi = (query_info *) msg->user_data;
	client_handle_info * info = (client_handle_info *) qi->pv_data;

	char szQuery[QUERY_MAX_LEN];
	snprintf(szQuery, sizeof(szQuery), "SELECT pid1, pid2, pid3, pid4, empire FROM player_index%s WHERE id=%u", get_table_postfix(), 
			info->account_table->id);
	db_manager::instance().return_query(szQuery, QID_LOGIN_BY_KEY, peer_value->handle(), info);
}

auto create_account_table_from_res(MYSQL_RES * res) -> std::unique_ptr<account_table>
{
	char input_pwd[PASSWD_MAX_LEN + 1];
	MYSQL_ROW row = nullptr;
	uint32_t col;

	row = mysql_fetch_row(res);
	col = 0;

	auto pkTab = std::make_unique<account_table>();
	memset(pkTab.get(), 0, sizeof(account_table));

	strlcpy(input_pwd, row[col++], sizeof(input_pwd));
	str_to_number(pkTab->id, row[col++]);
	strlcpy(pkTab->login, row[col++], sizeof(pkTab->login));
	strlcpy(pkTab->passwd, row[col++], sizeof(pkTab->passwd));
	strlcpy(pkTab->social_id, row[col++], sizeof(pkTab->social_id));
	str_to_number(pkTab->empire, row[col++]);

	for (int j = 0; j < PLAYER_PER_ACCOUNT; ++j)
		str_to_number(pkTab->players[j].id, row[col++]);

	strlcpy(pkTab->status, row[col++], sizeof(pkTab->status));

	if (strcmp(pkTab->passwd, input_pwd))
	{
		return nullptr;
	}

	return pkTab;
}

void create_account_player_data_from_res(MYSQL_RES * res, account_table * tab)
{
	if (!res)
		return;

	for (uint32_t i = 0; i < mysql_num_rows(res); ++i)
	{
		MYSQL_ROW row = mysql_fetch_row(res);
		int col = 0;

		uint32_t player_id = 0;
		!row[col++] ? 0 : str_to_number(player_id, row[col - 1]);

		if (!player_id)
			continue;

		int j;

		for (j = 0; j < PLAYER_PER_ACCOUNT; ++j)
		{
			if (tab->players[j].id == player_id)
			{
				player_table_cache * pc = client_manager::instance().player_cache(player_id);
				player_table * pt = pc ? pc->get(false) : nullptr;

				if (pt)
				{
					strlcpy(tab->players[j].name, pt->name, sizeof(tab->players[j].name));

					tab->players[j].job			= pt->job;
					tab->players[j].level			= pt->level;
					tab->players[j].play_minutes		= pt->playtime;
					tab->players[j].st			= pt->st;
					tab->players[j].ht			= pt->ht;
					tab->players[j].dx			= pt->dx;
					tab->players[j].iq			= pt->iq;
					tab->players[j].main_part			= pt->parts[PART_MAIN];
					tab->players[j].hair_part			= pt->parts[PART_HAIR];
					tab->players[j].x				= pt->x;
					tab->players[j].y				= pt->y;
					tab->players[j].skill_group		= pt->skill_group;
					tab->players[j].change_name		= 0;
				}
				else
				{
					if (!row[col++])
						*tab->players[j].name = '\0';
					else
						strlcpy(tab->players[j].name, row[col - 1], sizeof(tab->players[j].name));

					tab->players[j].job			= 0;
					tab->players[j].level		= 0;
					tab->players[j].play_minutes	= 0;
					tab->players[j].st			= 0;
					tab->players[j].ht			= 0;
					tab->players[j].dx			= 0;
					tab->players[j].iq			= 0;
					tab->players[j].main_part		= 0;
					tab->players[j].hair_part		= 0;
					tab->players[j].x				= 0;
					tab->players[j].y				= 0;
					tab->players[j].skill_group	= 0;
					tab->players[j].change_name	= 0;

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

void client_manager::result_login(peer * peer, sql_msg * msg)
{
	query_info * qi = (query_info *) msg->user_data;
	client_handle_info * info = (client_handle_info *) qi->pv_data;

	if (info->account_index == 0)
	{
		if (msg->get()->num_rows == 0)
		{
			metin2::log::info("Login: account not found");
			peer->encode_header(HEADER_DG_LOGIN_NOT_EXIST, info->handle, 0);
			delete info;
			return;
		}

		info->account_table = create_account_table_from_res(msg->get()->res);

		if (!info->account_table)
		{
			metin2::log::info("Login: wrong password");
			peer->encode_return(HEADER_DG_LOGIN_WRONG_PASSWD, info->handle);
			delete info;
		}
		else
		{
			++info->account_index;

			char queryStr[512];
			snprintf(queryStr, sizeof(queryStr),
					"SELECT id, name, job, level, playtime, st, ht, dx, iq, part_main, part_hair, x, y, skill_group, change_name FROM player%s WHERE account_id=%u",
					get_table_postfix(), info->account_table->id);

			db_manager::instance().return_query(queryStr, QID_LOGIN, peer->handle(), info);
		}
		return;
	}
	else
	{
		if (!info->account_table)
		{
			peer->encode_return(HEADER_DG_LOGIN_WRONG_PASSWD, info->handle);
			delete info;
			return;
		}

		if (!insert_logon_account(info->account_table->login, peer->handle()))
		{
			metin2::log::info("Login: account {} is already logged in", info->account_table->login);

			packet_dg_login_already p;
			strlcpy(p.login, info->account_table->login, sizeof(p.login));

			peer->encode_header(HEADER_DG_LOGIN_ALREADY, info->handle, sizeof(packet_dg_login_already));
			peer->encode(&p, sizeof(p));
		}
		else
		{
			metin2::log::info("Login: account {} logged in ({:d} players)", info->account_table->login, msg->get()->num_rows);

			if (msg->get()->num_rows > 0)
				create_account_player_data_from_res(msg->get()->res, info->account_table.get());

			login_data * p = get_login_data_by_login(info->account_table->login);
			memcpy(&p->account_ref(), info->account_table.get(), sizeof(account_table));

			peer->encode_header(HEADER_DG_LOGIN_SUCCESS, info->handle, sizeof(account_table));
			peer->encode(info->account_table.get(), sizeof(account_table));

		}

		delete info;
	}
}

void client_manager::query_logout(peer * peer, uint32_t handle_value,const char * data)
{
	logout_packet* packet = (logout_packet*)data;

	if (!*packet->login)
		return;

	login_data * pLoginData = get_login_data_by_login(packet->login);

	if (pLoginData == nullptr)
		return;

	int pid[PLAYER_PER_ACCOUNT];

	for (int n = 0; n < PLAYER_PER_ACCOUNT; ++n)
	{
		if (pLoginData->account_ref().players[n].id == 0)
		{
			if (metin2::config::test_server())
				metin2::log::debug("Logout: account {} has empty slot (pid {:d})", packet->login, pLoginData->account_ref().players[n].id);
			continue;
		}
		
		pid[n] = pLoginData->account_ref().players[n].id;

		if (metin2::config::db().login_log)
			metin2::log::info("Logout: account {}, player pid {:d}", packet->login, pid[n]);

		insert_logout_player(pid[n]);
	}
	
	if (delete_logon_account(packet->login, peer->handle()))
	{
		if (metin2::config::db().login_log)
			metin2::log::info("Logout: account {}", packet->login);
	}
}

void client_manager::query_change_name(peer * peer, uint32_t handle, packet_gd_change_name * p)
{
	std::string query = sql::format(
			"SELECT COUNT(*) as count FROM player{} WHERE name={} AND id <> {}", sql::raw(get_table_postfix()), p->name, p->pid);

	std::unique_ptr<sql_msg> pMsg(db_manager::instance().direct_query(query.c_str(), SQL_PLAYER));

	if (pMsg->get()->num_rows)
	{
		if (!pMsg->get()->res)
		{
			peer->encode_header(HEADER_DG_PLAYER_CREATE_FAILED, handle, 0);
			return;
		}

		MYSQL_ROW row = mysql_fetch_row(pMsg->get()->res);

		if (*row[0] != '0')
		{
			peer->encode_header(HEADER_DG_PLAYER_CREATE_ALREADY, handle, 0);
			return;
		}
	}   
	else
	{
		peer->encode_header(HEADER_DG_PLAYER_CREATE_FAILED, handle, 0);
		return;
	}

	query = sql::format(
			"UPDATE player{} SET name={},change_name=0 WHERE id={}", sql::raw(get_table_postfix()), p->name, p->pid);

	std::unique_ptr<sql_msg> pMsg0(db_manager::instance().direct_query(query.c_str(), SQL_PLAYER));

	packet_dg_change_name pdg;
	peer->encode_header(HEADER_DG_CHANGE_NAME, handle, sizeof(packet_dg_change_name));
	pdg.pid = p->pid;
	strlcpy(pdg.name, p->name, sizeof(pdg.name));
	peer->encode(&pdg, sizeof(packet_dg_change_name));
}
}
