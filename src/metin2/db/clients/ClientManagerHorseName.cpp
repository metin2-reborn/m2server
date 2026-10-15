// vim:ts=4 sw=4
#include <metin2/db/clients/ClientManager.h>
#include <metin2/db/storage/DBManager.h>
#include <metin2/db/clients/Peer.h>

#include <metin2/proto/tables.h>
#include <metin2/sql/AsyncSQL.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

namespace metin2::db::inline clients {

void CClientManager::UpdateHorseName(TPacketUpdateHorseName* data, CPeer* peer)
{
	const std::string query = sql::format("REPLACE INTO horse_name VALUES({}, {})", data->dwPlayerID, data->szHorseName);

	std::unique_ptr<SQLMsg> pmsg_insert(CDBManager::instance().DirectQuery(query.c_str()));

	ForwardPacket(HEADER_DG_UPDATE_HORSE_NAME, data, sizeof(TPacketUpdateHorseName), 0, peer);
}

void CClientManager::AckHorseName(uint32_t dwPID, CPeer* peer)
{
	char szQuery[512];

	snprintf(szQuery, sizeof(szQuery), "SELECT name FROM horse_name WHERE id = %u", dwPID);

	std::unique_ptr<SQLMsg> pmsg(CDBManager::instance().DirectQuery(szQuery));

	TPacketUpdateHorseName packet;
	packet.dwPlayerID = dwPID;

	if (pmsg->Get()->uiNumRows == 0)
	{
		memset(packet.szHorseName, 0, sizeof (packet.szHorseName));
	}
	else
	{
		MYSQL_ROW row = mysql_fetch_row(pmsg->Get()->pSQLResult);
		strlcpy(packet.szHorseName, row[0], sizeof(packet.szHorseName));
	}

	peer->EncodeHeader(HEADER_DG_ACK_HORSE_NAME, 0, sizeof(TPacketUpdateHorseName));
	peer->Encode(&packet, sizeof(TPacketUpdateHorseName));
}
}
