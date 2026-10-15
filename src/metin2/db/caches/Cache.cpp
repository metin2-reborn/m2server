
#include <metin2/db/caches/Cache.h>

#include <metin2/db/clients/ClientManager.h>
#include <metin2/db/storage/DBManager.h>
#include <metin2/db/app/Main.h>
#include <metin2/db/clients/QID.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/config/config.h>
#include <metin2/log/log.h>

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

// This is weird...
// If it was deleted, shouldn't the Cache be released too???
// But there's no part that releases the Cache.
// Did I just not find it?
// Like this, it keeps deleting the item every time the timer fires...
// The item is already gone... a double tap??????
// fixme
// by rtsummit
void item_cache::delete_() {
    if (data_.vnum == 0) {
        return;
    }

    if (metin2::config::test_server()) {
        metin2::log::info( "ItemCache::Delete : DELETE {:d}", data_.id);
    }

    data_.vnum = 0;
    need_query_ = true;
    last_update_time_ = time(nullptr);
    on_flush();

    // m_lastUpdateTime = time(0) - m_expireTime; // make it time out immediately.
}

void item_cache::on_flush() {
    if (data_.vnum == 0) // A vnum of 0 means it is marked for deletion.
    {
        char szQuery[QUERY_MAX_LEN];
        snprintf(szQuery, sizeof(szQuery), "DELETE FROM item%s WHERE id=%u", get_table_postfix(), data_.id);
        db_manager::instance().return_query(szQuery, QID_ITEM_DESTROY, 0, nullptr);

        if (metin2::config::test_server()) {
            metin2::log::info( "ItemCache::Flush : DELETE {:d} {}", data_.id, szQuery);
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
            metin2::log::info( "ItemCache::Flush :REPLACE  ({})", szItemQuery);

        db_manager::instance().return_query(szItemQuery, QID_ITEM_SAVE, 0, nullptr);

        ++item_count;
    }

    need_query_ = false;
}

//
// CPlayerTableCache
//
player_table_cache::player_table_cache() {
    expire_time_ = std::min(1800, metin2::config::db().player_cache_flush_seconds);
}

player_table_cache::~player_table_cache() {
}

void player_table_cache::on_flush() {
    if (metin2::config::test_server())
        metin2::log::info( "PlayerTableCache::Flush : {}", data_.name);

    const std::string query = create_player_save_query(&data_);
    db_manager::instance().return_query(query.c_str(), QID_PLAYER_SAVE, 0, nullptr);
}

const int item_price_list_table_cache::min_flush_sec_ = 1800;

item_price_list_table_cache::item_price_list_table_cache() {
    expire_time_ = std::min(min_flush_sec_, metin2::config::db().pricelist_cache_flush_seconds);
}

void item_price_list_table_cache::update_list(const item_price_list_table *pUpdateList) {
    //
    // Find items duplicating already-cached items and put the non-duplicated old info into tmpvec.
    //

    std::vector<item_price_info> tmpvec;

    for (uint idx = 0; idx < data_.count; ++idx) {
        const item_price_info *pos = pUpdateList->price_info;
        for (; pos != pUpdateList->price_info + pUpdateList->count && data_.price_info[idx].vnum != pos->vnum; ++pos)
            ;

        if (pos == pUpdateList->price_info + pUpdateList->count)
            tmpvec.push_back(data_.price_info[idx]);
    }

    //
    // Copy pUpdateList into m_data and fill the remaining space from the front of tmpvec.
    //

    if (pUpdateList->count > SHOP_PRICELIST_MAX_NUM) {
        metin2::log::error("Count overflow!");
        return;
    }

    data_.count = pUpdateList->count;

    std::memcpy(data_.price_info, pUpdateList->price_info, sizeof(item_price_info) * pUpdateList->count);

    int nDeletedNum; // Number of deleted price entries

    if (pUpdateList->count < SHOP_PRICELIST_MAX_NUM) {
        size_t sizeAddOldDataSize = SHOP_PRICELIST_MAX_NUM - pUpdateList->count;

        if (tmpvec.size() < sizeAddOldDataSize) {
            sizeAddOldDataSize = tmpvec.size();
        }

        std::memcpy(data_.price_info + pUpdateList->count, &tmpvec[0], sizeof(item_price_info) * sizeAddOldDataSize);
        data_.count += sizeAddOldDataSize;

        nDeletedNum = tmpvec.size() - sizeAddOldDataSize;
    } else {
        nDeletedNum = tmpvec.size();
    }

    need_query_ = true;

    metin2::log::info(
        "ItemPriceListTableCache::UpdateList : OwnerID[{:d}] Update [{:d}] Items, Delete [{:d}] Items, Total [{:d}] Items",
        data_.owner_id, pUpdateList->count, nDeletedNum, data_.count);
}

void item_price_list_table_cache::on_flush() {
    char szQuery[QUERY_MAX_LEN];

    //
    // Delete all item price info previously stored in the DB for this cache's owner.
    //

    snprintf(szQuery, sizeof(szQuery), "DELETE FROM myshop_pricelist%s WHERE owner_id = %u", get_table_postfix(), data_.owner_id);
    db_manager::instance().return_query(szQuery, QID_ITEMPRICE_DESTROY, 0, nullptr);

    //
    // Write the entire cache contents to the DB.
    //

    for (int idx = 0; idx < data_.count; ++idx) {
        snprintf(szQuery, sizeof(szQuery),
            // "INSERT INTO myshop_pricelist%s(owner_id, item_vnum, price) VALUES(%u, %u, %u)",
            "REPLACE myshop_pricelist%s (owner_id, item_vnum, price) VALUES(%u, %u, %u)",
            get_table_postfix(), data_.owner_id, data_.price_info[idx].vnum, data_.price_info[idx].price);
        db_manager::instance().return_query(szQuery, QID_ITEMPRICE_SAVE, 0, nullptr);
    }

    metin2::log::info( "ItemPriceListTableCache::Flush : OwnerID[{:d}] Update [{:d}]Items", data_.owner_id, data_.count);

    need_query_ = false;
}
}
