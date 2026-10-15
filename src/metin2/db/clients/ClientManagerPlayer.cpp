
#include <metin2/db/caches/Cache.h>
#include <metin2/db/clients/ClientManager.h>
#include <metin2/db/storage/DBManager.h>
#include <metin2/db/items/ItemAwardManager.h>
#include <metin2/db/login/LoginData.h>
#include <metin2/db/app/Main.h>
#include <metin2/db/clients/Peer.h>
#include <metin2/db/clients/QID.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/common/utils.h>
#include <metin2/sql/AsyncSQL.h>
#include <metin2/sql/query.h>
#include <metin2/config/config.h>
#include <metin2/log/log.h>

#include <mysql.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace metin2::db::inline clients {



//
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!!!!!!!!!! IMPORTANT !!!!!!!!!!!!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//
// Check all SELECT syntax on item table before change this function!!!
//
bool CreateItemTableFromRes(MYSQL_RES * res, std::vector<TPlayerItem> * pVec, uint32_t dwPID)
{
	if (!res)
	{
		pVec->clear();
		return true;
	}

	int rows;

	if ((rows = mysql_num_rows(res)) <= 0)	// No data
	{
		pVec->clear();
		return true;
	}

	pVec->resize(rows);

	for (int i = 0; i < rows; ++i)
	{
		MYSQL_ROW row = mysql_fetch_row(res);
		TPlayerItem & item = pVec->at(i);

		int cur = 0;

		// Check all SELECT syntax on item table before change this function!!!
		// Check all SELECT syntax on item table before change this function!!!
		// Check all SELECT syntax on item table before change this function!!!
		str_to_number(item.id, row[cur++]);
		str_to_number(item.window, row[cur++]);
		str_to_number(item.pos, row[cur++]);
		str_to_number(item.count, row[cur++]);
		str_to_number(item.vnum, row[cur++]);
		str_to_number(item.alSockets[0], row[cur++]);
		str_to_number(item.alSockets[1], row[cur++]);
		str_to_number(item.alSockets[2], row[cur++]);

		for (int j = 0; j < ITEM_ATTRIBUTE_MAX_NUM; j++)
		{
			str_to_number(item.aAttr[j].bType, row[cur++]);
			str_to_number(item.aAttr[j].sValue, row[cur++]);
		}

		item.owner		= dwPID;
	}

	return true;
}

std::string CreatePlayerSaveQuery(const TPlayerTable * pkTab)
{
	return sql::format(
			"UPDATE player{} SET "
			"job = {}, "
			"voice = {}, "
			"dir = {}, "
			"x = {}, "
			"y = {}, "
			"z = {}, "
			"map_index = {}, "
			"exit_x = {}, "
			"exit_y = {}, "
			"exit_map_index = {}, "
			"hp = {}, "
			"mp = {}, "
			"stamina = {}, "
			"random_hp = {}, "
			"random_sp = {}, "
			"playtime = {}, "
			"level = {}, "
			"level_step = {}, "
			"st = {}, "
			"ht = {}, "
			"dx = {}, "
			"iq = {}, "
			"gold = {}, "
			"exp = {}, "
			"stat_point = {}, "
			"skill_point = {}, "
			"sub_skill_point = {}, "
			"stat_reset_count = {}, "
			"ip = {}, "
			"part_main = {}, "
			"part_hair = {}, "
			"last_play = NOW(), "
			"skill_group = {}, "
			"alignment = {}, "
			"horse_level = {}, "
			"horse_riding = {}, "
			"horse_hp = {}, "
			"horse_hp_droptime = {}, "
			"horse_stamina = {}, "
			"horse_skill_point = {}, "
			"skill_level = {}, "
			"quickslot = {} "
			" WHERE id={}",
		sql::raw(GetTablePostfix()),
		pkTab->job,
		pkTab->voice,
		pkTab->dir,
		pkTab->x,
		pkTab->y,
		pkTab->z,
		pkTab->lMapIndex,
		pkTab->lExitX,
		pkTab->lExitY,
		pkTab->lExitMapIndex,
		pkTab->hp,
		pkTab->sp,
		pkTab->stamina,
		pkTab->sRandomHP,
		pkTab->sRandomSP,
		pkTab->playtime,
		pkTab->level,
		pkTab->level_step,
		pkTab->st,
		pkTab->ht,
		pkTab->dx,
		pkTab->iq,
		pkTab->gold,
		pkTab->exp,
		pkTab->stat_point,
		pkTab->skill_point,
		pkTab->sub_skill_point,
		pkTab->stat_reset_count,
		pkTab->ip,
		pkTab->parts[PART_MAIN],
		pkTab->parts[PART_HAIR],
		pkTab->skill_group,
		pkTab->lAlignment,
		pkTab->horse.bLevel,
		pkTab->horse.bRiding,
		pkTab->horse.sHealth,
		pkTab->horse.dwHorseHealthDropTime,
		pkTab->horse.sStamina,
		pkTab->horse_skill_point,
		std::string_view(reinterpret_cast<const char *>(pkTab->skills), sizeof(pkTab->skills)),
		std::string_view(reinterpret_cast<const char *>(pkTab->quickslot), sizeof(pkTab->quickslot)),
		pkTab->id);
}

CPlayerTableCache * CClientManager::GetPlayerCache(uint32_t id)
{
	TPlayerTableCacheMap::iterator it = m_map_playerCache.find(id);

	if (it == m_map_playerCache.end())
		return nullptr;

	TPlayerTable* pTable = it->second->Get(false);
	pTable->logoff_interval = GetCurrentTime() - it->second->GetLastUpdateTime();
	return it->second;
}

void CClientManager::PutPlayerCache(TPlayerTable * pNew)
{
	CPlayerTableCache * c;

	c = GetPlayerCache(pNew->id);

	if (!c)
	{
		c = new CPlayerTableCache;
		m_map_playerCache.insert(TPlayerTableCacheMap::value_type(pNew->id, c));
	}

	c->Put(pNew);
}

/*
 * PLAYER LOAD
 */
void CClientManager::QUERY_PLAYER_LOAD(CPeer * peer, uint32_t dwHandle, TPlayerLoadPacket * packet)
{
	CPlayerTableCache * c;
	TPlayerTable * pTab;
	
	//
	// Cache all characters belonging to an account
	//
	CLoginData * pLoginData = GetLoginDataByAID(packet->account_id);

	if (pLoginData)
	{
		for (int n = 0; n < PLAYER_PER_ACCOUNT; ++n)
			if (pLoginData->GetAccountRef().players[n].dwID != 0)
				DeleteLogoutPlayer(pLoginData->GetAccountRef().players[n].dwID);
	}

	//----------------------------------------------------------------
	// 1. User info exists in DBCache: from DBCache 
	// 2. User info not in DBCache: from DB 
	// ---------------------------------------------------------------
	
	//----------------------------------
	// 1. User info exists in DBCache: from DBCache 
	//----------------------------------
	if ((c = GetPlayerCache(packet->player_id)))
	{
		CLoginData * pkLD = GetLoginDataByAID(packet->account_id);

		if (!pkLD || pkLD->IsPlay())
		{
			metin2::log::info( "PLAYER_LOAD_ERROR: LoginData {} IsPlay {:d}", static_cast<const void *>(pkLD), pkLD ? pkLD->IsPlay() : 0);
			peer->EncodeHeader(HEADER_DG_PLAYER_LOAD_FAILED, dwHandle, 0); 
			return;
		}

		pTab = c->Get();

		pkLD->SetPlay(true);
		std::memcpy(pTab->aiPremiumTimes, pkLD->GetPremiumPtr(), sizeof(pTab->aiPremiumTimes));

		peer->EncodeHeader(HEADER_DG_PLAYER_LOAD_SUCCESS, dwHandle, sizeof(TPlayerTable));
		peer->Encode(pTab, sizeof(TPlayerTable));

		if (packet->player_id != pkLD->GetLastPlayerID())
		{
			TPacketNeedLoginLogInfo logInfo;
			logInfo.dwPlayerID = packet->player_id;

			pkLD->SetLastPlayerID( packet->player_id );

			peer->EncodeHeader( HEADER_DG_NEED_LOGIN_LOG, dwHandle, sizeof(TPacketNeedLoginLogInfo) );
			peer->Encode( &logInfo, sizeof(TPacketNeedLoginLogInfo) );
		}

		char szQuery[1024] = { 0, };

		TItemCacheSet * pSet = GetItemCacheSet(pTab->id);

		metin2::log::info( "[PLAYER_LOAD] ID {} pid {:d} gold {:d} ", pTab->name, pTab->id, pTab->gold);

		//--------------------------------------------
		// Loading items & AFFECT & QUEST: 
		//--------------------------------------------
		// 1) Items exist in DBCache: fetch from DBCache
		// 2) Items not in DBCache: fetch from DB 

		/////////////////////////////////////////////
		// 1) Items exist in DBCache: fetch from DBCache
		/////////////////////////////////////////////
		if (pSet)
		{
			static std::vector<TPlayerItem> s_items;
			s_items.resize(pSet->size());

			uint32_t dwCount = 0;
			TItemCacheSet::iterator it = pSet->begin();

			while (it != pSet->end())
			{
				CItemCache * c = *it++;
				TPlayerItem * p = c->Get();

				if (p->vnum) // No vnum means a deleted item.
					std::memcpy(&s_items[dwCount++], p, sizeof(TPlayerItem));
			}

			if (metin2::config::test_server())
				metin2::log::info( "ITEM_CACHE: HIT! {} count: {:d}", pTab->name, dwCount);

			peer->EncodeHeader(HEADER_DG_ITEM_LOAD, dwHandle, sizeof(uint32_t) + sizeof(TPlayerItem) * dwCount);
			peer->EncodeDWORD(dwCount);

			if (dwCount)
				peer->Encode(&s_items[0], sizeof(TPlayerItem) * dwCount);

			// Quest
			snprintf(szQuery, sizeof(szQuery),
					"SELECT dwPID,szName,szState,lValue FROM quest%s WHERE dwPID=%d AND lValue<>0",
					GetTablePostfix(), pTab->id);
			
			CDBManager::instance().ReturnQuery(szQuery, QID_QUEST, peer->GetHandle(), new ClientHandleInfo(dwHandle,0,packet->account_id));

			// Affect
			snprintf(szQuery, sizeof(szQuery),
					"SELECT dwPID,bType,bApplyOn,lApplyValue,dwFlag,lDuration,lSPCost FROM affect%s WHERE dwPID=%d",
					GetTablePostfix(), pTab->id);
			CDBManager::instance().ReturnQuery(szQuery, QID_AFFECT, peer->GetHandle(), new ClientHandleInfo(dwHandle));
		}
		/////////////////////////////////////////////
		// 2) Items not in DBCache: fetch from DB 
		/////////////////////////////////////////////
		else
		{
			snprintf(szQuery, sizeof(szQuery), 
					"SELECT id,window+0,pos,count,vnum,socket0,socket1,socket2,attrtype0,attrvalue0,attrtype1,attrvalue1,attrtype2,attrvalue2,attrtype3,attrvalue3,attrtype4,attrvalue4,attrtype5,attrvalue5,attrtype6,attrvalue6 "
					"FROM item%s WHERE owner_id=%d AND (window < %d or window = %d)",
					GetTablePostfix(), pTab->id, SAFEBOX, DRAGON_SOUL_INVENTORY);

			CDBManager::instance().ReturnQuery(szQuery,
					QID_ITEM,
					peer->GetHandle(),
					new ClientHandleInfo(dwHandle, pTab->id));
			snprintf(szQuery, sizeof(szQuery), 
					"SELECT dwPID, szName, szState, lValue FROM quest%s WHERE dwPID=%d",
					GetTablePostfix(), pTab->id);

			CDBManager::instance().ReturnQuery(szQuery,
					QID_QUEST,
					peer->GetHandle(),
					new ClientHandleInfo(dwHandle, pTab->id));
			snprintf(szQuery, sizeof(szQuery), 
					"SELECT dwPID, bType, bApplyOn, lApplyValue, dwFlag, lDuration, lSPCost FROM affect%s WHERE dwPID=%d",
					GetTablePostfix(), pTab->id);

			CDBManager::instance().ReturnQuery(szQuery,
					QID_AFFECT,
					peer->GetHandle(),
					new ClientHandleInfo(dwHandle, pTab->id));
		}
		//ljw
	}
	//----------------------------------
	// 2. User info not in DBCache: from DB 
	//----------------------------------
	else
	{
		metin2::log::info( "[PLAYER_LOAD] Load from PlayerDB pid[{:d}]", packet->player_id);

		char queryStr[QUERY_MAX_LEN];

		//--------------------------------------------------------------
		// Get character info: always from DB 
		//--------------------------------------------------------------
		snprintf(queryStr, sizeof(queryStr),
				"SELECT "
				"id,name,job,voice,dir,x,y,z,map_index,exit_x,exit_y,exit_map_index,hp,mp,stamina,random_hp,random_sp,playtime,"
				"gold,level,level_step,st,ht,dx,iq,exp,"
				"stat_point,skill_point,sub_skill_point,stat_reset_count,part_base,part_hair,"
				"skill_level,quickslot,skill_group,alignment,mobile,horse_level,horse_riding,horse_hp,horse_hp_droptime,horse_stamina,"
				"UNIX_TIMESTAMP(NOW())-UNIX_TIMESTAMP(last_play),horse_skill_point FROM player%s WHERE id=%d",
				GetTablePostfix(), packet->player_id);

		ClientHandleInfo * pkInfo = new ClientHandleInfo(dwHandle, packet->player_id);
		pkInfo->account_id = packet->account_id;
		CDBManager::instance().ReturnQuery(queryStr, QID_PLAYER, peer->GetHandle(), pkInfo);

		//--------------------------------------------------------------
		// Fetch items 
		//--------------------------------------------------------------
		snprintf(queryStr, sizeof(queryStr),
				"SELECT id,window+0,pos,count,vnum,socket0,socket1,socket2,attrtype0,attrvalue0,attrtype1,attrvalue1,attrtype2,attrvalue2,attrtype3,attrvalue3,attrtype4,attrvalue4,attrtype5,attrvalue5,attrtype6,attrvalue6 "
				"FROM item%s WHERE owner_id=%d AND (window < %d or window = %d)",
				GetTablePostfix(), packet->player_id, SAFEBOX, DRAGON_SOUL_INVENTORY);
		CDBManager::instance().ReturnQuery(queryStr, QID_ITEM, peer->GetHandle(), new ClientHandleInfo(dwHandle, packet->player_id));

		//--------------------------------------------------------------
		// Fetch QUEST 
		//--------------------------------------------------------------
		snprintf(queryStr, sizeof(queryStr),
				"SELECT dwPID,szName,szState,lValue FROM quest%s WHERE dwPID=%d",
				GetTablePostfix(), packet->player_id);
		CDBManager::instance().ReturnQuery(queryStr, QID_QUEST, peer->GetHandle(), new ClientHandleInfo(dwHandle, packet->player_id,packet->account_id));
		//Also pass account id so the German gift feature can get login info from the item_award table
		//--------------------------------------------------------------
		// Fetch AFFECT 
		//--------------------------------------------------------------
		snprintf(queryStr, sizeof(queryStr),
				"SELECT dwPID,bType,bApplyOn,lApplyValue,dwFlag,lDuration,lSPCost FROM affect%s WHERE dwPID=%d",
				GetTablePostfix(), packet->player_id);
		CDBManager::instance().ReturnQuery(queryStr, QID_AFFECT, peer->GetHandle(), new ClientHandleInfo(dwHandle, packet->player_id));
	}
	
	
}
void CClientManager::ItemAward(CPeer * peer,char* login)
{
	std::string login_t = login;
	std::set<TItemAward *> * pSet = ItemAwardManager::instance().GetByLogin(login_t.c_str());	
	if(pSet == nullptr)
		return;
	__typeof(pSet->begin()) it = pSet->begin();	//Read those whose taken_time is NULL	
	while(it != pSet->end() )
	{				
		TItemAward * pItemAward = *(it++);
		InformGiftAward(pItemAward->szLogin, pItemAward->szWhy, pItemAward->dwVnum);
	}
}

void CClientManager::InformGiftAward(const char * login, const char * why, uint32_t vnum)
{
	// The command is the text between a leading '[' and the first ']' (or the end).
	const std::string_view text(why);
	if (!text.starts_with("[GIFT") || (text.size() > 5 && text[5] != ']'))
		return;

	TPacketItemAwardInfromer giftData{};
	strlcpy(giftData.login, login, sizeof(giftData.login));
	strlcpy(giftData.command, "GIFT", sizeof(giftData.command));
	giftData.vnum = vnum;
	ForwardPacket(HEADER_DG_ITEMAWARD_INFORMER, &giftData, sizeof(giftData));
}

bool CreatePlayerTableFromRes(MYSQL_RES * res, TPlayerTable * pkTab)
{
	if (mysql_num_rows(res) == 0)	// No data
		return false;

	memset(pkTab, 0, sizeof(TPlayerTable));

	MYSQL_ROW row = mysql_fetch_row(res);

	int	col = 0;

	// "id,name,job,voice,dir,x,y,z,map_index,exit_x,exit_y,exit_map_index,hp,mp,stamina,random_hp,random_sp,playtime,"
	// "gold,level,level_step,st,ht,dx,iq,exp,"
	// "stat_point,skill_point,sub_skill_point,stat_reset_count,part_base,part_hair,"
	// "skill_level,quickslot,skill_group,alignment,mobile,horse_level,horse_riding,horse_hp,horse_stamina FROM player%s WHERE id=%d",
	str_to_number(pkTab->id, row[col++]);
	strlcpy(pkTab->name, row[col++], sizeof(pkTab->name));
	str_to_number(pkTab->job, row[col++]);
	str_to_number(pkTab->voice, row[col++]);
	str_to_number(pkTab->dir, row[col++]);
	str_to_number(pkTab->x, row[col++]);
	str_to_number(pkTab->y, row[col++]);
	str_to_number(pkTab->z, row[col++]);
	str_to_number(pkTab->lMapIndex, row[col++]);
	str_to_number(pkTab->lExitX, row[col++]);
	str_to_number(pkTab->lExitY, row[col++]);
	str_to_number(pkTab->lExitMapIndex,  row[col++]);
	str_to_number(pkTab->hp, row[col++]);
	str_to_number(pkTab->sp, row[col++]);
	str_to_number(pkTab->stamina, row[col++]);
	str_to_number(pkTab->sRandomHP, row[col++]);
	str_to_number(pkTab->sRandomSP, row[col++]);
	str_to_number(pkTab->playtime, row[col++]);
	str_to_number(pkTab->gold, row[col++]);
	str_to_number(pkTab->level, row[col++]);
	str_to_number(pkTab->level_step, row[col++]);
	str_to_number(pkTab->st, row[col++]);
	str_to_number(pkTab->ht, row[col++]);
	str_to_number(pkTab->dx, row[col++]);
	str_to_number(pkTab->iq, row[col++]);
	str_to_number(pkTab->exp, row[col++]);
	str_to_number(pkTab->stat_point, row[col++]);
	str_to_number(pkTab->skill_point, row[col++]);
	str_to_number(pkTab->sub_skill_point, row[col++]);
	str_to_number(pkTab->stat_reset_count, row[col++]);
	str_to_number(pkTab->part_base, row[col++]);
	str_to_number(pkTab->parts[PART_HAIR], row[col++]);

	if (row[col])
		std::memcpy(pkTab->skills, row[col], sizeof(pkTab->skills));
	else
		memset(&pkTab->skills, 0, sizeof(pkTab->skills));

	col++;

	if (row[col])
		std::memcpy(pkTab->quickslot, row[col], sizeof(pkTab->quickslot));
	else
		memset(pkTab->quickslot, 0, sizeof(pkTab->quickslot));

	col++;

	str_to_number(pkTab->skill_group, row[col++]);
	str_to_number(pkTab->lAlignment, row[col++]);

	if (row[col])
	{
		strlcpy(pkTab->szMobile, row[col], sizeof(pkTab->szMobile));
	}

	col++;

	str_to_number(pkTab->horse.bLevel, row[col++]);
	str_to_number(pkTab->horse.bRiding, row[col++]);
	str_to_number(pkTab->horse.sHealth, row[col++]);
	str_to_number(pkTab->horse.dwHorseHealthDropTime, row[col++]);
	str_to_number(pkTab->horse.sStamina, row[col++]);
	str_to_number(pkTab->logoff_interval, row[col++]);
	str_to_number(pkTab->horse_skill_point, row[col++]);

	// reset sub_skill_point
	{
		pkTab->skills[123].bLevel = 0; // SKILL_CREATE

		if (pkTab->level > 9)
		{
			int max_point = pkTab->level - 9;

			int skill_point = 
				std::min<int>(20, pkTab->skills[121].bLevel) +	// SKILL_LEADERSHIP			Leadership
				std::min<int>(20, pkTab->skills[124].bLevel) +	// SKILL_MINING				Mining
				std::min<int>(10, pkTab->skills[131].bLevel) +	// SKILL_HORSE_SUMMON		Horse summon
				std::min<int>(20, pkTab->skills[141].bLevel) +	// SKILL_ADD_HP				HP boost
				std::min<int>(20, pkTab->skills[142].bLevel);		// SKILL_RESIST_PENETRATE	Penetration resistance

			pkTab->sub_skill_point = max_point - skill_point;
		}
		else
			pkTab->sub_skill_point = 0;
	}

	return true;
}

void CClientManager::RESULT_COMPOSITE_PLAYER(CPeer * peer, SQLMsg * pMsg, uint32_t dwQID)
{
	CQueryInfo * qi = (CQueryInfo *) pMsg->pvUserData;
	std::unique_ptr<ClientHandleInfo> info((ClientHandleInfo *) qi->pvData);
	
	MYSQL_RES * pSQLResult = pMsg->Get()->pSQLResult;
	if (!pSQLResult)
	{
		metin2::log::error("null MYSQL_RES QID {:d}", dwQID);
		return;
	}

	switch (dwQID)
	{
		case QID_PLAYER:
			metin2::log::info( "QID_PLAYER {:d} {:d}", info->dwHandle, info->player_id);
			RESULT_PLAYER_LOAD(peer, pSQLResult, info.get());

			break;

		case QID_ITEM:
			metin2::log::info( "QID_ITEM {:d}", info->dwHandle);
			RESULT_ITEM_LOAD(peer, pSQLResult, info->dwHandle, info->player_id);
			break;

		case QID_QUEST:
			{
				metin2::log::info( "QID_QUEST {:d}", info->dwHandle);
				RESULT_QUEST_LOAD(peer, pSQLResult, info->dwHandle, info->player_id);
				//Get aid
				ClientHandleInfo*  temp1 = info.get();
				if (temp1 == nullptr)
					break;
				
				CLoginData* pLoginData1 = GetLoginDataByAID(temp1->account_id);	//				
				//German gift feature
				if (pLoginData1 == nullptr)
					break;
				metin2::log::info("info of pLoginData1 before call ItemAwardfunction {}",static_cast<const void *>(static_cast<void*>(pLoginData1)));
				ItemAward(peer,pLoginData1->GetAccountRef().login);
			}
			break;

		case QID_AFFECT:
			metin2::log::info( "QID_AFFECT {:d}", info->dwHandle);
			RESULT_AFFECT_LOAD(peer, pSQLResult, info->dwHandle);
			break;
	}
	
}

void CClientManager::RESULT_PLAYER_LOAD(CPeer * peer, MYSQL_RES * pRes, ClientHandleInfo * pkInfo)
{
	TPlayerTable tab;

	if (!CreatePlayerTableFromRes(pRes, &tab))
	{
		peer->EncodeHeader(HEADER_DG_PLAYER_LOAD_FAILED, pkInfo->dwHandle, 0); 
		return;
	}

	CLoginData * pkLD = GetLoginDataByAID(pkInfo->account_id);
	
	if (!pkLD || pkLD->IsPlay())
	{
		metin2::log::info( "PLAYER_LOAD_ERROR: LoginData {} IsPlay {:d}", static_cast<const void *>(pkLD), pkLD ? pkLD->IsPlay() : 0);
		peer->EncodeHeader(HEADER_DG_PLAYER_LOAD_FAILED, pkInfo->dwHandle, 0); 
		return;
	}

	pkLD->SetPlay(true);
	std::memcpy(tab.aiPremiumTimes, pkLD->GetPremiumPtr(), sizeof(tab.aiPremiumTimes));

	peer->EncodeHeader(HEADER_DG_PLAYER_LOAD_SUCCESS, pkInfo->dwHandle, sizeof(TPlayerTable));
	peer->Encode(&tab, sizeof(TPlayerTable));

	if (tab.id != pkLD->GetLastPlayerID())
	{
		TPacketNeedLoginLogInfo logInfo;
		logInfo.dwPlayerID = tab.id;

		pkLD->SetLastPlayerID( tab.id );

		peer->EncodeHeader( HEADER_DG_NEED_LOGIN_LOG, pkInfo->dwHandle, sizeof(TPacketNeedLoginLogInfo) );
		peer->Encode( &logInfo, sizeof(TPacketNeedLoginLogInfo) );
	}
}

void CClientManager::RESULT_ITEM_LOAD(CPeer * peer, MYSQL_RES * pRes, uint32_t dwHandle, uint32_t dwPID)
{
	static std::vector<TPlayerItem> s_items;
	//Read item info from DB.
	CreateItemTableFromRes(pRes, &s_items, dwPID);
	uint32_t dwCount = s_items.size();

	peer->EncodeHeader(HEADER_DG_ITEM_LOAD, dwHandle, sizeof(uint32_t) + sizeof(TPlayerItem) * dwCount);
	peer->EncodeDWORD(dwCount);

	//Create the CacheSet  
	CreateItemCacheSet(dwPID);

	// ITEM_LOAD_LOG_ATTACH_PID
	metin2::log::info( "ITEM_LOAD: count {:d} pid {:d}", dwCount, dwPID);
	// END_OF_ITEM_LOAD_LOG_ATTACH_PID

	if (dwCount)
	{
		peer->Encode(&s_items[0], sizeof(TPlayerItem) * dwCount);

		for (uint32_t i = 0; i < dwCount; ++i)
			PutItemCache(&s_items[i], true); // Loaded data doesn't need to be saved separately, so pass true for bSkipQuery.
	}
}

void CClientManager::RESULT_AFFECT_LOAD(CPeer * peer, MYSQL_RES * pRes, uint32_t dwHandle)
{
	int iNumRows;

	if ((iNumRows = mysql_num_rows(pRes)) == 0) // No data
		return;

	static std::vector<TPacketAffectElement> s_elements;
	s_elements.resize(iNumRows);

	uint32_t dwPID = 0;

	MYSQL_ROW row;

	for (int i = 0; i < iNumRows; ++i)
	{
		TPacketAffectElement & r = s_elements[i];
		row = mysql_fetch_row(pRes);

		if (dwPID == 0)
			str_to_number(dwPID, row[0]);

		str_to_number(r.dwType, row[1]);
		str_to_number(r.bApplyOn, row[2]);
		str_to_number(r.lApplyValue, row[3]);
		str_to_number(r.dwFlag, row[4]);
		str_to_number(r.lDuration, row[5]);
		str_to_number(r.lSPCost, row[6]);
	}

	metin2::log::info( "AFFECT_LOAD: count {:d} PID {:d}", s_elements.size(), dwPID);

	uint32_t dwCount = s_elements.size();

	peer->EncodeHeader(HEADER_DG_AFFECT_LOAD, dwHandle, sizeof(uint32_t) + sizeof(uint32_t) + sizeof(TPacketAffectElement) * dwCount);
	peer->Encode(&dwPID, sizeof(uint32_t));
	peer->Encode(&dwCount, sizeof(uint32_t));
	peer->Encode(&s_elements[0], sizeof(TPacketAffectElement) * dwCount);
}

void CClientManager::RESULT_QUEST_LOAD(CPeer * peer, MYSQL_RES * pRes, uint32_t dwHandle, uint32_t pid)
{
	int iNumRows;

	if ((iNumRows = mysql_num_rows(pRes)) == 0)
	{
		uint32_t dwCount = 0; 
		peer->EncodeHeader(HEADER_DG_QUEST_LOAD, dwHandle, sizeof(uint32_t));
		peer->Encode(&dwCount, sizeof(uint32_t));
		return;
	}

	static std::vector<TQuestTable> s_table;
	s_table.resize(iNumRows);

	MYSQL_ROW row;

	for (int i = 0; i < iNumRows; ++i)
	{
		TQuestTable & r = s_table[i];

		row = mysql_fetch_row(pRes);

		str_to_number(r.dwPID, row[0]);
		strlcpy(r.szName, row[1], sizeof(r.szName));
		strlcpy(r.szState, row[2], sizeof(r.szState));
		str_to_number(r.lValue, row[3]);
	}

	metin2::log::info( "QUEST_LOAD: count {:d} PID {:d}", s_table.size(), s_table[0].dwPID);

	uint32_t dwCount = s_table.size();

	peer->EncodeHeader(HEADER_DG_QUEST_LOAD, dwHandle, sizeof(uint32_t) + sizeof(TQuestTable) * dwCount);
	peer->Encode(&dwCount, sizeof(uint32_t));
	peer->Encode(&s_table[0], sizeof(TQuestTable) * dwCount);
}

/*
 * PLAYER SAVE
 */
void CClientManager::QUERY_PLAYER_SAVE(CPeer * peer, uint32_t dwHandle, TPlayerTable * pkTab)
{
	if (metin2::config::test_server())
		metin2::log::info( "PLAYER_SAVE: {}", pkTab->name);

	PutPlayerCache(pkTab);
}

typedef std::map<uint32_t, time_t> time_by_id_map_t;
static time_by_id_map_t s_createTimeByAccountID;

/*
 * PLAYER CREATE
 */
void CClientManager::__QUERY_PLAYER_CREATE(CPeer *peer, uint32_t dwHandle, TPlayerCreatePacket* packet)
{
	int		player_id;

	// Cannot create a character within X seconds on one account.
	time_by_id_map_t::iterator it = s_createTimeByAccountID.find(packet->account_id);

	if (it != s_createTimeByAccountID.end())
	{
		time_t curtime = time(nullptr);

		if (curtime - it->second < 30)
		{
			peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
			return;
		}
	}

	const auto postfix = sql::raw(GetTablePostfix());

	std::string query = sql::format(
			"SELECT pid{} FROM player_index{} WHERE id={}", packet->account_index + 1, postfix, packet->account_id);

	std::unique_ptr<SQLMsg> pMsg0(CDBManager::instance().DirectQuery(query.c_str()));

	if (pMsg0->Get()->uiNumRows != 0)
	{
		if (!pMsg0->Get()->pSQLResult)
		{
			peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
			return;
		}

		MYSQL_ROW row = mysql_fetch_row(pMsg0->Get()->pSQLResult);

		uint32_t dwPID = 0; str_to_number(dwPID, row[0]);
		if (row[0] && dwPID > 0)
		{
			peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_ALREADY, dwHandle, 0);
			metin2::log::info( "ALREADY EXIST AccountChrIdx {:d} ID {:d}", packet->account_index, dwPID);
			return;
		}
	}
	else
	{
		peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
		return;
	}

	query = sql::format(
			"SELECT COUNT(*) as count FROM player{} WHERE name={}", postfix, packet->player_table.name);

	std::unique_ptr<SQLMsg> pMsg1(CDBManager::instance().DirectQuery(query.c_str()));

	if (pMsg1->Get()->uiNumRows)
	{
		if (!pMsg1->Get()->pSQLResult)
		{
			peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
			return;
		}

		MYSQL_ROW row = mysql_fetch_row(pMsg1->Get()->pSQLResult);

		if (*row[0] != '0')
		{
			metin2::log::info( "ALREADY EXIST name {}, row[0] {} query {}", packet->player_table.name, row[0], query.c_str());
			peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_ALREADY, dwHandle, 0);
			return;
		}
	}
	else
	{
		peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
		return;
	}

	query = sql::format(
			"INSERT INTO player{} "
			"(id, account_id, name, level, st, ht, dx, iq, "
			"job, voice, dir, x, y, z, "
			"hp, mp, random_hp, random_sp, stat_point, stamina, part_base, part_main, part_hair, gold, playtime, "
			"skill_level, quickslot) "
			"VALUES(0, {}, {}, {}, {}, {}, {}, {}, "
			"{}, {}, {}, {}, {}, {}, {}, "
			"{}, {}, {}, {}, {}, {}, {}, 0, {}, 0, "
			"{}, {})",
			postfix,
			packet->account_id, packet->player_table.name, packet->player_table.level, packet->player_table.st, packet->player_table.ht, packet->player_table.dx, packet->player_table.iq,
			packet->player_table.job, packet->player_table.voice, packet->player_table.dir, packet->player_table.x, packet->player_table.y, packet->player_table.z,
			packet->player_table.hp, packet->player_table.sp, packet->player_table.sRandomHP, packet->player_table.sRandomSP, packet->player_table.stat_point, packet->player_table.stamina, packet->player_table.part_base, packet->player_table.part_base, packet->player_table.gold,
			std::string_view(reinterpret_cast<const char *>(packet->player_table.skills), sizeof(packet->player_table.skills)),
			std::string_view(reinterpret_cast<const char *>(packet->player_table.quickslot), sizeof(packet->player_table.quickslot)));

	metin2::log::info( "PlayerCreate accountid {:d} name {} level {:d} gold {:d}, st {:d} ht {:d} job {:d}",
			packet->account_id, 
			packet->player_table.name, 
			packet->player_table.level, 
			packet->player_table.gold, 
			packet->player_table.st, 
			packet->player_table.ht, 
			packet->player_table.job);

	std::unique_ptr<SQLMsg> pMsg2(CDBManager::instance().DirectQuery(query.c_str()));
	if (metin2::config::test_server())
		metin2::log::info( "Create_Player queryLen[{:d}]", query.size());

	if (pMsg2->Get()->uiAffectedRows <= 0)
	{
		peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_ALREADY, dwHandle, 0);
		metin2::log::info( "ALREADY EXIST3 query: {} AffectedRows {:d}", query.c_str(), pMsg2->Get()->uiAffectedRows);
		return;
	}

	player_id = pMsg2->Get()->uiInsertID;

	query = sql::format("UPDATE player_index{} SET pid{}={} WHERE id={}", 
			postfix, packet->account_index + 1, player_id, packet->account_id);
	std::unique_ptr<SQLMsg> pMsg3(CDBManager::instance().DirectQuery(query.c_str()));

	if (pMsg3->Get()->uiAffectedRows <= 0)
	{
		metin2::log::error("QUERY_ERROR: {}", query.c_str());

		query = sql::format("DELETE FROM player{} WHERE id={}", postfix, player_id);
		CDBManager::instance().DirectQuery(query.c_str());

		peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
		return;
	}

	TPacketDGCreateSuccess pack;
	memset(&pack, 0, sizeof(pack));

	pack.bAccountCharacterIndex = packet->account_index;

	pack.player.dwID			= player_id;
	strlcpy(pack.player.szName, packet->player_table.name, sizeof(pack.player.szName));
	pack.player.byJob			= packet->player_table.job;
	pack.player.byLevel			= 1;
	pack.player.dwPlayMinutes	= 0;
	pack.player.byST			= packet->player_table.st;
	pack.player.byHT			= packet->player_table.ht;
	pack.player.byDX 			= packet->player_table.dx;
	pack.player.byIQ			= packet->player_table.iq;
	pack.player.wMainPart		= packet->player_table.part_base;
	pack.player.x			= packet->player_table.x;
	pack.player.y			= packet->player_table.y;

	peer->EncodeHeader(HEADER_DG_PLAYER_CREATE_SUCCESS, dwHandle, sizeof(TPacketDGCreateSuccess));
	peer->Encode(&pack, sizeof(TPacketDGCreateSuccess));

	metin2::log::info( "7 name {} job {:d}", pack.player.szName, pack.player.byJob);

	s_createTimeByAccountID[packet->account_id] = time(nullptr);
}

/*
 * PLAYER DELETE
 */
void CClientManager::__QUERY_PLAYER_DELETE(CPeer* peer, uint32_t dwHandle, TPlayerDeletePacket* packet)
{
	if (!packet->login[0] || !packet->player_id || packet->account_index >= PLAYER_PER_ACCOUNT)
		return;

	CLoginData * ld = GetLoginDataByLogin(packet->login);

	if (!ld)
	{
		peer->EncodeHeader(HEADER_DG_PLAYER_DELETE_FAILED, dwHandle, 1);
		peer->EncodeBYTE(packet->account_index);
		return;
	}

	TAccountTable & r = ld->GetAccountRef();

	// block for japan 
	if (strlen(r.social_id) < 7 || strncmp(packet->private_code, r.social_id + strlen(r.social_id) - 7, 7))
	{
		metin2::log::info( "PLAYER_DELETE FAILED len({:d})", strlen(r.social_id));
		peer->EncodeHeader(HEADER_DG_PLAYER_DELETE_FAILED, dwHandle, 1);
		peer->EncodeBYTE(packet->account_index);
		return;
	}

	CPlayerTableCache * pkPlayerCache = GetPlayerCache(packet->player_id);
	if (pkPlayerCache)
	{
		TPlayerTable * pTab = pkPlayerCache->Get();

		if (pTab->level >= m_iPlayerDeleteLevelLimit)
		{
			metin2::log::info( "PLAYER_DELETE FAILED LEVEL {:d} >= DELETE LIMIT {:d}", pTab->level, m_iPlayerDeleteLevelLimit);
			peer->EncodeHeader(HEADER_DG_PLAYER_DELETE_FAILED, dwHandle, 1);
			peer->EncodeBYTE(packet->account_index);
			return;
		}

		if (pTab->level < m_iPlayerDeleteLevelLimitLower)
		{
			metin2::log::info( "PLAYER_DELETE FAILED LEVEL {:d} < DELETE LIMIT {:d}", pTab->level, m_iPlayerDeleteLevelLimitLower);
			peer->EncodeHeader(HEADER_DG_PLAYER_DELETE_FAILED, dwHandle, 1);
			peer->EncodeBYTE(packet->account_index);
			return;
		}
	}

	char szQuery[128];
	snprintf(szQuery, sizeof(szQuery), "SELECT p.id, p.level, p.name FROM player_index%s AS i, player%s AS p WHERE pid%u=%u AND pid%u=p.id", 
			GetTablePostfix(), GetTablePostfix(), packet->account_index + 1, packet->player_id, packet->account_index + 1);

	ClientHandleInfo * pi = new ClientHandleInfo(dwHandle, packet->player_id);
	pi->account_index = packet->account_index;

	metin2::log::info( "PLAYER_DELETE TRY: {} {:d} pid{:d}", packet->login, packet->player_id, packet->account_index + 1);
	CDBManager::instance().ReturnQuery(szQuery, QID_PLAYER_DELETE, peer->GetHandle(), pi);
}

//
// @version	05/06/10 Bang2ni - Added deletion of the price info list on player deletion.
//
void CClientManager::__RESULT_PLAYER_DELETE(CPeer *peer, SQLMsg* msg)
{
	CQueryInfo * qi = (CQueryInfo *) msg->pvUserData;
	std::unique_ptr<ClientHandleInfo> pi(static_cast<ClientHandleInfo *>(qi->pvData));

	if (msg->Get() && msg->Get()->uiNumRows)
	{
		MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);

		uint32_t dwPID = 0;
		str_to_number(dwPID, row[0]);

		int deletedLevelLimit = 0;
		str_to_number(deletedLevelLimit, row[1]);

		std::string strName = row[2];

		if (deletedLevelLimit >= m_iPlayerDeleteLevelLimit)
		{
			metin2::log::info( "PLAYER_DELETE FAILED LEVEL {:d} >= DELETE LIMIT {:d}", deletedLevelLimit, m_iPlayerDeleteLevelLimit);
			peer->EncodeHeader(HEADER_DG_PLAYER_DELETE_FAILED, pi->dwHandle, 1);
			peer->EncodeBYTE(pi->account_index);
			return;
		}

		if (deletedLevelLimit < m_iPlayerDeleteLevelLimitLower)
		{
			metin2::log::info( "PLAYER_DELETE FAILED LEVEL {:d} < DELETE LIMIT {:d}", deletedLevelLimit, m_iPlayerDeleteLevelLimitLower);
			peer->EncodeHeader(HEADER_DG_PLAYER_DELETE_FAILED, pi->dwHandle, 1);
			peer->EncodeBYTE(pi->account_index);
			return;
		}

		const auto postfix = sql::raw(GetTablePostfix());

		std::string query = sql::format("INSERT INTO player{}_deleted SELECT * FROM player{} WHERE id={}", 
				postfix, postfix, pi->player_id);
		std::unique_ptr<SQLMsg> pIns(CDBManager::instance().DirectQuery(query.c_str()));

		if (pIns->Get()->uiAffectedRows == 0 || pIns->Get()->uiAffectedRows == (uint32_t)-1)
		{
			metin2::log::info( "PLAYER_DELETE FAILED {:d} CANNOT INSERT TO player{}_deleted", dwPID, GetTablePostfix());

			peer->EncodeHeader(HEADER_DG_PLAYER_DELETE_FAILED, pi->dwHandle, 1);
			peer->EncodeBYTE(pi->account_index);
			return;
		}

		// Deletion succeeded
		metin2::log::info( "PLAYER_DELETE SUCCESS {:d}", dwPID);

		char account_index_string[16];

		snprintf(account_index_string, sizeof(account_index_string), "player_id%d", m_iPlayerIDStart + pi->account_index);

		// Remove the player table from the cache.
		CPlayerTableCache * pkPlayerCache = GetPlayerCache(pi->player_id);

		if (pkPlayerCache)
		{
			m_map_playerCache.erase(pi->player_id);
			delete pkPlayerCache;
		}

		// Remove the items from the cache.
		TItemCacheSet * pSet = GetItemCacheSet(pi->player_id);

		if (pSet)
		{
			TItemCacheSet::iterator it = pSet->begin();

			while (it != pSet->end())
			{
				CItemCache * pkItemCache = *it++;
				DeleteItemCache(pkItemCache->Get()->id);
			}

			pSet->clear();

			m_map_pkItemCacheSetPtr.erase(pi->player_id);
		}

		query = sql::format("UPDATE player_index{} SET pid{}=0 WHERE pid{}={}", 
				postfix, 
				pi->account_index + 1, 
				pi->account_index + 1, 
				pi->player_id);

		std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQuery(query.c_str()));

		if (pMsg->Get()->uiAffectedRows == 0 || pMsg->Get()->uiAffectedRows == (uint32_t)-1)
		{
			metin2::log::info( "PLAYER_DELETE FAIL WHEN UPDATE account table");
			peer->EncodeHeader(HEADER_DG_PLAYER_DELETE_FAILED, pi->dwHandle, 1);
			peer->EncodeBYTE(pi->account_index);
			return;
		}

		query = sql::format("DELETE FROM player{} WHERE id={}", postfix, pi->player_id);
		std::unique_ptr<SQLMsg>(CDBManager::instance().DirectQuery(query.c_str()));

		query = sql::format("DELETE FROM item{} WHERE owner_id={} AND (window < {} or window = {})", postfix, pi->player_id, SAFEBOX, DRAGON_SOUL_INVENTORY);
		std::unique_ptr<SQLMsg>(CDBManager::instance().DirectQuery(query.c_str()));

		query = sql::format("DELETE FROM quest{} WHERE dwPID={}", postfix, pi->player_id);
		CDBManager::instance().AsyncQuery(query.c_str());

		query = sql::format("DELETE FROM affect{} WHERE dwPID={}", postfix, pi->player_id);
		CDBManager::instance().AsyncQuery(query.c_str());

		query = sql::format("DELETE FROM guild_member{} WHERE pid={}", postfix, pi->player_id);
		CDBManager::instance().AsyncQuery(query.c_str());

		// MYSHOP_PRICE_LIST
		query = sql::format("DELETE FROM myshop_pricelist{} WHERE owner_id={}", postfix, pi->player_id);
		CDBManager::instance().AsyncQuery(query.c_str());
		// END_OF_MYSHOP_PRICE_LIST

		query = sql::format("DELETE FROM messenger_list{} WHERE account={} OR companion={}", postfix, strName, strName);
		CDBManager::instance().AsyncQuery(query.c_str());

		peer->EncodeHeader(HEADER_DG_PLAYER_DELETE_SUCCESS, pi->dwHandle, 1);
		peer->EncodeBYTE(pi->account_index);
	}
	else
	{
		// Deletion failed
		metin2::log::info( "PLAYER_DELETE FAIL NO ROW");
		peer->EncodeHeader(HEADER_DG_PLAYER_DELETE_FAILED, pi->dwHandle, 1);
		peer->EncodeBYTE(pi->account_index);
	}
}

void CClientManager::QUERY_ADD_AFFECT(CPeer * peer, TPacketGDAddAffect * p)
{
	char queryStr[QUERY_MAX_LEN];
	/*
	   snprintf(queryStr, sizeof(queryStr),
	   "INSERT INTO affect%s (dwPID, bType, bApplyOn, lApplyValue, dwFlag, lDuration, lSPCost) "
	   "VALUES(%u, %u, %u, %d, %u, %d, %d) "
	   "ON DUPLICATE KEY UPDATE lApplyValue=%d, dwFlag=%u, lDuration=%d, lSPCost=%d",
	   GetTablePostfix(),
	   p->dwPID,
	   p->elem.dwType,
	   p->elem.bApplyOn,
	   p->elem.lApplyValue,
	   p->elem.dwFlag,
	   p->elem.lDuration,
	   p->elem.lSPCost,
	   p->elem.lApplyValue,
	   p->elem.dwFlag,
	   p->elem.lDuration,
	   p->elem.lSPCost);
	   */
	snprintf(queryStr, sizeof(queryStr),
			"REPLACE INTO affect%s (dwPID, bType, bApplyOn, lApplyValue, dwFlag, lDuration, lSPCost) "
			"VALUES(%u, %u, %u, %d, %u, %d, %d)",
			GetTablePostfix(),
			p->dwPID,
			p->elem.dwType,
			p->elem.bApplyOn,
			p->elem.lApplyValue,
			p->elem.dwFlag,
			p->elem.lDuration,
			p->elem.lSPCost);

	CDBManager::instance().AsyncQuery(queryStr);
}

void CClientManager::QUERY_REMOVE_AFFECT(CPeer * peer, TPacketGDRemoveAffect * p)
{
	char queryStr[QUERY_MAX_LEN];

	snprintf(queryStr, sizeof(queryStr),
			"DELETE FROM affect%s WHERE dwPID=%u AND bType=%u AND bApplyOn=%u",
			GetTablePostfix(), p->dwPID, p->dwType, p->bApplyOn);

	CDBManager::instance().AsyncQuery(queryStr);
}


void CClientManager::QUERY_HIGHSCORE_REGISTER(CPeer* peer, TPacketGDHighscore * data)
{
	const std::string query = sql::format("SELECT value FROM highscore{} WHERE board={} AND pid = {}", sql::raw(GetTablePostfix()), data->szBoard, data->dwPID);

	metin2::log::info( "HEADER_GD_HIGHSCORE_REGISTER: PID {:d}", data->dwPID);

	ClientHandleInfo * pi = new ClientHandleInfo(0);
	strlcpy(pi->login, data->szBoard, sizeof(pi->login));
	pi->account_id = (uint32_t)data->lValue;
	pi->player_id = data->dwPID;
	pi->account_index = (data->cDir > 0);

	CDBManager::instance().ReturnQuery(query.c_str(), QID_HIGHSCORE_REGISTER, peer->GetHandle(), pi);
}

void CClientManager::RESULT_HIGHSCORE_REGISTER(CPeer * pkPeer, SQLMsg * msg)
{
	CQueryInfo * qi = (CQueryInfo *) msg->pvUserData;
	ClientHandleInfo * pi = (ClientHandleInfo *) qi->pvData;

	char szBoard[21];
	strlcpy(szBoard, pi->login, sizeof(szBoard));
	int value = (int)pi->account_id;

	SQLResult * res = msg->Get();

	if (res->uiNumRows == 0)
	{
		// Insert a new high score
		const std::string query = sql::format("INSERT INTO highscore{} VALUES({}, {}, {})", sql::raw(GetTablePostfix()), szBoard, pi->player_id, value);
		CDBManager::instance().AsyncQuery(query.c_str());
	}
	else
	{
		if (!res->pSQLResult)
		{
			delete pi;
			return;
		}

		MYSQL_ROW row = mysql_fetch_row(res->pSQLResult);
		if (row && row[0])
		{
			int current_value = 0; str_to_number(current_value, row[0]);
			if ((pi->account_index && current_value >= value) || (!pi->account_index && current_value <= value))
			{
				value = current_value;
			}
			else
			{
				const std::string query = sql::format("REPLACE INTO highscore{} VALUES({}, {}, {})", sql::raw(GetTablePostfix()), szBoard, pi->player_id, value);
				CDBManager::instance().AsyncQuery(query.c_str());
			}
		}
		else
		{
			const std::string query = sql::format("INSERT INTO highscore{} VALUES({}, {}, {})", sql::raw(GetTablePostfix()), szBoard, pi->player_id, value);
			CDBManager::instance().AsyncQuery(query.c_str());
		}
	}
	// TODO: check here whether the high score was updated and broadcast a notice.
	delete pi;
}

void CClientManager::InsertLogoutPlayer(uint32_t pid)
{
	TLogoutPlayerMap::iterator it = m_map_logout.find(pid);

	// Add if it doesn't exist
	if (it != m_map_logout.end())
	{
		// If it exists, only update the time
		if (metin2::config::db().login_log)
			metin2::log::info( "LOGOUT: Update player time pid({:d})", pid);

		it->second->time = time(nullptr);
		return;
	}
		
	auto pLogout = std::make_unique<TLogoutPlayer>();
	pLogout->pid = pid;
	pLogout->time = time(nullptr);
	m_map_logout.insert(std::make_pair(pid, std::move(pLogout)));

	if (metin2::config::db().login_log)
		metin2::log::info( "LOGOUT: Insert player pid({:d})", pid);
}

void CClientManager::DeleteLogoutPlayer(uint32_t pid)
{
	TLogoutPlayerMap::iterator it = m_map_logout.find(pid);

	if (it != m_map_logout.end())
	{
		m_map_logout.erase(it);
	}
}


void CClientManager::UpdateLogoutPlayer()
{
	time_t now = time(nullptr);

	TLogoutPlayerMap::iterator it = m_map_logout.begin();

	while (it != m_map_logout.end())
	{
		TLogoutPlayer* pLogout = it->second.get();

		if (now - g_iLogoutSeconds > pLogout->time)
		{
			FlushItemCacheSet(pLogout->pid);
			FlushPlayerCacheSet(pLogout->pid);

			m_map_logout.erase(it++);
		}
		else
			++it;
	}
}

void CClientManager::FlushPlayerCacheSet(uint32_t pid)
{
	TPlayerTableCacheMap::iterator it = m_map_playerCache.find(pid);

	if (it != m_map_playerCache.end())
	{
		CPlayerTableCache * c = it->second;
		m_map_playerCache.erase(it);

		c->Flush();
		delete c; 
	}
}
}
