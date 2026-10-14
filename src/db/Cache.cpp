
#include "Cache.h"

#include "ClientManager.h"
#include "DBManager.h"
#include "Main.h"
#include "QID.h"

#include <common/item_length.h>
#include <common/length.h>
#include <common/tables.h>
#include <libthecore/log.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>

#include <sys/types.h>

CItemCache::CItemCache() {
    m_expireTime = std::min(1800, g_iItemCacheFlushSeconds);
}

CItemCache::~CItemCache() {
}

// This is weird...
// If it was deleted, shouldn't the Cache be released too???
// But there's no part that releases the Cache.
// Did I just not find it?
// Like this, it keeps deleting the item every time the timer fires...
// The item is already gone... a double tap??????
// fixme
// by rtsummit
void CItemCache::Delete() {
    if (m_data.vnum == 0) {
        return;
    }

    if (g_test_server) {
        sys_log(0, "ItemCache::Delete : DELETE %u", m_data.id);
    }

    m_data.vnum = 0;
    m_bNeedQuery = true;
    m_lastUpdateTime = time(nullptr);
    OnFlush();

    // m_lastUpdateTime = time(0) - m_expireTime; // make it time out immediately.
}

void CItemCache::OnFlush() {
    if (m_data.vnum == 0) // A vnum of 0 means it is marked for deletion.
    {
        char szQuery[QUERY_MAX_LEN];
        snprintf(szQuery, sizeof(szQuery), "DELETE FROM item%s WHERE id=%u", GetTablePostfix(), m_data.id);
        CDBManager::instance().ReturnQuery(szQuery, QID_ITEM_DESTROY, 0, nullptr);

        if (g_test_server) {
            sys_log(0, "ItemCache::Flush : DELETE %u %s", m_data.id, szQuery);
        }
    } else {
        int32_t alSockets[ITEM_SOCKET_MAX_NUM];
        TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
        bool isSocket = false, isAttr = false;

        memset(&alSockets, 0, sizeof(int32_t) * ITEM_SOCKET_MAX_NUM);
        memset(&aAttr, 0, sizeof(TPlayerItemAttribute) * ITEM_ATTRIBUTE_MAX_NUM);

        TPlayerItem *p = &m_data;

        if (memcmp(alSockets, p->alSockets, sizeof(int32_t) * ITEM_SOCKET_MAX_NUM)) {
            isSocket = true;
        }

        if (memcmp(aAttr, p->aAttr, sizeof(TPlayerItemAttribute) * ITEM_ATTRIBUTE_MAX_NUM)) {
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
                ", %u, %u, %u", p->alSockets[0], p->alSockets[1], p->alSockets[2]);
            iUpdateLen += snprintf(szUpdate + iUpdateLen, sizeof(szUpdate) - iUpdateLen,
                ", socket0=%u, socket1=%u, socket2=%u", p->alSockets[0], p->alSockets[1], p->alSockets[2]);
        }

        if (isAttr) {
            iLen += snprintf(szColumns + iLen, sizeof(szColumns) - iLen,
                ", attrtype0, attrvalue0, attrtype1, attrvalue1, attrtype2, attrvalue2, attrtype3, attrvalue3"
                ", attrtype4, attrvalue4, attrtype5, attrvalue5, attrtype6, attrvalue6");

            iValueLen += snprintf(szValues + iValueLen, sizeof(szValues) - iValueLen,
                ", %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d",
                p->aAttr[0].bType, p->aAttr[0].sValue,
                p->aAttr[1].bType, p->aAttr[1].sValue,
                p->aAttr[2].bType, p->aAttr[2].sValue,
                p->aAttr[3].bType, p->aAttr[3].sValue,
                p->aAttr[4].bType, p->aAttr[4].sValue,
                p->aAttr[5].bType, p->aAttr[5].sValue,
                p->aAttr[6].bType, p->aAttr[6].sValue);

            iUpdateLen += snprintf(szUpdate + iUpdateLen, sizeof(szUpdate) - iUpdateLen,
                ", attrtype0=%d, attrvalue0=%d"
                ", attrtype1=%d, attrvalue1=%d"
                ", attrtype2=%d, attrvalue2=%d"
                ", attrtype3=%d, attrvalue3=%d"
                ", attrtype4=%d, attrvalue4=%d"
                ", attrtype5=%d, attrvalue5=%d"
                ", attrtype6=%d, attrvalue6=%d",
                p->aAttr[0].bType, p->aAttr[0].sValue,
                p->aAttr[1].bType, p->aAttr[1].sValue,
                p->aAttr[2].bType, p->aAttr[2].sValue,
                p->aAttr[3].bType, p->aAttr[3].sValue,
                p->aAttr[4].bType, p->aAttr[4].sValue,
                p->aAttr[5].bType, p->aAttr[5].sValue,
                p->aAttr[6].bType, p->aAttr[6].sValue);
        }

        char szItemQuery[QUERY_MAX_LEN + QUERY_MAX_LEN];
        snprintf(szItemQuery, sizeof(szItemQuery), "REPLACE INTO item%s (%s) VALUES(%s)", GetTablePostfix(), szColumns, szValues);

        if (g_test_server)
            sys_log(0, "ItemCache::Flush :REPLACE  (%s)", szItemQuery);

        CDBManager::instance().ReturnQuery(szItemQuery, QID_ITEM_SAVE, 0, nullptr);

        ++g_item_count;
    }

    m_bNeedQuery = false;
}

//
// CPlayerTableCache
//
CPlayerTableCache::CPlayerTableCache() {
    m_expireTime = std::min(1800, g_iPlayerCacheFlushSeconds);
}

CPlayerTableCache::~CPlayerTableCache() {
}

void CPlayerTableCache::OnFlush() {
    if (g_test_server)
        sys_log(0, "PlayerTableCache::Flush : %s", m_data.name);

    char szQuery[QUERY_MAX_LEN];
    CreatePlayerSaveQuery(szQuery, sizeof(szQuery), &m_data);
    CDBManager::instance().ReturnQuery(szQuery, QID_PLAYER_SAVE, 0, nullptr);
}

const int CItemPriceListTableCache::s_nMinFlushSec = 1800;

CItemPriceListTableCache::CItemPriceListTableCache() {
    m_expireTime = std::min(s_nMinFlushSec, g_iItemPriceListTableCacheFlushSeconds);
}

void CItemPriceListTableCache::UpdateList(const TItemPriceListTable *pUpdateList) {
    //
    // Find items duplicating already-cached items and put the non-duplicated old info into tmpvec.
    //

    std::vector<TItemPriceInfo> tmpvec;

    for (uint idx = 0; idx < m_data.byCount; ++idx) {
        const TItemPriceInfo *pos = pUpdateList->aPriceInfo;
        for (; pos != pUpdateList->aPriceInfo + pUpdateList->byCount && m_data.aPriceInfo[idx].dwVnum != pos->dwVnum; ++pos)
            ;

        if (pos == pUpdateList->aPriceInfo + pUpdateList->byCount)
            tmpvec.push_back(m_data.aPriceInfo[idx]);
    }

    //
    // Copy pUpdateList into m_data and fill the remaining space from the front of tmpvec.
    //

    if (pUpdateList->byCount > SHOP_PRICELIST_MAX_NUM) {
        sys_err("Count overflow!");
        return;
    }

    m_data.byCount = pUpdateList->byCount;

    std::memcpy(m_data.aPriceInfo, pUpdateList->aPriceInfo, sizeof(TItemPriceInfo) * pUpdateList->byCount);

    int nDeletedNum; // Number of deleted price entries

    if (pUpdateList->byCount < SHOP_PRICELIST_MAX_NUM) {
        size_t sizeAddOldDataSize = SHOP_PRICELIST_MAX_NUM - pUpdateList->byCount;

        if (tmpvec.size() < sizeAddOldDataSize) {
            sizeAddOldDataSize = tmpvec.size();
        }

        std::memcpy(m_data.aPriceInfo + pUpdateList->byCount, &tmpvec[0], sizeof(TItemPriceInfo) * sizeAddOldDataSize);
        m_data.byCount += sizeAddOldDataSize;

        nDeletedNum = tmpvec.size() - sizeAddOldDataSize;
    } else {
        nDeletedNum = tmpvec.size();
    }

    m_bNeedQuery = true;

    sys_log(0,
        "ItemPriceListTableCache::UpdateList : OwnerID[%u] Update [%u] Items, Delete [%u] Items, Total [%u] Items",
        m_data.dwOwnerID, pUpdateList->byCount, nDeletedNum, m_data.byCount);
}

void CItemPriceListTableCache::OnFlush() {
    char szQuery[QUERY_MAX_LEN];

    //
    // Delete all item price info previously stored in the DB for this cache's owner.
    //

    snprintf(szQuery, sizeof(szQuery), "DELETE FROM myshop_pricelist%s WHERE owner_id = %u", GetTablePostfix(), m_data.dwOwnerID);
    CDBManager::instance().ReturnQuery(szQuery, QID_ITEMPRICE_DESTROY, 0, nullptr);

    //
    // Write the entire cache contents to the DB.
    //

    for (int idx = 0; idx < m_data.byCount; ++idx) {
        snprintf(szQuery, sizeof(szQuery),
            // "INSERT INTO myshop_pricelist%s(owner_id, item_vnum, price) VALUES(%u, %u, %u)",
            "REPLACE myshop_pricelist%s (owner_id, item_vnum, price) VALUES(%u, %u, %u)",
            GetTablePostfix(), m_data.dwOwnerID, m_data.aPriceInfo[idx].dwVnum, m_data.aPriceInfo[idx].dwPrice);
        CDBManager::instance().ReturnQuery(szQuery, QID_ITEMPRICE_SAVE, 0, nullptr);
    }

    sys_log(0, "ItemPriceListTableCache::Flush : OwnerID[%u] Update [%u]Items", m_data.dwOwnerID, m_data.byCount);

    m_bNeedQuery = false;
}
