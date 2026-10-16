#include <metin2/config/config.h>
#include <metin2/game/persistence/db.h>

#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/app/event.h>
#include <metin2/game/protocol/input.h>
#include <metin2/game/items/item.h>
#include <metin2/game/protocol/login_data.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/text/spam.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/text/locale_service.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/common/utils.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <mysql.h>

#include <cstdarg>
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



db_manager::db_manager() : is_connect_(false)
{
}

db_manager::~db_manager()
{
}

auto db_manager::connect(const char * host, const int port, const char * user, const char * pwd, const char * db) -> bool
{
	if (sql_.setup(host, user, pwd, db, locale.c_str(), false, port))
		is_connect_ = true;

	if (!sql_direct_.setup(host, user, pwd, db, locale.c_str(), true, port))
		metin2::log::error("Cannot open direct sql connection to {}", host);

	if (is_connect_ && !metin2::config::is_auth())
	{
		load_db_string();
	}

	return is_connect_;
}

void db_manager::query(const std::string & query)
{
	sql_.async_query(query.c_str());
}

void db_manager::query(const char * format, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, format);
	vsnprintf(szQuery, sizeof(szQuery), format, args);
	va_end(args);

	sql_.async_query(szQuery);
}

auto db_manager::direct_query(const std::string & query) -> sql_msg *
{
	return sql_direct_.direct_query(query.c_str());
}

auto db_manager::direct_query(const char * format, ...) -> sql_msg *
{
	char szQuery[4096];
	va_list args;

	va_start(args, format);
	vsnprintf(szQuery, sizeof(szQuery), format, args);
	va_end(args);

	return sql_direct_.direct_query(szQuery);
}

auto db_manager::is_connected() -> bool
{
	return is_connect_;
}

void db_manager::return_query(int type, uint32_t ident, void * data, const char * format, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, format);
	vsnprintf(szQuery, sizeof(szQuery), format, args);
	va_end(args);

	return_query(type, ident, data, std::string(szQuery));
}

void db_manager::return_query(int type_value, uint32_t ident_value, void * data, const std::string & query)
{
	return_query_info * p = new return_query_info;

	p->query_type = QUERY_TYPE_RETURN;
	p->type = type_value;
	p->ident = ident_value;
	p->pv_data = data;

	sql_.return_query(query.c_str(), p);
}

auto db_manager::pop_result() -> sql_msg *
{
	sql_msg * p;

	if (sql_.pop_result(&p))
		return p;

	return nullptr;
}

void db_manager::process()
{
	while (std::unique_ptr<sql_msg> pMsg{pop_result()})
	{
		if( nullptr != pMsg->user_data )
		{
			switch( reinterpret_cast<query_info*>(pMsg->user_data)->query_type )
			{
				case QUERY_TYPE_RETURN:
					analyze_return_query(pMsg.get());
					break;

				case QUERY_TYPE_FUNCTION:
					{
						std::unique_ptr<func_query_info> qi(reinterpret_cast<func_query_info*>( pMsg->user_data ));
						qi->f(pMsg.get());
					}
					break;

				case QUERY_TYPE_AFTER_FUNCTION:
					{
						std::unique_ptr<func_after_query_info> qi(reinterpret_cast<func_after_query_info*>( pMsg->user_data ));
						qi->f();
					}
					break;
			}
		}
	}
}

void db_manager::func_query(std::function<void(sql_msg *)> f, const char * format, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, format);
	vsnprintf(szQuery, sizeof(szQuery), format, args);
	va_end(args);

	func_query(std::move(f), std::string(szQuery));
}

void db_manager::func_query(std::function<void(sql_msg *)> f, const std::string & query)
{
	func_query_info * p = new func_query_info;
	p->query_type = QUERY_TYPE_FUNCTION;
	p->f = std::move(f);

	sql_.return_query(query.c_str(), p);
}

void db_manager::func_after_query(std::function<void()> f, const char * format, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, format);
	vsnprintf(szQuery, sizeof(szQuery), format, args);
	va_end(args);

	func_after_query(std::move(f), std::string(szQuery));
}

void db_manager::func_after_query(std::function<void()> f, const std::string & query)
{
	func_after_query_info * p = new func_after_query_info;
	p->query_type = QUERY_TYPE_AFTER_FUNCTION;
	p->f = std::move(f);

	sql_.return_query(query.c_str(), p);
}

auto db_manager::get_login_data(uint32_t key) -> login_data *
{
	auto it = map_pk_login_data_.find(key);

	if (it == map_pk_login_data_.end())
		return nullptr;

	return it->second.get();
}

void db_manager::insert_login_data(std::unique_ptr<login_data> ld)
{
	const uint32_t dwKey = ld->key();
	map_pk_login_data_.insert(std::make_pair(dwKey, std::move(ld)));
}

void db_manager::delete_login_data(login_data * ld)
{
	auto it = map_pk_login_data_.find(ld->key());

	if (it == map_pk_login_data_.end())
		return;

	metin2::log::debug( "Login data of account {} released ({})", ld->login(), static_cast<const void *>(ld));

	map_pk_login_data_.erase(it);
}

void db_manager::send_auth_login(desc * d)
{
	const account_table & r = d->get_account_table();

	login_data * pkLD = get_login_data(d->get_login_key());

	if (!pkLD)
		return;

	packet_gd_auth_login ptod;
	ptod.id = r.id;
	
	trim_and_lower(r.login, ptod.login, sizeof(ptod.login));
	strlcpy(ptod.social_id, r.social_id, sizeof(ptod.social_id));
	ptod.login_key = d->get_login_key();

	std::memcpy(ptod.premium_times, pkLD->premium_ptr(), sizeof(ptod.premium_times));
	std::memcpy(&ptod.client_key, pkLD->client_key(), sizeof(uint32_t) * 4);

	db_clientdesc->db_packet(HEADER_GD_AUTH_LOGIN, d->handle(), &ptod, sizeof(packet_gd_auth_login));
	metin2::log::debug( "Auth login sent to db: account {} (id {:d})", ptod.login, ptod.id);

	delete_login_data(pkLD);
}

void db_manager::login_prepare(desc * d, uint32_t * client_key, int * pai_premium_times)
{
	const account_table & r = d->get_account_table();

	auto pkLD = std::make_unique<login_data>();

	pkLD->set_key(d->get_login_key());
	pkLD->set_login(r.login);
	pkLD->set_client_key(client_key);

	if (pai_premium_times)
		pkLD->set_premium(pai_premium_times);

	insert_login_data(std::move(pkLD));

	send_auth_login(d);
}

void db_manager::analyze_return_query(sql_msg * msg_value)
{
	return_query_info * qi = (return_query_info *) msg_value->user_data;

	switch (qi->type)
	{
		case QID_AUTH_LOGIN:
			{
				packet_cg_login3 * pinfo = (packet_cg_login3 *) qi->pv_data;
				desc * d = desc_manager::instance().find_by_login_key(qi->ident);

				if (!d)
				{
					delete pinfo;
					break;
				}
				d->set_login(pinfo->login);

				metin2::log::debug( "Auth login query result: key {:d}, connection {}", qi->ident, static_cast<const void *>(d));

				if (msg_value->get()->num_rows == 0)
				{
					metin2::log::info( "Auth login refused: account {} not found", pinfo->login);
					login_failure(d, "NOID");
					delete pinfo;
				}
				else
				{
					MYSQL_ROW row = mysql_fetch_row(msg_value->get()->res);
					int col = 0;

					char szEncrytPassword[45 + 1];
					char szPassword[45 + 1];
					char szSocialID[SOCIAL_ID_MAX_LEN + 1];
					char szStatus[ACCOUNT_STATUS_MAX_LEN + 1];
					uint32_t dwID = 0;

					if (!row[col]) 
					{ 
						metin2::log::error("Auth login: column {:d} is null for account {}", col, pinfo->login);
						delete pinfo;
					   	break; 
					}
					
					strlcpy(szEncrytPassword, row[col++], sizeof(szEncrytPassword));

					if (!row[col]) 
					{
					   	metin2::log::error("Auth login: column {:d} is null for account {}", col, pinfo->login);
						delete pinfo;
					   	break;
				   	}
				
					strlcpy(szPassword, row[col++], sizeof(szPassword));

					if (!row[col])
				   	{ 
						metin2::log::error("Auth login: column {:d} is null for account {}", col, pinfo->login); 
						delete pinfo;
						break;
				   	}

					strlcpy(szSocialID, row[col++], sizeof(szSocialID));

					if (!row[col])
				   	{
					   	metin2::log::error("Auth login: column {:d} is null for account {}", col, pinfo->login);
						delete pinfo;
					   	break;
				   	}
				
					str_to_number(dwID, row[col++]);
					
					if (!row[col]) 
					{
					   	metin2::log::error("Auth login: column {:d} is null for account {}", col, pinfo->login); 
						delete pinfo;
						break;
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
					struct tm * tm1;
					tm1 = localtime(&create_time);
					strftime(szCreateDate, 255, "%Y%m%d", tm1);

					metin2::log::debug( "Auth login: account created at {:d} ({})", retValue, szCreateDate);
					metin2::log::debug( "Auth login: creation date vs block date: {:d}", strncmp(szCreateDate, metin2::config::auth().block_login_date.c_str(), 8));

					int nPasswordDiff = strcmp(szEncrytPassword, szPassword);


					if (nPasswordDiff)
					{
						login_failure(d, "WRONGPWD");
						metin2::log::info( "Auth login refused: wrong password for account {}", pinfo->login);
						delete pinfo;
					}
					else if (bNotAvail)
					{
						login_failure(d, "NOTAVAIL");
						metin2::log::info( "Auth login refused: account {} not available", pinfo->login);
						delete pinfo;
					}
					else if (desc_manager::instance().find_by_login_name(pinfo->login))
					{
						login_failure(d, "ALREADY");
						metin2::log::info( "Auth login refused: account {} already logged in", pinfo->login);
						delete pinfo;
					}
					else if (strcmp(szStatus, "OK"))
					{
						login_failure(d, szStatus);
						metin2::log::info( "Auth login refused: account {} has status {}", pinfo->login, szStatus);
						delete pinfo;
					}
					else
					{
						if (strncmp(szCreateDate, metin2::config::auth().block_login_date.c_str(), 8) >= 0)
						{
							login_failure(d, "BLKLOGIN");
							metin2::log::info( "Auth login refused: account {} created after the login block date", pinfo->login);
							delete pinfo;
							break;
						}

						std::unique_ptr<sql_msg> msg( db_manager::instance().direct_query(sql::format("UPDATE account SET last_play=NOW() WHERE id={}", dwID)) );

						account_table & r = d->get_account_table();

						r.id = dwID;
						trim_and_lower(pinfo->login, r.login, sizeof(r.login));
						strlcpy(r.passwd, pinfo->passwd, sizeof(r.passwd));
						strlcpy(r.social_id, szSocialID, sizeof(r.social_id));
						desc_manager::instance().connect_account(r.login, d);


						login_prepare(d, pinfo->client_key, aiPremiumTimes);
						metin2::log::info( "Auth login: account {} accepted", pinfo->login);
						delete pinfo;
					}
				}
			}
			break;
		case QID_SAFEBOX_SIZE:
			{
				character * ch = character_manager::instance().find_by_pid(qi->ident);

				if (ch)
				{
					if (msg_value->get()->num_rows > 0)
					{
						MYSQL_ROW row = mysql_fetch_row(msg_value->get()->res);
						int	size = 0;
						str_to_number(size, row[0]);
						ch->set_safebox_size(SAFEBOX_PAGE_SIZE * size);
					}
				}
			}
			break;

		case QID_DB_STRING:
			{
				map_dbstring_.clear();
				vec_greet_message_.clear();

				for (uint i = 0; i < msg_value->get()->num_rows; ++i)
				{
					MYSQL_ROW row = mysql_fetch_row(msg_value->get()->res);
					if (row[0] && row[1])
					{
						map_dbstring_.insert(make_pair(std::string(row[0]), std::string(row[1])));
						metin2::log::debug( "Db string {} = \"{}\"", row[0], row[1]);
					}
				}
				if (map_dbstring_.find("GREET") != map_dbstring_.end())
				{
					std::istringstream is(map_dbstring_["GREET"]);
					while (!is.eof())
					{
						std::string str;
						getline(is, str);
						vec_greet_message_.push_back(str);
					}
				}
			}
			break;

		case QID_LOTTO:
			{
				character * ch = character_manager::instance().find_by_pid(qi->ident);
				uint32_t * pdw = (uint32_t *) qi->pv_data;

				if (ch)
				{
					if (msg_value->get()->affected_rows == 0 || msg_value->get()->affected_rows == (uint32_t)-1)
					{
						metin2::log::warn( "Lotto: cannot record ticket for pid {:d}", ch->player_id());
					}
					else
					{
						item * pkItem = ch->auto_give_item(pdw[0], pdw[1]);

						if (pkItem)
						{
							metin2::log::info( "Lotto: ticket given to player {} (pid {:d})", ch->name(), qi->ident);

							pkItem->set_socket(0, msg_value->get()->insert_id);
							pkItem->set_socket(1, pdw[2]);
						}
						else
							metin2::log::warn( "Lotto: cannot give ticket item to pid {:d}", ch->player_id());
					}
				}

				delete[] pdw;
			}
			break;

		case QID_HIGHSCORE_REGISTER:
			{
				highscore_register_query_info * info = (highscore_register_query_info *) qi->pv_data;
				bool bQuery = true;

				if (msg_value->get()->num_rows)
				{
					MYSQL_ROW row = mysql_fetch_row(msg_value->get()->res);

					if (row && row[0])
					{
						int iCur = 0;
						str_to_number(iCur, row[0]);

						if ((info->order && iCur >= info->value) ||
								(!info->order && iCur <= info->value))
							bQuery = false;
					}
				}

				if (bQuery)
					query(sql::format("REPLACE INTO highscore{} VALUES({}, {}, {})",
							sql::raw(get_table_postfix()), info->board, info->pid, info->value));

				delete info;
			}
			break;

		case QID_HIGHSCORE_SHOW:
			{
			}
			break;

		case QID_BLOCK_CHAT_LIST:
			{
				character * ch = character_manager::instance().find_by_pid(qi->ident);
				
				if (ch == nullptr)
					break;
				if (msg_value->get()->num_rows)
				{
					MYSQL_ROW row;
					while ((row = mysql_fetch_row(msg_value->get()->res)))
					{
						ch->chat_packet(CHAT_TYPE_INFO, "%s %s sec", row[0], row[1]);
					}
				}
				else
				{
					ch->chat_packet(CHAT_TYPE_INFO, "No one currently blocked.");
				}
			}
			break;


	}

	delete qi;
}

void db_manager::load_db_string()
{
	return_query(QID_DB_STRING, 0, nullptr, sql::format("SELECT name, text FROM string{}", sql::raw(get_table_postfix())));
}

auto db_manager::greet_message() -> const std::vector<std::string>&
{
	return vec_greet_message_;
}

void db_manager::send_money_log(uint8_t type, uint32_t vnum, int gold)
{
	if (!gold)
		return;
	packet_money_log p;
	p.type = type;
	p.vnum = vnum;
	p.gold = gold;
	db_clientdesc->db_packet(HEADER_GD_MONEY_LOG, 0, &p, sizeof(p));
}

auto db_manager::escape_string(char* dst, size_t dst_size, const char *src, size_t src_size) -> size_t
{
	return sql_direct_.escape_string(dst, dst_size, src, src_size);
}

account_db::account_db() :
	is_connect_(false)
{
}

auto account_db::is_connected() -> bool
{
	return is_connect_;
}

auto account_db::connect(const char * host, const int port, const char * user, const char * pwd, const char * db) -> bool
{
	is_connect_ = sql_direct_.setup(host, user, pwd, db, "", true, port);

	if (false == is_connect_)
	{
		fprintf(stderr, "cannot open direct sql connection to host: %s user: %s db: %s\n", host, user, db);
		return false;
	}

	return is_connect_;
}

auto account_db::connect_async(const char * host, const int port, const char * user, const char * pwd, const char * db, const char * locale) -> bool
{
	sql_.setup(host, user, pwd, db, locale, false, port);
	return true;
}

void account_db::set_locale(const std::string & locale)
{
	sql_direct_.set_locale(locale);
	sql_direct_.query_locale_set();
}

auto account_db::direct_query(const char * query) -> sql_msg*
{
	return sql_direct_.direct_query(query);
}

void account_db::return_query(int type, uint32_t ident, void * data, const char * format, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, format);
	vsnprintf(szQuery, sizeof(szQuery), format, args);
	va_end(args);

	return_query(type, ident, data, std::string(szQuery));
}

void account_db::return_query(int type_value, uint32_t ident_value, void * data, const std::string & query)
{
	return_query_info * p = new return_query_info;

	p->query_type = QUERY_TYPE_RETURN;
	p->type = type_value;
	p->ident = ident_value;
	p->pv_data = data;

	sql_.return_query(query.c_str(), p);
}

auto account_db::pop_result() -> sql_msg *
{
	sql_msg * p;

	if (sql_.pop_result(&p))
		return p;

	return nullptr;
}

void account_db::process()
{
	while (std::unique_ptr<sql_msg> pMsg{pop_result()})
	{
		query_info* qi = (query_info *) pMsg->user_data;

		switch (qi->query_type)
		{
			case QUERY_TYPE_RETURN:
				analyze_return_query(pMsg.get());
				break;
		}
	}
}


enum account_qid
{
	QID_SPAM_DB,
};

static event_ptr reload_spam_event_handle = nullptr;

EVENTINFO(reload_spam_event_info)
{
	uint32_t empty;
};

EVENTFUNC(reload_spam_event)
{
	account_db::instance().return_query(QID_SPAM_DB, 0, nullptr, "SELECT word, score FROM spam_db WHERE type='SPAM'");
	return passes_per_sec(metin2::config::game().spam.reload_cycle);
}

void load_spam_db()
{
	account_db::instance().return_query(QID_SPAM_DB, 0, nullptr, "SELECT word, score FROM spam_db WHERE type='SPAM'");

	if (nullptr == reload_spam_event_handle)
	{
		reload_spam_event_info* info = alloc_event_info<reload_spam_event_info>();
		reload_spam_event_handle = event_create(reload_spam_event, info, passes_per_sec(metin2::config::game().spam.reload_cycle));
	}
}

void cancel_reload_spam_event() {
	reload_spam_event_handle = nullptr;
}

void account_db::analyze_return_query(sql_msg * msg)
{
	return_query_info * qi = (return_query_info *) msg->user_data;

	switch (qi->type)
	{
		case QID_SPAM_DB:
			{
				if (msg->get()->num_rows > 0)
				{
					MYSQL_ROW row;

					spam_manager::instance().clear();

					while ((row = mysql_fetch_row(msg->get()->res)))
						spam_manager::instance().insert(row[0], atoi(row[1]));
				}
			}
			break;
	}

	delete qi;
}
}
