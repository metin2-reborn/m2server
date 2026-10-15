#pragma once

#include "affect.h"
#include "affect_flag.h"
#include "cube.h"
#include "entity.h"
#include "event.h"
#include "FSM.h"
#include "horse_rider.h"
#include "state.h"
#include "typedef.h"
#include "vid.h"

#include <common/length.h>
#include <common/tables.h>
#include <libthecore/main.h>
#include <libthecore/utils.h>

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

class CBuffOnAttributes;
class CPetSystem;

inline constexpr int INSTANT_FLAG_DEATH_PENALTY = (1 << 0);
inline constexpr int INSTANT_FLAG_SHOP = (1 << 1);
inline constexpr int INSTANT_FLAG_STUN = (1 << 3);
inline constexpr int INSTANT_FLAG_NO_REWARD = (1 << 4);
extern int g_nPortalLimitTime;

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

enum EDamageType
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

enum EPointTypes
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

enum EPKModes
{
	PK_MODE_PEACE,
	PK_MODE_REVENGE,
	PK_MODE_FREE,
	PK_MODE_PROTECT,
	PK_MODE_GUILD,
	PK_MODE_MAX_NUM
};

enum EPositions
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

enum EBlockAction
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
struct DynamicCharacterPtr {
	DynamicCharacterPtr() : is_pc(false), id(0) {}

	// Returns the CHARACTER * found in CHARACTER_MANAGER.
	CHARACTER * Get() const; 
	// Clears the current settings.
	void Reset() {
		is_pc = false;
		id = 0;
	}

	// Supports assignment with CHARACTER * type.
	DynamicCharacterPtr& operator=(CHARACTER * character);
	// Supports type casting to CHARACTER *.
	operator CHARACTER *() const {
		return Get();
	}

	bool is_pc;
	uint32_t id;
};

/* Saved data */
struct CHARACTER_POINT {
	int32_t			points[POINT_MAX_NUM];

	uint8_t			job;
	uint8_t			voice;

	uint8_t			level;
	uint32_t			exp;
	int32_t			gold;

	int				hp;
	int				sp;

	int				iRandomHP;
	int				iRandomSP;

	int				stamina;

	uint8_t			skill_group;
};

/* Character data that is not saved */
struct CHARACTER_POINT_INSTANT {
	int32_t			points[POINT_MAX_NUM];

	float			fRot;

	int				iMaxHP;
	int				iMaxSP;

	int32_t			position;

	int32_t			instant_flag;
	uint32_t			dwAIFlag;
	uint32_t			dwImmuneFlag;
	uint32_t			dwLastShoutPulse;

	uint16_t			parts[PART_MAX_NUM];

	CItem *			pItems[INVENTORY_AND_EQUIP_SLOT_MAX];
	uint8_t			bItemGrid[INVENTORY_AND_EQUIP_SLOT_MAX];

	// Dragon soul inventory.
	CItem *			pDSItems[DRAGON_SOUL_INVENTORY_MAX_NUM];
	uint16_t			wDSItemGrid[DRAGON_SOUL_INVENTORY_MAX_NUM];

	// by mhh
	CItem *			pCubeItems[CUBE_MAX_NUM];
	CHARACTER *		pCubeNpc;

	CHARACTER *			battle_victim;

	uint8_t			gm_level;

	uint8_t			bBasePart;	// Casual clothing number

	int				iMaxStamina;

	uint8_t			bBlockMode;

	int				iDragonSoulActiveDeck;
	CEntity *		m_pDragonSoulRefineWindowOpener;
};

#define TRIGGERPARAM		CHARACTER * ch, CHARACTER * causer

class CTrigger
{
	public:
		CTrigger() : bType(0), pFunc(nullptr)
		{
		}

		uint8_t	bType;
		int	(*pFunc) (TRIGGERPARAM);
};

EVENTINFO(char_event_info)
{
	DynamicCharacterPtr ch;
};

struct TSkillUseInfo
{
	int	    iHitCount;
	int	    iMaxHitCount;
	int	    iSplashCount;
	uint32_t   dwNextSkillUsableTime;
	int	    iRange;
	bool    bUsed;
	uint32_t   dwVID;
	bool    isGrandMaster;

	std::unordered_map<uint32_t, size_t> TargetVIDMap;

	TSkillUseInfo()
		: iHitCount(0), iMaxHitCount(0), iSplashCount(0), dwNextSkillUsableTime(0), iRange(0), bUsed(false),
		dwVID(0), isGrandMaster(false)
   	{}

	bool    HitOnce(uint32_t dwVnum = 0);

	bool    UseSkill(bool isGrandMaster, uint32_t vid, uint32_t dwCooltime, int splashcount = 1, int hitcount = -1, int range = -1);
	uint32_t   GetMainTargetVID() const	{ return dwVID; }
	void    SetMainTargetVID(uint32_t vid) { dwVID=vid; }
};

struct TPacketGCPartyUpdate;
class CExchange;
class CSkillProto;
class CParty;
class CDungeon;
class CWarMap;
class CAffect;
class CGuild;
class CSafebox;
class CArena;

class CShop;

class CMob;
class CMobInstance;
struct TMobSkillInfo;

//SKILL_POWER_BY_LEVEL
extern int GetSkillPowerByLevelFromType(int job, int skillgroup, int skilllevel);
//END_SKILL_POWER_BY_LEVEL

namespace marriage
{
	class WeddingMap;
}

class CHARACTER : public CEntity, public CFSM, public CHorseRider
{
	protected:
		//////////////////////////////////////////////////////////////////////////////////
		// Entity related
		void	EncodeInsertPacket(CEntity * entity) override;
		void	EncodeRemovePacket(CEntity * entity) override;
		//////////////////////////////////////////////////////////////////////////////////

	public:
		CHARACTER *			FindCharacterInView(const char * name, bool bFindPCOnly);
		void				UpdatePacket();
		int				analyze_protect; 
		int				analyze_protect_count;

		//////////////////////////////////////////////////////////////////////////////////
		// FSM (Finite State Machine) related
	protected:
		CStateTemplate<CHARACTER>	m_stateMove;
		CStateTemplate<CHARACTER>	m_stateBattle;
		CStateTemplate<CHARACTER>	m_stateIdle;

	public:
		virtual void		StateMove();
		virtual void		StateBattle();
		virtual void		StateIdle();
		virtual void		StateFlag();
		virtual void		StateFlagBase();
		void				StateHorse();

	protected:
		// STATE_IDLE_REFACTORING
		void				__StateIdle_Monster();
		void				__StateIdle_Stone();
		void				__StateIdle_NPC();
		// END_OF_STATE_IDLE_REFACTORING

	public:
		uint32_t GetAIFlag() const	{ return m_pointsInstant.dwAIFlag; }
	
		void				SetAggressive();
		bool				IsAggressive() const;

		void				SetCoward();
		bool				IsCoward() const;
		void				CowardEscape();

		bool				IsNoAttackShinsu() const;

		bool				IsNoAttackChunjo() const;

		bool				IsNoAttackJinno() const;

		bool				IsAttackMob() const;

		virtual void			BeginStateEmpty();
		virtual void			EndStateEmpty() {}

		void				RestartAtSamePos();

	protected:
		uint32_t				m_dwStateDuration;
		//////////////////////////////////////////////////////////////////////////////////

	public:
		CHARACTER();
		~CHARACTER() override;

		void			Create(const char * c_pszName, uint32_t vid, bool isPC);
		void			Destroy();

		void			Disconnect(const char * c_pszReason);

	protected:
		void			Initialize();

		//////////////////////////////////////////////////////////////////////////////////
		// Basic Points
	public:
		uint32_t			GetPlayerID() const	{ return m_dwPlayerID; }

		void			SetPlayerProto(const TPlayerTable * table);
		void			CreatePlayerProto(TPlayerTable & tab);	// Used when saving

		void			SetProto(const CMob * c_pkMob);
		uint16_t			GetRaceNum() const;

		void			Save();		// DelayedSave
		void			SaveReal();	// Actual save
		void			FlushDelayedSaveItem();

		const char *	GetName() const;
		const VID &		GetVID() const		{ return m_vid;		}

		void			SetName(const std::string& name) { m_stName = name; }

		void			SetRace(uint8_t race);
		bool			ChangeSex();

		uint32_t			GetAID() const;
		int				GetChangeEmpireCount() const;
		void			SetChangeEmpireCount();
		int				ChangeEmpire(uint8_t empire);

		uint8_t			GetJob() const;
		uint8_t			GetCharType() const;

		bool			IsPC() const		{ return GetDesc() ? true : false; }
		bool			IsNPC()	const		{ return m_bCharType != CHAR_TYPE_PC; }
		bool			IsMonster()	const	{ return m_bCharType == CHAR_TYPE_MONSTER; }
		bool			IsStone() const		{ return m_bCharType == CHAR_TYPE_STONE; }
		bool			IsDoor() const		{ return m_bCharType == CHAR_TYPE_DOOR; } 
		bool			IsBuilding() const	{ return m_bCharType == CHAR_TYPE_BUILDING;  }
		bool			IsWarp() const		{ return m_bCharType == CHAR_TYPE_WARP; }
		bool			IsGoto() const		{ return m_bCharType == CHAR_TYPE_GOTO; }
//		bool			IsPet() const		{ return m_bCharType == CHAR_TYPE_PET; }

		uint32_t			GetLastShoutPulse() const	{ return m_pointsInstant.dwLastShoutPulse; }
		void			SetLastShoutPulse(uint32_t pulse) { m_pointsInstant.dwLastShoutPulse = pulse; }
		int				GetLevel() const		{ return m_points.level;	}
		void			SetLevel(uint8_t level);

		uint8_t			GetGMLevel() const;
		bool 			IsGM() const;
		void			SetGMLevel(); 

		uint32_t			GetExp() const		{ return m_points.exp;	}
		void			SetExp(uint32_t exp)	{ m_points.exp = exp;	}
		uint32_t			GetNextExp() const;
		CHARACTER *		DistributeExp();	// Returns the one who dealt the most hits.
		void			DistributeHP(CHARACTER * pkKiller);
		void			DistributeSP(CHARACTER * pkKiller, int iMethod=0);

		void			SetPosition(int pos);
		bool			IsPosition(int pos) const	{ return m_pointsInstant.position == pos ? true : false; }
		int				GetPosition() const		{ return m_pointsInstant.position; }

		void			SetPart(uint8_t bPartPos, uint16_t wVal);
		uint16_t			GetPart(uint8_t bPartPos) const;
		uint16_t			GetOriginalPart(uint8_t bPartPos) const;

		void			SetHP(int hp)		{ m_points.hp = hp; }
		int				GetHP() const		{ return m_points.hp; }

		void			SetSP(int sp)		{ m_points.sp = sp; }
		int				GetSP() const		{ return m_points.sp; }

		void			SetStamina(int stamina)	{ m_points.stamina = stamina; }
		int				GetStamina() const		{ return m_points.stamina; }

		void			SetMaxHP(int iVal)	{ m_pointsInstant.iMaxHP = iVal; }
		int				GetMaxHP() const	{ return m_pointsInstant.iMaxHP; }

		void			SetMaxSP(int iVal)	{ m_pointsInstant.iMaxSP = iVal; }
		int				GetMaxSP() const	{ return m_pointsInstant.iMaxSP; }

		void			SetMaxStamina(int iVal)	{ m_pointsInstant.iMaxStamina = iVal; }
		int				GetMaxStamina() const	{ return m_pointsInstant.iMaxStamina; }

		void			SetRandomHP(int v)	{ m_points.iRandomHP = v; }
		void			SetRandomSP(int v)	{ m_points.iRandomSP = v; }

		int				GetHPPct() const;

		void			SetRealPoint(uint8_t idx, int val);
		int				GetRealPoint(uint8_t idx) const;

		void			SetPoint(uint8_t idx, int val);
		int				GetPoint(uint8_t idx) const;
		int				GetLimitPoint(uint8_t idx) const;
		int				GetPolymorphPoint(uint8_t idx) const;

		const TMobTable &	GetMobTable() const;
		uint8_t				GetMobRank() const;
		uint8_t				GetMobBattleType() const;
		uint32_t				GetMobDamageMin() const;
		uint32_t				GetMobDamageMax() const;
		uint16_t				GetMobAttackRange() const;
		uint32_t				GetMobDropItemVnum() const;
		float				GetMobDamageMultiply() const;

		// NEWAI
		bool			IsBerserker() const;
		bool			IsBerserk() const;
		void			SetBerserk(bool mode);

		bool			IsStoneSkinner() const;

		bool			IsGodSpeeder() const;
		bool			IsGodSpeed() const;
		void			SetGodSpeed(bool mode);

		bool			IsDeathBlower() const;
		bool			IsDeathBlow() const;

		bool			IsReviver() const;
		bool			HasReviverInParty() const;
		bool			IsRevive() const;
		void			SetRevive(bool mode);
		// NEWAI END

		bool			IsRaceFlag(uint32_t dwBit) const;
		bool			IsSummonMonster() const;
		uint32_t			GetSummonVnum() const;

		uint32_t			GetPolymorphItemVnum() const;
		uint32_t			GetMonsterDrainSPPoint() const;

		void			MainCharacterPacket();	// Tells it that I am the main character.

		void			ComputePoints();
		void			ComputeBattlePoints();
		void			PointChange(uint8_t type, int amount, bool bAmount = false, bool bBroadcast = false);
		void			PointsPacket();
		void			ApplyPoint(uint8_t bApplyType, int iVal);
		void			CheckMaximumPoints();	// Checks whether current values like HP and SP exceed their max and lowers them if so.

		bool			Show(int32_t lMapIndex, int32_t x, int32_t y, int32_t z = INT32_MAX, bool bShowSpawnMotion = false);

		void			Sitdown(int is_ground);
		void			Standup();

		void			SetRotation(float fRot);
		void			SetRotationToXY(int32_t x, int32_t y);
		float			GetRotation() const	{ return m_pointsInstant.fRot; }

		void			ChatPacket(uint8_t type, const char *format, ...);
		void			MonsterChat(uint8_t bMonsterChatType);
		void			SendGreetMessage();

		void			ResetPoint(int iLv);

		void			SetBlockMode(uint8_t bFlag);
		void			SetBlockModeForce(uint8_t bFlag);
		bool			IsBlockMode(uint8_t bFlag) const	{ return (m_pointsInstant.bBlockMode & bFlag)?true:false; }

		bool			IsPolymorphed() const		{ return m_dwPolymorphRace>0; }
		bool			IsPolyMaintainStat() const	{ return m_bPolyMaintainStat; } // Polymorph that keeps the previous stats.
		void			SetPolymorph(uint32_t dwRaceNum, bool bMaintainStat = false);
		uint32_t			GetPolymorphVnum() const	{ return m_dwPolymorphRace; }
		int				GetPolymorphPower() const;

		// FISING	
		void			fishing();
		void			fishing_take();
		// END_OF_FISHING

		// MINING
		void			mining(CHARACTER * chLoad);
		void			mining_cancel();
		void			mining_take();
		// END_OF_MINING

		void			ResetPlayTime(uint32_t dwTimeRemain = 0);

		void			CreateFly(uint8_t bType, CHARACTER * pkVictim);

		void			ResetChatCounter();
		uint8_t			IncreaseChatCounter();
		uint8_t			GetChatCounter() const;

	protected:
		uint32_t			m_dwPolymorphRace;
		bool			m_bPolyMaintainStat;
		uint32_t			m_dwPlayerID;
		VID				m_vid;
		std::string		m_stName;
		uint8_t			m_bCharType;

		CHARACTER_POINT		m_points;
		CHARACTER_POINT_INSTANT	m_pointsInstant;

		uint32_t			m_dwPlayStartTime;
		uint8_t			m_bAddChrState;
		bool			m_bSkipSave;
		std::string		m_stMobile;
		std::string		m_stMobileAuth;
		uint8_t			m_bChatCounter;

		// End of Basic Points

		//////////////////////////////////////////////////////////////////////////////////
		// Move & Synchronize Positions
		//////////////////////////////////////////////////////////////////////////////////
	public:
		bool			IsWalking() const			{ return m_bNowWalking || GetStamina()<=0; }
		void			SetWalking(bool bWalkFlag)	{ m_bWalking=bWalkFlag; }
		void			SetNowWalking(bool bWalkFlag);	
		void			ResetWalking()			{ SetNowWalking(m_bWalking); }

		bool			Goto(int32_t x, int32_t y);	// Blend to the target position instead of moving immediately.
		void			Stop();

		bool			CanMove() const;		// Can it move?

		void			SyncPacket();
		bool			Sync(int32_t x, int32_t y);	// Actual movement happens through this method (no movement restrictions from conditions)
		bool			Move(int32_t x, int32_t y);	// Checks conditions and moves via the Sync method.
		void			OnMove(bool bIsAttack = false);	// Called when moving. May be called from places other than Move().
		uint32_t			GetMotionMode() const;
		float			GetMoveMotionSpeed() const;
		float			GetMoveSpeed() const;
		void			CalculateMoveDuration();
		void			SendMovePacket(uint8_t bFunc, uint8_t bArg, uint32_t x, uint32_t y, uint32_t dwDuration, uint32_t dwTime=0, int iRot=-1);
		uint32_t			GetCurrentMoveDuration() const	{ return m_dwMoveDuration; }
		uint32_t			GetWalkStartTime() const	{ return m_dwWalkStartTime; }
		uint32_t			GetLastMoveTime() const		{ return m_dwLastMoveTime; }
		uint32_t			GetLastAttackTime() const	{ return m_dwLastAttackTime; }

		void			SetLastAttacked(uint32_t time);	// Stores the time and position of the last attack received

		bool			SetSyncOwner(CHARACTER * ch, bool bRemoveFromList = true);
		bool			IsSyncOwner(CHARACTER * ch) const;

		bool			WarpSet(int32_t x, int32_t y, int32_t lRealMapIndex = 0);
		void			SetWarpLocation(int32_t lMapIndex, int32_t x, int32_t y);
		void			WarpEnd();
		bool			WarpToPID(uint32_t dwPID);

		void			SaveExitLocation();
		void			ExitToSavedLocation();

		void			StartStaminaConsume();
		void			StopStaminaConsume();
		bool			IsStaminaConsume() const;
		bool			IsStaminaHalfConsume() const;

		void			ResetStopTime();
		uint32_t			GetStopTime() const;

	protected:
		void			ClearSync();

		float			m_fSyncTime;
		CHARACTER *		m_pkChrSyncOwner;
		CHARACTER_LIST	m_kLst_pkChrSyncOwned;	// Those for whom I am the SyncOwner

		PIXEL_POSITION	m_posDest;
		PIXEL_POSITION	m_posStart;
		PIXEL_POSITION	m_posWarp;
		int32_t			m_lWarpMapIndex;

		PIXEL_POSITION	m_posExit;
		int32_t			m_lExitMapIndex;

		uint32_t			m_dwMoveStartTime;
		uint32_t			m_dwMoveDuration;

		uint32_t			m_dwLastMoveTime;
		uint32_t			m_dwLastAttackTime;
		uint32_t			m_dwWalkStartTime;
		uint32_t			m_dwStopTime;

		bool			m_bWalking;
		bool			m_bNowWalking;
		bool			m_bStaminaConsume;
		// End

		// Quickslot related
	public:
		void			SyncQuickslot(uint8_t bType, uint8_t bOldPos, uint8_t bNewPos);
		bool			GetQuickslot(uint8_t pos, TQuickslot ** ppSlot);
		bool			SetQuickslot(uint8_t pos, TQuickslot & rSlot);
		bool			DelQuickslot(uint8_t pos);
		bool			SwapQuickslot(uint8_t a, uint8_t b);
		void			ChainQuickslotItem(CItem * pItem, uint8_t bType, uint8_t bOldPos);

	protected:
		TQuickslot		m_quickslot[QUICKSLOT_MAX_NUM];

		////////////////////////////////////////////////////////////////////////////////////////
		// Affect
	public:
		void			StartAffectEvent();
		void			ClearAffect(bool bSave=false);
		void			ComputeAffect(CAffect * pkAff, bool bAdd);
		bool			AddAffect(uint32_t dwType, uint8_t bApplyOn, int32_t lApplyValue, uint32_t dwFlag, int32_t lDuration, int32_t lSPCost, bool bOverride, bool IsCube = false);
		void			RefreshAffect();
		bool			RemoveAffect(uint32_t dwType);
		bool			IsAffectFlag(uint32_t dwAff) const;

		bool			UpdateAffect();	// called from EVENT
		int				ProcessAffect();

		void			LoadAffect(uint32_t dwCount, TPacketAffectElement * pElements);
		void			SaveAffect();

		bool			IsGoodAffect(uint8_t bAffectType) const;

		void			RemoveGoodAffect();
		void			RemoveBadAffect();

		CAffect *		FindAffect(uint32_t dwType, uint8_t bApply=APPLY_NONE) const;
		const std::list<CAffect *> & GetAffectContainer() const	{ return m_list_pkAffect; }
		bool			RemoveAffect(CAffect * pkAff);

	protected:
		TAffectFlag		m_afAffectFlag;
		std::list<CAffect *>	m_list_pkAffect;

	public:
		// PARTY_JOIN_BUG_FIX
		void			SetParty(CParty * pkParty);
		CParty *			GetParty() const	{ return m_pkParty; }

		bool			RequestToParty(CHARACTER * leader);
		void			DenyToParty(CHARACTER * member);
		void			AcceptToParty(CHARACTER * member);

		/// Invites another character to own party.
		/**
		 * @param	pchInvitee character to invite. Must be in a state that allows joining a party.
		 *
		 * If either character is not in a state to invite or be invited to a party, sends the corresponding chat message to the inviting character.
		 */
		void			PartyInvite(CHARACTER * pchInvitee);

		/// Handles acceptance by an invited character.
		/**
		 * @param	pchInvitee character joining the party. Must be in a state that allows joining a party.
		 *
		 * If pchInvitee cannot join the party, sends the corresponding chat message.
		 */
		void			PartyInviteAccept(CHARACTER * pchInvitee);

		/// Handles refusal by an invited character.
		/**
		 * @param [in]	dwPID PID of the invited character
		 */
		void			PartyInviteDeny(uint32_t dwPID);

		bool			BuildUpdatePartyPacket(TPacketGCPartyUpdate & out);
		int				GetLeadershipSkillLevel() const;

		bool			CanSummon(int iLeaderShip);

		void			SetPartyRequestEvent(LPEVENT pkEvent) { m_pkPartyRequestEvent = pkEvent; }

	protected:

		/// Joins a party.
		/**
		 * @param	pkLeader leader of the party to join
		 */
		void			PartyJoin(CHARACTER * pkLeader);

		/**
		 * Error codes for when a party cannot be joined.
		 * Error codes are split into mutable types and static types depending on whether they are time dependent.
		 * If the error code value is below PERR_SEPARATOR it is a mutable type; if above, a static type.
		 */
		enum PartyJoinErrCode {
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
		static PartyJoinErrCode	IsPartyJoinableCondition(CHARACTER * const pchLeader, CHARACTER * const pchGuest);

		/// Checks dynamic conditions for joining or forming a party.
		/**
		 * @param 	pchLeader party leader or the inviting character
		 * @param	pchGuest invited character
		 * @return	returns only mutable type codes.
		 */
		static PartyJoinErrCode	IsPartyJoinableMutableCondition(CHARACTER * const pchLeader, CHARACTER * const pchGuest);

		CParty *			m_pkParty;
		uint32_t			m_dwLastDeadTime;
		LPEVENT			m_pkPartyRequestEvent;

		/**
		 * Party invite event map.
		 * key: PID of the invited character
		 * value: event pointer
		 *
		 * Event map for invited characters.
		 */
		typedef std::map< uint32_t, LPEVENT >	EventMap;
		EventMap		m_PartyInviteEventMap;

		// END_OF_PARTY_JOIN_BUG_FIX

		////////////////////////////////////////////////////////////////////////////////////////
		// Dungeon
	public:
		void			SetDungeon(CDungeon * pkDungeon);
		CDungeon *		GetDungeon() const	{ return m_pkDungeon; }
		CDungeon *		GetDungeonForce() const;
	protected:
		CDungeon *	m_pkDungeon;
		int			m_iEventAttr;

		////////////////////////////////////////////////////////////////////////////////////////
		// Guild
	public:
		void			SetGuild(CGuild * pGuild);
		CGuild*			GetGuild() const	{ return m_pGuild; }

		void			SetWarMap(CWarMap* pWarMap);
		CWarMap*		GetWarMap() const	{ return m_pWarMap; }

	protected:
		CGuild *		m_pGuild;
		uint32_t			m_dwUnderGuildWarInfoMessageTime;
		CWarMap *		m_pWarMap;

		////////////////////////////////////////////////////////////////////////////////////////
		// Item related
	public:
		bool			CanHandleItem(bool bSkipRefineCheck = false, bool bSkipObserver = false); // Can item-related actions be performed?

		bool			IsItemLoaded() const	{ return m_bItemLoaded; }
		void			SetItemLoaded()	{ m_bItemLoaded = true; }

		void			ClearItem();
		void			SetItem(TItemPos Cell, CItem * item);
		CItem *			GetItem(TItemPos Cell) const;
		CItem *			GetInventoryItem(uint16_t wCell) const;
		bool			IsEmptyItemGrid(TItemPos Cell, uint8_t size, int iExceptionCell = -1) const;

		void			SetWear(uint8_t bCell, CItem * item);
		CItem *			GetWear(uint8_t bCell) const;

		// MYSHOP_PRICE_LIST
		void			UseSilkBotary(void); 		/// Use of the silk bundle item

		/// Sends the price list received from the DB cache to the user and handles use of the bundle item.
		/**
		 * @param [in] p	price list packet
		 *
		 * On the first use of the silk bundle item after logging in, UseSilkBotary requests the price list from the DB cache,
		 * and once the response arrives this function performs the actual silk bundle use.
		 */
		void			UseSilkBotaryReal(const TPacketMyshopPricelistHeader* p);
		// END_OF_MYSHOP_PRICE_LIST

		bool			UseItemEx(CItem * item, TItemPos DestCell);
		bool			UseItem(TItemPos Cell, TItemPos DestCell = NPOS);

		// ADD_REFINE_BUILDING
		bool			IsRefineThroughGuild() const;
		CGuild *		GetRefineGuild() const;
		int				ComputeRefineFee(int iCost, int iMultiply = 5) const;
		void			PayRefineFee(int iTotalMoney);
		void			SetRefineNPC(CHARACTER * ch);
		// END_OF_ADD_REFINE_BUILDING

		bool			RefineItem(CItem * pkItem, CItem * pkTarget);
		bool			DropItem(TItemPos Cell,  uint8_t bCount=0);
		bool			GiveRecallItem(CItem * item);
		void			ProcessRecallItem(CItem * item);

		void			EffectPacket(int enumEffectType);
		void			SpecificEffectPacket(const char filename[128]);

		// ADD_MONSTER_REFINE
		bool			DoRefine(CItem * item, bool bMoneyOnly = false);
		// END_OF_ADD_MONSTER_REFINE

		bool			DoRefineWithScroll(CItem * item);
		bool			RefineInformation(uint8_t bCell, uint8_t bType, int iAdditionalCell = -1);

		void			SetRefineMode(int iAdditionalCell = -1);
		void			ClearRefineMode();

		bool			GiveItem(CHARACTER * victim, TItemPos Cell);
		bool			CanReceiveItem(CHARACTER * from, CItem * item) const;
		void			ReceiveItem(CHARACTER * from, CItem * item);
		bool			GiveItemFromSpecialItemGroup(uint32_t dwGroupNum, std::vector <uint32_t> &dwItemVnums, 
							std::vector <uint32_t> &dwItemCounts, std::vector <CItem *> &item_gets, int &count);

		bool			MoveItem(TItemPos pos, TItemPos change_pos, uint8_t num);
		bool			PickupItem(uint32_t vid);
		bool			EquipItem(CItem * item, int iCandidateCell = -1);
		bool			UnequipItem(CItem * item);

		// Checks whether the item can currently be equipped; if not, tells the character why
		bool			CanEquipNow(CItem * const item, const TItemPos& srcCell = NPOS, const TItemPos& destCell = NPOS);

		// Checks whether the equipped item can be removed; if not, tells the character why
		bool			CanUnequipNow(CItem * const item, const TItemPos& srcCell = NPOS, const TItemPos& destCell = NPOS);

		bool			SwapItem(uint8_t bCell, uint8_t bDestCell);
		CItem *			AutoGiveItem(uint32_t dwItemVnum, uint8_t bCount=1, int iRarePct = -1, bool bMsg = true);
		void			AutoGiveItem(CItem * item, bool longOwnerShip = false);
		
		int				GetEmptyInventory(uint8_t size) const;
		int				GetEmptyDragonSoulInventory(CItem * pItem) const;
		void			CopyDragonSoulItemGrid(std::vector<uint16_t>& vDragonSoulItemGrid) const;

		int				CountEmptyInventory() const;

		int				CountSpecifyItem(uint32_t vnum) const;
		void			RemoveSpecifyItem(uint32_t vnum, uint32_t count = 1);
		CItem *			FindSpecifyItem(uint32_t vnum) const;
		CItem *			FindItemByID(uint32_t id) const;

		int				CountSpecifyTypeItem(uint8_t type) const;
		void			RemoveSpecifyTypeItem(uint8_t type, uint32_t count = 1);

		bool			IsEquipUniqueItem(uint32_t dwItemVnum) const;

		// CHECK_UNIQUE_GROUP
		bool			IsEquipUniqueGroup(uint32_t dwGroupVnum) const;
		// END_OF_CHECK_UNIQUE_GROUP

		void			SendEquipment(CHARACTER * ch);
		// End of Item

	protected:

		/// Sends price information for one item.
		/**
		 * @param [in]	dwItemVnum item vnum
		 * @param [in]	dwItemPrice item price
		 */
		void			SendMyShopPriceListCmd(uint32_t dwItemVnum, uint32_t dwItemPrice);

		bool			m_bNoOpenedShop;	///< Whether a private shop has been opened since this login (true if never opened)

		bool			m_bItemLoaded;
		int				m_iRefineAdditionalCell;
		bool			m_bUnderRefine;
		uint32_t			m_dwRefineNPCVID;

	public:
		////////////////////////////////////////////////////////////////////////////////////////
		// Money related
		int32_t				GetGold() const		{ return m_points.gold;	}
		void			SetGold(int32_t gold)	{ m_points.gold = gold;	}
		bool			DropGold(int32_t gold);
		void			GiveGold(int32_t iAmount);	// If in a party, handle party distribution, logging, etc.
		// End of Money

		////////////////////////////////////////////////////////////////////////////////////////
		// Shop related
	public:
		void			SetShop(CShop * pkShop);
		CShop *			GetShop() const { return m_pkShop; }

		void			SetShopOwner(CHARACTER * ch) { m_pkChrShopOwner = ch; }
		CHARACTER *		GetShopOwner() const { return m_pkChrShopOwner;}

		void			OpenMyShop(const char * c_pszSign, TShopItemTable * pTable, uint8_t bItemCount);
		CShop *			GetMyShop() const { return m_pkMyShop; }
		void			CloseMyShop();

	protected:

		CShop *			m_pkShop;
		CShop *			m_pkMyShop;
		std::string		m_stShopSign;
		CHARACTER *		m_pkChrShopOwner;
		// End of shop

		////////////////////////////////////////////////////////////////////////////////////////
		// Exchange related
	public:
		bool			ExchangeStart(CHARACTER * victim);
		void			SetExchange(CExchange * pkExchange);
		CExchange *		GetExchange() const	{ return m_pkExchange;	}

	protected:
		CExchange *		m_pkExchange;
		// End of Exchange

		////////////////////////////////////////////////////////////////////////////////////////
		// Battle
	public:
		struct TBattleInfo
		{
			int iTotalDamage;
			int iAggro;

			TBattleInfo(int iTot, int iAggr)
				: iTotalDamage(iTot), iAggro(iAggr)
				{}
		};
		typedef std::map<VID, TBattleInfo>	TDamageMap;

		struct AttackLog {
			uint32_t	dwVID;
			uint32_t	dwTime;
		};

		bool				Damage(CHARACTER * pAttacker, int dam, EDamageType type = DAMAGE_TYPE_NORMAL);
		void				DeathPenalty(uint8_t bExpLossPercent);
		void				ReviveInvisible(int iDur);

		bool				Attack(CHARACTER * pkVictim, uint8_t bType = 0);

		bool				CanBeginFight() const;
		void				BeginFight(CHARACTER * pkVictim); // Starts fighting pkVictimr. (Forced; use CanBeginFight to check whether it can start)

		bool				IsStun() const;
		void				Stun();
		bool				IsDead() const;
		void				Dead(CHARACTER * pkKiller = nullptr, bool bImmediateDead=false);

		void				Reward(bool bItemDrop);
		void				RewardGold(CHARACTER * pkAttacker);

		bool				Shoot(uint8_t bType);
		void				FlyTarget(uint32_t dwTargetVID, int32_t x, int32_t y, uint8_t bHeader);

		void				ForgetMyAttacker();
		void				AggregateMonster();
		void				AttractRanger();
		void				PullMonster();

		int					GetArrowAndBow(CItem * * ppkBow, CItem * * ppkArrow, int iArrowCount = 1);
		void				UseArrow(CItem * pkArrow, uint32_t dwArrowCount);

		void				AttackedByPoison(CHARACTER * pkAttacker);
		void				RemovePoison();

		void				AttackedByFire(CHARACTER * pkAttacker, int amount, int count);
		void				RemoveFire();

		void				UpdateAlignment(int iAmount);
		int					GetAlignment() const;

		//Get alignment 
		int					GetRealAlignment() const;
		void				ShowAlignment(bool bShow);

		void				SetKillerMode(bool bOn);
		bool				IsKillerMode() const;
		void				UpdateKillerMode();

		uint8_t				GetPKMode() const;
		void				SetPKMode(uint8_t bPKMode);

		void				ItemDropPenalty(CHARACTER * pkKiller);

		void				UpdateAggrPoint(CHARACTER * ch, EDamageType type, int dam);

		//
		// HACK
		// 
	public:
		void SetComboSequence(uint8_t seq);
		uint8_t GetComboSequence() const;

		void SetLastComboTime(uint32_t time);
		uint32_t GetLastComboTime() const;

		int GetValidComboInterval() const;
		void SetValidComboInterval(int interval);

		uint8_t GetComboIndex() const;

		void IncreaseComboHackCount(int k = 1);
		void ResetComboHackCount();
		void SkipComboAttackByTime(int interval);
		uint32_t GetSkipComboAttackByTime() const;

	protected:
		uint8_t m_bComboSequence;
		uint32_t m_dwLastComboTime;
		int m_iValidComboInterval;
		uint8_t m_bComboIndex;
		int m_iComboHackCount;
		uint32_t m_dwSkipComboAttackByTime;

	protected:
		void				UpdateAggrPointEx(CHARACTER * ch, EDamageType type, int dam, TBattleInfo & info);
		void				ChangeVictimByAggro(int iNewAggro, CHARACTER * pNewVictim);

		uint32_t				m_dwFlyTargetID;
		std::vector<uint32_t>	m_vec_dwFlyTargets;
		TDamageMap			m_map_kDamage;	// How much damage has each character dealt to me?
		uint32_t				m_dwKillerPID;

		int					m_iAlignment;		// Lawful/Chaotic value -200000 ~ 200000
		int					m_iRealAlignment;
		int					m_iKillerModePulse;
		uint8_t				m_bPKMode;

		// Aggro
		uint32_t				m_dwLastVictimSetTime;
		int					m_iMaxAggro;
		// End of Battle

		// Stone
	public:
		void				SetStone(CHARACTER * pkChrStone);
		void				ClearStone();
		void				DetermineDropMetinStone();
		uint32_t				GetDropMetinStoneVnum() const { return m_dwDropMetinStone; }
		uint8_t				GetDropMetinStonePct() const { return m_bDropMetinStonePct; }

	protected:
		CHARACTER *			m_pkChrStone;		// The stone that spawned me
		CHARACTER_SET		m_set_pkChrSpawnedBy;	// The ones I spawned
		uint32_t				m_dwDropMetinStone;
		uint8_t				m_bDropMetinStonePct;
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

		void				SkillLevelPacket();
		void				SkillLevelUp(uint32_t dwVnum, uint8_t bMethod = SKILL_UP_BY_POINT);
		bool				SkillLevelDown(uint32_t dwVnum);
		// ADD_GRANDMASTER_SKILL
		bool				UseSkill(uint32_t dwVnum, CHARACTER * pkVictim, bool bUseGrandMaster = true);
		void				ResetSkill();
		void				SetSkillLevel(uint32_t dwVnum, uint8_t bLev);
		int					GetUsedSkillMasterType(uint32_t dwVnum);

		bool				IsLearnableSkill(uint32_t dwSkillVnum) const;
		// END_OF_ADD_GRANDMASTER_SKILL

		bool				CheckSkillHitCount(const uint8_t SkillID, const VID dwTargetVID);
		bool				CanUseSkill(uint32_t dwSkillVnum) const;
		bool				IsUsableSkillMotion(uint32_t dwMotionIndex) const;
		int					GetSkillLevel(uint32_t dwVnum) const;
		int					GetSkillMasterType(uint32_t dwVnum) const;
		int					GetSkillPower(uint32_t dwVnum, uint8_t bLevel = 0) const;

		time_t				GetSkillNextReadTime(uint32_t dwVnum) const;
		void				SetSkillNextReadTime(uint32_t dwVnum, time_t time);
		void				SkillLearnWaitMoreTimeMessage(uint32_t dwVnum);

		int					ComputeSkill(uint32_t dwVnum, CHARACTER * pkVictim, uint8_t bSkillLevel = 0);
		int					ComputeSkillAtPosition(uint32_t dwVnum, const PIXEL_POSITION& posTarget, uint8_t bSkillLevel = 0);
		void				ComputeSkillPoints();

		void				SetSkillGroup(uint8_t bSkillGroup); 
		uint8_t				GetSkillGroup() const		{ return m_points.skill_group; }

		int					ComputeCooltime(int time);

		void				GiveRandomSkillBook();

		void				DisableCooltime();
		bool				LearnSkillByBook(uint32_t dwSkillVnum, uint8_t bProb = 0);
		bool				LearnGrandMasterSkill(uint32_t dwSkillVnum);

	private:
		bool				m_bDisableCooltime;
		uint32_t				m_dwLastSkillTime;	///< Last time a skill was used (millisecond).
		// End of Skill

		// MOB_SKILL
	public:
		bool				HasMobSkill() const;
		size_t				CountMobSkill() const;
		const TMobSkillInfo * GetMobSkill(unsigned int idx) const;
		bool				CanUseMobSkill(unsigned int idx) const;
		bool				UseMobSkill(unsigned int idx);
		void				ResetMobSkillCooltime();
	protected:
		uint32_t				m_adwMobSkillCooltime[MOB_SKILL_MAX_NUM];
		// END_OF_MOB_SKILL

		// for SKILL_MUYEONG
	public:
		void				StartMuyeongEvent();
		void				StopMuyeongEvent();

	private:
		LPEVENT				m_pkMuyeongEvent;

		// for SKILL_CHAIN lighting
	public:
		int					GetChainLightningIndex() const { return m_iChainLightingIndex; }
		void				IncChainLightningIndex() { ++m_iChainLightingIndex; }
		void				AddChainLightningExcept(CHARACTER * ch) { m_setExceptChainLighting.insert(ch); }
		void				ResetChainLightningIndex() { m_iChainLightingIndex = 0; m_setExceptChainLighting.clear(); }
		int					GetChainLightningMaxCount() const;
		const CHARACTER_SET& GetChainLightingExcept() const { return m_setExceptChainLighting; }

	private:
		int					m_iChainLightingIndex;
		CHARACTER_SET m_setExceptChainLighting;

		// for SKILL_EUNHYUNG
	public:
		void				SetAffectedEunhyung();
		void				ClearAffectedEunhyung() { m_dwAffectedEunhyungLevel = 0; }
		bool				GetAffectedEunhyung() const { return m_dwAffectedEunhyungLevel; }

	private:
		uint32_t				m_dwAffectedEunhyungLevel;

		//
		// Skill levels
		//
	protected:
		std::unique_ptr<TPlayerSkill[]>	m_pSkillLevels;
		std::unordered_map<uint8_t, int>		m_SkillDamageBonus;
		std::map<int, TSkillUseInfo>	m_SkillUseInfo;

		////////////////////////////////////////////////////////////////////////////////////////
		// AI related
	public:
		void			AssignTriggers(const TMobTable * table);
		CHARACTER *		GetVictim() const;	// Returns the target to attack
		void			SetVictim(CHARACTER * pkVictim);
		CHARACTER *		GetNearestVictim(CHARACTER * pkChr);
		CHARACTER *		GetProtege() const;	// Returns the target to protect

		bool			Follow(CHARACTER * pkChr, float fMinimumDistance = 150.0f);
		bool			Return();
		bool			IsGuardNPC() const;
		bool			IsChangeAttackPosition(CHARACTER * target) const;
		void			ResetChangeAttackPositionTime() { m_dwLastChangeAttackPositionTime = get_dword_time() - AI_CHANGE_ATTACK_POISITION_TIME_NEAR;}
		void			SetChangeAttackPositionTime() { m_dwLastChangeAttackPositionTime = get_dword_time();}

		void			OnClick(CHARACTER * pkChrCauser);

		VID				m_kVIDVictim;

	protected:
		uint32_t			m_dwLastChangeAttackPositionTime;
		CTrigger		m_triggerOnClick;
		// End of AI

		////////////////////////////////////////////////////////////////////////////////////////
		// Target
	protected:
		CHARACTER *				m_pkChrTarget;		// My target
		CHARACTER_SET	m_set_pkChrTargetedBy;	// Those who have me as their target

	public:
		void				SetTarget(CHARACTER * pkChrTarget);
		void				BroadcastTargetPacket();
		void				ClearTarget();
		void				CheckTarget();
		CHARACTER *			GetTarget() const { return m_pkChrTarget; }

		////////////////////////////////////////////////////////////////////////////////////////
		// Safebox
	public:
		int					GetSafeboxSize() const;
		void				QuerySafeboxSize();
		void				SetSafeboxSize(int size);

		CSafebox *			GetSafebox() const;
		void				LoadSafebox(int iSize, uint32_t dwGold, int iItemCount, TPlayerItem * pItems);
		void				ChangeSafeboxSize(uint8_t bSize);
		void				CloseSafebox();

		/// Safebox open request
		/**
		 * @param [in]	pszPassword safebox password of 1 to 6 characters
		 *
		 * Requests the DB to open the safebox.
		 * The safebox cannot be opened twice, and cannot be opened within 10 seconds of last closing it.
		 */
		void				ReqSafeboxLoad(const char* pszPassword);

		/// Cancels a safebox open request
		/**
		 * After calling ReqSafeboxLoad without CloseSafebox, calling this function allows the safebox to be opened.
		 * If the safebox open request gets a failure response from the DB server, this function allows requesting again.
		 */
		void				CancelSafeboxLoad( void ) { m_bOpeningSafebox = false; }

		void				SetMallLoadTime(int t) { m_iMallLoadTime = t; }
		int					GetMallLoadTime() const { return m_iMallLoadTime; }

		CSafebox *			GetMall() const;
		void				LoadMall(int iItemCount, TPlayerItem * pItems);
		void				CloseMall();

		void				SetSafeboxOpenPosition();
		float				GetDistanceFromSafeboxOpen() const;

	protected:
		std::unique_ptr<CSafebox>	m_pkSafebox;
		int					m_iSafeboxSize;
		int					m_iSafeboxLoadTime;
		bool				m_bOpeningSafebox;	///< Whether the safebox is being requested or open; true means requested or open.

		std::unique_ptr<CSafebox>	m_pkMall;
		int					m_iMallLoadTime;

		PIXEL_POSITION		m_posSafeboxOpen;

		////////////////////////////////////////////////////////////////////////////////////////

		////////////////////////////////////////////////////////////////////////////////////////
		// Mounting
	public:
		void				MountVnum(uint32_t vnum);
		uint32_t				GetMountVnum() const { return m_dwMountVnum; }
		uint32_t				GetLastMountTime() const { return m_dwMountTime; }

		bool				CanUseHorseSkill();

		// Horse
		void		SetHorseLevel(int iLevel) override;

		bool		StartRiding() override;
		bool		StopRiding() override;

		uint32_t		GetMyHorseVnum() const override;

		void		HorseDie() override;
		bool		ReviveHorse() override;

		void		SendHorseInfo() override;
		void		ClearHorseInfo() override;

		void				HorseSummon(bool bSummon, bool bFromFar = false, uint32_t dwVnum = 0, const char* name = nullptr);

		CHARACTER *			GetHorse() const			{ return m_chHorse; }	 // Currently summoned horse
		CHARACTER *			GetRider() const; // rider on horse
		void				SetRider(CHARACTER * ch);

		bool				IsRiding() const;

	public:
		CPetSystem*			GetPetSystem()				{ return m_petSystem.get(); }

	protected:
		std::unique_ptr<CPetSystem>	m_petSystem;

	public:

	protected:
		CHARACTER *			m_chHorse;
		CHARACTER *			m_chRider;

		uint32_t				m_dwMountVnum;
		uint32_t				m_dwMountTime;

		uint8_t				m_bSendHorseLevel;
		uint8_t				m_bSendHorseHealthGrade;
		uint8_t				m_bSendHorseStaminaGrade;

		////////////////////////////////////////////////////////////////////////////////////////
		// Detailed Log
	public:
		void				DetailLog() { m_bDetailLog = !m_bDetailLog; }
		void				ToggleMonsterLog();
		void				MonsterLog(const char* format, ...);
	private:
		bool				m_bDetailLog;
		bool				m_bMonsterLog;

		////////////////////////////////////////////////////////////////////////////////////////
		// Empire

	public:
		void 				SetEmpire(uint8_t bEmpire);
		uint8_t				GetEmpire() const { return m_bEmpire; }

	protected:
		uint8_t				m_bEmpire;

		////////////////////////////////////////////////////////////////////////////////////////
		// Regen
	public:
		void				SetRegen(REGEN * pkRegen);

	protected:
		PIXEL_POSITION			m_posRegen;
		REGEN *				m_pkRegen;
		size_t				regen_id_; // to help dungeon regen identification
		// End of Regen

		////////////////////////////////////////////////////////////////////////////////////////
		// Resists & Proofs
	public:
		bool				CannotMoveByAffect() const;	// Is it unable to move due to a specific effect?
		bool				IsImmune(uint32_t dwImmuneFlag);
		void				SetImmuneFlag(uint32_t dw) { m_pointsInstant.dwImmuneFlag = dw; }

	protected:
		void				ApplyMobAttribute(const TMobTable* table);
		// End of Resists & Proofs

		////////////////////////////////////////////////////////////////////////////////////////
		// QUEST
		// 
	public:
		void				SetQuestNPCID(uint32_t vid);
		uint32_t				GetQuestNPCID() const { return m_dwQuestNPCVID; }
		CHARACTER *			GetQuestNPC() const;

		void				SetQuestItemPtr(CItem * item);
		void				ClearQuestItemPtr();
		CItem *				GetQuestItemPtr() const;

		void				SetQuestBy(uint32_t dwQuestVnum)	{ m_dwQuestByVnum = dwQuestVnum; }
		uint32_t				GetQuestBy() const			{ return m_dwQuestByVnum; }

		int					GetQuestFlag(const std::string& flag) const;
		void				SetQuestFlag(const std::string& flag, int value);

		void				ConfirmWithMsg(const char* szMsg, int iTimeout, uint32_t dwRequestPID);

	private:
		uint32_t				m_dwQuestNPCVID;
		uint32_t				m_dwQuestByVnum;
		CItem *				m_pQuestItem;

		// Events
	public:
		bool				StartStateMachine(int iPulse = 1);
		void				StopStateMachine();
		void				UpdateStateMachine(uint32_t dwPulse);
		void				SetNextStatePulse(int iPulseNext);

		// Character instance update function. Previously, through an odd inheritance structure, CFSM::Update or UpdateStateMachine was called; added a separate update function.
		void				UpdateCharacter(uint32_t dwPulse);

	protected:
		uint32_t				m_dwNextStatePulse;

		// Marriage
	public:
		CHARACTER *			GetMarryPartner() const;
		void				SetMarryPartner(CHARACTER * ch);
		int					GetMarriageBonus(uint32_t dwItemVnum, bool bSum = true);

		void				SetWeddingMap(marriage::WeddingMap* pMap);

	private:
		marriage::WeddingMap* m_pWeddingMap;
		CHARACTER *			m_pkChrMarried;

		// Warp Character
	public:
		void				StartWarpNPCEvent();

	public:
		void				StartSaveEvent();
		void				StartRecoveryEvent();
		void				StartCheckSpeedHackEvent();
		void				StartDestroyWhenIdleEvent();

		LPEVENT				m_pkDeadEvent;
		LPEVENT				m_pkStunEvent;
		LPEVENT				m_pkSaveEvent;
		LPEVENT				m_pkRecoveryEvent;
		LPEVENT				m_pkTimedEvent;
		LPEVENT				m_pkFishingEvent;
		LPEVENT				m_pkAffectEvent;
		LPEVENT				m_pkPoisonEvent;
		LPEVENT				m_pkFireEvent;
		LPEVENT				m_pkWarpNPCEvent;
		//DELAYED_WARP
		//END_DELAYED_WARP

		// MINING
		LPEVENT				m_pkMiningEvent;
		// END_OF_MINING
		LPEVENT				m_pkWarpEvent;
		LPEVENT				m_pkCheckSpeedHackEvent;
		LPEVENT				m_pkDestroyWhenIdleEvent;
		LPEVENT				m_pkPetSystemUpdateEvent;

		bool IsWarping() const { return m_pkWarpEvent ? true : false; }

		bool				m_bHasPoisoned;

		const CMob *		m_pkMobData;
		std::unique_ptr<CMobInstance>	m_pkMobInst;

		std::map<int, LPEVENT> m_mapMobSkillEvent;

		friend struct FuncSplashDamage;
		friend struct FuncSplashAffect;
		friend class CFuncShoot;

	public:
		int				GetPremiumRemainSeconds(uint8_t bType) const;

	private:
		int				m_aiPremiumTimes[PREMIUM_MAX_NUM];

		// CHANGE_ITEM_ATTRIBUTES
		static const uint32_t		msc_dwDefaultChangeItemAttrCycle;	///< Default interval for item attribute changes
		static const char		msc_szLastChangeItemAttrFlag[];		///< Quest Flag name for the time of the last item attribute change
		static const char		msc_szChangeItemAttrCycleFlag[];		///< Quest Flag name for the item attribute change interval
		// END_OF_CHANGE_ITEM_ATTRIBUTES

		// NEW_HAIR_STYLE_ADD
	public :
		bool ItemProcess_Hair(CItem * item, int iDestCell);
		// END_NEW_HAIR_STYLE_ADD

	public :
		void ClearSkill();
		void ClearSubSkill();

		// RESET_ONE_SKILL
		bool ResetOneSkill(uint32_t dwVnum);
		// END_RESET_ONE_SKILL

	private :
		void SendDamagePacket(CHARACTER * pAttacker, int Damage, uint8_t DamageFlag);

	// ARENA
	private :
		CArena *m_pArena;
		bool m_ArenaObserver;
		int m_nPotionLimit;

	public :
		void 	SetArena(CArena* pArena) { m_pArena = pArena; }
		void	SetArenaObserverMode(bool flag) { m_ArenaObserver = flag; }

		CArena* GetArena() const { return m_pArena; }
		bool	GetArenaObserverMode() const { return m_ArenaObserver; }

		void	SetPotionLimit(int count) { m_nPotionLimit = count; }
		int		GetPotionLimit() const { return m_nPotionLimit; }
	// END_ARENA

		//PREVENT_TRADE_WINDOW
	public:
		bool	IsOpenSafebox() const { return m_isOpenSafebox ? true : false; }
		void 	SetOpenSafebox(bool b) { m_isOpenSafebox = b; }

		int		GetSafeboxLoadTime() const { return m_iSafeboxLoadTime; }
		void	SetSafeboxLoadTime() { m_iSafeboxLoadTime = thecore_pulse(); }
		//END_PREVENT_TRADE_WINDOW
	private:
		bool	m_isOpenSafebox;

	public:
		int		GetSkillPowerByLevel(int level, bool bMob = false) const;
		
		//PREVENT_REFINE_HACK
		int		GetRefineTime() const { return m_iRefineTime; }
		void	SetRefineTime() { m_iRefineTime = thecore_pulse(); } 
		int		m_iRefineTime;
		//END_PREVENT_REFINE_HACK

		//END_RESTRICT_USE_SEED_OR_MOONBOTTLE
		
		//PREVENT_PORTAL_AFTER_EXCHANGE
		int		GetExchangeTime() const { return m_iExchangeTime; }
		void	SetExchangeTime() { m_iExchangeTime = thecore_pulse(); }
		int		m_iExchangeTime;
		//END_PREVENT_PORTAL_AFTER_EXCHANGE
		
		int 	m_iMyShopTime;
		int		GetMyShopTime() const	{ return m_iMyShopTime; }
		void	SetMyShopTime() { m_iMyShopTime = thecore_pulse(); }

		// Check to prevent hacks.
		bool	IsHack(bool bSendMsg = true, bool bCheckShopOwner = true, int limittime = g_nPortalLimitTime);

		// MONARCH
		bool	IsMonarch() const;
		// END_MONARCH

		enum MONARCH_COOLTIME
		{
			MC_HEAL = 10,
			MC_WARP	= 60,
			MC_TRANSFER = 60,
			MC_TAX = (60 * 60 * 24 * 7),
			MC_SUMMON = (60 * 60),
		};

		enum MONARCH_INDEX
		{ 
			MI_HEAL = 0,
			MI_WARP,
			MI_TRANSFER,
			MI_TAX,
			MI_SUMMON,
			MI_MAX
		};

		uint32_t m_dwMonarchCooltime[MI_MAX];
		uint32_t m_dwMonarchCooltimelimit[MI_MAX];

		void  InitMC();
		uint32_t GetMC(enum MONARCH_INDEX e) const;
		void SetMC(enum MONARCH_INDEX e);
		bool IsMCOK(enum MONARCH_INDEX e) const;
		uint32_t GetMCL(enum MONARCH_INDEX e) const;
		uint32_t GetMCLTime(enum MONARCH_INDEX e) const;

	public:
		bool ItemProcess_Polymorph(CItem * item);

		// by mhh
		CItem **	GetCubeItem() { return m_pointsInstant.pCubeItems; }
		bool IsCubeOpen () const	{ return (m_pointsInstant.pCubeNpc?true:false); }
		void SetCubeNpc(CHARACTER * npc)	{ m_pointsInstant.pCubeNpc = npc; }
		bool CanDoCube() const;

	public:

	private:
		int		m_deposit_pulse;

	public:
		void	UpdateDepositPulse();
		bool	CanDeposit() const;

	private:
		void	__OpenPrivateShop();

	public:
		struct AttackedLog
		{
			uint32_t 	dwPID;
			uint32_t	dwAttackedTime;
			
			AttackedLog() : dwPID(0), dwAttackedTime(0)
			{
			}
		};

		AttackLog	m_kAttackLog;
		AttackedLog m_AttackedLog;
		int			m_speed_hack_count;

	private :
		std::string m_strNewName;

	public :
		const std::string GetNewName() const { return this->m_strNewName; }
		void SetNewName(const std::string name) { this->m_strNewName = name; }

	public :
		void GoHome();

	private :
		std::set<uint32_t>	m_known_guild;

	public :
		void SendGuildName(CGuild* pGuild);
		void SendGuildName(uint32_t dwGuildID);

	private :
		uint32_t m_dwLogOffInterval;

	public :
		uint32_t GetLogOffInterval() const { return m_dwLogOffInterval; }

	public:
		bool UnEquipSpecialRideUniqueItem ();

		bool CanWarp () const;

	private:
		uint32_t m_dwLastGoldDropTime;

		// bool GetHackShieldCheckMode() const { return m_HackShieldCheckMode; }
		// void SetHackShieldCheckMode(bool m) { m_HackShieldCheckMode = m; }

	public:
		void AutoRecoveryItemProcess (const EAffectTypes);

	public:
		void BuffOnAttr_AddBuffsFromItem(CItem * pItem);
		void BuffOnAttr_RemoveBuffsFromItem(CItem * pItem);

	private:
		void BuffOnAttr_ValueChange(uint8_t bType, uint8_t bOldValue, uint8_t bNewValue);
		void BuffOnAttr_ClearAll();

		typedef std::map <uint8_t, std::unique_ptr<CBuffOnAttributes>> TMapBuffOnAttrs;
		TMapBuffOnAttrs m_map_buff_on_attrs;
		// Invincible: for smoother testing.
	public:
		void SetArmada() { cannot_dead = true; }
		void ResetArmada() { cannot_dead = false; }
	private:
		bool cannot_dead;

	private:
		bool m_bIsPet;
	public:
		void SetPet() { m_bIsPet = true; }
		bool IsPet() { return m_bIsPet; }

	//Final damage adjustment.
	private:
		float m_fAttMul;
		float m_fDamMul;
	public:
		float GetAttMul() { return this->m_fAttMul; }
		void SetAttMul(float newAttMul) {this->m_fAttMul = newAttMul; }
		float GetDamMul() { return this->m_fDamMul; }
		void SetDamMul(float newDamMul) {this->m_fDamMul = newDamMul; }

	private:
		bool IsValidItemPosition(TItemPos Pos) const;

		//Temporary storage of German gift feature packet
	private:
		unsigned int itemAward_vnum;
		char		 itemAward_cmd[20];
	public:
		unsigned int GetItemAward_vnum() { return itemAward_vnum; }
		char*		 GetItemAward_cmd() { return itemAward_cmd;	  }
		//bool		 GetItemAward_flag() { return itemAward_flag; }
		void		 SetItemAward_vnum(unsigned int vnum) { itemAward_vnum = vnum; }
		void		 SetItemAward_cmd(const char* cmd) { strlcpy(itemAward_cmd, cmd, sizeof(itemAward_cmd)); }
		//void		 SetItemAward_flag(bool flag) { itemAward_flag = flag; }

	public:
		//Dragon soul
		
		// DragonSoul_Initialize must not be called before the character's affects and quests are loaded.
		// Affects are loaded last, so it is called from LoadAffect.
		void	DragonSoul_Initialize();

		bool	DragonSoul_IsQualified() const;
		void	DragonSoul_GiveQualification();

		int		DragonSoul_GetActiveDeck() const;
		bool	DragonSoul_IsDeckActivated() const;
		bool	DragonSoul_ActivateDeck(int deck_idx);

		void	DragonSoul_DeactivateAll();
		// Must be called before ClearItem.
		// Because....
		// Each time a dragon soul stone is deactivated, it checks whether the deck still has an active one,
		// and if there are no active dragon soul stones, removes the character's dragon soul affect and active state.
		// 
		// But ClearItem unequips every item the character is wearing,
		// so the dragon soul affect is removed, and in the end the dragon soul is not activated on login.
		// (When unequipping, there is no way to know whether we are logging out.)
		// Only deactivates the dragon soul stones and leaves the character's dragon soul deck active state untouched.
		void	DragonSoul_CleanUp();
		// Dragon soul refine window
	public:
		bool		DragonSoul_RefineWindow_Open(CEntity * pEntity);
		bool		DragonSoul_RefineWindow_Close();
		CEntity *	DragonSoul_RefineWindow_GetOpener() { return  m_pointsInstant.m_pDragonSoulRefineWindowOpener; }
		bool		DragonSoul_RefineWindow_CanRefine();

	private:
		// To defend against hacks that abuse SyncPosition to send other users to odd places,
		// record when SyncPosition happens.
		timeval		m_tvLastSyncTime;
		int			m_iSyncHackCount;
	public:
		void			SetLastSyncTime(const timeval &tv) { memcpy(&m_tvLastSyncTime, &tv, sizeof(timeval)); }
		const timeval&	GetLastSyncTime() { return m_tvLastSyncTime; }
		void			SetSyncHackCount(int iCount) { m_iSyncHackCount = iCount;}
		int				GetSyncHackCount() { return m_iSyncHackCount; }
	public:
		int					LastDropTime;
		int					CountDrops;
		void ClearPMCounter(void)       { m_iPMCounter = 0;      } 
		void IncreasePMCounter(void)    { m_iPMCounter++;        }
		void SetLastPMPulse(void);
		int  GetPMCounter(void)   const { return m_iPMCounter;   }
		int  GetLastPMPulse(void) const { return m_iLastPMPulse; }
	protected:
		int m_iLastPMPulse;
		int m_iPMCounter;
};

ESex GET_SEX(CHARACTER * ch);

extern bool IS_SUMMONABLE_ZONE(int map_index);
extern CHARACTER * FindVictim(CHARACTER * pkChr, int iMaxDistance);
