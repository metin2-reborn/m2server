#pragma once

#include <metin2/common/cache.h>
#include <metin2/proto/tables.h>

#include <cstdint>

namespace metin2::db::inline caches {

class item_cache : public cache<player_item> {
  public:
    item_cache();

    ~item_cache() override;

    void remove();
    void on_flush() override;
};

class player_table_cache : public cache<player_table> {
  public:
    player_table_cache();

    ~player_table_cache() override;

    void on_flush() override;

    auto last_update_time() -> uint32_t { return last_update_time_; }
};

/**
 * @class	CItemPriceListTableCache
 * @brief	Cache class for private shop item price lists
 * @version	05/06/10 Bang2ni - First release.
 */
class item_price_list_table_cache : public cache<item_price_list_table> {
  public:
    /**
     * Sets the cache expiration time.
     */
    item_price_list_table_cache(void);

    /**
     * @param [in]	pUpdateList list to update with
     *
     * Updates the cached price info.
     * If the price list is full, previously cached entries are removed from the back.
     */
    void update_list(const item_price_list_table *update_list);

    /// Writes the price info to the DB.
    void on_flush(void) override;

  private:
    static const int min_flush_sec_; ///< Minimum cache expire time
};
}
