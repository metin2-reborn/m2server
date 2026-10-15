#pragma once

#include <metin2/common/singleton.h>
#include <metin2/proto/tables.h>
#include <metin2/poly/Poly.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>

namespace metin2::game::inline combat {

enum skill_flags
{
	SKILL_FLAG_ATTACK			= (1 << 0),	// Attack skill
	SKILL_FLAG_USE_MELEE_DAMAGE		= (1 << 1),	// Use base melee damage as the b value
	SKILL_FLAG_COMPUTE_ATTGRADE		= (1 << 2),	// Compute attack grade
	SKILL_FLAG_SELFONLY			= (1 << 3),	// Can only be used on self
	SKILL_FLAG_USE_MAGIC_DAMAGE		= (1 << 4),	// Use base magic damage as the b value
	SKILL_FLAG_USE_HP_AS_COST		= (1 << 5),	// Uses HP instead of SP
	SKILL_FLAG_COMPUTE_MAGIC_DAMAGE	= (1 << 6),
	SKILL_FLAG_SPLASH			= (1 << 7),
	SKILL_FLAG_GIVE_PENALTY		= (1 << 8),	// After use, takes double damage for a short while (3 seconds).
	SKILL_FLAG_USE_ARROW_DAMAGE		= (1 << 9),	// Use base arrow damage as the b value
	SKILL_FLAG_PENETRATE		= (1 << 10),	// Ignore defense
	SKILL_FLAG_IGNORE_TARGET_RATING	= (1 << 11),	// Ignore target rating
	SKILL_FLAG_SLOW			= (1 << 12),	// Slow attack
	SKILL_FLAG_STUN			= (1 << 13),	// Stun attack
	SKILL_FLAG_HP_ABSORB		= (1 << 14),	// HP absorb
	SKILL_FLAG_SP_ABSORB		= (1 << 15),	// SP absorb
	SKILL_FLAG_FIRE_CONT		= (1 << 16),	// FIRE damage over time
	SKILL_FLAG_REMOVE_BAD_AFFECT	= (1 << 17),	// Remove bad effects
	SKILL_FLAG_REMOVE_GOOD_AFFECT	= (1 << 18),	// Remove bad effects
	SKILL_FLAG_CRUSH			= (1 << 19),	// Knocks the target back
	SKILL_FLAG_POISON			= (1 << 20),	// Poison attack
	SKILL_FLAG_TOGGLE			= (1 << 21),	// Toggle
	SKILL_FLAG_DISABLE_BY_POINT_UP	= (1 << 22),	// Cannot be raised with points.
	SKILL_FLAG_CRUSH_LONG		= (1 << 23),	// Knocks the target far back
	SKILL_FLAG_WIND		= (1 << 24),	// Wind attribute 
	SKILL_FLAG_ELEC		= (1 << 25),	// Electric attribute
	SKILL_FLAG_FIRE		= (1 << 26),	// Fire attribute
};

enum
{
	SKILL_PENALTY_DURATION = 3,
	SKILL_TYPE_HORSE = 5,
};

enum skill_indexes
{
	SKILL_RESERVED = 0,

	// Warrior: body line
	// A
	SKILL_SAMYEON = 1,		// Three-Way Cut (triple slash)
	SKILL_PALBANG,		// Sword Spin
	// S
	SKILL_JEONGWI,		// Berserk
	SKILL_GEOMKYUNG,		// Aura of the Sword
	SKILL_TANHWAN,		// Dash

	// Warrior: mental line
	// A
	SKILL_GIGONGCHAM = 16,	// Spirit Strike
	SKILL_GYOKSAN,		// Bash
	SKILL_DAEJINGAK,		// Stump
	// S
	SKILL_CHUNKEON,		// Strong Body
	SKILL_GEOMPUNG,		// Sword Strike

	// Assassin: dagger line
	// A
	SKILL_AMSEOP = 31,		// Ambush 
	SKILL_GUNGSIN,		// Fast Attack 
	SKILL_CHARYUN,		// Rolling Dagger 
	// S
	SKILL_EUNHYUNG,		// Stealth 
	SKILL_SANGONG,		// Poisonous Cloud

	// Assassin: archery line
	// A
	SKILL_YEONSA = 46,		// Repetitive Shot 
	SKILL_KWANKYEOK,		// Arrow Shower 
	SKILL_HWAJO,		// Fire Arrow
	// S
	SKILL_GYEONGGONG,		// Feather Walk 
	SKILL_GIGUNG,		// Poison Arrow

	// Sura: weapon line
	// A
	SKILL_SWAERYUNG = 61,	// Finger Strike 
	SKILL_YONGKWON,		// Dragon Swirl 
	// S
	SKILL_GWIGEOM,		// Enchanted Blade
	SKILL_TERROR,		// Fear 
	SKILL_JUMAGAP,		// Enchanted Armour 
	SKILL_PABEOB,		// Dispel

	// Sura: black magic line
	// A
	SKILL_MARYUNG = 76,		// Dark Strike 
	SKILL_HWAYEOMPOK,		// Flame Strike 
	SKILL_MUYEONG,		// Flame Spirit 
	// S
	SKILL_MANASHILED,		// Dark Protection
	SKILL_TUSOK,		// Spirit Strike 
	SKILL_MAHWAN,		// Dark Orb

	// Shaman: dragon line
	// A
	SKILL_BIPABU = 91,
	SKILL_YONGBI,		// Flying Talisman 
	SKILL_PAERYONG,		// Shooting Dragon
	// S
	//SKILL_BUDONG,		// Budongbakbu 
	SKILL_HOSIN,		// Blessing 
	SKILL_REFLECT,		// Reflect
	SKILL_GICHEON,		// Dragon's Aid

	// Shaman: healing (thunder) line
	// A
	SKILL_NOEJEON = 106,	// Lightning Throw 
	SKILL_BYEURAK,		// Summon Lightning 
	SKILL_CHAIN,		// Lightning Claw (chain lightning) 
	// S
	SKILL_JEONGEOP,		// Cure 
	SKILL_KWAESOK,		// Movement speed up (Swiftness)
	SKILL_JEUNGRYEOK,		// Attack Up

	// Common skills
	// 7
	SKILL_7_A_ANTI_TANHWAN = 112,
	SKILL_7_B_ANTI_AMSEOP,
	SKILL_7_C_ANTI_SWAERYUNG,
	SKILL_7_D_ANTI_YONGBI,

	// 8
	SKILL_8_A_ANTI_GIGONGCHAM,
	SKILL_8_B_ANTI_YEONSA,
	SKILL_8_C_ANTI_MAHWAN,
	SKILL_8_D_ANTI_BYEURAK,

	// Support skills

	SKILL_LEADERSHIP = 121,	// Leadership
	SKILL_COMBO	= 122,		// Combo
	SKILL_CREATE = 123,		// Crafting
	SKILL_MINING = 124,

	SKILL_LANGUAGE1 = 126,	// Shinsoo language ability
	SKILL_LANGUAGE2 = 127,	// Chunjo language ability
	SKILL_LANGUAGE3 = 128,	// Jinno language ability
	SKILL_POLYMORPH = 129,	// Polymorph

	SKILL_HORSE			= 130,	// Riding skill
	SKILL_HORSE_SUMMON		= 131,	// Horse summon skill
	SKILL_HORSE_WILDATTACK	= 137,	// Wild attack (horse)
	SKILL_HORSE_CHARGE		= 138,	// Charge
	SKILL_HORSE_ESCAPE		= 139,	// Escape
	SKILL_HORSE_WILDATTACK_RANGE = 140,	// Wild attack (bow)

	SKILL_ADD_HP	=	141,			// Blood increase
	SKILL_RESIST_PENETRATE	=	142,	// Iron wall

	GUILD_SKILL_START = 151,
	GUILD_SKILL_EYE = 151,
	GUILD_SKILL_BLOOD = 152,
	GUILD_SKILL_BLESS = 153,
	GUILD_SKILL_SEONGHWI = 154,
	GUILD_SKILL_ACCEL = 155,
	GUILD_SKILL_BUNNO = 156,
	GUILD_SKILL_JUMUN = 157,
	GUILD_SKILL_TELEPORT = 158,
	GUILD_SKILL_DOOR = 159,
	GUILD_SKILL_END = 162,

	GUILD_SKILL_COUNT = GUILD_SKILL_END - GUILD_SKILL_START + 1,

};

class skill_proto
{
	public:
		char	name[64];
		uint32_t	vnum;			// Number

		uint32_t	type;			// 0: all jobs, 1: Warrior, 2: Assassin, 3: Sura, 4: Shaman
		uint8_t	max_level;		// Max mastery
		uint8_t	level_limit;		// Level limit
		int	splash_range;		// Splash range limit

		uint8_t	point_on;		// Where is the result applied? (damage, MAX HP, HP REGEN, etc.)
		expression	point_poly;		// Formula for the result value

		expression	sp_cost_poly;		// SP cost formula
		expression	duration_poly;		// Duration formula
		expression	duration_sp_cost_poly;	// Duration SP formula
		expression	cooldown_poly;		// Cooldown formula
		expression	master_bonus_poly;	// Bonus formula when mastered
		expression	splash_around_damage_adjust_poly;	// For splash attacks, damage reduction ratio applied to surrounding enemies

		uint32_t	flag;			// Skill options
		uint32_t	affect_flag;		// Affect applied when hit by the skill

		uint8_t	level_step;		// Skill points needed per raise
		uint32_t	pre_skill_vnum;		// Prerequisite skill that must be learned first
		uint8_t	pre_skill_level;		// Required level of the prerequisite skill

		int32_t	max_hit;

		uint8_t	skill_attr_type;

		// 2nd application
		uint8_t	point_on2;		
		expression	point_poly2;		
		expression	duration_poly2;		
		uint32_t	flag2;			
		uint32_t	affect_flag2;		

		uint32_t   target_range;

		bool	is_charge_skill()
		{
			return vnum == SKILL_TANHWAN || vnum == SKILL_HORSE_CHARGE; 
		}

		// 3rd application
		uint8_t point_on3;
		expression point_poly3;
		expression duration_poly3;

		expression grand_master_add_sp_cost_poly;

		void set_point_var(const std::string& strName, double dVar);
		void set_duration_var(const std::string& strName, double dVar);
		void set_sp_cost_var(const std::string& strName, double dVar);
};

class skill_manager : public singleton<skill_manager>
{
	public:
		skill_manager();
		~skill_manager() override;

		bool initialize(skill_table * pTab, int iSize);
		skill_proto * get(uint32_t dwVnum);
		skill_proto * get(const char * c_pszSkillName);

	protected:
		std::map<uint32_t, std::unique_ptr<skill_proto>> map_pk_skill_proto_;
};
}
