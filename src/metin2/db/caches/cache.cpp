
#include <metin2/db/caches/cache.h>

#include <metin2/db/clients/client_manager.h>
#include <metin2/db/storage/db_manager.h>

#include <metin2/config/config.h>
#include <metin2/log/log.h>
#include <metin2/proto/shop_types.h>
#include <metin2/proto/tables.h>
#include <metin2/sql/query.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <vector>

#include <sys/types.h>

namespace metin2::db::inline caches {
    item_cache::item_cache() {
        expire_time_ = std::chrono::seconds(std::min(1800, metin2::config::db().item_cache_flush_seconds));
    }

    void item_cache::remove() {
        if (data_.vnum == 0) {
            return;
        }

        if (metin2::config::test_server()) {
            metin2::log::debug("Item cache: marked item {:d} for deletion", data_.id);
        }

        data_.vnum = 0;
        need_query_ = true;
        last_update_time_ = clock::now();

        on_flush();
    }

    void item_cache::on_flush() {
        if (data_.vnum == 0) {
            const auto query = sql::format("DELETE FROM item WHERE id={}", data_.id);

            get_db_manager().return_query(query, [](sql_msg &) {});

            if (metin2::config::test_server()) {
                metin2::log::debug("Item cache: deleting item {:d}: {}", data_.id, query);
            }
        } else {
            const auto query = create_item_save_query(data_);

            if (metin2::config::test_server()) {
                metin2::log::debug("Item cache: saving item: {}", query);
            }

            get_db_manager().return_query(query, [](sql_msg &) {});

            ++item_count;
        }

        need_query_ = false;
    }

    player_table_cache::player_table_cache() {
        expire_time_ = std::chrono::seconds(std::min(1800, metin2::config::db().player_cache_flush_seconds));
    }

    void player_table_cache::on_flush() {
        if (metin2::config::test_server()) {
            metin2::log::debug("Player cache: saving player {}", data_.name);
        }

        const auto query = create_player_save_query(&data_);

        get_db_manager().return_query(query, [](sql_msg &) -> void {});
    }

    const int item_price_list_table_cache::min_flush_sec_ = 1800;

    item_price_list_table_cache::item_price_list_table_cache() {
        expire_time_ = std::chrono::seconds(std::min(min_flush_sec_, metin2::config::db().pricelist_cache_flush_seconds));
    }

    void item_price_list_table_cache::update_list(const item_price_list_table *update_list) {
        std::vector<item_price_info> tmpvec;

        for (uint idx = 0; idx < data_.count; ++idx) {
            const item_price_info *pos = update_list->price_info;
            for (; pos != update_list->price_info + update_list->count && data_.price_info[idx].vnum != pos->vnum; ++pos)
                ;

            if (pos == update_list->price_info + update_list->count) {
                tmpvec.push_back(data_.price_info[idx]);
            }
        }

        if (update_list->count > SHOP_PRICELIST_MAX_NUM) {
            metin2::log::warn("Shop price list update has too many entries, ignored");
            return;
        }

        data_.count = update_list->count;

        std::memcpy(data_.price_info, update_list->price_info, sizeof(item_price_info) * update_list->count);

        int deleted_num;

        if (update_list->count < SHOP_PRICELIST_MAX_NUM) {
            size_t size_add_old_data_size = SHOP_PRICELIST_MAX_NUM - update_list->count;

            if (tmpvec.size() < size_add_old_data_size) {
                size_add_old_data_size = tmpvec.size();
            }

            std::memcpy(data_.price_info + update_list->count, tmpvec.data(), sizeof(item_price_info) * size_add_old_data_size);
            data_.count += size_add_old_data_size;

            deleted_num = tmpvec.size() - size_add_old_data_size;
        } else {
            deleted_num = tmpvec.size();
        }

        need_query_ = true;

        metin2::log::info(
            "Price list: owner pid {:d} updated {:d} items, deleted {:d}, total {:d}",
            data_.owner_id, update_list->count, deleted_num, data_.count);
    }

    void item_price_list_table_cache::on_flush() {
        const auto query = sql::format("DELETE FROM myshop_pricelist WHERE owner_id = {}", data_.owner_id);
        get_db_manager().return_query(query, [](sql_msg &) {});

        for (int idx = 0; idx < data_.count; ++idx) {
            const auto replace_query = sql::format(
                "REPLACE myshop_pricelist (owner_id, item_vnum, price) VALUES({}, {}, {})", data_.owner_id, data_.price_info[idx].vnum, data_.price_info[idx].price);

            get_db_manager().return_query(replace_query, [](sql_msg &) -> void {});
        }

        metin2::log::debug("Price list: saving {1:d} items for owner pid {0:d}", data_.owner_id, data_.count);

        need_query_ = false;
    }
}
