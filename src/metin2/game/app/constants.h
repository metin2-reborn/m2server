#pragma once

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <cstdint>
#include <map>

namespace metin2::game::inline app {

enum monster_chat_state
{
	MONSTER_CHAT_WAIT,
	MONSTER_CHAT_ATTACK,
	MONSTER_CHAT_CHASE,
	MONSTER_CHAT_ATTACKED,
};

struct mob_rank_stat {
	int gold_percent;
};

struct job_initial_point {
	int		st, ht, dx, iq;
	int		max_hp, max_sp;
	int		hp_per_ht, sp_per_iq;
	int		hp_per_lv_begin, hp_per_lv_end;
	int		sp_per_lv_begin, sp_per_lv_end;
	int		max_stamina;
	int		stamina_per_con;
	int		stamina_per_lv_begin, stamina_per_lv_end;
};

struct coord {
	int		x, y;
};

struct apply_info {
	uint8_t	point_type;
};

enum {
	FORTUNE_BIG_LUCK,
	FORTUNE_LUCK,
	FORTUNE_SMALL_LUCK,
	FORTUNE_NORMAL,
	FORTUNE_SMALL_BAD_LUCK,
	FORTUNE_BAD_LUCK,
	FORTUNE_BIG_BAD_LUCK,
	FORTUNE_MAX_NUM,
};

const int STONE_INFO_MAX_NUM = 10;
const int STONE_LEVEL_MAX_NUM = 4;

struct stone_drop_info
{
	uint32_t mob_vnum;
	int drop_pct;
	int level_pct[STONE_LEVEL_MAX_NUM+1];
};

inline bool operator < (const stone_drop_info& l, uint32_t r)
{
	return l.mob_vnum < r;
}

extern const apply_info		a_apply_info[MAX_APPLY_NUM];
extern const mob_rank_stat       mob_rank_stats[MOB_RANK_MAX_NUM];

extern const uint32_t		party_exp_distribute_table[PLAYER_MAX_LEVEL_CONST + 1];

extern const uint32_t		exp_table[PLAYER_EXP_TABLE_MAX + 1];

extern const uint32_t		guild_exp_table2[GUILD_MAX_LEVEL + 1];

inline constexpr int MAX_EXP_DELTA_OF_LEV = 31;
extern const int		percent_by_delta_lev[MAX_EXP_DELTA_OF_LEV];
extern const int		percent_by_delta_lev_for_boss[MAX_EXP_DELTA_OF_LEV];
inline int percent_lvdelta(int me, int victim) { return percent_by_delta_lev[std::clamp((victim + 15) - me, 0, MAX_EXP_DELTA_OF_LEV - 1)]; }
inline int percent_lvdelta_boss(int me, int victim) { return percent_by_delta_lev_for_boss[std::clamp((victim + 15) - me, 0, MAX_EXP_DELTA_OF_LEV - 1)]; }
inline int calculate_value_lvdelta(int me, int victim, int val) { return (val * percent_lvdelta(me, victim)) / 100; }

inline constexpr int ARROUND_COORD_MAX_NUM = 161;
extern coord			arround_coords[ARROUND_COORD_MAX_NUM];
extern job_initial_point	job_initial_points[JOB_MAX_NUM];

extern const int		mob_enchant_apply_idx[MOB_ENCHANTS_MAX_NUM];
extern const int		mob_resists_apply_idx[MOB_RESISTS_MAX_NUM];

extern const int		skill_attack_affect_prob_by_rank[MOB_RANK_MAX_NUM];

extern const int item_magic_attribute_percent_high[ITEM_ATTRIBUTE_MAX_LEVEL];
extern const int item_magic_attribute_percent_low[ITEM_ATTRIBUTE_MAX_LEVEL];

extern const int item_attribute_add_percent[ITEM_ATTRIBUTE_MAX_NUM];

extern const int exp_loss_percents[PLAYER_EXP_TABLE_MAX + 1];

extern const int * skill_power_by_level;
extern const int skill_power_by_level_euckr[SKILL_MAX_LEVEL + 1];

extern const int polymorph_power_by_level[SKILL_MAX_LEVEL + 1];

extern const int skill_book_count_for_level_up[10];
extern const int grand_master_skill_book_count_for_level_up[10];
extern const int grand_master_skill_book_min_count[10];
extern const int grand_master_skill_book_max_count[10];
extern const int chn_ai_party_bonus_exp_percent_by_member_count[9];

typedef std::map<uint32_t, item_attr_table> item_attr_map;
extern item_attr_map map_item_attr;
extern item_attr_map map_item_rare;

extern const int chain_lightning_count_by_skill_level[SKILL_MAX_LEVEL + 1];

extern const char * apsz_empire_names[EMPIRE_MAX_NUM];
extern const char * apsz_priv_names[MAX_PRIV_NUM];
extern const stone_drop_info stone_drop[STONE_INFO_MAX_NUM];

struct guild_war_info {
	int32_t map_index;
	int war_price;
	int winner_potion_reward_pct_to_winner;
	int loser_potion_reward_pct_to_winner;
	int initial_score;
	int end_score;
};

extern guild_war_info kor_a_guild_war_info[GUILD_WAR_TYPE_MAX_NUM];

enum 
{
	ITEM_ACCESSORY_SOCKET_MAX_NUM = 3
};

extern const int accessory_socket_effective_pct[ITEM_ACCESSORY_SOCKET_MAX_NUM + 1];
extern const int accessory_socket_degrade_time[ITEM_ACCESSORY_SOCKET_MAX_NUM + 1];
extern const int accessory_socket_put_pct[ITEM_ACCESSORY_SOCKET_MAX_NUM + 1];
int32_t fn_get_apply_type(const char *apply_type_string);


int32_t fn_get_apply_type(const char *apply_type_string);
}
