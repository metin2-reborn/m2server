// length.h includes this file once the constants used here exist; going through it first
// makes a direct include work as well.
#include "length.h" // IWYU pragma: export

#ifndef __INC_METIN2_ITEM_LENGTH_H__
#define __INC_METIN2_ITEM_LENGTH_H__

#include <cstdint>

inline constexpr int ITEM_NAME_MAX_LEN = 24;
inline constexpr int PROTO_NAME_MAX_LEN = 63;	// item/mob prototype names (UTF-8: Korean takes 3 bytes a character)
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
// Wow, is this insane?
// What are you going to do when sockets get extended later -_-;;;
inline constexpr int ITEM_SOCKET_UNIQUE_SAVE_TIME = ITEM_SOCKET_MAX_NUM - 2;
inline constexpr int ITEM_SOCKET_UNIQUE_REMAIN_TIME = ITEM_SOCKET_MAX_NUM - 1;

enum EItemTypes
{
    ITEM_NONE,              //0
    ITEM_WEAPON,            //1//Weapon
    ITEM_ARMOR,             //2//Armor
    ITEM_USE,               //3//Usable item
    ITEM_AUTOUSE,           //4
    ITEM_MATERIAL,          //5
    ITEM_SPECIAL,           //6 //Special item
    ITEM_TOOL,              //7
    ITEM_LOTTERY,           //8//Lottery
    ITEM_ELK,               //9//Money
    ITEM_METIN,             //10
    ITEM_CONTAINER,         //11
    ITEM_FISH,              //12//Fishing
    ITEM_ROD,               //13
    ITEM_RESOURCE,          //14
    ITEM_CAMPFIRE,          //15
    ITEM_UNIQUE,            //16
    ITEM_SKILLBOOK,         //17
    ITEM_QUEST,             //18
    ITEM_POLYMORPH,         //19
    ITEM_TREASURE_BOX,      //20//Treasure chest
    ITEM_TREASURE_KEY,      //21//Treasure chest key
    ITEM_SKILLFORGET,       //22
    ITEM_GIFTBOX,           //23
    ITEM_PICK,              //24
    ITEM_HAIR,              //25//Hair
    ITEM_TOTEM,             //26//Totem
	ITEM_BLEND,				//27//Potion that gets random attributes on creation
	ITEM_COSTUME,			//28//Costume item (for the costume system added in August 2011)
	ITEM_DS,				//29 //Dragon soul stone
	ITEM_SPECIAL_DS,		//30 // Special dragon soul stone (think of it as a UNIQUE item worn in a DS_SLOT)
	ITEM_EXTRACT,			//31 Extraction tool.
	ITEM_SECONDARY_COIN,	//32 ?? Myeongdojeon coin??
	ITEM_RING,				//33 Ring
	ITEM_BELT,				//34 Belt
};

enum EMetinSubTypes
{
	METIN_NORMAL,
	METIN_GOLD,
};

enum EWeaponSubTypes
{
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

enum EArmorSubTypes
{
	ARMOR_BODY,
	ARMOR_HEAD,
	ARMOR_SHIELD,
	ARMOR_WRIST,
	ARMOR_FOOTS,
	ARMOR_NECK,
	ARMOR_EAR,
	ARMOR_NUM_TYPES
};

enum ECostumeSubTypes
{
	COSTUME_BODY = ARMOR_BODY,			// [IMPORTANT!!] ECostumeSubTypes enum values must match the corresponding EArmorSubTypes values.
	COSTUME_HAIR = ARMOR_HEAD,			// This is to reuse the existing logic, per the business team's request to add extra attributes to costume items.
	COSTUME_NUM_TYPES,
};

enum EDragonSoulSubType
{
	DS_SLOT1,
	DS_SLOT2,
	DS_SLOT3,
	DS_SLOT4,
	DS_SLOT5,
	DS_SLOT6,
	DS_SLOT_MAX,
};

enum EDragonSoulGradeTypes
{
	DRAGON_SOUL_GRADE_NORMAL,
	DRAGON_SOUL_GRADE_BRILLIANT,
	DRAGON_SOUL_GRADE_RARE,
	DRAGON_SOUL_GRADE_ANCIENT,
	DRAGON_SOUL_GRADE_LEGENDARY,
	DRAGON_SOUL_GRADE_MAX,

};

enum EDragonSoulStepTypes
{
	DRAGON_SOUL_STEP_LOWEST,
	DRAGON_SOUL_STEP_LOW,
	DRAGON_SOUL_STEP_MID,
	DRAGON_SOUL_STEP_HIGH,
	DRAGON_SOUL_STEP_HIGHEST,
	DRAGON_SOUL_STEP_MAX,
};
inline constexpr int DRAGON_SOUL_STRENGTH_MAX = 7;

inline constexpr int DRAGON_SOUL_INVENTORY_MAX_NUM = static_cast<int>(DS_SLOT_MAX) * static_cast<int>(DRAGON_SOUL_GRADE_MAX) * DRAGON_SOUL_BOX_SIZE;

enum EFishSubTypes
{
	FISH_ALIVE,
	FISH_DEAD,
};

enum EResourceSubTypes
{
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

enum EUniqueSubTypes
{
	UNIQUE_NONE,
	UNIQUE_BOOK,
	UNIQUE_SPECIAL_RIDE,
	UNIQUE_SPECIAL_MOUNT_RIDE,
};

enum EUseSubTypes
{
	USE_POTION,					// 0
	USE_TALISMAN,
	USE_TUNING,
	USE_MOVE,
	USE_TREASURE_BOX,
	USE_MONEYBAG,
	USE_BAIT,
	USE_ABILITY_UP,
	USE_AFFECT,
	USE_CREATE_STONE,
	USE_SPECIAL,				// 10
	USE_POTION_NODELAY,
	USE_CLEAR,
	USE_INVISIBILITY,
	USE_DETACHMENT,
	USE_BUCKET,
	USE_POTION_CONTINUE,
	USE_CLEAN_SOCKET,
	USE_CHANGE_ATTRIBUTE,
	USE_ADD_ATTRIBUTE,
	USE_ADD_ACCESSORY_SOCKET,	// 20
	USE_PUT_INTO_ACCESSORY_SOCKET,
	USE_ADD_ATTRIBUTE2,
	USE_RECIPE,
	USE_CHANGE_ATTRIBUTE2,
	USE_BIND,
	USE_UNBIND,
	USE_TIME_CHARGE_PER,
	USE_TIME_CHARGE_FIX,				// 28
	USE_PUT_INTO_BELT_SOCKET,			// 29 Item usable in the belt socket 
	USE_PUT_INTO_RING_SOCKET,			// 30 Item usable in the ring socket (not unique rings; the newly added ring slot)
};

enum EExtractSubTypes
{
	EXTRACT_DRAGON_SOUL,
	EXTRACT_DRAGON_HEART,
};

enum EAutoUseSubTypes
{
	AUTOUSE_POTION,
	AUTOUSE_ABILITY_UP,
	AUTOUSE_BOMB,
	AUTOUSE_GOLD,
	AUTOUSE_MONEYBAG,
	AUTOUSE_TREASURE_BOX
};

enum EMaterialSubTypes
{
	MATERIAL_LEATHER,
	MATERIAL_BLOOD,
	MATERIAL_ROOT,
	MATERIAL_NEEDLE,
	MATERIAL_JEWEL,
	MATERIAL_DS_REFINE_NORMAL, 
	MATERIAL_DS_REFINE_BLESSED, 
	MATERIAL_DS_REFINE_HOLLY,
};

enum ESpecialSubTypes
{
	SPECIAL_MAP,
	SPECIAL_KEY,
	SPECIAL_DOC,
	SPECIAL_SPIRIT,
};

enum EToolSubTypes
{
	TOOL_FISHING_ROD
};

enum ELotterySubTypes
{
	LOTTERY_TICKET,
	LOTTERY_INSTANT
};

enum EItemFlag
{
	ITEM_FLAG_REFINEABLE		= (1 << 0),
	ITEM_FLAG_SAVE			= (1 << 1),
	ITEM_FLAG_STACKABLE		= (1 << 2),	// Stackable
	ITEM_FLAG_COUNT_PER_1GOLD	= (1 << 3),
	ITEM_FLAG_SLOW_QUERY		= (1 << 4),
	ITEM_FLAG_UNUSED01		= (1 << 5),	// UNUSED
	ITEM_FLAG_UNIQUE		= (1 << 6),
	ITEM_FLAG_MAKECOUNT		= (1 << 7),
	ITEM_FLAG_IRREMOVABLE		= (1 << 8),
	ITEM_FLAG_CONFIRM_WHEN_USE	= (1 << 9),
	ITEM_FLAG_QUEST_USE		= (1 << 10),
	ITEM_FLAG_QUEST_USE_MULTIPLE	= (1 << 11),
	ITEM_FLAG_QUEST_GIVE		= (1 << 12),
	ITEM_FLAG_LOG			= (1 << 13),
	ITEM_FLAG_APPLICABLE		= (1 << 14),
};

enum EItemAntiFlag
{
	ITEM_ANTIFLAG_FEMALE	= (1 << 0), // Not usable by females
	ITEM_ANTIFLAG_MALE		= (1 << 1), // Not usable by males
	ITEM_ANTIFLAG_WARRIOR	= (1 << 2), // Not usable by warriors
	ITEM_ANTIFLAG_ASSASSIN	= (1 << 3), // Not usable by assassins
	ITEM_ANTIFLAG_SURA		= (1 << 4), // Not usable by sura 
	ITEM_ANTIFLAG_SHAMAN	= (1 << 5), // Not usable by shamans
	ITEM_ANTIFLAG_GET		= (1 << 6), // Cannot be picked up
	ITEM_ANTIFLAG_DROP		= (1 << 7), // Cannot be dropped
	ITEM_ANTIFLAG_SELL		= (1 << 8), // Cannot be sold
	ITEM_ANTIFLAG_EMPIRE_A	= (1 << 9), // Not usable by empire A
	ITEM_ANTIFLAG_EMPIRE_B	= (1 << 10), // Not usable by empire B
	ITEM_ANTIFLAG_EMPIRE_C	= (1 << 11), // Not usable by empire C
	ITEM_ANTIFLAG_SAVE		= (1 << 12), // Not saved
	ITEM_ANTIFLAG_GIVE		= (1 << 13), // Not tradable
	ITEM_ANTIFLAG_PKDROP	= (1 << 14), // Not dropped on PK
	ITEM_ANTIFLAG_STACK		= (1 << 15), // Not stackable
	ITEM_ANTIFLAG_MYSHOP	= (1 << 16), // Cannot be listed in a private shop
	ITEM_ANTIFLAG_SAFEBOX	= (1 << 17), // Cannot be put in the safebox
};

enum EItemWearableFlag
{
	WEARABLE_BODY	= (1 << 0),
	WEARABLE_HEAD	= (1 << 1),
	WEARABLE_FOOTS	= (1 << 2),
	WEARABLE_WRIST	= (1 << 3),
	WEARABLE_WEAPON	= (1 << 4),
	WEARABLE_NECK	= (1 << 5),
	WEARABLE_EAR	= (1 << 6),
	WEARABLE_UNIQUE	= (1 << 7),
	WEARABLE_SHIELD	= (1 << 8),
	WEARABLE_ARROW	= (1 << 9),
	WEARABLE_HAIR	= (1 << 10),
	WEARABLE_ABILITY		= (1 << 11),
	WEARABLE_COSTUME_BODY	= (1 << 12),
};

enum ELimitTypes
{
	LIMIT_NONE,

	LIMIT_LEVEL,
	LIMIT_STR,
	LIMIT_DEX,
	LIMIT_INT,
	LIMIT_CON,
	LIMIT_PCBANG,

	/// Time is deducted in real time regardless of whether it's equipped (expiry time stored in socket0: unix_timestamp)
	LIMIT_REAL_TIME,						

	/// Realtime timer starts from the moment the item is first used (or equipped) 
	/// Before first use, socket0 holds the usable time (in seconds; 0 means use the proto's limit value), 
	/// and on use the use count is stored in socket1 and a unix_timestamp expiry time in socket0.
	LIMIT_REAL_TIME_START_FIRST_USE,

	/// Item whose time is deducted only while equipped
	/// socket0 holds the remaining time in seconds. (On first use, if it is 0, the proto's limit value is copied into socket0)
	LIMIT_TIMER_BASED_ON_WEAR,

	LIMIT_MAX_NUM
};

enum ERefineType
{
	REFINE_TYPE_NORMAL,
	REFINE_TYPE_NOT_USED1,
	REFINE_TYPE_SCROLL,
	REFINE_TYPE_HYUNIRON,
	REFINE_TYPE_MONEY_ONLY,
	REFINE_TYPE_MUSIN,
	REFINE_TYPE_BDRAGON,
};

#endif
