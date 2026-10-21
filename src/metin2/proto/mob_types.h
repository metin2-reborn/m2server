#pragma once

namespace metin2::inline proto {
    inline constexpr int MOB_SKILL_MAX_NUM = 5;

    enum mob_rank {
        MOB_RANK_PAWN,
        MOB_RANK_S_PAWN,
        MOB_RANK_KNIGHT,
        MOB_RANK_S_KNIGHT,
        MOB_RANK_BOSS,
        MOB_RANK_KING,
        MOB_RANK_MAX_NUM
    };

    enum on_click_events {
        ON_CLICK_NONE,
        ON_CLICK_SHOP,
        ON_CLICK_TALK,
        ON_CLICK_MAX_NUM
    };

    enum mob_sizes {
        MOBSIZE_RESERVED,
        MOBSIZE_SMALL,
        MOBSIZE_MEDIUM,
        MOBSIZE_BIG
    };

    enum ai_flags {
        AIFLAG_AGGRESSIVE = (1 << 0),
        AIFLAG_NOMOVE = (1 << 1),
        AIFLAG_COWARD = (1 << 2),
        AIFLAG_NOATTACKSHINSU = (1 << 3),
        AIFLAG_NOATTACKJINNO = (1 << 4),
        AIFLAG_NOATTACKCHUNJO = (1 << 5),
        AIFLAG_ATTACKMOB = (1 << 6),
        AIFLAG_BERSERK = (1 << 7),
        AIFLAG_STONESKIN = (1 << 8),
        AIFLAG_GODSPEED = (1 << 9),
        AIFLAG_DEATHBLOW = (1 << 10),
        AIFLAG_REVIVE = (1 << 11),
    };

    enum mob_stat_type {
        MOB_STATTYPE_POWER,
        MOB_STATTYPE_TANKER,
        MOB_STATTYPE_SUPER_POWER,
        MOB_STATTYPE_SUPER_TANKER,
        MOB_STATTYPE_RANGE,
        MOB_STATTYPE_MAGIC,
        MOB_STATTYPE_MAX_NUM
    };

    enum immune_flags {
        IMMUNE_STUN = (1 << 0),
        IMMUNE_SLOW = (1 << 1),
        IMMUNE_FALL = (1 << 2),
        IMMUNE_CURSE = (1 << 3),
        IMMUNE_POISON = (1 << 4),
        IMMUNE_TERROR = (1 << 5),
        IMMUNE_REFLECT = (1 << 6),
    };

    enum mob_enchants {
        MOB_ENCHANT_CURSE,
        MOB_ENCHANT_SLOW,
        MOB_ENCHANT_POISON,
        MOB_ENCHANT_STUN,
        MOB_ENCHANT_CRITICAL,
        MOB_ENCHANT_PENETRATE,
        MOB_ENCHANTS_MAX_NUM
    };

    enum mob_resists {
        MOB_RESIST_SWORD,
        MOB_RESIST_TWOHAND,
        MOB_RESIST_DAGGER,
        MOB_RESIST_BELL,
        MOB_RESIST_FAN,
        MOB_RESIST_BOW,
        MOB_RESIST_FIRE,
        MOB_RESIST_ELECT,
        MOB_RESIST_MAGIC,
        MOB_RESIST_WIND,
        MOB_RESIST_POISON,
        MOB_RESISTS_MAX_NUM
    };
}
