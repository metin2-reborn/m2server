
#include <metin2/db/caches/Cache.h>
#include <metin2/db/clients/ClientManager.h>
#include <metin2/db/storage/DBManager.h>
#include <metin2/db/login/LoginData.h>
#include <metin2/db/app/Main.h>
#include <metin2/db/clients/Peer.h>
#include <metin2/db/clients/QID.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/common/utils.h>
#include <metin2/sql/AsyncSQL.h>
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


bool client_manager::insert_logon_account(const char * c_pszLogin, uint32_t dwHandle)
{
	char szLogin[LOGIN_MAX_LEN + 1];
	trim_and_lower(c_pszLogin, szLogin, sizeof(szLogin));

	auto it = map_k_logon_account_.find(szLogin);

	if (map_k_logon_account_.end() != it)
		return false;

	login_data * pkLD = get_login_data_by_login(c_pszLogin);

	if (!pkLD)
		return false;

	pkLD->set_connected_peer_handle(dwHandle);

	map_k_logon_account_.insert(logon_account_map::value_type(szLogin, pkLD));
	return true;
}

bool client_manager::delete_logon_account(const char * c_pszLogin, uint32_t dwHandle)
{
	char szLogin[LOGIN_MAX_LEN + 1];
	trim_and_lower(c_pszLogin, szLogin, sizeof(szLogin));

	auto it = map_k_logon_account_.find(szLogin);

	if (it == map_k_logon_account_.end())
		return false;

	login_data * pkLD = it->second;

	if (pkLD->connected_peer_handle() != dwHandle)
	{
		metin2::log::error("{} tried to logout in other peer handle {:d}, current handle {:d}", szLogin, dwHandle, pkLD->connected_peer_handle());
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

bool client_manager::find_logon_account(const char * c_pszLogin)
{
	char szLogin[LOGIN_MAX_LEN + 1];
	trim_and_lower(c_pszLogin, szLogin, sizeof(szLogin));

	if (map_k_logon_account_.end() == map_k_logon_account_.find(szLogin))
		return false;

	return true;
}

void client_manager::query_login_by_key(peer * pkPeer, uint32_t dwHandle, packet_gd_login_by_key * p)
{

	login_data * pkLoginData = get_login_data(p->login_key);
	char szLogin[LOGIN_MAX_LEN + 1];
	trim_and_lower(p->login, szLogin, sizeof(szLogin));

	if (!pkLoginData)
	{
		metin2::log::info( "LOGIN_BY_KEY key not exist {} {:d}", szLogin, p->login_key);
		pkPeer->encode_return(HEADER_DG_LOGIN_NOT_EXIST, dwHandle);
		return;
	}

	account_table & r = pkLoginData->account_ref();

	if (find_logon_account(r.login))
	{
		metin2::log::info( "LOGIN_BY_KEY already login {} {:d}", r.login, p->login_key);
		packet_dg_login_already ptog;
		strlcpy(ptog.login, szLogin, sizeof(ptog.login));
		pkPeer->encode_header(HEADER_DG_LOGIN_ALREADY, dwHandle, sizeof(packet_dg_login_already));
		pkPeer->encode(&ptog, sizeof(packet_dg_login_already));
		return;
	}

	if (strcasecmp(r.login, szLogin))
	{
		metin2::log::info( "LOGIN_BY_KEY login differ {} {:d} input {}", r.login, p->login_key, szLogin);
		pkPeer->encode_return(HEADER_DG_LOGIN_NOT_EXIST, dwHandle);
		return;
	}

	if (memcmp(pkLoginData->client_key(), p->client_key, sizeof(uint32_t) * 4))
	{
		const uint32_t * pdwClientKey = pkLoginData->client_key();

		metin2::log::info( "LOGIN_BY_KEY client key differ {} {:d} {:d} {:d} {:d}, {:d} {:d} {:d} {:d}",
				r.login,
				p->client_key[0], p->client_key[1], p->client_key[2], p->client_key[3],
				pdwClientKey[0], pdwClientKey[1], pdwClientKey[2], pdwClientKey[3]);

		pkPeer->encode_return(HEADER_DG_LOGIN_NOT_EXIST, dwHandle);
		return;
	}

	auto pkTab = std::make_unique<account_table>();
	memset(pkTab.get(), 0, sizeof(account_table));

	pkTab->id = r.id;
	trim_and_lower(r.login, pkTab->login, sizeof(pkTab->login));
	strlcpy(pkTab->passwd, r.passwd, sizeof(pkTab->passwd));
	strlcpy(pkTab->social_id, r.social_id, sizeof(pkTab->social_id));
	strlcpy(pkTab->status, "OK", sizeof(pkTab->status));

	client_handle_info * info = new client_handle_info(dwHandle);
	info->account_table = std::move(pkTab);
	strlcpy(info->ip, p->ip, sizeof(info->ip));

	metin2::log::info( "LOGIN_BY_KEY success {} {:d} {}", r.login, p->login_key, info->ip);
	char szQuery[QUERY_MAX_LEN];
	snprintf(szQuery, sizeof(szQuery), "SELECT pid1, pid2, pid3, pid4, empire FROM player_index%s WHERE id=%u", get_table_postfix(), r.id);
	db_manager::instance().return_query(szQuery, QID_LOGIN_BY_KEY, pkPeer->handle(), info);
}

void client_manager::result_login_by_key(peer * peer, ql_msg * msg)
{
	query_info * qi = (query_info *) msg->pv_user_data;
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
		std::unique_ptr<ql_msg> pMsg(db_manager::instance().direct_query(szQuery, SQL_PLAYER));
		
		metin2::log::info( "RESULT_LOGIN_BY_KEY FAIL player_index's NULL : ID:{:d}", account_id);

		if (pMsg->get()->num_rows == 0)
		{
			metin2::log::info( "RESULT_LOGIN_BY_KEY FAIL player_index's NULL : ID:{:d}", account_id);

			// PLAYER_INDEX_CREATE_BUG_FIX
			snprintf(szQuery, sizeof(szQuery), "INSERT INTO player_index%s (id) VALUES(%u)", get_table_postfix(), info->account_table->id);
			db_manager::instance().return_query(szQuery, QID_PLAYER_INDEX_CREATE, peer->handle(), info);
			// END_PLAYER_INDEX_CREATE_BUF_FIX
		}
		return;
	}

	MYSQL_ROW row = mysql_fetch_row(msg->get()->sql_result);

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

// PLAYER_INDEX_CREATE_BUG_FIX
void client_manager::result_player_index_create(peer * pkPeer, ql_msg * msg)
{
	query_info * qi = (query_info *) msg->pv_user_data;
	client_handle_info * info = (client_handle_info *) qi->pv_data;

	char szQuery[QUERY_MAX_LEN];
	snprintf(szQuery, sizeof(szQuery), "SELECT pid1, pid2, pid3, pid4, empire FROM player_index%s WHERE id=%u", get_table_postfix(), 
			info->account_table->id);
	db_manager::instance().return_query(szQuery, QID_LOGIN_BY_KEY, pkPeer->handle(), info);
}
// END_PLAYER_INDEX_CREATE_BUG_FIX

std::unique_ptr<account_table> create_account_table_from_res(MYSQL_RES * res)
{
	char input_pwd[PASSWD_MAX_LEN + 1];
	MYSQL_ROW row = nullptr;
	uint32_t col;

	row = mysql_fetch_row(res);
	col = 0;

	auto pkTab = std::make_unique<account_table>();
	memset(pkTab.get(), 0, sizeof(account_table));

	// Only look at the first column (it's for the JOIN QUERY)
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

void create_account_player_data_from_res(MYSQL_RES * pRes, account_table * pkTab)
{
	if (!pRes)
		return;

	for (uint32_t i = 0; i < mysql_num_rows(pRes); ++i)
	{
		MYSQL_ROW row = mysql_fetch_row(pRes);
		int col = 0;

		uint32_t player_id = 0;
		!row[col++] ? 0 : str_to_number(player_id, row[col - 1]);

		if (!player_id)
			continue;

		int j;

		for (j = 0; j < PLAYER_PER_ACCOUNT; ++j)
		{
			if (pkTab->players[j].id == player_id)
			{
				player_table_cache * pc = client_manager::instance().player_cache(player_id);
				player_table * pt = pc ? pc->get(false) : nullptr;

				if (pt)
				{
					strlcpy(pkTab->players[j].name, pt->name, sizeof(pkTab->players[j].name));

					pkTab->players[j].job			= pt->job;
					pkTab->players[j].level			= pt->level;
					pkTab->players[j].play_minutes		= pt->playtime;
					pkTab->players[j].st			= pt->st;
					pkTab->players[j].ht			= pt->ht;
					pkTab->players[j].dx			= pt->dx;
					pkTab->players[j].iq			= pt->iq;
					pkTab->players[j].main_part			= pt->parts[PART_MAIN];
					pkTab->players[j].hair_part			= pt->parts[PART_HAIR];
					pkTab->players[j].x				= pt->x;
					pkTab->players[j].y				= pt->y;
					pkTab->players[j].skill_group		= pt->skill_group;
					pkTab->players[j].change_name		= 0;
				}
				else
				{
					if (!row[col++])
						*pkTab->players[j].name = '\0';
					else
						strlcpy(pkTab->players[j].name, row[col - 1], sizeof(pkTab->players[j].name));

					pkTab->players[j].job			= 0;
					pkTab->players[j].level		= 0;
					pkTab->players[j].play_minutes	= 0;
					pkTab->players[j].st			= 0;
					pkTab->players[j].ht			= 0;
					pkTab->players[j].dx			= 0;
					pkTab->players[j].iq			= 0;
					pkTab->players[j].main_part		= 0;
					pkTab->players[j].hair_part		= 0;
					pkTab->players[j].x				= 0;
					pkTab->players[j].y				= 0;
					pkTab->players[j].skill_group	= 0;
					pkTab->players[j].change_name	= 0;

					str_to_number(pkTab->players[j].job, row[col++]);
					str_to_number(pkTab->players[j].level, row[col++]);
					str_to_number(pkTab->players[j].play_minutes, row[col++]);
					str_to_number(pkTab->players[j].st, row[col++]);
					str_to_number(pkTab->players[j].ht, row[col++]);
					str_to_number(pkTab->players[j].dx, row[col++]);
					str_to_number(pkTab->players[j].iq, row[col++]);
					str_to_number(pkTab->players[j].main_part, row[col++]);
					str_to_number(pkTab->players[j].hair_part, row[col++]);
					str_to_number(pkTab->players[j].x, row[col++]);
					str_to_number(pkTab->players[j].y, row[col++]);
					str_to_number(pkTab->players[j].skill_group, row[col++]);
					str_to_number(pkTab->players[j].change_name, row[col++]);
				}

				metin2::log::info( "{} {:d} {:d} hair {:d}",
						pkTab->players[j].name, pkTab->players[j].x, pkTab->players[j].y, pkTab->players[j].hair_part);
				break;
			}
		}
		/*
		   if (j == PLAYER_PER_ACCOUNT)
		   metin2::log::error("cannot find player_id on this account (login: {} id {:d} account {:d} {:d} {:d})",
		   pkTab->login, player_id,
		   pkTab->players[0].dwID,
		   pkTab->players[1].dwID,
		   pkTab->players[2].dwID);
		   */
	}
}

void client_manager::result_login(peer * peer, ql_msg * msg)
{
	query_info * qi = (query_info *) msg->pv_user_data;
	client_handle_info * info = (client_handle_info *) qi->pv_data;

	if (info->account_index == 0)
	{
		// No account?
		if (msg->get()->num_rows == 0)
		{
			metin2::log::info( "RESULT_LOGIN: no account");
			peer->encode_header(HEADER_DG_LOGIN_NOT_EXIST, info->handle, 0);
			delete info;
			return;
		}

		info->account_table = create_account_table_from_res(msg->get()->sql_result);

		if (!info->account_table)
		{
			metin2::log::info( "RESULT_LOGIN: no account : WRONG_PASSWD");
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
		if (!info->account_table) // This shouldn't happen, but;;
		{
			peer->encode_return(HEADER_DG_LOGIN_WRONG_PASSWD, info->handle);
			delete info;
			return;
		}

		// If another connection has already logged in.. we must reply that it's already connected.
		if (!insert_logon_account(info->account_table->login, peer->handle()))
		{
			metin2::log::info( "RESULT_LOGIN: already logon {}", info->account_table->login);

			packet_dg_login_already p;
			strlcpy(p.login, info->account_table->login, sizeof(p.login));

			peer->encode_header(HEADER_DG_LOGIN_ALREADY, info->handle, sizeof(packet_dg_login_already));
			peer->encode(&p, sizeof(p));
		}
		else
		{
			metin2::log::info( "RESULT_LOGIN: login success {} rows: {:d}", info->account_table->login, msg->get()->num_rows);

			if (msg->get()->num_rows > 0)
				create_account_player_data_from_res(msg->get()->sql_result, info->account_table.get());

			//PREVENT_COPY_ITEM
			login_data * p = get_login_data_by_login(info->account_table->login);
			memcpy(&p->account_ref(), info->account_table.get(), sizeof(account_table));

			//END_PREVENT_COPY_ITEM
			peer->encode_header(HEADER_DG_LOGIN_SUCCESS, info->handle, sizeof(account_table));
			peer->encode(info->account_table.get(), sizeof(account_table));

		}

		delete info;
	}
}

void client_manager::query_logout(peer * peer, uint32_t dwHandle,const char * data)
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
				metin2::log::info( "LOGOUT {} {:d}", packet->login, pLoginData->account_ref().players[n].id);
			continue;
		}
		
		pid[n] = pLoginData->account_ref().players[n].id;

		if (metin2::config::db().login_log)
			metin2::log::info( "LOGOUT InsertLogoutPlayer {} {:d}", packet->login, pid[n]);

		insert_logout_player(pid[n]);
	}
	
	if (delete_logon_account(packet->login, peer->handle()))
	{
		if (metin2::config::db().login_log)
			metin2::log::info( "LOGOUT {} ", packet->login);
	}
}

void client_manager::query_change_name(peer * peer, uint32_t dwHandle, packet_gd_change_name * p)
{
	std::string query = sql::format(
			"SELECT COUNT(*) as count FROM player{} WHERE name={} AND id <> {}", sql::raw(get_table_postfix()), p->name, p->pid);

	std::unique_ptr<ql_msg> pMsg(db_manager::instance().direct_query(query.c_str(), SQL_PLAYER));

	if (pMsg->get()->num_rows)
	{
		if (!pMsg->get()->sql_result)
		{
			peer->encode_header(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
			return;
		}

		MYSQL_ROW row = mysql_fetch_row(pMsg->get()->sql_result);

		if (*row[0] != '0')
		{
			peer->encode_header(HEADER_DG_PLAYER_CREATE_ALREADY, dwHandle, 0);
			return;
		}
	}   
	else
	{
		peer->encode_header(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
		return;
	}

	query = sql::format(
			"UPDATE player{} SET name={},change_name=0 WHERE id={}", sql::raw(get_table_postfix()), p->name, p->pid);

	std::unique_ptr<ql_msg> pMsg0(db_manager::instance().direct_query(query.c_str(), SQL_PLAYER));

	packet_dg_change_name pdg;
	peer->encode_header(HEADER_DG_CHANGE_NAME, dwHandle, sizeof(packet_dg_change_name));
	pdg.pid = p->pid;
	strlcpy(pdg.name, p->name, sizeof(pdg.name));
	peer->encode(&pdg, sizeof(packet_dg_change_name));
}
}
