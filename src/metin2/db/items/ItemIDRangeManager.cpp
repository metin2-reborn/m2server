#include <metin2/db/items/ItemIDRangeManager.h>

#include <metin2/db/clients/ClientManager.h>
#include <metin2/db/storage/DBManager.h>
#include <metin2/db/app/Main.h>
#include <metin2/db/clients/Peer.h>

#include <metin2/proto/tables.h>
#include <metin2/common/utils.h>
#include <metin2/sql/AsyncSQL.h>
#include <metin2/log/log.h>

#include <mysql.h>

#include <cstdint>
#include <cstdio>
#include <memory>

namespace metin2::db::inline items {

CItemIDRangeManager::CItemIDRangeManager()
{
	m_listData.clear();
}

void CItemIDRangeManager::Build()
{
	uint32_t dwMin = 0;
	uint32_t dwMax = 0;
	TItemIDRangeTable range;

	for (int i = 0; ; ++i)
	{
		dwMin = cs_dwMinimumRange * (i + 1) + 1;
		dwMax = cs_dwMinimumRange * (i + 2);

		if (dwMax == cs_dwMaxItemID)
			break;

		if (CClientManager::instance().GetItemRange().dwMin <= dwMin &&
			CClientManager::instance().GetItemRange().dwMax >= dwMax)
		{
			continue;
		}

		if (BuildRange(dwMin, dwMax, range) == true)
		{
			m_listData.push_back(range);
		}
	}
}

struct FCheckCollision
{
	bool hasCollision;
	TItemIDRangeTable range;

	FCheckCollision(TItemIDRangeTable data)
	{
		hasCollision = false;
		range = data;
	}

	void operator() (CPeer* peer)
	{
		if (hasCollision == false)
		{
			hasCollision = peer->CheckItemIDRangeCollision(range);
		}
	}
};

TItemIDRangeTable CItemIDRangeManager::GetRange()
{
	TItemIDRangeTable ret;
	ret.dwMin = 0;
	ret.dwMax = 0;
	ret.dwUsableItemIDMin = 0;

	if (m_listData.size() > 0)
	{
		while (m_listData.size() > 0)
		{
			ret = m_listData.front();
			m_listData.pop_front();

			FCheckCollision f(ret);
			CClientManager::instance().for_each_peer(f);

			if (f.hasCollision == false) return ret;
		}
	}

	for (int i = 0; i < 10; ++i)
		metin2::log::error("ItemIDRange: NO MORE ITEM ID RANGE");

	return ret;
}

bool CItemIDRangeManager::BuildRange(uint32_t dwMin, uint32_t dwMax, TItemIDRangeTable& range)
{
	char szQuery[1024];
	uint32_t dwItemMaxID = 0;
	std::unique_ptr<SQLMsg> pMsg;
	MYSQL_ROW row;

	snprintf(szQuery, sizeof(szQuery), "SELECT MAX(id) FROM item%s WHERE id >= %u and id <= %u", GetTablePostfix(), dwMin, dwMax);

	pMsg.reset(CDBManager::instance().DirectQuery(szQuery));

	if (pMsg != nullptr)
	{
		if (pMsg->Get()->uiNumRows > 0)
		{
			row = mysql_fetch_row(pMsg->Get()->pSQLResult);
			str_to_number(dwItemMaxID, row[0]);
		}
	}

	if (dwItemMaxID == 0)
		dwItemMaxID = dwMin;
	else
		dwItemMaxID++;
	
	if ((dwMax < dwItemMaxID) || (dwMax - dwItemMaxID < cs_dwMinimumRemainCount))
	{
		metin2::log::info( "ItemIDRange: Build {:d} ~ {:d} start: {:d}\tNOT USE remain count is below {:d}",
			   	dwMin, dwMax, dwItemMaxID, cs_dwMinimumRemainCount);
	}
	else
	{
		range.dwMin = dwMin;
		range.dwMax = dwMax;
		range.dwUsableItemIDMin = dwItemMaxID;

		snprintf(szQuery, sizeof(szQuery), "SELECT COUNT(*) FROM item%s WHERE id >= %u AND id <= %u", 
				GetTablePostfix(), range.dwUsableItemIDMin, range.dwMax);

		pMsg.reset(CDBManager::instance().DirectQuery(szQuery));

		if (pMsg != nullptr)
		{
			if (pMsg->Get()->uiNumRows > 0)
			{
				uint32_t count = 0;
				row = mysql_fetch_row(pMsg->Get()->pSQLResult);
				str_to_number(count, row[0]);

				if (count > 0)
				{
					metin2::log::error("ItemIDRange: Build: {:d} ~ {:d}\thave a item", range.dwUsableItemIDMin, range.dwMax);
					return false;
				}
				else
				{
					metin2::log::info( "ItemIDRange: Build: {:d} ~ {:d} start:{:d}", range.dwMin, range.dwMax, range.dwUsableItemIDMin);
					return true;
				}
			}
		}
	}

	return false;
}

void CItemIDRangeManager::UpdateRange(uint32_t dwMin, uint32_t dwMax)
{
	TItemIDRangeTable range;

	if (BuildRange(dwMin, dwMax, range) == true)
	{
		m_listData.push_back(range);
	}
}
}
