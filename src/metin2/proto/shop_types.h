#pragma once

namespace metin2::inline proto {
    inline constexpr int SHOP_SIGN_MAX_LEN = 32;
    inline constexpr int SHOP_HOST_ITEM_MAX_NUM = 40;
    inline constexpr int SHOP_PRICELIST_MAX_NUM = 40;
    inline constexpr int SHOP_TAB_NAME_MAX = 32;
    inline constexpr int SHOP_TAB_COUNT_MAX = 3;

    enum shop_coin_type {
        SHOP_COIN_TYPE_GOLD,
        SHOP_COIN_TYPE_SECONDARY_COIN,
    };
}
