
#include <metin2/db/caches/cache.h>
#include <metin2/db/clients/client_manager.h>
#include <metin2/db/storage/db_manager.h>
#include <metin2/db/items/item_award_manager.h>
#include <metin2/db/login/login_data.h>
#include <metin2/db/app/main.h>
#include <metin2/db/clients/peer.h>
#include <metin2/db/clients/qid.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/common/utils.h>
#include <metin2/sql/async_sql.h>
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
bool create_item_table_from_res(MYSQL_RES * res, std::vector<player_item> * pVec, uint32_t dwPID)
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
		player_item & item = pVec->at(i);

		int cur = 0;

		// Check all SELECT syntax on item table before change this function!!!
		// Check all SELECT syntax on item table before change this function!!!
		// Check all SELECT syntax on item table before change this function!!!
		str_to_number(item.id, row[cur++]);
		str_to_number(item.window, row[cur++]);
		str_to_number(item.pos, row[cur++]);
		str_to_number(item.count, row[cur++]);
		str_to_number(item.vnum, row[cur++]);
		str_to_number(item.sockets[0], row[cur++]);
		str_to_number(item.sockets[1], row[cur++]);
		str_to_number(item.sockets[2], row[cur++]);

		for (int j = 0; j < ITEM_ATTRIBUTE_MAX_NUM; j++)
		{
			str_to_number(item.attr[j].type, row[cur++]);
			str_to_number(item.attr[j].s_value, row[cur++]);
		}

		item.owner		= dwPID;
	}

	return true;
}

std::string create_player_save_query(const player_table * pkTab)
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
		sql::raw(get_table_postfix()),
		pkTab->job,
		pkTab->voice,
		pkTab->dir,
		pkTab->x,
		pkTab->y,
		pkTab->z,
		pkTab->map_index,
		pkTab->exit_x,
		pkTab->exit_y,
		pkTab->exit_map_index,
		pkTab->hp,
		pkTab->sp,
		pkTab->stamina,
		pkTab->s_random_hp,
		pkTab->s_random_sp,
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
		pkTab->alignment,
		pkTab->horse.level,
		pkTab->horse.riding,
		pkTab->horse.s_health,
		pkTab->horse.horse_health_drop_time,
		pkTab->horse.s_stamina,
		pkTab->horse_skill_point,
		std::string_view(reinterpret_cast<const char *>(pkTab->skills), sizeof(pkTab->skills)),
		std::string_view(reinterpret_cast<const char *>(pkTab->quickslot), sizeof(pkTab->quickslot)),
		pkTab->id);
}

player_table_cache * client_manager::player_cache(uint32_t id)
{
	player_table_cache_map::iterator it = map_player_cache_.find(id);

	if (it == map_player_cache_.end())
		return nullptr;

	player_table* pTable = it->second->get(false);
	pTable->logoff_interval = current_time() - it->second->last_update_time();
	return it->second;
}

void client_manager::put_player_cache(player_table * pNew)
{
	player_table_cache * c;

	c = player_cache(pNew->id);

	if (!c)
	{
		c = new player_table_cache;
		map_player_cache_.insert(player_table_cache_map::value_type(pNew->id, c));
	}

	c->put(pNew);
}

/*
 * PLAYER LOAD
 */
void client_manager::query_player_load(peer * peer, uint32_t dwHandle, player_load_packet * packet)
{
	player_table_cache * c;
	player_table * pTab;
	
	//
	// Cache all characters belonging to an account
	//
	login_data * pLoginData = get_login_data_by_aid(packet->account_id);

	if (pLoginData)
	{
		for (int n = 0; n < PLAYER_PER_ACCOUNT; ++n)
			if (pLoginData->account_ref().players[n].id != 0)
				delete_logout_player(pLoginData->account_ref().players[n].id);
	}

	//----------------------------------------------------------------
	// 1. User info exists in DBCache: from DBCache 
	// 2. User info not in DBCache: from DB 
	// ---------------------------------------------------------------
	
	//----------------------------------
	// 1. User info exists in DBCache: from DBCache 
	//----------------------------------
	if ((c = player_cache(packet->player_id)))
	{
		login_data * pkLD = get_login_data_by_aid(packet->account_id);

		if (!pkLD || pkLD->is_play())
		{
			metin2::log::info( "PLAYER_LOAD_ERROR: LoginData {} IsPlay {:d}", static_cast<const void *>(pkLD), pkLD ? pkLD->is_play() : 0);
			peer->encode_header(HEADER_DG_PLAYER_LOAD_FAILED, dwHandle, 0); 
			return;
		}

		pTab = c->get();

		pkLD->set_play(true);
		std::memcpy(pTab->premium_times, pkLD->premium_ptr(), sizeof(pTab->premium_times));

		peer->encode_header(HEADER_DG_PLAYER_LOAD_SUCCESS, dwHandle, sizeof(player_table));
		peer->encode(pTab, sizeof(player_table));

		if (packet->player_id != pkLD->last_player_id())
		{
			packet_need_login_log_info logInfo;
			logInfo.player_id = packet->player_id;

			pkLD->set_last_player_id( packet->player_id );

			peer->encode_header( HEADER_DG_NEED_LOGIN_LOG, dwHandle, sizeof(packet_need_login_log_info) );
			peer->encode( &logInfo, sizeof(packet_need_login_log_info) );
		}

		char szQuery[1024] = { 0, };

		item_cache_set * pSet = get_item_cache_set(pTab->id);

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
			static std::vector<player_item> s_items;
			s_items.resize(pSet->size());

			uint32_t dwCount = 0;
			item_cache_set::iterator it = pSet->begin();

			while (it != pSet->end())
			{
				item_cache * c = *it++;
				player_item * p = c->get();

				if (p->vnum) // No vnum means a deleted item.
					std::memcpy(&s_items[dwCount++], p, sizeof(player_item));
			}

			if (metin2::config::test_server())
				metin2::log::info( "ITEM_CACHE: HIT! {} count: {:d}", pTab->name, dwCount);

			peer->encode_header(HEADER_DG_ITEM_LOAD, dwHandle, sizeof(uint32_t) + sizeof(player_item) * dwCount);
			peer->encode_dword(dwCount);

			if (dwCount)
				peer->encode(&s_items[0], sizeof(player_item) * dwCount);

			// Quest
			snprintf(szQuery, sizeof(szQuery),
					"SELECT dwPID,szName,szState,lValue FROM quest%s WHERE dwPID=%d AND lValue<>0",
					get_table_postfix(), pTab->id);
			
			db_manager::instance().return_query(szQuery, QID_QUEST, peer->handle(), new client_handle_info(dwHandle,0,packet->account_id));

			// Affect
			snprintf(szQuery, sizeof(szQuery),
					"SELECT dwPID,bType,bApplyOn,lApplyValue,dwFlag,lDuration,lSPCost FROM affect%s WHERE dwPID=%d",
					get_table_postfix(), pTab->id);
			db_manager::instance().return_query(szQuery, QID_AFFECT, peer->handle(), new client_handle_info(dwHandle));
		}
		/////////////////////////////////////////////
		// 2) Items not in DBCache: fetch from DB 
		/////////////////////////////////////////////
		else
		{
			snprintf(szQuery, sizeof(szQuery), 
					"SELECT id,window+0,pos,count,vnum,socket0,socket1,socket2,attrtype0,attrvalue0,attrtype1,attrvalue1,attrtype2,attrvalue2,attrtype3,attrvalue3,attrtype4,attrvalue4,attrtype5,attrvalue5,attrtype6,attrvalue6 "
					"FROM item%s WHERE owner_id=%d AND (window < %d or window = %d)",
					get_table_postfix(), pTab->id, SAFEBOX, DRAGON_SOUL_INVENTORY);

			db_manager::instance().return_query(szQuery,
					QID_ITEM,
					peer->handle(),
					new client_handle_info(dwHandle, pTab->id));
			snprintf(szQuery, sizeof(szQuery), 
					"SELECT dwPID, szName, szState, lValue FROM quest%s WHERE dwPID=%d",
					get_table_postfix(), pTab->id);

			db_manager::instance().return_query(szQuery,
					QID_QUEST,
					peer->handle(),
					new client_handle_info(dwHandle, pTab->id));
			snprintf(szQuery, sizeof(szQuery), 
					"SELECT dwPID, bType, bApplyOn, lApplyValue, dwFlag, lDuration, lSPCost FROM affect%s WHERE dwPID=%d",
					get_table_postfix(), pTab->id);

			db_manager::instance().return_query(szQuery,
					QID_AFFECT,
					peer->handle(),
					new client_handle_info(dwHandle, pTab->id));
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
				get_table_postfix(), packet->player_id);

		client_handle_info * pkInfo = new client_handle_info(dwHandle, packet->player_id);
		pkInfo->account_id = packet->account_id;
		db_manager::instance().return_query(queryStr, QID_PLAYER, peer->handle(), pkInfo);

		//--------------------------------------------------------------
		// Fetch items 
		//--------------------------------------------------------------
		snprintf(queryStr, sizeof(queryStr),
				"SELECT id,window+0,pos,count,vnum,socket0,socket1,socket2,attrtype0,attrvalue0,attrtype1,attrvalue1,attrtype2,attrvalue2,attrtype3,attrvalue3,attrtype4,attrvalue4,attrtype5,attrvalue5,attrtype6,attrvalue6 "
				"FROM item%s WHERE owner_id=%d AND (window < %d or window = %d)",
				get_table_postfix(), packet->player_id, SAFEBOX, DRAGON_SOUL_INVENTORY);
		db_manager::instance().return_query(queryStr, QID_ITEM, peer->handle(), new client_handle_info(dwHandle, packet->player_id));

		//--------------------------------------------------------------
		// Fetch QUEST 
		//--------------------------------------------------------------
		snprintf(queryStr, sizeof(queryStr),
				"SELECT dwPID,szName,szState,lValue FROM quest%s WHERE dwPID=%d",
				get_table_postfix(), packet->player_id);
		db_manager::instance().return_query(queryStr, QID_QUEST, peer->handle(), new client_handle_info(dwHandle, packet->player_id,packet->account_id));
		//Also pass account id so the German gift feature can get login info from the item_award table
		//--------------------------------------------------------------
		// Fetch AFFECT 
		//--------------------------------------------------------------
		snprintf(queryStr, sizeof(queryStr),
				"SELECT dwPID,bType,bApplyOn,lApplyValue,dwFlag,lDuration,lSPCost FROM affect%s WHERE dwPID=%d",
				get_table_postfix(), packet->player_id);
		db_manager::instance().return_query(queryStr, QID_AFFECT, peer->handle(), new client_handle_info(dwHandle, packet->player_id));
	}
	
	
}
void client_manager::item_award(peer * peer,char* login)
{
	std::string login_t = login;
	std::set<db::items::item_award *> * pSet = item_award_manager::instance().by_login(login_t.c_str());	
	if(pSet == nullptr)
		return;
	__typeof(pSet->begin()) it = pSet->begin();	//Read those whose taken_time is NULL	
	while(it != pSet->end() )
	{				
		db::items::item_award * pItemAward = *(it++);
		inform_gift_award(pItemAward->login, pItemAward->why, pItemAward->vnum);
	}
}

void client_manager::inform_gift_award(const char * login, const char * why, uint32_t vnum)
{
	// The command is the text between a leading '[' and the first ']' (or the end).
	const std::string_view text(why);
	if (!text.starts_with("[GIFT") || (text.size() > 5 && text[5] != ']'))
		return;

	packet_item_award_infromer giftData{};
	strlcpy(giftData.login, login, sizeof(giftData.login));
	strlcpy(giftData.command, "GIFT", sizeof(giftData.command));
	giftData.vnum = vnum;
	forward_packet(HEADER_DG_ITEMAWARD_INFORMER, &giftData, sizeof(giftData));
}

bool create_player_table_from_res(MYSQL_RES * res, player_table * pkTab)
{
	if (mysql_num_rows(res) == 0)	// No data
		return false;

	memset(pkTab, 0, sizeof(player_table));

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
	str_to_number(pkTab->map_index, row[col++]);
	str_to_number(pkTab->exit_x, row[col++]);
	str_to_number(pkTab->exit_y, row[col++]);
	str_to_number(pkTab->exit_map_index,  row[col++]);
	str_to_number(pkTab->hp, row[col++]);
	str_to_number(pkTab->sp, row[col++]);
	str_to_number(pkTab->stamina, row[col++]);
	str_to_number(pkTab->s_random_hp, row[col++]);
	str_to_number(pkTab->s_random_sp, row[col++]);
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
	str_to_number(pkTab->alignment, row[col++]);

	if (row[col])
	{
		strlcpy(pkTab->mobile, row[col], sizeof(pkTab->mobile));
	}

	col++;

	str_to_number(pkTab->horse.level, row[col++]);
	str_to_number(pkTab->horse.riding, row[col++]);
	str_to_number(pkTab->horse.s_health, row[col++]);
	str_to_number(pkTab->horse.horse_health_drop_time, row[col++]);
	str_to_number(pkTab->horse.s_stamina, row[col++]);
	str_to_number(pkTab->logoff_interval, row[col++]);
	str_to_number(pkTab->horse_skill_point, row[col++]);

	// reset sub_skill_point
	{
		pkTab->skills[123].level = 0; // SKILL_CREATE

		if (pkTab->level > 9)
		{
			int max_point = pkTab->level - 9;

			int skill_point = 
				std::min<int>(20, pkTab->skills[121].level) +	// SKILL_LEADERSHIP			Leadership
				std::min<int>(20, pkTab->skills[124].level) +	// SKILL_MINING				Mining
				std::min<int>(10, pkTab->skills[131].level) +	// SKILL_HORSE_SUMMON		Horse summon
				std::min<int>(20, pkTab->skills[141].level) +	// SKILL_ADD_HP				HP boost
				std::min<int>(20, pkTab->skills[142].level);		// SKILL_RESIST_PENETRATE	Penetration resistance

			pkTab->sub_skill_point = max_point - skill_point;
		}
		else
			pkTab->sub_skill_point = 0;
	}

	return true;
}

void client_manager::result_composite_player(peer * peer, ql_msg * pMsg, uint32_t dwQID)
{
	query_info * qi = (query_info *) pMsg->pv_user_data;
	std::unique_ptr<client_handle_info> info((client_handle_info *) qi->pv_data);
	
	MYSQL_RES * pSQLResult = pMsg->get()->sql_result;
	if (!pSQLResult)
	{
		metin2::log::error("null MYSQL_RES QID {:d}", dwQID);
		return;
	}

	switch (dwQID)
	{
		case QID_PLAYER:
			metin2::log::info( "QID_PLAYER {:d} {:d}", info->handle, info->player_id);
			result_player_load(peer, pSQLResult, info.get());

			break;

		case QID_ITEM:
			metin2::log::info( "QID_ITEM {:d}", info->handle);
			result_item_load(peer, pSQLResult, info->handle, info->player_id);
			break;

		case QID_QUEST:
			{
				metin2::log::info( "QID_QUEST {:d}", info->handle);
				result_quest_load(peer, pSQLResult, info->handle, info->player_id);
				//Get aid
				client_handle_info*  temp1 = info.get();
				if (temp1 == nullptr)
					break;
				
				login_data* pLoginData1 = get_login_data_by_aid(temp1->account_id);	//				
				//German gift feature
				if (pLoginData1 == nullptr)
					break;
				metin2::log::info("info of pLoginData1 before call ItemAwardfunction {}",static_cast<const void *>(static_cast<void*>(pLoginData1)));
				item_award(peer,pLoginData1->account_ref().login);
			}
			break;

		case QID_AFFECT:
			metin2::log::info( "QID_AFFECT {:d}", info->handle);
			result_affect_load(peer, pSQLResult, info->handle);
			break;
	}
	
}

void client_manager::result_player_load(peer * peer, MYSQL_RES * pRes, client_handle_info * pkInfo)
{
	player_table tab;

	if (!create_player_table_from_res(pRes, &tab))
	{
		peer->encode_header(HEADER_DG_PLAYER_LOAD_FAILED, pkInfo->handle, 0); 
		return;
	}

	login_data * pkLD = get_login_data_by_aid(pkInfo->account_id);
	
	if (!pkLD || pkLD->is_play())
	{
		metin2::log::info( "PLAYER_LOAD_ERROR: LoginData {} IsPlay {:d}", static_cast<const void *>(pkLD), pkLD ? pkLD->is_play() : 0);
		peer->encode_header(HEADER_DG_PLAYER_LOAD_FAILED, pkInfo->handle, 0); 
		return;
	}

	pkLD->set_play(true);
	std::memcpy(tab.premium_times, pkLD->premium_ptr(), sizeof(tab.premium_times));

	peer->encode_header(HEADER_DG_PLAYER_LOAD_SUCCESS, pkInfo->handle, sizeof(player_table));
	peer->encode(&tab, sizeof(player_table));

	if (tab.id != pkLD->last_player_id())
	{
		packet_need_login_log_info logInfo;
		logInfo.player_id = tab.id;

		pkLD->set_last_player_id( tab.id );

		peer->encode_header( HEADER_DG_NEED_LOGIN_LOG, pkInfo->handle, sizeof(packet_need_login_log_info) );
		peer->encode( &logInfo, sizeof(packet_need_login_log_info) );
	}
}

void client_manager::result_item_load(peer * peer, MYSQL_RES * pRes, uint32_t dwHandle, uint32_t dwPID)
{
	static std::vector<player_item> s_items;
	//Read item info from DB.
	create_item_table_from_res(pRes, &s_items, dwPID);
	uint32_t dwCount = s_items.size();

	peer->encode_header(HEADER_DG_ITEM_LOAD, dwHandle, sizeof(uint32_t) + sizeof(player_item) * dwCount);
	peer->encode_dword(dwCount);

	//Create the CacheSet  
	create_item_cache_set(dwPID);

	// ITEM_LOAD_LOG_ATTACH_PID
	metin2::log::info( "ITEM_LOAD: count {:d} pid {:d}", dwCount, dwPID);
	// END_OF_ITEM_LOAD_LOG_ATTACH_PID

	if (dwCount)
	{
		peer->encode(&s_items[0], sizeof(player_item) * dwCount);

		for (uint32_t i = 0; i < dwCount; ++i)
			put_item_cache(&s_items[i], true); // Loaded data doesn't need to be saved separately, so pass true for bSkipQuery.
	}
}

void client_manager::result_affect_load(peer * peer, MYSQL_RES * pRes, uint32_t dwHandle)
{
	int iNumRows;

	if ((iNumRows = mysql_num_rows(pRes)) == 0) // No data
		return;

	static std::vector<packet_affect_element> s_elements;
	s_elements.resize(iNumRows);

	uint32_t dwPID = 0;

	MYSQL_ROW row;

	for (int i = 0; i < iNumRows; ++i)
	{
		packet_affect_element & r = s_elements[i];
		row = mysql_fetch_row(pRes);

		if (dwPID == 0)
			str_to_number(dwPID, row[0]);

		str_to_number(r.type, row[1]);
		str_to_number(r.apply_on, row[2]);
		str_to_number(r.apply_value, row[3]);
		str_to_number(r.flag, row[4]);
		str_to_number(r.duration, row[5]);
		str_to_number(r.sp_cost, row[6]);
	}

	metin2::log::info( "AFFECT_LOAD: count {:d} PID {:d}", s_elements.size(), dwPID);

	uint32_t dwCount = s_elements.size();

	peer->encode_header(HEADER_DG_AFFECT_LOAD, dwHandle, sizeof(uint32_t) + sizeof(uint32_t) + sizeof(packet_affect_element) * dwCount);
	peer->encode(&dwPID, sizeof(uint32_t));
	peer->encode(&dwCount, sizeof(uint32_t));
	peer->encode(&s_elements[0], sizeof(packet_affect_element) * dwCount);
}

void client_manager::result_quest_load(peer * peer, MYSQL_RES * pRes, uint32_t dwHandle, uint32_t pid)
{
	int iNumRows;

	if ((iNumRows = mysql_num_rows(pRes)) == 0)
	{
		uint32_t dwCount = 0; 
		peer->encode_header(HEADER_DG_QUEST_LOAD, dwHandle, sizeof(uint32_t));
		peer->encode(&dwCount, sizeof(uint32_t));
		return;
	}

	static std::vector<quest_table> s_table;
	s_table.resize(iNumRows);

	MYSQL_ROW row;

	for (int i = 0; i < iNumRows; ++i)
	{
		quest_table & r = s_table[i];

		row = mysql_fetch_row(pRes);

		str_to_number(r.pid, row[0]);
		strlcpy(r.name, row[1], sizeof(r.name));
		strlcpy(r.state, row[2], sizeof(r.state));
		str_to_number(r.value, row[3]);
	}

	metin2::log::info( "QUEST_LOAD: count {:d} PID {:d}", s_table.size(), s_table[0].pid);

	uint32_t dwCount = s_table.size();

	peer->encode_header(HEADER_DG_QUEST_LOAD, dwHandle, sizeof(uint32_t) + sizeof(quest_table) * dwCount);
	peer->encode(&dwCount, sizeof(uint32_t));
	peer->encode(&s_table[0], sizeof(quest_table) * dwCount);
}

/*
 * PLAYER SAVE
 */
void client_manager::query_player_save(peer * peer, uint32_t dwHandle, player_table * pkTab)
{
	if (metin2::config::test_server())
		metin2::log::info( "PLAYER_SAVE: {}", pkTab->name);

	put_player_cache(pkTab);
}

typedef std::map<uint32_t, time_t> time_by_id_map_t;
static time_by_id_map_t create_time_by_account_id;

/*
 * PLAYER CREATE
 */
void client_manager::query_player_create(peer *peer, uint32_t dwHandle, player_create_packet* packet)
{
	int		player_id;

	// Cannot create a character within X seconds on one account.
	time_by_id_map_t::iterator it = create_time_by_account_id.find(packet->account_id);

	if (it != create_time_by_account_id.end())
	{
		time_t curtime = time(nullptr);

		if (curtime - it->second < 30)
		{
			peer->encode_header(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
			return;
		}
	}

	const auto postfix = sql::raw(get_table_postfix());

	std::string query = sql::format(
			"SELECT pid{} FROM player_index{} WHERE id={}", packet->account_index + 1, postfix, packet->account_id);

	std::unique_ptr<ql_msg> pMsg0(db_manager::instance().direct_query(query.c_str()));

	if (pMsg0->get()->num_rows != 0)
	{
		if (!pMsg0->get()->sql_result)
		{
			peer->encode_header(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
			return;
		}

		MYSQL_ROW row = mysql_fetch_row(pMsg0->get()->sql_result);

		uint32_t dwPID = 0; str_to_number(dwPID, row[0]);
		if (row[0] && dwPID > 0)
		{
			peer->encode_header(HEADER_DG_PLAYER_CREATE_ALREADY, dwHandle, 0);
			metin2::log::info( "ALREADY EXIST AccountChrIdx {:d} ID {:d}", packet->account_index, dwPID);
			return;
		}
	}
	else
	{
		peer->encode_header(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
		return;
	}

	query = sql::format(
			"SELECT COUNT(*) as count FROM player{} WHERE name={}", postfix, packet->player_table.name);

	std::unique_ptr<ql_msg> pMsg1(db_manager::instance().direct_query(query.c_str()));

	if (pMsg1->get()->num_rows)
	{
		if (!pMsg1->get()->sql_result)
		{
			peer->encode_header(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
			return;
		}

		MYSQL_ROW row = mysql_fetch_row(pMsg1->get()->sql_result);

		if (*row[0] != '0')
		{
			metin2::log::info( "ALREADY EXIST name {}, row[0] {} query {}", packet->player_table.name, row[0], query.c_str());
			peer->encode_header(HEADER_DG_PLAYER_CREATE_ALREADY, dwHandle, 0);
			return;
		}
	}
	else
	{
		peer->encode_header(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
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
			packet->player_table.hp, packet->player_table.sp, packet->player_table.s_random_hp, packet->player_table.s_random_sp, packet->player_table.stat_point, packet->player_table.stamina, packet->player_table.part_base, packet->player_table.part_base, packet->player_table.gold,
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

	std::unique_ptr<ql_msg> pMsg2(db_manager::instance().direct_query(query.c_str()));
	if (metin2::config::test_server())
		metin2::log::info( "Create_Player queryLen[{:d}]", query.size());

	if (pMsg2->get()->affected_rows <= 0)
	{
		peer->encode_header(HEADER_DG_PLAYER_CREATE_ALREADY, dwHandle, 0);
		metin2::log::info( "ALREADY EXIST3 query: {} AffectedRows {:d}", query.c_str(), pMsg2->get()->affected_rows);
		return;
	}

	player_id = pMsg2->get()->insert_id;

	query = sql::format("UPDATE player_index{} SET pid{}={} WHERE id={}", 
			postfix, packet->account_index + 1, player_id, packet->account_id);
	std::unique_ptr<ql_msg> pMsg3(db_manager::instance().direct_query(query.c_str()));

	if (pMsg3->get()->affected_rows <= 0)
	{
		metin2::log::error("QUERY_ERROR: {}", query.c_str());

		query = sql::format("DELETE FROM player{} WHERE id={}", postfix, player_id);
		db_manager::instance().direct_query(query.c_str());

		peer->encode_header(HEADER_DG_PLAYER_CREATE_FAILED, dwHandle, 0);
		return;
	}

	packet_dg_create_success pack;
	memset(&pack, 0, sizeof(pack));

	pack.account_character_index = packet->account_index;

	pack.player.id			= player_id;
	strlcpy(pack.player.name, packet->player_table.name, sizeof(pack.player.name));
	pack.player.job			= packet->player_table.job;
	pack.player.level			= 1;
	pack.player.play_minutes	= 0;
	pack.player.st			= packet->player_table.st;
	pack.player.ht			= packet->player_table.ht;
	pack.player.dx 			= packet->player_table.dx;
	pack.player.iq			= packet->player_table.iq;
	pack.player.main_part		= packet->player_table.part_base;
	pack.player.x			= packet->player_table.x;
	pack.player.y			= packet->player_table.y;

	peer->encode_header(HEADER_DG_PLAYER_CREATE_SUCCESS, dwHandle, sizeof(packet_dg_create_success));
	peer->encode(&pack, sizeof(packet_dg_create_success));

	metin2::log::info( "7 name {} job {:d}", pack.player.name, pack.player.job);

	create_time_by_account_id[packet->account_id] = time(nullptr);
}

/*
 * PLAYER DELETE
 */
void client_manager::query_player_delete(peer* peer, uint32_t dwHandle, player_delete_packet* packet)
{
	if (!packet->login[0] || !packet->player_id || packet->account_index >= PLAYER_PER_ACCOUNT)
		return;

	login_data * ld = get_login_data_by_login(packet->login);

	if (!ld)
	{
		peer->encode_header(HEADER_DG_PLAYER_DELETE_FAILED, dwHandle, 1);
		peer->encode_byte(packet->account_index);
		return;
	}

	account_table & r = ld->account_ref();

	// block for japan 
	if (strlen(r.social_id) < 7 || strncmp(packet->private_code, r.social_id + strlen(r.social_id) - 7, 7))
	{
		metin2::log::info( "PLAYER_DELETE FAILED len({:d})", strlen(r.social_id));
		peer->encode_header(HEADER_DG_PLAYER_DELETE_FAILED, dwHandle, 1);
		peer->encode_byte(packet->account_index);
		return;
	}

	player_table_cache * pkPlayerCache = player_cache(packet->player_id);
	if (pkPlayerCache)
	{
		player_table * pTab = pkPlayerCache->get();

		if (pTab->level >= player_delete_level_limit_)
		{
			metin2::log::info( "PLAYER_DELETE FAILED LEVEL {:d} >= DELETE LIMIT {:d}", pTab->level, player_delete_level_limit_);
			peer->encode_header(HEADER_DG_PLAYER_DELETE_FAILED, dwHandle, 1);
			peer->encode_byte(packet->account_index);
			return;
		}

		if (pTab->level < player_delete_level_limit_lower_)
		{
			metin2::log::info( "PLAYER_DELETE FAILED LEVEL {:d} < DELETE LIMIT {:d}", pTab->level, player_delete_level_limit_lower_);
			peer->encode_header(HEADER_DG_PLAYER_DELETE_FAILED, dwHandle, 1);
			peer->encode_byte(packet->account_index);
			return;
		}
	}

	char szQuery[128];
	snprintf(szQuery, sizeof(szQuery), "SELECT p.id, p.level, p.name FROM player_index%s AS i, player%s AS p WHERE pid%u=%u AND pid%u=p.id", 
			get_table_postfix(), get_table_postfix(), packet->account_index + 1, packet->player_id, packet->account_index + 1);

	client_handle_info * pi = new client_handle_info(dwHandle, packet->player_id);
	pi->account_index = packet->account_index;

	metin2::log::info( "PLAYER_DELETE TRY: {} {:d} pid{:d}", packet->login, packet->player_id, packet->account_index + 1);
	db_manager::instance().return_query(szQuery, QID_PLAYER_DELETE, peer->handle(), pi);
}

//
// @version	05/06/10 Bang2ni - Added deletion of the price info list on player deletion.
//
void client_manager::result_player_delete(peer *peer, ql_msg* msg)
{
	query_info * qi = (query_info *) msg->pv_user_data;
	std::unique_ptr<client_handle_info> pi(static_cast<client_handle_info *>(qi->pv_data));

	if (msg->get() && msg->get()->num_rows)
	{
		MYSQL_ROW row = mysql_fetch_row(msg->get()->sql_result);

		uint32_t dwPID = 0;
		str_to_number(dwPID, row[0]);

		int deletedLevelLimit = 0;
		str_to_number(deletedLevelLimit, row[1]);

		std::string strName = row[2];

		if (deletedLevelLimit >= player_delete_level_limit_)
		{
			metin2::log::info( "PLAYER_DELETE FAILED LEVEL {:d} >= DELETE LIMIT {:d}", deletedLevelLimit, player_delete_level_limit_);
			peer->encode_header(HEADER_DG_PLAYER_DELETE_FAILED, pi->handle, 1);
			peer->encode_byte(pi->account_index);
			return;
		}

		if (deletedLevelLimit < player_delete_level_limit_lower_)
		{
			metin2::log::info( "PLAYER_DELETE FAILED LEVEL {:d} < DELETE LIMIT {:d}", deletedLevelLimit, player_delete_level_limit_lower_);
			peer->encode_header(HEADER_DG_PLAYER_DELETE_FAILED, pi->handle, 1);
			peer->encode_byte(pi->account_index);
			return;
		}

		const auto postfix = sql::raw(get_table_postfix());

		std::string query = sql::format("INSERT INTO player{}_deleted SELECT * FROM player{} WHERE id={}", 
				postfix, postfix, pi->player_id);
		std::unique_ptr<ql_msg> pIns(db_manager::instance().direct_query(query.c_str()));

		if (pIns->get()->affected_rows == 0 || pIns->get()->affected_rows == (uint32_t)-1)
		{
			metin2::log::info( "PLAYER_DELETE FAILED {:d} CANNOT INSERT TO player{}_deleted", dwPID, get_table_postfix());

			peer->encode_header(HEADER_DG_PLAYER_DELETE_FAILED, pi->handle, 1);
			peer->encode_byte(pi->account_index);
			return;
		}

		// Deletion succeeded
		metin2::log::info( "PLAYER_DELETE SUCCESS {:d}", dwPID);

		char account_index_string[16];

		snprintf(account_index_string, sizeof(account_index_string), "player_id%d", player_id_start_ + pi->account_index);

		// Remove the player table from the cache.
		player_table_cache * pkPlayerCache = player_cache(pi->player_id);

		if (pkPlayerCache)
		{
			map_player_cache_.erase(pi->player_id);
			delete pkPlayerCache;
		}

		// Remove the items from the cache.
		item_cache_set * pSet = get_item_cache_set(pi->player_id);

		if (pSet)
		{
			item_cache_set::iterator it = pSet->begin();

			while (it != pSet->end())
			{
				item_cache * pkItemCache = *it++;
				delete_item_cache(pkItemCache->get()->id);
			}

			pSet->clear();

			map_pk_item_cache_set_ptr_.erase(pi->player_id);
		}

		query = sql::format("UPDATE player_index{} SET pid{}=0 WHERE pid{}={}", 
				postfix, 
				pi->account_index + 1, 
				pi->account_index + 1, 
				pi->player_id);

		std::unique_ptr<ql_msg> pMsg(db_manager::instance().direct_query(query.c_str()));

		if (pMsg->get()->affected_rows == 0 || pMsg->get()->affected_rows == (uint32_t)-1)
		{
			metin2::log::info( "PLAYER_DELETE FAIL WHEN UPDATE account table");
			peer->encode_header(HEADER_DG_PLAYER_DELETE_FAILED, pi->handle, 1);
			peer->encode_byte(pi->account_index);
			return;
		}

		query = sql::format("DELETE FROM player{} WHERE id={}", postfix, pi->player_id);
		std::unique_ptr<ql_msg>(db_manager::instance().direct_query(query.c_str()));

		query = sql::format("DELETE FROM item{} WHERE owner_id={} AND (window < {} or window = {})", postfix, pi->player_id, SAFEBOX, DRAGON_SOUL_INVENTORY);
		std::unique_ptr<ql_msg>(db_manager::instance().direct_query(query.c_str()));

		query = sql::format("DELETE FROM quest{} WHERE dwPID={}", postfix, pi->player_id);
		db_manager::instance().async_query(query.c_str());

		query = sql::format("DELETE FROM affect{} WHERE dwPID={}", postfix, pi->player_id);
		db_manager::instance().async_query(query.c_str());

		query = sql::format("DELETE FROM guild_member{} WHERE pid={}", postfix, pi->player_id);
		db_manager::instance().async_query(query.c_str());

		// MYSHOP_PRICE_LIST
		query = sql::format("DELETE FROM myshop_pricelist{} WHERE owner_id={}", postfix, pi->player_id);
		db_manager::instance().async_query(query.c_str());
		// END_OF_MYSHOP_PRICE_LIST

		query = sql::format("DELETE FROM messenger_list{} WHERE account={} OR companion={}", postfix, strName, strName);
		db_manager::instance().async_query(query.c_str());

		peer->encode_header(HEADER_DG_PLAYER_DELETE_SUCCESS, pi->handle, 1);
		peer->encode_byte(pi->account_index);
	}
	else
	{
		// Deletion failed
		metin2::log::info( "PLAYER_DELETE FAIL NO ROW");
		peer->encode_header(HEADER_DG_PLAYER_DELETE_FAILED, pi->handle, 1);
		peer->encode_byte(pi->account_index);
	}
}

void client_manager::query_add_affect(peer * peer, packet_gd_add_affect * p)
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
			get_table_postfix(),
			p->pid,
			p->elem.type,
			p->elem.apply_on,
			p->elem.apply_value,
			p->elem.flag,
			p->elem.duration,
			p->elem.sp_cost);

	db_manager::instance().async_query(queryStr);
}

void client_manager::query_remove_affect(peer * peer, packet_gd_remove_affect * p)
{
	char queryStr[QUERY_MAX_LEN];

	snprintf(queryStr, sizeof(queryStr),
			"DELETE FROM affect%s WHERE dwPID=%u AND bType=%u AND bApplyOn=%u",
			get_table_postfix(), p->pid, p->type, p->apply_on);

	db_manager::instance().async_query(queryStr);
}


void client_manager::query_highscore_register(peer* peer, packet_gd_highscore * data)
{
	const std::string query = sql::format("SELECT value FROM highscore{} WHERE board={} AND pid = {}", sql::raw(get_table_postfix()), data->board, data->pid);

	metin2::log::info( "HEADER_GD_HIGHSCORE_REGISTER: PID {:d}", data->pid);

	client_handle_info * pi = new client_handle_info(0);
	strlcpy(pi->login, data->board, sizeof(pi->login));
	pi->account_id = (uint32_t)data->value;
	pi->player_id = data->pid;
	pi->account_index = (data->c_dir > 0);

	db_manager::instance().return_query(query.c_str(), QID_HIGHSCORE_REGISTER, peer->handle(), pi);
}

void client_manager::result_highscore_register(peer * pkPeer, ql_msg * msg)
{
	query_info * qi = (query_info *) msg->pv_user_data;
	client_handle_info * pi = (client_handle_info *) qi->pv_data;

	char szBoard[21];
	strlcpy(szBoard, pi->login, sizeof(szBoard));
	int value = (int)pi->account_id;

	ql_result * res = msg->get();

	if (res->num_rows == 0)
	{
		// Insert a new high score
		const std::string query = sql::format("INSERT INTO highscore{} VALUES({}, {}, {})", sql::raw(get_table_postfix()), szBoard, pi->player_id, value);
		db_manager::instance().async_query(query.c_str());
	}
	else
	{
		if (!res->sql_result)
		{
			delete pi;
			return;
		}

		MYSQL_ROW row = mysql_fetch_row(res->sql_result);
		if (row && row[0])
		{
			int current_value = 0; str_to_number(current_value, row[0]);
			if ((pi->account_index && current_value >= value) || (!pi->account_index && current_value <= value))
			{
				value = current_value;
			}
			else
			{
				const std::string query = sql::format("REPLACE INTO highscore{} VALUES({}, {}, {})", sql::raw(get_table_postfix()), szBoard, pi->player_id, value);
				db_manager::instance().async_query(query.c_str());
			}
		}
		else
		{
			const std::string query = sql::format("INSERT INTO highscore{} VALUES({}, {}, {})", sql::raw(get_table_postfix()), szBoard, pi->player_id, value);
			db_manager::instance().async_query(query.c_str());
		}
	}
	// TODO: check here whether the high score was updated and broadcast a notice.
	delete pi;
}

void client_manager::insert_logout_player(uint32_t pid)
{
	logout_player_map::iterator it = map_logout_.find(pid);

	// Add if it doesn't exist
	if (it != map_logout_.end())
	{
		// If it exists, only update the time
		if (metin2::config::db().login_log)
			metin2::log::info( "LOGOUT: Update player time pid({:d})", pid);

		it->second->time = time(nullptr);
		return;
	}
		
	auto pLogout = std::make_unique<logout_player>();
	pLogout->pid = pid;
	pLogout->time = time(nullptr);
	map_logout_.insert(std::make_pair(pid, std::move(pLogout)));

	if (metin2::config::db().login_log)
		metin2::log::info( "LOGOUT: Insert player pid({:d})", pid);
}

void client_manager::delete_logout_player(uint32_t pid)
{
	logout_player_map::iterator it = map_logout_.find(pid);

	if (it != map_logout_.end())
	{
		map_logout_.erase(it);
	}
}


void client_manager::update_logout_player()
{
	time_t now = time(nullptr);

	logout_player_map::iterator it = map_logout_.begin();

	while (it != map_logout_.end())
	{
		logout_player* pLogout = it->second.get();

		if (now - logout_seconds > pLogout->time)
		{
			flush_item_cache_set(pLogout->pid);
			flush_player_cache_set(pLogout->pid);

			map_logout_.erase(it++);
		}
		else
			++it;
	}
}

void client_manager::flush_player_cache_set(uint32_t pid)
{
	player_table_cache_map::iterator it = map_player_cache_.find(pid);

	if (it != map_player_cache_.end())
	{
		player_table_cache * c = it->second;
		map_player_cache_.erase(it);

		c->flush();
		delete c; 
	}
}
}
