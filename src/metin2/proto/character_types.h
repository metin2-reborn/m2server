#pragma once

namespace metin2::inline proto {
    inline constexpr int QUICKSLOT_MAX_NUM = 36;
    inline constexpr int PLAYER_EXP_TABLE_MAX = 120;
    inline constexpr int PLAYER_MAX_LEVEL_CONST = 120;
    inline constexpr int SKILL_MAX_NUM = 255;
    inline constexpr int SKILLBOOK_DELAY_MIN = 64800;
    inline constexpr int SKILLBOOK_DELAY_MAX = 108000;
    inline constexpr int SKILL_MAX_LEVEL = 40;
    inline constexpr int POINT_MAX_NUM = 255;

    enum sex {
        SEX_MALE,
        SEX_FEMALE
    };

    enum jobs {
        JOB_WARRIOR,
        JOB_ASSASSIN,
        JOB_SURA,
        JOB_SHAMAN,
        JOB_MAX_NUM
    };

    inline constexpr int SKILL_GROUP_MAX_NUM = 2;

    enum race_flags {
        RACE_FLAG_ANIMAL = (1 << 0),
        RACE_FLAG_UNDEAD = (1 << 1),
        RACE_FLAG_DEVIL = (1 << 2),
        RACE_FLAG_HUMAN = (1 << 3),
        RACE_FLAG_ORC = (1 << 4),
        RACE_FLAG_MILGYO = (1 << 5),
        RACE_FLAG_INSECT = (1 << 6),
        RACE_FLAG_FIRE = (1 << 7),
        RACE_FLAG_ICE = (1 << 8),
        RACE_FLAG_DESERT = (1 << 9),
        RACE_FLAG_TREE = (1 << 10),
        RACE_FLAG_ATT_ELEC = (1 << 11),
        RACE_FLAG_ATT_FIRE = (1 << 12),
        RACE_FLAG_ATT_ICE = (1 << 13),
        RACE_FLAG_ATT_WIND = (1 << 14),
        RACE_FLAG_ATT_EARTH = (1 << 15),
        RACE_FLAG_ATT_DARK = (1 << 16),
    };

    enum {
        QUICKSLOT_TYPE_NONE,
        QUICKSLOT_TYPE_ITEM,
        QUICKSLOT_TYPE_SKILL,
        QUICKSLOT_TYPE_COMMAND,
        QUICKSLOT_TYPE_MAX_NUM,
    };

    enum parts {
        PART_MAIN,
        PART_WEAPON,
        PART_HEAD,
        PART_HAIR,

        PART_MAX_NUM,
        PART_WEAPON_SUB,
    };

    enum character_position {
        POSITION_GENERAL,
        POSITION_BATTLE,
        POSITION_DYING,
        POSITION_SITTING_CHAIR,
        POSITION_SITTING_GROUND,
        POSITION_INTRO,
        POSITION_MAX_NUM
    };

    enum gm_levels {
        GM_PLAYER,
        GM_LOW_WIZARD,
        GM_WIZARD,
        GM_HIGH_WIZARD,
        GM_GOD,
        GM_IMPLEMENTOR
    };

    enum char_type {
        CHAR_TYPE_MONSTER,
        CHAR_TYPE_NPC,
        CHAR_TYPE_STONE,
        CHAR_TYPE_WARP,
        CHAR_TYPE_DOOR,
        CHAR_TYPE_BUILDING,
        CHAR_TYPE_PC,
        CHAR_TYPE_POLYMORPH_PC,
        CHAR_TYPE_HORSE,
        CHAR_TYPE_GOTO
    };

    enum battle_type {
        BATTLE_TYPE_MELEE,
        BATTLE_TYPE_RANGE,
        BATTLE_TYPE_MAGIC,
        BATTLE_TYPE_SPECIAL,
        BATTLE_TYPE_POWER,
        BATTLE_TYPE_TANKER,
        BATTLE_TYPE_SUPER_POWER,
        BATTLE_TYPE_SUPER_TANKER,
        BATTLE_TYPE_MAX_NUM
    };

    enum {
        SKILL_ATTR_TYPE_NORMAL = 1,
        SKILL_ATTR_TYPE_MELEE,
        SKILL_ATTR_TYPE_RANGE,
        SKILL_ATTR_TYPE_MAGIC
    };

    enum {
        SKILL_NORMAL,
        SKILL_MASTER,
        SKILL_GRAND_MASTER,
        SKILL_PERFECT_MASTER,
    };

    enum premium_types {
        PREMIUM_EXP,
        PREMIUM_ITEM,
        PREMIUM_SAFEBOX,
        PREMIUM_AUTOLOOT,
        PREMIUM_FISH_MIND,
        PREMIUM_MARRIAGE_FAST,
        PREMIUM_GOLD,
        PREMIUM_MAX_NUM = 9
    };

    enum special_effect {
        SE_NONE,

        SE_HPUP_RED,
        SE_SPUP_BLUE,
        SE_SPEEDUP_GREEN,
        SE_DXUP_PURPLE,
        SE_CRITICAL,
        SE_PENETRATE,
        SE_BLOCK,
        SE_DODGE,
        SE_CHINA_FIREWORK,
        SE_SPIN_TOP,
        SE_SUCCESS,
        SE_FAIL,
        SE_FR_SUCCESS,
        SE_LEVELUP_ON_14_FOR_GERMANY,
        SE_LEVELUP_UNDER_15_FOR_GERMANY,
        SE_PERCENT_DAMAGE1,
        SE_PERCENT_DAMAGE2,
        SE_PERCENT_DAMAGE3,

        SE_AUTO_HPUP,
        SE_AUTO_SPUP,

        SE_EQUIP_RAMADAN_RING,
        SE_EQUIP_HALLOWEEN_CANDY,
        SE_EQUIP_HAPPINESS_RING,
        SE_EQUIP_LOVE_PENDANT,
    };
}
