#include "db.h"

#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "event.h"
#include "input.h"
#include "item.h"
#include "login_data.h"
#include "packet.h"
#include "spam.h"
#include "typedef.h"

#include <common/length.h>
#include <common/tables.h>
#include <common/utils.h>
#include <libsql/AsyncSQL.h>
#include <libsql/query.h>
#include <metin2/log/log.h>
#include <libthecore/utils.h>

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



DBManager::DBManager() : m_bIsConnect(false)
{
}

DBManager::~DBManager()
{
}

bool DBManager::Connect(const char * host, const int port, const char * user, const char * pwd, const char * db)
{
	if (m_sql.Setup(host, user, pwd, db, g_stLocale.c_str(), false, port))
		m_bIsConnect = true;

	if (!m_sql_direct.Setup(host, user, pwd, db, g_stLocale.c_str(), true, port))
		metin2::log::error("cannot open direct sql connection to host {}", host);

	if (m_bIsConnect && !g_bAuthServer)
	{
		LoadDBString();
	}

	return m_bIsConnect;
}

void DBManager::Query(const std::string & query)
{
	m_sql.AsyncQuery(query.c_str());
}

void DBManager::Query(const char * c_pszFormat, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, c_pszFormat);
	vsnprintf(szQuery, sizeof(szQuery), c_pszFormat, args);
	va_end(args);

	m_sql.AsyncQuery(szQuery);
}

SQLMsg * DBManager::DirectQuery(const std::string & query)
{
	return m_sql_direct.DirectQuery(query.c_str());
}

SQLMsg * DBManager::DirectQuery(const char * c_pszFormat, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, c_pszFormat);
	vsnprintf(szQuery, sizeof(szQuery), c_pszFormat, args);
	va_end(args);

	return m_sql_direct.DirectQuery(szQuery);
}

bool DBManager::IsConnected()
{
	return m_bIsConnect;
}

void DBManager::ReturnQuery(int iType, uint32_t dwIdent, void * pvData, const char * c_pszFormat, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, c_pszFormat);
	vsnprintf(szQuery, sizeof(szQuery), c_pszFormat, args);
	va_end(args);

	ReturnQuery(iType, dwIdent, pvData, std::string(szQuery));
}

void DBManager::ReturnQuery(int iType, uint32_t dwIdent, void * pvData, const std::string & query)
{
	CReturnQueryInfo * p = new CReturnQueryInfo;

	p->iQueryType = QUERY_TYPE_RETURN;
	p->iType = iType;
	p->dwIdent = dwIdent;
	p->pvData = pvData;

	m_sql.ReturnQuery(query.c_str(), p);
}

SQLMsg * DBManager::PopResult()
{
	SQLMsg * p;

	if (m_sql.PopResult(&p))
		return p;

	return nullptr;
}

void DBManager::Process()
{
	while (std::unique_ptr<SQLMsg> pMsg{PopResult()})
	{
		if( nullptr != pMsg->pvUserData )
		{
			switch( reinterpret_cast<CQueryInfo*>(pMsg->pvUserData)->iQueryType )
			{
				case QUERY_TYPE_RETURN:
					AnalyzeReturnQuery(pMsg.get());
					break;

				case QUERY_TYPE_FUNCTION:
					{
						std::unique_ptr<CFuncQueryInfo> qi(reinterpret_cast<CFuncQueryInfo*>( pMsg->pvUserData ));
						qi->f(pMsg.get());
					}
					break;

				case QUERY_TYPE_AFTER_FUNCTION:
					{
						std::unique_ptr<CFuncAfterQueryInfo> qi(reinterpret_cast<CFuncAfterQueryInfo*>( pMsg->pvUserData ));
						qi->f();
					}
					break;
			}
		}
	}
}

void DBManager::FuncQuery(std::function<void(SQLMsg *)> f, const char * c_pszFormat, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, c_pszFormat);
	vsnprintf(szQuery, sizeof(szQuery), c_pszFormat, args);
	va_end(args);

	FuncQuery(std::move(f), std::string(szQuery));
}

void DBManager::FuncQuery(std::function<void(SQLMsg *)> f, const std::string & query)
{
	CFuncQueryInfo * p = new CFuncQueryInfo;
	p->iQueryType = QUERY_TYPE_FUNCTION;
	p->f = std::move(f);

	m_sql.ReturnQuery(query.c_str(), p);
}

void DBManager::FuncAfterQuery(std::function<void()> f, const char * c_pszFormat, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, c_pszFormat);
	vsnprintf(szQuery, sizeof(szQuery), c_pszFormat, args);
	va_end(args);

	FuncAfterQuery(std::move(f), std::string(szQuery));
}

void DBManager::FuncAfterQuery(std::function<void()> f, const std::string & query)
{
	CFuncAfterQueryInfo * p = new CFuncAfterQueryInfo;
	p->iQueryType = QUERY_TYPE_AFTER_FUNCTION;
	p->f = std::move(f);

	m_sql.ReturnQuery(query.c_str(), p);
}

CLoginData * DBManager::GetLoginData(uint32_t dwKey)
{
	auto it = m_map_pkLoginData.find(dwKey);

	if (it == m_map_pkLoginData.end())
		return nullptr;

	return it->second.get();
}

void DBManager::InsertLoginData(std::unique_ptr<CLoginData> pkLD)
{
	const uint32_t dwKey = pkLD->GetKey();
	m_map_pkLoginData.insert(std::make_pair(dwKey, std::move(pkLD)));
}

void DBManager::DeleteLoginData(CLoginData * pkLD)
{
	auto it = m_map_pkLoginData.find(pkLD->GetKey());

	if (it == m_map_pkLoginData.end())
		return;

	metin2::log::info( "DeleteLoginData {} {}", pkLD->GetLogin(), static_cast<const void *>(pkLD));

	m_map_pkLoginData.erase(it);
}

void DBManager::SendAuthLogin(DESC * d)
{
	const TAccountTable & r = d->GetAccountTable();

	CLoginData * pkLD = GetLoginData(d->GetLoginKey());

	if (!pkLD)
		return;

	TPacketGDAuthLogin ptod;
	ptod.dwID = r.id;
	
	trim_and_lower(r.login, ptod.szLogin, sizeof(ptod.szLogin));
	strlcpy(ptod.szSocialID, r.social_id, sizeof(ptod.szSocialID));
	ptod.dwLoginKey = d->GetLoginKey();

	std::memcpy(ptod.iPremiumTimes, pkLD->GetPremiumPtr(), sizeof(ptod.iPremiumTimes));
	std::memcpy(&ptod.adwClientKey, pkLD->GetClientKey(), sizeof(uint32_t) * 4);

	db_clientdesc->DBPacket(HEADER_GD_AUTH_LOGIN, d->GetHandle(), &ptod, sizeof(TPacketGDAuthLogin));
	metin2::log::info( "SendAuthLogin {} key {:d}", ptod.szLogin, ptod.dwID);

	// The db keeps the login from here on.
	DeleteLoginData(pkLD);
}

void DBManager::LoginPrepare(DESC * d, uint32_t * pdwClientKey, int * paiPremiumTimes)
{
	const TAccountTable & r = d->GetAccountTable();

	auto pkLD = std::make_unique<CLoginData>();

	pkLD->SetKey(d->GetLoginKey());
	pkLD->SetLogin(r.login);
	pkLD->SetClientKey(pdwClientKey);

	if (paiPremiumTimes)
		pkLD->SetPremium(paiPremiumTimes);

	InsertLoginData(std::move(pkLD));

	SendAuthLogin(d);
}

void DBManager::AnalyzeReturnQuery(SQLMsg * pMsg)
{
	CReturnQueryInfo * qi = (CReturnQueryInfo *) pMsg->pvUserData;

	switch (qi->iType)
	{
		case QID_AUTH_LOGIN:
			{
				TPacketCGLogin3 * pinfo = (TPacketCGLogin3 *) qi->pvData;
				DESC * d = DESC_MANAGER::instance().FindByLoginKey(qi->dwIdent);

				if (!d)
				{
					delete pinfo;
					break;
				}
				//Moved - By SeMinZ
				d->SetLogin(pinfo->login);

				metin2::log::info( "QID_AUTH_LOGIN: START {:d} {}", qi->dwIdent, static_cast<const void *>(d));

				if (pMsg->Get()->uiNumRows == 0)
				{
					metin2::log::info( "   NOID");
					LoginFailure(d, "NOID");
					delete pinfo;
				}
				else
				{
					MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
					int col = 0;

					// PASSWORD('%s'), password, social_id, id, status
					char szEncrytPassword[45 + 1];
					char szPassword[45 + 1];
					char szSocialID[SOCIAL_ID_MAX_LEN + 1];
					char szStatus[ACCOUNT_STATUS_MAX_LEN + 1];
					uint32_t dwID = 0;

					if (!row[col]) 
					{ 
						metin2::log::error("error column {:d}", col);
						delete pinfo;
					   	break; 
					}
					
					strlcpy(szEncrytPassword, row[col++], sizeof(szEncrytPassword));

					if (!row[col]) 
					{
					   	metin2::log::error("error column {:d}", col);
						delete pinfo;
					   	break;
				   	}
				
					strlcpy(szPassword, row[col++], sizeof(szPassword));

					if (!row[col])
				   	{ 
						metin2::log::error("error column {:d}", col); 
						delete pinfo;
						break;
				   	}

					strlcpy(szSocialID, row[col++], sizeof(szSocialID));

					if (!row[col])
				   	{
					   	metin2::log::error("error column {:d}", col);
						delete pinfo;
					   	break;
				   	}
				
					str_to_number(dwID, row[col++]);
					
					if (!row[col]) 
					{
					   	metin2::log::error("error column {:d}", col); 
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

					metin2::log::info( "Create_Time {:d} {}", retValue, szCreateDate);
					metin2::log::info( "Block Time {:d} ", strncmp(szCreateDate, g_stBlockDate.c_str(), 8));

					int nPasswordDiff = strcmp(szEncrytPassword, szPassword);


					if (nPasswordDiff)
					{
						LoginFailure(d, "WRONGPWD");
						metin2::log::info( "   WRONGPWD");
						delete pinfo;
					}
					else if (bNotAvail)
					{
						LoginFailure(d, "NOTAVAIL");
						metin2::log::info( "   NOTAVAIL");
						delete pinfo;
					}
					else if (DESC_MANAGER::instance().FindByLoginName(pinfo->login))
					{
						LoginFailure(d, "ALREADY");
						metin2::log::info( "   ALREADY");
						delete pinfo;
					}
					else if (strcmp(szStatus, "OK"))
					{
						LoginFailure(d, szStatus);
						metin2::log::info( "   STATUS: {}", szStatus);
						delete pinfo;
					}
					else
					{
						//stBlockData >= 0 == date is later than BlockDate 
						if (strncmp(szCreateDate, g_stBlockDate.c_str(), 8) >= 0)
						{
							LoginFailure(d, "BLKLOGIN");
							metin2::log::info( "   BLKLOGIN");
							delete pinfo;
							break;
						}

						std::unique_ptr<SQLMsg> msg( DBManager::instance().DirectQuery(sql::format("UPDATE account SET last_play=NOW() WHERE id={}", dwID)) );

						TAccountTable & r = d->GetAccountTable();

						r.id = dwID;
						trim_and_lower(pinfo->login, r.login, sizeof(r.login));
						strlcpy(r.passwd, pinfo->passwd, sizeof(r.passwd));
						strlcpy(r.social_id, szSocialID, sizeof(r.social_id));
						DESC_MANAGER::instance().ConnectAccount(r.login, d);


						LoginPrepare(d, pinfo->adwClientKey, aiPremiumTimes);
						metin2::log::info( "QID_AUTH_LOGIN: SUCCESS {}", pinfo->login);
						delete pinfo;
					}
				}
			}
			break;
		case QID_SAFEBOX_SIZE:
			{
				CHARACTER * ch = CHARACTER_MANAGER::instance().FindByPID(qi->dwIdent);

				if (ch)
				{
					if (pMsg->Get()->uiNumRows > 0)
					{
						MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
						int	size = 0;
						str_to_number(size, row[0]);
						ch->SetSafeboxSize(SAFEBOX_PAGE_SIZE * size);
					}
				}
			}
			break;

		case QID_DB_STRING:
			{
				m_map_dbstring.clear();
				m_vec_GreetMessage.clear();

				for (uint i = 0; i < pMsg->Get()->uiNumRows; ++i)
				{
					MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
					if (row[0] && row[1])
					{
						m_map_dbstring.insert(make_pair(std::string(row[0]), std::string(row[1])));
						metin2::log::info( "DBSTR '{}' '{}'", row[0], row[1]);
					}
				}
				if (m_map_dbstring.find("GREET") != m_map_dbstring.end())
				{
					std::istringstream is(m_map_dbstring["GREET"]);
					while (!is.eof())
					{
						std::string str;
						getline(is, str);
						m_vec_GreetMessage.push_back(str);
					}
				}
			}
			break;

		case QID_LOTTO:
			{
				CHARACTER * ch = CHARACTER_MANAGER::instance().FindByPID(qi->dwIdent);
				uint32_t * pdw = (uint32_t *) qi->pvData;

				if (ch)
				{
					if (pMsg->Get()->uiAffectedRows == 0 || pMsg->Get()->uiAffectedRows == (uint32_t)-1)
					{
						metin2::log::info( "GIVE LOTTO FAIL TO pid {:d}", ch->GetPlayerID());
					}
					else
					{
						CItem * pkItem = ch->AutoGiveItem(pdw[0], pdw[1]);

						if (pkItem)
						{
							metin2::log::info( "GIVE LOTTO SUCCESS TO {} (pid {:d})", ch->GetName(), qi->dwIdent);
							//ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("아이템 획득: %s"), pkItem->GetName());

							pkItem->SetSocket(0, pMsg->Get()->uiInsertID);
							pkItem->SetSocket(1, pdw[2]);
						}
						else
							metin2::log::info( "GIVE LOTTO FAIL2 TO pid {:d}", ch->GetPlayerID());
					}
				}

				delete[] pdw;
			}
			break;

		case QID_HIGHSCORE_REGISTER:
			{
				THighscoreRegisterQueryInfo * info = (THighscoreRegisterQueryInfo *) qi->pvData;
				bool bQuery = true;

				if (pMsg->Get()->uiNumRows)
				{
					MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);

					if (row && row[0])
					{
						int iCur = 0;
						str_to_number(iCur, row[0]);

						if ((info->bOrder && iCur >= info->iValue) ||
								(!info->bOrder && iCur <= info->iValue))
							bQuery = false;
					}
				}

				if (bQuery)
					Query(sql::format("REPLACE INTO highscore{} VALUES({}, {}, {})",
							sql::raw(get_table_postfix()), info->szBoard, info->dwPID, info->iValue));

				delete info;
			}
			break;

		case QID_HIGHSCORE_SHOW:
			{
			}
			break;

			// BLOCK_CHAT
		case QID_BLOCK_CHAT_LIST:
			{
				CHARACTER * ch = CHARACTER_MANAGER::instance().FindByPID(qi->dwIdent);
				
				if (ch == nullptr)
					break;
				if (pMsg->Get()->uiNumRows)
				{
					MYSQL_ROW row;
					while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
					{
						ch->ChatPacket(CHAT_TYPE_INFO, "%s %s sec", row[0], row[1]);
					}
				}
				else
				{
					ch->ChatPacket(CHAT_TYPE_INFO, "No one currently blocked.");
				}
			}
			break;
			// END_OF_BLOCK_CHAT


	}

	delete qi;
}

void DBManager::LoadDBString()
{
	ReturnQuery(QID_DB_STRING, 0, nullptr, sql::format("SELECT name, text FROM string{}", sql::raw(get_table_postfix())));
}

const std::vector<std::string>& DBManager::GetGreetMessage()
{
	return m_vec_GreetMessage;
}

void DBManager::SendMoneyLog(uint8_t type, uint32_t vnum, int gold)
{
	if (!gold)
		return;
	TPacketMoneyLog p;
	p.type = type;
	p.vnum = vnum;
	p.gold = gold;
	db_clientdesc->DBPacket(HEADER_GD_MONEY_LOG, 0, &p, sizeof(p));
}

size_t DBManager::EscapeString(char* dst, size_t dstSize, const char *src, size_t srcSize)
{
	return m_sql_direct.EscapeString(dst, dstSize, src, srcSize);
}

//
// Common SQL 
//
AccountDB::AccountDB() :
	m_IsConnect(false)
{
}

bool AccountDB::IsConnected()
{
	return m_IsConnect;
}

bool AccountDB::Connect(const char * host, const int port, const char * user, const char * pwd, const char * db)
{
	m_IsConnect = m_sql_direct.Setup(host, user, pwd, db, "", true, port);

	if (false == m_IsConnect)
	{
		fprintf(stderr, "cannot open direct sql connection to host: %s user: %s db: %s\n", host, user, db);
		return false;
	}

	return m_IsConnect;
}

bool AccountDB::ConnectAsync(const char * host, const int port, const char * user, const char * pwd, const char * db, const char * locale)
{
	m_sql.Setup(host, user, pwd, db, locale, false, port);
	return true;
}

void AccountDB::SetLocale(const std::string & stLocale)
{
	m_sql_direct.SetLocale(stLocale);
	m_sql_direct.QueryLocaleSet();
}

SQLMsg* AccountDB::DirectQuery(const char * query)
{
	return m_sql_direct.DirectQuery(query);
}

void AccountDB::ReturnQuery(int iType, uint32_t dwIdent, void * pvData, const char * c_pszFormat, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, c_pszFormat);
	vsnprintf(szQuery, sizeof(szQuery), c_pszFormat, args);
	va_end(args);

	ReturnQuery(iType, dwIdent, pvData, std::string(szQuery));
}

void AccountDB::ReturnQuery(int iType, uint32_t dwIdent, void * pvData, const std::string & query)
{
	CReturnQueryInfo * p = new CReturnQueryInfo;

	p->iQueryType = QUERY_TYPE_RETURN;
	p->iType = iType;
	p->dwIdent = dwIdent;
	p->pvData = pvData;

	m_sql.ReturnQuery(query.c_str(), p);
}

SQLMsg * AccountDB::PopResult()
{
	SQLMsg * p;

	if (m_sql.PopResult(&p))
		return p;

	return nullptr;
}

void AccountDB::Process()
{
	while (std::unique_ptr<SQLMsg> pMsg{PopResult()})
	{
		CQueryInfo* qi = (CQueryInfo *) pMsg->pvUserData;

		switch (qi->iQueryType)
		{
			case QUERY_TYPE_RETURN:
				AnalyzeReturnQuery(pMsg.get());
				break;
		}
	}
}


enum EAccountQID
{
	QID_SPAM_DB,
};

// Reload every 10 minutes
static LPEVENT s_pkReloadSpamEvent = nullptr;

EVENTINFO(reload_spam_event_info)
{
	// used to send command
	uint32_t empty;
};

EVENTFUNC(reload_spam_event)
{
	AccountDB::instance().ReturnQuery(QID_SPAM_DB, 0, nullptr, "SELECT word, score FROM spam_db WHERE type='SPAM'");
	return PASSES_PER_SEC(g_uiSpamReloadCycle);
}

void LoadSpamDB()
{
	AccountDB::instance().ReturnQuery(QID_SPAM_DB, 0, nullptr, "SELECT word, score FROM spam_db WHERE type='SPAM'");

	if (nullptr == s_pkReloadSpamEvent)
	{
		reload_spam_event_info* info = AllocEventInfo<reload_spam_event_info>();
		s_pkReloadSpamEvent = event_create(reload_spam_event, info, PASSES_PER_SEC(g_uiSpamReloadCycle));
	}
}

void CancelReloadSpamEvent() {
	s_pkReloadSpamEvent = nullptr;
}

void AccountDB::AnalyzeReturnQuery(SQLMsg * pMsg)
{
	CReturnQueryInfo * qi = (CReturnQueryInfo *) pMsg->pvUserData;

	switch (qi->iType)
	{
		case QID_SPAM_DB:
			{
				if (pMsg->Get()->uiNumRows > 0)
				{
					MYSQL_ROW row;

					SpamManager::instance().Clear();

					while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
						SpamManager::instance().Insert(row[0], atoi(row[1]));
				}
			}
			break;
	}

	delete qi;
}
