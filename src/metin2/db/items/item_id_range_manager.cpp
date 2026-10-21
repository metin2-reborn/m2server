#include <metin2/db/items/item_id_range_manager.h>

#include <metin2/db/clients/client_manager.h>
#include <metin2/db/clients/peer.h>
#include <metin2/db/storage/db_manager.h>

#include <metin2/common/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/tables.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>

namespace metin2::db::inline items {
    namespace {
        auto query_number(const std::string &query, uint64_t &out) -> bool {
            const auto msg = get_db_manager().direct_query(query);
            if (msg->sql_errno != 0 || !msg->get() || msg->get()->num_rows == 0) {
                return false;
            }

            const MYSQL_ROW row = mysql_fetch_row(msg->get()->res.get());
            out = 0;
            if (row[0]) {
                str_to_number(out, row[0]);
            }
            return true;
        }
    }

    auto item_id_range_manager::build() -> bool {
        uint64_t stored = 0;
        if (!query_number("SELECT next_range FROM item_id_state", stored)) {
            metin2::log::error("Item id ranges: cannot read item_id_state (the table needs exactly one row)");
            return false;
        }

        uint64_t highest = 0;
        if (!query_number("SELECT MAX(id) FROM item", highest)) {
            metin2::log::error("Item id ranges: cannot read the highest item id");
            return false;
        }

        next_range_ = std::max({stored, highest / RANGE_SIZE + 1, uint64_t{1}});
        metin2::log::info("Item id ranges: next range {:d} (stored {:d}, highest item id {:d})", next_range_, stored, highest);
        return true;
    }

    auto item_id_range_manager::range() -> item_id_range_table {
        const uint64_t index = next_range_;

        const auto msg = get_db_manager().direct_query(sql::format("UPDATE item_id_state SET next_range = {}", index + 1));
        if (msg->sql_errno != 0 || !msg->get() || msg->get()->affected_rows != 1) {
            metin2::log::error("Item id ranges: cannot reserve range {:d}", index);
            return {};
        }

        ++next_range_;

        return {
            .min = index * RANGE_SIZE,
            .max = (index + 1) * RANGE_SIZE - 1,
            .usable_item_id_min = index * RANGE_SIZE,
        };
    }

    auto item_id_range_manager::build_range(uint64_t min_value, uint64_t max_value, item_id_range_table &range) -> bool {
        uint64_t item_max_id = 0;
        std::unique_ptr<sql_msg> msg;
        MYSQL_ROW row;

        const std::string max_query = sql::format("SELECT MAX(id) FROM item WHERE id >= {} and id <= {}", min_value, max_value);

        msg = get_db_manager().direct_query(max_query);

        if (msg != nullptr) {
            if (msg->get()->num_rows > 0) {
                row = mysql_fetch_row(msg->get()->res.get());
                str_to_number(item_max_id, row[0]);
            }
        }

        if (item_max_id == 0)
            item_max_id = min_value;
        else
            item_max_id++;

        if ((max_value < item_max_id) || (max_value - item_max_id < minimum_remain_count_)) {
            metin2::log::info("Item id range: skipping {:d} to {:d} (next free id {:d}), fewer than {:d} ids left",
                min_value, max_value, item_max_id, minimum_remain_count_);
        } else {
            range.min = min_value;
            range.max = max_value;
            range.usable_item_id_min = item_max_id;

            const std::string count_query = sql::format("SELECT COUNT(*) FROM item WHERE id >= {} AND id <= {}", range.usable_item_id_min, range.max);

            msg = get_db_manager().direct_query(count_query);

            if (msg != nullptr) {
                if (msg->get()->num_rows > 0) {
                    uint64_t count = 0;
                    row = mysql_fetch_row(msg->get()->res.get());
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
}
