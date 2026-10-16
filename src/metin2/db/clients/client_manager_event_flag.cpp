#include <metin2/db/clients/client_manager.h>
#include <metin2/db/clients/peer.h>
#include <metin2/db/storage/db_manager.h>

#include <metin2/common/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/tables.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <memory>
#include <string>
#include <utility>

namespace metin2::db::inline clients {
    void client_manager::load_event_flag() {
        const std::string query = sql::format("SELECT szName, lValue FROM quest WHERE dwPID = 0");
        auto pmsg = get_db_manager().direct_query(query);

        sql_result *res = pmsg->get();
        if (res->num_rows) {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res->res.get()))) {
                packet_set_event_flag p;
                strlcpy(p.flag_name, row[0], sizeof(p.flag_name));
                str_to_number(p.value, row[1]);
                metin2::log::info("Event flag: loaded {} = {:d}", p.flag_name, p.value);
                map_event_flag_.insert(std::make_pair(std::string(p.flag_name), p.value));
                forward_packet(HEADER_DG_SET_EVENT_FLAG, &p, sizeof(packet_set_event_flag));
            }
        }
    }

    void client_manager::set_event_flag(packet_set_event_flag *p) {
        forward_packet(HEADER_DG_SET_EVENT_FLAG, p, sizeof(packet_set_event_flag));

        bool changed = false;

        auto it = map_event_flag_.find(p->flag_name);
        if (it == map_event_flag_.end()) {
            changed = true;
            map_event_flag_.insert(std::make_pair(std::string(p->flag_name), p->value));
        } else if (it->second != p->value) {
            changed = true;
            it->second = p->value;
        }

        if (changed) {
            const std::string query = sql::format(
                "REPLACE INTO quest (dwPID, szName, szState, lValue) VALUES(0, {}, '', {})", p->flag_name, p->value);

            get_db_manager().async_query(query);
            metin2::log::info("Event flag: set {} = {:d}", p->flag_name, p->value);
            return;
        }

        metin2::log::debug("Event flag: {} already {:d}, unchanged", p->flag_name, p->value);
    }

    void client_manager::send_event_flags_on_setup(peer *peer) {
        for (auto it = map_event_flag_.begin(); it != map_event_flag_.end(); ++it) {
            packet_set_event_flag p;

            strlcpy(p.flag_name, it->first.c_str(), sizeof(p.flag_name));
            p.value = it->second;

            peer->encode_header(HEADER_DG_SET_EVENT_FLAG, 0, sizeof(packet_set_event_flag));
            peer->encode(&p, sizeof(packet_set_event_flag));
        }
    }
}
