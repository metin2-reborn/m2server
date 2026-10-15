
#include "ClientManager.h"

#include "Cache.h"
#include "DBManager.h"
#include "GuildManager.h"
#include "ItemAwardManager.h"
#include "ItemIDRangeManager.h"
#include "LoginData.h"
#include "Main.h"
#include "Marriage.h"
#include "Monarch.h"
#include "MoneyLog.h"
#include "Peer.h"
#include "PrivManager.h"
#include "QID.h"

#include <common/building.h>
#include <common/length.h>
#include <common/peer_security.h>
#include <common/tables.h>
#include <common/utils.h>
#include <libgame/grid.h>
#include <libnet/connection.h>
#include <libnet/io.h>
#include <libnet/listener.h>
#include <libsql/AsyncSQL.h>
#include <libsql/query.h>
#include <libthecore/core_signal.h>
#include <metin2/config/config.h>
#include <metin2/log/log.h>
#include <libthecore/main.h>
#include <libthecore/utils.h>

#include <mysql.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <strings.h>

namespace metin2::db {

bool CreateItemTableFromRes(MYSQL_RES * res, std::vector<TPlayerItem> * pVec, uint32_t dwPID);

uint32_t g_dwUsageMax = 0;
uint32_t g_dwUsageAvg = 0;

int g_item_count = 0;
int g_query_count[2];

CClientManager::CClientManager() :
	m_pkAuthPeer(nullptr),
	m_iPlayerIDStart(0),
	m_iPlayerDeleteLevelLimit(0),
	m_iPlayerDeleteLevelLimitLower(0),
	m_iShopTableSize(0),
	m_pShopTable(nullptr),
	m_iRefineTableSize(0),
	m_pRefineTable(nullptr),
	m_bShutdowned(false),
	m_iCacheFlushCount(0),
	m_iCacheFlushCountLimit(200)
{
	m_itemRange.dwMin = 0;
	m_itemRange.dwMax = 0;
	m_itemRange.dwUsableItemIDMin = 0;

	memset(g_query_count, 0, sizeof(g_query_count));
}

CClientManager::~CClientManager()
{
	Destroy();
}

void CClientManager::SetPlayerIDStart(int iIDStart)
{
	m_iPlayerIDStart = iIDStart;
}

void CClientManager::Destroy()
{
	m_mChannelStatus.clear();
	for (CPeer * peer : m_peerList)
		delete peer;

	m_peerList.clear();

	if (m_listener)
	{
		m_listener->close();
		m_listener.reset();
	}
}

bool CClientManager::Initialize()
{


	//BOOT_LOCALIZATION
	if (!InitializeLocalization())
	{
		fprintf(stderr, "Failed Localization Infomation so exit\n");
		return false;
	}

	//END_BOOT_LOCALIZATION
	//ITEM_UNIQUE_ID

	if (!InitializeNowItemID())
	{
		fprintf(stderr, " Item range Initialize Failed. Exit DBCache Server\n");
		return false;
	}
	//END_ITEM_UNIQUE_ID

	if (!InitializeTables())
	{
		metin2::log::error("Table Initialize FAILED");
		return false;
	}

	CGuildManager::instance().BootReserveWar();

	const metin2::config::db_settings & db = metin2::config::db();

	m_listener = net::listener::open(db.bind_ip, db.port, [this](net::tcp::socket socket) { AddPeer(std::move(socket)); });

	if (!m_listener)
		return false;

	metin2::log::info("LISTEN: {}:{:d}", db.bind_ip, db.port);

	m_iPlayerDeleteLevelLimit = db.player_delete_level_limit > 0 ? db.player_delete_level_limit : PLAYER_MAX_LEVEL_CONST + 1;
	m_iPlayerDeleteLevelLimitLower = db.player_delete_level_limit_lower;

	metin2::log::info("PLAYER_DELETE_LEVEL_LIMIT set to {:d}", m_iPlayerDeleteLevelLimit);
	metin2::log::info("PLAYER_DELETE_LEVEL_LIMIT_LOWER set to {:d}", m_iPlayerDeleteLevelLimitLower);

	LoadEventFlag();

	// Force the database character set

	return true;
}

void CClientManager::MainLoop()
{
	metin2::log::info( "ClientManager pointer is {}", static_cast<const void *>(this));

	// Main loop
	while (!m_bShutdowned)
	{
		while (std::unique_ptr<SQLMsg> tmp{CDBManager::instance().PopResult()})
		{
			AnalyzeQueryResult(tmp.get());
		}

		if (!Process())
			break;

		metin2::log::rotate();
	}

	//
	// Main loop shutdown handling
	//
	metin2::log::info( "MainLoop exited, Starting cache flushing");

	signal_timer_disable();

	auto it = m_map_playerCache.begin();

	//Flush player table cache	
	while (it != m_map_playerCache.end())
	{
		CPlayerTableCache * c = (it++)->second;

		c->Flush();
		delete c;
	}
	m_map_playerCache.clear();

	auto it2 = m_map_itemCache.begin();
	//Flush items
	while (it2 != m_map_itemCache.end())
	{
		CItemCache * c = (it2++)->second;

		c->Flush();
		delete c;
	}
	m_map_itemCache.clear();

	// MYSHOP_PRICE_LIST
	//
	// Flush private shop item price list
	//
	for (auto itPriceList = m_mapItemPriceListCache.begin(); itPriceList != m_mapItemPriceListCache.end(); ++itPriceList)
	{
		CItemPriceListTableCache* pCache = itPriceList->second;
		pCache->Flush();
		delete pCache;
	}

	m_mapItemPriceListCache.clear();
	// END_OF_MYSHOP_PRICE_LIST
}

void CClientManager::QUERY_BOOT(CPeer* peer, TPacketGDBoot * p)
{
	const uint8_t bPacketVersion = 6; // Bump the number whenever the BOOT packet changes.

	std::vector<tAdminInfo> vAdmin;
	std::vector<std::string> vHost;

	__GetHostInfo(vHost);
	__GetAdminInfo(p->szIP, vAdmin);	

	metin2::log::info( "QUERY_BOOT : AdminInfo (Request ServerIp {}) ", p->szIP);

	uint32_t dwPacketSize = 
		sizeof(uint32_t) +
		sizeof(uint8_t) +
		sizeof(uint16_t) + sizeof(uint16_t) + sizeof(TMobTable) * m_vec_mobTable.size() +
		sizeof(uint16_t) + sizeof(uint16_t) + sizeof(TItemTable) * m_vec_itemTable.size() +
		sizeof(uint16_t) + sizeof(uint16_t) + sizeof(TShopTable) * m_iShopTableSize +
		sizeof(uint16_t) + sizeof(uint16_t) + sizeof(TSkillTable) * m_vec_skillTable.size() +
		sizeof(uint16_t) + sizeof(uint16_t) + sizeof(TRefineTable) * m_iRefineTableSize +
		sizeof(uint16_t) + sizeof(uint16_t) + sizeof(TItemAttrTable) * m_vec_itemAttrTable.size() +
		sizeof(uint16_t) + sizeof(uint16_t) + sizeof(TItemAttrTable) * m_vec_itemRareTable.size() +
		sizeof(uint16_t) + sizeof(uint16_t) + sizeof(TBanwordTable) * m_vec_banwordTable.size() +
		sizeof(uint16_t) + sizeof(uint16_t) + sizeof(building::TLand) * m_vec_kLandTable.size() +
		sizeof(uint16_t) + sizeof(uint16_t) + sizeof(building::TObjectProto) * m_vec_kObjectProto.size() + 
		sizeof(uint16_t) + sizeof(uint16_t) + sizeof(building::TObject) * m_map_pkObjectTable.size() +
		sizeof(int32_t) +
		sizeof(uint16_t) + sizeof(uint16_t) + sizeof(TItemIDRangeTable)*2 +
		//ADMIN_MANAGER
		sizeof(uint16_t) + sizeof(uint16_t) + 16 * vHost.size() +
		sizeof(uint16_t) + sizeof(uint16_t) +  sizeof(tAdminInfo) *  vAdmin.size() +
		//END_ADMIN_MANAGER
		sizeof(uint16_t) + sizeof(uint16_t) + sizeof(MonarchInfo) + 
		sizeof(uint16_t) + sizeof(uint16_t) + sizeof(MonarchCandidacy)* CMonarch::instance().MonarchCandidacySize() +
		sizeof(uint16_t); 

	peer->EncodeHeader(HEADER_DG_BOOT, 0, dwPacketSize);
	peer->Encode(&dwPacketSize, sizeof(uint32_t));
	peer->Encode(&bPacketVersion, sizeof(uint8_t));

	metin2::log::info( "BOOT: PACKET: {:d}", dwPacketSize);
	metin2::log::info( "BOOT: VERSION: {:d}", bPacketVersion);

	metin2::log::info( "sizeof(TMobTable) = {:d}", sizeof(TMobTable));
	metin2::log::info( "sizeof(TItemTable) = {:d}", sizeof(TItemTable));
	metin2::log::info( "sizeof(TShopTable) = {:d}", sizeof(TShopTable));
	metin2::log::info( "sizeof(TSkillTable) = {:d}", sizeof(TSkillTable));
	metin2::log::info( "sizeof(TRefineTable) = {:d}", sizeof(TRefineTable));
	metin2::log::info( "sizeof(TItemAttrTable) = {:d}", sizeof(TItemAttrTable));
	metin2::log::info( "sizeof(TItemRareTable) = {:d}", sizeof(TItemAttrTable));
	metin2::log::info( "sizeof(TBanwordTable) = {:d}", sizeof(TBanwordTable));
	metin2::log::info( "sizeof(TLand) = {:d}", sizeof(building::TLand));
	metin2::log::info( "sizeof(TObjectProto) = {:d}", sizeof(building::TObjectProto));
	metin2::log::info( "sizeof(TObject) = {:d}", sizeof(building::TObject));
	//ADMIN_MANAGER
	metin2::log::info( "sizeof(tAdminInfo) total = {:d} ", sizeof(tAdminInfo) * vAdmin.size());
	//END_ADMIN_MANAGER
	metin2::log::info( "sizeof(TMonarchInfo) = {:d}", sizeof(MonarchInfo));

	peer->EncodeWORD(sizeof(TMobTable));
	peer->EncodeWORD(m_vec_mobTable.size());
	peer->Encode(&m_vec_mobTable[0], sizeof(TMobTable) * m_vec_mobTable.size());

	peer->EncodeWORD(sizeof(TItemTable));
	peer->EncodeWORD(m_vec_itemTable.size());
	peer->Encode(&m_vec_itemTable[0], sizeof(TItemTable) * m_vec_itemTable.size());

	peer->EncodeWORD(sizeof(TShopTable));
	peer->EncodeWORD(m_iShopTableSize);
	peer->Encode(m_pShopTable.get(), sizeof(TShopTable) * m_iShopTableSize);

	peer->EncodeWORD(sizeof(TSkillTable));
	peer->EncodeWORD(m_vec_skillTable.size());
	peer->Encode(&m_vec_skillTable[0], sizeof(TSkillTable) * m_vec_skillTable.size());

	peer->EncodeWORD(sizeof(TRefineTable));
	peer->EncodeWORD(m_iRefineTableSize);
	peer->Encode(m_pRefineTable.get(), sizeof(TRefineTable) * m_iRefineTableSize);

	peer->EncodeWORD(sizeof(TItemAttrTable));
	peer->EncodeWORD(m_vec_itemAttrTable.size());
	peer->Encode(&m_vec_itemAttrTable[0], sizeof(TItemAttrTable) * m_vec_itemAttrTable.size());

	peer->EncodeWORD(sizeof(TItemAttrTable));
	peer->EncodeWORD(m_vec_itemRareTable.size());
	peer->Encode(&m_vec_itemRareTable[0], sizeof(TItemAttrTable) * m_vec_itemRareTable.size());

	peer->EncodeWORD(sizeof(TBanwordTable));
	peer->EncodeWORD(m_vec_banwordTable.size());
	peer->Encode(&m_vec_banwordTable[0], sizeof(TBanwordTable) * m_vec_banwordTable.size());

	peer->EncodeWORD(sizeof(building::TLand));
	peer->EncodeWORD(m_vec_kLandTable.size());
	peer->Encode(&m_vec_kLandTable[0], sizeof(building::TLand) * m_vec_kLandTable.size());

	peer->EncodeWORD(sizeof(building::TObjectProto));
	peer->EncodeWORD(m_vec_kObjectProto.size());
	peer->Encode(&m_vec_kObjectProto[0], sizeof(building::TObjectProto) * m_vec_kObjectProto.size());

	peer->EncodeWORD(sizeof(building::TObject));
	peer->EncodeWORD(m_map_pkObjectTable.size());

	auto it = m_map_pkObjectTable.begin();

	while (it != m_map_pkObjectTable.end())
		peer->Encode((it++)->second.get(), sizeof(building::TObject));

	// Auction Boot
	int32_t now = time(nullptr);
	peer->Encode(&now, sizeof(int32_t));

	TItemIDRangeTable itemRange = CItemIDRangeManager::instance().GetRange();
	TItemIDRangeTable itemRangeSpare = CItemIDRangeManager::instance().GetRange();

	peer->EncodeWORD(sizeof(TItemIDRangeTable));
	peer->EncodeWORD(1);
	peer->Encode(&itemRange, sizeof(TItemIDRangeTable));
	peer->Encode(&itemRangeSpare, sizeof(TItemIDRangeTable));

	peer->SetItemIDRange(itemRange);
	peer->SetSpareItemIDRange(itemRangeSpare);

	//ADMIN_MANAGER
	peer->EncodeWORD(16);
	peer->EncodeWORD(vHost.size());

	for (size_t n = 0; n < vHost.size(); ++n)
	{
		peer->Encode(vHost[n].c_str(), 16);
		metin2::log::info( "GMHosts {}", vHost[n].c_str());
	}

	peer->EncodeWORD(sizeof(tAdminInfo));
	peer->EncodeWORD(vAdmin.size());

	for (size_t n = 0; n < vAdmin.size(); ++n)
	{
		peer->Encode(&vAdmin[n], sizeof(tAdminInfo));
		metin2::log::info( "Admin name {} ConntactIP {}", vAdmin[n].m_szName, vAdmin[n].m_szContactIP);
	}
	//END_ADMIN_MANAGER

	//MONARCH
	peer->EncodeWORD(sizeof(MonarchInfo));
	peer->EncodeWORD(1);
	peer->Encode(CMonarch::instance().GetMonarch(), sizeof(MonarchInfo));

	CMonarch::VEC_MONARCHCANDIDACY & rVecMonarchCandidacy = CMonarch::instance().GetVecMonarchCandidacy();

	size_t num_monarch_candidacy = CMonarch::instance().MonarchCandidacySize();
	peer->EncodeWORD(sizeof(MonarchCandidacy));
	peer->EncodeWORD(num_monarch_candidacy);
	if (num_monarch_candidacy != 0) {
		peer->Encode(&rVecMonarchCandidacy[0], sizeof(MonarchCandidacy) * num_monarch_candidacy);
	}
	//END_MONARCE

	if (metin2::config::test_server())
		metin2::log::info( "MONARCHCandidacy Size {:d}", CMonarch::instance().MonarchCandidacySize());

	peer->EncodeWORD(0xffff);
}

void CClientManager::SendPartyOnSetup(CPeer* pkPeer)
{
	TPartyMap & pm = m_map_pkChannelParty[pkPeer->GetChannel()];

	for (auto it_party = pm.begin(); it_party != pm.end(); ++it_party)
	{
		metin2::log::info( "PARTY SendPartyOnSetup Party [{:d}]", it_party->first);
		pkPeer->EncodeHeader(HEADER_DG_PARTY_CREATE, 0, sizeof(TPacketPartyCreate));
		pkPeer->Encode(&it_party->first, sizeof(uint32_t));

		for (auto it_member = it_party->second.begin(); it_member != it_party->second.end(); ++it_member)
		{
			metin2::log::info( "PARTY SendPartyOnSetup Party [{:d}] Member [{:d}]", it_party->first, it_member->first);
			pkPeer->EncodeHeader(HEADER_DG_PARTY_ADD, 0, sizeof(TPacketPartyAdd));
			pkPeer->Encode(&it_party->first, sizeof(uint32_t));
			pkPeer->Encode(&it_member->first, sizeof(uint32_t));
			pkPeer->Encode(&it_member->second.bRole, sizeof(uint8_t));

			pkPeer->EncodeHeader(HEADER_DG_PARTY_SET_MEMBER_LEVEL, 0, sizeof(TPacketPartySetMemberLevel));
			pkPeer->Encode(&it_party->first, sizeof(uint32_t));
			pkPeer->Encode(&it_member->first, sizeof(uint32_t));
			pkPeer->Encode(&it_member->second.bLevel, sizeof(uint8_t));
		}
	}
}

void CClientManager::QUERY_PLAYER_COUNT(CPeer * pkPeer, TPlayerCountPacket * pPacket)
{
	pkPeer->SetUserCount(pPacket->dwCount);
}

void CClientManager::QUERY_QUEST_SAVE(CPeer * pkPeer, TQuestTable * pTable, uint32_t dwLen)
{
	if (0 != (dwLen % sizeof(TQuestTable)))
	{
		metin2::log::error("invalid packet size {:d}, sizeof(TQuestTable) == {:d}", dwLen, sizeof(TQuestTable));
		return;
	}

	int iSize = dwLen / sizeof(TQuestTable);

	std::string query;

	for (int i = 0; i < iSize; ++i, ++pTable)
	{
		if (pTable->lValue == 0)
		{
			query = sql::format(
					"DELETE FROM quest{} WHERE dwPID={} AND szName={} AND szState={}",
					sql::raw(GetTablePostfix()), pTable->dwPID, pTable->szName, pTable->szState);
		}
		else
		{
			query = sql::format(
					"REPLACE INTO quest{} (dwPID, szName, szState, lValue) VALUES({}, {}, {}, {})",
					sql::raw(GetTablePostfix()), pTable->dwPID, pTable->szName, pTable->szState, pTable->lValue);
		}

		CDBManager::instance().ReturnQuery(query.c_str(), QID_QUEST_SAVE, pkPeer->GetHandle(), nullptr);
	}
}

void CClientManager::QUERY_SAFEBOX_LOAD(CPeer * pkPeer, uint32_t dwHandle, TSafeboxLoadPacket * packet, bool bMall)
{
	ClientHandleInfo * pi = new ClientHandleInfo(dwHandle);
	strlcpy(pi->safebox_password, packet->szPassword, sizeof(pi->safebox_password));
	pi->account_id = packet->dwID;
	pi->account_index = 0;
	pi->ip[0] = bMall ? 1 : 0;
	strlcpy(pi->login, packet->szLogin, sizeof(pi->login));

	char szQuery[QUERY_MAX_LEN];
	snprintf(szQuery, sizeof(szQuery),
			"SELECT account_id, size, password FROM safebox%s WHERE account_id=%u",
			GetTablePostfix(), packet->dwID);

	if (metin2::config::db().login_log)
		metin2::log::info( "HEADER_GD_SAFEBOX_LOAD (handle: {:d} account.id {:d} is_mall {:d})", dwHandle, packet->dwID, bMall ? 1 : 0);

	CDBManager::instance().ReturnQuery(szQuery, QID_SAFEBOX_LOAD, pkPeer->GetHandle(), pi);
}

void CClientManager::RESULT_SAFEBOX_LOAD(CPeer * pkPeer, SQLMsg * msg)
{
	CQueryInfo * qi = (CQueryInfo *) msg->pvUserData;
	ClientHandleInfo * pi = (ClientHandleInfo *) qi->pvData;
	uint32_t dwHandle = pi->dwHandle;

	// account_index used here means the query order.
	// The first query, to find the password, is 0
	// The second query, to fetch the actual data, is 1

	if (pi->account_index == 0)
	{
		std::string strSafeboxPassword = pi->safebox_password;

		auto pSafebox = std::make_unique<TSafeboxTable>();
		memset(pSafebox.get(), 0, sizeof(TSafeboxTable));

		SQLResult * res = msg->Get();

		if (res->uiNumRows == 0)
		{
			if (strSafeboxPassword != "000000")
			{
				pkPeer->EncodeHeader(HEADER_DG_SAFEBOX_WRONG_PASSWORD, dwHandle, 0);
				delete pi;
				return;
			}
		}
		else
		{
			MYSQL_ROW row = mysql_fetch_row(res->pSQLResult);

			// If the password is wrong..
			if (((!row[2] || !*row[2]) && strSafeboxPassword != "000000") ||
				((row[2] && *row[2]) && strSafeboxPassword != row[2]))
			{
				pkPeer->EncodeHeader(HEADER_DG_SAFEBOX_WRONG_PASSWORD, dwHandle, 0);
				delete pi;
				return;
			}

			if (!row[0])
				pSafebox->dwID = 0;
			else
				str_to_number(pSafebox->dwID, row[0]);

			if (!row[1])
				pSafebox->bSize = 0;
			else
				str_to_number(pSafebox->bSize, row[1]);
			if (pi->ip[0] == 1)
			{
				pSafebox->bSize = 1;
				metin2::log::info( "MALL id[{:d}] size[{:d}]", pSafebox->dwID, pSafebox->bSize);
			}
			else
				metin2::log::info( "SAFEBOX id[{:d}] size[{:d}]", pSafebox->dwID, pSafebox->bSize);
		}

		if (0 == pSafebox->dwID)
			pSafebox->dwID = pi->account_id;

		pi->pSafebox = std::move(pSafebox);

		const std::string query = sql::format(
				"SELECT id, window+0, pos, count, vnum, socket0, socket1, socket2, "
				"attrtype0, attrvalue0, "
				"attrtype1, attrvalue1, "
				"attrtype2, attrvalue2, "
				"attrtype3, attrvalue3, "
				"attrtype4, attrvalue4, "
				"attrtype5, attrvalue5, "
				"attrtype6, attrvalue6 "
				"FROM item{} WHERE owner_id={} AND window={}",
				sql::raw(GetTablePostfix()), pi->account_id, pi->ip[0] == 0 ? "SAFEBOX" : "MALL");

		pi->account_index = 1;

		CDBManager::instance().ReturnQuery(query.c_str(), QID_SAFEBOX_LOAD, pkPeer->GetHandle(), pi);
	}
	else
	{

		if (!pi->pSafebox)
		{
			metin2::log::error("null safebox pointer!");
			delete pi;
			return;
		}

		// There was a query error; if we respond, the safebox would look empty,
		// so it's better for the safebox not to open at all
		if (!msg->Get()->pSQLResult)
		{
			metin2::log::error("null safebox result");
			delete pi;
			return;
		}

		static std::vector<TPlayerItem> s_items;
		CreateItemTableFromRes(msg->Get()->pSQLResult, &s_items, pi->account_id);

		std::set<TItemAward *> * pSet = ItemAwardManager::instance().GetByLogin(pi->login);

		if (pSet && !m_vec_itemTable.empty())
		{

			CGrid grid(5, std::max<int>(1, pi->pSafebox->bSize) * 9);
			bool bEscape = false;

			for (uint32_t i = 0; i < s_items.size(); ++i)
			{
				TPlayerItem & r = s_items[i];

				auto it = m_map_itemTableByVnum.find(r.vnum);

				if (it == m_map_itemTableByVnum.end())
				{
					bEscape = true;
					metin2::log::error("invalid item vnum {:d} in safebox: login {}", r.vnum, pi->login);
					break;
				}

				grid.Put(r.pos, 1, it->second->bSize);
			}

			if (!bEscape)
			{
				std::vector<std::pair<uint32_t, uint32_t> > vec_dwFinishedAwardID;

				__typeof(pSet->begin()) it = pSet->begin();

				std::string query;

				while (it != pSet->end())
				{
					TItemAward * pItemAward = *(it++);
					const uint32_t& dwItemVnum = pItemAward->dwVnum;

					if (pItemAward->bTaken)
						continue;

					if (pi->ip[0] == 0 && pItemAward->bMall)
						continue;

					if (pi->ip[0] == 1 && !pItemAward->bMall)
						continue;

					auto it = m_map_itemTableByVnum.find(pItemAward->dwVnum);

					if (it == m_map_itemTableByVnum.end())
					{
						metin2::log::error("invalid item vnum {:d} in item_award: login {}", pItemAward->dwVnum, pi->login);
						continue;
					}

					TItemTable * pItemTable = it->second;

					int iPos;

					if ((iPos = grid.FindBlank(1, it->second->bSize)) == -1)
						break;

					TPlayerItem item;
					memset(&item, 0, sizeof(TPlayerItem));

					uint32_t dwSocket2 = pItemAward->dwSocket2; //Fix

					if (pItemTable->bType == ITEM_UNIQUE)
					{
						if (pItemAward->dwSocket2 != 0)
							dwSocket2 = pItemAward->dwSocket2;
						else
							dwSocket2 = pItemTable->alValues[0];
					}
					else if ((dwItemVnum == 50300 || dwItemVnum == 70037) && pItemAward->dwSocket0 == 0)
					{
						uint32_t dwSkillIdx;
						uint32_t dwSkillVnum;

						do
						{
							dwSkillIdx = number(0, m_vec_skillTable.size()-1);

							dwSkillVnum = m_vec_skillTable[dwSkillIdx].dwVnum;

							if (dwSkillVnum > 120)
								continue;

							break;
						} while (1);

						pItemAward->dwSocket0 = dwSkillVnum;
					}
					else
					{
						switch (dwItemVnum)
						{
							case 72723: case 72724: case 72725: case 72726:
							case 72727: case 72728: case 72729: case 72730:
							// It's horrible, but fixing what was done before is scary...
							// So just hard-coded. Auto-potion items for gift boxes.
							case 76004: case 76005: case 76021: case 76022:
							case 79012: case 79013:
								if (pItemAward->dwSocket2 == 0)
								{
									dwSocket2 = pItemTable->alValues[0];
								}
								else
								{
									dwSocket2 = pItemAward->dwSocket2;
								}
								break;
						}
					}

					if (GetItemID () > m_itemRange.dwMax)
					{
						metin2::log::error("UNIQUE ID OVERFLOW!!");
						break;
					}

					{
						auto it = m_map_itemTableByVnum.find (dwItemVnum);
						if (it == m_map_itemTableByVnum.end())
						{
							metin2::log::error("Invalid item(vnum : {:d}). It is not in m_map_itemTableByVnum.", dwItemVnum);
							continue;
						}
						TItemTable* item_table = it->second;
						if (item_table == nullptr)
						{
							metin2::log::error("Invalid item_table (vnum : {:d}). It's value is NULL in m_map_itemTableByVnum.", dwItemVnum);
							continue;
						}
						if (0 == pItemAward->dwSocket0)
						{
							for (int i = 0; i < ITEM_LIMIT_MAX_NUM; i++)
							{
								if (LIMIT_REAL_TIME == item_table->aLimits[i].bType)
								{
									if (0 == item_table->aLimits[i].lValue)
										pItemAward->dwSocket0 = time(nullptr) + 60 * 60 * 24 * 7;
									else
										pItemAward->dwSocket0 = time(nullptr) + item_table->aLimits[i].lValue;

									break;
								}
								else if (LIMIT_REAL_TIME_START_FIRST_USE == item_table->aLimits[i].bType || LIMIT_TIMER_BASED_ON_WEAR == item_table->aLimits[i].bType)
								{
									if (0 == item_table->aLimits[i].lValue)
										pItemAward->dwSocket0 = 60 * 60 * 24 * 7;
									else
										pItemAward->dwSocket0 = item_table->aLimits[i].lValue;

									break;
								}
							}
						}

						query = sql::format(
								"INSERT INTO item{} (id, owner_id, window, pos, vnum, count, socket0, socket1, socket2) "
								"VALUES({}, {}, {}, {}, {}, {}, {}, {}, {})",
								sql::raw(GetTablePostfix()),
								GainItemID(),
								pi->account_id,
								pi->ip[0] == 0 ? "SAFEBOX" : "MALL",
								iPos,
								pItemAward->dwVnum, pItemAward->dwCount, pItemAward->dwSocket0, pItemAward->dwSocket1, dwSocket2);
					}

					std::unique_ptr<SQLMsg> pmsg(CDBManager::instance().DirectQuery(query.c_str()));
					SQLResult * pRes = pmsg->Get();
					metin2::log::info( "SAFEBOX Query : [{}]", query.c_str());

					if (pRes->uiAffectedRows == 0 || pRes->uiInsertID == 0 || pRes->uiAffectedRows == (uint32_t)-1)
						break;

					item.id = pmsg->Get()->uiInsertID;
					item.window = pi->ip[0] == 0 ? SAFEBOX : MALL,
					item.pos = iPos;
					item.count = pItemAward->dwCount;
					item.vnum = pItemAward->dwVnum;
					item.alSockets[0] = pItemAward->dwSocket0;
					item.alSockets[1] = pItemAward->dwSocket1;
					item.alSockets[2] = dwSocket2;
					s_items.push_back(item);

					vec_dwFinishedAwardID.push_back(std::make_pair(pItemAward->dwID, item.id));
					grid.Put(iPos, 1, it->second->bSize);
				}

				for (uint32_t i = 0; i < vec_dwFinishedAwardID.size(); ++i)
					ItemAwardManager::instance().Taken(vec_dwFinishedAwardID[i].first, vec_dwFinishedAwardID[i].second);
			}
		}

		pi->pSafebox->wItemCount = s_items.size();

		pkPeer->EncodeHeader(pi->ip[0] == 0 ? HEADER_DG_SAFEBOX_LOAD : HEADER_DG_MALL_LOAD, dwHandle, sizeof(TSafeboxTable) + sizeof(TPlayerItem) * s_items.size());

		pkPeer->Encode(pi->pSafebox.get(), sizeof(TSafeboxTable));

		if (!s_items.empty())
			pkPeer->Encode(&s_items[0], sizeof(TPlayerItem) * s_items.size());

		delete pi;
	}
}

void CClientManager::QUERY_SAFEBOX_CHANGE_SIZE(CPeer * pkPeer, uint32_t dwHandle, TSafeboxChangeSizePacket * p)
{
	ClientHandleInfo * pi = new ClientHandleInfo(dwHandle);
	pi->account_index = p->bSize;	// Temporarily use account_index as the size

	char szQuery[QUERY_MAX_LEN];

	if (p->bSize == 1)
		snprintf(szQuery, sizeof(szQuery), "INSERT INTO safebox%s (account_id, size) VALUES(%u, %u)", GetTablePostfix(), p->dwID, p->bSize);
	else
		snprintf(szQuery, sizeof(szQuery), "UPDATE safebox%s SET size=%u WHERE account_id=%u", GetTablePostfix(), p->bSize, p->dwID);

	CDBManager::instance().ReturnQuery(szQuery, QID_SAFEBOX_CHANGE_SIZE, pkPeer->GetHandle(), pi);
}

void CClientManager::RESULT_SAFEBOX_CHANGE_SIZE(CPeer * pkPeer, SQLMsg * msg)
{
	CQueryInfo * qi = (CQueryInfo *) msg->pvUserData;
	ClientHandleInfo * p = (ClientHandleInfo *) qi->pvData;
	uint32_t dwHandle = p->dwHandle;
	uint8_t bSize = p->account_index;

	delete p;

	if (msg->Get()->uiNumRows > 0)
	{
		pkPeer->EncodeHeader(HEADER_DG_SAFEBOX_CHANGE_SIZE, dwHandle, sizeof(uint8_t));
		pkPeer->EncodeBYTE(bSize);
	}
}

void CClientManager::QUERY_SAFEBOX_CHANGE_PASSWORD(CPeer * pkPeer, uint32_t dwHandle, TSafeboxChangePasswordPacket * p)
{
	ClientHandleInfo * pi = new ClientHandleInfo(dwHandle);
	strlcpy(pi->safebox_password, p->szNewPassword, sizeof(pi->safebox_password));
	strlcpy(pi->login, p->szOldPassword, sizeof(pi->login));
	pi->account_id = p->dwID;

	char szQuery[QUERY_MAX_LEN];
	snprintf(szQuery, sizeof(szQuery), "SELECT password FROM safebox%s WHERE account_id=%u", GetTablePostfix(), p->dwID);

	CDBManager::instance().ReturnQuery(szQuery, QID_SAFEBOX_CHANGE_PASSWORD, pkPeer->GetHandle(), pi);
}

void CClientManager::RESULT_SAFEBOX_CHANGE_PASSWORD(CPeer * pkPeer, SQLMsg * msg)
{
	CQueryInfo * qi = (CQueryInfo *) msg->pvUserData;
	ClientHandleInfo * p = (ClientHandleInfo *) qi->pvData;
	uint32_t dwHandle = p->dwHandle;

	if (msg->Get()->uiNumRows > 0)
	{
		MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);

		if ((row[0] && *row[0] && !strcasecmp(row[0], p->login)) || ((!row[0] || !*row[0]) && !strcmp("000000", p->login)))
		{
			const std::string query = sql::format("UPDATE safebox{} SET password={} WHERE account_id={}", sql::raw(GetTablePostfix()), p->safebox_password, p->account_id);

			CDBManager::instance().ReturnQuery(query.c_str(), QID_SAFEBOX_CHANGE_PASSWORD_SECOND, pkPeer->GetHandle(), p);
			return;
		}
	}

	delete p;

	// Wrong old password
	pkPeer->EncodeHeader(HEADER_DG_SAFEBOX_CHANGE_PASSWORD_ANSWER, dwHandle, sizeof(uint8_t));
	pkPeer->EncodeBYTE(0);
}

void CClientManager::RESULT_SAFEBOX_CHANGE_PASSWORD_SECOND(CPeer * pkPeer, SQLMsg * msg)
{
	CQueryInfo * qi = (CQueryInfo *) msg->pvUserData;
	ClientHandleInfo * p = (ClientHandleInfo *) qi->pvData;
	uint32_t dwHandle = p->dwHandle;
	delete p;

	pkPeer->EncodeHeader(HEADER_DG_SAFEBOX_CHANGE_PASSWORD_ANSWER, dwHandle, sizeof(uint8_t));
	pkPeer->EncodeBYTE(1);
}

// MYSHOP_PRICE_LIST
void CClientManager::RESULT_PRICELIST_LOAD(CPeer* peer, SQLMsg* pMsg)
{
	TItemPricelistReqInfo* pReqInfo = (TItemPricelistReqInfo*)static_cast<CQueryInfo*>(pMsg->pvUserData)->pvData;

	//
	// Store info loaded from DB in the cache
	//

	TItemPriceListTable table;
	table.dwOwnerID = pReqInfo->second;
	table.byCount = 0;

	MYSQL_ROW row;

	while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
	{
		str_to_number(table.aPriceInfo[table.byCount].dwVnum, row[0]);
		str_to_number(table.aPriceInfo[table.byCount].dwPrice, row[1]);
		table.byCount++;
	}

	PutItemPriceListCache(&table);

	//
	// Send the loaded data to the game server
	//

	TPacketMyshopPricelistHeader header;

	header.dwOwnerID = pReqInfo->second;
	header.byCount = table.byCount;

	size_t sizePriceListSize = sizeof(TItemPriceInfo) * header.byCount;

	peer->EncodeHeader(HEADER_DG_MYSHOP_PRICELIST_RES, pReqInfo->first, sizeof(header) + sizePriceListSize);
	peer->Encode(&header, sizeof(header));
	peer->Encode(table.aPriceInfo, sizePriceListSize);

	metin2::log::info( "Load MyShopPricelist handle[{:d}] pid[{:d}] count[{:d}]", pReqInfo->first, pReqInfo->second, header.byCount);

	delete pReqInfo;
}

void CClientManager::RESULT_PRICELIST_LOAD_FOR_UPDATE(SQLMsg* pMsg)
{
	TItemPriceListTable* pUpdateTable = (TItemPriceListTable*)static_cast<CQueryInfo*>(pMsg->pvUserData)->pvData;

	//
	// Store info loaded from DB in the cache
	//

	TItemPriceListTable table;
	table.dwOwnerID = pUpdateTable->dwOwnerID;
	table.byCount = 0;

	MYSQL_ROW row;

	while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
	{
		str_to_number(table.aPriceInfo[table.byCount].dwVnum, row[0]);
		str_to_number(table.aPriceInfo[table.byCount].dwPrice, row[1]);
		table.byCount++;
	}

	PutItemPriceListCache(&table);

	// Update cache
	GetItemPriceListCache(pUpdateTable->dwOwnerID)->UpdateList(pUpdateTable);

	delete pUpdateTable;
}
// END_OF_MYSHOP_PRICE_LIST

void CClientManager::QUERY_SAFEBOX_SAVE(CPeer * pkPeer, TSafeboxTable * pTable)
{
	char szQuery[QUERY_MAX_LEN];

	snprintf(szQuery, sizeof(szQuery),
			"UPDATE safebox%s SET gold='%u' WHERE account_id=%u", 
			GetTablePostfix(), pTable->dwGold, pTable->dwID);

	CDBManager::instance().ReturnQuery(szQuery, QID_SAFEBOX_SAVE, pkPeer->GetHandle(), nullptr);
}

void CClientManager::QUERY_EMPIRE_SELECT(CPeer * pkPeer, uint32_t dwHandle, TEmpireSelectPacket * p)
{
	char szQuery[QUERY_MAX_LEN];

	snprintf(szQuery, sizeof(szQuery), "UPDATE player_index%s SET empire=%u WHERE id=%u", GetTablePostfix(), p->bEmpire, p->dwAccountID);
	std::unique_ptr<SQLMsg>(CDBManager::instance().DirectQuery(szQuery));

	metin2::log::info( "EmpireSelect: {}", szQuery);
	{
		snprintf(szQuery, sizeof(szQuery),
				"SELECT pid1, pid2, pid3, pid4 FROM player_index%s WHERE id=%u", GetTablePostfix(), p->dwAccountID);

		std::unique_ptr<SQLMsg> pmsg(CDBManager::instance().DirectQuery(szQuery));

		SQLResult * pRes = pmsg->Get();

		if (pRes->uiNumRows)
		{
			metin2::log::info( "EMPIRE {:d}", pRes->uiNumRows);

			MYSQL_ROW row = mysql_fetch_row(pRes->pSQLResult);
			uint32_t pids[3];

			uint32_t g_start_map[4] =
			{
				0,  // reserved
				1,  // Shinsoo
				21, // Chunjo
				41  // Jinno
			};

			// FIXME share with game
			uint32_t g_start_position[4][2]=
			{
				{      0,      0 },
				{ 469300, 964200 }, // Shinsoo
				{  55700, 157900 }, // Chunjo
				{ 969600, 278400 }  // Jinno
			};

			for (int i = 0; i < 3; ++i)
			{
				str_to_number(pids[i], row[i]);
				metin2::log::info( "EMPIRE PIDS[{:d}]", pids[i]);

				if (pids[i])
				{
					metin2::log::info( "EMPIRE move to pid[{:d}] to villiage of {:d}, map_index {:d}", 
							pids[i], p->bEmpire, g_start_map[p->bEmpire]);

					snprintf(szQuery, sizeof(szQuery), "UPDATE player%s SET map_index=%u,x=%u,y=%u WHERE id=%u", 
							GetTablePostfix(),
							g_start_map[p->bEmpire],
							g_start_position[p->bEmpire][0],
							g_start_position[p->bEmpire][1],
							pids[i]);

					std::unique_ptr<SQLMsg> pmsg2(CDBManager::instance().DirectQuery(szQuery));
				}
			}
		}
	}

	pkPeer->EncodeHeader(HEADER_DG_EMPIRE_SELECT, dwHandle, sizeof(uint8_t));
	pkPeer->EncodeBYTE(p->bEmpire);
}

void CClientManager::QUERY_SETUP(CPeer * peer, uint32_t dwHandle, const char * c_pData)
{
	TPacketGDSetup * p = (TPacketGDSetup *) c_pData;
	c_pData += sizeof(TPacketGDSetup);

	if (p->bAuthServer)
	{
		metin2::log::info( "AUTH_PEER ptr {}", static_cast<const void *>(peer));

		m_pkAuthPeer = peer;
		return;
	}

	peer->SetPublicIP(p->szPublicIP);
	peer->SetChannel(p->bChannel);
	peer->SetListenPort(p->wListenPort);
	peer->SetP2PPort(p->wP2PPort);
	peer->SetMaps(p->alMaps);

	//
	// Send which map is on which server
	//
	TMapLocation kMapLocations;

	strlcpy(kMapLocations.szHost, peer->GetPublicIP(), sizeof(kMapLocations.szHost));
	kMapLocations.wPort = peer->GetListenPort();
	std::memcpy(kMapLocations.alMaps, peer->GetMaps(), sizeof(kMapLocations.alMaps));

	uint8_t bMapCount;

	std::vector<TMapLocation> vec_kMapLocations;

	if (peer->GetChannel() == 1)
	{
		for (auto i = m_peerList.begin(); i != m_peerList.end(); ++i)
		{
			CPeer * tmp = *i;

			if (tmp == peer)
				continue;

			if (!tmp->GetChannel())
				continue;

			if (tmp->GetChannel() == GUILD_WARP_WAR_CHANNEL || tmp->GetChannel() == peer->GetChannel())
			{
				TMapLocation kMapLocation2;
				strlcpy(kMapLocation2.szHost, tmp->GetPublicIP(), sizeof(kMapLocation2.szHost));
				kMapLocation2.wPort = tmp->GetListenPort();
				std::memcpy(kMapLocation2.alMaps, tmp->GetMaps(), sizeof(kMapLocation2.alMaps));
				vec_kMapLocations.push_back(kMapLocation2);

				tmp->EncodeHeader(HEADER_DG_MAP_LOCATIONS, 0, sizeof(uint8_t) + sizeof(TMapLocation));
				bMapCount = 1;
				tmp->EncodeBYTE(bMapCount);
				tmp->Encode(&kMapLocations, sizeof(TMapLocation));
			}
		}
	}
	else if (peer->GetChannel() == GUILD_WARP_WAR_CHANNEL)
	{
		for (auto i = m_peerList.begin(); i != m_peerList.end(); ++i)
		{
			CPeer * tmp = *i;

			if (tmp == peer)
				continue;

			if (!tmp->GetChannel())
				continue;

			if (tmp->GetChannel() == 1 || tmp->GetChannel() == peer->GetChannel())
			{
				TMapLocation kMapLocation2;
				strlcpy(kMapLocation2.szHost, tmp->GetPublicIP(), sizeof(kMapLocation2.szHost));
				kMapLocation2.wPort = tmp->GetListenPort();
				std::memcpy(kMapLocation2.alMaps, tmp->GetMaps(), sizeof(kMapLocation2.alMaps));
				vec_kMapLocations.push_back(kMapLocation2);
			}

			tmp->EncodeHeader(HEADER_DG_MAP_LOCATIONS, 0, sizeof(uint8_t) + sizeof(TMapLocation));
			bMapCount = 1;
			tmp->EncodeBYTE(bMapCount);
			tmp->Encode(&kMapLocations, sizeof(TMapLocation));
		}
	}
	else
	{
		for (auto i = m_peerList.begin(); i != m_peerList.end(); ++i)
		{
			CPeer * tmp = *i;

			if (tmp == peer)
				continue;

			if (!tmp->GetChannel())
				continue;

			if (tmp->GetChannel() == GUILD_WARP_WAR_CHANNEL || tmp->GetChannel() == peer->GetChannel())
			{
				TMapLocation kMapLocation2;

				strlcpy(kMapLocation2.szHost, tmp->GetPublicIP(), sizeof(kMapLocation2.szHost));
				kMapLocation2.wPort = tmp->GetListenPort();
				std::memcpy(kMapLocation2.alMaps, tmp->GetMaps(), sizeof(kMapLocation2.alMaps));

				vec_kMapLocations.push_back(kMapLocation2);
			}

			if (tmp->GetChannel() == peer->GetChannel())
			{
				tmp->EncodeHeader(HEADER_DG_MAP_LOCATIONS, 0, sizeof(uint8_t) + sizeof(TMapLocation));
				bMapCount = 1;
				tmp->EncodeBYTE(bMapCount);
				tmp->Encode(&kMapLocations, sizeof(TMapLocation));
			}
		}
	}

	vec_kMapLocations.push_back(kMapLocations);

	peer->EncodeHeader(HEADER_DG_MAP_LOCATIONS, 0, sizeof(uint8_t) + sizeof(TMapLocation) * vec_kMapLocations.size());
	bMapCount = vec_kMapLocations.size();
	peer->EncodeBYTE(bMapCount);
	peer->Encode(&vec_kMapLocations[0], sizeof(TMapLocation) * vec_kMapLocations.size());

	//
	// Setup: make the other peers connect to the connected peer. (create P2P connections)
	// 
	metin2::log::info( "SETUP: channel {:d} listen {:d} p2p {:d} count {:d}", peer->GetChannel(), p->wListenPort, p->wP2PPort, bMapCount);

	TPacketDGP2P p2pSetupPacket;
	p2pSetupPacket.wPort = peer->GetP2PPort();
	p2pSetupPacket.bChannel = peer->GetChannel();
	strlcpy(p2pSetupPacket.szHost, peer->GetPublicIP(), sizeof(p2pSetupPacket.szHost));

	for (auto i = m_peerList.begin(); i != m_peerList.end();++i)
	{
		CPeer * tmp = *i;

		if (tmp == peer)
			continue;

		// If channel is 0, it can be considered a peer whose SETUP packet hasn't arrived yet, or auth
		if (0 == tmp->GetChannel())
			continue;

		tmp->EncodeHeader(HEADER_DG_P2P, 0, sizeof(TPacketDGP2P));
		tmp->Encode(&p2pSetupPacket, sizeof(TPacketDGP2P));
	}

	//
	// Restore the accounts that are logged in on this peer
	//
	TPacketLoginOnSetup * pck = (TPacketLoginOnSetup *) c_pData;

	for (uint32_t c = 0; c < p->dwLoginCount; ++c, ++pck)
	{
		CLoginData * pkLD = new CLoginData;

		pkLD->SetKey(pck->dwLoginKey);
		pkLD->SetClientKey(pck->adwClientKey);

		TAccountTable & r = pkLD->GetAccountRef();

		r.id = pck->dwID;
		trim_and_lower(pck->szLogin, r.login, sizeof(r.login));
		strlcpy(r.social_id, pck->szSocialID, sizeof(r.social_id));
		strlcpy(r.passwd, "TEMP", sizeof(r.passwd));

		InsertLoginData(pkLD);

		if (InsertLogonAccount(pck->szLogin, peer->GetHandle()))
		{
			metin2::log::info( "SETUP: login {:d} {} login_key {:d} host {}", pck->dwID, pck->szLogin, pck->dwLoginKey, pck->szHost);
			pkLD->SetPlay(true);
		}
		else
			metin2::log::info( "SETUP: login_fail {:d} {} login_key {:d}", pck->dwID, pck->szLogin, pck->dwLoginKey);
	}

	SendPartyOnSetup(peer);
	CGuildManager::instance().OnSetup(peer);
	CPrivManager::instance().SendPrivOnSetup(peer);
	SendEventFlagsOnSetup(peer);
	marriage::CManager::instance().OnSetup(peer);
}

void CClientManager::QUERY_ITEM_FLUSH(CPeer * pkPeer, const char * c_pData)
{
	uint32_t dwID = *(uint32_t *) c_pData;

	if (metin2::config::db().login_log)
		metin2::log::info( "HEADER_GD_ITEM_FLUSH: {:d}", dwID);

	CItemCache * c = GetItemCache(dwID);

	if (c)
		c->Flush();
}

void CClientManager::QUERY_ITEM_SAVE(CPeer * pkPeer, const char * c_pData)
{
	TPlayerItem * p = (TPlayerItem *) c_pData;

	// Safebox items are not cached, and any already in the cache must be removed.
	// Auction must not take this route. It must go through EnrollInAuction.

	if (p->window == SAFEBOX || p->window == MALL)
	{
		CItemCache * c = GetItemCache(p->id);

		if (c)
		{
			TItemCacheSetPtrMap::iterator it = m_map_pkItemCacheSetPtr.find(c->Get()->owner);

			if (it != m_map_pkItemCacheSetPtr.end())
			{
				if (metin2::config::test_server())
					metin2::log::info( "ITEM_CACHE: safebox owner {:d} id {:d}", c->Get()->owner, c->Get()->id);

				it->second->erase(c);
			}

			m_map_itemCache.erase(p->id);

			delete c;
		}
		char szQuery[512];

		snprintf(szQuery, sizeof(szQuery), 
			"REPLACE INTO item%s (id, owner_id, window, pos, count, vnum, socket0, socket1, socket2, "
			"attrtype0, attrvalue0, "
			"attrtype1, attrvalue1, "
			"attrtype2, attrvalue2, "
			"attrtype3, attrvalue3, "
			"attrtype4, attrvalue4, "
			"attrtype5, attrvalue5, "
			"attrtype6, attrvalue6) "
			"VALUES(%u, %u, %d, %d, %u, %u, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d)",
			GetTablePostfix(),
			p->id,
			p->owner,
			p->window,
			p->pos,
			p->count,
			p->vnum,
			p->alSockets[0],
			p->alSockets[1],
			p->alSockets[2],
			p->aAttr[0].bType, p->aAttr[0].sValue,
			p->aAttr[1].bType, p->aAttr[1].sValue,
			p->aAttr[2].bType, p->aAttr[2].sValue,
			p->aAttr[3].bType, p->aAttr[3].sValue,
			p->aAttr[4].bType, p->aAttr[4].sValue,
			p->aAttr[5].bType, p->aAttr[5].sValue,
			p->aAttr[6].bType, p->aAttr[6].sValue);

		CDBManager::instance().ReturnQuery(szQuery, QID_ITEM_SAVE, pkPeer->GetHandle(), nullptr);
	}
	else
	{
		if (metin2::config::test_server())
			metin2::log::info( "QUERY_ITEM_SAVE => PutItemCache() owner {:d} id {:d} vnum {:d} ", p->owner, p->id, p->vnum);

		PutItemCache(p);
	}
}

CClientManager::TItemCacheSet * CClientManager::GetItemCacheSet(uint32_t pid)
{
	TItemCacheSetPtrMap::iterator it = m_map_pkItemCacheSetPtr.find(pid);

	if (it == m_map_pkItemCacheSetPtr.end())
		return nullptr;

	return it->second.get();
}

void CClientManager::CreateItemCacheSet(uint32_t pid)
{
	if (m_map_pkItemCacheSetPtr.find(pid) != m_map_pkItemCacheSetPtr.end())
		return;

	m_map_pkItemCacheSetPtr.insert(TItemCacheSetPtrMap::value_type(pid, std::make_unique<TItemCacheSet>()));

	if (metin2::config::db().login_log)
		metin2::log::info( "ITEM_CACHE: new cache {:d}", pid);
}

void CClientManager::FlushItemCacheSet(uint32_t pid)
{
	TItemCacheSetPtrMap::iterator it = m_map_pkItemCacheSetPtr.find(pid);

	if (it == m_map_pkItemCacheSetPtr.end())
	{
		metin2::log::info( "FLUSH_ITEMCACHESET : No ItemCacheSet pid({:d})", pid);
		return;
	}

	TItemCacheSet * pSet = it->second.get();
	TItemCacheSet::iterator it_set = pSet->begin();

	while (it_set != pSet->end())
	{
		CItemCache * c = *it_set++;
		c->Flush();

		m_map_itemCache.erase(c->Get()->id);
		delete c;
	}

	pSet->clear();

	m_map_pkItemCacheSetPtr.erase(it);

	if (metin2::config::db().login_log)
		metin2::log::info( "FLUSH_ITEMCACHESET : Deleted pid({:d})", pid);
}

CItemCache * CClientManager::GetItemCache(uint32_t id)
{
	TItemCacheMap::iterator it = m_map_itemCache.find(id);

	if (it == m_map_itemCache.end())
		return nullptr;

	return it->second;
}

void CClientManager::PutItemCache(TPlayerItem * pNew, bool bSkipQuery)
{       
	CItemCache * c;     

	c = GetItemCache(pNew->id);

	// Create new item
	if (!c)
	{
		if (metin2::config::db().login_log)
			metin2::log::info( "ITEM_CACHE: PutItemCache ==> New CItemCache id{:d} vnum{:d} new owner{:d}", pNew->id, pNew->vnum, pNew->owner);

		c = new CItemCache;
		m_map_itemCache.insert(TItemCacheMap::value_type(pNew->id, c));
	}
	// If it exists
	else
	{
		if (metin2::config::db().login_log)
			metin2::log::info( "ITEM_CACHE: PutItemCache ==> Have Cache");
		// If the owner differs
		if (pNew->owner != c->Get()->owner)
		{
			// Remove the item from the user who already had it.
			TItemCacheSetPtrMap::iterator it = m_map_pkItemCacheSetPtr.find(c->Get()->owner);

			if (it != m_map_pkItemCacheSetPtr.end())
			{
				if (metin2::config::db().login_log)
				metin2::log::info( "ITEM_CACHE: delete owner {:d} id {:d} new owner {:d}", c->Get()->owner, c->Get()->id, pNew->owner);
				it->second->erase(c);
			}
		}
	}

	// Update with new info 
	c->Put(pNew, bSkipQuery);

	TItemCacheSetPtrMap::iterator it = m_map_pkItemCacheSetPtr.find(c->Get()->owner);

	if (it != m_map_pkItemCacheSetPtr.end())
	{
		if (metin2::config::db().login_log)
			metin2::log::info( "ITEM_CACHE: save {:d} id {:d}", c->Get()->owner, c->Get()->id);
		else
			metin2::log::debug( "ITEM_CACHE: save {:d} id {:d}", c->Get()->owner, c->Get()->id);
		it->second->insert(c);
	}
	else
	{
		// There is no current owner, so save it right away so it can be queried from SQL
		// on the next login.
		if (metin2::config::db().login_log)
			metin2::log::info( "ITEM_CACHE: direct save {:d} id {:d}", c->Get()->owner, c->Get()->id);
		else
			metin2::log::debug( "ITEM_CACHE: direct save {:d} id {:d}", c->Get()->owner, c->Get()->id);

		c->OnFlush();
	}
}

bool CClientManager::DeleteItemCache(uint32_t dwID)
{
	CItemCache * c = GetItemCache(dwID);

	if (!c)
		return false;

	c->Delete();
	return true;
}

// MYSHOP_PRICE_LIST
CItemPriceListTableCache* CClientManager::GetItemPriceListCache(uint32_t dwID)
{
	TItemPriceListCacheMap::iterator it = m_mapItemPriceListCache.find(dwID);

	if (it == m_mapItemPriceListCache.end())
		return nullptr;

	return it->second;
}

void CClientManager::PutItemPriceListCache(const TItemPriceListTable* pItemPriceList)
{
	CItemPriceListTableCache* pCache = GetItemPriceListCache(pItemPriceList->dwOwnerID);

	if (!pCache)
	{
		pCache = new CItemPriceListTableCache;
		m_mapItemPriceListCache.insert(TItemPriceListCacheMap::value_type(pItemPriceList->dwOwnerID, pCache));
	}

	pCache->Put(const_cast<TItemPriceListTable*>(pItemPriceList), true);
}

void CClientManager::UpdatePlayerCache()
{
	TPlayerTableCacheMap::iterator it = m_map_playerCache.begin();

	while (it != m_map_playerCache.end())
	{
		CPlayerTableCache * c = (it++)->second;

		if (c->CheckTimeout())
		{
			if (metin2::config::db().login_log)
				metin2::log::info( "UPDATE : UpdatePlayerCache() ==> FlushPlayerCache {:d} {} ", c->Get(false)->id, c->Get(false)->name);

			c->Flush();

			// Update the Item Cache too
			UpdateItemCacheSet(c->Get()->id);
		}
		else if (c->CheckFlushTimeout())
			c->Flush();
	}
}
// END_OF_MYSHOP_PRICE_LIST

void CClientManager::SetCacheFlushCountLimit(int iLimit)
{
	m_iCacheFlushCountLimit = std::max(10, iLimit);
	metin2::log::info( "CACHE_FLUSH_LIMIT_PER_SECOND: {:d}", m_iCacheFlushCountLimit);
}

void CClientManager::UpdateItemCache()
{
	if (m_iCacheFlushCount >= m_iCacheFlushCountLimit)
		return;

	TItemCacheMap::iterator it = m_map_itemCache.begin();

	while (it != m_map_itemCache.end())
	{
		CItemCache * c = (it++)->second;

		// Items are only flushed.
		if (c->CheckFlushTimeout())
		{
			if (metin2::config::test_server())
				metin2::log::info( "UpdateItemCache ==> Flush() vnum {:d} id {:d} owner {:d}", c->Get()->vnum, c->Get()->id, c->Get()->owner);

			c->Flush();

			if (++m_iCacheFlushCount >= m_iCacheFlushCountLimit)
				break;
		}
	}
}

void CClientManager::UpdateItemPriceListCache()
{
	TItemPriceListCacheMap::iterator it = m_mapItemPriceListCache.begin();

	while (it != m_mapItemPriceListCache.end())
	{
		CItemPriceListTableCache* pCache = it->second;

		if (pCache->CheckFlushTimeout())
		{
			pCache->Flush();
			m_mapItemPriceListCache.erase(it++);
		}
		else
			++it;
	}
}

void CClientManager::QUERY_ITEM_DESTROY(CPeer * pkPeer, const char * c_pData)
{
	uint32_t dwID = *(uint32_t *) c_pData;
	c_pData += sizeof(uint32_t);

	uint32_t dwPID = *(uint32_t *) c_pData;

	if (!DeleteItemCache(dwID))
	{
		char szQuery[64];
		snprintf(szQuery, sizeof(szQuery), "DELETE FROM item%s WHERE id=%u", GetTablePostfix(), dwID);

		if (metin2::config::db().login_log)
			metin2::log::info( "HEADER_GD_ITEM_DESTROY: PID {:d} ID {:d}", dwPID, dwID);

		if (dwPID == 0) // If nobody had it, async query
			CDBManager::instance().AsyncQuery(szQuery);
		else
			CDBManager::instance().ReturnQuery(szQuery, QID_ITEM_DESTROY, pkPeer->GetHandle(), nullptr);
	}
}

void CClientManager::QUERY_FLUSH_CACHE(CPeer * pkPeer, const char * c_pData)
{
	uint32_t dwPID = *(uint32_t *) c_pData;

	CPlayerTableCache * pkCache = GetPlayerCache(dwPID);

	if (!pkCache)
		return;

	metin2::log::info( "FLUSH_CACHE: {:d}", dwPID);

	pkCache->Flush();
	FlushItemCacheSet(dwPID);

	m_map_playerCache.erase(dwPID);
	delete pkCache;
}

void CClientManager::QUERY_SMS(CPeer * pkPeer, TPacketGDSMS * pack)
{
	const std::string query = sql::format(
			"INSERT INTO sms_pool (server, sender, receiver, mobile, msg) VALUES({}, {}, {}, {}, {})",
			(m_iPlayerIDStart + 2) / 3, pack->szFrom, pack->szTo, pack->szMobile, pack->szMsg);

	CDBManager::instance().AsyncQuery(query.c_str());
}

void CClientManager::QUERY_RELOAD_PROTO()
{
	if (!InitializeTables())
	{
		metin2::log::error("QUERY_RELOAD_PROTO: cannot load tables");
		return;
	}

	for (TPeerList::iterator i = m_peerList.begin(); i != m_peerList.end(); ++i)
	{
		CPeer * tmp = *i;

		if (!tmp->GetChannel())
			continue;

		tmp->EncodeHeader(HEADER_DG_RELOAD_PROTO, 0, 
				sizeof(uint16_t) + sizeof(TSkillTable) * m_vec_skillTable.size() +
				sizeof(uint16_t) + sizeof(TBanwordTable) * m_vec_banwordTable.size() +
				sizeof(uint16_t) + sizeof(TItemTable) * m_vec_itemTable.size() +
				sizeof(uint16_t) + sizeof(TMobTable) * m_vec_mobTable.size());

		tmp->EncodeWORD(m_vec_skillTable.size());
		tmp->Encode(&m_vec_skillTable[0], sizeof(TSkillTable) * m_vec_skillTable.size());

		tmp->EncodeWORD(m_vec_banwordTable.size());
		tmp->Encode(&m_vec_banwordTable[0], sizeof(TBanwordTable) * m_vec_banwordTable.size());

		tmp->EncodeWORD(m_vec_itemTable.size());
		tmp->Encode(&m_vec_itemTable[0], sizeof(TItemTable) * m_vec_itemTable.size());

		tmp->EncodeWORD(m_vec_mobTable.size());
		tmp->Encode(&m_vec_mobTable[0], sizeof(TMobTable) * m_vec_mobTable.size());
	}
}

// ADD_GUILD_PRIV_TIME
/**
 * @version	05/06/08 Bang2ni - added duration
 */
void CClientManager::AddGuildPriv(TPacketGiveGuildPriv* p)
{
	CPrivManager::instance().AddGuildPriv(p->guild_id, p->type, p->value, p->duration_sec);
}

void CClientManager::AddEmpirePriv(TPacketGiveEmpirePriv* p)
{
	CPrivManager::instance().AddEmpirePriv(p->empire, p->type, p->value, p->duration_sec);
}
// END_OF_ADD_GUILD_PRIV_TIME

void CClientManager::AddCharacterPriv(TPacketGiveCharacterPriv* p)
{
	CPrivManager::instance().AddCharPriv(p->pid, p->type, p->value);
}

void CClientManager::MoneyLog(TPacketMoneyLog* p)
{
	CMoneyLog::instance().AddLog(p->type, p->vnum, p->gold);
}

CLoginData * CClientManager::GetLoginData(uint32_t dwKey)
{
	TLoginDataByLoginKey::iterator it = m_map_pkLoginData.find(dwKey);

	if (it == m_map_pkLoginData.end())
		return nullptr;

	return it->second;
}

CLoginData * CClientManager::GetLoginDataByLogin(const char * c_pszLogin)
{
	char szLogin[LOGIN_MAX_LEN + 1];
	trim_and_lower(c_pszLogin, szLogin, sizeof(szLogin));

	TLoginDataByLogin::iterator it = m_map_pkLoginDataByLogin.find(szLogin);

	if (it == m_map_pkLoginDataByLogin.end())
		return nullptr;

	return it->second;
}

CLoginData * CClientManager::GetLoginDataByAID(uint32_t dwAID)
{
	TLoginDataByAID::iterator it = m_map_pkLoginDataByAID.find(dwAID);

	if (it == m_map_pkLoginDataByAID.end())
		return nullptr;

	return it->second;
}

void CClientManager::InsertLoginData(CLoginData * pkLD)
{
	char szLogin[LOGIN_MAX_LEN + 1];
	trim_and_lower(pkLD->GetAccountRef().login, szLogin, sizeof(szLogin));

	m_map_pkLoginData.insert(std::make_pair(pkLD->GetKey(), pkLD));
	m_map_pkLoginDataByLogin.insert(std::make_pair(szLogin, pkLD));
	m_map_pkLoginDataByAID.insert(std::make_pair(pkLD->GetAccountRef().id, pkLD));
}

void CClientManager::DeleteLoginData(CLoginData * pkLD)
{
	m_map_pkLoginData.erase(pkLD->GetKey());
	m_map_pkLoginDataByLogin.erase(pkLD->GetAccountRef().login);
	m_map_pkLoginDataByAID.erase(pkLD->GetAccountRef().id);

	if (m_map_kLogonAccount.find(pkLD->GetAccountRef().login) == m_map_kLogonAccount.end())
		delete pkLD;
	else
		pkLD->SetDeleted(true);
}

void CClientManager::QUERY_AUTH_LOGIN(CPeer * pkPeer, uint32_t dwHandle, TPacketGDAuthLogin * p)
{
	if (metin2::config::test_server())
		metin2::log::info( "QUERY_AUTH_LOGIN {:d} {:d} {}", p->dwID, p->dwLoginKey, p->szLogin);
	CLoginData * pkLD = GetLoginDataByLogin(p->szLogin);

	if (pkLD)
	{
		DeleteLoginData(pkLD);
	}

	uint8_t bResult;

	if (GetLoginData(p->dwLoginKey))
	{
		metin2::log::error("LoginData already exist key {:d} login {}", p->dwLoginKey, p->szLogin);
		bResult = 0;

		pkPeer->EncodeHeader(HEADER_DG_AUTH_LOGIN, dwHandle, sizeof(uint8_t));
		pkPeer->EncodeBYTE(bResult);
	}
	else
	{
		CLoginData * pkLD = new CLoginData;

		pkLD->SetKey(p->dwLoginKey);
		pkLD->SetClientKey(p->adwClientKey);
		pkLD->SetPremium(p->iPremiumTimes);

		TAccountTable & r = pkLD->GetAccountRef();

		r.id = p->dwID;
		trim_and_lower(p->szLogin, r.login, sizeof(r.login));
		strlcpy(r.social_id, p->szSocialID, sizeof(r.social_id));
		strlcpy(r.passwd, "TEMP", sizeof(r.passwd));

		metin2::log::info( "AUTH_LOGIN id({:d}) login({}) social_id({}) login_key({:d}), client_key({:d} {:d} {:d} {:d})",
				p->dwID, p->szLogin, p->szSocialID, p->dwLoginKey,
				p->adwClientKey[0], p->adwClientKey[1], p->adwClientKey[2], p->adwClientKey[3]);

		bResult = 1;

		InsertLoginData(pkLD);

		pkPeer->EncodeHeader(HEADER_DG_AUTH_LOGIN, dwHandle, sizeof(uint8_t));
		pkPeer->EncodeBYTE(bResult);
	}
}

void CClientManager::GuildDepositMoney(TPacketGDGuildMoney* p)
{
	CGuildManager::instance().DepositMoney(p->dwGuild, p->iGold);
}

void CClientManager::GuildWithdrawMoney(CPeer* peer, TPacketGDGuildMoney* p)
{
	CGuildManager::instance().WithdrawMoney(peer, p->dwGuild, p->iGold);
}

void CClientManager::GuildWithdrawMoneyGiveReply(TPacketGDGuildMoneyWithdrawGiveReply* p)
{
	CGuildManager::instance().WithdrawMoneyReply(p->dwGuild, p->bGiveSuccess, p->iChangeGold);
}

void CClientManager::GuildWarBet(TPacketGDGuildWarBet * p)
{
	CGuildManager::instance().Bet(p->dwWarID, p->szLogin, p->dwGold, p->dwGuild);
}

void CClientManager::CreateObject(TPacketGDCreateObject * p)
{
	using namespace building;

	char szQuery[512];

	snprintf(szQuery, sizeof(szQuery),
			"INSERT INTO object%s (land_id, vnum, map_index, x, y, x_rot, y_rot, z_rot) VALUES(%u, %u, %d, %d, %d, %f, %f, %f)",
			GetTablePostfix(), p->dwLandID, p->dwVnum, p->lMapIndex, p->x, p->y, p->xRot, p->yRot, p->zRot);

	std::unique_ptr<SQLMsg> pmsg(CDBManager::instance().DirectQuery(szQuery));

	if (pmsg->Get()->uiInsertID == 0)
	{
		metin2::log::error("cannot insert object");
		return;
	}

	auto pkObj = std::make_unique<TObject>();

	memset(pkObj.get(), 0, sizeof(TObject));

	pkObj->dwID = pmsg->Get()->uiInsertID;
	pkObj->dwVnum = p->dwVnum;
	pkObj->dwLandID = p->dwLandID;
	pkObj->lMapIndex = p->lMapIndex;
	pkObj->x = p->x;
	pkObj->y = p->y;
	pkObj->xRot = p->xRot;
	pkObj->yRot = p->yRot;
	pkObj->zRot = p->zRot;
	pkObj->lLife = 0;

	ForwardPacket(HEADER_DG_CREATE_OBJECT, pkObj.get(), sizeof(TObject));

	m_map_pkObjectTable.insert(std::make_pair(pkObj->dwID, std::move(pkObj)));
}

void CClientManager::DeleteObject(uint32_t dwID)
{
	char szQuery[128];

	snprintf(szQuery, sizeof(szQuery), "DELETE FROM object%s WHERE id=%u", GetTablePostfix(), dwID);

	std::unique_ptr<SQLMsg> pmsg(CDBManager::instance().DirectQuery(szQuery));

	if (pmsg->Get()->uiAffectedRows == 0 || pmsg->Get()->uiAffectedRows == (uint32_t)-1)
	{
		metin2::log::error("no object by id {:d}", dwID);
		return;
	}

	auto it = m_map_pkObjectTable.find(dwID);

	if (it != m_map_pkObjectTable.end())
	{
		m_map_pkObjectTable.erase(it);
	}

	ForwardPacket(HEADER_DG_DELETE_OBJECT, &dwID, sizeof(uint32_t));
}

void CClientManager::UpdateLand(uint32_t * pdw)
{
	uint32_t dwID = pdw[0];
	uint32_t dwGuild = pdw[1];

	building::TLand * p = &m_vec_kLandTable[0];

	uint32_t i;

	for (i = 0; i < m_vec_kLandTable.size(); ++i, ++p)
	{
		if (p->dwID == dwID)
		{
			char buf[256];
			snprintf(buf, sizeof(buf), "UPDATE land%s SET guild_id=%u WHERE id=%u", GetTablePostfix(), dwGuild, dwID);
			CDBManager::instance().AsyncQuery(buf);

			p->dwGuildID = dwGuild;
			break;
		}
	}

	if (i < m_vec_kLandTable.size())
		ForwardPacket(HEADER_DG_UPDATE_LAND, p, sizeof(building::TLand));
}

// BLOCK_CHAT
void CClientManager::BlockChat(TPacketBlockChat* p)
{
	const std::string query = sql::format("SELECT id FROM player{} WHERE name = {}", sql::raw(GetTablePostfix()), p->szName);
	std::unique_ptr<SQLMsg> pmsg(CDBManager::instance().DirectQuery(query.c_str()));
	SQLResult * pRes = pmsg->Get();

	if (pRes->uiNumRows)
	{
		MYSQL_ROW row = mysql_fetch_row(pRes->pSQLResult);
		uint32_t pid = strtoul(row[0], nullptr, 10);

		TPacketGDAddAffect pa;
		pa.dwPID = pid;
		pa.elem.dwType = 223;
		pa.elem.bApplyOn = 0;
		pa.elem.lApplyValue = 0;
		pa.elem.dwFlag = 0;
		pa.elem.lDuration = p->lDuration;
		pa.elem.lSPCost = 0;
		QUERY_ADD_AFFECT(nullptr, &pa);
	}
	else
	{
		// cannot find user with that name
	}
}
// END_OF_BLOCK_CHAT

void CClientManager::MarriageAdd(TPacketMarriageAdd * p)
{
	metin2::log::info( "MarriageAdd {:d} {:d} {} {}", p->dwPID1, p->dwPID2, p->szName1, p->szName2);
	marriage::CManager::instance().Add(p->dwPID1, p->dwPID2, p->szName1, p->szName2);
}

void CClientManager::MarriageUpdate(TPacketMarriageUpdate * p)
{
	metin2::log::info( "MarriageUpdate PID:{:d} {:d} LP:{:d} ST:{:d}", p->dwPID1, p->dwPID2, p->iLovePoint, p->byMarried);
	marriage::CManager::instance().Update(p->dwPID1, p->dwPID2, p->iLovePoint, p->byMarried);
}

void CClientManager::MarriageRemove(TPacketMarriageRemove * p)
{
	metin2::log::info( "MarriageRemove {:d} {:d}", p->dwPID1, p->dwPID2);
	marriage::CManager::instance().Remove(p->dwPID1, p->dwPID2);
}

void CClientManager::WeddingRequest(TPacketWeddingRequest * p)
{
	metin2::log::info( "WeddingRequest {:d} {:d}", p->dwPID1, p->dwPID2);
	ForwardPacket(HEADER_DG_WEDDING_REQUEST, p, sizeof(TPacketWeddingRequest));
}

void CClientManager::WeddingReady(TPacketWeddingReady * p)
{
	metin2::log::info( "WeddingReady {:d} {:d}", p->dwPID1, p->dwPID2);
	ForwardPacket(HEADER_DG_WEDDING_READY, p, sizeof(TPacketWeddingReady));
	marriage::CManager::instance().ReadyWedding(p->dwMapIndex, p->dwPID1, p->dwPID2);
}

void CClientManager::WeddingEnd(TPacketWeddingEnd * p)
{
	metin2::log::info( "WeddingEnd {:d} {:d}", p->dwPID1, p->dwPID2);
	marriage::CManager::instance().EndWedding(p->dwPID1, p->dwPID2);
}

//
// If the cache has the price info, update the cache; if not,
// first load the existing data, build the cache from it, then apply the newly received price info.
//

// Old code:

// Fixed code:
void CClientManager::MyshopPricelistUpdate(const TItemPriceListTable* pPacket)
{
	if (pPacket->byCount > SHOP_PRICELIST_MAX_NUM)
	{
		metin2::log::error("count overflow!");
		return;
	}

	CItemPriceListTableCache* pCache = GetItemPriceListCache(pPacket->dwOwnerID);

	if (pCache)
	{
		TItemPriceListTable table;

		table.dwOwnerID = pPacket->dwOwnerID;
		table.byCount = pPacket->byCount;

		std::memcpy(table.aPriceInfo, pPacket->aPriceInfo, sizeof(TItemPriceInfo) * pPacket->byCount);

		pCache->UpdateList(&table);
	}
	else
	{
		TItemPriceListTable* pUpdateTable = new TItemPriceListTable;

		pUpdateTable->dwOwnerID = pPacket->dwOwnerID;
		pUpdateTable->byCount = pPacket->byCount;

		std::memcpy(pUpdateTable->aPriceInfo, pPacket->aPriceInfo, sizeof(TItemPriceInfo) * pPacket->byCount);

		char szQuery[QUERY_MAX_LEN];
		snprintf(szQuery, sizeof(szQuery), "SELECT item_vnum, price FROM myshop_pricelist%s WHERE owner_id=%u", GetTablePostfix(), pPacket->dwOwnerID);
		CDBManager::instance().ReturnQuery(szQuery, QID_ITEMPRICE_LOAD_FOR_UPDATE, 0, pUpdateTable);
	}
}

// MYSHOP_PRICE_LIST
// If price info is cached, read the cache and send it immediately; otherwise query the DB.
//
void CClientManager::MyshopPricelistRequest(CPeer* peer, uint32_t dwHandle, uint32_t dwPlayerID)
{
	if (CItemPriceListTableCache* pCache = GetItemPriceListCache(dwPlayerID))
	{
		metin2::log::info( "Cache MyShopPricelist handle[{:d}] pid[{:d}]", dwHandle, dwPlayerID);

		TItemPriceListTable* pTable = pCache->Get(false);

		TPacketMyshopPricelistHeader header =
		{
			pTable->dwOwnerID,
			pTable->byCount
		};

		size_t sizePriceListSize = sizeof(TItemPriceInfo) * pTable->byCount;

		peer->EncodeHeader(HEADER_DG_MYSHOP_PRICELIST_RES, dwHandle, sizeof(header) + sizePriceListSize);
		peer->Encode(&header, sizeof(header));
		peer->Encode(pTable->aPriceInfo, sizePriceListSize);

	}
	else
	{
		metin2::log::info( "Query MyShopPricelist handle[{:d}] pid[{:d}]", dwHandle, dwPlayerID);

		char szQuery[QUERY_MAX_LEN];
		snprintf(szQuery, sizeof(szQuery), "SELECT item_vnum, price FROM myshop_pricelist%s WHERE owner_id=%u", GetTablePostfix(), dwPlayerID);
		CDBManager::instance().ReturnQuery(szQuery, QID_ITEMPRICE_LOAD, peer->GetHandle(), new TItemPricelistReqInfo(dwHandle, dwPlayerID));
	}
}
// END_OF_MYSHOP_PRICE_LIST

void CClientManager::ProcessPackets(CPeer * peer)
{
	uint8_t		header;
	uint32_t		dwHandle;
	uint32_t		dwLength;
	const char * data = nullptr;
	int			i = 0;
	int			iCount = 0;

	while (peer->PeekPacket(i, header, dwHandle, dwLength, &data))
	{
		// DISABLE_DB_HEADER_LOG
		// END_OF_DISABLE_DB_HEADER_LOG
		++iCount;

		if (metin2::config::test_server())
		{
			if (header != 10)
				metin2::log::info( " ProcessPacket Header [{:d}] Handle[{:d}] Length[{:d}] iCount[{:d}]", header, dwHandle, dwLength, iCount);
		}

		switch (header)
		{
			case HEADER_GD_BOOT:
				QUERY_BOOT(peer, (TPacketGDBoot *) data);
				break;

			case HEADER_GD_HAMMER_OF_TOR:
				break;

			case HEADER_GD_LOGIN_BY_KEY:
				QUERY_LOGIN_BY_KEY(peer, dwHandle, (TPacketGDLoginByKey *) data);
				break;

			case HEADER_GD_LOGOUT:
				QUERY_LOGOUT(peer, dwHandle, data);
				break;

			case HEADER_GD_PLAYER_LOAD:
				metin2::log::debug( "HEADER_GD_PLAYER_LOAD (handle: {:d} length: {:d})", dwHandle, dwLength);
				QUERY_PLAYER_LOAD(peer, dwHandle, (TPlayerLoadPacket *) data);
				break;

			case HEADER_GD_PLAYER_SAVE:
				metin2::log::debug( "HEADER_GD_PLAYER_SAVE (handle: {:d} length: {:d})", dwHandle, dwLength);
				QUERY_PLAYER_SAVE(peer, dwHandle, (TPlayerTable *) data);
				break;

			case HEADER_GD_PLAYER_CREATE:
				metin2::log::info( "HEADER_GD_PLAYER_CREATE (handle: {:d} length: {:d})", dwHandle, dwLength);
				__QUERY_PLAYER_CREATE(peer, dwHandle, (TPlayerCreatePacket *) data);
				metin2::log::info( "END");
				break;

			case HEADER_GD_PLAYER_DELETE:
				metin2::log::debug( "HEADER_GD_PLAYER_DELETE (handle: {:d} length: {:d})", dwHandle, dwLength);
				__QUERY_PLAYER_DELETE(peer, dwHandle, (TPlayerDeletePacket *) data);
				break;

			case HEADER_GD_PLAYER_COUNT:
				QUERY_PLAYER_COUNT(peer, (TPlayerCountPacket *) data);
				break;

			case HEADER_GD_QUEST_SAVE:
				metin2::log::debug( "HEADER_GD_QUEST_SAVE (handle: {:d} length: {:d})", dwHandle, dwLength);
				QUERY_QUEST_SAVE(peer, (TQuestTable *) data, dwLength);
				break;

			case HEADER_GD_SAFEBOX_LOAD:
				QUERY_SAFEBOX_LOAD(peer, dwHandle, (TSafeboxLoadPacket *) data, 0);
				break;

			case HEADER_GD_SAFEBOX_SAVE:
				metin2::log::debug( "HEADER_GD_SAFEBOX_SAVE (handle: {:d} length: {:d})", dwHandle, dwLength);
				QUERY_SAFEBOX_SAVE(peer, (TSafeboxTable *) data);
				break;

			case HEADER_GD_SAFEBOX_CHANGE_SIZE:
				QUERY_SAFEBOX_CHANGE_SIZE(peer, dwHandle, (TSafeboxChangeSizePacket *) data);
				break;

			case HEADER_GD_SAFEBOX_CHANGE_PASSWORD:
				QUERY_SAFEBOX_CHANGE_PASSWORD(peer, dwHandle, (TSafeboxChangePasswordPacket *) data);
				break;

			case HEADER_GD_MALL_LOAD:
				QUERY_SAFEBOX_LOAD(peer, dwHandle, (TSafeboxLoadPacket *) data, 1);
				break;

			case HEADER_GD_EMPIRE_SELECT:
				QUERY_EMPIRE_SELECT(peer, dwHandle, (TEmpireSelectPacket *) data);
				break;

			case HEADER_GD_SETUP:
				QUERY_SETUP(peer, dwHandle, data);
				break;

			case HEADER_GD_GUILD_CREATE:
				GuildCreate(peer, *(uint32_t *) data);
				break;

			case HEADER_GD_GUILD_SKILL_UPDATE:
				GuildSkillUpdate(peer, (TPacketGuildSkillUpdate *) data);		
				break;

			case HEADER_GD_GUILD_EXP_UPDATE:
				GuildExpUpdate(peer, (TPacketGuildExpUpdate *) data);
				break;

			case HEADER_GD_GUILD_ADD_MEMBER:
				GuildAddMember(peer, (TPacketGDGuildAddMember*) data);
				break;

			case HEADER_GD_GUILD_REMOVE_MEMBER:
				GuildRemoveMember(peer, (TPacketGuild*) data);
				break;

			case HEADER_GD_GUILD_CHANGE_GRADE:
				GuildChangeGrade(peer, (TPacketGuild*) data);
				break;

			case HEADER_GD_GUILD_CHANGE_MEMBER_DATA:
				GuildChangeMemberData(peer, (TPacketGuildChangeMemberData*) data);
				break;

			case HEADER_GD_GUILD_DISBAND:
				GuildDisband(peer, (TPacketGuild*) data);
				break;

			case HEADER_GD_GUILD_WAR:
				GuildWar(peer, (TPacketGuildWar*) data);
				break;

			case HEADER_GD_GUILD_WAR_SCORE:
				GuildWarScore(peer, (TPacketGuildWarScore*) data);
				break;

			case HEADER_GD_GUILD_CHANGE_LADDER_POINT:
				GuildChangeLadderPoint((TPacketGuildLadderPoint*) data);
				break;

			case HEADER_GD_GUILD_USE_SKILL:
				GuildUseSkill((TPacketGuildUseSkill*) data);
				break;

			case HEADER_GD_FLUSH_CACHE:
				QUERY_FLUSH_CACHE(peer, data);
				break;

			case HEADER_GD_ITEM_SAVE:
				QUERY_ITEM_SAVE(peer, data);
				break;

			case HEADER_GD_ITEM_DESTROY:
				QUERY_ITEM_DESTROY(peer, data);
				break;

			case HEADER_GD_ITEM_FLUSH:
				QUERY_ITEM_FLUSH(peer, data);
				break;

			case HEADER_GD_ADD_AFFECT:
				metin2::log::debug( "HEADER_GD_ADD_AFFECT");
				QUERY_ADD_AFFECT(peer, (TPacketGDAddAffect *) data);
				break;

			case HEADER_GD_REMOVE_AFFECT:
				metin2::log::debug( "HEADER_GD_REMOVE_AFFECT");
				QUERY_REMOVE_AFFECT(peer, (TPacketGDRemoveAffect *) data);
				break;

			case HEADER_GD_HIGHSCORE_REGISTER:
				QUERY_HIGHSCORE_REGISTER(peer, (TPacketGDHighscore *) data);
				break;

			case HEADER_GD_PARTY_CREATE:
				QUERY_PARTY_CREATE(peer, (TPacketPartyCreate*) data);
				break;

			case HEADER_GD_PARTY_DELETE:
				QUERY_PARTY_DELETE(peer, (TPacketPartyDelete*) data);
				break;

			case HEADER_GD_PARTY_ADD:
				QUERY_PARTY_ADD(peer, (TPacketPartyAdd*) data);
				break;

			case HEADER_GD_PARTY_REMOVE:
				QUERY_PARTY_REMOVE(peer, (TPacketPartyRemove*) data);
				break;

			case HEADER_GD_PARTY_STATE_CHANGE:
				QUERY_PARTY_STATE_CHANGE(peer, (TPacketPartyStateChange*) data);
				break;

			case HEADER_GD_PARTY_SET_MEMBER_LEVEL:
				QUERY_PARTY_SET_MEMBER_LEVEL(peer, (TPacketPartySetMemberLevel*) data);
				break;

			case HEADER_GD_RELOAD_PROTO:
				QUERY_RELOAD_PROTO();
				break;

			case HEADER_GD_CHANGE_NAME:
				QUERY_CHANGE_NAME(peer, dwHandle, (TPacketGDChangeName *) data);
				break;

			case HEADER_GD_SMS:
				QUERY_SMS(peer, (TPacketGDSMS *) data);
				break;

			case HEADER_GD_AUTH_LOGIN:
				QUERY_AUTH_LOGIN(peer, dwHandle, (TPacketGDAuthLogin *) data);
				break;

			case HEADER_GD_REQUEST_GUILD_PRIV:
				AddGuildPriv((TPacketGiveGuildPriv*)data);
				break;

			case HEADER_GD_REQUEST_EMPIRE_PRIV:
				AddEmpirePriv((TPacketGiveEmpirePriv*)data);
				break;

			case HEADER_GD_REQUEST_CHARACTER_PRIV:
				AddCharacterPriv((TPacketGiveCharacterPriv*) data);
				break;

			case HEADER_GD_MONEY_LOG:
				MoneyLog((TPacketMoneyLog*)data);
				break;

			case HEADER_GD_GUILD_DEPOSIT_MONEY:
				GuildDepositMoney((TPacketGDGuildMoney*)data);
				break;

			case HEADER_GD_GUILD_WITHDRAW_MONEY:
				GuildWithdrawMoney(peer, (TPacketGDGuildMoney*)data);
				break;

			case HEADER_GD_GUILD_WITHDRAW_MONEY_GIVE_REPLY:
				GuildWithdrawMoneyGiveReply((TPacketGDGuildMoneyWithdrawGiveReply*)data);
				break;

			case HEADER_GD_GUILD_WAR_BET:
				GuildWarBet((TPacketGDGuildWarBet *) data);
				break;

			case HEADER_GD_SET_EVENT_FLAG:
				SetEventFlag((TPacketSetEventFlag*) data);
				break;

			case HEADER_GD_CREATE_OBJECT:
				CreateObject((TPacketGDCreateObject *) data);
				break;

			case HEADER_GD_DELETE_OBJECT:
				DeleteObject(*(uint32_t *) data);
				break;

			case HEADER_GD_UPDATE_LAND:
				UpdateLand((uint32_t *) data);
				break;

			case HEADER_GD_MARRIAGE_ADD:
				MarriageAdd((TPacketMarriageAdd *) data);
				break;

			case HEADER_GD_MARRIAGE_UPDATE:
				MarriageUpdate((TPacketMarriageUpdate *) data);
				break;

			case HEADER_GD_MARRIAGE_REMOVE:
				MarriageRemove((TPacketMarriageRemove *) data);
				break;

			case HEADER_GD_WEDDING_REQUEST:
				WeddingRequest((TPacketWeddingRequest *) data);
				break;

			case HEADER_GD_WEDDING_READY:
				WeddingReady((TPacketWeddingReady *) data);
				break;

			case HEADER_GD_WEDDING_END:
				WeddingEnd((TPacketWeddingEnd *) data);
				break;

				// BLOCK_CHAT
			case HEADER_GD_BLOCK_CHAT:
				BlockChat((TPacketBlockChat *) data);
				break;
				// END_OF_BLOCK_CHAT

				// MYSHOP_PRICE_LIST
			case HEADER_GD_MYSHOP_PRICELIST_UPDATE:
				MyshopPricelistUpdate((TItemPriceListTable*)data);
				break;

			case HEADER_GD_MYSHOP_PRICELIST_REQ:
				MyshopPricelistRequest(peer, dwHandle, *(uint32_t*)data);
				break;
				// END_OF_MYSHOP_PRICE_LIST

				//RELOAD_ADMIN
			case HEADER_GD_RELOAD_ADMIN:
				ReloadAdmin(peer, (TPacketReloadAdmin*)data);
				break;
				//END_RELOAD_ADMIN

			case HEADER_GD_BREAK_MARRIAGE:
				BreakMarriage(peer, data);
				break;

			//MOANRCH
			case HEADER_GD_ELECT_MONARCH:
				Election(peer, dwHandle, data);
				break;

			case HEADER_GD_CANDIDACY:
				Candidacy(peer, dwHandle, data);
				break;

			case HEADER_GD_ADD_MONARCH_MONEY:
				AddMonarchMoney(peer, dwHandle, data);
				break;

			case HEADER_GD_DEC_MONARCH_MONEY:
				DecMonarchMoney(peer, dwHandle, data);
				break;

			case HEADER_GD_TAKE_MONARCH_MONEY:
				TakeMonarchMoney(peer, dwHandle, data);
				break;

			case HEADER_GD_COME_TO_VOTE:
				ComeToVote(peer, dwHandle, data);
				break;

			case HEADER_GD_RMCANDIDACY:		//< Remove candidate (GM)
				RMCandidacy(peer, dwHandle, data);
				break;

			case HEADER_GD_SETMONARCH:		///<Set monarch (GM)
				SetMonarch(peer, dwHandle, data);
				break;

			case HEADER_GD_RMMONARCH:		///<Delete monarch
				RMMonarch(peer, dwHandle, data);
				break;
			//END_MONARCH

			case HEADER_GD_CHANGE_MONARCH_LORD :
				ChangeMonarchLord(peer, dwHandle, (TPacketChangeMonarchLord*)data);
				break;

			case HEADER_GD_REQ_SPARE_ITEM_ID_RANGE :
				SendSpareItemIDRange(peer);
				break;

			case HEADER_GD_REQ_CHANGE_GUILD_MASTER :
				GuildChangeMaster((TPacketChangeGuildMaster*) data);
				break;

			case HEADER_GD_UPDATE_HORSE_NAME :
				UpdateHorseName((TPacketUpdateHorseName*) data, peer);
				break;

			case HEADER_GD_REQ_HORSE_NAME :
				AckHorseName(*(uint32_t*)data, peer);
				break;

			case HEADER_GD_DC:
				DeleteLoginKey((TPacketDC*) data);
				break;

			case HEADER_GD_VALID_LOGOUT:
				ResetLastPlayerID((TPacketNeedLoginLogInfo*)data);
				break;

			case HEADER_GD_REQUEST_CHARGE_CASH:
				ChargeCash((TRequestChargeCash*)data);
				break;

			//delete gift notify icon

			case HEADER_GD_DELETE_AWARDID:
				DeleteAwardId((TPacketDeleteAwardID*) data);
				break;

			case HEADER_GD_UPDATE_CHANNELSTATUS:
				UpdateChannelStatus((TChannelStatus*) data);
				break;
			case HEADER_GD_REQUEST_CHANNELSTATUS:
				RequestChannelStatus(peer, dwHandle);
				break;
			default:					
				metin2::log::error("Unknown header (header: {:d} handle: {:d} length: {:d})", header, dwHandle, dwLength);
				break;
		}
	}

	peer->RecvEnd(i);
}

void CClientManager::AddPeer(net::tcp::socket socket)
{
	// The db's input and output can be large (the boot packet carries every proto table).
	constexpr std::size_t MAX_PEER_BUFFER = 256 * 1024 * 1024;

	auto connection = net::connection::create(std::move(socket), MAX_PEER_BUFFER, MAX_PEER_BUFFER);

	// only the configured game servers (ALLOWED_PEER_HOSTS), or localhost when none are configured
	if (!is_allowed_peer(connection->remote_address().c_str(), metin2::config::db().allowed_peers, "127.0.0.1"))
	{
		metin2::log::info( "PORT_SECURITY: BLOCK CONNECTION FROM {}", connection->remote_address().c_str());
		connection->close();
		return;
	}

	m_peerList.push_front(new CPeer(std::move(connection)));
}

void CClientManager::RemovePeer(CPeer * pPeer)
{
	if (m_pkAuthPeer == pPeer)
	{
		m_pkAuthPeer = nullptr;
	}
	else
	{
		TLogonAccountMap::iterator it = m_map_kLogonAccount.begin();

		while (it != m_map_kLogonAccount.end())
		{
			CLoginData * pkLD = it->second;

			if (pkLD->GetConnectedPeerHandle() == pPeer->GetHandle())
			{
				pkLD->SetPlay(false);

				if (pkLD->IsDeleted())
				{
					metin2::log::info( "DELETING LoginData");
					delete pkLD;
				}

				m_map_kLogonAccount.erase(it++);
			}
			else
				++it;
		}
	}

	pPeer->ReturnItemIDRange();
	m_peerList.remove(pPeer);
	delete pPeer;
}

CPeer * CClientManager::GetPeer(uint32_t ident)
{
	for (auto i = m_peerList.begin(); i != m_peerList.end();++i)
	{
		CPeer * tmp = *i;

		if (tmp->GetHandle() == ident)
			return tmp;
	}

	return nullptr;
}

CPeer * CClientManager::GetAnyPeer()
{
	if (m_peerList.empty())
		return nullptr;

	return m_peerList.front();
}

// Process results received from the DB manager.
//
// @version	05/06/10 Bang2ni - added price info queries (QID_ITEMPRICE_XXX)
int CClientManager::AnalyzeQueryResult(SQLMsg * msg)
{
	CQueryInfo * qi = (CQueryInfo *) msg->pvUserData;
	CPeer * peer = GetPeer(qi->dwIdent);

	switch (qi->iType)
	{
		case QID_ITEM_AWARD_LOAD:
			ItemAwardManager::instance().Load(msg);
			delete qi;
			return true;

		case QID_GUILD_RANKING:
			CGuildManager::instance().ResultRanking(msg->Get()->pSQLResult);
			break;

			// MYSHOP_PRICE_LIST
		case QID_ITEMPRICE_LOAD_FOR_UPDATE:
			RESULT_PRICELIST_LOAD_FOR_UPDATE(msg);
			break;
			// END_OF_MYSHOP_PRICE_LIST
	}

	if (!peer)
	{	
		delete qi;
		return true;
	}

	switch (qi->iType)
	{
		case QID_PLAYER:
		case QID_ITEM:
		case QID_QUEST:
		case QID_AFFECT:
			RESULT_COMPOSITE_PLAYER(peer, msg, qi->iType);
			break;

		case QID_LOGIN:
			RESULT_LOGIN(peer, msg);
			break;

		case QID_SAFEBOX_LOAD:
			metin2::log::info( "QUERY_RESULT: HEADER_GD_SAFEBOX_LOAD");
			RESULT_SAFEBOX_LOAD(peer, msg);
			break;

		case QID_SAFEBOX_CHANGE_SIZE:
			metin2::log::info( "QUERY_RESULT: HEADER_GD_SAFEBOX_CHANGE_SIZE");
			RESULT_SAFEBOX_CHANGE_SIZE(peer, msg);
			break;

		case QID_SAFEBOX_CHANGE_PASSWORD:
			metin2::log::info( "QUERY_RESULT: HEADER_GD_SAFEBOX_CHANGE_PASSWORD {}", static_cast<const void *>(msg));
			RESULT_SAFEBOX_CHANGE_PASSWORD(peer, msg);
			break;

		case QID_SAFEBOX_CHANGE_PASSWORD_SECOND:
			metin2::log::info( "QUERY_RESULT: HEADER_GD_SAFEBOX_CHANGE_PASSWORD {}", static_cast<const void *>(msg));
			RESULT_SAFEBOX_CHANGE_PASSWORD_SECOND(peer, msg);
			break;

		case QID_HIGHSCORE_REGISTER:
			metin2::log::info( "QUERY_RESULT: HEADER_GD_HIGHSCORE_REGISTER {}", static_cast<const void *>(msg));
			RESULT_HIGHSCORE_REGISTER(peer, msg);
			break;

		case QID_SAFEBOX_SAVE:
		case QID_ITEM_SAVE:
		case QID_ITEM_DESTROY:
		case QID_QUEST_SAVE:
		case QID_PLAYER_SAVE:
		case QID_ITEM_AWARD_TAKEN:
			break;

			// PLAYER_INDEX_CREATE_BUG_FIX	
		case QID_PLAYER_INDEX_CREATE:
			RESULT_PLAYER_INDEX_CREATE(peer, msg);
			break;
			// END_PLAYER_INDEX_CREATE_BUG_FIX	

		case QID_PLAYER_DELETE:
			__RESULT_PLAYER_DELETE(peer, msg);
			break;

		case QID_LOGIN_BY_KEY:
			RESULT_LOGIN_BY_KEY(peer, msg);
			break;

			// MYSHOP_PRICE_LIST
		case QID_ITEMPRICE_LOAD:
			RESULT_PRICELIST_LOAD(peer, msg);
			break;
			// END_OF_MYSHOP_PRICE_LIST

		default:
			metin2::log::info( "CClientManager::AnalyzeQueryResult unknown query result type: {:d}, str: {}", qi->iType, msg->stQuery.c_str());
			break;
	}

	delete qi;
	return true;
}

void UsageLog()
{   
	FILE* fp = nullptr;

	time_t      ct;
	char        *time_s;
	struct tm   lt;

	int         avg = g_dwUsageAvg / 3600; // 60 seconds * 60 minutes

	fp = fopen("usage.txt", "a+");

	if (!fp)
		return;

	ct = time(nullptr);
	lt = *localtime(&ct);
	time_s = asctime(&lt);

	time_s[strlen(time_s) - 1] = '\0';

	fprintf(fp, "| %4d %-15.15s | %5d | %5u |", lt.tm_year + 1900, time_s + 4, avg, g_dwUsageMax);

	fprintf(fp, "\n");
	fclose(fp);

	g_dwUsageMax = g_dwUsageAvg = 0;
}

int CClientManager::Process()
{
	int pulses;

	if (!(pulses = thecore_idle()))
		return 0;

	while (pulses--)
	{
		++thecore_heart->pulse;

		if (!(thecore_heart->pulse % thecore_heart->passes_per_sec))
		{
			if (metin2::config::test_server())
			{

				if (!(thecore_heart->pulse % (thecore_heart->passes_per_sec * 10)))	

				{
					metin2::log::debug("[perf] [{:9d}] return {:d}/{:d}/{:d}/{:d} async {:d}/{:d}/{:d}/{:d}",
							thecore_heart->pulse,
							CDBManager::instance().CountReturnQuery(SQL_PLAYER),
							CDBManager::instance().CountReturnResult(SQL_PLAYER),
							CDBManager::instance().CountReturnQueryFinished(SQL_PLAYER),
							CDBManager::instance().CountReturnCopiedQuery(SQL_PLAYER),
							CDBManager::instance().CountAsyncQuery(SQL_PLAYER),
							CDBManager::instance().CountAsyncResult(SQL_PLAYER),
							CDBManager::instance().CountAsyncQueryFinished(SQL_PLAYER),
							CDBManager::instance().CountAsyncCopiedQuery(SQL_PLAYER));

					if ((thecore_heart->pulse % 50) == 0) 
						metin2::log::info( "[{:9d}] return {:d}/{:d}/{:d} async {:d}/{:d}/{:d}",
								thecore_heart->pulse,
								CDBManager::instance().CountReturnQuery(SQL_PLAYER),
								CDBManager::instance().CountReturnResult(SQL_PLAYER),
								CDBManager::instance().CountReturnQueryFinished(SQL_PLAYER),
								CDBManager::instance().CountAsyncQuery(SQL_PLAYER),
								CDBManager::instance().CountAsyncResult(SQL_PLAYER),
								CDBManager::instance().CountAsyncQueryFinished(SQL_PLAYER));
				}
			}
			else
			{
				metin2::log::debug("[perf] [{:9d}] return {:d}/{:d}/{:d}/{:d} async {:d}/{:d}/{:d}/{:d}",
						thecore_heart->pulse,
						CDBManager::instance().CountReturnQuery(SQL_PLAYER),
						CDBManager::instance().CountReturnResult(SQL_PLAYER),
						CDBManager::instance().CountReturnQueryFinished(SQL_PLAYER),
						CDBManager::instance().CountReturnCopiedQuery(SQL_PLAYER),
						CDBManager::instance().CountAsyncQuery(SQL_PLAYER),
						CDBManager::instance().CountAsyncResult(SQL_PLAYER),
						CDBManager::instance().CountAsyncQueryFinished(SQL_PLAYER),
						CDBManager::instance().CountAsyncCopiedQuery(SQL_PLAYER));

						if ((thecore_heart->pulse % 50) == 0) 
						metin2::log::info( "[{:9d}] return {:d}/{:d}/{:d} async {:d}/{:d}/{:d}",
							thecore_heart->pulse,
							CDBManager::instance().CountReturnQuery(SQL_PLAYER),
							CDBManager::instance().CountReturnResult(SQL_PLAYER),
							CDBManager::instance().CountReturnQueryFinished(SQL_PLAYER),
							CDBManager::instance().CountAsyncQuery(SQL_PLAYER),
							CDBManager::instance().CountAsyncResult(SQL_PLAYER),
							CDBManager::instance().CountAsyncQueryFinished(SQL_PLAYER));
						}

			CDBManager::instance().ResetCounter();

			uint32_t dwCount = CClientManager::instance().GetUserCount();

			g_dwUsageAvg += dwCount;
			g_dwUsageMax = std::max<int>(g_dwUsageMax, dwCount);

			thecore_idle_time = 0;

			if (!(thecore_heart->pulse % (thecore_heart->passes_per_sec * 3600)))
				UsageLog();

			m_iCacheFlushCount = 0;

			//Flush players
			UpdatePlayerCache();
			//Flush items
			UpdateItemCache();
			//Logout handling - flush cache set
			UpdateLogoutPlayer();

			// MYSHOP_PRICE_LIST
			UpdateItemPriceListCache();
			// END_OF_MYSHOP_PRICE_LIST

			CGuildManager::instance().Update();
			CPrivManager::instance().Update();
			marriage::CManager::instance().Update();
		}

		if (!(thecore_heart->pulse % (thecore_heart->passes_per_sec * 5)))
		{
			ItemAwardManager::instance().RequestLoad();
		}

		if (!(thecore_heart->pulse % (thecore_heart->passes_per_sec * 10)))
		{
			metin2::log::debug("[perf] QUERY: MAIN[{:d}] ASYNC[{:d}]", g_query_count[0], g_query_count[1]);
			g_query_count[0] = 0;
			g_query_count[1] = 0;
			/////////////////////////////////////////////////////////////////

			/////////////////////////////////////////////////////////////////
			metin2::log::debug("[perf] ITEM:{:d}", g_item_count);
			g_item_count = 0;
			/////////////////////////////////////////////////////////////////
		}

		if (!(thecore_heart->pulse % (thecore_heart->passes_per_sec * 60)))	// Once every 60 seconds
		{
			// Send the time for unique items.
			CClientManager::instance().SendTime();
		}

		if (!(thecore_heart->pulse % (thecore_heart->passes_per_sec * 3600)))	// Once an hour
		{
			CMoneyLog::instance().Save();
		}
	}

	net::io().poll();

	// Peers whose connection ended are removed here, outside their connection's handlers.
	for (auto it = m_peerList.begin(); it != m_peerList.end();)
	{
		CPeer * peer = *it++;

		if (peer->IsClosed())
			RemovePeer(peer);
	}

	return 1;
}

uint32_t CClientManager::GetUserCount()
{
	// Just counts logins.. --;
	return m_map_kLogonAccount.size();
}

void CClientManager::SendTime()
{
	int32_t now = GetCurrentTime();
	ForwardPacket(HEADER_DG_TIME, &now, sizeof(int32_t));
}

void CClientManager::ForwardPacket(uint8_t header, const void* data, int size, uint8_t bChannel, CPeer* except)
{
	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer * peer = *it;

		if (peer == except)
			continue;

		if (!peer->GetChannel())
			continue;

		if (bChannel && peer->GetChannel() != bChannel)
			continue;

		peer->EncodeHeader(header, 0, size);

		if (size > 0 && data)
			peer->Encode(data, size);
	}
}

time_t CClientManager::GetCurrentTime()
{
	return time(nullptr);
}

// ITEM_UNIQUE_ID
bool CClientManager::InitializeNowItemID()
{
	const uint32_t dwMin = metin2::config::db().item_id_min;
	const uint32_t dwMax = metin2::config::db().item_id_max;

	metin2::log::info( "ItemRange From File {:d} ~ {:d} ", dwMin, dwMax);

	if (CItemIDRangeManager::instance().BuildRange(dwMin, dwMax, m_itemRange) == false)
	{
		metin2::log::error("Can not build ITEM_ID_RANGE");
		return false;
	}

	metin2::log::info( " Init Success Start {:d} End {:d} Now {:d}", m_itemRange.dwMin, m_itemRange.dwMax, m_itemRange.dwUsableItemIDMin);

	return true;
}

uint32_t CClientManager::GainItemID()
{
	return m_itemRange.dwUsableItemIDMin++;
}

uint32_t CClientManager::GetItemID()
{
	return m_itemRange.dwUsableItemIDMin;
}
// ITEM_UNIQUE_ID_END
//BOOT_LOCALIZATION

bool CClientManager::InitializeLocalization() 
{
	char szQuery[512];	
	snprintf(szQuery, sizeof(szQuery), "SELECT mValue, mKey FROM locale");
	std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQuery(szQuery, SQL_COMMON));

	if (pMsg->Get()->uiNumRows == 0)
	{
		metin2::log::error("InitializeLocalization() ==> DirectQuery failed({})", szQuery);
		return false;
	}

	metin2::log::info( "InitializeLocalization() - LoadLocaleTable(count:{:d})", pMsg->Get()->uiNumRows);

	m_vec_Locale.clear();

	MYSQL_ROW row = nullptr;

	while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)) != nullptr)
	{
		int col = 0;
		tLocale locale;

		strlcpy(locale.szValue, row[col++], sizeof(locale.szValue));
		strlcpy(locale.szKey, row[col++], sizeof(locale.szKey));

		//DB_NAME_COLUMN Setting		
		if (strcmp(locale.szKey, "LOCALE") == 0)
		{
			// The server runs the english locale only.
			if (strcmp(locale.szValue, "english") != 0)
			{
				metin2::log::error("locale[LOCALE] = {} is not supported (only english)", locale.szValue);
				exit(0);
			}

			metin2::log::info( "locale[LOCALE] = {}", locale.szValue);
			g_stLocaleNameColumn = "locale_name";
		}
		else if (strcmp(locale.szKey, "DB_NAME_COLUMN") == 0)
		{
			metin2::log::info( "locale[DB_NAME_COLUMN] = {}", locale.szValue);
			g_stLocaleNameColumn = locale.szValue;	
		}
		else
		{
			metin2::log::info( "locale[UNKNOWN_KEY({})] = {}", locale.szKey, locale.szValue);
		}
		m_vec_Locale.push_back(locale);
	}	

	return true;
}
//END_BOOT_LOCALIZATION
//ADMIN_MANAGER

bool CClientManager::__GetAdminInfo(const char *szIP, std::vector<tAdminInfo> & rAdminVec)
{
	//If szIP == NULL, has GM privileges on all servers.
	const std::string query = sql::format(
			"SELECT mID,mAccount,mName,mContactIP,mServerIP,mAuthority FROM gmlist WHERE mServerIP='ALL' or mServerIP={}",
		   	szIP ? szIP : "ALL");

	std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQuery(query.c_str(), SQL_COMMON));

	if (pMsg->Get()->uiNumRows == 0)
	{
		metin2::log::error("__GetAdminInfo() ==> DirectQuery failed({})", query.c_str());
		return false;
	}

	MYSQL_ROW row;
	rAdminVec.reserve(pMsg->Get()->uiNumRows);

	while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
	{
		int idx = 0;
		tAdminInfo Info;

		str_to_number(Info.m_ID, row[idx++]);
		trim_and_lower(row[idx++], Info.m_szAccount, sizeof(Info.m_szAccount));
		strlcpy(Info.m_szName, row[idx++], sizeof(Info.m_szName));
		strlcpy(Info.m_szContactIP, row[idx++], sizeof(Info.m_szContactIP));
		strlcpy(Info.m_szServerIP, row[idx++], sizeof(Info.m_szServerIP));
		std::string stAuth = row[idx++];

		if (!stAuth.compare("IMPLEMENTOR"))
			Info.m_Authority = GM_IMPLEMENTOR;
		else if (!stAuth.compare("GOD"))
			Info.m_Authority = GM_GOD; 
		else if (!stAuth.compare("HIGH_WIZARD"))
			Info.m_Authority = GM_HIGH_WIZARD;
		else if (!stAuth.compare("LOW_WIZARD")) 
			Info.m_Authority = GM_LOW_WIZARD;
		else if (!stAuth.compare("WIZARD"))
			Info.m_Authority = GM_WIZARD;
		else 
			continue;

		rAdminVec.push_back(Info);

		metin2::log::info( "GM: PID {:d} Login {} Character {} ContactIP {} ServerIP {} Authority {:d}[{}]",
			   	Info.m_ID, Info.m_szAccount, Info.m_szName, Info.m_szContactIP, Info.m_szServerIP, Info.m_Authority, stAuth.c_str());
	}

	return true;
}

bool CClientManager::__GetHostInfo(std::vector<std::string> & rIPVec)
{
	char szQuery[512];
	snprintf(szQuery, sizeof(szQuery), "SELECT mIP FROM gmhost");
	std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQuery(szQuery, SQL_COMMON));

	if (pMsg->Get()->uiNumRows == 0)
	{
		metin2::log::error("__GetHostInfo() ==> DirectQuery failed({})", szQuery);
		return false;
	}

	rIPVec.reserve(pMsg->Get()->uiNumRows);

	MYSQL_ROW row; 

	while ((row = mysql_fetch_row(pMsg->Get()->pSQLResult)))
	{
		if (row[0] && *row[0])
		{
			rIPVec.push_back(row[0]);
			metin2::log::info( "GMHOST: {}", row[0]);
		}
	}

	return true;
}
//END_ADMIN_MANAGER

void CClientManager::ReloadAdmin(CPeer*, TPacketReloadAdmin* p)
{
	std::vector<tAdminInfo> vAdmin;
	std::vector<std::string> vHost;

	__GetHostInfo(vHost);
	__GetAdminInfo(p->szIP, vAdmin);

	uint32_t dwPacketSize = sizeof(uint16_t) + sizeof (uint16_t) + sizeof(tAdminInfo) * vAdmin.size() + 
		  sizeof(uint16_t) + sizeof(uint16_t) + 16 * vHost.size();	

	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer * peer = *it;

		if (!peer->GetChannel())
			continue;

		peer->EncodeHeader(HEADER_DG_RELOAD_ADMIN, 0, dwPacketSize);

		peer->EncodeWORD(16);
		peer->EncodeWORD(vHost.size());

		for (size_t n = 0; n < vHost.size(); ++n)
			peer->Encode(vHost[n].c_str(), 16);

		peer->EncodeWORD(sizeof(tAdminInfo));
		peer->EncodeWORD(vAdmin.size());

		for (size_t n = 0; n < vAdmin.size(); ++n)
			peer->Encode(&vAdmin[n], sizeof(tAdminInfo));
	}

	metin2::log::info( "ReloadAdmin End {}", p->szIP);
}

//BREAK_MARRIAGE
void CClientManager::BreakMarriage(CPeer * peer, const char * data)
{
	uint32_t pid1, pid2;

	pid1 = *(int *) data;
	data += sizeof(int);

	pid2 = *(int *) data;
	data += sizeof(int);

	metin2::log::info( "Breaking off a marriage engagement! pid {:d} and pid {:d}", pid1, pid2);
	marriage::CManager::instance().Remove(pid1, pid2);
}
//END_BREAK_MARIIAGE

void CClientManager::UpdateItemCacheSet(uint32_t pid)
{
	auto it = m_map_pkItemCacheSetPtr.find(pid);

	if (it == m_map_pkItemCacheSetPtr.end())
	{
		if (metin2::config::test_server())
			metin2::log::info( "UPDATE_ITEMCACHESET : UpdateItemCacheSet ==> No ItemCacheSet pid({:d})", pid);
		return;
	}

	TItemCacheSet * pSet = it->second.get();
	TItemCacheSet::iterator it_set = pSet->begin();

	while (it_set != pSet->end())
	{
		CItemCache * c = *it_set++;
		c->Flush();
	}

	if (metin2::config::db().login_log)
		metin2::log::info( "UPDATE_ITEMCACHESET : UpdateItemCachsSet pid({:d})", pid);
}

void CClientManager::Election(CPeer * peer, uint32_t dwHandle, const char* data)
{
	uint32_t idx;
	uint32_t selectingpid;

	idx = *(uint32_t *) data;
	data += sizeof(uint32_t);

	selectingpid = *(uint32_t *) data;
	data += sizeof(uint32_t);

	int Success = 0;

	if (!(Success = CMonarch::instance().VoteMonarch(selectingpid, idx)))
	{
		if (metin2::config::test_server())
		metin2::log::info( "[MONARCH_VOTE] Failed {:d} {:d}", idx, selectingpid);
		peer->EncodeHeader(HEADER_DG_ELECT_MONARCH, dwHandle, sizeof(int));
		peer->Encode(&Success, sizeof(int));
		return;
	}
	else
	{
		if (metin2::config::test_server())
		metin2::log::info( "[MONARCH_VOTE] Success {:d} {:d}", idx, selectingpid);
		peer->EncodeHeader(HEADER_DG_ELECT_MONARCH, dwHandle, sizeof(int));
		peer->Encode(&Success, sizeof(int));
		return;
	}

}
void CClientManager::Candidacy(CPeer *  peer, uint32_t dwHandle, const char* data)
{
	uint32_t pid;

	pid = *(uint32_t *) data;
	data += sizeof(uint32_t);

	if (!CMonarch::instance().AddCandidacy(pid, data))
	{
		if (metin2::config::test_server())
			metin2::log::info( "[MONARCH_CANDIDACY] Failed {:d} {}", pid, data);

		peer->EncodeHeader(HEADER_DG_CANDIDACY, dwHandle, sizeof(int) + 32);
		peer->Encode(nullptr, sizeof(int));
		peer->Encode(data, 32);
		return;
	}
	else
	{
		if (metin2::config::test_server())
			metin2::log::info( "[MONARCH_CANDIDACY] Success {:d} {}", pid, data);

		for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
		{
			CPeer * p = *it;

			if (!p->GetChannel())
				continue;

			if (p == peer)
			{	
				p->EncodeHeader(HEADER_DG_CANDIDACY, dwHandle, sizeof(int) + 32);
				p->Encode(&pid, sizeof(int));
				p->Encode(data, 32);
			}
			else
			{
				p->EncodeHeader(HEADER_DG_CANDIDACY, 0, sizeof(int) + 32);
				p->Encode(&pid, sizeof(int));
				p->Encode(data, 32);
			}
		}
	}
}

void CClientManager::AddMonarchMoney(CPeer * peer, uint32_t dwHandle, const char * data)
{
	int Empire = *(int *) data;
	data += sizeof(int);

	int Money = *(int *) data;
	data += sizeof(int);

	if (metin2::config::test_server())
		metin2::log::info( "[MONARCH] Add money Empire({:d}) Money({:d})", Empire, Money);

	CMonarch::instance().AddMoney(Empire, Money);

	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer * p = *it;

		if (!p->GetChannel())
			continue;

		if (p == peer)
		{	
			p->EncodeHeader(HEADER_DG_ADD_MONARCH_MONEY, dwHandle, sizeof(int) + sizeof(int));
			p->Encode(&Empire, sizeof(int));
			p->Encode(&Money, sizeof(int));
		}
		else
		{
			p->EncodeHeader(HEADER_DG_ADD_MONARCH_MONEY, 0, sizeof(int) + sizeof(int));
			p->Encode(&Empire, sizeof(int));
			p->Encode(&Money, sizeof(int));
		}

	}
}
void CClientManager::DecMonarchMoney(CPeer * peer, uint32_t dwHandle, const char * data)
{
	int Empire = *(int *) data;
	data += sizeof(int);

	int Money = *(int *) data;
	data += sizeof(int);

	if (metin2::config::test_server())
		metin2::log::info( "[MONARCH] Dec money Empire({:d}) Money({:d})", Empire, Money);

	CMonarch::instance().DecMoney(Empire, Money);

	for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
	{
		CPeer * p = *it;

		if (!p->GetChannel())
			continue;

		if (p == peer)
		{	
			p->EncodeHeader(HEADER_DG_DEC_MONARCH_MONEY, dwHandle, sizeof(int) + sizeof(int));
			p->Encode(&Empire, sizeof(int));
			p->Encode(&Money, sizeof(int));
		}
		else
		{
			p->EncodeHeader(HEADER_DG_DEC_MONARCH_MONEY, 0, sizeof(int) + sizeof(int));
			p->Encode(&Empire, sizeof(int));
			p->Encode(&Money, sizeof(int));
		}
	}
}

void CClientManager::TakeMonarchMoney(CPeer * peer, uint32_t dwHandle, const char * data)
{
	int Empire = *(int *) data;
	data += sizeof(int);

	uint32_t pid = *(uint32_t *) data;
	data += sizeof(int);

	int Money = *(int *) data;
	data += sizeof(int);

	if (metin2::config::test_server())
		metin2::log::info( "[MONARCH] Take money Empire({:d}) Money({:d})", Empire, Money);

	if (CMonarch::instance().TakeMoney(Empire, pid, Money) == true)
	{
		peer->EncodeHeader(HEADER_DG_TAKE_MONARCH_MONEY, dwHandle, sizeof(int) + sizeof(int));
		peer->Encode(&Empire, sizeof(int));
		peer->Encode(&Money, sizeof(int));
	}
	else
	{
		Money = 0;
		peer->EncodeHeader(HEADER_DG_TAKE_MONARCH_MONEY, dwHandle, sizeof(int) + sizeof(int));
		peer->Encode(&Empire, sizeof(int));
		peer->Encode(&Money, sizeof(int));
	}
}

void CClientManager::ComeToVote(CPeer * peer, uint32_t dwHandle, const char * data)
{
	CMonarch::instance().ElectMonarch();	
}

void CClientManager::RMCandidacy(CPeer * peer, uint32_t dwHandle, const char * data)
{
	char szName[32];

	strlcpy(szName, data, sizeof(szName));
	metin2::log::info( "[MONARCH_GM] Remove candidacy name({})", szName); 

	int iRet = CMonarch::instance().DelCandidacy(szName) ? 1 : 0;

	if (1 == iRet)
	{
		for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
		{
			CPeer * p = *it;

			if (!p->GetChannel())
				continue;

			if (p == peer)
			{
				p->EncodeHeader(HEADER_DG_RMCANDIDACY, dwHandle, sizeof(int) + sizeof(szName));
				p->Encode(&iRet, sizeof(int));
				p->Encode(szName, sizeof(szName));
			}
			else
			{
				p->EncodeHeader(HEADER_DG_RMCANDIDACY, dwHandle, sizeof(int) + sizeof(szName));
				p->Encode(&iRet, sizeof(int));
				p->Encode(szName, sizeof(szName));
			}
		}
	}
	else
	{
		CPeer * p = peer;
		p->EncodeHeader(HEADER_DG_RMCANDIDACY, dwHandle, sizeof(int) + sizeof(szName));
		p->Encode(&iRet, sizeof(int));
		p->Encode(szName, sizeof(szName));
	}
}

void CClientManager::SetMonarch(CPeer * peer, uint32_t dwHandle, const char * data)
{
	char szName[32];

	strlcpy(szName, data, sizeof(szName));

	if (metin2::config::test_server())
		metin2::log::info( "[MONARCH_GM] Set Monarch name({})", szName); 

	int iRet = CMonarch::instance().SetMonarch(szName) ? 1 : 0;

	if (1 == iRet)
	{
		for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
		{
			CPeer * p = *it;

			if (!p->GetChannel())
				continue;

			if (p == peer)
			{
				p->EncodeHeader(HEADER_DG_RMCANDIDACY, dwHandle, sizeof(int) + sizeof(szName));
				p->Encode(&iRet, sizeof(int));
				p->Encode(szName, sizeof(szName));
			}
			else
			{
				p->EncodeHeader(HEADER_DG_RMCANDIDACY, dwHandle, sizeof(int) + sizeof(szName));
				p->Encode(&iRet, sizeof(int));
				p->Encode(szName, sizeof(szName));
			}
		}
	}
	else
	{
		CPeer * p = peer;
		p->EncodeHeader(HEADER_DG_RMCANDIDACY, dwHandle, sizeof(int) + sizeof(szName));
		p->Encode(&iRet, sizeof(int));
		p->Encode(szName, sizeof(szName));
	}
}

void CClientManager::RMMonarch(CPeer * peer, uint32_t dwHandle, const char * data)
{
	char szName[32];

	strlcpy(szName, data, sizeof(szName));

	if (metin2::config::test_server())
		metin2::log::info( "[MONARCH_GM] Remove Monarch name({})", szName); 

	int iRet = CMonarch::instance().DelMonarch(szName) ? 1 : 0;

	if (1 == iRet)
	{
		for (auto it = m_peerList.begin(); it != m_peerList.end(); ++it)
		{
			CPeer * p = *it;

			if (!p->GetChannel())
				continue;

			if (p == peer)
			{
				p->EncodeHeader(HEADER_DG_RMMONARCH, dwHandle, sizeof(int) + sizeof(szName));
				p->Encode(&iRet, sizeof(int));
				p->Encode(szName, sizeof(szName));
			}
			else
			{
				p->EncodeHeader(HEADER_DG_RMMONARCH, dwHandle, sizeof(int) + sizeof(szName));
				p->Encode(&iRet, sizeof(int));
				p->Encode(szName, sizeof(szName));
			}
		}
	}
	else
	{
		CPeer * p = peer;
		p->EncodeHeader(HEADER_DG_RMCANDIDACY, dwHandle, sizeof(int) + sizeof(szName));
		p->Encode(&iRet, sizeof(int));
		p->Encode(szName, sizeof(szName));
	}
}

void CClientManager::ChangeMonarchLord(CPeer * peer, uint32_t dwHandle, TPacketChangeMonarchLord* info)
{
	char szQuery[1024];
	snprintf(szQuery, sizeof(szQuery), 
			"SELECT a.name, NOW() FROM player%s AS a, player_index%s AS b WHERE (a.account_id=b.id AND a.id=%u AND b.empire=%u) AND "
		    "(b.pid1=%u OR b.pid2=%u OR b.pid3=%u OR b.pid4=%u)", 
			GetTablePostfix(), GetTablePostfix(), info->dwPID, info->bEmpire,
		   	info->dwPID, info->dwPID, info->dwPID, info->dwPID);

	std::unique_ptr<SQLMsg> pMsg(CDBManager::instance().DirectQuery(szQuery, SQL_PLAYER));

	if (pMsg->Get()->uiNumRows != 0)
	{
		TPacketChangeMonarchLordACK ack;
		ack.bEmpire = info->bEmpire;
		ack.dwPID = info->dwPID;

		MYSQL_ROW row = mysql_fetch_row(pMsg->Get()->pSQLResult);
		strlcpy(ack.szName, row[0], sizeof(ack.szName));
		strlcpy(ack.szDate, row[1], sizeof(ack.szDate));

		snprintf(szQuery, sizeof(szQuery), "UPDATE monarch SET pid=%u, windate=NOW() WHERE empire=%d", ack.dwPID, ack.bEmpire);
		std::unique_ptr<SQLMsg> pMsg2(CDBManager::instance().DirectQuery(szQuery, SQL_PLAYER));

		if (pMsg2->Get()->uiAffectedRows > 0)
		{
			CMonarch::instance().LoadMonarch();

			MonarchInfo* newInfo = CMonarch::instance().GetMonarch();

			for (auto it = m_peerList.begin(); it != m_peerList.end(); it++)
			{
				CPeer* client = *it;

				client->EncodeHeader(HEADER_DG_CHANGE_MONARCH_LORD_ACK, 0, sizeof(TPacketChangeMonarchLordACK));
				client->Encode(&ack, sizeof(TPacketChangeMonarchLordACK));

				client->EncodeHeader(HEADER_DG_UPDATE_MONARCH_INFO, 0, sizeof(MonarchInfo));
				client->Encode(newInfo, sizeof(MonarchInfo));
			}
		}
	}
}

void CClientManager::SendSpareItemIDRange(CPeer* peer)
{
	peer->SendSpareItemIDRange();
}

//
// Remove only the Login Key from the map.
// 
void CClientManager::DeleteLoginKey(TPacketDC *data)
{
	char login[LOGIN_MAX_LEN+1] = {0};
	trim_and_lower(data->login, login, sizeof(login));

	CLoginData *pkLD = GetLoginDataByLogin(login);

	if (pkLD)
	{
		TLoginDataByLoginKey::iterator it = m_map_pkLoginData.find(pkLD->GetKey());

		if (it != m_map_pkLoginData.end())
			m_map_pkLoginData.erase(it);
	}
}

// delete gift notify icon
void CClientManager::DeleteAwardId(TPacketDeleteAwardID *data)
{
	std::map<uint32_t, TItemAward *>::iterator it;
	it = ItemAwardManager::instance().GetMapAward().find(data->dwID);
	if ( it != ItemAwardManager::instance().GetMapAward().end() )
	{
		std::set<TItemAward *> & kSet = ItemAwardManager::instance().GetMapkSetAwardByLogin()[it->second->szLogin];
		if(kSet.erase(it->second))
			metin2::log::info("erase ItemAward id: {:d} from cache", data->dwID);
		ItemAwardManager::instance().GetMapAward().erase(data->dwID);
	}
	else
	{
		metin2::log::info("DELETE_AWARDID : could not find the id: {:d}", data->dwID);
	}

}

void CClientManager::UpdateChannelStatus(TChannelStatus* pData)
{
	TChannelStatusMap::iterator it = m_mChannelStatus.find(pData->nPort);
	if (it != m_mChannelStatus.end()) {
		it->second = pData->bStatus;
	}
	else {
		m_mChannelStatus.insert(TChannelStatusMap::value_type(pData->nPort, pData->bStatus));
	}
}

void CClientManager::RequestChannelStatus(CPeer* peer, uint32_t dwHandle)
{
	const int nSize = m_mChannelStatus.size();
	peer->EncodeHeader(HEADER_DG_RESPOND_CHANNELSTATUS, dwHandle, sizeof(TChannelStatus)*nSize+sizeof(int));
	peer->Encode(&nSize, sizeof(int));
	for (TChannelStatusMap::iterator it = m_mChannelStatus.begin(); it != m_mChannelStatus.end(); it++) {
		peer->Encode(&it->first, sizeof(short));
		peer->Encode(&it->second, sizeof(uint8_t));
	}
}

void CClientManager::ResetLastPlayerID(const TPacketNeedLoginLogInfo* data)
{
	CLoginData* pkLD = GetLoginDataByAID( data->dwPlayerID );

	if (nullptr != pkLD)
	{
		pkLD->SetLastPlayerID( 0 );
	}
}

void CClientManager::ChargeCash(const TRequestChargeCash* packet)
{
	char szQuery[512];

	if (ERequestCharge_Cash == packet->eChargeType)
		snprintf(szQuery, sizeof(szQuery), "update account set `cash` = `cash` + %d where id = %d limit 1", packet->dwAmount, packet->dwAID);
	else if(ERequestCharge_Mileage == packet->eChargeType)
		snprintf(szQuery, sizeof(szQuery), "update account set `mileage` = `mileage` + %d where id = %d limit 1", packet->dwAmount, packet->dwAID);
	else
	{
		metin2::log::error("Invalid request charge type (type : {:d}, amount : {:d}, aid : {:d})", packet->eChargeType, packet->dwAmount, packet->dwAID);
		return;
	}

	metin2::log::error("Request Charge (type : {:d}, amount : {:d}, aid : {:d})", packet->eChargeType, packet->dwAmount, packet->dwAID);

	CDBManager::instance().AsyncQuery(szQuery, SQL_ACCOUNT);
}
}
