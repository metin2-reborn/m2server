#pragma once

#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>
#include <metin2/sql/AsyncSQL.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace metin2::game::inline protocol {
    class login_data;
}

namespace metin2::game::inline persistence {

enum
{
	QUERY_TYPE_RETURN = 1,
	QUERY_TYPE_FUNCTION = 2,
	QUERY_TYPE_AFTER_FUNCTION = 3,
};

enum
{
	QID_SAFEBOX_SIZE,
	QID_DB_STRING,
	QID_AUTH_LOGIN,
	QID_LOTTO,
	QID_HIGHSCORE_REGISTER,
	QID_HIGHSCORE_SHOW,

	// BLOCK_CHAT
	QID_BLOCK_CHAT_LIST,
	// END_OF_BLOCK_CHAT

	// PROTECT_CHILD_FOR_NEWCIBN
	QID_PROTECT_CHILD,
	// END_PROTECT_CHILD_FOR_NEWCIBN

};

class query_info
{
	public:
		int	query_type;
};

class return_query_info : public query_info
{
	public:
		int	type;
		uint32_t	ident;
		void			*	pv_data;
};

class func_query_info : public query_info
{
	public:
		std::function<void(ql_msg *)> f;
};

class func_after_query_info : public query_info
{
	public:
		std::function<void()> f;
};


class db_manager : public singleton<db_manager>
{
	public:
		db_manager();
		~db_manager() override;

		bool			is_connected();

		bool			connect(const char * host, const int port, const char * user, const char * pwd, const char * db);
		// The std::string overloads take a finished statement (see sql::format); the others format
		// it printf-style first.
		void			query(const std::string & query);
		void			query(const char * c_pszFormat, ...) __attribute__((format(printf, 2, 3)));

		ql_msg *		direct_query(const std::string & query);
		ql_msg *		direct_query(const char * c_pszFormat, ...) __attribute__((format(printf, 2, 3)));
		void			return_query(int iType, uint32_t dwIdent, void* pvData, const std::string & query);
		void			return_query(int iType, uint32_t dwIdent, void* pvData, const char * c_pszFormat, ...) __attribute__((format(printf, 5, 6)));

		void			process();
		void			analyze_return_query(ql_msg * pmsg);

		void			send_money_log(uint8_t type, uint32_t vnum, int gold);

		void			login_prepare(desc * d, uint32_t * pdwClientKey, int * paiPremiumTimes);
		void			send_auth_login(desc * d);

		void			insert_login_data(std::unique_ptr<login_data> pkLD);
		void			delete_login_data(login_data * pkLD);
		login_data *		get_login_data(uint32_t dwKey);

		uint32_t			count_query()		{ return sql_.count_query(); }

		// BLOCK EXCEPTION
		// BLOCK EXCEPTION

		void			load_db_string();
		const std::vector<std::string> & greet_message();

		// Runs the query and calls f with its result (freed afterwards) on the main thread.
		void			func_query(std::function<void(ql_msg *)> f, const std::string & query);
		void			func_query(std::function<void(ql_msg *)> f, const char * c_pszFormat, ...) __attribute__((format(printf, 3, 4)));
		// Runs the query and calls f once it has completed.
		void			func_after_query(std::function<void()> f, const std::string & query);
		void			func_after_query(std::function<void()> f, const char * c_pszFormat, ...) __attribute__((format(printf, 3, 4)));

		size_t escape_string(char* dst, size_t dstSize, const char *src, size_t srcSize);

	private:
		ql_msg *				pop_result();

		async_sql				sql_;
		async_sql				sql_direct_;
		bool					is_connect_;

		std::map<std::string, std::string>	map_dbstring_;
		std::vector<std::string>		vec_greet_message_;
		std::map<uint32_t, std::unique_ptr<login_data>>		map_pk_login_data_;
};

////////////////////////////////////////////////////////////////
struct highscore_register_query_info {
	char    board[20+1]; 
	uint32_t   pid;
	int     value;
	bool    order;
};

// ACCOUNT_DB
class account_db : public singleton<account_db>
{
	public:
		account_db();

		bool is_connected();
		bool connect(const char * host, const int port, const char * user, const char * pwd, const char * db);
		bool connect_async(const char * host, const int port, const char * user, const char * pwd, const char * db, const char * locale);

		ql_msg* direct_query(const char * query);
		ql_msg* direct_query(const std::string & query) { return direct_query(query.c_str()); }
		void return_query(int iType, uint32_t dwIdent, void * pvData, const std::string & query);
		void return_query(int iType, uint32_t dwIdent, void * pvData, const char * c_pszFormat, ...) __attribute__((format(printf, 5, 6)));

		void set_locale(const std::string & stLocale);

		void process();

	private:
		ql_msg * pop_result();
		void analyze_return_query(ql_msg * pMsg);

		async_sql	sql_direct_;
		async_sql	sql_;
		bool		is_connect_;

};
//END_ACCOUNT_DB

extern void cancel_reload_spam_event();
extern void load_spam_db();
}
