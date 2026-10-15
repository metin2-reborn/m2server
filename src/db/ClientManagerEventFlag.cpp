// vim:ts=4 sw=4
#include "ClientManager.h"
#include "DBManager.h"
#include "Main.h"
#include "Peer.h"

#include <common/tables.h>
#include <common/utils.h>
#include <libsql/AsyncSQL.h>
#include <libsql/query.h>
#include <libthecore/log.h>

#include <mysql.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

void CClientManager::LoadEventFlag()
{
	char szQuery[1024];
	snprintf(szQuery, sizeof(szQuery), "SELECT szName, lValue FROM quest%s WHERE dwPID = 0", GetTablePostfix());
	std::unique_ptr<SQLMsg> pmsg(CDBManager::instance().DirectQuery(szQuery));

	SQLResult* pRes = pmsg->Get();
	if (pRes->uiNumRows)
	{
		MYSQL_ROW row;
		while ((row = mysql_fetch_row(pRes->pSQLResult)))
		{
			TPacketSetEventFlag p;
			strlcpy(p.szFlagName, row[0], sizeof(p.szFlagName));
			str_to_number(p.lValue, row[1]);
			sys_log(0, "EventFlag Load %s %d", p.szFlagName, p.lValue);
			m_map_lEventFlag.insert(std::make_pair(std::string(p.szFlagName), p.lValue));
			ForwardPacket(HEADER_DG_SET_EVENT_FLAG, &p, sizeof(TPacketSetEventFlag));
		}
	}
}

void CClientManager::SetEventFlag(TPacketSetEventFlag* p)
{
	ForwardPacket(HEADER_DG_SET_EVENT_FLAG, p, sizeof(TPacketSetEventFlag));

	bool bChanged = false;

	__typeof(m_map_lEventFlag.begin()) it = m_map_lEventFlag.find(p->szFlagName);
	if (it == m_map_lEventFlag.end())
	{
		bChanged = true;
		m_map_lEventFlag.insert(std::make_pair(std::string(p->szFlagName), p->lValue));
	}
	else if (it->second != p->lValue)
	{
		bChanged = true;
		it->second = p->lValue;
	}

	if (bChanged)
	{
		const std::string query = sql::format(
				"REPLACE INTO quest{} (dwPID, szName, szState, lValue) VALUES(0, {}, '', {})",
				sql::raw(GetTablePostfix()), p->szFlagName, p->lValue);

		CDBManager::instance().AsyncQuery(query.c_str());
		sys_log(0, "HEADER_GD_SET_EVENT_FLAG : Changed CClientmanager::SetEventFlag(%s %d) ", p->szFlagName, p->lValue);
		return;
	}
	sys_log(0, "HEADER_GD_SET_EVENT_FLAG : No Changed CClientmanager::SetEventFlag(%s %d) ", p->szFlagName, p->lValue);
}

void CClientManager::SendEventFlagsOnSetup(CPeer* peer)
{
	__typeof(m_map_lEventFlag.begin()) it;
	for (it = m_map_lEventFlag.begin(); it != m_map_lEventFlag.end(); ++it)
	{
		TPacketSetEventFlag p;
		strlcpy(p.szFlagName, it->first.c_str(), sizeof(p.szFlagName));
		p.lValue = it->second;
		peer->EncodeHeader(HEADER_DG_SET_EVENT_FLAG, 0, sizeof(TPacketSetEventFlag));
		peer->Encode(&p, sizeof(TPacketSetEventFlag));
	}
}

