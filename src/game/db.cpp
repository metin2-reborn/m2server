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
#include <libthecore/log.h>
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
		sys_err("cannot open direct sql connection to host %s", host);

	if (m_bIsConnect && !g_bAuthServer)
	{
		LoadDBString();
	}

	return m_bIsConnect;
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

	CReturnQueryInfo * p = new CReturnQueryInfo;

	p->iQueryType = QUERY_TYPE_RETURN;
	p->iType = iType;
	p->dwIdent = dwIdent;
	p->pvData = pvData;

	m_sql.ReturnQuery(szQuery, p);
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
	SQLMsg* pMsg = nullptr;

	while ((pMsg = PopResult()))
	{
		if( nullptr != pMsg->pvUserData )
		{
			switch( reinterpret_cast<CQueryInfo*>(pMsg->pvUserData)->iQueryType )
			{
				case QUERY_TYPE_RETURN:
					AnalyzeReturnQuery(pMsg);
					break;

				case QUERY_TYPE_FUNCTION:
					{
						CFuncQueryInfo* qi = reinterpret_cast<CFuncQueryInfo*>( pMsg->pvUserData );
						qi->f(pMsg);
						delete qi;
					}
					break;

				case QUERY_TYPE_AFTER_FUNCTION:
					{
						CFuncAfterQueryInfo* qi = reinterpret_cast<CFuncAfterQueryInfo*>( pMsg->pvUserData );
						qi->f();
						delete qi;
					}
					break;
			}
		}

		delete pMsg;
	}
}

void DBManager::FuncQuery(std::function<void(SQLMsg *)> f, const char * c_pszFormat, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, c_pszFormat);
	vsnprintf(szQuery, sizeof(szQuery), c_pszFormat, args);
	va_end(args);

	CFuncQueryInfo * p = new CFuncQueryInfo;
	p->iQueryType = QUERY_TYPE_FUNCTION;
	p->f = std::move(f);

	m_sql.ReturnQuery(szQuery, p);
}

void DBManager::FuncAfterQuery(std::function<void()> f, const char * c_pszFormat, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, c_pszFormat);
	vsnprintf(szQuery, sizeof(szQuery), c_pszFormat, args);
	va_end(args);

	CFuncAfterQueryInfo * p = new CFuncAfterQueryInfo;
	p->iQueryType = QUERY_TYPE_AFTER_FUNCTION;
	p->f = std::move(f);

	m_sql.ReturnQuery(szQuery, p);
}

CLoginData * DBManager::GetLoginData(uint32_t dwKey)
{
	std::map<uint32_t, CLoginData *>::iterator it = m_map_pkLoginData.find(dwKey);

	if (it == m_map_pkLoginData.end())
		return nullptr;

	return it->second;
}

void DBManager::InsertLoginData(CLoginData * pkLD)
{
	m_map_pkLoginData.insert(std::make_pair(pkLD->GetKey(), pkLD));
}

void DBManager::DeleteLoginData(CLoginData * pkLD)
{
	std::map<uint32_t, CLoginData *>::iterator it = m_map_pkLoginData.find(pkLD->GetKey());

	if (it == m_map_pkLoginData.end())
		return;

	sys_log(0, "DeleteLoginData %s %p", pkLD->GetLogin(), pkLD);

	delete it->second;
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
	sys_log(0, "SendAuthLogin %s key %u", ptod.szLogin, ptod.dwID);

	// The db keeps the login from here on.
	DeleteLoginData(pkLD);
}

void DBManager::LoginPrepare(DESC * d, uint32_t * pdwClientKey, int * paiPremiumTimes)
{
	const TAccountTable & r = d->GetAccountTable();

	CLoginData * pkLD = new CLoginData;

	pkLD->SetKey(d->GetLoginKey());
	pkLD->SetLogin(r.login);
	pkLD->SetClientKey(pdwClientKey);

	if (paiPremiumTimes)
		pkLD->SetPremium(paiPremiumTimes);

	InsertLoginData(pkLD);

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

				sys_log(0, "QID_AUTH_LOGIN: START %u %p", qi->dwIdent, d);

				if (pMsg->Get()->uiNumRows == 0)
				{
					sys_log(0, "   NOID");
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
						sys_err("error column %d", col);
						delete pinfo;
					   	break; 
					}
					
					strlcpy(szEncrytPassword, row[col++], sizeof(szEncrytPassword));

					if (!row[col]) 
					{
					   	sys_err("error column %d", col);
						delete pinfo;
					   	break;
				   	}
				
					strlcpy(szPassword, row[col++], sizeof(szPassword));

					if (!row[col])
				   	{ 
						sys_err("error column %d", col); 
						delete pinfo;
						break;
				   	}

					strlcpy(szSocialID, row[col++], sizeof(szSocialID));

					if (!row[col])
				   	{
					   	sys_err("error column %d", col);
						delete pinfo;
					   	break;
				   	}
				
					str_to_number(dwID, row[col++]);
					
					if (!row[col]) 
					{
					   	sys_err("error column %d", col); 
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

					sys_log(0, "Create_Time %d %s", retValue, szCreateDate);
					sys_log(0, "Block Time %d ", strncmp(szCreateDate, g_stBlockDate.c_str(), 8));

					int nPasswordDiff = strcmp(szEncrytPassword, szPassword);


					if (nPasswordDiff)
					{
						LoginFailure(d, "WRONGPWD");
						sys_log(0, "   WRONGPWD");
						delete pinfo;
					}
					else if (bNotAvail)
					{
						LoginFailure(d, "NOTAVAIL");
						sys_log(0, "   NOTAVAIL");
						delete pinfo;
					}
					else if (DESC_MANAGER::instance().FindByLoginName(pinfo->login))
					{
						LoginFailure(d, "ALREADY");
						sys_log(0, "   ALREADY");
						delete pinfo;
					}
					else if (strcmp(szStatus, "OK"))
					{
						LoginFailure(d, szStatus);
						sys_log(0, "   STATUS: %s", szStatus);
						delete pinfo;
					}
					else
					{
						//stBlockData >= 0 == date is later than BlockDate 
						if (strncmp(szCreateDate, g_stBlockDate.c_str(), 8) >= 0)
						{
							LoginFailure(d, "BLKLOGIN");
							sys_log(0, "   BLKLOGIN");
							delete pinfo;
							break;
						}

						char szQuery[1024];
						snprintf(szQuery, sizeof(szQuery), "UPDATE account SET last_play=NOW() WHERE id=%u", dwID);
						std::unique_ptr<SQLMsg> msg( DBManager::instance().DirectQuery(szQuery) );

						TAccountTable & r = d->GetAccountTable();

						r.id = dwID;
						trim_and_lower(pinfo->login, r.login, sizeof(r.login));
						strlcpy(r.passwd, pinfo->passwd, sizeof(r.passwd));
						strlcpy(r.social_id, szSocialID, sizeof(r.social_id));
						DESC_MANAGER::instance().ConnectAccount(r.login, d);


						LoginPrepare(d, pinfo->adwClientKey, aiPremiumTimes);
						sys_log(0, "QID_AUTH_LOGIN: SUCCESS %s", pinfo->login);
						delete pinfo;
					}
				}
			}
			break;
		case QID_AUTH_LOGIN_OPENID:
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

				sys_log(0, "QID_AUTH_LOGIN_OPENID: START %u %p", qi->dwIdent, d);

				if (pMsg->Get()->uiNumRows == 0)
				{
					sys_log(0, "   NOID");
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
						sys_err("error column %d", col);
						delete pinfo;
					   	break; 
					}
					
					strlcpy(szEncrytPassword, row[col++], sizeof(szEncrytPassword));

					if (!row[col]) 
					{
					   	sys_err("error column %d", col);
						delete pinfo;
					   	break;
				   	}
				
					strlcpy(szPassword, row[col++], sizeof(szPassword));

					if (!row[col])
				   	{ 
						sys_err("error column %d", col); 
						delete pinfo;
						break;
				   	}

					strlcpy(szSocialID, row[col++], sizeof(szSocialID));

					if (!row[col])
				   	{
					   	sys_err("error column %d", col);
						delete pinfo;
					   	break;
				   	}
				
					str_to_number(dwID, row[col++]);
					
					if (!row[col]) 
					{
					   	sys_err("error column %d", col); 
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

					sys_log(0, "Create_Time %d %s", retValue, szCreateDate);
					sys_log(0, "Block Time %d ", strncmp(szCreateDate, g_stBlockDate.c_str(), 8));

					int nPasswordDiff = strcmp(szEncrytPassword, szPassword);


					//OpenID : for OpenID, the password is not checked.
					if (openid_server)
					{
						nPasswordDiff = 0;
					}

					if (nPasswordDiff)
					{
						LoginFailure(d, "WRONGPWD");
						sys_log(0, "   WRONGPWD");
						delete pinfo;
					}
					else if (bNotAvail)
					{
						LoginFailure(d, "NOTAVAIL");
						sys_log(0, "   NOTAVAIL");
						delete pinfo;
					}
					else if (DESC_MANAGER::instance().FindByLoginName(pinfo->login))
					{
						LoginFailure(d, "ALREADY");
						sys_log(0, "   ALREADY");
						delete pinfo;
					}
					else if (strcmp(szStatus, "OK"))
					{
						LoginFailure(d, szStatus);
						sys_log(0, "   STATUS: %s", szStatus);
						delete pinfo;
					}
					else
					{
						//stBlockData >= 0 == date is later than BlockDate 
						if (strncmp(szCreateDate, g_stBlockDate.c_str(), 8) >= 0)
						{
							LoginFailure(d, "BLKLOGIN");
							sys_log(0, "   BLKLOGIN");
							delete pinfo;
							break;
						}

						char szQuery[1024];
						snprintf(szQuery, sizeof(szQuery), "UPDATE account SET last_play=NOW() WHERE id=%u", dwID);
						std::unique_ptr<SQLMsg> msg( DBManager::instance().DirectQuery(szQuery) );

						TAccountTable & r = d->GetAccountTable();

						r.id = dwID;
						trim_and_lower(pinfo->login, r.login, sizeof(r.login));
						strlcpy(r.passwd, pinfo->passwd, sizeof(r.passwd));
						strlcpy(r.social_id, szSocialID, sizeof(r.social_id));
						DESC_MANAGER::instance().ConnectAccount(r.login, d);


						LoginPrepare(d, pinfo->adwClientKey, aiPremiumTimes);
						sys_log(0, "QID_AUTH_LOGIN_OPENID: SUCCESS %s", pinfo->login);
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
						sys_log(0, "DBSTR '%s' '%s'", row[0], row[1]);
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
						sys_log(0, "GIVE LOTTO FAIL TO pid %u", ch->GetPlayerID());
					}
					else
					{
						CItem * pkItem = ch->AutoGiveItem(pdw[0], pdw[1]);

						if (pkItem)
						{
							sys_log(0, "GIVE LOTTO SUCCESS TO %s (pid %u)", ch->GetName(), qi->dwIdent);
							//ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("아이템 획득: %s"), pkItem->GetName());

							pkItem->SetSocket(0, pMsg->Get()->uiInsertID);
							pkItem->SetSocket(1, pdw[2]);
						}
						else
							sys_log(0, "GIVE LOTTO FAIL2 TO pid %u", ch->GetPlayerID());
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
					Query("REPLACE INTO highscore%s VALUES('%s', %u, %d)",
							get_table_postfix(), info->szBoard, info->dwPID, info->iValue);

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


		case QID_BRAZIL_CREATE_ID :
			{
				TPacketCGLogin3 * pinfo = (TPacketCGLogin3 *) qi->pvData ;

				if( pMsg->Get()->uiAffectedRows == 0 || pMsg->Get()->uiAffectedRows == (uint32_t)-1 )
				{
					DESC * d = DESC_MANAGER::instance().FindByLoginKey(qi->dwIdent) ;
					sys_log(0, "[AUTH_BRAZIL]   NOID") ;
					sys_log(0, "[AUTH_BRAZIL] : Failed to create a new account %s", pinfo->login) ;
					LoginFailure(d, "NOID") ;
					delete pinfo;
				}
				else
				{
					sys_log(0, "[AUTH_BRAZIL] : Succeed to create a new account %s", pinfo->login) ;

					ReturnQuery(QID_AUTH_LOGIN, qi->dwIdent, pinfo,
							"SELECT PASSWORD('%s'),password,social_id,id,status,availDt - NOW() > 0,"
							"UNIX_TIMESTAMP(silver_expire),"
							"UNIX_TIMESTAMP(gold_expire),"
							"UNIX_TIMESTAMP(safebox_expire),"
							"UNIX_TIMESTAMP(autoloot_expire),"
							"UNIX_TIMESTAMP(fish_mind_expire),"
							"UNIX_TIMESTAMP(marriage_fast_expire),"
							"UNIX_TIMESTAMP(money_drop_rate_expire),"
							"UNIX_TIMESTAMP(create_time)"
							" FROM account WHERE login='%s'",
							pinfo->passwd, pinfo->login) ;
				}
			}
			break;
		case QID_JAPAN_CREATE_ID :
			{
				TPacketCGLogin3 * pinfo = (TPacketCGLogin3 *) qi->pvData ;

				if( pMsg->Get()->uiAffectedRows == 0 || pMsg->Get()->uiAffectedRows == (uint32_t)-1 )
				{
					DESC * d = DESC_MANAGER::instance().FindByLoginKey(qi->dwIdent) ;
					sys_log(0, "[AUTH_JAPAN]   NOID") ;
					sys_log(0, "[AUTH_JAPAN] : Failed to create a new account %s", pinfo->login) ;
					LoginFailure(d, "NOID") ;
					delete pinfo;
				}
				else
				{
					sys_log(0, "[AUTH_JAPAN] : Succeed to create a new account %s", pinfo->login) ;

					ReturnQuery(QID_AUTH_LOGIN_OPENID, qi->dwIdent, pinfo,
							"SELECT PASSWORD('%s'),password,social_id,id,status,availDt - NOW() > 0,"
							"UNIX_TIMESTAMP(silver_expire),"
							"UNIX_TIMESTAMP(gold_expire),"
							"UNIX_TIMESTAMP(safebox_expire),"
							"UNIX_TIMESTAMP(autoloot_expire),"
							"UNIX_TIMESTAMP(fish_mind_expire),"
							"UNIX_TIMESTAMP(marriage_fast_expire),"
							"UNIX_TIMESTAMP(money_drop_rate_expire),"
							"UNIX_TIMESTAMP(create_time)"
							" FROM account WHERE login='%s'",
							pinfo->passwd, pinfo->login) ;
				}
			}
			break;

		default:
			sys_err("FATAL ERROR!!! Unhandled return query id %d", qi->iType);
			break;
	}

	delete qi;
}

void DBManager::LoadDBString()
{
	ReturnQuery(QID_DB_STRING, 0, nullptr, "SELECT name, text FROM string%s", get_table_postfix());
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

void DBManager::RequestBlockException(const char *login, int cmd)
{
	TPacketBlockException packet;

	packet.cmd = cmd;
	strlcpy(packet.login, login, sizeof(packet.login));
	db_clientdesc->DBPacket(HEADER_GD_BLOCK_EXCEPTION, 0, &packet, sizeof(packet));
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

	CReturnQueryInfo * p = new CReturnQueryInfo;

	p->iQueryType = QUERY_TYPE_RETURN;
	p->iType = iType;
	p->dwIdent = dwIdent;
	p->pvData = pvData;

	m_sql.ReturnQuery(szQuery, p);
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
	SQLMsg* pMsg = nullptr;

	while ((pMsg = PopResult()))
	{
		CQueryInfo* qi = (CQueryInfo *) pMsg->pvUserData;

		switch (qi->iQueryType)
		{
			case QUERY_TYPE_RETURN:
				AnalyzeReturnQuery(pMsg);
				break;
		}
	}

	delete pMsg;
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
