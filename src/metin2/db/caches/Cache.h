#pragma once

#include <metin2/common/cache.h>
#include <metin2/proto/tables.h>

#include <cstdint>

namespace metin2::db::inline caches {

class CItemCache : public cache<TPlayerItem> {
  public:
    CItemCache();

    ~CItemCache() override;

    void Delete();
    void OnFlush() override;
};

class CPlayerTableCache : public cache<TPlayerTable> {
  public:
    CPlayerTableCache();

    ~CPlayerTableCache() override;

    void OnFlush() override;

    auto GetLastUpdateTime() -> uint32_t { return m_lastUpdateTime; }
};

/**
 * @class	CItemPriceListTableCache
 * @brief	Cache class for private shop item price lists
 * @version	05/06/10 Bang2ni - First release.
 */
class CItemPriceListTableCache : public cache<TItemPriceListTable> {
  public:
    /**
     * Sets the cache expiration time.
     */
    CItemPriceListTableCache(void);

    /**
     * @param [in]	pUpdateList list to update with
     *
     * Updates the cached price info.
     * If the price list is full, previously cached entries are removed from the back.
     */
    void UpdateList(const TItemPriceListTable *pUpdateList);

    /// Writes the price info to the DB.
    void OnFlush(void) override;

  private:
    static const int s_nMinFlushSec; ///< Minimum cache expire time
};
}
