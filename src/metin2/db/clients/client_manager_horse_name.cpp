#include <metin2/db/clients/client_manager.h>
#include <metin2/db/clients/peer.h>
#include <metin2/db/storage/db_manager.h>

#include <metin2/proto/tables.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

namespace metin2::db::inline clients {

    void client_manager::update_horse_name(packet_update_horse_name *data, peer *peer) {
        const std::string query = sql::format("REPLACE INTO horse_name VALUES({}, {})", data->player_id, data->horse_name);

        auto pmsg_insert = db_manager::instance().direct_query(query);

        forward_packet(HEADER_DG_UPDATE_HORSE_NAME, data, sizeof(packet_update_horse_name), 0, peer);
    }

    void client_manager::ack_horse_name(uint32_t pid, peer *peer) {
        const std::string query = sql::format("SELECT name FROM horse_name WHERE id = {}", pid);

        auto pmsg = db_manager::instance().direct_query(query);

        packet_update_horse_name packet;
        packet.player_id = pid;

        if (pmsg->get()->num_rows == 0) {
            memset(packet.horse_name, 0, sizeof(packet.horse_name));
        } else {
            MYSQL_ROW row = mysql_fetch_row(pmsg->get()->res);
            strlcpy(packet.horse_name, row[0], sizeof(packet.horse_name));
        }

        peer->encode_header(HEADER_DG_ACK_HORSE_NAME, 0, sizeof(packet_update_horse_name));
        peer->encode(&packet, sizeof(packet_update_horse_name));
    }
}
