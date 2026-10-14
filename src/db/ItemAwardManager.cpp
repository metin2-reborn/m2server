#include "ItemAwardManager.h"

#include "ClientManager.h"
#include "DBManager.h"
#include "QID.h"

#include <common/length.h>
#include <common/tables.h>
#include <common/utils.h>
#include <libsql/AsyncSQL.h>
#include <libthecore/log.h>
#include <libthecore/utils.h>

#include <mysql.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <utility>

#include <sys/types.h>



uint32_t g_dwLastCachedItemAwardID = 0;
ItemAwardManager::ItemAwardManager()
{
}

ItemAwardManager::~ItemAwardManager()
{
}

void ItemAwardManager::RequestLoad()
{
	char szQuery[QUERY_MAX_LEN];
	snprintf(szQuery, sizeof(szQuery), "SELECT id,login,vnum,count,socket0,socket1,socket2,mall,why FROM item_award WHERE taken_time IS NULL and id > %d", g_dwLastCachedItemAwardID);
	CDBManager::instance().ReturnQuery(szQuery, QID_ITEM_AWARD_LOAD, 0, nullptr);
}

void ItemAwardManager::Load(SQLMsg * pMsg)
{
	MYSQL_RES * pRes = pMsg->Get()->pSQLResult;

	for (uint i = 0; i < pMsg->Get()->uiNumRows; ++i)
	{
		MYSQL_ROW row = mysql_fetch_row(pRes);
		int col = 0;

		uint32_t dwID = 0;
		str_to_number(dwID, row[col++]);

		if (m_map_award.find(dwID) != m_map_award.end())
			continue;

		TItemAward * kData = new TItemAward;
		memset(kData, 0, sizeof(TItemAward));

		kData->dwID	= dwID;
		trim_and_lower(row[col++], kData->szLogin, sizeof(kData->szLogin));
		str_to_number(kData->dwVnum, row[col++]);
		str_to_number(kData->dwCount, row[col++]);
		str_to_number(kData->dwSocket0, row[col++]);
		str_to_number(kData->dwSocket1, row[col++]);
		str_to_number(kData->dwSocket2, row[col++]);
		str_to_number(kData->bMall, row[col++]);

		if (row[col])
		{
			strlcpy(kData->szWhy, row[col], sizeof(kData->szWhy));
			//If the why column changes during the game				
			char* whyStr = kData->szWhy;	//Read the why column
			char cmdStr[100] = "";	//Copy the value read from the why column into a temporary string
			strcpy(cmdStr,whyStr);	//because tokenizing while getting the command would tokenize the original too
			char command[20] = "";
			strcpy(command,CClientManager::instance().GetCommand(cmdStr));	// Get the command
			if( !(strcmp(command,"GIFT") ))	// If the command is GIFT
			{
				TPacketItemAwardInfromer giftData;
				strcpy(giftData.login, kData->szLogin);	//Copy the login ID
				strcpy(giftData.command, command);					//Copy the command
				giftData.vnum = kData->dwVnum;				//Also copy the item vnum
				CClientManager::instance().ForwardPacket(HEADER_DG_ITEMAWARD_INFORMER,&giftData,sizeof(TPacketItemAwardInfromer));
			}
		}

		m_map_award.insert(std::make_pair(dwID, kData));

		printf("ITEM_AWARD load id %u bMall %d \n", kData->dwID, kData->bMall);
		sys_log(0, "ITEM_AWARD: load id %u login %s vnum %u count %u socket %u", kData->dwID, kData->szLogin, kData->dwVnum, kData->dwCount, kData->dwSocket0);
		std::set<TItemAward *> & kSet = m_map_kSetAwardByLogin[kData->szLogin];
		kSet.insert(kData);

		if (dwID > g_dwLastCachedItemAwardID)
			g_dwLastCachedItemAwardID = dwID;
	}
}

std::set<TItemAward *> * ItemAwardManager::GetByLogin(const char * c_pszLogin)
{
	auto it = m_map_kSetAwardByLogin.find(c_pszLogin);

	if (it == m_map_kSetAwardByLogin.end())
		return nullptr;

	return &it->second;
}

void ItemAwardManager::Taken(uint32_t dwAwardID, uint32_t dwItemID)
{
	auto it = m_map_award.find(dwAwardID);

	if (it == m_map_award.end())
	{
		sys_log(0, "ITEM_AWARD: Taken ID not exist %u", dwAwardID);
		return;
	}

	TItemAward * k = it->second;
	k->bTaken = true;

	//
	// Update taken_time in database to prevent not to give him again.
	// 
	char szQuery[QUERY_MAX_LEN];

	snprintf(szQuery, sizeof(szQuery), 
			"UPDATE item_award SET taken_time=NOW(),item_id=%u WHERE id=%u AND taken_time IS NULL", 
			dwItemID, dwAwardID);

	CDBManager::instance().ReturnQuery(szQuery, QID_ITEM_AWARD_TAKEN, 0, nullptr);
}

std::map<uint32_t, TItemAward *>& ItemAwardManager::GetMapAward()
{
	return m_map_award;
}

std::map<std::string, std::set<TItemAward *> >& ItemAwardManager::GetMapkSetAwardByLogin()
{
	return m_map_kSetAwardByLogin;
}