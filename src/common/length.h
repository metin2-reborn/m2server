#pragma once

#include <cstdint>

inline constexpr int WORD_MAX = 0xffff;
inline constexpr int MAX_HOST_LENGTH = 15;
inline constexpr int IP_ADDRESS_LENGTH = 15;
inline constexpr int LOGIN_MAX_LEN = 30;
inline constexpr int PASSWD_MAX_LEN = 16;
inline constexpr int PLAYER_PER_ACCOUNT = 4;
inline constexpr int ACCOUNT_STATUS_MAX_LEN = 8;
inline constexpr int CHARACTER_NAME_MAX_LEN = 24;
inline constexpr int SHOP_SIGN_MAX_LEN = 32;
inline constexpr int INVENTORY_MAX_NUM = 90;
inline constexpr int ABILITY_MAX_NUM = 50;
inline constexpr int EMPIRE_MAX_NUM = 4;
inline constexpr int BANWORD_MAX_LEN = 24;
inline constexpr int SMS_MAX_LEN = 80;
inline constexpr int MOBILE_MAX_LEN = 32;
inline constexpr int SOCIAL_ID_MAX_LEN = 18;

inline constexpr int GUILD_NAME_MAX_LEN = 12;

inline constexpr int SHOP_HOST_ITEM_MAX_NUM = 40;  // Max item count for the host
inline constexpr int SHOP_GUEST_ITEM_MAX_NUM = 18; // Max item count for the guest

inline constexpr int SHOP_PRICELIST_MAX_NUM = 40; ///< Max number of price entries to keep in the private shop price list

inline constexpr int CHAT_MAX_LEN = 512;

inline constexpr int QUICKSLOT_MAX_NUM = 36;

inline constexpr int JOURNAL_MAX_NUM = 2;

inline constexpr int QUERY_MAX_LEN = 8192;

inline constexpr int FILE_MAX_LEN = 128;

inline constexpr int PLAYER_EXP_TABLE_MAX = 120;
inline constexpr int PLAYER_MAX_LEVEL_CONST = 120;

inline constexpr int GUILD_MAX_LEVEL = 20;
inline constexpr int MOB_MAX_LEVEL = 100;

inline constexpr int ATTRIBUTE_MAX_VALUE = 20;
inline constexpr int CHARACTER_PATH_MAX_NUM = 64;
inline constexpr int SKILL_MAX_NUM = 255;
inline constexpr int SKILLBOOK_DELAY_MIN = 64800;
inline constexpr int SKILLBOOK_DELAY_MAX = 108000;
inline constexpr int SKILL_MAX_LEVEL = 40;

inline constexpr int APPLY_NAME_MAX_LEN = 32;
inline constexpr int EVENT_FLAG_NAME_MAX_LEN = 32;

inline constexpr int MOB_SKILL_MAX_NUM = 5;

inline constexpr int POINT_MAX_NUM = 255;
inline constexpr int DRAGON_SOUL_BOX_SIZE = 32;
inline constexpr int DRAGON_SOUL_BOX_COLUMN_NUM = 8;
inline constexpr int DRAGON_SOUL_BOX_ROW_NUM = DRAGON_SOUL_BOX_SIZE / DRAGON_SOUL_BOX_COLUMN_NUM;
inline constexpr int DRAGON_SOUL_REFINE_GRID_SIZE = 15;
inline constexpr int MAX_AMOUNT_OF_MALL_BONUS = 20;

inline constexpr int WEAR_MAX_NUM = 32;

inline constexpr int GOLD_MAX = 2000000000;

inline constexpr int OPENID_AUTHKEY_LEN = 32;

inline constexpr int SHOP_TAB_NAME_MAX = 32;
inline constexpr int SHOP_TAB_COUNT_MAX = 3;

inline constexpr int BELT_INVENTORY_SLOT_WIDTH = 4;
inline constexpr int BELT_INVENTORY_SLOT_HEIGHT = 4;

inline constexpr int BELT_INVENTORY_SLOT_COUNT = BELT_INVENTORY_SLOT_WIDTH * BELT_INVENTORY_SLOT_HEIGHT;

/**
**** Item areas allocated so far (Item Position in DB) ****
+------------------------------------------------------+ 0
| Character base inventory (45 cells * 2 pages) 90     |
+------------------------------------------------------+ 90 = INVENTORY_MAX_NUM(90)
| Character equipment window (worn items) 32           |
+------------------------------------------------------+ 122 = INVENTORY_MAX_NUM(90) + WEAR_MAX_NUM(32)
| Dragon soul equipment window (worn DS) 12            |
+------------------------------------------------------+ 134 = 122 + DS_SLOT_MAX(6) * DRAGON_SOUL_DECK_MAX_NUM(2)
| Dragon soul equipment reserved (unused yet) 18       |
+------------------------------------------------------+ 152 = 134 + DS_SLOT_MAX(6) * DRAGON_SOUL_DECK_RESERVED_MAX_NUM(3)
| Belt inventory (active by belt level if belt worn)   |
+------------------------------------------------------+ 168 = 152 + BELT_INVENTORY_SLOT_COUNT(16) = INVENTORY_AND_EQUIP_CELL_MAX
| Unused                                               |
+------------------------------------------------------+ ??
*/

inline constexpr int MATRIX_CODE_MAX_LEN = 192;
inline constexpr int MATRIX_ANSWER_MAX_LEN = 8;

enum EWearPositions {
    WEAR_BODY,         // 0
    WEAR_HEAD,         // 1
    WEAR_FOOTS,        // 2
    WEAR_WRIST,        // 3
    WEAR_WEAPON,       // 4
    WEAR_NECK,         // 5
    WEAR_EAR,          // 6
    WEAR_UNIQUE1,      // 7
    WEAR_UNIQUE2,      // 8
    WEAR_ARROW,        // 9
    WEAR_SHIELD,       // 10
    WEAR_ABILITY1,     // 11
    WEAR_ABILITY2,     // 12
    WEAR_ABILITY3,     // 13
    WEAR_ABILITY4,     // 14
    WEAR_ABILITY5,     // 15
    WEAR_ABILITY6,     // 16
    WEAR_ABILITY7,     // 17
    WEAR_ABILITY8,     // 18
    WEAR_COSTUME_BODY, // 19
    WEAR_COSTUME_HAIR, // 20

    WEAR_RING1, // 21	: new ring slot 1 (left)
    WEAR_RING2, // 22	: new ring slot 2 (right)

    WEAR_BELT, // 23	: new belt slot

    WEAR_MAX = 32 //
};

enum EDragonSoulDeckType {
    DRAGON_SOUL_DECK_0,
    DRAGON_SOUL_DECK_1,
    DRAGON_SOUL_DECK_MAX_NUM = 2,

    DRAGON_SOUL_DECK_RESERVED_MAX_NUM = 3, // NOTE: Important! Not used yet, but 3 pages' worth are reserved. If you increase the DS DECK count, you must subtract the same amount from RESERVED!
};

enum ESex {
    SEX_MALE,
    SEX_FEMALE
};

enum EJobs {
    JOB_WARRIOR,
    JOB_ASSASSIN,
    JOB_SURA,
    JOB_SHAMAN,
    JOB_MAX_NUM
};

inline constexpr int SKILL_GROUP_MAX_NUM = 2;

enum ERaceFlags {
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

enum EParts {
    PART_MAIN,
    PART_WEAPON,
    PART_HEAD,
    PART_HAIR,

    PART_MAX_NUM,
    PART_WEAPON_SUB,
};

enum EChatType {
    CHAT_TYPE_TALKING, /* Plain chat */
    CHAT_TYPE_INFO,    /* Info (picked up an item, gained experience, etc.) */
    CHAT_TYPE_NOTICE,  /* Notice */
    CHAT_TYPE_PARTY,   /* Party chat */
    CHAT_TYPE_GUILD,   /* Guild chat */
    CHAT_TYPE_COMMAND, /* Normal command */
    CHAT_TYPE_SHOUT,   /* Shout */
    CHAT_TYPE_WHISPER,
    CHAT_TYPE_BIG_NOTICE,
    CHAT_TYPE_MONARCH_NOTICE,
    CHAT_TYPE_MAX_NUM
};

enum EWhisperType {
    WHISPER_TYPE_NORMAL = 0,
    WHISPER_TYPE_NOT_EXIST = 1,
    WHISPER_TYPE_TARGET_BLOCKED = 2,
    WHISPER_TYPE_SENDER_BLOCKED = 3,
    WHISPER_TYPE_ERROR = 4,
    WHISPER_TYPE_GM = 5,
    WHISPER_TYPE_SYSTEM = 0xFF
};

enum ECharacterPosition {
    POSITION_GENERAL,
    POSITION_BATTLE,
    POSITION_DYING,
    POSITION_SITTING_CHAIR,
    POSITION_SITTING_GROUND,
    POSITION_INTRO,
    POSITION_MAX_NUM
};

enum EGMLevels {
    GM_PLAYER,
    GM_LOW_WIZARD,
    GM_WIZARD,
    GM_HIGH_WIZARD,
    GM_GOD,
    GM_IMPLEMENTOR
};

enum EMobRank {
    MOB_RANK_PAWN,
    MOB_RANK_S_PAWN,
    MOB_RANK_KNIGHT,
    MOB_RANK_S_KNIGHT,
    MOB_RANK_BOSS,
    MOB_RANK_KING,
    MOB_RANK_MAX_NUM
};

enum ECharType {
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

enum EBattleType {
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

enum EApplyTypes {
    APPLY_NONE,                    // 0
    APPLY_MAX_HP,                  // 1
    APPLY_MAX_SP,                  // 2
    APPLY_CON,                     // 3
    APPLY_INT,                     // 4
    APPLY_STR,                     // 5
    APPLY_DEX,                     // 6
    APPLY_ATT_SPEED,               // 7
    APPLY_MOV_SPEED,               // 8
    APPLY_CAST_SPEED,              // 9
    APPLY_HP_REGEN,                // 10
    APPLY_SP_REGEN,                // 11
    APPLY_POISON_PCT,              // 12
    APPLY_STUN_PCT,                // 13
    APPLY_SLOW_PCT,                // 14
    APPLY_CRITICAL_PCT,            // 15
    APPLY_PENETRATE_PCT,           // 16
    APPLY_ATTBONUS_HUMAN,          // 17
    APPLY_ATTBONUS_ANIMAL,         // 18
    APPLY_ATTBONUS_ORC,            // 19
    APPLY_ATTBONUS_MILGYO,         // 20
    APPLY_ATTBONUS_UNDEAD,         // 21
    APPLY_ATTBONUS_DEVIL,          // 22
    APPLY_STEAL_HP,                // 23
    APPLY_STEAL_SP,                // 24
    APPLY_MANA_BURN_PCT,           // 25
    APPLY_DAMAGE_SP_RECOVER,       // 26
    APPLY_BLOCK,                   // 27
    APPLY_DODGE,                   // 28
    APPLY_RESIST_SWORD,            // 29
    APPLY_RESIST_TWOHAND,          // 30
    APPLY_RESIST_DAGGER,           // 31
    APPLY_RESIST_BELL,             // 32
    APPLY_RESIST_FAN,              // 33
    APPLY_RESIST_BOW,              // 34
    APPLY_RESIST_FIRE,             // 35
    APPLY_RESIST_ELEC,             // 36
    APPLY_RESIST_MAGIC,            // 37
    APPLY_RESIST_WIND,             // 38
    APPLY_REFLECT_MELEE,           // 39
    APPLY_REFLECT_CURSE,           // 40
    APPLY_POISON_REDUCE,           // 41
    APPLY_KILL_SP_RECOVER,         // 42
    APPLY_EXP_DOUBLE_BONUS,        // 43
    APPLY_GOLD_DOUBLE_BONUS,       // 44
    APPLY_ITEM_DROP_BONUS,         // 45
    APPLY_POTION_BONUS,            // 46
    APPLY_KILL_HP_RECOVER,         // 47
    APPLY_IMMUNE_STUN,             // 48
    APPLY_IMMUNE_SLOW,             // 49
    APPLY_IMMUNE_FALL,             // 50
    APPLY_SKILL,                   // 51
    APPLY_BOW_DISTANCE,            // 52
    APPLY_ATT_GRADE_BONUS,         // 53
    APPLY_DEF_GRADE_BONUS,         // 54
    APPLY_MAGIC_ATT_GRADE,         // 55
    APPLY_MAGIC_DEF_GRADE,         // 56
    APPLY_CURSE_PCT,               // 57
    APPLY_MAX_STAMINA,             // 58
    APPLY_ATTBONUS_WARRIOR,        // 59
    APPLY_ATTBONUS_ASSASSIN,       // 60
    APPLY_ATTBONUS_SURA,           // 61
    APPLY_ATTBONUS_SHAMAN,         // 62
    APPLY_ATTBONUS_MONSTER,        // 63
    APPLY_MALL_ATTBONUS,           // 64 attack +x%
    APPLY_MALL_DEFBONUS,           // 65 defense +x%
    APPLY_MALL_EXPBONUS,           // 66 experience +x%
    APPLY_MALL_ITEMBONUS,          // 67 item drop rate x/10 times
    APPLY_MALL_GOLDBONUS,          // 68 gold drop rate x/10 times
    APPLY_MAX_HP_PCT,              // 69 max HP +x%
    APPLY_MAX_SP_PCT,              // 70 max SP +x%
    APPLY_SKILL_DAMAGE_BONUS,      // 71 skill damage * (100+x)%
    APPLY_NORMAL_HIT_DAMAGE_BONUS, // 72 normal attack damage * (100+x)%
    APPLY_SKILL_DEFEND_BONUS,      // 73 skill damage defense * (100-x)%
    APPLY_NORMAL_HIT_DEFEND_BONUS, // 74 normal attack damage defense * (100-x)%
    APPLY_PC_BANG_EXP_BONUS,       // 75 PC cafe item EXP bonus
    APPLY_PC_BANG_DROP_BONUS,      // 76 PC cafe item drop rate bonus

    APPLY_EXTRACT_HP_PCT, // 77 consumes HP on use

    APPLY_RESIST_WARRIOR,           // 78 resistance to warriors
    APPLY_RESIST_ASSASSIN,          // 79 resistance to assassins
    APPLY_RESIST_SURA,              // 80 resistance to suras
    APPLY_RESIST_SHAMAN,            // 81 resistance to shamans
    APPLY_ENERGY,                   // 82 energy
    APPLY_DEF_GRADE,                // 83 defense. DEF_GRADE_BONUS has an intended bug(...) where the client shows it doubled.
    APPLY_COSTUME_ATTR_BONUS,       // 84 attribute bonus attached to costume items
    APPLY_MAGIC_ATTBONUS_PER,       // 85 magic attack +x%
    APPLY_MELEE_MAGIC_ATTBONUS_PER, // 86 magic + melee attack +x%

    APPLY_RESIST_ICE,   // 87 ice resistance
    APPLY_RESIST_EARTH, // 88 earth resistance
    APPLY_RESIST_DARK,  // 89 dark resistance

    APPLY_ANTI_CRITICAL_PCT,  // 90 critical resistance
    APPLY_ANTI_PENETRATE_PCT, // 91 piercing hit resistance

    MAX_APPLY_NUM, //
};

enum EOnClickEvents {
    ON_CLICK_NONE,
    ON_CLICK_SHOP,
    ON_CLICK_TALK,
    ON_CLICK_MAX_NUM
};

enum EWindows {
    RESERVED_WINDOW,
    INVENTORY,
    EQUIPMENT,
    SAFEBOX,
    MALL,
    DRAGON_SOUL_INVENTORY,
    BELT_INVENTORY,
    GROUND
};

enum EMobSizes {
    MOBSIZE_RESERVED,
    MOBSIZE_SMALL,
    MOBSIZE_MEDIUM,
    MOBSIZE_BIG
};

enum EAIFlags {
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

enum EMobStatType {
    MOB_STATTYPE_POWER,
    MOB_STATTYPE_TANKER,
    MOB_STATTYPE_SUPER_POWER,
    MOB_STATTYPE_SUPER_TANKER,
    MOB_STATTYPE_RANGE,
    MOB_STATTYPE_MAGIC,
    MOB_STATTYPE_MAX_NUM
};

enum EImmuneFlags {
    IMMUNE_STUN = (1 << 0),
    IMMUNE_SLOW = (1 << 1),
    IMMUNE_FALL = (1 << 2),
    IMMUNE_CURSE = (1 << 3),
    IMMUNE_POISON = (1 << 4),
    IMMUNE_TERROR = (1 << 5),
    IMMUNE_REFLECT = (1 << 6),
};

enum EMobEnchants {
    MOB_ENCHANT_CURSE,
    MOB_ENCHANT_SLOW,
    MOB_ENCHANT_POISON,
    MOB_ENCHANT_STUN,
    MOB_ENCHANT_CRITICAL,
    MOB_ENCHANT_PENETRATE,
    MOB_ENCHANTS_MAX_NUM
};

enum EMobResists {
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

enum {
    SKILL_ATTR_TYPE_NORMAL = 1,
    SKILL_ATTR_TYPE_MELEE,
    SKILL_ATTR_TYPE_RANGE,
    SKILL_ATTR_TYPE_MAGIC
    /*
       SKILL_ATTR_TYPE_FIRE,
       SKILL_ATTR_TYPE_ICE,
       SKILL_ATTR_TYPE_ELEC,
       SKILL_ATTR_TYPE_DARK,
       */
};

enum {
    SKILL_NORMAL,
    SKILL_MASTER,
    SKILL_GRAND_MASTER,
    SKILL_PERFECT_MASTER,
};

enum EGuildWarType {
    GUILD_WAR_TYPE_FIELD,
    GUILD_WAR_TYPE_BATTLE,
    GUILD_WAR_TYPE_FLAG,
    GUILD_WAR_TYPE_MAX_NUM
};

enum EGuildWarState {
    GUILD_WAR_NONE,
    GUILD_WAR_SEND_DECLARE,
    GUILD_WAR_REFUSE,
    GUILD_WAR_RECV_DECLARE,
    GUILD_WAR_WAIT_START,
    GUILD_WAR_CANCEL,
    GUILD_WAR_ON_WAR,
    GUILD_WAR_END,
    GUILD_WAR_OVER,
    GUILD_WAR_RESERVE,

    GUILD_WAR_DURATION = 30 * 60, // 1 hour
    GUILD_WAR_WIN_POINT = 1000,
    GUILD_WAR_LADDER_HALF_PENALTY_TIME = 12 * 60 * 60,
};

enum EAttributeSet {
    ATTRIBUTE_SET_WEAPON,
    ATTRIBUTE_SET_BODY,
    ATTRIBUTE_SET_WRIST,
    ATTRIBUTE_SET_FOOTS,
    ATTRIBUTE_SET_NECK,
    ATTRIBUTE_SET_HEAD,
    ATTRIBUTE_SET_SHIELD,
    ATTRIBUTE_SET_EAR,
    ATTRIBUTE_SET_MAX_NUM
};

enum EPrivType {
    PRIV_NONE,
    PRIV_ITEM_DROP,
    PRIV_GOLD_DROP,
    PRIV_GOLD10_DROP,
    PRIV_EXP_PCT,
    MAX_PRIV_NUM,
};

enum EMoneyLogType {
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

enum EPremiumTypes {
    PREMIUM_EXP,           // experience x1.2
    PREMIUM_ITEM,          // item drop rate x2
    PREMIUM_SAFEBOX,       // safebox goes from 1 to 3 pages
    PREMIUM_AUTOLOOT,      // auto-pickup gold
    PREMIUM_FISH_MIND,     // higher chance to catch premium fish
    PREMIUM_MARRIAGE_FAST, // speeds up marriage love point gain.
    PREMIUM_GOLD,          // gold drop rate x1.5
    PREMIUM_MAX_NUM = 9
};

enum SPECIAL_EFFECT {
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

    SE_EQUIP_RAMADAN_RING,    // effect when equipping the Ramadan crescent ring (71135) (trigger effect, not a persistent one)
    SE_EQUIP_HALLOWEEN_CANDY, // effect triggered the moment Halloween candy is equipped (-_-;)
    SE_EQUIP_HAPPINESS_RING,  // effect when equipping the Christmas ring of happiness (71143) (trigger effect, not a persistent one)
    SE_EQUIP_LOVE_PENDANT,    // effect when equipping the Valentine pendant of love (71145) (trigger effect, not a persistent one)
};

#include "item_length.h"

// struct representing an inventory position
// The reason for the implicit conversion to/from int is that,
// all inventory functions took only a cell, not a window_type (previously there was only one inventory, so no inventory type was needed),
// and changing every call site of the inventory functions would be a pain.

inline constexpr int DRAGON_SOUL_REFINE_GRID_MAX = 15;

inline constexpr int DRAGON_SOUL_EQUIP_SLOT_START = INVENTORY_MAX_NUM + WEAR_MAX_NUM;
inline constexpr int DRAGON_SOUL_EQUIP_SLOT_END = DRAGON_SOUL_EQUIP_SLOT_START + (static_cast<int>(DS_SLOT_MAX) * static_cast<int>(DRAGON_SOUL_DECK_MAX_NUM));
inline constexpr int DRAGON_SOUL_EQUIP_RESERVED_SLOT_END = DRAGON_SOUL_EQUIP_SLOT_END + (static_cast<int>(DS_SLOT_MAX) * static_cast<int>(DRAGON_SOUL_DECK_RESERVED_MAX_NUM));

inline constexpr int BELT_INVENTORY_SLOT_START = DRAGON_SOUL_EQUIP_RESERVED_SLOT_END;
inline constexpr int BELT_INVENTORY_SLOT_END = BELT_INVENTORY_SLOT_START + BELT_INVENTORY_SLOT_COUNT;

inline constexpr int INVENTORY_AND_EQUIP_SLOT_MAX = BELT_INVENTORY_SLOT_END;

#pragma pack(push, 1)

using TItemPos = struct SItemPos {
    uint8_t window_type;
    uint16_t cell;
    SItemPos() {
        window_type = INVENTORY;
        cell = WORD_MAX;
    }

    SItemPos(uint8_t _window_type, uint16_t _cell) {
        window_type = _window_type;
        cell = _cell;
    }

    auto IsValidItemPosition() const -> bool {
        switch (window_type) {
        case RESERVED_WINDOW:
            return false;

        case INVENTORY:
        case EQUIPMENT:
        case BELT_INVENTORY:
            return cell < INVENTORY_AND_EQUIP_SLOT_MAX;

        case DRAGON_SOUL_INVENTORY:
            return cell < (DRAGON_SOUL_INVENTORY_MAX_NUM);

        // Windows whose size is determined dynamically cannot be validity-checked.
        case SAFEBOX:
        case MALL:
            return false;

        default:
            return false;
        }

        return false;
    }

    auto IsSamePosition(const SItemPos &other) const -> bool { // ItemMove dupe exploit fix
        return *this == other || ((INVENTORY == window_type || EQUIPMENT == window_type) && (INVENTORY == other.window_type || EQUIPMENT == other.window_type) && cell == other.cell);
    }

    auto IsEquipPosition() const -> bool {
        return ((INVENTORY == window_type || EQUIPMENT == window_type) && cell >= INVENTORY_MAX_NUM && cell < INVENTORY_MAX_NUM + WEAR_MAX_NUM) || IsDragonSoulEquipPosition();
    }

    auto IsDragonSoulEquipPosition() const -> bool {
        return (DRAGON_SOUL_EQUIP_SLOT_START <= cell) && (DRAGON_SOUL_EQUIP_SLOT_END > cell);
    }

    auto IsBeltInventoryPosition() const -> bool {
        return (BELT_INVENTORY_SLOT_START <= cell) && (BELT_INVENTORY_SLOT_END > cell);
    }

    auto IsDefaultInventoryPosition() const -> bool {
        return INVENTORY == window_type && cell < INVENTORY_MAX_NUM;
    }

    auto operator==(const struct SItemPos &rhs) const -> bool {
        return (window_type == rhs.window_type) && (cell == rhs.cell);
    }

    auto operator<(const struct SItemPos &rhs) const -> bool {
        return (window_type < rhs.window_type) || ((window_type == rhs.window_type) && (cell < rhs.cell));
    }
};

const TItemPos NPOS(RESERVED_WINDOW, WORD_MAX);

using EShopCoinType = enum {
    SHOP_COIN_TYPE_GOLD, // DEFAULT VALUE
    SHOP_COIN_TYPE_SECONDARY_COIN,
};

#pragma pack(pop)
