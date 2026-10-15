#pragma once

#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/affect_flag.h>
#include <metin2/game/items/cube.h>
#include <metin2/game/world/entity.h>
#include <metin2/game/app/event.h>
#include <metin2/game/characters/fsm.h>
#include <metin2/game/characters/horse_rider.h>
#include <metin2/game/characters/state.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/world/vid.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/core/main.h>
#include <metin2/core/utils.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <sys/time.h>

namespace metin2::game::inline combat {
    class skill_proto;
}
namespace metin2::game::inline events {
    class arena;
}
namespace metin2::game::inline guilds {
    class guild;
    class war_map;
}
namespace metin2::game::inline parties {
    class party;
}
namespace metin2::game::inline protocol {
    struct packet_gc_party_update;
}
namespace metin2::game::inline social {
    namespace marriage {
        class wedding_map;
    }
}
namespace metin2::game::inline trade {
    class exchange;
    class safebox;
    class shop;
}
namespace metin2::game::inline world {
    class dungeon;
}

namespace metin2::game::inline characters {

class buff_on_attributes;
class pet_system;

inline constexpr int INSTANT_FLAG_DEATH_PENALTY = (1 << 0);
inline constexpr int INSTANT_FLAG_SHOP = (1 << 1);
inline constexpr int INSTANT_FLAG_STUN = (1 << 3);
inline constexpr int INSTANT_FLAG_NO_REWARD = (1 << 4);
extern int portal_limit_time;

enum
{
	MAIN_RACE_WARRIOR_M,
	MAIN_RACE_ASSASSIN_W,
	MAIN_RACE_SURA_M,
	MAIN_RACE_SHAMAN_W,
	MAIN_RACE_WARRIOR_W,
	MAIN_RACE_ASSASSIN_M,
	MAIN_RACE_SURA_W,
	MAIN_RACE_SHAMAN_M,
	MAIN_RACE_MAX_NUM,
};

enum
{
	POISON_LENGTH = 30,
	STAMINA_PER_STEP = 1,
	SAFEBOX_PAGE_SIZE = 9,
	AI_CHANGE_ATTACK_POISITION_TIME_NEAR = 10000,
	AI_CHANGE_ATTACK_POISITION_TIME_FAR = 1000,
	AI_CHANGE_ATTACK_POISITION_DISTANCE = 100,
	SUMMON_MONSTER_COUNT = 3,
};

enum
{
	FLY_NONE,
	FLY_EXP,
	FLY_HP_MEDIUM,
	FLY_HP_BIG,
	FLY_SP_SMALL,
	FLY_SP_MEDIUM,
	FLY_SP_BIG,
	FLY_FIREWORK1,
	FLY_FIREWORK2,
	FLY_FIREWORK3,
	FLY_FIREWORK4,
	FLY_FIREWORK5,
	FLY_FIREWORK6,
	FLY_FIREWORK_CHRISTMAS,
	FLY_CHAIN_LIGHTNING,
	FLY_HP_SMALL,
	FLY_SKILL_MUYEONG,
};

enum damage_type
{
	DAMAGE_TYPE_NONE,
	DAMAGE_TYPE_NORMAL,
	DAMAGE_TYPE_NORMAL_RANGE,
	//Skill
	DAMAGE_TYPE_MELEE,
	DAMAGE_TYPE_RANGE,
	DAMAGE_TYPE_FIRE,
	DAMAGE_TYPE_ICE,
	DAMAGE_TYPE_ELEC,
	DAMAGE_TYPE_MAGIC,
	DAMAGE_TYPE_POISON,
	DAMAGE_TYPE_SPECIAL,
};

enum point_types
{
	POINT_NONE,                 // 0
	POINT_LEVEL,                // 1
	POINT_VOICE,                // 2
	POINT_EXP,                  // 3
	POINT_NEXT_EXP,             // 4
	POINT_HP,                   // 5
	POINT_MAX_HP,               // 6
	POINT_SP,                   // 7
	POINT_MAX_SP,               // 8  
	POINT_STAMINA,              // 9  Stamina
	POINT_MAX_STAMINA,          // 10 Max stamina

	POINT_GOLD,                 // 11
	POINT_ST,                   // 12 Strength
	POINT_HT,                   // 13 Vitality
	POINT_DX,                   // 14 Dexterity
	POINT_IQ,                   // 15 Intelligence
	POINT_DEF_GRADE,		// 16 ...
	POINT_ATT_SPEED,            // 17 Attack speed
	POINT_ATT_GRADE,		// 18 Attack power MAX
	POINT_MOV_SPEED,            // 19 Movement speed
	POINT_CLIENT_DEF_GRADE,	// 20 Defense grade
	POINT_CASTING_SPEED,        // 21 Casting speed: (cooldown*100) / (100 + this value) = final cooldown time
	POINT_MAGIC_ATT_GRADE,      // 22 Magic attack
	POINT_MAGIC_DEF_GRADE,      // 23 Magic defense
	POINT_EMPIRE_POINT,         // 24 Empire points
	POINT_LEVEL_STEP,           // 25 Step within a level.. (reward at 1 2 3, level up at 4)
	POINT_STAT,                 // 26 Number of stat points available
	POINT_SUB_SKILL,		// 27 Support skill points
	POINT_SKILL,		// 28 Active skill points
	POINT_WEAPON_MIN,		// 29 Weapon min damage
	POINT_WEAPON_MAX,		// 30 Weapon max damage
	POINT_PLAYTIME,             // 31 Play time
	POINT_HP_REGEN,             // 32 HP regen rate
	POINT_SP_REGEN,             // 33 SP regen rate

	POINT_BOW_DISTANCE,         // 34 Bow range bonus (meter)

	POINT_HP_RECOVERY,          // 35 HP recovery bonus
	POINT_SP_RECOVERY,          // 36 SP recovery bonus

	POINT_POISON_PCT,           // 37 Poison chance
	POINT_STUN_PCT,             // 38 Stun chance
	POINT_SLOW_PCT,             // 39 Slow chance
	POINT_CRITICAL_PCT,         // 40 Critical chance
	POINT_PENETRATE_PCT,        // 41 Piercing hit chance
	POINT_CURSE_PCT,            // 42 Curse chance

	POINT_ATTBONUS_HUMAN,       // 43 Strong against humans
	POINT_ATTBONUS_ANIMAL,      // 44 Damage % bonus against animals
	POINT_ATTBONUS_ORC,         // 45 Damage % bonus against orcs
	POINT_ATTBONUS_MILGYO,      // 46 Damage % bonus against esoterics
	POINT_ATTBONUS_UNDEAD,      // 47 Damage % bonus against undead
	POINT_ATTBONUS_DEVIL,       // 48 Damage % bonus against devils (demons)
	POINT_ATTBONUS_INSECT,      // 49 Insect race
	POINT_ATTBONUS_FIRE,        // 50 Fire race
	POINT_ATTBONUS_ICE,         // 51 Ice race
	POINT_ATTBONUS_DESERT,      // 52 Desert race
	POINT_ATTBONUS_MONSTER,     // 53 Strong against all monsters
	POINT_ATTBONUS_WARRIOR,     // 54 Strong against Warriors
	POINT_ATTBONUS_ASSASSIN,	// 55 Strong against Assassins
	POINT_ATTBONUS_SURA,		// 56 Strong against Suras
	POINT_ATTBONUS_SHAMAN,		// 57 Strong against Shamans
	POINT_ATTBONUS_TREE,     	// 58 Strong against trees 20050729.myevan UNUSED5 

	POINT_RESIST_WARRIOR,		// 59 Resistance to Warriors
	POINT_RESIST_ASSASSIN,		// 60 Resistance to Assassins
	POINT_RESIST_SURA,			// 61 Resistance to Suras
	POINT_RESIST_SHAMAN,		// 62 Resistance to Shamans

	POINT_STEAL_HP,             // 63 HP steal
	POINT_STEAL_SP,             // 64 SP steal

	POINT_MANA_BURN_PCT,        // 65 Mana burn

	/// Bonuses when damaged ///

	POINT_DAMAGE_SP_RECOVER,    // 66 Chance to recover SP when attacked

	POINT_BLOCK,                // 67 Block rate
	POINT_DODGE,                // 68 Dodge rate

	POINT_RESIST_SWORD,         // 69
	POINT_RESIST_TWOHAND,       // 70
	POINT_RESIST_DAGGER,        // 71
	POINT_RESIST_BELL,          // 72
	POINT_RESIST_FAN,           // 73
	POINT_RESIST_BOW,           // 74  Arrow   resist   : damage reduction
	POINT_RESIST_FIRE,          // 75  Fire   resist   : damage reduction against fire attacks
	POINT_RESIST_ELEC,          // 76  Lightning   resist   : damage reduction against lightning attacks
	POINT_RESIST_MAGIC,         // 77  Magic   resist   : damage reduction against all magic
	POINT_RESIST_WIND,          // 78  Wind   resist   : damage reduction against wind attacks

	POINT_REFLECT_MELEE,        // 79 Reflect attack

	/// When taking special damage ///
	POINT_REFLECT_CURSE,		// 80 Reflect curse
	POINT_POISON_REDUCE,		// 81 Poison damage reduction

	/// On killing an enemy ///
	POINT_KILL_SP_RECOVER,		// 82 MP recovery on killing an enemy
	POINT_EXP_DOUBLE_BONUS,		// 83
	POINT_GOLD_DOUBLE_BONUS,		// 84
	POINT_ITEM_DROP_BONUS,		// 85

	/// Recovery related ///
	POINT_POTION_BONUS,			// 86
	POINT_KILL_HP_RECOVERY,		// 87

	POINT_IMMUNE_STUN,			// 88
	POINT_IMMUNE_SLOW,			// 89
	POINT_IMMUNE_FALL,			// 90
	//////////////////

	POINT_PARTY_ATTACKER_BONUS,		// 91
	POINT_PARTY_TANKER_BONUS,		// 92

	POINT_ATT_BONUS,			// 93
	POINT_DEF_BONUS,			// 94

	POINT_ATT_GRADE_BONUS,		// 95
	POINT_DEF_GRADE_BONUS,		// 96
	POINT_MAGIC_ATT_GRADE_BONUS,	// 97
	POINT_MAGIC_DEF_GRADE_BONUS,	// 98

	POINT_RESIST_NORMAL_DAMAGE,		// 99

	POINT_HIT_HP_RECOVERY,		// 100
	POINT_HIT_SP_RECOVERY, 		// 101
	POINT_MANASHIELD,			// 102 Mana shield strength from the Dark Protection skill

	POINT_PARTY_BUFFER_BONUS,		// 103
	POINT_PARTY_SKILL_MASTER_BONUS,	// 104

	POINT_HP_RECOVER_CONTINUE,		// 105
	POINT_SP_RECOVER_CONTINUE,		// 106

	POINT_STEAL_GOLD,			// 107 
	POINT_POLYMORPH,			// 108 Polymorphed monster number
	POINT_MOUNT,			// 109 Mounted monster number

	POINT_PARTY_HASTE_BONUS,		// 110
	POINT_PARTY_DEFENDER_BONUS,		// 111
	POINT_STAT_RESET_COUNT,		// 112 Stat reset points from using Blood Pills (1 point resets 1 stat point)

	POINT_HORSE_SKILL,			// 113

	POINT_MALL_ATTBONUS,		// 114 Attack +x%
	POINT_MALL_DEFBONUS,		// 115 Defense +x%
	POINT_MALL_EXPBONUS,		// 116 Experience +x%
	POINT_MALL_ITEMBONUS,		// 117 Item drop rate x/10 times
	POINT_MALL_GOLDBONUS,		// 118 Gold drop rate x/10 times

	POINT_MAX_HP_PCT,			// 119 Max HP +x%
	POINT_MAX_SP_PCT,			// 120 Max SP +x%

	POINT_SKILL_DAMAGE_BONUS,		// 121 Skill damage *(100+x)%
	POINT_NORMAL_HIT_DAMAGE_BONUS,	// 122 Normal hit damage *(100+x)%

	// DEFEND_BONUS_ATTRIBUTES
	POINT_SKILL_DEFEND_BONUS,		// 123 Skill damage defense
	POINT_NORMAL_HIT_DEFEND_BONUS,	// 124 Normal hit damage defense
	// END_OF_DEFEND_BONUS_ATTRIBUTES

	// PC_BANG_ITEM_ADD 
	POINT_PC_BANG_EXP_BONUS,		// 125 PC-bang-only experience bonus
	POINT_PC_BANG_DROP_BONUS,		// 126 PC-bang-only drop rate bonus
	// END_PC_BANG_ITEM_ADD
	POINT_RAMADAN_CANDY_BONUS_EXP,			// For the Ramadan candy experience bonus

	POINT_ENERGY = 128,					// 128 Energy

	// For the energy UI.
	// Not used on the server, but done this way because the client manages the energy end time as a POINT.
	// How embarrassing
	POINT_ENERGY_END_TIME = 129,					// 129 Energy end time

	POINT_COSTUME_ATTR_BONUS = 130,
	POINT_MAGIC_ATT_BONUS_PER = 131,
	POINT_MELEE_MAGIC_ATT_BONUS_PER = 132,

	// Additional attribute resistances
	POINT_RESIST_ICE = 133,          //   Ice resist   : damage reduction against ice attacks
	POINT_RESIST_EARTH = 134,        //   Earth resist   : damage reduction against ice attacks
	POINT_RESIST_DARK = 135,         //   Dark resist   : damage reduction against ice attacks

	POINT_RESIST_CRITICAL = 136,		// Critical resist	: reduces the opponent's critical chance
	POINT_RESIST_PENETRATE = 137,		// Piercing resist	: reduces the opponent's piercing hit chance

	//POINT_MAX_NUM = 129	common/length.h
};

enum pk_modes
{
	PK_MODE_PEACE,
	PK_MODE_REVENGE,
	PK_MODE_FREE,
	PK_MODE_PROTECT,
	PK_MODE_GUILD,
	PK_MODE_MAX_NUM
};

enum positions
{
	POS_DEAD,
	POS_SLEEPING,
	POS_RESTING,
	POS_SITTING,
	POS_FISHING,
	POS_FIGHTING,
	POS_MOUNTING,
	POS_STANDING
};

enum block_action
{
	BLOCK_EXCHANGE		= (1 << 0),
	BLOCK_PARTY_INVITE		= (1 << 1),
	BLOCK_GUILD_INVITE		= (1 << 2),
	BLOCK_WHISPER		= (1 << 3),
	BLOCK_MESSENGER_INVITE	= (1 << 4),
	BLOCK_PARTY_REQUEST		= (1 << 5),
};

// <Factor> Dynamically evaluated CHARACTER* equivalent.
// Referring to SCharDeadEventInfo.
struct dynamic_character_ptr {
	dynamic_character_ptr() : is_pc(false), id(0) {}

	// Returns the CHARACTER * found in CHARACTER_MANAGER.
	character * get() const; 
	// Clears the current settings.
	void reset() {
		is_pc = false;
		id = 0;
	}

	// Supports assignment with CHARACTER * type.
	dynamic_character_ptr& operator=(character * character);
	// Supports type casting to CHARACTER *.
	operator character *() const {
		return get();
	}

	bool is_pc;
	uint32_t id;
};

/* Saved data */
struct character_point {
	int32_t			points[POINT_MAX_NUM];

	uint8_t			job;
	uint8_t			voice;

	uint8_t			level;
	uint32_t			exp;
	int32_t			gold;

	int				hp;
	int				sp;

	int				random_hp;
	int				random_sp;

	int				stamina;

	uint8_t			skill_group;
};

/* Character data that is not saved */
struct character_point_instant {
	int32_t			points[POINT_MAX_NUM];

	float			rot;

	int				max_hp;
	int				max_sp;

	int32_t			position;

	int32_t			instant_flag;
	uint32_t			ai_flag;
	uint32_t			immune_flag;
	uint32_t			last_shout_pulse;

	uint16_t			parts[PART_MAX_NUM];

	item *			items[INVENTORY_AND_EQUIP_SLOT_MAX];
	uint8_t			item_grid[INVENTORY_AND_EQUIP_SLOT_MAX];

	// Dragon soul inventory.
	item *			ds_items[DRAGON_SOUL_INVENTORY_MAX_NUM];
	uint16_t			ds_item_grid[DRAGON_SOUL_INVENTORY_MAX_NUM];

	// by mhh
	item *			cube_items[CUBE_MAX_NUM];
	character *		cube_npc;

	character *			battle_victim;

	uint8_t			gm_level;

	uint8_t			base_part;	// Casual clothing number

	int				max_stamina;

	uint8_t			block_mode;

	int				dragon_soul_active_deck;
	entity *		dragon_soul_refine_window_opener;
};

#define TRIGGERPARAM		character * ch, character * causer

class trigger
{
	public:
		trigger() : type(0), func(nullptr)
		{
		}

		uint8_t	type;
		int	(*func) (TRIGGERPARAM);
};

EVENTINFO(char_event_info)
{
	dynamic_character_ptr ch;
};

struct skill_use_info
{
	int	    hit_count;
	int	    max_hit_count;
	int	    splash_count;
	uint32_t   next_skill_usable_time;
	int	    range;
	bool    used;
	uint32_t   vid;
	bool    is_grand_master;

	std::unordered_map<uint32_t, size_t> target_vid_map;

	skill_use_info()
		: hit_count(0), max_hit_count(0), splash_count(0), next_skill_usable_time(0), range(0), used(false),
		vid(0), is_grand_master(false)
   	{}

	bool    hit_once(uint32_t vnum = 0);

	bool    use_skill(bool is_grand_master_value, uint32_t vid_value, uint32_t cooltime, int splashcount = 1, int hitcount = -1, int range_value = -1);
	uint32_t   main_target_vid() const	{ return vid; }
	void    set_main_target_vid(uint32_t vid_value) { vid=vid_value; }
};

class affect;


class mob;
class mob_instance;
struct mob_skill_info;

//SKILL_POWER_BY_LEVEL
extern int get_skill_power_by_level_from_type(int job, int skillgroup, int skilllevel);
//END_SKILL_POWER_BY_LEVEL


class character : public entity, public fsm, public horse_rider
{
	protected:
		//////////////////////////////////////////////////////////////////////////////////
		// Entity related
		void	encode_insert_packet(entity * entity) override;
		void	encode_remove_packet(entity * entity) override;
		//////////////////////////////////////////////////////////////////////////////////

	public:
		character *			find_character_in_view(const char * name_value, bool find_pc_only);
		void				update_packet();
		int				analyze_protect; 
		int				analyze_protect_count;

		//////////////////////////////////////////////////////////////////////////////////
		// FSM (Finite State Machine) related
	protected:
		state_template<character>	state_move_;
		state_template<character>	state_battle_;
		state_template<character>	state_idle_;

	public:
		virtual void		state_move();
		virtual void		state_battle();
		virtual void		state_idle();
		virtual void		state_flag();
		virtual void		state_flag_base();
		void				state_horse();

	protected:
		// STATE_IDLE_REFACTORING
		void				state_idle_monster();
		void				state_idle_stone();
		void				state_idle_npc();
		// END_OF_STATE_IDLE_REFACTORING

	public:
		uint32_t ai_flag() const	{ return points_instant_.ai_flag; }
	
		void				set_aggressive();
		bool				is_aggressive() const;

		void				set_coward();
		bool				is_coward() const;
		void				coward_escape();

		bool				is_no_attack_shinsu() const;

		bool				is_no_attack_chunjo() const;

		bool				is_no_attack_jinno() const;

		bool				is_attack_mob() const;

		virtual void			begin_state_empty();
		virtual void			end_state_empty() {}

		void				restart_at_same_pos();

	protected:
		uint32_t				state_duration_;
		//////////////////////////////////////////////////////////////////////////////////

	public:
		character();
		~character() override;

		void			create(const char * name, uint32_t vid, bool is_pc);
		void			destroy();

		void			disconnect(const char * reason);

	protected:
		void			initialize();

		//////////////////////////////////////////////////////////////////////////////////
		// Basic Points
	public:
		uint32_t			player_id() const	{ return player_id_; }

		void			set_player_proto(const player_table * t);
		void			create_player_proto(player_table & tab);	// Used when saving

		void			set_proto(const mob * mob_value);
		uint16_t			race_num() const;

		void			save();		// DelayedSave
		void			save_real();	// Actual save
		void			flush_delayed_save_item();

		const char *	name() const;
		const vid &		get_vid() const		{ return vid_;		}

		void			set_name(const std::string& name) { name_ = name; }

		void			set_race(uint8_t race);
		bool			change_sex();

		uint32_t			aid() const;
		int				change_empire_count() const;
		void			set_change_empire_count();
		int				change_empire(uint8_t empire_value);

		uint8_t			job() const;
		uint8_t			get_char_type() const;

		bool			is_pc() const		{ return get_desc() ? true : false; }
		bool			is_npc()	const		{ return char_type_ != CHAR_TYPE_PC; }
		bool			is_monster()	const	{ return char_type_ == CHAR_TYPE_MONSTER; }
		bool			is_stone() const		{ return char_type_ == CHAR_TYPE_STONE; }
		bool			is_door() const		{ return char_type_ == CHAR_TYPE_DOOR; } 
		bool			is_building() const	{ return char_type_ == CHAR_TYPE_BUILDING;  }
		bool			is_warp() const		{ return char_type_ == CHAR_TYPE_WARP; }
		bool			is_goto() const		{ return char_type_ == CHAR_TYPE_GOTO; }
//		bool			IsPet() const		{ return m_bCharType == CHAR_TYPE_PET; }

		uint32_t			last_shout_pulse() const	{ return points_instant_.last_shout_pulse; }
		void			set_last_shout_pulse(uint32_t pulse) { points_instant_.last_shout_pulse = pulse; }
		int				level() const		{ return points_.level;	}
		void			set_level(uint8_t level);

		uint8_t			gm_level() const;
		bool 			is_gm() const;
		void			set_gm_level(); 

		uint32_t			get_exp() const		{ return points_.exp;	}
		void			set_exp(uint32_t exp)	{ points_.exp = exp;	}
		uint32_t			next_exp() const;
		character *		distribute_exp();	// Returns the one who dealt the most hits.
		void			distribute_hp(character * killer);
		void			distribute_sp(character * killer, int method=0);

		void			set_position(int pos);
		bool			is_position(int pos) const	{ return points_instant_.position == pos ? true : false; }
		int				get_position() const		{ return points_instant_.position; }

		void			set_part(uint8_t part_pos, uint16_t val);
		uint16_t			part(uint8_t part_pos) const;
		uint16_t			original_part(uint8_t part_pos) const;

		void			set_hp(int hp)		{ points_.hp = hp; }
		int				hp() const		{ return points_.hp; }

		void			set_sp(int sp)		{ points_.sp = sp; }
		int				sp() const		{ return points_.sp; }

		void			set_stamina(int stamina)	{ points_.stamina = stamina; }
		int				stamina() const		{ return points_.stamina; }

		void			set_max_hp(int val)	{ points_instant_.max_hp = val; }
		int				max_hp() const	{ return points_instant_.max_hp; }

		void			set_max_sp(int val)	{ points_instant_.max_sp = val; }
		int				max_sp() const	{ return points_instant_.max_sp; }

		void			set_max_stamina(int val)	{ points_instant_.max_stamina = val; }
		int				max_stamina() const	{ return points_instant_.max_stamina; }

		void			set_random_hp(int v)	{ points_.random_hp = v; }
		void			set_random_sp(int v)	{ points_.random_sp = v; }

		int				hp_pct() const;

		void			set_real_point(uint8_t type, int val);
		int				real_point(uint8_t type) const;

		void			set_point(uint8_t type, int val);
		int				point(uint8_t type) const;
		int				limit_point(uint8_t type) const;
		int				polymorph_point(uint8_t type) const;

		const mob_table &	get_mob_table() const;
		uint8_t				get_mob_rank() const;
		uint8_t				mob_battle_type() const;
		uint32_t				mob_damage_min() const;
		uint32_t				mob_damage_max() const;
		uint16_t				mob_attack_range() const;
		uint32_t				mob_drop_item_vnum() const;
		float				mob_damage_multiply() const;

		// NEWAI
		bool			is_berserker() const;
		bool			is_berserk() const;
		void			set_berserk(bool mode);

		bool			is_stone_skinner() const;

		bool			is_god_speeder() const;
		bool			is_god_speed() const;
		void			set_god_speed(bool mode);

		bool			is_death_blower() const;
		bool			is_death_blow() const;

		bool			is_reviver() const;
		bool			has_reviver_in_party() const;
		bool			is_revive() const;
		void			set_revive(bool mode);
		// NEWAI END

		bool			is_race_flag(uint32_t bit) const;
		bool			is_summon_monster() const;
		uint32_t			summon_vnum() const;

		uint32_t			polymorph_item_vnum() const;
		uint32_t			monster_drain_sp_point() const;

		void			main_character_packet();	// Tells it that I am the main character.

		void			compute_points();
		void			compute_battle_points();
		void			point_change(uint8_t type, int amount, bool amount_value = false, bool broadcast = false);
		void			points_packet();
		void			apply_point(uint8_t apply_type, int val);
		void			check_maximum_points();	// Checks whether current values like HP and SP exceed their max and lowers them if so.

		bool			show(int32_t map_index, int32_t x, int32_t y, int32_t z = INT32_MAX, bool show_spawn_motion = false);

		void			sitdown(int is_ground);
		void			standup();

		void			set_rotation(float rot_value);
		void			set_rotation_to_xy(int32_t x_value, int32_t y_value);
		float			rotation() const	{ return points_instant_.rot; }

		void			chat_packet(uint8_t type, const char *format, ...);
		void			monster_chat(uint8_t monster_chat_type);
		void			send_greet_message();

		void			reset_point(int lv);

		void			set_block_mode(uint8_t flag);
		void			set_block_mode_force(uint8_t flag);
		bool			is_block_mode(uint8_t flag) const	{ return (points_instant_.block_mode & flag)?true:false; }

		bool			is_polymorphed() const		{ return polymorph_race_>0; }
		bool			is_poly_maintain_stat() const	{ return poly_maintain_stat_; } // Polymorph that keeps the previous stats.
		void			set_polymorph(uint32_t race_num, bool maintain_stat = false);
		uint32_t			polymorph_vnum() const	{ return polymorph_race_; }
		int				polymorph_power() const;

		// FISING	
		void			fishing();
		void			fishing_take();
		// END_OF_FISHING

		// MINING
		void			mining(character * ch_load);
		void			mining_cancel();
		void			mining_take();
		// END_OF_MINING

		void			reset_play_time(uint32_t time_remain = 0);

		void			create_fly(uint8_t type_value, character * victim);

		void			reset_chat_counter();
		uint8_t			increase_chat_counter();
		uint8_t			chat_counter() const;

	protected:
		uint32_t			polymorph_race_;
		bool			poly_maintain_stat_;
		uint32_t			player_id_;
		vid				vid_;
		std::string		name_;
		uint8_t			char_type_;

		character_point		points_;
		character_point_instant	points_instant_;

		uint32_t			play_start_time_;
		uint8_t			add_chr_state_;
		bool			skip_save_;
		std::string		mobile_;
		std::string		mobile_auth_;
		uint8_t			chat_counter_;

		// End of Basic Points

		//////////////////////////////////////////////////////////////////////////////////
		// Move & Synchronize Positions
		//////////////////////////////////////////////////////////////////////////////////
	public:
		bool			is_walking() const			{ return now_walking_ || stamina()<=0; }
		void			set_walking(bool walk_flag)	{ walking_=walk_flag; }
		void			set_now_walking(bool walk_flag);	
		void			reset_walking()			{ set_now_walking(walking_); }

		bool			go_to(int32_t x_value, int32_t y_value);	// Blend to the target position instead of moving immediately.
		void			stop();

		bool			can_move() const;		// Can it move?

		void			sync_packet();
		bool			sync(int32_t x, int32_t y);	// Actual movement happens through this method (no movement restrictions from conditions)
		bool			move(int32_t x_value, int32_t y_value);	// Checks conditions and moves via the Sync method.
		void			on_move(bool is_attack = false);	// Called when moving. May be called from places other than Move().
		uint32_t			get_motion_mode() const;
		float			move_motion_speed() const;
		float			move_speed() const;
		void			calculate_move_duration();
		void			send_move_packet(uint8_t func, uint8_t arg, uint32_t x, uint32_t y, uint32_t duration, uint32_t time=0, int rot=-1);
		uint32_t			current_move_duration() const	{ return move_duration_; }
		uint32_t			walk_start_time() const	{ return walk_start_time_; }
		uint32_t			last_move_time() const		{ return last_move_time_; }
		uint32_t			last_attack_time() const	{ return last_attack_time_; }

		void			set_last_attacked(uint32_t time);	// Stores the time and position of the last attack received

		bool			set_sync_owner(character * ch, bool remove_from_list = true);
		bool			is_sync_owner(character * ch) const;

		bool			warp_set(int32_t x, int32_t y, int32_t private_map_index = 0);
		void			set_warp_location(int32_t map_index, int32_t x, int32_t y);
		void			warp_end();
		bool			warp_to_pid(uint32_t pid);

		void			save_exit_location();
		void			exit_to_saved_location();

		void			start_stamina_consume();
		void			stop_stamina_consume();
		bool			is_stamina_consume() const;
		bool			is_stamina_half_consume() const;

		void			reset_stop_time();
		uint32_t			stop_time() const;

	protected:
		void			clear_sync();

		float			sync_time_;
		character *		chr_sync_owner_;
		character_list	lst_pk_chr_sync_owned_;	// Those for whom I am the SyncOwner

		pixel_position	pos_dest_;
		pixel_position	pos_start_;
		pixel_position	pos_warp_;
		int32_t			warp_map_index_;

		pixel_position	pos_exit_;
		int32_t			exit_map_index_;

		uint32_t			move_start_time_;
		uint32_t			move_duration_;

		uint32_t			last_move_time_;
		uint32_t			last_attack_time_;
		uint32_t			walk_start_time_;
		uint32_t			stop_time_;

		bool			walking_;
		bool			now_walking_;
		bool			stamina_consume_;
		// End

		// Quickslot related
	public:
		void			sync_quickslot(uint8_t type_value, uint8_t old_pos, uint8_t new_pos);
		bool			get_quickslot(uint8_t pos, quickslot ** pp_slot);
		bool			set_quickslot(uint8_t pos, quickslot & slot_value);
		bool			del_quickslot(uint8_t pos);
		bool			swap_quickslot(uint8_t a, uint8_t b);
		void			chain_quickslot_item(item * item_value, uint8_t type_value, uint8_t old_pos);

	protected:
		quickslot		quickslot_[QUICKSLOT_MAX_NUM];

		////////////////////////////////////////////////////////////////////////////////////////
		// Affect
	public:
		void			start_affect_event();
		void			clear_affect(bool save=false);
		void			compute_affect(affect * aff, bool add);
		bool			add_affect(uint32_t type_value, uint8_t apply_on_value, int32_t apply_value_value, uint32_t flag_value, int32_t duration_value, int32_t sp_cost_value, bool override, bool is_cube = false);
		void			refresh_affect();
		bool			remove_affect(uint32_t type);
		bool			is_affect_flag(uint32_t aff) const;

		bool			update_affect();	// called from EVENT
		int				process_affect();

		void			load_affect(uint32_t count_value, packet_affect_element * elements);
		void			save_affect();

		bool			is_good_affect(uint8_t affect_type) const;

		void			remove_good_affect();
		void			remove_bad_affect();

		affect *		find_affect(uint32_t type_value, uint8_t apply=APPLY_NONE) const;
		const std::list<affect *> & affect_container() const	{ return list_pk_affect_; }
		bool			remove_affect(affect * aff);

	protected:
		affect_flag		af_affect_flag_;
		std::list<affect *>	list_pk_affect_;

	public:
		// PARTY_JOIN_BUG_FIX
		void			set_party(party * party_value);
		party *			get_party() const	{ return party_; }

		bool			request_to_party(character * leader);
		void			deny_to_party(character * member);
		void			accept_to_party(character * member);

		/// Invites another character to own party.
		/**
		 * @param	pchInvitee character to invite. Must be in a state that allows joining a party.
		 *
		 * If either character is not in a state to invite or be invited to a party, sends the corresponding chat message to the inviting character.
		 */
		void			party_invite(character * pch_invitee);

		/// Handles acceptance by an invited character.
		/**
		 * @param	pchInvitee character joining the party. Must be in a state that allows joining a party.
		 *
		 * If pchInvitee cannot join the party, sends the corresponding chat message.
		 */
		void			party_invite_accept(character * pch_invitee);

		/// Handles refusal by an invited character.
		/**
		 * @param [in]	dwPID PID of the invited character
		 */
		void			party_invite_deny(uint32_t pid);

		bool			build_update_party_packet(packet_gc_party_update & out);
		int				leadership_skill_level() const;

		bool			can_summon(int leader_ship);

		void			set_party_request_event(event_ptr event) { party_request_event_ = event; }

	protected:

		/// Joins a party.
		/**
		 * @param	pkLeader leader of the party to join
		 */
		void			party_join(character * leader);

		/**
		 * Error codes for when a party cannot be joined.
		 * Error codes are split into mutable types and static types depending on whether they are time dependent.
		 * If the error code value is below PERR_SEPARATOR it is a mutable type; if above, a static type.
		 */
		enum party_join_err_code {
			PERR_NONE		= 0,	///< Success
			PERR_SERVER,			///< Party operations unavailable due to a server problem
			PERR_DUNGEON,			///< Character is in a dungeon
			PERR_OBSERVER,			///< In observer mode
			PERR_LVBOUNDARY,		///< Level difference with the other character
			PERR_LOWLEVEL,			///< 30 levels below the other party's highest level
			PERR_HILEVEL,			///< 30 levels above the other party's lowest level
			PERR_ALREADYJOIN,		///< Target character is already in a party
			PERR_PARTYISFULL,		///< Party member limit exceeded
			PERR_SEPARATOR,			///< Error type separator.
			PERR_DIFFEMPIRE,		///< Different empire from the other character
			PERR_MAX				///< Highest error code. Add error codes before this.
		};

		/// Checks conditions for joining or forming a party.
		/**
		 * @param 	pchLeader party leader or the inviting character
		 * @param	pchGuest invited character
		 * @return	any PartyJoinErrCode may be returned.
		 */
		static party_join_err_code	is_party_joinable_condition(character * const pch_leader, character * const pch_guest);

		/// Checks dynamic conditions for joining or forming a party.
		/**
		 * @param 	pchLeader party leader or the inviting character
		 * @param	pchGuest invited character
		 * @return	returns only mutable type codes.
		 */
		static party_join_err_code	is_party_joinable_mutable_condition(character * const pch_leader, character * const pch_guest);

		party *			party_;
		uint32_t			last_dead_time_;
		event_ptr			party_request_event_;

		/**
		 * Party invite event map.
		 * key: PID of the invited character
		 * value: event pointer
		 *
		 * Event map for invited characters.
		 */
		typedef std::map< uint32_t, event_ptr >	event_map;
		event_map		party_invite_event_map_;

		// END_OF_PARTY_JOIN_BUG_FIX

		////////////////////////////////////////////////////////////////////////////////////////
		// Dungeon
	public:
		void			set_dungeon(dungeon * dungeon_value);
		dungeon *		get_dungeon() const	{ return dungeon_; }
		dungeon *		dungeon_force() const;
	protected:
		dungeon *	dungeon_;
		int			event_attr_;

		////////////////////////////////////////////////////////////////////////////////////////
		// Guild
	public:
		void			set_guild(guild * guild_value);
		guild*			get_guild() const	{ return guild_; }

		void			set_war_map(war_map* war_map_value);
		war_map*		get_war_map() const	{ return war_map_; }

	protected:
		guild *		guild_;
		uint32_t			under_guild_war_info_message_time_;
		war_map *		war_map_;

		////////////////////////////////////////////////////////////////////////////////////////
		// Item related
	public:
		bool			can_handle_item(bool skip_check_refine = false, bool skip_observer = false); // Can item-related actions be performed?

		bool			is_item_loaded() const	{ return item_loaded_; }
		void			set_item_loaded()	{ item_loaded_ = true; }

		void			clear_item();
		void			set_item(item_pos cell_value, item * item_value);
		item *			get_item(item_pos cell_value) const;
		item *			inventory_item(uint16_t cell) const;
		bool			is_empty_item_grid(item_pos cell_value, uint8_t size, int exception_cell = -1) const;

		void			set_wear(uint8_t cell, item * item);
		item *			wear(uint8_t cell) const;

		// MYSHOP_PRICE_LIST
		void			use_silk_botary(void); 		/// Use of the silk bundle item

		/// Sends the price list received from the DB cache to the user and handles use of the bundle item.
		/**
		 * @param [in] p	price list packet
		 *
		 * On the first use of the silk bundle item after logging in, UseSilkBotary requests the price list from the DB cache,
		 * and once the response arrives this function performs the actual silk bundle use.
		 */
		void			use_silk_botary_real(const packet_myshop_pricelist_header* p);
		// END_OF_MYSHOP_PRICE_LIST

		bool			use_item_ex(item * item, item_pos dest_cell);
		bool			use_item(item_pos cell_value, item_pos dest_cell = NPOS);

		// ADD_REFINE_BUILDING
		bool			is_refine_through_guild() const;
		guild *		refine_guild() const;
		int				compute_refine_fee(int cost, int multiply = 5) const;
		void			pay_refine_fee(int total_money);
		void			set_refine_npc(character * ch);
		// END_OF_ADD_REFINE_BUILDING

		bool			refine_item(item * item_value, item * target);
		bool			drop_item(item_pos cell_value,  uint8_t count_value=0);
		bool			give_recall_item(item * item);
		void			process_recall_item(item * item);

		void			effect_packet(int enum_effect_type);
		void			specific_effect_packet(const char filename[128]);

		// ADD_MONSTER_REFINE
		bool			do_refine(item * item, bool money_only = false);
		// END_OF_ADD_MONSTER_REFINE

		bool			do_refine_with_scroll(item * item);
		bool			refine_information(uint8_t cell, uint8_t type_value, int additional_cell = -1);

		void			set_refine_mode(int additional_cell = -1);
		void			clear_refine_mode();

		bool			give_item(character * victim, item_pos cell);
		bool			can_receive_item(character * from, item * item) const;
		void			receive_item(character * from, item * item);
		bool			give_item_from_special_item_group(uint32_t group_num, std::vector <uint32_t> &item_vnums, 
							std::vector <uint32_t> &item_counts, std::vector <item *> &item_gets, int &count);

		bool			move_item(item_pos cell_value, item_pos dest_cell, uint8_t count);
		bool			pickup_item(uint32_t vid);
		bool			equip_item(item * item, int candidate_cell = -1);
		bool			unequip_item(item * item);

		// Checks whether the item can currently be equipped; if not, tells the character why
		bool			can_equip_now(item * const item, const item_pos& src_cell = NPOS, const item_pos& dest_cell = NPOS);

		// Checks whether the equipped item can be removed; if not, tells the character why
		bool			can_unequip_now(item * const item, const item_pos& src_cell = NPOS, const item_pos& dest_cell = NPOS);

		bool			swap_item(uint8_t cell_value, uint8_t dest_cell);
		item *			auto_give_item(uint32_t item_vnum, uint8_t count_value=1, int rare_pct = -1, bool msg = true);
		void			auto_give_item(item * item, bool long_owner_ship = false);
		
		int				empty_inventory(uint8_t size) const;
		int				empty_dragon_soul_inventory(item * item_value) const;
		void			copy_dragon_soul_item_grid(std::vector<uint16_t>& dragon_soul_item_grid) const;

		int				count_empty_inventory() const;

		int				count_specify_item(uint32_t vnum) const;
		void			remove_specify_item(uint32_t vnum, uint32_t count = 1);
		item *			find_specify_item(uint32_t vnum) const;
		item *			find_item_by_id(uint32_t id) const;

		int				count_specify_type_item(uint8_t type) const;
		void			remove_specify_type_item(uint8_t type, uint32_t count = 1);

		bool			is_equip_unique_item(uint32_t item_vnum) const;

		// CHECK_UNIQUE_GROUP
		bool			is_equip_unique_group(uint32_t group_vnum) const;
		// END_OF_CHECK_UNIQUE_GROUP

		void			send_equipment(character * ch);
		// End of Item

	protected:

		/// Sends price information for one item.
		/**
		 * @param [in]	dwItemVnum item vnum
		 * @param [in]	dwItemPrice item price
		 */
		void			send_my_shop_price_list_cmd(uint32_t item_vnum, uint32_t item_price);

		bool			no_opened_shop_;	///< Whether a private shop has been opened since this login (true if never opened)

		bool			item_loaded_;
		int				refine_additional_cell_;
		bool			under_refine_;
		uint32_t			refine_npcvid_;

	public:
		////////////////////////////////////////////////////////////////////////////////////////
		// Money related
		int32_t				gold() const		{ return points_.gold;	}
		void			set_gold(int32_t gold)	{ points_.gold = gold;	}
		bool			drop_gold(int32_t gold_value);
		void			give_gold(int32_t amount);	// If in a party, handle party distribution, logging, etc.
		// End of Money

		////////////////////////////////////////////////////////////////////////////////////////
		// Shop related
	public:
		void			set_shop(shop * shop_value);
		shop *			get_shop() const { return shop_; }

		void			set_shop_owner(character * ch) { chr_shop_owner_ = ch; }
		character *		shop_owner() const { return chr_shop_owner_;}

		void			open_my_shop(const char * sign_value, shop_item_table * table, uint8_t item_count);
		shop *			my_shop() const { return my_shop_; }
		void			close_my_shop();

	protected:

		shop *			shop_;
		shop *			my_shop_;
		std::string		shop_sign_;
		character *		chr_shop_owner_;
		// End of shop

		////////////////////////////////////////////////////////////////////////////////////////
		// Exchange related
	public:
		bool			exchange_start(character * victim);
		void			set_exchange(exchange * exchange_value);
		exchange *		get_exchange() const	{ return exchange_;	}

	protected:
		exchange *		exchange_;
		// End of Exchange

		////////////////////////////////////////////////////////////////////////////////////////
		// Battle
	public:
		struct battle_info
		{
			int total_damage;
			int aggro;

			battle_info(int tot, int aggr)
				: total_damage(tot), aggro(aggr)
				{}
		};
		typedef std::map<vid, battle_info>	damage_map;

		struct attack_log_entry {
			uint32_t	vid;
			uint32_t	time;
		};

		bool				damage(character * attacker, int dam, damage_type type = DAMAGE_TYPE_NORMAL);
		void				death_penalty(uint8_t town);
		void				revive_invisible(int dur);

		bool				attack(character * victim, uint8_t type = 0);

		bool				can_begin_fight() const;
		void				begin_fight(character * victim); // Starts fighting pkVictimr. (Forced; use CanBeginFight to check whether it can start)

		bool				is_stun() const;
		void				stun();
		bool				is_dead() const;
		void				dead(character * killer = nullptr, bool immediate_dead=false);

		void				reward(bool item_drop);
		void				reward_gold(character * attacker);

		bool				shoot(uint8_t type);
		void				fly_target(uint32_t target_vid_value, int32_t x, int32_t y, uint8_t header_value);

		void				forget_my_attacker();
		void				aggregate_monster();
		void				attract_ranger();
		void				pull_monster();

		int					arrow_and_bow(item * * ppk_bow, item * * ppk_arrow, int arrow_count = 1);
		void				use_arrow(item * arrow, uint32_t arrow_count);

		void				attacked_by_poison(character * attacker);
		void				remove_poison();

		void				attacked_by_fire(character * attacker, int amount, int count);
		void				remove_fire();

		void				update_alignment(int amount);
		int					alignment() const;

		//Get alignment 
		int					real_alignment() const;
		void				show_alignment(bool show);

		void				set_killer_mode(bool is_on);
		bool				is_killer_mode() const;
		void				update_killer_mode();

		uint8_t				pk_mode() const;
		void				set_pk_mode(uint8_t pk_mode);

		void				item_drop_penalty(character * killer);

		void				update_aggr_point(character * attacker, damage_type type, int dam);

		//
		// HACK
		// 
	public:
		void set_combo_sequence(uint8_t seq);
		uint8_t combo_sequence() const;

		void set_last_combo_time(uint32_t time);
		uint32_t last_combo_time() const;

		int valid_combo_interval() const;
		void set_valid_combo_interval(int interval);

		uint8_t combo_index() const;

		void increase_combo_hack_count(int k = 1);
		void reset_combo_hack_count();
		void skip_combo_attack_by_time(int interval);
		uint32_t get_skip_combo_attack_by_time() const;

	protected:
		uint8_t combo_sequence_;
		uint32_t last_combo_time_;
		int valid_combo_interval_;
		uint8_t combo_index_;
		int combo_hack_count_;
		uint32_t skip_combo_attack_by_time_;

	protected:
		void				update_aggr_point_ex(character * attacker, damage_type type, int dam, battle_info & info);
		void				change_victim_by_aggro(int new_aggro, character * new_victim);

		uint32_t				fly_target_id_;
		std::vector<uint32_t>	vec_dw_fly_targets_;
		damage_map			map_k_damage_;	// How much damage has each character dealt to me?
		uint32_t				killer_pid_;

		int					alignment_;		// Lawful/Chaotic value -200000 ~ 200000
		int					real_alignment_;
		int					killer_mode_pulse_;
		uint8_t				pk_mode_;

		// Aggro
		uint32_t				last_victim_set_time_;
		int					max_aggro_;
		// End of Battle

		// Stone
	public:
		void				set_stone(character * chr_stone);
		void				clear_stone();
		void				determine_drop_metin_stone();
		uint32_t				drop_metin_stone_vnum() const { return drop_metin_stone_; }
		uint8_t				drop_metin_stone_pct() const { return drop_metin_stone_pct_; }

	protected:
		character *			chr_stone_;		// The stone that spawned me
		character_set		set_pk_chr_spawned_by_;	// The ones I spawned
		uint32_t				drop_metin_stone_;
		uint8_t				drop_metin_stone_pct_;
		// End of Stone

	public:
		enum
		{
			SKILL_UP_BY_POINT,
			SKILL_UP_BY_BOOK,
			SKILL_UP_BY_TRAIN,

			// ADD_GRANDMASTER_SKILL
			SKILL_UP_BY_QUEST,
			// END_OF_ADD_GRANDMASTER_SKILL
		};

		void				skill_level_packet();
		void				skill_level_up(uint32_t vnum_value, uint8_t method = SKILL_UP_BY_POINT);
		bool				skill_level_down(uint32_t vnum_value);
		// ADD_GRANDMASTER_SKILL
		bool				use_skill(uint32_t vnum_value, character * victim, bool use_grand_master = true);
		void				reset_skill();
		void				set_skill_level(uint32_t vnum, uint8_t lev);
		int					used_skill_master_type(uint32_t vnum);

		bool				is_learnable_skill(uint32_t skill_vnum) const;
		// END_OF_ADD_GRANDMASTER_SKILL

		bool				check_skill_hit_count(const uint8_t skill_id, const vid target_vid);
		bool				can_use_skill(uint32_t skill_vnum) const;
		bool				is_usable_skill_motion(uint32_t motion_index) const;
		int					skill_level(uint32_t vnum) const;
		int					skill_master_type(uint32_t vnum) const;
		int					skill_power(uint32_t vnum, uint8_t level = 0) const;

		time_t				skill_next_read_time(uint32_t vnum) const;
		void				set_skill_next_read_time(uint32_t vnum, time_t time);
		void				skill_learn_wait_more_time_message(uint32_t ms);

		int					compute_skill(uint32_t vnum_value, character * victim, uint8_t skill_level_value = 0);
		int					compute_skill_at_position(uint32_t vnum_value, const pixel_position& pos_target, uint8_t skill_level_value = 0);
		void				compute_skill_points();

		void				set_skill_group(uint8_t skill_group_value); 
		uint8_t				skill_group() const		{ return points_.skill_group; }

		int					compute_cooltime(int time);

		void				give_random_skill_book();

		void				disable_cooltime();
		bool				learn_skill_by_book(uint32_t skill_vnum, uint8_t prob = 0);
		bool				learn_grand_master_skill(uint32_t skill_vnum);

	private:
		bool				disable_cooltime_;
		uint32_t				last_skill_time_;	///< Last time a skill was used (millisecond).
		// End of Skill

		// MOB_SKILL
	public:
		bool				has_mob_skill() const;
		size_t				count_mob_skill() const;
		const mob_skill_info * mob_skill(unsigned int idx) const;
		bool				can_use_mob_skill(unsigned int idx) const;
		bool				use_mob_skill(unsigned int idx);
		void				reset_mob_skill_cooltime();
	protected:
		uint32_t				mob_skill_cooltime_[MOB_SKILL_MAX_NUM];
		// END_OF_MOB_SKILL

		// for SKILL_MUYEONG
	public:
		void				start_muyeong_event();
		void				stop_muyeong_event();

	private:
		event_ptr				muyeong_event_;

		// for SKILL_CHAIN lighting
	public:
		int					chain_lightning_index() const { return chain_lighting_index_; }
		void				inc_chain_lightning_index() { ++chain_lighting_index_; }
		void				add_chain_lightning_except(character * ch) { except_chain_lighting_.insert(ch); }
		void				reset_chain_lightning_index() { chain_lighting_index_ = 0; except_chain_lighting_.clear(); }
		int					chain_lightning_max_count() const;
		const character_set& chain_lighting_except() const { return except_chain_lighting_; }

	private:
		int					chain_lighting_index_;
		character_set except_chain_lighting_;

		// for SKILL_EUNHYUNG
	public:
		void				set_affected_eunhyung();
		void				clear_affected_eunhyung() { affected_eunhyung_level_ = 0; }
		bool				affected_eunhyung() const { return affected_eunhyung_level_; }

	private:
		uint32_t				affected_eunhyung_level_;

		//
		// Skill levels
		//
	protected:
		std::unique_ptr<player_skill[]>	skill_levels_;
		std::unordered_map<uint8_t, int>		skill_damage_bonus_;
		std::map<int, skill_use_info>	skill_use_info_;

		////////////////////////////////////////////////////////////////////////////////////////
		// AI related
	public:
		void			assign_triggers(const mob_table * table);
		character *		victim() const;	// Returns the target to attack
		void			set_victim(character * victim);
		character *		nearest_victim(character * chr);
		character *		protege() const;	// Returns the target to protect

		bool			follow(character * chr, float min_distance = 150.0f);
		bool			return_home();
		bool			is_guard_npc() const;
		bool			is_change_attack_position(character * target) const;
		void			reset_change_attack_position_time() { last_change_attack_position_time_ = get_dword_time() - AI_CHANGE_ATTACK_POISITION_TIME_NEAR;}
		void			set_change_attack_position_time() { last_change_attack_position_time_ = get_dword_time();}

		void			on_click(character * chr_causer);

		vid				vid_victim;

	protected:
		uint32_t			last_change_attack_position_time_;
		trigger		trigger_on_click_;
		// End of AI

		////////////////////////////////////////////////////////////////////////////////////////
		// Target
	protected:
		character *				chr_target_;		// My target
		character_set	set_pk_chr_targeted_by_;	// Those who have me as their target

	public:
		void				set_target(character * chr_target);
		void				broadcast_target_packet();
		void				clear_target();
		void				check_target();
		character *			target() const { return chr_target_; }

		////////////////////////////////////////////////////////////////////////////////////////
		// Safebox
	public:
		int					safebox_size() const;
		void				query_safebox_size();
		void				set_safebox_size(int size);

		safebox *			get_safebox() const;
		void				load_safebox(int size_value, uint32_t gold, int item_count, player_item * items);
		void				change_safebox_size(uint8_t size_value);
		void				close_safebox();

		/// Safebox open request
		/**
		 * @param [in]	pszPassword safebox password of 1 to 6 characters
		 *
		 * Requests the DB to open the safebox.
		 * The safebox cannot be opened twice, and cannot be opened within 10 seconds of last closing it.
		 */
		void				req_safebox_load(const char* password_value);

		/// Cancels a safebox open request
		/**
		 * After calling ReqSafeboxLoad without CloseSafebox, calling this function allows the safebox to be opened.
		 * If the safebox open request gets a failure response from the DB server, this function allows requesting again.
		 */
		void				cancel_safebox_load( void ) { opening_safebox_ = false; }

		void				set_mall_load_time(int t) { mall_load_time_ = t; }
		int					mall_load_time() const { return mall_load_time_; }

		safebox *			mall() const;
		void				load_mall(int item_count, player_item * items);
		void				close_mall();

		void				set_safebox_open_position();
		float				distance_from_safebox_open() const;

	protected:
		std::unique_ptr<safebox>	safebox_;
		int					safebox_size_;
		int					safebox_load_time_;
		bool				opening_safebox_;	///< Whether the safebox is being requested or open; true means requested or open.

		std::unique_ptr<safebox>	mall_;
		int					mall_load_time_;

		pixel_position		pos_safebox_open_;

		////////////////////////////////////////////////////////////////////////////////////////

		////////////////////////////////////////////////////////////////////////////////////////
		// Mounting
	public:
		void				mount_vnum(uint32_t vnum);
		uint32_t				get_mount_vnum() const { return mount_vnum_; }
		uint32_t				last_mount_time() const { return mount_time_; }

		bool				can_use_horse_skill();

		// Horse
		void		set_horse_level(int level) override;

		bool		start_riding() override;
		bool		stop_riding() override;

		uint32_t		my_horse_vnum() const override;

		void		horse_die() override;
		bool		revive_horse() override;

		void		send_horse_info() override;
		void		clear_horse_info() override;

		void				horse_summon(bool summon, bool from_far = false, uint32_t vnum = 0, const char* pet_name = nullptr);

		character *			horse() const			{ return ch_horse_; }	 // Currently summoned horse
		character *			rider() const; // rider on horse
		void				set_rider(character * ch);

		bool				is_riding() const;

	public:
		pet_system*			get_pet_system()				{ return pet_system_.get(); }

	protected:
		std::unique_ptr<pet_system>	pet_system_;

	public:

	protected:
		character *			ch_horse_;
		character *			ch_rider_;

		uint32_t				mount_vnum_;
		uint32_t				mount_time_;

		uint8_t				send_horse_level_;
		uint8_t				send_horse_health_grade_;
		uint8_t				send_horse_stamina_grade_;

		////////////////////////////////////////////////////////////////////////////////////////
		// Detailed Log
	public:
		void				detail_log() { detail_log_ = !detail_log_; }
		void				toggle_monster_log();
		void				monster_log(const char* format, ...);
	private:
		bool				detail_log_;
		bool				monster_log_;

		////////////////////////////////////////////////////////////////////////////////////////
		// Empire

	public:
		void 				set_empire(uint8_t empire);
		uint8_t				empire() const { return empire_; }

	protected:
		uint8_t				empire_;

		////////////////////////////////////////////////////////////////////////////////////////
		// Regen
	public:
		void				set_regen(regen * regen_value);

	protected:
		pixel_position			pos_regen_;
		regen *				regen_;
		size_t				regen_id_; // to help dungeon regen identification
		// End of Regen

		////////////////////////////////////////////////////////////////////////////////////////
		// Resists & Proofs
	public:
		bool				cannot_move_by_affect() const;	// Is it unable to move due to a specific effect?
		bool				is_immune(uint32_t immune_flag_value);
		void				set_immune_flag(uint32_t dw) { points_instant_.immune_flag = dw; }

	protected:
		void				apply_mob_attribute(const mob_table* table);
		// End of Resists & Proofs

		////////////////////////////////////////////////////////////////////////////////////////
		// QUEST
		// 
	public:
		void				set_quest_npcid(uint32_t vid);
		uint32_t				quest_npcid() const { return quest_npcvid_; }
		character *			quest_npc() const;

		void				set_quest_item_ptr(item * item);
		void				clear_quest_item_ptr();
		item *				quest_item_ptr() const;

		void				set_quest_by(uint32_t quest_vnum)	{ quest_by_vnum_ = quest_vnum; }
		uint32_t				quest_by() const			{ return quest_by_vnum_; }

		int					quest_flag(const std::string& flag) const;
		void				set_quest_flag(const std::string& flag, int value);

		void				confirm_with_msg(const char* msg_value, int timeout_value, uint32_t request_pid_value);

	private:
		uint32_t				quest_npcvid_;
		uint32_t				quest_by_vnum_;
		item *				quest_item_;

		// Events
	public:
		bool				start_state_machine(int next_pulse = 1);
		void				stop_state_machine();
		void				update_state_machine(uint32_t pulse);
		void				set_next_state_pulse(int next_pulse);

		// Character instance update function. Previously, through an odd inheritance structure, CFSM::Update or UpdateStateMachine was called; added a separate update function.
		void				update_character(uint32_t pulse);

	protected:
		uint32_t				next_state_pulse_;

		// Marriage
	public:
		character *			marry_partner() const;
		void				set_marry_partner(character * ch);
		int					marriage_bonus(uint32_t item_vnum, bool sum = true);

		void				set_wedding_map(marriage::wedding_map* map);

	private:
		marriage::wedding_map* wedding_map_;
		character *			chr_married_;

		// Warp Character
	public:
		void				start_warp_npc_event();

	public:
		void				start_save_event();
		void				start_recovery_event();
		void				start_check_speed_hack_event();
		void				start_destroy_when_idle_event();

		event_ptr				dead_event;
		event_ptr				stun_event;
		event_ptr				save_event;
		event_ptr				recovery_event;
		event_ptr				timed_event;
		event_ptr				fishing_event;
		event_ptr				affect_event;
		event_ptr				poison_event;
		event_ptr				fire_event;
		event_ptr				warp_npc_event;
		//DELAYED_WARP
		//END_DELAYED_WARP

		// MINING
		event_ptr				mining_event;
		// END_OF_MINING
		event_ptr				warp_event;
		event_ptr				check_speed_hack_event;
		event_ptr				destroy_when_idle_event;
		event_ptr				pet_system_update_event;

		bool is_warping() const { return warp_event ? true : false; }

		bool				has_poisoned;

		const mob *		mob_data;
		std::unique_ptr<mob_instance>	mob_inst;

		std::map<int, event_ptr> mob_skill_event;

		friend struct func_splash_damage;
		friend struct func_splash_affect;
		friend class func_shoot;

	public:
		int				premium_remain_seconds(uint8_t type) const;

	private:
		int				premium_times_[PREMIUM_MAX_NUM];

		// CHANGE_ITEM_ATTRIBUTES
		static const uint32_t		default_change_item_attr_cycle_;	///< Default interval for item attribute changes
		static const char		last_change_item_attr_flag_[];		///< Quest Flag name for the time of the last item attribute change
		static const char		change_item_attr_cycle_flag_[];		///< Quest Flag name for the item attribute change interval
		// END_OF_CHANGE_ITEM_ATTRIBUTES

		// NEW_HAIR_STYLE_ADD
	public :
		bool item_process_hair(item * item, int dest_cell);
		// END_NEW_HAIR_STYLE_ADD

	public :
		void clear_skill();
		void clear_sub_skill();

		// RESET_ONE_SKILL
		bool reset_one_skill(uint32_t vnum);
		// END_RESET_ONE_SKILL

	private :
		void send_damage_packet(character * attacker, int damage_value, uint8_t damage_flag);

	// ARENA
	private :
		arena *arena_;
		bool arena_observer_;
		int potion_limit_;

	public :
		void 	set_arena(arena* arena_value) { arena_ = arena_value; }
		void	set_arena_observer_mode(bool flag) { arena_observer_ = flag; }

		arena* get_arena() const { return arena_; }
		bool	arena_observer_mode() const { return arena_observer_; }

		void	set_potion_limit(int count) { potion_limit_ = count; }
		int		potion_limit() const { return potion_limit_; }
	// END_ARENA

		//PREVENT_TRADE_WINDOW
	public:
		bool	is_open_safebox() const { return is_open_safebox_ ? true : false; }
		void 	set_open_safebox(bool b) { is_open_safebox_ = b; }

		int		safebox_load_time() const { return safebox_load_time_; }
		void	set_safebox_load_time() { safebox_load_time_ = thecore_pulse(); }
		//END_PREVENT_TRADE_WINDOW
	private:
		bool	is_open_safebox_;

	public:
		int		skill_power_by_level(int level, bool mob = false) const;
		
		//PREVENT_REFINE_HACK
		int		get_refine_time() const { return refine_time; }
		void	set_refine_time() { refine_time = thecore_pulse(); } 
		int		refine_time;
		//END_PREVENT_REFINE_HACK

		//END_RESTRICT_USE_SEED_OR_MOONBOTTLE
		
		//PREVENT_PORTAL_AFTER_EXCHANGE
		int		get_exchange_time() const { return exchange_time; }
		void	set_exchange_time() { exchange_time = thecore_pulse(); }
		int		exchange_time;
		//END_PREVENT_PORTAL_AFTER_EXCHANGE
		
		int 	my_shop_time;
		int		get_my_shop_time() const	{ return my_shop_time; }
		void	set_my_shop_time() { my_shop_time = thecore_pulse(); }

		// Check to prevent hacks.
		bool	is_hack(bool send_msg = true, bool check_shop_owner = true, int limittime = portal_limit_time);

		// MONARCH
		bool	is_monarch() const;
		// END_MONARCH

		enum monarch_cooltime_values
		{
			MC_HEAL = 10,
			MC_WARP	= 60,
			MC_TRANSFER = 60,
			MC_TAX = (60 * 60 * 24 * 7),
			MC_SUMMON = (60 * 60),
		};

		enum monarch_index
		{ 
			MI_HEAL = 0,
			MI_WARP,
			MI_TRANSFER,
			MI_TAX,
			MI_SUMMON,
			MI_MAX
		};

		uint32_t monarch_cooltime[MI_MAX];
		uint32_t monarch_cooltimelimit[MI_MAX];

		void  init_mc();
		uint32_t mc(enum monarch_index e) const;
		void set_mc(enum monarch_index e);
		bool is_mcok(enum monarch_index e) const;
		uint32_t mcl(enum monarch_index e) const;
		uint32_t mcl_time(enum monarch_index e) const;

	public:
		bool item_process_polymorph(item * item);

		// by mhh
		item **	cube_item() { return points_instant_.cube_items; }
		bool is_cube_open () const	{ return (points_instant_.cube_npc?true:false); }
		void set_cube_npc(character * npc)	{ points_instant_.cube_npc = npc; }
		bool can_do_cube() const;

	public:

	private:
		int		deposit_pulse_;

	public:
		void	update_deposit_pulse();
		bool	can_deposit() const;

	private:
		void	open_private_shop();

	public:
		struct attacked_log_entry
		{
			uint32_t 	pid;
			uint32_t	attacked_time;
			
			attacked_log_entry() : pid(0), attacked_time(0)
			{
			}
		};

		attack_log_entry	attack_log;
		attacked_log_entry attacked_log;
		int			speed_hack_count;

	private :
		std::string new_name_;

	public :
		const std::string new_name() const { return this->new_name_; }
		void set_new_name(const std::string name) { this->new_name_ = name; }

	public :
		void go_home();

	private :
		std::set<uint32_t>	known_guild_;

	public :
		void send_guild_name(guild* guild_value);
		void send_guild_name(uint32_t guild_id);

	private :
		uint32_t log_off_interval_;

	public :
		uint32_t log_off_interval() const { return log_off_interval_; }

	public:
		bool un_equip_special_ride_unique_item ();

		bool can_warp () const;

	private:
		uint32_t last_gold_drop_time_;

		// bool GetHackShieldCheckMode() const { return m_HackShieldCheckMode; }
		// void SetHackShieldCheckMode(bool m) { m_HackShieldCheckMode = m; }

	public:
		void auto_recovery_item_process (const affect_types);

	public:
		void buff_on_attr_add_buffs_from_item(item * item_value);
		void buff_on_attr_remove_buffs_from_item(item * item_value);

	private:
		void buff_on_attr_value_change(uint8_t type, uint8_t old_value, uint8_t new_value);
		void buff_on_attr_clear_all();

		typedef std::map <uint8_t, std::unique_ptr<buff_on_attributes>> map_buff_on_attrs;
		map_buff_on_attrs map_buff_on_attrs_;
		// Invincible: for smoother testing.
	public:
		void set_armada() { cannot_dead_ = true; }
		void reset_armada() { cannot_dead_ = false; }
	private:
		bool cannot_dead_;

	private:
		bool is_pet_;
	public:
		void set_pet() { is_pet_ = true; }
		bool is_pet() { return is_pet_; }

	//Final damage adjustment.
	private:
		float att_mul_;
		float dam_mul_;
	public:
		float att_mul() { return this->att_mul_; }
		void set_att_mul(float new_att_mul) {this->att_mul_ = new_att_mul; }
		float dam_mul() { return this->dam_mul_; }
		void set_dam_mul(float new_dam_mul) {this->dam_mul_ = new_dam_mul; }

	private:
		bool is_valid_item_position(item_pos pos) const;

	public:
		// Set when the guild NPC quest asks for a guild name (game.request_make_guild); the
		// answer must carry it, and it is good for one answer.
		void		set_guild_creation_key(uint32_t key) { guild_creation_key_ = key; }
		uint32_t	guild_creation_key() const { return guild_creation_key_; }

	private:
		uint32_t	guild_creation_key_;

		//Temporary storage of German gift feature packet
	private:
		unsigned int item_award_vnum_;
		char		 item_award_cmd_[20];
	public:
		unsigned int item_award_vnum() { return item_award_vnum_; }
		char*		 item_award_cmd() { return item_award_cmd_;	  }
		//bool		 GetItemAward_flag() { return itemAward_flag; }
		void		 set_item_award_vnum(unsigned int vnum) { item_award_vnum_ = vnum; }
		void		 set_item_award_cmd(const char* cmd) { strlcpy(item_award_cmd_, cmd, sizeof(item_award_cmd_)); }
		//void		 SetItemAward_flag(bool flag) { itemAward_flag = flag; }

	public:
		//Dragon soul
		
		// DragonSoul_Initialize must not be called before the character's affects and quests are loaded.
		// Affects are loaded last, so it is called from LoadAffect.
		void	dragon_soul_initialize();

		bool	dragon_soul_is_qualified() const;
		void	dragon_soul_give_qualification();

		int		dragon_soul_get_active_deck() const;
		bool	dragon_soul_is_deck_activated() const;
		bool	dragon_soul_activate_deck(int deck_idx);

		void	dragon_soul_deactivate_all();
		// Must be called before ClearItem.
		// Because....
		// Each time a dragon soul stone is deactivated, it checks whether the deck still has an active one,
		// and if there are no active dragon soul stones, removes the character's dragon soul affect and active state.
		// 
		// But ClearItem unequips every item the character is wearing,
		// so the dragon soul affect is removed, and in the end the dragon soul is not activated on login.
		// (When unequipping, there is no way to know whether we are logging out.)
		// Only deactivates the dragon soul stones and leaves the character's dragon soul deck active state untouched.
		void	dragon_soul_clean_up();
		// Dragon soul refine window
	public:
		bool		dragon_soul_refine_window_open(entity * entity_value);
		bool		dragon_soul_refine_window_close();
		entity *	dragon_soul_refine_window_get_opener() { return  points_instant_.dragon_soul_refine_window_opener; }
		bool		dragon_soul_refine_window_can_refine();

	private:
		// To defend against hacks that abuse SyncPosition to send other users to odd places,
		// record when SyncPosition happens.
		timeval		tv_last_sync_time_;
		int			sync_hack_count_;
	public:
		void			set_last_sync_time(const timeval &tv) { memcpy(&tv_last_sync_time_, &tv, sizeof(timeval)); }
		const timeval&	last_sync_time() { return tv_last_sync_time_; }
		void			set_sync_hack_count(int count) { sync_hack_count_ = count;}
		int				sync_hack_count() { return sync_hack_count_; }
	public:
		int					last_drop_time;
		int					count_drops;
		void clear_pm_counter(void)       { pm_counter_ = 0;      } 
		void increase_pm_counter(void)    { pm_counter_++;        }
		void set_last_pm_pulse(void);
		int  pm_counter(void)   const { return pm_counter_;   }
		int  last_pm_pulse(void) const { return last_pm_pulse_; }
	protected:
		int last_pm_pulse_;
		int pm_counter_;
};

sex get_sex(character * ch);

extern bool is_summonable_zone(int map_index);
extern character * find_victim(character * chr, int max_distance);

void character_set_bgm_volume_enable();
void character_add_bgm_info(unsigned index, const char* name, float vol);
}
