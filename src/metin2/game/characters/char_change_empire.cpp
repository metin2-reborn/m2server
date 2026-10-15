
#include <metin2/game/characters/char.h>
#include <metin2/game/characters/char_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/social/marriage.h>

#include <metin2/common/utils.h>
#include <metin2/sql/AsyncSQL.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

/*

   Return Value
		0 : unknown error or query error
		1 : trying to change to the same empire
		2 : a character has joined a guild
		3 : a character is married

		999 : empire change succeeded
*/
namespace metin2::game::inline characters {

int CHARACTER::ChangeEmpire(uint8_t empire)
{
	if (GetEmpire() == empire)
		return 1;

	uint32_t dwAID;
	uint32_t dwPID[4];
	memset(dwPID, 0, sizeof(dwPID));

	{
		// 1. get all pids of my account
		const std::string query = sql::format(
				"SELECT id, pid1, pid2, pid3, pid4 FROM player_index{} WHERE (pid1={} OR pid2={} OR pid3={} OR pid4={}) AND empire={}", 
				sql::raw(get_table_postfix()), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetEmpire());

		std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(query));

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
		for (int i = 0; i < loop; ++i)
		{
			const std::string query = sql::format("SELECT guild_id FROM guild_member{} WHERE pid={}", sql::raw(get_table_postfix()), dwPID[i]);

			std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery(query));

			if (pMsg != nullptr)
			{
				if (pMsg->Get()->uiNumRows > 0)
				{
					MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);

					str_to_number(dwGuildID[i], row[0]);

					pGuild[i] = CGuildManager::instance().FindGuild(dwGuildID[i]);

					if (pGuild[i] != nullptr)
					{
						return 2;
					}
				}
				else
				{
					dwGuildID[i] = 0;
					pGuild[i] = nullptr;
				}
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
		const std::string query = sql::format("UPDATE player_index{} SET empire={} WHERE (pid1={} OR pid2={} OR pid3={} OR pid4={}) AND empire={}", 
				sql::raw(get_table_postfix()), empire, GetPlayerID(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetEmpire());

		std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery(query));

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
	uint32_t dwAID = GetAID();

	if (dwAID == 0)
		return 0;

	std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery(sql::format("SELECT change_count FROM change_empire WHERE account_id = {}", dwAID)));

	if (pMsg != nullptr)
	{
		if (pMsg->Get()->uiNumRows == 0)
		{
			return 0;
		}

		MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);

		uint32_t count = 0;
		str_to_number(count, row[0]);

		return count;
	}

	return 0;
}

void CHARACTER::SetChangeEmpireCount()
{
	std::string query;

	uint32_t dwAID = GetAID();

	if (dwAID == 0) return;

	int count = GetChangeEmpireCount();

	if (count == 0)
	{
		count++;
		query = sql::format("INSERT INTO change_empire VALUES({}, {}, NOW())", dwAID, count);
	}
	else
	{
		count++;
		query = sql::format("UPDATE change_empire SET change_count={} WHERE account_id={}", count, dwAID);
	}

	std::unique_ptr<SQLMsg> pmsg(DBManager::instance().DirectQuery(query));
}

uint32_t CHARACTER::GetAID() const
{
	uint32_t dwAID = 0;

	const std::string query = sql::format("SELECT id FROM player_index{} WHERE (pid1={} OR pid2={} OR pid3={} OR pid4={}) AND empire={}", 
			sql::raw(get_table_postfix()), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetPlayerID(), GetEmpire());

	std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery(query));

	if (pMsg != nullptr)
	{
		if (pMsg->Get()->uiNumRows == 0)
		{
			return 0;
		}

		MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);

		str_to_number(dwAID, row[0]);

		return dwAID;
	}
	else
	{
		return 0;
	}
}
}
