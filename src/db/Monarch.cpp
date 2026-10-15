#include "Monarch.h"

#include "DBManager.h"
#include "Main.h"

#include <common/tables.h>
#include <common/utils.h>
#include <libsql/AsyncSQL.h>
#include <libsql/query.h>
#include <metin2/config/config.h>
#include <metin2/log/log.h>

#include <mysql.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <utility>


CMonarch::CMonarch()
{
	memset(&m_MonarchInfo, 0, sizeof(MonarchInfo));
}

CMonarch::~CMonarch()
{
}

bool CMonarch::VoteMonarch(uint32_t pid, uint32_t selectdpid)
{
	MAP_MONARCHELECTION::iterator it = m_map_MonarchElection.find(pid);

	if (it == m_map_MonarchElection.end())
	{
		auto p = std::make_unique<MonarchElectionInfo>();
		p->pid = pid;
		p->selectedpid= selectdpid;
		m_map_MonarchElection.insert(MAP_MONARCHELECTION::value_type(pid, std::move(p)));

		char szQuery[256];
		snprintf(szQuery, sizeof(szQuery),
				"INSERT INTO monarch_election(pid, selectedpid, electiondata) VALUES(%d, %d, now())", pid, selectdpid);

		CDBManager::instance().AsyncQuery(szQuery);
		return 1;	
	}

	return 0;
}

void CMonarch::ElectMonarch()
{
	int size = GetVecMonarchCandidacy().size();

	auto s = std::make_unique<int[]>(size);

	auto it = m_map_MonarchElection.begin();

	int idx = 0;

	for (; it != m_map_MonarchElection.end(); ++it)	
	{
		if ((idx =  GetCandidacyIndex(it->second->pid)) < 0)
			continue;

		++s[idx];

		if (metin2::config::test_server())
			metin2::log::info( "[MONARCH_VOTE] pid({:d}) come to vote candidacy pid({:d})", it->second->pid, m_vec_MonarchCandidacy[idx].pid);
	}
}

bool CMonarch::IsCandidacy(uint32_t pid)
{
	VEC_MONARCHCANDIDACY::iterator it = m_vec_MonarchCandidacy.begin();
	
	for (; it != m_vec_MonarchCandidacy.end(); ++it)
	{
		if (it->pid == pid)
			return false;
	}
			
	return true;
}

bool CMonarch::AddCandidacy(uint32_t pid, const char * name)
{
	if (IsCandidacy(pid) == false)
		return false;

	MonarchCandidacy info;

	info.pid = pid;
	strlcpy(info.name, name, sizeof(info.name));
	m_vec_MonarchCandidacy.push_back(info);	

	char szQuery[256];
	snprintf(szQuery, sizeof(szQuery),
			"INSERT INTO monarch_candidacy(pid, date) VALUES(%d, now())", pid);

	CDBManager::instance().AsyncQuery(szQuery);
	return true;
}

bool CMonarch::DelCandidacy(const char * name)
{
	auto it = m_vec_MonarchCandidacy.begin();
	for (; it != m_vec_MonarchCandidacy.end(); ++it)
	{
		if (0 == strncmp(it->name, name, sizeof(it->name)))
		{
			char szQuery[256];
			snprintf(szQuery, sizeof(szQuery),
					"DELETE FROM monarch_candidacy WHERE pid=%d ", it->pid);
			CDBManager::instance().AsyncQuery(szQuery);
		
			m_vec_MonarchCandidacy.erase (it);
			return true;
		}
	}
	return false;

}

bool CMonarch::IsMonarch(int Empire, uint32_t pid)
{
	if (m_MonarchInfo.pid[Empire] != pid)
		return false;
	return true;
}

bool CMonarch::AddMoney(int Empire, int64_t Money)
{
	if (m_MonarchInfo.money[Empire] + Money > 2000000000)
		return true;

	m_MonarchInfo.money[Empire] += Money;

	int64_t Money64 = m_MonarchInfo.money[Empire];

	char szQuery[1024];	
	snprintf(szQuery, sizeof(szQuery), "UPDATE monarch set money=%ld where empire=%d", Money64, Empire);

	CDBManager::instance().AsyncQuery(szQuery);

	return true;
}

bool CMonarch::DecMoney(int Empire, int64_t Money)
{
	if (m_MonarchInfo.money[Empire] - Money <  0)
		return false;
	m_MonarchInfo.money[Empire] -= Money;

	int64_t Money64 = m_MonarchInfo.money[Empire];

	char szQuery[1024];	
	snprintf(szQuery, sizeof(szQuery), "UPDATE monarch set money=%ld where empire=%d", Money64, Empire);

	CDBManager::instance().AsyncQuery(szQuery);
	return true;
}

bool CMonarch::TakeMoney(int Empire, uint32_t pid, int64_t Money)
{
	if (IsMonarch(Empire, pid) == false)
		return false;

	if (m_MonarchInfo.money[Empire] < Money)
		return false;
	
	m_MonarchInfo.money[Empire] -= Money;

	char szQuery[1024];	
	snprintf(szQuery, sizeof(szQuery), 
			"UPDATE monarch SET money=%ld WHERE empire=%d", m_MonarchInfo.money[Empire], Empire);

	CDBManager::instance().AsyncQuery(szQuery);

	if (metin2::config::test_server())
		metin2::log::info( "[MONARCH] Take money empire({:d}) money({:d})", Empire, m_MonarchInfo.money[Empire]);
	return true;
}

bool CMonarch::LoadMonarch()
{
	MonarchInfo * p = &m_MonarchInfo;
    char szQuery[256];
	snprintf(szQuery, sizeof(szQuery), "SELECT empire, pid, name, money, windate FROM monarch a, player%s b where a.pid=b.id", GetTablePostfix());
    std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQuery(szQuery, SQL_PLAYER));

    if (pMsg->Get()->uiNumRows == 0)
    {
        return false;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)) != nullptr)
    {
        int idx = 0;
        int Empire = 0; str_to_number(Empire, row[idx++]);

        str_to_number(p->pid[Empire], row[idx++]);
		strlcpy(p->name[Empire], row[idx++], sizeof(p->name[Empire]));

        str_to_number(p->money[Empire], row[idx++]);
		strlcpy(p->date[Empire], row[idx++], sizeof(p->date[Empire]));

		if (metin2::config::test_server())
		metin2::log::info( "[LOAD_MONARCH] Empire {:d} pid {:d} money {:d} windate {}", Empire, p->pid[Empire], p->money[Empire], p->date[Empire]);
    }

    return true;
}

bool CMonarch::SetMonarch(const char * name)
{
	MonarchInfo * p = &m_MonarchInfo;
	std::string query = sql::format(
			"SELECT b.empire, a.id, a.name FROM player{} AS a, player_index{} AS b WHERE a.account_id=b.id AND a.name={}",
			sql::raw(GetTablePostfix()), sql::raw(GetTablePostfix()), std::string_view(name, strnlen(name, CHARACTER_NAME_MAX_LEN)));
    std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQuery(query.c_str(), SQL_PLAYER));

    if (pMsg->Get()->uiNumRows == 0)
    {
        return false;
    }

    MYSQL_ROW row;
	int Empire = 0;
    while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)) != nullptr)
    {
        int idx = 0;
        str_to_number(Empire, row[idx++]);

        str_to_number(p->pid[Empire], row[idx++]);
		strlcpy(p->name[Empire], row[idx++], sizeof(p->name[Empire]));

		if (metin2::config::test_server())
			metin2::log::info( "[Set_MONARCH] Empire {:d} pid {:d} money {:d} windate {}", Empire, p->pid[Empire], p->money[Empire], p->date[Empire]);
    }

	//Insert into db
	query = sql::format(
					"REPLACE INTO monarch (empire, pid, windate, money) VALUES({}, {}, NOW(), {})", Empire, p->pid[Empire], p->money[Empire]);

 	CDBManager::instance().AsyncQuery(query.c_str(), SQL_PLAYER);	
    return true;
}

bool CMonarch::DelMonarch(const char * name)
{
	for (int n = 1; n < 4; ++n)
	{
		if (0 == strncmp(m_MonarchInfo.name[n], name, sizeof(m_MonarchInfo.name[n])))
		{
			char szQuery[256];

			int Empire = n;
			snprintf(szQuery, sizeof(szQuery), "DELETE FROM monarch WHERE empire=%d", Empire);
			std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQuery(szQuery, SQL_PLAYER));

			if (pMsg->Get()->uiAffectedRows == 0 || pMsg->Get()->uiAffectedRows == (uint32_t)-1)
			{
				metin2::log::error(" DirectQuery failed({})", szQuery);
				return false;
			}

			memset(m_MonarchInfo.name[Empire], 0, 32);
			m_MonarchInfo.money[Empire] = 0;
			m_MonarchInfo.pid[Empire] = 0;
			return true;
		}
	}
		
	return false;
}

int CMonarch::GetCandidacyIndex(uint32_t pid)
{
	auto it = m_vec_MonarchCandidacy.begin();

	for (int n = 0; it != m_vec_MonarchCandidacy.end(); ++it, ++n)
	{
		if (it->pid == pid)
			return n;		
	}

	return -1;
}
