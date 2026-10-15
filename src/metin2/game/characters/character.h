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
	POINT_NONE,
	POINT_LEVEL,
	POINT_VOICE,
	POINT_EXP,
	POINT_NEXT_EXP,
	POINT_HP,
	POINT_MAX_HP,
	POINT_SP,
	POINT_MAX_SP,
	POINT_STAMINA,
	POINT_MAX_STAMINA,

	POINT_GOLD,
	POINT_ST,
	POINT_HT,
	POINT_DX,
	POINT_IQ,
	POINT_DEF_GRADE,
	POINT_ATT_SPEED,
	POINT_ATT_GRADE,
	POINT_MOV_SPEED,
	POINT_CLIENT_DEF_GRADE,
	POINT_CASTING_SPEED,
	POINT_MAGIC_ATT_GRADE,
	POINT_MAGIC_DEF_GRADE,
	POINT_EMPIRE_POINT,
	POINT_LEVEL_STEP,
	POINT_STAT,
	POINT_SUB_SKILL,
	POINT_SKILL,
	POINT_WEAPON_MIN,
	POINT_WEAPON_MAX,
	POINT_PLAYTIME,
	POINT_HP_REGEN,
	POINT_SP_REGEN,

	POINT_BOW_DISTANCE,

	POINT_HP_RECOVERY,
	POINT_SP_RECOVERY,

	POINT_POISON_PCT,
	POINT_STUN_PCT,
	POINT_SLOW_PCT,
	POINT_CRITICAL_PCT,
	POINT_PENETRATE_PCT,
	POINT_CURSE_PCT,

	POINT_ATTBONUS_HUMAN,
	POINT_ATTBONUS_ANIMAL,
	POINT_ATTBONUS_ORC,
	POINT_ATTBONUS_MILGYO,
	POINT_ATTBONUS_UNDEAD,
	POINT_ATTBONUS_DEVIL,
	POINT_ATTBONUS_INSECT,
	POINT_ATTBONUS_FIRE,
	POINT_ATTBONUS_ICE,
	POINT_ATTBONUS_DESERT,
	POINT_ATTBONUS_MONSTER,
	POINT_ATTBONUS_WARRIOR,
	POINT_ATTBONUS_ASSASSIN,
	POINT_ATTBONUS_SURA,
	POINT_ATTBONUS_SHAMAN,
	POINT_ATTBONUS_TREE,

	POINT_RESIST_WARRIOR,
	POINT_RESIST_ASSASSIN,
	POINT_RESIST_SURA,
	POINT_RESIST_SHAMAN,

	POINT_STEAL_HP,
	POINT_STEAL_SP,

	POINT_MANA_BURN_PCT,


	POINT_DAMAGE_SP_RECOVER,

	POINT_BLOCK,
	POINT_DODGE,

	POINT_RESIST_SWORD,
	POINT_RESIST_TWOHAND,
	POINT_RESIST_DAGGER,
	POINT_RESIST_BELL,
	POINT_RESIST_FAN,
	POINT_RESIST_BOW,
	POINT_RESIST_FIRE,
	POINT_RESIST_ELEC,
	POINT_RESIST_MAGIC,
	POINT_RESIST_WIND,

	POINT_REFLECT_MELEE,

	POINT_REFLECT_CURSE,
	POINT_POISON_REDUCE,

	POINT_KILL_SP_RECOVER,
	POINT_EXP_DOUBLE_BONUS,
	POINT_GOLD_DOUBLE_BONUS,
	POINT_ITEM_DROP_BONUS,

	POINT_POTION_BONUS,
	POINT_KILL_HP_RECOVERY,

	POINT_IMMUNE_STUN,
	POINT_IMMUNE_SLOW,
	POINT_IMMUNE_FALL,

	POINT_PARTY_ATTACKER_BONUS,
	POINT_PARTY_TANKER_BONUS,

	POINT_ATT_BONUS,
	POINT_DEF_BONUS,

	POINT_ATT_GRADE_BONUS,
	POINT_DEF_GRADE_BONUS,
	POINT_MAGIC_ATT_GRADE_BONUS,
	POINT_MAGIC_DEF_GRADE_BONUS,

	POINT_RESIST_NORMAL_DAMAGE,

	POINT_HIT_HP_RECOVERY,
	POINT_HIT_SP_RECOVERY,
	POINT_MANASHIELD,

	POINT_PARTY_BUFFER_BONUS,
	POINT_PARTY_SKILL_MASTER_BONUS,

	POINT_HP_RECOVER_CONTINUE,
	POINT_SP_RECOVER_CONTINUE,

	POINT_STEAL_GOLD,
	POINT_POLYMORPH,
	POINT_MOUNT,

	POINT_PARTY_HASTE_BONUS,
	POINT_PARTY_DEFENDER_BONUS,
	POINT_STAT_RESET_COUNT,

	POINT_HORSE_SKILL,

	POINT_MALL_ATTBONUS,
	POINT_MALL_DEFBONUS,
	POINT_MALL_EXPBONUS,
	POINT_MALL_ITEMBONUS,
	POINT_MALL_GOLDBONUS,

	POINT_MAX_HP_PCT,
	POINT_MAX_SP_PCT,

	POINT_SKILL_DAMAGE_BONUS,
	POINT_NORMAL_HIT_DAMAGE_BONUS,

	POINT_SKILL_DEFEND_BONUS,
	POINT_NORMAL_HIT_DEFEND_BONUS,

	POINT_PC_BANG_EXP_BONUS,
	POINT_PC_BANG_DROP_BONUS,
	POINT_RAMADAN_CANDY_BONUS_EXP,

	POINT_ENERGY = 128,

	// Not used by the server; the client keeps the energy end time as a point.
	POINT_ENERGY_END_TIME = 129,

	POINT_COSTUME_ATTR_BONUS = 130,
	POINT_MAGIC_ATT_BONUS_PER = 131,
	POINT_MELEE_MAGIC_ATT_BONUS_PER = 132,

	POINT_RESIST_ICE = 133,
	POINT_RESIST_EARTH = 134,
	POINT_RESIST_DARK = 135,

	POINT_RESIST_CRITICAL = 136,
	POINT_RESIST_PENETRATE = 137,

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

struct dynamic_character_ptr {
	dynamic_character_ptr() : is_pc(false), id(0) {}

	auto get() const -> character *; 
	void reset() {
		is_pc = false;
		id = 0;
	}

	auto operator=(character * character) -> dynamic_character_ptr&;
	operator character *() const {
		return get();
	}

	bool is_pc;
	uint32_t id;
};

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

	item *			ds_items[DRAGON_SOUL_INVENTORY_MAX_NUM];
	uint16_t			ds_item_grid[DRAGON_SOUL_INVENTORY_MAX_NUM];

	item *			cube_items[CUBE_MAX_NUM];
	character *		cube_npc;

	character *			battle_victim;

	uint8_t			gm_level;

	uint8_t			base_part;

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

	auto    hit_once(uint32_t vnum = 0) -> bool;

	auto    use_skill(bool is_grand_master_value, uint32_t vid_value, uint32_t cooltime, int splashcount = 1, int hitcount = -1, int range_value = -1) -> bool;
	auto   main_target_vid() const -> uint32_t	{ return vid; }
	void    set_main_target_vid(uint32_t vid_value) { vid=vid_value; }
};

class affect;


class mob;
class mob_instance;
struct mob_skill_info;

extern auto get_skill_power_by_level_from_type(int job, int skillgroup, int skilllevel) -> int;


class character : public entity, public fsm, public horse_rider
{
	protected:
		void	encode_insert_packet(entity * entity) override;
		void	encode_remove_packet(entity * entity) override;

	public:
		auto			find_character_in_view(const char * name_value, bool find_pc_only) -> character *;
		void				update_packet();
		int				analyze_protect; 
		int				analyze_protect_count;

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
		void				state_idle_monster();
		void				state_idle_stone();
		void				state_idle_npc();

	public:
		auto ai_flag() const -> uint32_t	{ return points_instant_.ai_flag; }
	
		void				set_aggressive();
		auto				is_aggressive() const -> bool;

		void				set_coward();
		auto				is_coward() const -> bool;
		void				coward_escape();

		auto				is_no_attack_shinsu() const -> bool;

		auto				is_no_attack_chunjo() const -> bool;

		auto				is_no_attack_jinno() const -> bool;

		auto				is_attack_mob() const -> bool;

		virtual void			begin_state_empty();
		virtual void			end_state_empty() {}

		void				restart_at_same_pos();

	protected:
		uint32_t				state_duration_;

	public:
		character();
		~character() override;

		void			create(const char * name, uint32_t vid, bool is_pc);
		void			destroy();

		void			disconnect(const char * reason);

	protected:
		void			initialize();

	public:
		auto			player_id() const -> uint32_t	{ return player_id_; }

		void			set_player_proto(const player_table * t);
		void			create_player_proto(player_table & tab);

		void			set_proto(const mob * mob_value);
		auto			race_num() const -> uint16_t;

		void			save();
		void			save_real();
		void			flush_delayed_save_item();

		auto	name() const -> const char *;
		auto		get_vid() const -> const vid &		{ return vid_;		}

		void			set_name(const std::string& name) { name_ = name; }

		void			set_race(uint8_t race);
		auto			change_sex() -> bool;

		auto			aid() const -> uint32_t;
		auto				change_empire_count() const -> int;
		void			set_change_empire_count();
		auto				change_empire(uint8_t empire_value) -> int;

		auto			job() const -> uint8_t;
		auto			get_char_type() const -> uint8_t;

		auto			is_pc() const -> bool		{ return get_desc() ? true : false; }
		auto			is_npc()	const -> bool		{ return char_type_ != CHAR_TYPE_PC; }
		auto			is_monster()	const -> bool	{ return char_type_ == CHAR_TYPE_MONSTER; }
		auto			is_stone() const -> bool		{ return char_type_ == CHAR_TYPE_STONE; }
		auto			is_door() const -> bool		{ return char_type_ == CHAR_TYPE_DOOR; } 
		auto			is_building() const -> bool	{ return char_type_ == CHAR_TYPE_BUILDING;  }
		auto			is_warp() const -> bool		{ return char_type_ == CHAR_TYPE_WARP; }
		auto			is_goto() const -> bool		{ return char_type_ == CHAR_TYPE_GOTO; }

		auto			last_shout_pulse() const -> uint32_t	{ return points_instant_.last_shout_pulse; }
		void			set_last_shout_pulse(uint32_t pulse) { points_instant_.last_shout_pulse = pulse; }
		auto				level() const -> int		{ return points_.level;	}
		void			set_level(uint8_t level);

		auto			gm_level() const -> uint8_t;
		auto 			is_gm() const -> bool;
		void			set_gm_level(); 

		auto			get_exp() const -> uint32_t		{ return points_.exp;	}
		void			set_exp(uint32_t exp)	{ points_.exp = exp;	}
		auto			next_exp() const -> uint32_t;
		auto		distribute_exp() -> character *;
		void			distribute_hp(character * killer);
		void			distribute_sp(character * killer, int method=0);

		void			set_position(int pos);
		auto			is_position(int pos) const -> bool	{ return points_instant_.position == pos ? true : false; }
		auto				get_position() const -> int		{ return points_instant_.position; }

		void			set_part(uint8_t part_pos, uint16_t val);
		auto			part(uint8_t part_pos) const -> uint16_t;
		auto			original_part(uint8_t part_pos) const -> uint16_t;

		void			set_hp(int hp)		{ points_.hp = hp; }
		auto				hp() const -> int		{ return points_.hp; }

		void			set_sp(int sp)		{ points_.sp = sp; }
		auto				sp() const -> int		{ return points_.sp; }

		void			set_stamina(int stamina)	{ points_.stamina = stamina; }
		auto				stamina() const -> int		{ return points_.stamina; }

		void			set_max_hp(int val)	{ points_instant_.max_hp = val; }
		auto				max_hp() const -> int	{ return points_instant_.max_hp; }

		void			set_max_sp(int val)	{ points_instant_.max_sp = val; }
		auto				max_sp() const -> int	{ return points_instant_.max_sp; }

		void			set_max_stamina(int val)	{ points_instant_.max_stamina = val; }
		auto				max_stamina() const -> int	{ return points_instant_.max_stamina; }

		void			set_random_hp(int v)	{ points_.random_hp = v; }
		void			set_random_sp(int v)	{ points_.random_sp = v; }

		auto				hp_pct() const -> int;

		void			set_real_point(uint8_t type, int val);
		auto				real_point(uint8_t type) const -> int;

		void			set_point(uint8_t type, int val);
		auto				point(uint8_t type) const -> int;
		auto				limit_point(uint8_t type) const -> int;
		auto				polymorph_point(uint8_t type) const -> int;

		auto	get_mob_table() const -> const mob_table &;
		auto				get_mob_rank() const -> uint8_t;
		auto				mob_battle_type() const -> uint8_t;
		auto				mob_damage_min() const -> uint32_t;
		auto				mob_damage_max() const -> uint32_t;
		auto				mob_attack_range() const -> uint16_t;
		auto				mob_drop_item_vnum() const -> uint32_t;
		auto				mob_damage_multiply() const -> float;

		auto			is_berserker() const -> bool;
		auto			is_berserk() const -> bool;
		void			set_berserk(bool mode);

		auto			is_stone_skinner() const -> bool;

		auto			is_god_speeder() const -> bool;
		auto			is_god_speed() const -> bool;
		void			set_god_speed(bool mode);

		auto			is_death_blower() const -> bool;
		auto			is_death_blow() const -> bool;

		auto			is_reviver() const -> bool;
		auto			has_reviver_in_party() const -> bool;
		auto			is_revive() const -> bool;
		void			set_revive(bool mode);

		auto			is_race_flag(uint32_t bit) const -> bool;
		auto			is_summon_monster() const -> bool;
		auto			summon_vnum() const -> uint32_t;

		auto			polymorph_item_vnum() const -> uint32_t;
		auto			monster_drain_sp_point() const -> uint32_t;

		void			main_character_packet();

		void			compute_points();
		void			compute_battle_points();
		void			point_change(uint8_t type, int amount, bool amount_value = false, bool broadcast = false);
		void			points_packet();
		void			apply_point(uint8_t apply_type, int val);
		void			check_maximum_points();

		auto			show(int32_t map_index, int32_t x, int32_t y, int32_t z = INT32_MAX, bool show_spawn_motion = false) -> bool;

		void			sitdown(int is_ground);
		void			standup();

		void			set_rotation(float rot_value);
		void			set_rotation_to_xy(int32_t x_value, int32_t y_value);
		auto			rotation() const -> float	{ return points_instant_.rot; }

		void			chat_packet(uint8_t type, const char *format, ...);
		void			monster_chat(uint8_t monster_chat_type);
		void			send_greet_message();

		void			reset_point(int lv);

		void			set_block_mode(uint8_t flag);
		void			set_block_mode_force(uint8_t flag);
		auto			is_block_mode(uint8_t flag) const -> bool	{ return (points_instant_.block_mode & flag)?true:false; }

		auto			is_polymorphed() const -> bool		{ return polymorph_race_>0; }
		auto			is_poly_maintain_stat() const -> bool	{ return poly_maintain_stat_; }
		void			set_polymorph(uint32_t race_num, bool maintain_stat = false);
		auto			polymorph_vnum() const -> uint32_t	{ return polymorph_race_; }
		auto				polymorph_power() const -> int;

		void			fishing();
		void			fishing_take();

		void			mining(character * ch_load);
		void			mining_cancel();
		void			mining_take();

		void			reset_play_time(uint32_t time_remain = 0);

		void			create_fly(uint8_t type_value, character * victim);

		void			reset_chat_counter();
		auto			increase_chat_counter() -> uint8_t;
		auto			chat_counter() const -> uint8_t;

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


	public:
		auto			is_walking() const -> bool			{ return now_walking_ || stamina()<=0; }
		void			set_walking(bool walk_flag)	{ walking_=walk_flag; }
		void			set_now_walking(bool walk_flag);	
		void			reset_walking()			{ set_now_walking(walking_); }

		auto			go_to(int32_t x_value, int32_t y_value) -> bool;
		void			stop();

		auto			can_move() const -> bool;

		void			sync_packet();
		auto			sync(int32_t x, int32_t y) -> bool;
		auto			move(int32_t x_value, int32_t y_value) -> bool;
		void			on_move(bool is_attack = false);
		auto			get_motion_mode() const -> uint32_t;
		auto			move_motion_speed() const -> float;
		auto			move_speed() const -> float;
		void			calculate_move_duration();
		void			send_move_packet(uint8_t func, uint8_t arg, uint32_t x, uint32_t y, uint32_t duration, uint32_t time=0, int rot=-1);
		auto			current_move_duration() const -> uint32_t	{ return move_duration_; }
		auto			walk_start_time() const -> uint32_t	{ return walk_start_time_; }
		auto			last_move_time() const -> uint32_t		{ return last_move_time_; }
		auto			last_attack_time() const -> uint32_t	{ return last_attack_time_; }

		void			set_last_attacked(uint32_t time);

		auto			set_sync_owner(character * ch, bool remove_from_list = true) -> bool;
		auto			is_sync_owner(character * ch) const -> bool;

		auto			warp_set(int32_t x, int32_t y, int32_t private_map_index = 0) -> bool;
		void			set_warp_location(int32_t map_index, int32_t x, int32_t y);
		void			warp_end();
		auto			warp_to_pid(uint32_t pid) -> bool;

		void			save_exit_location();
		void			exit_to_saved_location();

		void			start_stamina_consume();
		void			stop_stamina_consume();
		auto			is_stamina_consume() const -> bool;
		auto			is_stamina_half_consume() const -> bool;

		void			reset_stop_time();
		auto			stop_time() const -> uint32_t;

	protected:
		void			clear_sync();

		float			sync_time_;
		character *		chr_sync_owner_;
		character_list	lst_pk_chr_sync_owned_;

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

	public:
		void			sync_quickslot(uint8_t type_value, uint8_t old_pos, uint8_t new_pos);
		auto			get_quickslot(uint8_t pos, quickslot ** pp_slot) -> bool;
		auto			set_quickslot(uint8_t pos, quickslot & slot_value) -> bool;
		auto			del_quickslot(uint8_t pos) -> bool;
		auto			swap_quickslot(uint8_t a, uint8_t b) -> bool;
		void			chain_quickslot_item(item * item_value, uint8_t type_value, uint8_t old_pos);

	protected:
		quickslot		quickslot_[QUICKSLOT_MAX_NUM];

	public:
		void			start_affect_event();
		void			clear_affect(bool save=false);
		void			compute_affect(affect * aff, bool add);
		auto			add_affect(uint32_t type_value, uint8_t apply_on_value, int32_t apply_value_value, uint32_t flag_value, int32_t duration_value, int32_t sp_cost_value, bool override, bool is_cube = false) -> bool;
		void			refresh_affect();
		auto			remove_affect(uint32_t type) -> bool;
		auto			is_affect_flag(uint32_t aff) const -> bool;

		auto			update_affect() -> bool;
		auto				process_affect() -> int;

		void			load_affect(uint32_t count_value, packet_affect_element * elements);
		void			save_affect();

		auto			is_good_affect(uint8_t affect_type) const -> bool;

		void			remove_good_affect();
		void			remove_bad_affect();

		auto		find_affect(uint32_t type_value, uint8_t apply=APPLY_NONE) const -> affect *;
		auto affect_container() const -> const std::list<affect *> &	{ return list_pk_affect_; }
		auto			remove_affect(affect * aff) -> bool;

	protected:
		affect_flag		af_affect_flag_;
		std::list<affect *>	list_pk_affect_;

	public:
		void			set_party(party * party_value);
		auto			get_party() const -> party *	{ return party_; }

		auto			request_to_party(character * leader) -> bool;
		void			deny_to_party(character * member);
		void			accept_to_party(character * member);

		void			party_invite(character * pch_invitee);

		void			party_invite_accept(character * pch_invitee);

		void			party_invite_deny(uint32_t pid);

		auto			build_update_party_packet(packet_gc_party_update & out) -> bool;
		auto				leadership_skill_level() const -> int;

		auto			can_summon(int leader_ship) -> bool;

		void			set_party_request_event(event_ptr event) { party_request_event_ = event; }

	protected:

		void			party_join(character * leader);

		enum party_join_err_code {
			PERR_NONE		= 0,
			PERR_SERVER,
			PERR_DUNGEON,
			PERR_OBSERVER,
			PERR_LVBOUNDARY,
			PERR_LOWLEVEL,
			PERR_HILEVEL,
			PERR_ALREADYJOIN,
			PERR_PARTYISFULL,
			PERR_SEPARATOR,
			PERR_DIFFEMPIRE,
			PERR_MAX
		};

		static auto	is_party_joinable_condition(character * const pch_leader, character * const pch_guest) -> party_join_err_code;

		static auto	is_party_joinable_mutable_condition(character * const pch_leader, character * const pch_guest) -> party_join_err_code;

		party *			party_;
		uint32_t			last_dead_time_;
		event_ptr			party_request_event_;

		typedef std::map< uint32_t, event_ptr >	event_map;
		event_map		party_invite_event_map_;


	public:
		void			set_dungeon(dungeon * dungeon_value);
		auto		get_dungeon() const -> dungeon *	{ return dungeon_; }
		auto		dungeon_force() const -> dungeon *;
	protected:
		dungeon *	dungeon_;
		int			event_attr_;

	public:
		void			set_guild(guild * guild_value);
		auto			get_guild() const -> guild*	{ return guild_; }

		void			set_war_map(war_map* war_map_value);
		auto		get_war_map() const -> war_map*	{ return war_map_; }

	protected:
		guild *		guild_;
		uint32_t			under_guild_war_info_message_time_;
		war_map *		war_map_;

	public:
		auto			can_handle_item(bool skip_check_refine = false, bool skip_observer = false) -> bool;

		auto			is_item_loaded() const -> bool	{ return item_loaded_; }
		void			set_item_loaded()	{ item_loaded_ = true; }

		void			clear_item();
		void			set_item(item_pos cell_value, item * item_value);
		auto			get_item(item_pos cell_value) const -> item *;
		auto			inventory_item(uint16_t cell) const -> item *;
		auto			is_empty_item_grid(item_pos cell_value, uint8_t size, int exception_cell = -1) const -> bool;

		void			set_wear(uint8_t cell, item * item);
		auto			wear(uint8_t cell) const -> item *;

		void			use_silk_botary(void);

		void			use_silk_botary_real(const packet_myshop_pricelist_header* p);

		auto			use_item_ex(item * item, item_pos dest_cell) -> bool;
		auto			use_item(item_pos cell_value, item_pos dest_cell = NPOS) -> bool;

		auto			is_refine_through_guild() const -> bool;
		auto		refine_guild() const -> guild *;
		auto				compute_refine_fee(int cost, int multiply = 5) const -> int;
		void			pay_refine_fee(int total_money);
		void			set_refine_npc(character * ch);

		auto			refine_item(item * item_value, item * target) -> bool;
		auto			drop_item(item_pos cell_value,  uint8_t count_value=0) -> bool;
		auto			give_recall_item(item * item) -> bool;
		void			process_recall_item(item * item);

		void			effect_packet(int enum_effect_type);
		void			specific_effect_packet(const char filename[128]);

		auto			do_refine(item * item, bool money_only = false) -> bool;

		auto			do_refine_with_scroll(item * item) -> bool;
		auto			refine_information(uint8_t cell, uint8_t type_value, int additional_cell = -1) -> bool;

		void			set_refine_mode(int additional_cell = -1);
		void			clear_refine_mode();

		auto			give_item(character * victim, item_pos cell) -> bool;
		auto			can_receive_item(character * from, item * item) const -> bool;
		void			receive_item(character * from, item * item);
		auto			give_item_from_special_item_group(uint32_t group_num, std::vector <uint32_t> &item_vnums, 
							std::vector <uint32_t> &item_counts, std::vector <item *> &item_gets, int &count) -> bool;

		auto			move_item(item_pos cell_value, item_pos dest_cell, uint8_t count) -> bool;
		auto			pickup_item(uint32_t vid) -> bool;
		auto			equip_item(item * item, int candidate_cell = -1) -> bool;
		auto			unequip_item(item * item) -> bool;

		auto			can_equip_now(item * const item, const item_pos& src_cell = NPOS, const item_pos& dest_cell = NPOS) -> bool;

		auto			can_unequip_now(item * const item, const item_pos& src_cell = NPOS, const item_pos& dest_cell = NPOS) -> bool;

		auto			swap_item(uint8_t cell_value, uint8_t dest_cell) -> bool;
		auto			auto_give_item(uint32_t item_vnum, uint8_t count_value=1, int rare_pct = -1, bool msg = true) -> item *;
		void			auto_give_item(item * item, bool long_owner_ship = false);
		
		auto				empty_inventory(uint8_t size) const -> int;
		auto				empty_dragon_soul_inventory(item * item_value) const -> int;
		void			copy_dragon_soul_item_grid(std::vector<uint16_t>& dragon_soul_item_grid) const;

		auto				count_empty_inventory() const -> int;

		auto				count_specify_item(uint32_t vnum) const -> int;
		void			remove_specify_item(uint32_t vnum, uint32_t count = 1);
		auto			find_specify_item(uint32_t vnum) const -> item *;
		auto			find_item_by_id(uint32_t id) const -> item *;

		auto				count_specify_type_item(uint8_t type) const -> int;
		void			remove_specify_type_item(uint8_t type, uint32_t count = 1);

		auto			is_equip_unique_item(uint32_t item_vnum) const -> bool;

		auto			is_equip_unique_group(uint32_t group_vnum) const -> bool;

		void			send_equipment(character * ch);

	protected:

		void			send_my_shop_price_list_cmd(uint32_t item_vnum, uint32_t item_price);

		bool			no_opened_shop_;

		bool			item_loaded_;
		int				refine_additional_cell_;
		bool			under_refine_;
		uint32_t			refine_npcvid_;

	public:
		auto				gold() const -> int32_t		{ return points_.gold;	}
		void			set_gold(int32_t gold)	{ points_.gold = gold;	}
		auto			drop_gold(int32_t gold_value) -> bool;
		void			give_gold(int32_t amount);

	public:
		void			set_shop(shop * shop_value);
		auto			get_shop() const -> shop * { return shop_; }

		void			set_shop_owner(character * ch) { chr_shop_owner_ = ch; }
		auto		shop_owner() const -> character * { return chr_shop_owner_;}

		void			open_my_shop(const char * sign_value, shop_item_table * table, uint8_t item_count);
		auto			my_shop() const -> shop * { return my_shop_; }
		void			close_my_shop();

	protected:

		shop *			shop_;
		shop *			my_shop_;
		std::string		shop_sign_;
		character *		chr_shop_owner_;

	public:
		auto			exchange_start(character * victim) -> bool;
		void			set_exchange(exchange * exchange_value);
		auto		get_exchange() const -> exchange *	{ return exchange_;	}

	protected:
		exchange *		exchange_;

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

		auto				damage(character * attacker, int dam, damage_type type = DAMAGE_TYPE_NORMAL) -> bool;
		void				death_penalty(uint8_t town);
		void				revive_invisible(int dur);

		auto				attack(character * victim, uint8_t type = 0) -> bool;

		auto				can_begin_fight() const -> bool;
		void				begin_fight(character * victim);

		auto				is_stun() const -> bool;
		void				stun();
		auto				is_dead() const -> bool;
		void				dead(character * killer = nullptr, bool immediate_dead=false);

		void				reward(bool item_drop);
		void				reward_gold(character * attacker);

		auto				shoot(uint8_t type) -> bool;
		void				fly_target(uint32_t target_vid_value, int32_t x, int32_t y, uint8_t header_value);

		void				forget_my_attacker();
		void				aggregate_monster();
		void				attract_ranger();
		void				pull_monster();

		auto					arrow_and_bow(item * * ppk_bow, item * * ppk_arrow, int arrow_count = 1) -> int;
		void				use_arrow(item * arrow, uint32_t arrow_count);

		void				attacked_by_poison(character * attacker);
		void				remove_poison();

		void				attacked_by_fire(character * attacker, int amount, int count);
		void				remove_fire();

		void				update_alignment(int amount);
		auto					alignment() const -> int;

		auto					real_alignment() const -> int;
		void				show_alignment(bool show);

		void				set_killer_mode(bool is_on);
		auto				is_killer_mode() const -> bool;
		void				update_killer_mode();

		auto				pk_mode() const -> uint8_t;
		void				set_pk_mode(uint8_t pk_mode);

		void				item_drop_penalty(character * killer);

		void				update_aggr_point(character * attacker, damage_type type, int dam);

	public:
		void set_combo_sequence(uint8_t seq);
		auto combo_sequence() const -> uint8_t;

		void set_last_combo_time(uint32_t time);
		auto last_combo_time() const -> uint32_t;

		auto valid_combo_interval() const -> int;
		void set_valid_combo_interval(int interval);

		auto combo_index() const -> uint8_t;

		void increase_combo_hack_count(int k = 1);
		void reset_combo_hack_count();
		void skip_combo_attack_by_time(int interval);
		auto get_skip_combo_attack_by_time() const -> uint32_t;

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
		damage_map			map_k_damage_;
		uint32_t				killer_pid_;

		int					alignment_;
		int					real_alignment_;
		int					killer_mode_pulse_;
		uint8_t				pk_mode_;

		uint32_t				last_victim_set_time_;
		int					max_aggro_;

	public:
		void				set_stone(character * chr_stone);
		void				clear_stone();
		void				determine_drop_metin_stone();
		auto				drop_metin_stone_vnum() const -> uint32_t { return drop_metin_stone_; }
		auto				drop_metin_stone_pct() const -> uint8_t { return drop_metin_stone_pct_; }

	protected:
		character *			chr_stone_;
		character_set		set_pk_chr_spawned_by_;
		uint32_t				drop_metin_stone_;
		uint8_t				drop_metin_stone_pct_;

	public:
		enum
		{
			SKILL_UP_BY_POINT,
			SKILL_UP_BY_BOOK,
			SKILL_UP_BY_TRAIN,

			SKILL_UP_BY_QUEST,
		};

		void				skill_level_packet();
		void				skill_level_up(uint32_t vnum_value, uint8_t method = SKILL_UP_BY_POINT);
		auto				skill_level_down(uint32_t vnum_value) -> bool;
		auto				use_skill(uint32_t vnum_value, character * victim, bool use_grand_master = true) -> bool;
		void				reset_skill();
		void				set_skill_level(uint32_t vnum, uint8_t lev);
		auto					used_skill_master_type(uint32_t vnum) -> int;

		auto				is_learnable_skill(uint32_t skill_vnum) const -> bool;

		auto				check_skill_hit_count(const uint8_t skill_id, const vid target_vid) -> bool;
		auto				can_use_skill(uint32_t skill_vnum) const -> bool;
		auto				is_usable_skill_motion(uint32_t motion_index) const -> bool;
		auto					skill_level(uint32_t vnum) const -> int;
		auto					skill_master_type(uint32_t vnum) const -> int;
		auto					skill_power(uint32_t vnum, uint8_t level = 0) const -> int;

		auto				skill_next_read_time(uint32_t vnum) const -> time_t;
		void				set_skill_next_read_time(uint32_t vnum, time_t time);
		void				skill_learn_wait_more_time_message(uint32_t ms);

		auto					compute_skill(uint32_t vnum_value, character * victim, uint8_t skill_level_value = 0) -> int;
		auto					compute_skill_at_position(uint32_t vnum_value, const pixel_position& pos_target, uint8_t skill_level_value = 0) -> int;
		void				compute_skill_points();

		void				set_skill_group(uint8_t skill_group_value); 
		auto				skill_group() const -> uint8_t		{ return points_.skill_group; }

		auto					compute_cooltime(int time) -> int;

		void				give_random_skill_book();

		void				disable_cooltime();
		auto				learn_skill_by_book(uint32_t skill_vnum, uint8_t prob = 0) -> bool;
		auto				learn_grand_master_skill(uint32_t skill_vnum) -> bool;

	private:
		bool				disable_cooltime_;
		uint32_t				last_skill_time_;

	public:
		auto				has_mob_skill() const -> bool;
		auto				count_mob_skill() const -> size_t;
		auto mob_skill(unsigned int idx) const -> const mob_skill_info *;
		auto				can_use_mob_skill(unsigned int idx) const -> bool;
		auto				use_mob_skill(unsigned int idx) -> bool;
		void				reset_mob_skill_cooltime();
	protected:
		uint32_t				mob_skill_cooltime_[MOB_SKILL_MAX_NUM];

	public:
		void				start_muyeong_event();
		void				stop_muyeong_event();

	private:
		event_ptr				muyeong_event_;

	public:
		auto					chain_lightning_index() const -> int { return chain_lighting_index_; }
		void				inc_chain_lightning_index() { ++chain_lighting_index_; }
		void				add_chain_lightning_except(character * ch) { except_chain_lighting_.insert(ch); }
		void				reset_chain_lightning_index() { chain_lighting_index_ = 0; except_chain_lighting_.clear(); }
		auto					chain_lightning_max_count() const -> int;
		auto chain_lighting_except() const -> const character_set& { return except_chain_lighting_; }

	private:
		int					chain_lighting_index_;
		character_set except_chain_lighting_;

	public:
		void				set_affected_eunhyung();
		void				clear_affected_eunhyung() { affected_eunhyung_level_ = 0; }
		auto				affected_eunhyung() const -> bool { return affected_eunhyung_level_; }

	private:
		uint32_t				affected_eunhyung_level_;

	protected:
		std::unique_ptr<player_skill[]>	skill_levels_;
		std::unordered_map<uint8_t, int>		skill_damage_bonus_;
		std::map<int, skill_use_info>	skill_use_info_;

	public:
		void			assign_triggers(const mob_table * table);
		auto		victim() const -> character *;
		void			set_victim(character * victim);
		auto		nearest_victim(character * chr) -> character *;
		auto		protege() const -> character *;

		auto			follow(character * chr, float min_distance = 150.0f) -> bool;
		auto			return_home() -> bool;
		auto			is_guard_npc() const -> bool;
		auto			is_change_attack_position(character * target) const -> bool;
		void			reset_change_attack_position_time() { last_change_attack_position_time_ = get_dword_time() - AI_CHANGE_ATTACK_POISITION_TIME_NEAR;}
		void			set_change_attack_position_time() { last_change_attack_position_time_ = get_dword_time();}

		void			on_click(character * chr_causer);

		vid				vid_victim;

	protected:
		uint32_t			last_change_attack_position_time_;
		trigger		trigger_on_click_;

	protected:
		character *				chr_target_;
		character_set	set_pk_chr_targeted_by_;

	public:
		void				set_target(character * chr_target);
		void				broadcast_target_packet();
		void				clear_target();
		void				check_target();
		auto			target() const -> character * { return chr_target_; }

	public:
		auto					safebox_size() const -> int;
		void				query_safebox_size();
		void				set_safebox_size(int size);

		auto			get_safebox() const -> safebox *;
		void				load_safebox(int size_value, uint32_t gold, int item_count, player_item * items);
		void				change_safebox_size(uint8_t size_value);
		void				close_safebox();

		void				req_safebox_load(const char* password_value);

		void				cancel_safebox_load( void ) { opening_safebox_ = false; }

		void				set_mall_load_time(int t) { mall_load_time_ = t; }
		auto					mall_load_time() const -> int { return mall_load_time_; }

		auto			mall() const -> safebox *;
		void				load_mall(int item_count, player_item * items);
		void				close_mall();

		void				set_safebox_open_position();
		auto				distance_from_safebox_open() const -> float;

	protected:
		std::unique_ptr<safebox>	safebox_;
		int					safebox_size_;
		int					safebox_load_time_;
		bool				opening_safebox_;

		std::unique_ptr<safebox>	mall_;
		int					mall_load_time_;

		pixel_position		pos_safebox_open_;


	public:
		void				mount_vnum(uint32_t vnum);
		auto				get_mount_vnum() const -> uint32_t { return mount_vnum_; }
		auto				last_mount_time() const -> uint32_t { return mount_time_; }

		auto				can_use_horse_skill() -> bool;

		void		set_horse_level(int level) override;

		auto		start_riding() -> bool override;
		auto		stop_riding() -> bool override;

		auto		my_horse_vnum() const -> uint32_t override;

		void		horse_die() override;
		auto		revive_horse() -> bool override;

		void		send_horse_info() override;
		void		clear_horse_info() override;

		void				horse_summon(bool summon, bool from_far = false, uint32_t vnum = 0, const char* pet_name = nullptr);

		auto			horse() const -> character *			{ return ch_horse_; }
		auto			rider() const -> character *;
		void				set_rider(character * ch);

		auto				is_riding() const -> bool;

	public:
		auto			get_pet_system() -> pet_system*				{ return pet_system_.get(); }

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

	public:
		void				detail_log() { detail_log_ = !detail_log_; }
		void				toggle_monster_log();
		void				monster_log(const char* format, ...);
	private:
		bool				detail_log_;
		bool				monster_log_;


	public:
		void 				set_empire(uint8_t empire);
		auto				empire() const -> uint8_t { return empire_; }

	protected:
		uint8_t				empire_;

	public:
		void				set_regen(regen * regen_value);

	protected:
		pixel_position			pos_regen_;
		regen *				regen_;
		size_t				regen_id_;

	public:
		auto				cannot_move_by_affect() const -> bool;
		auto				is_immune(uint32_t immune_flag_value) -> bool;
		void				set_immune_flag(uint32_t dw) { points_instant_.immune_flag = dw; }

	protected:
		void				apply_mob_attribute(const mob_table* table);

	public:
		void				set_quest_npcid(uint32_t vid);
		auto				quest_npcid() const -> uint32_t { return quest_npcvid_; }
		auto			quest_npc() const -> character *;

		void				set_quest_item_ptr(item * item);
		void				clear_quest_item_ptr();
		auto				quest_item_ptr() const -> item *;

		void				set_quest_by(uint32_t quest_vnum)	{ quest_by_vnum_ = quest_vnum; }
		auto				quest_by() const -> uint32_t			{ return quest_by_vnum_; }

		auto					quest_flag(const std::string& flag) const -> int;
		void				set_quest_flag(const std::string& flag, int value);

		void				confirm_with_msg(const char* msg_value, int timeout_value, uint32_t request_pid_value);

	private:
		uint32_t				quest_npcvid_;
		uint32_t				quest_by_vnum_;
		item *				quest_item_;

	public:
		auto				start_state_machine(int next_pulse = 1) -> bool;
		void				stop_state_machine();
		void				update_state_machine(uint32_t pulse);
		void				set_next_state_pulse(int next_pulse);

		void				update_character(uint32_t pulse);

	protected:
		uint32_t				next_state_pulse_;

	public:
		auto			marry_partner() const -> character *;
		void				set_marry_partner(character * ch);
		auto					marriage_bonus(uint32_t item_vnum, bool sum = true) -> int;

		void				set_wedding_map(marriage::wedding_map* map);

	private:
		marriage::wedding_map* wedding_map_;
		character *			chr_married_;

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

		event_ptr				mining_event;
		event_ptr				warp_event;
		event_ptr				check_speed_hack_event;
		event_ptr				destroy_when_idle_event;
		event_ptr				pet_system_update_event;

		auto is_warping() const -> bool { return warp_event ? true : false; }

		bool				has_poisoned;

		const mob *		mob_data;
		std::unique_ptr<mob_instance>	mob_inst;

		std::map<int, event_ptr> mob_skill_event;

		friend struct func_splash_damage;
		friend struct func_splash_affect;
		friend class func_shoot;

	public:
		auto				premium_remain_seconds(uint8_t type) const -> int;

	private:
		int				premium_times_[PREMIUM_MAX_NUM];

		static const uint32_t		default_change_item_attr_cycle_;
		static const char		last_change_item_attr_flag_[];
		static const char		change_item_attr_cycle_flag_[];

	public :
		auto item_process_hair(item * item, int dest_cell) -> bool;

	public :
		void clear_skill();
		void clear_sub_skill();

		auto reset_one_skill(uint32_t vnum) -> bool;

	private :
		void send_damage_packet(character * attacker, int damage_value, uint8_t damage_flag);

	private :
		arena *arena_;
		bool arena_observer_;
		int potion_limit_;

	public :
		void 	set_arena(arena* arena_value) { arena_ = arena_value; }
		void	set_arena_observer_mode(bool flag) { arena_observer_ = flag; }

		auto get_arena() const -> arena* { return arena_; }
		auto	arena_observer_mode() const -> bool { return arena_observer_; }

		void	set_potion_limit(int count) { potion_limit_ = count; }
		auto		potion_limit() const -> int { return potion_limit_; }

	public:
		auto	is_open_safebox() const -> bool { return is_open_safebox_ ? true : false; }
		void 	set_open_safebox(bool b) { is_open_safebox_ = b; }

		auto		safebox_load_time() const -> int { return safebox_load_time_; }
		void	set_safebox_load_time() { safebox_load_time_ = thecore_pulse(); }
	private:
		bool	is_open_safebox_;

	public:
		auto		skill_power_by_level(int level, bool mob = false) const -> int;
		
		auto		get_refine_time() const -> int { return refine_time; }
		void	set_refine_time() { refine_time = thecore_pulse(); } 
		int		refine_time;

		
		auto		get_exchange_time() const -> int { return exchange_time; }
		void	set_exchange_time() { exchange_time = thecore_pulse(); }
		int		exchange_time;
		
		int 	my_shop_time;
		auto		get_my_shop_time() const -> int	{ return my_shop_time; }
		void	set_my_shop_time() { my_shop_time = thecore_pulse(); }

		auto	is_hack(bool send_msg = true, bool check_shop_owner = true, int limittime = portal_limit_time) -> bool;

		auto	is_monarch() const -> bool;

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
		auto mc(enum monarch_index e) const -> uint32_t;
		void set_mc(enum monarch_index e);
		auto is_mcok(enum monarch_index e) const -> bool;
		auto mcl(enum monarch_index e) const -> uint32_t;
		auto mcl_time(enum monarch_index e) const -> uint32_t;

	public:
		auto item_process_polymorph(item * item) -> bool;

		auto	cube_item() -> item ** { return points_instant_.cube_items; }
		auto is_cube_open () const -> bool	{ return (points_instant_.cube_npc?true:false); }
		void set_cube_npc(character * npc)	{ points_instant_.cube_npc = npc; }
		auto can_do_cube() const -> bool;

	public:

	private:
		int		deposit_pulse_;

	public:
		void	update_deposit_pulse();
		auto	can_deposit() const -> bool;

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
		auto new_name() const -> const std::string { return this->new_name_; }
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
		auto log_off_interval() const -> uint32_t { return log_off_interval_; }

	public:
		auto un_equip_special_ride_unique_item () -> bool;

		auto can_warp () const -> bool;

	private:
		uint32_t last_gold_drop_time_;


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
	public:
		void set_armada() { cannot_dead_ = true; }
		void reset_armada() { cannot_dead_ = false; }
	private:
		bool cannot_dead_;

	private:
		bool is_pet_;
	public:
		void set_pet() { is_pet_ = true; }
		auto is_pet() -> bool { return is_pet_; }

	private:
		float att_mul_;
		float dam_mul_;
	public:
		auto att_mul() -> float { return this->att_mul_; }
		void set_att_mul(float new_att_mul) {this->att_mul_ = new_att_mul; }
		auto dam_mul() -> float { return this->dam_mul_; }
		void set_dam_mul(float new_dam_mul) {this->dam_mul_ = new_dam_mul; }

	private:
		auto is_valid_item_position(item_pos pos) const -> bool;

	public:
		// Set when the guild NPC quest asks for a name; the answer must echo it, once.
		void		set_guild_creation_key(uint32_t key) { guild_creation_key_ = key; }
		auto	guild_creation_key() const -> uint32_t { return guild_creation_key_; }

	private:
		uint32_t	guild_creation_key_;

	private:
		unsigned int item_award_vnum_;
		char		 item_award_cmd_[20];
	public:
		auto item_award_vnum() -> unsigned int { return item_award_vnum_; }
		auto		 item_award_cmd() -> char* { return item_award_cmd_;	  }
		void		 set_item_award_vnum(unsigned int vnum) { item_award_vnum_ = vnum; }
		void		 set_item_award_cmd(const char* cmd) { strlcpy(item_award_cmd_, cmd, sizeof(item_award_cmd_)); }

	public:
		
		// Needs affects and quests loaded, so load_affect calls it.
		void	dragon_soul_initialize();

		auto	dragon_soul_is_qualified() const -> bool;
		void	dragon_soul_give_qualification();

		auto		dragon_soul_get_active_deck() const -> int;
		auto	dragon_soul_is_deck_activated() const -> bool;
		auto	dragon_soul_activate_deck(int deck_idx) -> bool;

		void	dragon_soul_deactivate_all();
		// Must run before clear_item: unequipping would drop the dragon soul affect and the deck
		// would not be active after login.
		void	dragon_soul_clean_up();
	public:
		auto		dragon_soul_refine_window_open(entity * entity_value) -> bool;
		auto		dragon_soul_refine_window_close() -> bool;
		auto	dragon_soul_refine_window_get_opener() -> entity * { return  points_instant_.dragon_soul_refine_window_opener; }
		auto		dragon_soul_refine_window_can_refine() -> bool;

	private:
		timeval		tv_last_sync_time_;
		int			sync_hack_count_;
	public:
		void			set_last_sync_time(const timeval &tv) { memcpy(&tv_last_sync_time_, &tv, sizeof(timeval)); }
		auto	last_sync_time() -> const timeval& { return tv_last_sync_time_; }
		void			set_sync_hack_count(int count) { sync_hack_count_ = count;}
		auto				sync_hack_count() -> int { return sync_hack_count_; }
	public:
		int					last_drop_time;
		int					count_drops;
		void clear_pm_counter(void)       { pm_counter_ = 0;      } 
		void increase_pm_counter(void)    { pm_counter_++;        }
		void set_last_pm_pulse(void);
		auto  pm_counter(void)   const -> int { return pm_counter_;   }
		auto  last_pm_pulse(void) const -> int { return last_pm_pulse_; }
	protected:
		int last_pm_pulse_;
		int pm_counter_;
};

auto get_sex(character * ch) -> sex;

extern auto is_summonable_zone(int map_index) -> bool;
extern auto find_victim(character * chr, int max_distance) -> character *;

void character_set_bgm_volume_enable();
void character_add_bgm_info(unsigned index, const char* name, float vol);
}
