#pragma once

#include <cstdint>

namespace metin2::inline proto {
    inline constexpr int ITEM_NAME_MAX_LEN = 24;
    inline constexpr int PROTO_NAME_MAX_LEN = 63;
    inline constexpr int ITEM_VALUES_MAX_NUM = 6;
    inline constexpr int ITEM_SMALL_DESCR_MAX_LEN = 256;
    inline constexpr int ITEM_LIMIT_MAX_NUM = 2;
    inline constexpr int ITEM_APPLY_MAX_NUM = 3;
    inline constexpr int ITEM_SOCKET_MAX_NUM = 3;
    inline constexpr int ITEM_MAX_COUNT = 200;
    inline constexpr int ITEM_ATTRIBUTE_MAX_NUM = 7;
    inline constexpr int ITEM_ATTRIBUTE_MAX_LEVEL = 5;
    inline constexpr int ITEM_AWARD_WHY_MAX_LEN = 50;

    inline constexpr int REFINE_MATERIAL_MAX_NUM = 5;

    inline constexpr int ITEM_ELK_VNUM = 50026;

    inline constexpr uint8_t ITEM_SOCKET_REMAIN_SEC = 0;
    inline constexpr int ITEM_VALUE_DRAGON_SOUL_POLL_OUT_BONUS_IDX = 0;
    inline constexpr int ITEM_VALUE_CHARGING_AMOUNT_IDX = 0;
    inline constexpr int ITEM_VALUE_SECONDARY_COIN_UNIT_IDX = 0;
    inline constexpr int ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX = 2;
    inline constexpr int ITEM_SOCKET_CHARGING_AMOUNT_IDX = 2;

    inline constexpr int ITEM_SOCKET_UNIQUE_SAVE_TIME = ITEM_SOCKET_MAX_NUM - 2;
    inline constexpr int ITEM_SOCKET_UNIQUE_REMAIN_TIME = ITEM_SOCKET_MAX_NUM - 1;

    enum item_types {
        ITEM_NONE,
        ITEM_WEAPON,
        ITEM_ARMOR,
        ITEM_USE,
        ITEM_AUTOUSE,
        ITEM_MATERIAL,
        ITEM_SPECIAL,
        ITEM_TOOL,
        ITEM_LOTTERY,
        ITEM_ELK,
        ITEM_METIN,
        ITEM_CONTAINER,
        ITEM_FISH,
        ITEM_ROD,
        ITEM_RESOURCE,
        ITEM_CAMPFIRE,
        ITEM_UNIQUE,
        ITEM_SKILLBOOK,
        ITEM_QUEST,
        ITEM_POLYMORPH,
        ITEM_TREASURE_BOX,
        ITEM_TREASURE_KEY,
        ITEM_SKILLFORGET,
        ITEM_GIFTBOX,
        ITEM_PICK,
        ITEM_HAIR,
        ITEM_TOTEM,
        ITEM_BLEND,
        ITEM_COSTUME,
        ITEM_DS,
        ITEM_SPECIAL_DS,
        ITEM_EXTRACT,
        ITEM_SECONDARY_COIN,
        ITEM_RING,
        ITEM_BELT,
    };

    enum metin_sub_types {
        METIN_NORMAL,
        METIN_GOLD,
    };

    enum weapon_sub_types {
        WEAPON_SWORD,
        WEAPON_DAGGER,
        WEAPON_BOW,
        WEAPON_TWO_HANDED,
        WEAPON_BELL,
        WEAPON_FAN,
        WEAPON_ARROW,
        WEAPON_MOUNT_SPEAR,
        WEAPON_NUM_TYPES,
    };

    enum armor_sub_types {
        ARMOR_BODY,
        ARMOR_HEAD,
        ARMOR_SHIELD,
        ARMOR_WRIST,
        ARMOR_FOOTS,
        ARMOR_NECK,
        ARMOR_EAR,
        ARMOR_NUM_TYPES
    };

    enum costume_sub_types {
        COSTUME_BODY = ARMOR_BODY, // Must match the corresponding armor subtype values.
        COSTUME_HAIR = ARMOR_HEAD,
        COSTUME_NUM_TYPES,
    };

    enum dragon_soul_sub_type {
        DS_SLOT1,
        DS_SLOT2,
        DS_SLOT3,
        DS_SLOT4,
        DS_SLOT5,
        DS_SLOT6,
        DS_SLOT_MAX,
    };

    enum dragon_soul_grade_types {
        DRAGON_SOUL_GRADE_NORMAL,
        DRAGON_SOUL_GRADE_BRILLIANT,
        DRAGON_SOUL_GRADE_RARE,
        DRAGON_SOUL_GRADE_ANCIENT,
        DRAGON_SOUL_GRADE_LEGENDARY,
        DRAGON_SOUL_GRADE_MAX,

    };

    enum dragon_soul_step_types {
        DRAGON_SOUL_STEP_LOWEST,
        DRAGON_SOUL_STEP_LOW,
        DRAGON_SOUL_STEP_MID,
        DRAGON_SOUL_STEP_HIGH,
        DRAGON_SOUL_STEP_HIGHEST,
        DRAGON_SOUL_STEP_MAX,
    };
    inline constexpr int DRAGON_SOUL_STRENGTH_MAX = 7;

    inline constexpr int DRAGON_SOUL_INVENTORY_MAX_NUM = static_cast<int>(DS_SLOT_MAX) * static_cast<int>(DRAGON_SOUL_GRADE_MAX) * DRAGON_SOUL_BOX_SIZE;

    enum fish_sub_types {
        FISH_ALIVE,
        FISH_DEAD,
    };

    enum resource_sub_types {
        RESOURCE_FISHBONE,
        RESOURCE_WATERSTONEPIECE,
        RESOURCE_WATERSTONE,
        RESOURCE_BLOOD_PEARL,
        RESOURCE_BLUE_PEARL,
        RESOURCE_WHITE_PEARL,
        RESOURCE_BUCKET,
        RESOURCE_CRYSTAL,
        RESOURCE_GEM,
        RESOURCE_STONE,
        RESOURCE_METIN,
        RESOURCE_ORE,
    };

    enum unique_sub_types {
        UNIQUE_NONE,
        UNIQUE_BOOK,
        UNIQUE_SPECIAL_RIDE,
        UNIQUE_SPECIAL_MOUNT_RIDE,
    };

    enum use_sub_types {
        USE_POTION,
        USE_TALISMAN,
        USE_TUNING,
        USE_MOVE,
        USE_TREASURE_BOX,
        USE_MONEYBAG,
        USE_BAIT,
        USE_ABILITY_UP,
        USE_AFFECT,
        USE_CREATE_STONE,
        USE_SPECIAL,
        USE_POTION_NODELAY,
        USE_CLEAR,
        USE_INVISIBILITY,
        USE_DETACHMENT,
        USE_BUCKET,
        USE_POTION_CONTINUE,
        USE_CLEAN_SOCKET,
        USE_CHANGE_ATTRIBUTE,
        USE_ADD_ATTRIBUTE,
        USE_ADD_ACCESSORY_SOCKET,
        USE_PUT_INTO_ACCESSORY_SOCKET,
        USE_ADD_ATTRIBUTE2,
        USE_RECIPE,
        USE_CHANGE_ATTRIBUTE2,
        USE_BIND,
        USE_UNBIND,
        USE_TIME_CHARGE_PER,
        USE_TIME_CHARGE_FIX,
        USE_PUT_INTO_BELT_SOCKET,
        USE_PUT_INTO_RING_SOCKET,
    };

    enum extract_sub_types {
        EXTRACT_DRAGON_SOUL,
        EXTRACT_DRAGON_HEART,
    };

    enum auto_use_sub_types {
        AUTOUSE_POTION,
        AUTOUSE_ABILITY_UP,
        AUTOUSE_BOMB,
        AUTOUSE_GOLD,
        AUTOUSE_MONEYBAG,
        AUTOUSE_TREASURE_BOX
    };

    enum material_sub_types {
        MATERIAL_LEATHER,
        MATERIAL_BLOOD,
        MATERIAL_ROOT,
        MATERIAL_NEEDLE,
        MATERIAL_JEWEL,
        MATERIAL_DS_REFINE_NORMAL,
        MATERIAL_DS_REFINE_BLESSED,
        MATERIAL_DS_REFINE_HOLLY,
    };

    enum special_sub_types {
        SPECIAL_MAP,
        SPECIAL_KEY,
        SPECIAL_DOC,
        SPECIAL_SPIRIT,
    };

    enum tool_sub_types {
        TOOL_FISHING_ROD
    };

    enum lottery_sub_types {
        LOTTERY_TICKET,
        LOTTERY_INSTANT
    };

    enum item_flag {
        ITEM_FLAG_REFINEABLE = (1 << 0),
        ITEM_FLAG_SAVE = (1 << 1),
        ITEM_FLAG_STACKABLE = (1 << 2),
        ITEM_FLAG_COUNT_PER_1GOLD = (1 << 3),
        ITEM_FLAG_SLOW_QUERY = (1 << 4),
        ITEM_FLAG_UNUSED01 = (1 << 5),
        ITEM_FLAG_UNIQUE = (1 << 6),
        ITEM_FLAG_MAKECOUNT = (1 << 7),
        ITEM_FLAG_IRREMOVABLE = (1 << 8),
        ITEM_FLAG_CONFIRM_WHEN_USE = (1 << 9),
        ITEM_FLAG_QUEST_USE = (1 << 10),
        ITEM_FLAG_QUEST_USE_MULTIPLE = (1 << 11),
        ITEM_FLAG_QUEST_GIVE = (1 << 12),
        ITEM_FLAG_LOG = (1 << 13),
        ITEM_FLAG_APPLICABLE = (1 << 14),
    };

    enum item_anti_flag {
        ITEM_ANTIFLAG_FEMALE = (1 << 0),
        ITEM_ANTIFLAG_MALE = (1 << 1),
        ITEM_ANTIFLAG_WARRIOR = (1 << 2),
        ITEM_ANTIFLAG_ASSASSIN = (1 << 3),
        ITEM_ANTIFLAG_SURA = (1 << 4),
        ITEM_ANTIFLAG_SHAMAN = (1 << 5),
        ITEM_ANTIFLAG_GET = (1 << 6),
        ITEM_ANTIFLAG_DROP = (1 << 7),
        ITEM_ANTIFLAG_SELL = (1 << 8),
        ITEM_ANTIFLAG_EMPIRE_A = (1 << 9),
        ITEM_ANTIFLAG_EMPIRE_B = (1 << 10),
        ITEM_ANTIFLAG_EMPIRE_C = (1 << 11),
        ITEM_ANTIFLAG_SAVE = (1 << 12),
        ITEM_ANTIFLAG_GIVE = (1 << 13),
        ITEM_ANTIFLAG_PKDROP = (1 << 14),
        ITEM_ANTIFLAG_STACK = (1 << 15),
        ITEM_ANTIFLAG_MYSHOP = (1 << 16),
        ITEM_ANTIFLAG_SAFEBOX = (1 << 17),
    };

    enum item_wearable_flag {
        WEARABLE_BODY = (1 << 0),
        WEARABLE_HEAD = (1 << 1),
        WEARABLE_FOOTS = (1 << 2),
        WEARABLE_WRIST = (1 << 3),
        WEARABLE_WEAPON = (1 << 4),
        WEARABLE_NECK = (1 << 5),
        WEARABLE_EAR = (1 << 6),
        WEARABLE_UNIQUE = (1 << 7),
        WEARABLE_SHIELD = (1 << 8),
        WEARABLE_ARROW = (1 << 9),
        WEARABLE_HAIR = (1 << 10),
        WEARABLE_ABILITY = (1 << 11),
        WEARABLE_COSTUME_BODY = (1 << 12),
    };

    enum limit_types {
        LIMIT_NONE,

        LIMIT_LEVEL,
        LIMIT_STR,
        LIMIT_DEX,
        LIMIT_INT,
        LIMIT_CON,
        LIMIT_PCBANG,

        LIMIT_REAL_TIME,

        LIMIT_REAL_TIME_START_FIRST_USE,

        LIMIT_TIMER_BASED_ON_WEAR,

        LIMIT_MAX_NUM
    };

    enum refine_type {
        REFINE_TYPE_NORMAL,
        REFINE_TYPE_NOT_USED1,
        REFINE_TYPE_SCROLL,
        REFINE_TYPE_HYUNIRON,
        REFINE_TYPE_MONEY_ONLY,
        REFINE_TYPE_MUSIN,
        REFINE_TYPE_BDRAGON,
    };
}
