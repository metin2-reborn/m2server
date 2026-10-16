#include <metin2/db/items/item_id_range_manager.h>

#include <metin2/db/app/main.h>
#include <metin2/db/clients/client_manager.h>
#include <metin2/db/clients/peer.h>
#include <metin2/db/storage/db_manager.h>

#include <metin2/common/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/tables.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <cstdint>
#include <memory>
#include <string>

namespace metin2::db::inline items {

    item_id_range_manager::item_id_range_manager() {
        data_.clear();
    }

    void item_id_range_manager::build() {
        uint32_t dwMin = 0;
        uint32_t dwMax = 0;
        item_id_range_table range;

        for (int i = 0;; ++i) {
            dwMin = minimum_range_ * (i + 1) + 1;
            dwMax = minimum_range_ * (i + 2);

            if (dwMax == max_item_id_)
                break;

            if (client_manager::instance().item_range().min <= dwMin &&
                client_manager::instance().item_range().max >= dwMax) {
                continue;
            }

            if (build_range(dwMin, dwMax, range) == true) {
                data_.push_back(range);
            }
        }
    }

    auto item_id_range_manager::range() -> item_id_range_table {
        item_id_range_table ret;
        ret.min = 0;
        ret.max = 0;
        ret.usable_item_id_min = 0;

        if (data_.size() > 0) {
            while (data_.size() > 0) {
                ret = data_.front();
                data_.pop_front();

                bool collides = false;
                client_manager::instance().for_each_peer([&](peer *p) {
                    collides = collides || !p->is_item_id_range_free(ret);
                });

                if (!collides)
                    return ret;
            }
        }

        metin2::log::error("Item id range: no free item id range left");

        return ret;
    }

    auto item_id_range_manager::build_range(uint32_t min_value, uint32_t max_value, item_id_range_table &range) -> bool {
        uint32_t dwItemMaxID = 0;
        std::unique_ptr<sql_msg> pMsg;
        MYSQL_ROW row;

        const std::string max_query = sql::format("SELECT MAX(id) FROM item{} WHERE id >= {} and id <= {}", sql::raw(get_table_postfix()), min_value, max_value);

        pMsg = db_manager::instance().direct_query(max_query);

        if (pMsg != nullptr) {
            if (pMsg->get()->num_rows > 0) {
                row = mysql_fetch_row(pMsg->get()->res.get());
                str_to_number(dwItemMaxID, row[0]);
            }
        }

        if (dwItemMaxID == 0)
            dwItemMaxID = min_value;
        else
            dwItemMaxID++;

        if ((max_value < dwItemMaxID) || (max_value - dwItemMaxID < minimum_remain_count_)) {
            metin2::log::info("Item id range: skipping {:d} to {:d} (next free id {:d}), fewer than {:d} ids left",
                min_value, max_value, dwItemMaxID, minimum_remain_count_);
        } else {
            range.min = min_value;
            range.max = max_value;
            range.usable_item_id_min = dwItemMaxID;

            const std::string count_query = sql::format("SELECT COUNT(*) FROM item{} WHERE id >= {} AND id <= {}",
                sql::raw(get_table_postfix()), range.usable_item_id_min, range.max);

            pMsg = db_manager::instance().direct_query(count_query);

            if (pMsg != nullptr) {
                if (pMsg->get()->num_rows > 0) {
                    uint32_t count = 0;
                    row = mysql_fetch_row(pMsg->get()->res.get());
                    str_to_number(count, row[0]);

                    if (count > 0) {
                        metin2::log::error("Item id range: {:d} to {:d} already contains items", range.usable_item_id_min, range.max);
                        return false;
                    } else {
                        metin2::log::info("Item id range: built {:d} to {:d}, next free id {:d}", range.min, range.max, range.usable_item_id_min);
                        return true;
                    }
                }
            }
        }

        return false;
    }

    void item_id_range_manager::update_range(uint32_t min, uint32_t max) {
        item_id_range_table range;

        if (build_range(min, max, range) == true) {
            data_.push_back(range);
        }
    }
}
