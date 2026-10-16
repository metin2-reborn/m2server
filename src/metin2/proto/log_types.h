#pragma once

namespace metin2::inline proto {
    enum priv_type {
        PRIV_NONE,
        PRIV_ITEM_DROP,
        PRIV_GOLD_DROP,
        PRIV_GOLD10_DROP,
        PRIV_EXP_PCT,
        MAX_PRIV_NUM,
    };

    enum money_log_type {
        MONEY_LOG_RESERVED,
        MONEY_LOG_MONSTER,
        MONEY_LOG_SHOP,
        MONEY_LOG_REFINE,
        MONEY_LOG_QUEST,
        MONEY_LOG_GUILD,
        MONEY_LOG_MISC,
        MONEY_LOG_MONSTER_KILL,
        MONEY_LOG_DROP,
        MONEY_LOG_TYPE_MAX_NUM,
    };
}
