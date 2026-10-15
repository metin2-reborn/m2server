#pragma once

#include "typedef.h"

#include <common/singleton.h>
#include <libsql/AsyncSQL.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

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

class CQueryInfo
{
	public:
		int	iQueryType;
};

class CReturnQueryInfo : public CQueryInfo
{
	public:
		int	iType;
		uint32_t	dwIdent;
		void			*	pvData;
};

class CFuncQueryInfo : public CQueryInfo
{
	public:
		std::function<void(SQLMsg *)> f;
};

class CFuncAfterQueryInfo : public CQueryInfo
{
	public:
		std::function<void()> f;
};

class CLoginData;

class DBManager : public singleton<DBManager>
{
	public:
		DBManager();
		~DBManager() override;

		bool			IsConnected();

		bool			Connect(const char * host, const int port, const char * user, const char * pwd, const char * db);
		// The std::string overloads take a finished statement (see sql::format); the others format
		// it printf-style first.
		void			Query(const std::string & query);
		void			Query(const char * c_pszFormat, ...) __attribute__((format(printf, 2, 3)));

		SQLMsg *		DirectQuery(const std::string & query);
		SQLMsg *		DirectQuery(const char * c_pszFormat, ...) __attribute__((format(printf, 2, 3)));
		void			ReturnQuery(int iType, uint32_t dwIdent, void* pvData, const std::string & query);
		void			ReturnQuery(int iType, uint32_t dwIdent, void* pvData, const char * c_pszFormat, ...) __attribute__((format(printf, 5, 6)));

		void			Process();
		void			AnalyzeReturnQuery(SQLMsg * pmsg);

		void			SendMoneyLog(uint8_t type, uint32_t vnum, int gold);

		void			LoginPrepare(DESC * d, uint32_t * pdwClientKey, int * paiPremiumTimes);
		void			SendAuthLogin(DESC * d);

		void			InsertLoginData(std::unique_ptr<CLoginData> pkLD);
		void			DeleteLoginData(CLoginData * pkLD);
		CLoginData *		GetLoginData(uint32_t dwKey);

		uint32_t			CountQuery()		{ return m_sql.CountQuery(); }

		// BLOCK EXCEPTION
		// BLOCK EXCEPTION

		void			LoadDBString();
		const std::vector<std::string> & GetGreetMessage();

		// Runs the query and calls f with its result (freed afterwards) on the main thread.
		void			FuncQuery(std::function<void(SQLMsg *)> f, const std::string & query);
		void			FuncQuery(std::function<void(SQLMsg *)> f, const char * c_pszFormat, ...) __attribute__((format(printf, 3, 4)));
		// Runs the query and calls f once it has completed.
		void			FuncAfterQuery(std::function<void()> f, const std::string & query);
		void			FuncAfterQuery(std::function<void()> f, const char * c_pszFormat, ...) __attribute__((format(printf, 3, 4)));

		size_t EscapeString(char* dst, size_t dstSize, const char *src, size_t srcSize);

	private:
		SQLMsg *				PopResult();

		CAsyncSQL				m_sql;
		CAsyncSQL				m_sql_direct;
		bool					m_bIsConnect;

		std::map<std::string, std::string>	m_map_dbstring;
		std::vector<std::string>		m_vec_GreetMessage;
		std::map<uint32_t, std::unique_ptr<CLoginData>>		m_map_pkLoginData;
};

////////////////////////////////////////////////////////////////
struct THighscoreRegisterQueryInfo {
	char    szBoard[20+1]; 
	uint32_t   dwPID;
	int     iValue;
	bool    bOrder;
};

// ACCOUNT_DB
class AccountDB : public singleton<AccountDB>
{
	public:
		AccountDB();

		bool IsConnected();
		bool Connect(const char * host, const int port, const char * user, const char * pwd, const char * db);
		bool ConnectAsync(const char * host, const int port, const char * user, const char * pwd, const char * db, const char * locale);

		SQLMsg* DirectQuery(const char * query);
		SQLMsg* DirectQuery(const std::string & query) { return DirectQuery(query.c_str()); }
		void ReturnQuery(int iType, uint32_t dwIdent, void * pvData, const std::string & query);
		void ReturnQuery(int iType, uint32_t dwIdent, void * pvData, const char * c_pszFormat, ...) __attribute__((format(printf, 5, 6)));

		void SetLocale(const std::string & stLocale);

		void Process();

	private:
		SQLMsg * PopResult();
		void AnalyzeReturnQuery(SQLMsg * pMsg);

		CAsyncSQL	m_sql_direct;
		CAsyncSQL	m_sql;
		bool		m_IsConnect;

};
//END_ACCOUNT_DB

extern void CancelReloadSpamEvent();
extern void LoadSpamDB();
