
#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "db.h"
#include "guild_manager.h"
#include "marriage.h"

#include <common/utils.h>
#include <libsql/AsyncSQL.h>

#include <mysql.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

/*
   Return Value
		0 : unknown error or query error
		1 : trying to change to the same empire
		2 : a character has joined a guild
		3 : a character is married

		999 : empire change succeeded
*/
int CHARACTER::ChangeEmpire(uint8_t empire)
{
	if (GetEmpire() == empire)
		return 1;

	char szQuery[1024+1];
	uint32_t dwAID;
	uint32_t dwPID[4];
	memset(dwPID, 0, sizeof(dwPID));

	{
		// 1. get all pids of my account
		snprintf(szQuery, sizeof(szQuery), 
				"SELECT id, pid1, pid2, pid3, pid4 FROM player_index%s WHERE pid1=%u OR pid2=%u OR pid3=%u OR pid4=%u AND empire=%u", 
				get_table_postfix(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetEmpire());

		std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(szQuery));

		if (msg->Get()->uiNumRows == 0)
		{
			return 0;
		}

		MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);

		str_to_number(dwAID, row[0]);
		str_to_number(dwPID[0], row[1]);
		str_to_number(dwPID[1], row[2]);
		str_to_number(dwPID[2], row[3]);
		str_to_number(dwPID[3], row[4]);
	}

	const int loop = 4;

	{
		// 2. get guild info for each character.
		//   if even one character is in a guild, the empire cannot be changed.
		uint32_t dwGuildID[4];
		CGuild * pGuild[4];
		SQLMsg * pMsg = nullptr;
		
		for (int i = 0; i < loop; ++i)
		{
			snprintf(szQuery, sizeof(szQuery), "SELECT guild_id FROM guild_member%s WHERE pid=%u", get_table_postfix(), dwPID[i]);

			pMsg = DBManager::instance().DirectQuery(szQuery);

			if (pMsg != nullptr)
			{
				if (pMsg->Get()->uiNumRows > 0)
				{
					MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);

					str_to_number(dwGuildID[i], row[0]);

					pGuild[i] = CGuildManager::instance().FindGuild(dwGuildID[i]);

					if (pGuild[i] != nullptr)
					{
						delete pMsg;
						return 2;
					}
				}
				else
				{
					dwGuildID[i] = 0;
					pGuild[i] = nullptr;
				}

				delete pMsg;
			}
		}
	}

	{
		// 3. get marriage info for each character.
		//   if even one character is married, the empire cannot be changed.
		for (int i = 0; i < loop; ++i)
		{
			if (marriage::CManager::instance().IsEngagedOrMarried(dwPID[i]) == true)
				return 3;
		}
	}
	
	{
		// 4. update the empire info in the db.
		snprintf(szQuery, sizeof(szQuery), "UPDATE player_index%s SET empire=%u WHERE pid1=%u OR pid2=%u OR pid3=%u OR pid4=%u AND empire=%u", 
				get_table_postfix(), empire, GetPlayerID(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetEmpire());

		std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(szQuery));

		if (msg->Get()->uiAffectedRows > 0)
		{
			// 5. add an empire change history record.
			SetChangeEmpireCount();

			return 999;
		}
	}

	return 0;
}

int CHARACTER::GetChangeEmpireCount() const
{
	char szQuery[1024+1];
	uint32_t dwAID = GetAID();

	if (dwAID == 0)
		return 0;

	snprintf(szQuery, sizeof(szQuery), "SELECT change_count FROM change_empire WHERE account_id = %u", dwAID);

	SQLMsg * pMsg = DBManager::instance().DirectQuery(szQuery);

	if (pMsg != nullptr)
	{
		if (pMsg->Get()->uiNumRows == 0)
		{
			delete pMsg;
			return 0;
		}

		MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);

		uint32_t count = 0;
		str_to_number(count, row[0]);

		delete pMsg;

		return count;
	}

	return 0;
}

void CHARACTER::SetChangeEmpireCount()
{
	char szQuery[1024+1];

	uint32_t dwAID = GetAID();

	if (dwAID == 0) return;

	int count = GetChangeEmpireCount();

	if (count == 0)
	{
		count++;
		snprintf(szQuery, sizeof(szQuery), "INSERT INTO change_empire VALUES(%u, %d, NOW())", dwAID, count);
	}
	else
	{
		count++;
		snprintf(szQuery, sizeof(szQuery), "UPDATE change_empire SET change_count=%d WHERE account_id=%u", count, dwAID);
	}

	std::unique_ptr<SQLMsg> pmsg(DBManager::instance().DirectQuery(szQuery));
}

uint32_t CHARACTER::GetAID() const
{
	char szQuery[1024+1];
	uint32_t dwAID = 0;

	snprintf(szQuery, sizeof(szQuery), "SELECT id FROM player_index%s WHERE pid1=%u OR pid2=%u OR pid3=%u OR pid4=%u AND empire=%u", 
			get_table_postfix(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetEmpire());

	SQLMsg* pMsg = DBManager::instance().DirectQuery(szQuery);

	if (pMsg != nullptr)
	{
		if (pMsg->Get()->uiNumRows == 0)
		{
			delete pMsg;
			return 0;
		}

		MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);

		str_to_number(dwAID, row[0]);

		delete pMsg;

		return dwAID;
	}
	else
	{
		return 0;
	}
}

