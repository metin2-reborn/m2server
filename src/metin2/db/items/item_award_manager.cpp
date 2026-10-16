#include <metin2/db/items/item_award_manager.h>

#include <metin2/db/clients/client_manager.h>
#include <metin2/db/storage/db_manager.h>

#include <metin2/common/utils.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/length.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <utility>

#include <sys/types.h>

namespace metin2::db::inline items {

    uint32_t last_cached_item_award_id = 0;
    item_award_manager::item_award_manager() {
    }

    void item_award_manager::request_load() {
        const std::string query = sql::format("SELECT id,login,vnum,count,socket0,socket1,socket2,mall,why FROM item_award WHERE taken_time IS NULL and id > {}", last_cached_item_award_id);
        db_manager::instance().return_query(query, [this](sql_msg &msg) { load(msg); });
    }

    void item_award_manager::load(sql_msg &msg) {
        MYSQL_RES *pRes = msg.get()->res.get();

        for (uint i = 0; i < msg.get()->num_rows; ++i) {
            MYSQL_ROW row = mysql_fetch_row(pRes);
            int col = 0;

            uint32_t dwID = 0;
            str_to_number(dwID, row[col++]);

            if (map_award_.find(dwID) != map_award_.end())
                continue;

            auto owned = std::make_unique<item_award>();
            item_award *kData = owned.get();

            kData->id = dwID;
            trim_and_lower(row[col++], kData->login);
            str_to_number(kData->vnum, row[col++]);
            str_to_number(kData->count, row[col++]);
            str_to_number(kData->socket0, row[col++]);
            str_to_number(kData->socket1, row[col++]);
            str_to_number(kData->socket2, row[col++]);
            str_to_number(kData->mall, row[col++]);

            if (row[col]) {
                strlcpy(kData->why, row[col], sizeof(kData->why));
                client_manager::instance().inform_gift_award(kData->login, kData->why, kData->vnum);
            }

            map_award_.emplace(dwID, std::move(owned));

            metin2::log::debug("Item award: loaded award {:d} for account {}, vnum {:d}, count {:d}, socket {:d}", kData->id, kData->login, kData->vnum, kData->count, kData->socket0);
            std::set<item_award *> &kSet = map_set_award_by_login_[kData->login];
            kSet.insert(kData);

            if (dwID > last_cached_item_award_id)
                last_cached_item_award_id = dwID;
        }
    }

    auto item_award_manager::by_login(const char *login) -> std::set<item_award *> * {
        auto it = map_set_award_by_login_.find(login);

        if (it == map_set_award_by_login_.end())
            return nullptr;

        return &it->second;
    }

    void item_award_manager::taken(uint32_t award_id, uint32_t item_id) {
        auto it = map_award_.find(award_id);

        if (it == map_award_.end()) {
            metin2::log::warn("Item award: award {:d} taken but not found", award_id);
            return;
        }

        item_award *k = it->second.get();
        k->taken = true;

        const std::string query = sql::format(
            "UPDATE item_award SET taken_time=NOW(),item_id={} WHERE id={} AND taken_time IS NULL",
            item_id, award_id);

        db_manager::instance().return_query(query, [](sql_msg &) {});
    }

    auto item_award_manager::remove(uint32_t award_id) -> bool {
        auto it = map_award_.find(award_id);
        if (it == map_award_.end())
            return false;

        if (auto set = map_set_award_by_login_.find(it->second->login); set != map_set_award_by_login_.end())
            set->second.erase(it->second.get());
        map_award_.erase(it);
        return true;
    }
}
