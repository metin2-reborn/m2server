#pragma once

#include <common/length.h>
#include <common/tables.h>

#include <algorithm>
#include <cstdint>
#include <map>

enum EMonsterChatState
{
	MONSTER_CHAT_WAIT,
	MONSTER_CHAT_ATTACK,
	MONSTER_CHAT_CHASE,
	MONSTER_CHAT_ATTACKED,
};

struct TMobRankStat {
	int iGoldPercent;   // Chance of gold dropping
};

struct TJobInitialPoints {
	int		st, ht, dx, iq;
	int		max_hp, max_sp;
	int		hp_per_ht, sp_per_iq;
	int		hp_per_lv_begin, hp_per_lv_end;
	int		sp_per_lv_begin, sp_per_lv_end;
	int		max_stamina;
	int		stamina_per_con;
	int		stamina_per_lv_begin, stamina_per_lv_end;
};

struct Coord {
	int		x, y;
};

struct TApplyInfo {
	uint8_t	bPointType;                          // APPLY -> POINT
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

struct SStoneDropInfo
{
	uint32_t dwMobVnum;
	int iDropPct;
	int iLevelPct[STONE_LEVEL_MAX_NUM+1];
};

inline bool operator < (const SStoneDropInfo& l, uint32_t r)
{
	return l.dwMobVnum < r;
}

extern const TApplyInfo		aApplyInfo[MAX_APPLY_NUM];
extern const TMobRankStat       MobRankStats[MOB_RANK_MAX_NUM];

extern const uint32_t		party_exp_distribute_table[PLAYER_MAX_LEVEL_CONST + 1];

extern const uint32_t		exp_table[PLAYER_EXP_TABLE_MAX + 1];

extern const uint32_t		guild_exp_table2[GUILD_MAX_LEVEL + 1];

inline constexpr int MAX_EXP_DELTA_OF_LEV = 31;
extern const int		aiPercentByDeltaLev[MAX_EXP_DELTA_OF_LEV];
extern const int		aiPercentByDeltaLevForBoss[MAX_EXP_DELTA_OF_LEV];
// Experience/drop percentage by the level difference between killer (me) and victim.
inline int PERCENT_LVDELTA(int me, int victim) { return aiPercentByDeltaLev[std::clamp((victim + 15) - me, 0, MAX_EXP_DELTA_OF_LEV - 1)]; }
inline int PERCENT_LVDELTA_BOSS(int me, int victim) { return aiPercentByDeltaLevForBoss[std::clamp((victim + 15) - me, 0, MAX_EXP_DELTA_OF_LEV - 1)]; }
inline int CALCULATE_VALUE_LVDELTA(int me, int victim, int val) { return (val * PERCENT_LVDELTA(me, victim)) / 100; }

inline constexpr int ARROUND_COORD_MAX_NUM = 161;
extern Coord			aArroundCoords[ARROUND_COORD_MAX_NUM];
extern TJobInitialPoints	JobInitialPoints[JOB_MAX_NUM];

extern const int		aiMobEnchantApplyIdx[MOB_ENCHANTS_MAX_NUM];
extern const int		aiMobResistsApplyIdx[MOB_RESISTS_MAX_NUM];

extern const int		aSkillAttackAffectProbByRank[MOB_RANK_MAX_NUM];

extern const int aiItemMagicAttributePercentHigh[ITEM_ATTRIBUTE_MAX_LEVEL]; // Up to 1
extern const int aiItemMagicAttributePercentLow[ITEM_ATTRIBUTE_MAX_LEVEL];

extern const int aiItemAttributeAddPercent[ITEM_ATTRIBUTE_MAX_NUM];

extern const int aiExpLossPercents[PLAYER_EXP_TABLE_MAX + 1];

extern const int * aiSkillPowerByLevel;
extern const int aiSkillPowerByLevel_euckr[SKILL_MAX_LEVEL + 1];

extern const int aiPolymorphPowerByLevel[SKILL_MAX_LEVEL + 1];

extern const int aiSkillBookCountForLevelUp[10];
extern const int aiGrandMasterSkillBookCountForLevelUp[10];
extern const int aiGrandMasterSkillBookMinCount[10];
extern const int aiGrandMasterSkillBookMaxCount[10];
extern const int CHN_aiPartyBonusExpPercentByMemberCount[9];

typedef std::map<uint32_t, TItemAttrTable> TItemAttrMap;
extern TItemAttrMap g_map_itemAttr;
extern TItemAttrMap g_map_itemRare;

extern const int aiChainLightningCountBySkillLevel[SKILL_MAX_LEVEL + 1];

extern const char * c_apszEmpireNames[EMPIRE_MAX_NUM];
extern const char * c_apszPrivNames[MAX_PRIV_NUM];
extern const SStoneDropInfo aStoneDrop[STONE_INFO_MAX_NUM];

struct TGuildWarInfo {
	int32_t lMapIndex;
	int iWarPrice;
	int iWinnerPotionRewardPctToWinner;
	int iLoserPotionRewardPctToWinner;
	int iInitialScore;
	int iEndScore;
};

extern TGuildWarInfo KOR_aGuildWarInfo[GUILD_WAR_TYPE_MAX_NUM];

// ACCESSORY_REFINE
enum 
{
	ITEM_ACCESSORY_SOCKET_MAX_NUM = 3
};

extern const int aiAccessorySocketEffectivePct[ITEM_ACCESSORY_SOCKET_MAX_NUM + 1];
extern const int aiAccessorySocketDegradeTime[ITEM_ACCESSORY_SOCKET_MAX_NUM + 1];
extern const int aiAccessorySocketPutPct[ITEM_ACCESSORY_SOCKET_MAX_NUM + 1];
int32_t FN_get_apply_type(const char *apply_type_string);

// END_OF_ACCESSORY_REFINE

int32_t FN_get_apply_type(const char *apply_type_string);
