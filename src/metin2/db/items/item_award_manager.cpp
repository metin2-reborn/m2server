#include <metin2/db/items/item_award_manager.h>

#include <metin2/db/clients/client_manager.h>
#include <metin2/db/storage/db_manager.h>

#include <metin2/common/utils.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>
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
    item_award_manager::item_award_manager() = default;

    void item_award_manager::request_load() {
        const std::string query = sql::format("SELECT id,login,vnum,count,socket0,socket1,socket2,mall,why FROM item_award WHERE taken_time IS NULL and id > {}", last_cached_item_award_id);
        get_db_manager().return_query(query, [this](sql_msg &msg) { load(msg); });
    }

    void item_award_manager::load(sql_msg &msg) {
        MYSQL_RES *res = msg.get()->res.get();

        for (uint i = 0; i < msg.get()->num_rows; ++i) {
            MYSQL_ROW row = mysql_fetch_row(res);
            int col = 0;

            uint32_t id = 0;
            str_to_number(id, row[col++]);

            if (awards_.find(id) != awards_.end())
                continue;

            auto owned = std::make_unique<item_award>();
            item_award *data = owned.get();

            data->id = id;
            trim_and_lower(row[col++], data->login);
            str_to_number(data->vnum, row[col++]);
            str_to_number(data->count, row[col++]);
            str_to_number(data->socket0, row[col++]);
            str_to_number(data->socket1, row[col++]);
            str_to_number(data->socket2, row[col++]);
            str_to_number(data->mall, row[col++]);

            if (row[col]) {
                strlcpy(data->why, row[col], sizeof(data->why));
                get_client_manager().inform_gift_award(data->login, data->why, data->vnum);
            }

            awards_.emplace(id, std::move(owned));

            metin2::log::debug("Item award: loaded award {:d} for account {}, vnum {:d}, count {:d}, socket {:d}", data->id, data->login, data->vnum, data->count, data->socket0);
            std::set<item_award *> &set = set_awards_by_login_[data->login];
            set.insert(data);

            if (id > last_cached_item_award_id)
                last_cached_item_award_id = id;
        }
    }

    auto item_award_manager::by_login(const char *login) -> std::set<item_award *> * {
        auto it = set_awards_by_login_.find(login);

        if (it == set_awards_by_login_.end())
            return nullptr;

        return &it->second;
    }

    void item_award_manager::taken(uint32_t award_id, uint32_t item_id) {
        auto it = awards_.find(award_id);

        if (it == awards_.end()) {
            metin2::log::warn("Item award: award {:d} taken but not found", award_id);
            return;
        }

        item_award *k = it->second.get();
        k->taken = true;

        const std::string query = sql::format(
            "UPDATE item_award SET taken_time=NOW(),item_id={} WHERE id={} AND taken_time IS NULL",
            item_id, award_id);

        get_db_manager().return_query(query, [](sql_msg &) {});
    }

    auto item_award_manager::remove(uint32_t award_id) -> bool {
        auto it = awards_.find(award_id);
        if (it == awards_.end())
            return false;

        if (auto set = set_awards_by_login_.find(it->second->login); set != set_awards_by_login_.end())
            set->second.erase(it->second.get());
        awards_.erase(it);
        return true;
    }
}
