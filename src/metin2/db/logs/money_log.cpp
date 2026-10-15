#include <metin2/db/logs/money_log.h>

#include <metin2/db/clients/client_manager.h>
#include <metin2/db/clients/peer.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>

#include <cstdint>

namespace metin2::db::inline logs {

money_log::money_log()
{
}

money_log::~money_log()
{
}

void money_log::save()
{
	peer* peer = client_manager::instance().any_peer();
	if (!peer)
		return;
	for (uint8_t bType = 0; bType < MONEY_LOG_TYPE_MAX_NUM; bType ++)
	{
		__typeof(money_log_container_[bType].begin()) it;
		for (it = money_log_container_[bType].begin(); it != money_log_container_[bType].end(); ++it)
		{
			//bType;
			packet_money_log p;
			p.type = bType;
			p.vnum = it->first;
			p.gold = it->second;
			peer->encode_header(HEADER_DG_MONEY_LOG, 0, sizeof(p));
			peer->encode(&p, sizeof(p));
		}
		money_log_container_[bType].clear();
	}
}

void money_log::add_log(uint8_t type, uint32_t vnum, int gold)
{
	money_log_container_[type][vnum] += gold;
}
}
