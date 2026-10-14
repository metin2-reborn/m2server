#include "HB.h"

#include "DBManager.h"
#include "Main.h"

#include <libsql/AsyncSQL.h>
#include <libthecore/log.h>
#include <libthecore/utils.h>

#include <mysql.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <utility>

PlayerHB::PlayerHB()
{
	m_iExpireTime = 3600; // 1 hour hotbackup default.
}

PlayerHB::~PlayerHB()
{
}

bool PlayerHB::Initialize()
{
	char szQuery[128];
	snprintf(szQuery, sizeof(szQuery), "SHOW CREATE TABLE player%s", GetTablePostfix());

	std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQuery(szQuery));

	if (pMsg->Get()->uiNumRows == 0)
		return false;

	MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
	m_stCreateTableQuery = row[1];
	return true;
}

//
// @version	05/07/05 Bang2ni - Added code to query and insert data when there's no data for the id.
//
void PlayerHB::Put(uint32_t id)
{
	auto it = m_map_data.find(id);

	if (it == m_map_data.end())
	{
		Query(id);
		m_map_data.insert(std::pair< uint32_t, time_t >(id, get_dword_time()));
		return;
	}

	if (time(nullptr) - it->second > m_iExpireTime)
		Query(id);
}

//
// @version	05/07/05 Bang2ni - Enlarged the query string buffer since it was too small.
//
bool PlayerHB::Query(uint32_t id)
{
	time_t ct = time(nullptr);
	struct tm curr_tm = *localtime(&ct);
	char szTableName[64];
	snprintf(szTableName, sizeof(szTableName), "hb_%02d%02d%02d%02d_player%s", 
			curr_tm.tm_year - 100, curr_tm.tm_mon + 1, curr_tm.tm_mday, curr_tm.tm_hour, GetTablePostfix());

	char szQuery[4096];

	if (m_stTableName.compare(szTableName))
	{
		char szFind[32];
		snprintf(szFind, sizeof(szFind), "CREATE TABLE `player%s`", GetTablePostfix());
		int pos = m_stCreateTableQuery.find(szFind);

		if (pos < 0)
		{
			sys_err("cannot find %s ", szFind);	
			return false;
		}

		snprintf(szQuery, sizeof(szQuery), "CREATE TABLE IF NOT EXISTS %s%s", szTableName, m_stCreateTableQuery.c_str() + strlen(szFind));
		std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQuery(szQuery, SQL_HOTBACKUP));
		m_stTableName = szTableName;
	}

	snprintf(szQuery, sizeof(szQuery), "REPLACE INTO %s SELECT * FROM %splayer%s WHERE id=%u", m_stTableName.c_str(), GetPlayerDBName(), GetTablePostfix(), id);
	CDBManager::instance().AsyncQuery(szQuery, SQL_HOTBACKUP);
	return true;
}

