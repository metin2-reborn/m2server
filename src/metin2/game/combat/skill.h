#pragma once

#include <metin2/common/singleton.h>
#include <metin2/proto/tables.h>
#include <metin2/poly/expression.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>

namespace metin2::game::inline combat {

enum skill_flags
{
	SKILL_FLAG_ATTACK			= (1 << 0),
	SKILL_FLAG_USE_MELEE_DAMAGE		= (1 << 1),
	SKILL_FLAG_COMPUTE_ATTGRADE		= (1 << 2),
	SKILL_FLAG_SELFONLY			= (1 << 3),
	SKILL_FLAG_USE_MAGIC_DAMAGE		= (1 << 4),
	SKILL_FLAG_USE_HP_AS_COST		= (1 << 5),
	SKILL_FLAG_COMPUTE_MAGIC_DAMAGE	= (1 << 6),
	SKILL_FLAG_SPLASH			= (1 << 7),
	SKILL_FLAG_GIVE_PENALTY		= (1 << 8),
	SKILL_FLAG_USE_ARROW_DAMAGE		= (1 << 9),
	SKILL_FLAG_PENETRATE		= (1 << 10),
	SKILL_FLAG_IGNORE_TARGET_RATING	= (1 << 11),
	SKILL_FLAG_SLOW			= (1 << 12),
	SKILL_FLAG_STUN			= (1 << 13),
	SKILL_FLAG_HP_ABSORB		= (1 << 14),
	SKILL_FLAG_SP_ABSORB		= (1 << 15),
	SKILL_FLAG_FIRE_CONT		= (1 << 16),
	SKILL_FLAG_REMOVE_BAD_AFFECT	= (1 << 17),
	SKILL_FLAG_REMOVE_GOOD_AFFECT	= (1 << 18),
	SKILL_FLAG_CRUSH			= (1 << 19),
	SKILL_FLAG_POISON			= (1 << 20),
	SKILL_FLAG_TOGGLE			= (1 << 21),
	SKILL_FLAG_DISABLE_BY_POINT_UP	= (1 << 22),
	SKILL_FLAG_CRUSH_LONG		= (1 << 23),
	SKILL_FLAG_WIND		= (1 << 24),
	SKILL_FLAG_ELEC		= (1 << 25),
	SKILL_FLAG_FIRE		= (1 << 26),
};

enum
{
	SKILL_PENALTY_DURATION = 3,
	SKILL_TYPE_HORSE = 5,
};

enum skill_indexes
{
	SKILL_RESERVED = 0,

	SKILL_SAMYEON = 1,
	SKILL_PALBANG,
	SKILL_JEONGWI,
	SKILL_GEOMKYUNG,
	SKILL_TANHWAN,

	SKILL_GIGONGCHAM = 16,
	SKILL_GYOKSAN,
	SKILL_DAEJINGAK,
	SKILL_CHUNKEON,
	SKILL_GEOMPUNG,

	SKILL_AMSEOP = 31,
	SKILL_GUNGSIN,
	SKILL_CHARYUN,
	SKILL_EUNHYUNG,
	SKILL_SANGONG,

	SKILL_YEONSA = 46,
	SKILL_KWANKYEOK,
	SKILL_HWAJO,
	SKILL_GYEONGGONG,
	SKILL_GIGUNG,

	SKILL_SWAERYUNG = 61,
	SKILL_YONGKWON,
	SKILL_GWIGEOM,
	SKILL_TERROR,
	SKILL_JUMAGAP,
	SKILL_PABEOB,

	SKILL_MARYUNG = 76,
	SKILL_HWAYEOMPOK,
	SKILL_MUYEONG,
	SKILL_MANASHILED,
	SKILL_TUSOK,
	SKILL_MAHWAN,

	SKILL_BIPABU = 91,
	SKILL_YONGBI,
	SKILL_PAERYONG,
	SKILL_HOSIN,
	SKILL_REFLECT,
	SKILL_GICHEON,

	SKILL_NOEJEON = 106,
	SKILL_BYEURAK,
	SKILL_CHAIN,
	SKILL_JEONGEOP,
	SKILL_KWAESOK,
	SKILL_JEUNGRYEOK,

	SKILL_7_A_ANTI_TANHWAN = 112,
	SKILL_7_B_ANTI_AMSEOP,
	SKILL_7_C_ANTI_SWAERYUNG,
	SKILL_7_D_ANTI_YONGBI,

	SKILL_8_A_ANTI_GIGONGCHAM,
	SKILL_8_B_ANTI_YEONSA,
	SKILL_8_C_ANTI_MAHWAN,
	SKILL_8_D_ANTI_BYEURAK,


	SKILL_LEADERSHIP = 121,
	SKILL_COMBO	= 122,
	SKILL_CREATE = 123,
	SKILL_MINING = 124,

	SKILL_LANGUAGE1 = 126,
	SKILL_LANGUAGE2 = 127,
	SKILL_LANGUAGE3 = 128,
	SKILL_POLYMORPH = 129,

	SKILL_HORSE			= 130,
	SKILL_HORSE_SUMMON		= 131,
	SKILL_HORSE_WILDATTACK	= 137,
	SKILL_HORSE_CHARGE		= 138,
	SKILL_HORSE_ESCAPE		= 139,
	SKILL_HORSE_WILDATTACK_RANGE = 140,

	SKILL_ADD_HP	=	141,
	SKILL_RESIST_PENETRATE	=	142,

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
		uint32_t	vnum;

		uint32_t	type;
		uint8_t	max_level;
		uint8_t	level_limit;
		int	splash_range;

		uint8_t	point_on;
		expression	point_poly;

		expression	sp_cost_poly;
		expression	duration_poly;
		expression	duration_sp_cost_poly;
		expression	cooldown_poly;
		expression	master_bonus_poly;
		expression	splash_around_damage_adjust_poly;

		uint32_t	flag;
		uint32_t	affect_flag;

		uint8_t	level_step;
		uint32_t	pre_skill_vnum;
		uint8_t	pre_skill_level;

		int32_t	max_hit;

		uint8_t	skill_attr_type;

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

		uint8_t point_on3;
		expression point_poly3;
		expression duration_poly3;

		expression grand_master_add_sp_cost_poly;

		void set_point_var(const std::string& name, double var);
		void set_duration_var(const std::string& name, double var);
		void set_sp_cost_var(const std::string& name, double var);
};

class skill_manager : public singleton<skill_manager>
{
	public:
		skill_manager();
		~skill_manager() override;

		bool initialize(skill_table * tab, int size);
		skill_proto * get(uint32_t vnum);
		skill_proto * get(const char * skill_name);

	protected:
		std::map<uint32_t, std::unique_ptr<skill_proto>> map_pk_skill_proto_;
};
}
