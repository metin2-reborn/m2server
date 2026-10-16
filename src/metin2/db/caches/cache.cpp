
#include <metin2/db/caches/cache.h>

#include <metin2/db/app/main.h>
#include <metin2/db/clients/client_manager.h>
#include <metin2/db/storage/db_manager.h>

#include <metin2/config/config.h>
#include <metin2/log/log.h>
#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include <sys/types.h>

namespace metin2::db::inline caches {

    item_cache::item_cache() {
        expire_time_ = std::min(1800, metin2::config::db().item_cache_flush_seconds);
    }

    item_cache::~item_cache() {
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
        last_update_time_ = time(nullptr);
        on_flush();
    }

    void item_cache::on_flush() {
        if (data_.vnum == 0) {
            char szQuery[QUERY_MAX_LEN];
            snprintf(szQuery, sizeof(szQuery), "DELETE FROM item%s WHERE id=%u", get_table_postfix(), data_.id);
            db_manager::instance().return_query(szQuery, [](sql_msg &) {});

            if (metin2::config::test_server()) {
                metin2::log::debug("Item cache: deleting item {:d}: {}", data_.id, szQuery);
            }
        } else {
            int32_t alSockets[ITEM_SOCKET_MAX_NUM];
            player_item_attribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
            bool isSocket = false, isAttr = false;

            memset(&alSockets, 0, sizeof(int32_t) * ITEM_SOCKET_MAX_NUM);
            memset(&aAttr, 0, sizeof(player_item_attribute) * ITEM_ATTRIBUTE_MAX_NUM);

            player_item *p = &data_;

            if (memcmp(alSockets, p->sockets, sizeof(int32_t) * ITEM_SOCKET_MAX_NUM)) {
                isSocket = true;
            }

            if (memcmp(aAttr, p->attr, sizeof(player_item_attribute) * ITEM_ATTRIBUTE_MAX_NUM)) {
                isAttr = true;
            }

            char szColumns[QUERY_MAX_LEN];
            char szValues[QUERY_MAX_LEN];
            char szUpdate[QUERY_MAX_LEN];

            int iLen = snprintf(szColumns, sizeof(szColumns), "id, owner_id, window, pos, count, vnum");

            int iValueLen = snprintf(szValues, sizeof(szValues), "%u, %u, %d, %d, %u, %u", p->id, p->owner, p->window, p->pos, p->count, p->vnum);
            int iUpdateLen = snprintf(szUpdate, sizeof(szUpdate), "owner_id=%u, window=%d, pos=%d, count=%u, vnum=%u", p->owner, p->window, p->pos, p->count, p->vnum);

            if (isSocket) {
                iLen += snprintf(szColumns + iLen, sizeof(szColumns) - iLen, ", socket0, socket1, socket2");
                iValueLen += snprintf(szValues + iValueLen, sizeof(szValues) - iValueLen,
                    ", %u, %u, %u", p->sockets[0], p->sockets[1], p->sockets[2]);
                iUpdateLen += snprintf(szUpdate + iUpdateLen, sizeof(szUpdate) - iUpdateLen,
                    ", socket0=%u, socket1=%u, socket2=%u", p->sockets[0], p->sockets[1], p->sockets[2]);
            }

            if (isAttr) {
                iLen += snprintf(szColumns + iLen, sizeof(szColumns) - iLen,
                    ", attrtype0, attrvalue0, attrtype1, attrvalue1, attrtype2, attrvalue2, attrtype3, attrvalue3"
                    ", attrtype4, attrvalue4, attrtype5, attrvalue5, attrtype6, attrvalue6");

                iValueLen += snprintf(szValues + iValueLen, sizeof(szValues) - iValueLen,
                    ", %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d",
                    p->attr[0].type, p->attr[0].s_value,
                    p->attr[1].type, p->attr[1].s_value,
                    p->attr[2].type, p->attr[2].s_value,
                    p->attr[3].type, p->attr[3].s_value,
                    p->attr[4].type, p->attr[4].s_value,
                    p->attr[5].type, p->attr[5].s_value,
                    p->attr[6].type, p->attr[6].s_value);

                iUpdateLen += snprintf(szUpdate + iUpdateLen, sizeof(szUpdate) - iUpdateLen,
                    ", attrtype0=%d, attrvalue0=%d"
                    ", attrtype1=%d, attrvalue1=%d"
                    ", attrtype2=%d, attrvalue2=%d"
                    ", attrtype3=%d, attrvalue3=%d"
                    ", attrtype4=%d, attrvalue4=%d"
                    ", attrtype5=%d, attrvalue5=%d"
                    ", attrtype6=%d, attrvalue6=%d",
                    p->attr[0].type, p->attr[0].s_value,
                    p->attr[1].type, p->attr[1].s_value,
                    p->attr[2].type, p->attr[2].s_value,
                    p->attr[3].type, p->attr[3].s_value,
                    p->attr[4].type, p->attr[4].s_value,
                    p->attr[5].type, p->attr[5].s_value,
                    p->attr[6].type, p->attr[6].s_value);
            }

            char szItemQuery[QUERY_MAX_LEN + QUERY_MAX_LEN];
            snprintf(szItemQuery, sizeof(szItemQuery), "REPLACE INTO item%s (%s) VALUES(%s)", get_table_postfix(), szColumns, szValues);

            if (metin2::config::test_server())
                metin2::log::debug("Item cache: saving item: {}", szItemQuery);

            db_manager::instance().return_query(szItemQuery, [](sql_msg &) {});

            ++item_count;
        }

        need_query_ = false;
    }

    player_table_cache::player_table_cache() {
        expire_time_ = std::min(1800, metin2::config::db().player_cache_flush_seconds);
    }

    player_table_cache::~player_table_cache() {
    }

    void player_table_cache::on_flush() {
        if (metin2::config::test_server())
            metin2::log::debug("Player cache: saving player {}", data_.name);

        const std::string query = create_player_save_query(&data_);
        db_manager::instance().return_query(query.c_str(), [](sql_msg &) {});
    }

    const int item_price_list_table_cache::min_flush_sec_ = 1800;

    item_price_list_table_cache::item_price_list_table_cache() {
        expire_time_ = std::min(min_flush_sec_, metin2::config::db().pricelist_cache_flush_seconds);
    }

    void item_price_list_table_cache::update_list(const item_price_list_table *update_list) {

        std::vector<item_price_info> tmpvec;

        for (uint idx = 0; idx < data_.count; ++idx) {
            const item_price_info *pos = update_list->price_info;
            for (; pos != update_list->price_info + update_list->count && data_.price_info[idx].vnum != pos->vnum; ++pos)
                ;

            if (pos == update_list->price_info + update_list->count)
                tmpvec.push_back(data_.price_info[idx]);
        }

        if (update_list->count > SHOP_PRICELIST_MAX_NUM) {
            metin2::log::warn("Shop price list update has too many entries, ignored");
            return;
        }

        data_.count = update_list->count;

        std::memcpy(data_.price_info, update_list->price_info, sizeof(item_price_info) * update_list->count);

        int nDeletedNum;

        if (update_list->count < SHOP_PRICELIST_MAX_NUM) {
            size_t sizeAddOldDataSize = SHOP_PRICELIST_MAX_NUM - update_list->count;

            if (tmpvec.size() < sizeAddOldDataSize) {
                sizeAddOldDataSize = tmpvec.size();
            }

            std::memcpy(data_.price_info + update_list->count, &tmpvec[0], sizeof(item_price_info) * sizeAddOldDataSize);
            data_.count += sizeAddOldDataSize;

            nDeletedNum = tmpvec.size() - sizeAddOldDataSize;
        } else {
            nDeletedNum = tmpvec.size();
        }

        need_query_ = true;

        metin2::log::info(
            "Price list: owner pid {:d} updated {:d} items, deleted {:d}, total {:d}",
            data_.owner_id, update_list->count, nDeletedNum, data_.count);
    }

    void item_price_list_table_cache::on_flush() {
        char szQuery[QUERY_MAX_LEN];

        snprintf(szQuery, sizeof(szQuery), "DELETE FROM myshop_pricelist%s WHERE owner_id = %u", get_table_postfix(), data_.owner_id);
        db_manager::instance().return_query(szQuery, [](sql_msg &) {});

        for (int idx = 0; idx < data_.count; ++idx) {
            snprintf(szQuery, sizeof(szQuery),
                "REPLACE myshop_pricelist%s (owner_id, item_vnum, price) VALUES(%u, %u, %u)",
                get_table_postfix(), data_.owner_id, data_.price_info[idx].vnum, data_.price_info[idx].price);
            db_manager::instance().return_query(szQuery, [](sql_msg &) {});
        }

        metin2::log::debug("Price list: saving {1:d} items for owner pid {0:d}", data_.owner_id, data_.count);

        need_query_ = false;
    }
}
