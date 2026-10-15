#pragma once

#include "typedef.h"

#include <cstdint>

namespace metin2::game {

class CAffect
{
	public:
		uint32_t	dwType;
		uint8_t    bApplyOn;
		int32_t    lApplyValue;
		uint32_t   dwFlag;
		int32_t	lDuration;
		int32_t	lSPCost;
};

enum EAffectTypes
{
	AFFECT_NONE,

	AFFECT_MOV_SPEED		= 200,
	AFFECT_ATT_SPEED,
	AFFECT_ATT_GRADE,
	AFFECT_INVISIBILITY,
	AFFECT_STR,
	AFFECT_DEX,			// 205
	AFFECT_CON,	
	AFFECT_INT,	
	AFFECT_FISH_MIND_PILL,

	AFFECT_POISON,
	AFFECT_STUN,		// 210
	AFFECT_SLOW,
	AFFECT_DUNGEON_READY,
	AFFECT_DUNGEON_UNIQUE,

	AFFECT_BUILDING,
	AFFECT_REVIVE_INVISIBLE,	// 215
	AFFECT_FIRE,
	AFFECT_CAST_SPEED,
	AFFECT_HP_RECOVER_CONTINUE,
	AFFECT_SP_RECOVER_CONTINUE,

	AFFECT_POLYMORPH,		// 220
	AFFECT_MOUNT,

	AFFECT_WAR_FLAG,		// 222

	AFFECT_BLOCK_CHAT,		// 223
	AFFECT_CHINA_FIREWORK,

	AFFECT_BOW_DISTANCE,	// 225
	AFFECT_DEF_GRADE,		// 226

	AFFECT_PREMIUM_START	= 500,
	AFFECT_EXP_BONUS		= 500,	// Ring of Experience
	AFFECT_ITEM_BONUS		= 501,	// Thief's Gloves
	AFFECT_SAFEBOX		= 502,  // PREMIUM_SAFEBOX,
	AFFECT_AUTOLOOT		= 503,	// PREMIUM_AUTOLOOT,
	AFFECT_FISH_MIND		= 504,	// PREMIUM_FISH_MIND,
	AFFECT_MARRIAGE_FAST	= 505,	// Mandarin Duck Feather
	AFFECT_GOLD_BONUS		= 506,	// 50% increased gold drop chance
	AFFECT_PREMIUM_END		= 509,

	AFFECT_MALL			= 510,	// Mall item effect
	AFFECT_NO_DEATH_PENALTY	= 511,	// Dragon God's Protection (blocks the experience penalty once)
	AFFECT_SKILL_BOOK_BONUS	= 512,	// Sage's Lesson (book training success chance +50%)
	AFFECT_SKILL_NO_BOOK_DELAY	= 513,	// Spell book

	AFFECT_HAIR	= 514,	// Hair effect
	AFFECT_COLLECT = 515, //Collection quest 
	AFFECT_EXP_BONUS_EURO_FREE = 516, // Ring of Experience (default effect at level 14 and below in the European version)
	AFFECT_EXP_BONUS_EURO_FREE_UNDER_15 = 517,
	AFFECT_UNIQUE_ABILITY = 518,

	AFFECT_CUBE_1,
	AFFECT_CUBE_2,
	AFFECT_CUBE_3,
	AFFECT_CUBE_4,
	AFFECT_CUBE_5,
	AFFECT_CUBE_6,
	AFFECT_CUBE_7,
	AFFECT_CUBE_8,
	AFFECT_CUBE_9,
	AFFECT_CUBE_10,
	AFFECT_CUBE_11,
	AFFECT_CUBE_12,

	AFFECT_BLEND,

	AFFECT_HORSE_NAME,
	AFFECT_MOUNT_BONUS,

	AFFECT_AUTO_HP_RECOVERY = 534,
	AFFECT_AUTO_SP_RECOVERY = 535,

	AFFECT_DRAGON_SOUL_QUALIFIED = 540,
	AFFECT_DRAGON_SOUL_DECK_0 = 541,
	AFFECT_DRAGON_SOUL_DECK_1 = 542,

	AFFECT_RAMADAN_ABILITY = 300,
	AFFECT_RAMADAN_RING	   = 301,

	AFFECT_NOG_ABILITY = 302,
	AFFECT_HOLLY_STONE_POWER = 303,

	AFFECT_QUEST_START_IDX = 1000
};

enum EAffectBits
{   
	AFF_NONE,

	AFF_YMIR,
	AFF_INVISIBILITY,
	AFF_SPAWN,

	AFF_POISON,
	AFF_SLOW,
	AFF_STUN,

	AFF_DUNGEON_READY,		// Ready state in dungeon
	AFF_DUNGEON_UNIQUE,		// Dungeon unique (not culled on the client)

	AFF_BUILDING_CONSTRUCTION_SMALL,
	AFF_BUILDING_CONSTRUCTION_LARGE,
	AFF_BUILDING_UPGRADE,

	AFF_MOV_SPEED_POTION,
	AFF_ATT_SPEED_POTION,

	AFF_FISH_MIND,

	AFF_JEONGWIHON,		// Spirit Strike
	AFF_GEOMGYEONG,		// Sword Aura
	AFF_CHEONGEUN,		// Heavy Weight
	AFF_GYEONGGONG,		// Light Step
	AFF_EUNHYUNG,		// Stealth
	AFF_GWIGUM,			// Enchanted Blade
	AFF_TERROR,			// Fear
	AFF_JUMAGAP,		// Enchanted Armor
	AFF_HOSIN,			// Blessing
	AFF_BOHO,			// Protection
	AFF_KWAESOK,		// Swiftness
	AFF_MANASHIELD,		// Mana shield
	AFF_MUYEONG,		// Shadowless affect
	AFF_REVIVE_INVISIBLE,	// Briefly invincible after revival
	AFF_FIRE,			// Continuous fire damage
	AFF_GICHEON,		// Strong Body
	AFF_JEUNGRYEOK,		// Strength boost
	AFF_TANHWAN_DASH,		// Running affect for Bullet Strike
	AFF_PABEOP,			// Dispel
	AFF_CHEONGEUN_WITH_FALL,	// Heavy Weight
	AFF_POLYMORPH,
	AFF_WAR_FLAG1,
	AFF_WAR_FLAG2,
	AFF_WAR_FLAG3,

	AFF_CHINA_FIREWORK,
	AFF_HAIR,	// Hair
	AFF_GERMANY, // Germany 

	AFF_BITS_MAX
};

extern void SendAffectAddPacket(DESC * d, CAffect * pkAff);

// AFFECT_DURATION_BUG_FIX
enum AffectVariable
{
	// Used when an Affect must last indefinitely.
	// Time keeps decreasing, so infinity is emulated with a very large value.
	//// 24 bits is too few, so use 25 bits.
	// ... what a comment, saying it uses 25 bits while actually using 29 bits...
	// The collect quest uses 60 years as infinite time, so use 60 years here too.

	INFINITE_AFFECT_DURATION = 60 * 365 * 24 * 60 * 60
};
// END_AFFECT_DURATION_BUG_FIX
}
