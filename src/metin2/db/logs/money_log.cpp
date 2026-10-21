#include <metin2/db/logs/money_log.h>

#include <metin2/db/clients/client_manager.h>
#include <metin2/db/clients/peer.h>

#include <metin2/log/log.h>
#include <metin2/proto/log_types.h>
#include <metin2/proto/tables.h>

#include <cstdint>

namespace metin2::db::inline logs {

    money_log::money_log() = default;

    void money_log::save() {
        peer *peer = get_client_manager().any_peer();
        if (!peer)
            return;
        for (uint8_t type = 0; type < MONEY_LOG_TYPE_MAX_NUM; type++) {
            __typeof(money_log_container_[type].begin()) it;
            for (it = money_log_container_[type].begin(); it != money_log_container_[type].end(); ++it) {
                packet_money_log p;
                p.type = type;
                p.vnum = it->first;
                p.gold = it->second;
                peer->encode_header(HEADER_DG_MONEY_LOG, 0, sizeof(p));
                peer->encode(&p, sizeof(p));
            }
            money_log_container_[type].clear();
        }
    }

    void money_log::add_log(uint8_t type, uint32_t vnum, int gold) {
        if (type >= MONEY_LOG_TYPE_MAX_NUM) {
            metin2::log::warn("Money log: invalid type {:d}", type);
            return;
        }
        money_log_container_[type][vnum] += gold;
    }
}
