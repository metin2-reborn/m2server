#pragma once

#include <metin2/proto/length.h>

#include <cstdint>

namespace metin2::inline proto {

// Internal game/DB routing capacity; all processes must use the same build.
constexpr int MAX_MAPS_PER_SERVER = 128;

/**
 * @version 05/06/10	Bang2ni - Added Myshop Pricelist packets HEADER_XX_MYSHOP_PRICELIST_XXX
 */
enum {
    HEADER_GD_LOGIN = 1,
    HEADER_GD_LOGOUT = 2,

    HEADER_GD_PLAYER_LOAD = 3,
    HEADER_GD_PLAYER_SAVE = 4,
    HEADER_GD_PLAYER_CREATE = 5,
    HEADER_GD_PLAYER_DELETE = 6,

    // 8 empty
    HEADER_GD_BOOT = 9,
    HEADER_GD_PLAYER_COUNT = 10,
    HEADER_GD_QUEST_SAVE = 11,
    HEADER_GD_SAFEBOX_LOAD = 12,
    HEADER_GD_SAFEBOX_SAVE = 13,
    HEADER_GD_SAFEBOX_CHANGE_SIZE = 14,
    HEADER_GD_EMPIRE_SELECT = 15,

    HEADER_GD_SAFEBOX_CHANGE_PASSWORD = 16,

    HEADER_GD_GUILD_SKILL_UPDATE = 19,
    HEADER_GD_GUILD_EXP_UPDATE = 20,
    HEADER_GD_GUILD_ADD_MEMBER = 21,
    HEADER_GD_GUILD_REMOVE_MEMBER = 22,
    HEADER_GD_GUILD_CHANGE_GRADE = 23,
    HEADER_GD_GUILD_CHANGE_MEMBER_DATA = 24,
    HEADER_GD_GUILD_DISBAND = 25,
    HEADER_GD_GUILD_WAR = 26,
    HEADER_GD_GUILD_WAR_SCORE = 27,
    HEADER_GD_GUILD_CREATE = 28,

    HEADER_GD_ITEM_SAVE = 30,
    HEADER_GD_ITEM_DESTROY = 31,

    HEADER_GD_ADD_AFFECT = 32,
    HEADER_GD_REMOVE_AFFECT = 33,

    HEADER_GD_HIGHSCORE_REGISTER = 34,
    HEADER_GD_ITEM_FLUSH = 35,

    HEADER_GD_PARTY_CREATE = 36,
    HEADER_GD_PARTY_DELETE = 37,
    HEADER_GD_PARTY_ADD = 38,
    HEADER_GD_PARTY_REMOVE = 39,
    HEADER_GD_PARTY_STATE_CHANGE = 40,

    HEADER_GD_FLUSH_CACHE = 42,
    HEADER_GD_RELOAD_PROTO = 43,

    HEADER_GD_CHANGE_NAME = 44,
    HEADER_GD_SMS = 45,

    HEADER_GD_GUILD_CHANGE_LADDER_POINT = 46,
    HEADER_GD_GUILD_USE_SKILL = 47,

    HEADER_GD_REQUEST_EMPIRE_PRIV = 48,
    HEADER_GD_REQUEST_GUILD_PRIV = 49,

    HEADER_GD_MONEY_LOG = 50,

    HEADER_GD_GUILD_DEPOSIT_MONEY = 51,
    HEADER_GD_GUILD_WITHDRAW_MONEY = 52,
    HEADER_GD_GUILD_WITHDRAW_MONEY_GIVE_REPLY = 53,

    HEADER_GD_REQUEST_CHARACTER_PRIV = 54,

    HEADER_GD_SET_EVENT_FLAG = 55,

    HEADER_GD_PARTY_SET_MEMBER_LEVEL = 56,

    HEADER_GD_GUILD_WAR_BET = 57,

    HEADER_GD_CREATE_OBJECT = 60,
    HEADER_GD_DELETE_OBJECT = 61,
    HEADER_GD_UPDATE_LAND = 62,

    HEADER_GD_MARRIAGE_ADD = 70,
    HEADER_GD_MARRIAGE_UPDATE = 71,
    HEADER_GD_MARRIAGE_REMOVE = 72,

    HEADER_GD_WEDDING_REQUEST = 73,
    HEADER_GD_WEDDING_READY = 74,
    HEADER_GD_WEDDING_END = 75,

    HEADER_GD_AUTH_LOGIN = 100,
    HEADER_GD_LOGIN_BY_KEY = 101,
    HEADER_GD_MALL_LOAD = 107,

    HEADER_GD_MYSHOP_PRICELIST_UPDATE = 108, ///< Price info update request
    HEADER_GD_MYSHOP_PRICELIST_REQ = 109,    ///< Price info list request

    HEADER_GD_BLOCK_CHAT = 110,

    HEADER_GD_HAMMER_OF_TOR = 114,
    HEADER_GD_RELOAD_ADMIN = 115,       ///< GM info request
    HEADER_GD_BREAK_MARRIAGE = 116,     ///< Divorce
    HEADER_GD_ELECT_MONARCH = 117,      ///< Monarch vote
    HEADER_GD_CANDIDACY = 118,          ///< Monarch candidacy
    HEADER_GD_ADD_MONARCH_MONEY = 119,  ///< Monarch money increase
    HEADER_GD_TAKE_MONARCH_MONEY = 120, ///< Monarch money decrease
    HEADER_GD_COME_TO_VOTE = 121,       ///< Vote
    HEADER_GD_RMCANDIDACY = 122,        ///< Remove candidate (GM)
    HEADER_GD_SETMONARCH = 123,         ///< Set monarch (GM)
    HEADER_GD_RMMONARCH = 124,          ///< Delete monarch
    HEADER_GD_DEC_MONARCH_MONEY = 125,

    HEADER_GD_CHANGE_MONARCH_LORD = 126,

    HEADER_GD_REQ_CHANGE_GUILD_MASTER = 129,

    HEADER_GD_REQ_SPARE_ITEM_ID_RANGE = 130,

    HEADER_GD_UPDATE_HORSE_NAME = 131,
    HEADER_GD_REQ_HORSE_NAME = 132,

    HEADER_GD_DC = 133, // Delete the login key

    HEADER_GD_VALID_LOGOUT = 134,

    // AUCTION

    HEADER_GD_REQUEST_CHARGE_CASH = 137,

    HEADER_GD_DELETE_AWARDID = 138, // delete gift notify icon

    HEADER_GD_UPDATE_CHANNELSTATUS = 139,
    HEADER_GD_REQUEST_CHANNELSTATUS = 140,

    HEADER_GD_SETUP = 0xff,

    ///////////////////////////////////////////////
    HEADER_DG_NOTICE = 1,

    HEADER_DG_LOGIN_SUCCESS = 30,
    HEADER_DG_LOGIN_NOT_EXIST = 31,
    HEADER_DG_LOGIN_WRONG_PASSWD = 33,
    HEADER_DG_LOGIN_ALREADY = 34,

    HEADER_DG_PLAYER_LOAD_SUCCESS = 35,
    HEADER_DG_PLAYER_LOAD_FAILED = 36,
    HEADER_DG_PLAYER_CREATE_SUCCESS = 37,
    HEADER_DG_PLAYER_CREATE_ALREADY = 38,
    HEADER_DG_PLAYER_CREATE_FAILED = 39,
    HEADER_DG_PLAYER_DELETE_SUCCESS = 40,
    HEADER_DG_PLAYER_DELETE_FAILED = 41,

    HEADER_DG_ITEM_LOAD = 42,

    HEADER_DG_BOOT = 43,
    HEADER_DG_QUEST_LOAD = 44,

    HEADER_DG_SAFEBOX_LOAD = 45,
    HEADER_DG_SAFEBOX_CHANGE_SIZE = 46,
    HEADER_DG_SAFEBOX_WRONG_PASSWORD = 47,
    HEADER_DG_SAFEBOX_CHANGE_PASSWORD_ANSWER = 48,

    HEADER_DG_EMPIRE_SELECT = 49,

    HEADER_DG_AFFECT_LOAD = 50,
    HEADER_DG_MALL_LOAD = 51,

    HEADER_DG_GUILD_SKILL_UPDATE = 56,
    HEADER_DG_GUILD_SKILL_RECHARGE = 57,
    HEADER_DG_GUILD_EXP_UPDATE = 58,

    HEADER_DG_PARTY_CREATE = 59,
    HEADER_DG_PARTY_DELETE = 60,
    HEADER_DG_PARTY_ADD = 61,
    HEADER_DG_PARTY_REMOVE = 62,
    HEADER_DG_PARTY_STATE_CHANGE = 63,
    HEADER_DG_PARTY_SET_MEMBER_LEVEL = 65,

    HEADER_DG_TIME = 90,

    HEADER_DG_GUILD_ADD_MEMBER = 92,
    HEADER_DG_GUILD_REMOVE_MEMBER = 93,
    HEADER_DG_GUILD_CHANGE_GRADE = 94,
    HEADER_DG_GUILD_CHANGE_MEMBER_DATA = 95,
    HEADER_DG_GUILD_DISBAND = 96,
    HEADER_DG_GUILD_WAR = 97,
    HEADER_DG_GUILD_WAR_SCORE = 98,
    HEADER_DG_GUILD_LOAD = 100,
    HEADER_DG_GUILD_LADDER = 101,
    HEADER_DG_GUILD_SKILL_USABLE_CHANGE = 102,
    HEADER_DG_GUILD_MONEY_CHANGE = 103,
    HEADER_DG_GUILD_WITHDRAW_MONEY_GIVE = 104,

    HEADER_DG_SET_EVENT_FLAG = 105,

    HEADER_DG_GUILD_WAR_RESERVE_ADD = 106,
    HEADER_DG_GUILD_WAR_RESERVE_DEL = 107,
    HEADER_DG_GUILD_WAR_BET = 108,

    HEADER_DG_RELOAD_PROTO = 120,
    HEADER_DG_CHANGE_NAME = 121,

    HEADER_DG_AUTH_LOGIN = 122,

    HEADER_DG_CHANGE_EMPIRE_PRIV = 124,
    HEADER_DG_CHANGE_GUILD_PRIV = 125,

    HEADER_DG_MONEY_LOG = 126,

    HEADER_DG_CHANGE_CHARACTER_PRIV = 127,

    HEADER_DG_CREATE_OBJECT = 140,
    HEADER_DG_DELETE_OBJECT = 141,
    HEADER_DG_UPDATE_LAND = 142,

    HEADER_DG_MARRIAGE_ADD = 150,
    HEADER_DG_MARRIAGE_UPDATE = 151,
    HEADER_DG_MARRIAGE_REMOVE = 152,

    HEADER_DG_WEDDING_REQUEST = 153,
    HEADER_DG_WEDDING_READY = 154,
    HEADER_DG_WEDDING_START = 155,
    HEADER_DG_WEDDING_END = 156,

    HEADER_DG_MYSHOP_PRICELIST_RES = 157, ///< Price info list response
    HEADER_DG_RELOAD_ADMIN = 158,         ///< GM info reload
    HEADER_DG_ELECT_MONARCH = 160,        ///< Monarch vote
    HEADER_DG_CANDIDACY = 161,            ///< Monarch candidacy
    HEADER_DG_ADD_MONARCH_MONEY = 162,    ///< Monarch money increase
    HEADER_DG_TAKE_MONARCH_MONEY = 163,   ///< Monarch money decrease
    HEADER_DG_RMCANDIDACY = 165,          ///< Remove candidate (GM)
    HEADER_DG_RMMONARCH = 167,            ///< Delete monarch
    HEADER_DG_DEC_MONARCH_MONEY = 168,

    HEADER_DG_CHANGE_MONARCH_LORD_ACK = 169,
    HEADER_DG_UPDATE_MONARCH_INFO = 170,

    HEADER_DG_ACK_CHANGE_GUILD_MASTER = 173,

    HEADER_DG_ACK_SPARE_ITEM_ID_RANGE = 174,

    HEADER_DG_UPDATE_HORSE_NAME = 175,
    HEADER_DG_ACK_HORSE_NAME = 176,

    HEADER_DG_NEED_LOGIN_LOG = 177,
    HEADER_DG_ITEMAWARD_INFORMER = 180, // gift notify
    HEADER_DG_RESPOND_CHANNELSTATUS = 181,

    HEADER_DG_MAP_LOCATIONS = 0xfe,
    HEADER_DG_P2P = 0xff,

};

/* ----------------------------------------------
 * table
 * ----------------------------------------------
 */

/* game Server -> DB Server */
#pragma pack(1)
enum ERequestChargeType {
    ERequestCharge_Cash = 0,
    ERequestCharge_Mileage,
};

struct TRequestChargeCash {
    uint32_t dwAID; // id(primary key) - Account Table
    uint32_t dwAmount;
    ERequestChargeType eChargeType;

};

struct TSimplePlayer {
    uint32_t dwID;
    char szName[CHARACTER_NAME_MAX_LEN + 1];
    uint8_t byJob;
    uint8_t byLevel;
    uint32_t dwPlayMinutes;
    uint8_t byST, byHT, byDX, byIQ;
    uint16_t wMainPart;
    uint8_t bChangeName;
    uint16_t wHairPart;
    uint8_t bDummy[4];
    int32_t x, y;
    int32_t lAddr;
    uint16_t wPort;
    uint8_t skill_group;
};

struct TAccountTable {
    uint32_t id;
    char login[LOGIN_MAX_LEN + 1];
    char passwd[PASSWD_MAX_LEN + 1];
    char social_id[SOCIAL_ID_MAX_LEN + 1];
    char status[ACCOUNT_STATUS_MAX_LEN + 1];
    uint8_t bEmpire;
    TSimplePlayer players[PLAYER_PER_ACCOUNT];
};

struct TPacketDGCreateSuccess {
    uint8_t bAccountCharacterIndex;
    TSimplePlayer player;
};

struct TPlayerItemAttribute {
    uint8_t bType;
    short sValue;
};

struct TPlayerItem {
    uint32_t id;
    uint8_t window;
    uint16_t pos;
    uint32_t count;

    uint32_t vnum;
    int32_t alSockets[ITEM_SOCKET_MAX_NUM]; // Socket number

    TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];

    uint32_t owner;
};

struct TQuickslot {
    uint8_t type;
    uint8_t pos;
};

struct TPlayerSkill {
    uint8_t bMasterType;
    uint8_t bLevel;
    int32_t tNextRead;
};

struct THorseInfo {
    uint8_t bLevel;
    uint8_t bRiding;
    short sStamina;
    short sHealth;
    uint32_t dwHorseHealthDropTime;
};

struct TPlayerTable {
    uint32_t id;

    char name[CHARACTER_NAME_MAX_LEN + 1];
    char ip[IP_ADDRESS_LENGTH + 1];

    uint16_t job;
    uint8_t voice;

    uint8_t level;
    uint8_t level_step;
    short st, ht, dx, iq;

    uint32_t exp;
    int32_t gold;

    uint8_t dir;
    int32_t x, y, z;
    int32_t lMapIndex;

    int32_t lExitX, lExitY;
    int32_t lExitMapIndex;

    // Fix
    int hp;
    int sp;

    int sRandomHP;
    int sRandomSP;

    int playtime;

    short stat_point;
    short skill_point;
    short sub_skill_point;
    short horse_skill_point;

    TPlayerSkill skills[SKILL_MAX_NUM];

    TQuickslot quickslot[QUICKSLOT_MAX_NUM];

    uint8_t part_base;
    uint16_t parts[PART_MAX_NUM];

    short stamina;

    uint8_t skill_group;
    int32_t lAlignment;
    char szMobile[MOBILE_MAX_LEN + 1];

    short stat_reset_count;

    THorseInfo horse;

    uint32_t logoff_interval;

    int aiPremiumTimes[PREMIUM_MAX_NUM];
};

struct TMobSkillLevel {
    uint32_t dwVnum;
    uint8_t bLevel;
};

struct TEntityTable {
    uint32_t dwVnum;
};

struct TMobTable : public TEntityTable {
    char szName[PROTO_NAME_MAX_LEN + 1];
    char szLocaleName[PROTO_NAME_MAX_LEN + 1];

    uint8_t bType;       // Monster, NPC
    uint8_t bRank;       // PAWN, KNIGHT, KING
    uint8_t bBattleType; // MELEE, etc..
    uint8_t bLevel;      // Level
    uint8_t bSize;

    uint32_t dwGoldMin;
    uint32_t dwGoldMax;
    uint32_t dwExp;
    uint32_t dwMaxHP;
    uint8_t bRegenCycle;
    uint8_t bRegenPercent;
    uint16_t wDef;

    uint32_t dwAIFlag;
    uint32_t dwRaceFlag;
    uint32_t dwImmuneFlag;

    uint8_t bStr, bDex, bCon, bInt;
    uint32_t dwDamageRange[2];

    short sAttackSpeed;
    short sMovingSpeed;
    uint8_t bAggresiveHPPct;
    uint16_t wAggressiveSight;
    uint16_t wAttackRange;

    char cEnchants[MOB_ENCHANTS_MAX_NUM];
    char cResists[MOB_RESISTS_MAX_NUM];

    uint32_t dwResurrectionVnum;
    uint32_t dwDropItemVnum;

    uint8_t bMountCapacity;
    uint8_t bOnClickType;

    uint8_t bEmpire;
    char szFolder[64 + 1];

    float fDamMultiply;

    uint32_t dwSummonVnum;
    uint32_t dwDrainSP;
    uint32_t dwMobColor;
    uint32_t dwPolymorphItemVnum;

    TMobSkillLevel Skills[MOB_SKILL_MAX_NUM];

    uint8_t bBerserkPoint;
    uint8_t bStoneSkinPoint;
    uint8_t bGodSpeedPoint;
    uint8_t bDeathBlowPoint;
    uint8_t bRevivePoint;
};

struct TSkillTable {
    uint32_t dwVnum;
    char szName[32 + 1];
    uint8_t bType;
    uint8_t bMaxLevel;
    uint32_t dwSplashRange;

    char szPointOn[64];
    char szPointPoly[100 + 1];
    char szSPCostPoly[100 + 1];
    char szDurationPoly[100 + 1];
    char szDurationSPCostPoly[100 + 1];
    char szCooldownPoly[100 + 1];
    char szMasterBonusPoly[100 + 1];
    char szGrandMasterAddSPCostPoly[100 + 1];
    uint32_t dwFlag;
    uint32_t dwAffectFlag;

    // Data for secondary skill
    char szPointOn2[64];
    char szPointPoly2[100 + 1];
    char szDurationPoly2[100 + 1];
    uint32_t dwAffectFlag2;

    // Data for grand master point
    char szPointOn3[64];
    char szPointPoly3[100 + 1];
    char szDurationPoly3[100 + 1];

    uint8_t bLevelStep;
    uint8_t bLevelLimit;
    uint32_t preSkillVnum;
    uint8_t preSkillLevel;

    int32_t lMaxHit;
    char szSplashAroundDamageAdjustPoly[100 + 1];

    uint8_t bSkillAttrType;

    uint32_t dwTargetRange;
};

struct TShopItemTable {
    uint32_t vnum;
    uint8_t count;

    TItemPos pos;        // Used only for PC shops
    uint32_t price;      // Used only for PC and shop_table_ex.txt shops
    uint8_t display_pos; // Used only for PC and shop_table_ex.txt shops; display position.
};

struct TShopTable {
    uint32_t dwVnum;
    uint32_t dwNPCVnum;

    uint8_t byItemCount;
    TShopItemTable items[SHOP_HOST_ITEM_MAX_NUM];
};

inline constexpr int QUEST_NAME_MAX_LEN = 32;
inline constexpr int QUEST_STATE_MAX_LEN = 64;

struct TQuestTable {
    uint32_t dwPID;
    char szName[QUEST_NAME_MAX_LEN + 1];
    char szState[QUEST_STATE_MAX_LEN + 1];
    int32_t lValue;
};

struct TItemLimit {
    uint8_t bType;
    int32_t lValue;
};

struct TItemApply {
    uint8_t bType;
    int32_t lValue;
};

struct TItemTable : public TEntityTable {
    uint32_t dwVnumRange;
    char szName[PROTO_NAME_MAX_LEN + 1];
    char szLocaleName[PROTO_NAME_MAX_LEN + 1];
    uint8_t bType;
    uint8_t bSubType;

    uint8_t bWeight;
    uint8_t bSize;

    uint32_t dwAntiFlags;
    uint32_t dwFlags;
    uint32_t dwWearFlags;
    uint32_t dwImmuneFlag;

    uint32_t dwGold;
    uint32_t dwShopBuyPrice;

    TItemLimit aLimits[ITEM_LIMIT_MAX_NUM];
    TItemApply aApplies[ITEM_APPLY_MAX_NUM];
    int32_t alValues[ITEM_VALUES_MAX_NUM];
    int32_t alSockets[ITEM_SOCKET_MAX_NUM];
    uint32_t dwRefinedVnum;
    uint16_t wRefineSet;
    uint8_t bAlterToMagicItemPct;
    uint8_t bSpecular;
    uint8_t bGainSocketPct;

    short int sAddonType; // Base attributes

    // The limit flags below are checked often in realtime and are fixed per item VNUM,
    // but with the current structure, looping up to LIMIT_MAX_NUM for every item each time is costly, so they are stored in advance.
    char cLimitRealTimeFirstUseIndex; // Index of the LIMIT_REAL_TIME_FIRST_USE flag among the item limit fields (-1 if none)
    char cLimitTimerBasedOnWearIndex; // Index of the LIMIT_TIMER_BASED_ON_WEAR flag among the item limit fields (-1 if none)

};

struct TItemAttrTable {
    TItemAttrTable() : szApply{}, dwApplyIndex(0), dwProb(0), lValues{}, bMaxLevelBySet{} {}

    char szApply[APPLY_NAME_MAX_LEN + 1];
    uint32_t dwApplyIndex;
    uint32_t dwProb;
    int32_t lValues[5];
    uint8_t bMaxLevelBySet[ATTRIBUTE_SET_MAX_NUM];
};

struct TLoginPacket {
    char login[LOGIN_MAX_LEN + 1];
    char passwd[PASSWD_MAX_LEN + 1];
};

struct TPlayerLoadPacket {
    uint32_t account_id;
    uint32_t player_id;
    uint8_t account_index; /* position in account */
};

struct TPlayerCreatePacket {
    char login[LOGIN_MAX_LEN + 1];
    char passwd[PASSWD_MAX_LEN + 1];
    uint32_t account_id;
    uint8_t account_index;
    TPlayerTable player_table;
};

struct TPlayerDeletePacket {
    char login[LOGIN_MAX_LEN + 1];
    uint32_t player_id;
    uint8_t account_index;
    char private_code[8];
};

struct TLogoutPacket {
    char login[LOGIN_MAX_LEN + 1];
    char passwd[PASSWD_MAX_LEN + 1];
};

struct TPlayerCountPacket {
    uint32_t dwCount;
};

inline constexpr int SAFEBOX_MAX_NUM = 135;
inline constexpr int SAFEBOX_PASSWORD_MAX_LEN = 6;

struct TSafeboxTable {
    uint32_t dwID;
    uint8_t bSize;
    uint32_t dwGold;
    uint16_t wItemCount;
};

struct TSafeboxChangeSizePacket {
    uint32_t dwID;
    uint8_t bSize;
};

struct TSafeboxLoadPacket {
    uint32_t dwID;
    char szLogin[LOGIN_MAX_LEN + 1];
    char szPassword[SAFEBOX_PASSWORD_MAX_LEN + 1];
};

struct TSafeboxChangePasswordPacket {
    uint32_t dwID;
    char szOldPassword[SAFEBOX_PASSWORD_MAX_LEN + 1];
    char szNewPassword[SAFEBOX_PASSWORD_MAX_LEN + 1];
};

struct TSafeboxChangePasswordPacketAnswer {
    uint8_t flag;
};

struct TEmpireSelectPacket {
    uint32_t dwAccountID;
    uint8_t bEmpire;
};

struct TPacketGDSetup {
    char szPublicIP[16];  // Public IP which listen to users
    uint8_t bChannel;     // Channel
    uint16_t wListenPort; // Port number clients connect to
    uint16_t wP2PPort;    // P2P port number for server-to-server connections
    int32_t alMaps[MAX_MAPS_PER_SERVER];
    uint32_t dwLoginCount;
    uint8_t bAuthServer;
};

struct TMapLocation {
    int32_t alMaps[MAX_MAPS_PER_SERVER];
    char szHost[MAX_HOST_LENGTH + 1];
    uint16_t wPort;
};

struct TPacketDGP2P {
    char szHost[MAX_HOST_LENGTH + 1];
    uint16_t wPort;
    uint8_t bChannel;
};

struct TPacketGuildSkillUpdate {
    uint32_t guild_id;
    int amount;
    uint8_t skill_levels[12];
    uint8_t skill_point;
    uint8_t save;
};

struct TPacketGuildExpUpdate {
    uint32_t guild_id;
    int amount;
};

struct TPacketGuildChangeMemberData {
    uint32_t guild_id;
    uint32_t pid;
    uint32_t offer;
    uint8_t level;
    uint8_t grade;
};

struct TPacketDGLoginAlready {
    char szLogin[LOGIN_MAX_LEN + 1];
};

struct TPacketAffectElement {
    uint32_t dwType;
    uint8_t bApplyOn;
    int32_t lApplyValue;
    uint32_t dwFlag;
    int32_t lDuration;
    int32_t lSPCost;
};

struct TPacketGDAddAffect {
    uint32_t dwPID;
    TPacketAffectElement elem;
};

struct TPacketGDRemoveAffect {
    uint32_t dwPID;
    uint32_t dwType;
    uint8_t bApplyOn;
};

struct TPacketGDHighscore {
    uint32_t dwPID;
    int32_t lValue;
    char cDir;
    char szBoard[21];
};

struct TPacketPartyCreate {
    uint32_t dwLeaderPID;
};

struct TPacketPartyDelete {
    uint32_t dwLeaderPID;
};

struct TPacketPartyAdd {
    uint32_t dwLeaderPID;
    uint32_t dwPID;
    uint8_t bState;
};

struct TPacketPartyRemove {
    uint32_t dwLeaderPID;
    uint32_t dwPID;
};

struct TPacketPartyStateChange {
    uint32_t dwLeaderPID;
    uint32_t dwPID;
    uint8_t bRole;
    uint8_t bFlag;
};

struct TPacketPartySetMemberLevel {
    uint32_t dwLeaderPID;
    uint32_t dwPID;
    uint8_t bLevel;
};

struct TPacketGDBoot {
    uint32_t dwItemIDRange[2];
    char szIP[16];
};

struct TPacketGuild {
    uint32_t dwGuild;
    uint32_t dwInfo;
};

struct TPacketGDGuildAddMember {
    uint32_t dwPID;
    uint32_t dwGuild;
    uint8_t bGrade;
};

struct TPacketDGGuildMember {
    uint32_t dwPID;
    uint32_t dwGuild;
    uint8_t bGrade;
    uint8_t isGeneral;
    uint8_t bJob;
    uint8_t bLevel;
    uint32_t dwOffer;
    char szName[CHARACTER_NAME_MAX_LEN + 1];
};

struct TPacketGuildWar {
    uint8_t bType;
    uint8_t bWar;
    uint32_t dwGuildFrom;
    uint32_t dwGuildTo;
    int32_t lWarPrice;
    int32_t lInitialScore;
};

// Game -> DB : relative change
// DB -> Game : final total value
struct TPacketGuildWarScore {
    uint32_t dwGuildGainPoint;
    uint32_t dwGuildOpponent;
    int32_t lScore;
    int32_t lBetScore;
};

struct TRefineMaterial {
    uint32_t vnum;
    int count;
};

struct TRefineTable {
    uint32_t id;
    uint8_t material_count;
    int cost; // Cost
    int prob; // Probability
    TRefineMaterial materials[REFINE_MATERIAL_MAX_NUM];
};

struct TBanwordTable {
    char szWord[BANWORD_MAX_LEN + 1];
};

struct TPacketGDChangeName {
    uint32_t pid;
    char name[CHARACTER_NAME_MAX_LEN + 1];
};

struct TPacketDGChangeName {
    uint32_t pid;
    char name[CHARACTER_NAME_MAX_LEN + 1];
};

struct TPacketGuildLadder {
    uint32_t dwGuild;
    int32_t lLadderPoint;
    int32_t lWin;
    int32_t lDraw;
    int32_t lLoss;
};

struct TPacketGuildLadderPoint {
    uint32_t dwGuild;
    int32_t lChange;
};

struct TPacketGDSMS {
    char szFrom[CHARACTER_NAME_MAX_LEN + 1];
    char szTo[CHARACTER_NAME_MAX_LEN + 1];
    char szMobile[MOBILE_MAX_LEN + 1];
    char szMsg[SMS_MAX_LEN + 1];
};

struct TPacketGuildUseSkill {
    uint32_t dwGuild;
    uint32_t dwSkillVnum;
    uint32_t dwCooltime;
};

struct TPacketGuildSkillUsableChange {
    uint32_t dwGuild;
    uint32_t dwSkillVnum;
    uint8_t bUsable;
};

struct TPacketGDAuthLogin {
    uint32_t dwID;
    uint32_t dwLoginKey;
    char szLogin[LOGIN_MAX_LEN + 1];
    char szSocialID[SOCIAL_ID_MAX_LEN + 1];
    uint32_t adwClientKey[4];
    int iPremiumTimes[PREMIUM_MAX_NUM];
};

struct TPacketGDLoginByKey {
    char szLogin[LOGIN_MAX_LEN + 1];
    uint32_t dwLoginKey;
    uint32_t adwClientKey[4];
    char szIP[MAX_HOST_LENGTH + 1];
};

/**
 * @version 05/06/08	Bang2ni - added duration
 */
struct TPacketGiveGuildPriv {
    uint8_t type;
    int value;
    uint32_t guild_id;
    int32_t duration_sec; ///< Duration
};
struct TPacketGiveEmpirePriv {
    uint8_t type;
    int value;
    uint8_t empire;
    int32_t duration_sec;
};
struct TPacketGiveCharacterPriv {
    uint8_t type;
    int value;
    uint32_t pid;
};

struct TPacketDGChangeCharacterPriv {
    uint8_t type;
    int value;
    uint32_t pid;
    uint8_t bLog;
};

/**
 * @version 05/06/08	Bang2ni - added duration
 */
struct TPacketDGChangeGuildPriv {
    uint8_t type;
    int value;
    uint32_t guild_id;
    uint8_t bLog;
    int32_t end_time_sec; ///< Duration
};

struct TPacketDGChangeEmpirePriv {
    uint8_t type;
    int value;
    uint8_t empire;
    uint8_t bLog;
    int32_t end_time_sec;
};

struct TPacketMoneyLog {
    uint8_t type;
    uint32_t vnum;
    int32_t gold;
};

struct TPacketGDGuildMoney {
    uint32_t dwGuild;
    int32_t iGold;
};

struct TPacketDGGuildMoneyChange {
    uint32_t dwGuild;
    int32_t iTotalGold;
};

struct TPacketDGGuildMoneyWithdraw {
    uint32_t dwGuild;
    int32_t iChangeGold;
};

struct TPacketGDGuildMoneyWithdrawGiveReply {
    uint32_t dwGuild;
    int32_t iChangeGold;
    uint8_t bGiveSuccess;
};

struct TPacketSetEventFlag {
    char szFlagName[EVENT_FLAG_NAME_MAX_LEN + 1];
    int32_t lValue;
};

struct TPacketLoginOnSetup {
    uint32_t dwID;
    char szLogin[LOGIN_MAX_LEN + 1];
    char szSocialID[SOCIAL_ID_MAX_LEN + 1];
    char szHost[MAX_HOST_LENGTH + 1];
    uint32_t dwLoginKey;
    uint32_t adwClientKey[4];
};

struct TPacketGDCreateObject {
    uint32_t dwVnum;
    uint32_t dwLandID;
    int32_t lMapIndex;
    int32_t x, y;
    float xRot;
    float yRot;
    float zRot;
};

struct TGuildWarReserve {
    uint32_t dwID;
    uint32_t dwGuildFrom;
    uint32_t dwGuildTo;
    uint32_t dwTime;
    uint8_t bType;
    int32_t lWarPrice;
    int32_t lInitialScore;
    bool bStarted;
    uint32_t dwBetFrom;
    uint32_t dwBetTo;
    int32_t lPowerFrom;
    int32_t lPowerTo;
    int32_t lHandicap;
};

struct TPacketGDGuildWarBet {
    uint32_t dwWarID;
    char szLogin[LOGIN_MAX_LEN + 1];
    uint32_t dwGold;
    uint32_t dwGuild;
};

// Marriage

struct TPacketMarriageAdd {
    uint32_t dwPID1;
    uint32_t dwPID2;
    int32_t tMarryTime;
    char szName1[CHARACTER_NAME_MAX_LEN + 1];
    char szName2[CHARACTER_NAME_MAX_LEN + 1];
};

struct TPacketMarriageUpdate {
    uint32_t dwPID1;
    uint32_t dwPID2;
    int32_t iLovePoint;
    uint8_t byMarried;
};

struct TPacketMarriageRemove {
    uint32_t dwPID1;
    uint32_t dwPID2;
};

struct TPacketWeddingRequest {
    uint32_t dwPID1;
    uint32_t dwPID2;
};

struct TPacketWeddingReady {
    uint32_t dwPID1;
    uint32_t dwPID2;
    uint32_t dwMapIndex;
};

struct TPacketWeddingStart {
    uint32_t dwPID1;
    uint32_t dwPID2;
};

struct TPacketWeddingEnd {
    uint32_t dwPID1;
    uint32_t dwPID2;
};

/// Header of private shop price info. Variable-size packet; byCount TItemPriceInfo entries follow.
struct TPacketMyshopPricelistHeader {
    uint32_t dwOwnerID; ///< ID of the player owning the price info
    uint8_t byCount;    ///< Number of price entries
};

/// Price info for a single item in a private shop
struct TItemPriceInfo {
    uint32_t dwVnum;  ///< Item vnum
    uint32_t dwPrice; ///< Price
};

/// Private shop item price info list table
struct TItemPriceListTable {
    uint32_t dwOwnerID; ///< ID of the player owning the price info
    uint8_t byCount;    ///< Number of entries in the price list

    TItemPriceInfo aPriceInfo[SHOP_PRICELIST_MAX_NUM]; ///< Price info list
};

struct TPacketBlockChat {
    char szName[CHARACTER_NAME_MAX_LEN + 1];
    int32_t lDuration;
};

// ADMIN_MANAGER
struct tAdminInfo {
    int m_ID;               // Unique ID
    char m_szAccount[32];   // Account
    char m_szName[32];      // Character name
    char m_szContactIP[16]; // Access IP
    char m_szServerIP[16];  // Server IP
    int m_Authority;        // Authority
};
// END_ADMIN_MANAGER

// BOOT_LOCALIZATION
struct tLocale {
    char szValue[32];
    char szKey[32];
};
// BOOT_LOCALIZATION

// RELOAD_ADMIN
struct TPacketReloadAdmin {
    char szIP[16];
};
// END_RELOAD_ADMIN

struct MonarchInfo {
    uint32_t pid[4];  // Monarch's PID
    int64_t money[4]; // Monarch's separate funds
    char name[4][32]; // Monarch's name
    char date[4][32]; // Monarch registration date
};

struct MonarchElectionInfo {
    uint32_t pid;         // PID of the voter
    uint32_t selectedpid; // PID voted for ( monarch candidate )
    char date[32];        // Vote date
};

// Monarch candidate
struct MonarchCandidacy {
    uint32_t pid;
    char name[32];
    char date[32];
};

struct TPacketChangeMonarchLord {
    uint8_t bEmpire;
    uint32_t dwPID;
};

struct TPacketChangeMonarchLordACK {
    uint8_t bEmpire;
    uint32_t dwPID;
    char szName[32];
    char szDate[32];
};

struct TPacketChangeGuildMaster {
    uint32_t dwGuildID;
    uint32_t idFrom;
    uint32_t idTo;
};

struct TItemIDRangeTable {
    uint32_t dwMin;
    uint32_t dwMax;
    uint32_t dwUsableItemIDMin;
};

struct TPacketUpdateHorseName {
    uint32_t dwPlayerID;
    char szHorseName[CHARACTER_NAME_MAX_LEN + 1];
};

struct TPacketDC {
    char login[LOGIN_MAX_LEN + 1];
};

struct TPacketNeedLoginLogInfo {
    uint32_t dwPlayerID;
};

// Packet info for testing the German gift notification feature
struct TPacketItemAwardInfromer {
    char login[LOGIN_MAX_LEN + 1];
    char command[20];  // Command
    unsigned int vnum; // Item
};

// Packet info for deleting gift notifications
struct TPacketDeleteAwardID {
    uint32_t dwID;
};

struct TChannelStatus {
    short nPort;
    uint8_t bStatus;
};

#pragma pack()
}
