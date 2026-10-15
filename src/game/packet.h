#pragma once

#include <common/length.h>
#include <common/tables.h>

#include <cstdint>

enum
{
	HEADER_CG_HANDSHAKE				= 0xff,
	HEADER_CG_PONG				= 0xfe,
	HEADER_CG_TIME_SYNC				= 0xfc,
	HEADER_CG_KEY_AGREEMENT			= 0xfb, // _IMPROVED_PACKET_ENCRYPTION_

	HEADER_CG_LOGIN				= 1,
	HEADER_CG_ATTACK				= 2,
	HEADER_CG_CHAT				= 3,
	HEADER_CG_CHARACTER_CREATE			= 4,
	HEADER_CG_CHARACTER_DELETE			= 5,
	HEADER_CG_CHARACTER_SELECT			= 6,
	HEADER_CG_MOVE				= 7,
	HEADER_CG_SYNC_POSITION			= 8,
	HEADER_CG_ENTERGAME				= 10,

	HEADER_CG_ITEM_USE				= 11,
	HEADER_CG_ITEM_DROP				= 12,
	HEADER_CG_ITEM_MOVE				= 13,
	HEADER_CG_ITEM_PICKUP			= 15,

	HEADER_CG_QUICKSLOT_ADD			= 16,
	HEADER_CG_QUICKSLOT_DEL			= 17,
	HEADER_CG_QUICKSLOT_SWAP			= 18,
	HEADER_CG_WHISPER				= 19,
	HEADER_CG_ITEM_DROP2			= 20,

	HEADER_CG_ON_CLICK				= 26,
	HEADER_CG_EXCHANGE				= 27,
	HEADER_CG_CHARACTER_POSITION		= 28,
	HEADER_CG_SCRIPT_ANSWER			= 29,
	HEADER_CG_QUEST_INPUT_STRING		= 30,
	HEADER_CG_QUEST_CONFIRM			= 31,

	HEADER_CG_SHOP				= 50,
	HEADER_CG_FLY_TARGETING			= 51,
	HEADER_CG_USE_SKILL				= 52,
	HEADER_CG_ADD_FLY_TARGETING			= 53,
	HEADER_CG_SHOOT				= 54,
	HEADER_CG_MYSHOP				= 55,

	HEADER_CG_ITEM_USE_TO_ITEM			= 60,
	HEADER_CG_TARGET			 	= 61,

	HEADER_CG_TEXT				= 64,	// if it starts with @, parse the text.
	HEADER_CG_WARP				= 65,
	HEADER_CG_SCRIPT_BUTTON			= 66,
	HEADER_CG_MESSENGER				= 67,

	HEADER_CG_MALL_CHECKOUT			= 69,
	HEADER_CG_SAFEBOX_CHECKIN			= 70,	// put an item into the safebox.
	HEADER_CG_SAFEBOX_CHECKOUT			= 71,	// take an item out of the safebox.

	HEADER_CG_PARTY_INVITE			= 72,
	HEADER_CG_PARTY_INVITE_ANSWER		= 73,
	HEADER_CG_PARTY_REMOVE			= 74,
	HEADER_CG_PARTY_SET_STATE                   = 75,
	HEADER_CG_PARTY_USE_SKILL			= 76,
	HEADER_CG_SAFEBOX_ITEM_MOVE			= 77,
	HEADER_CG_PARTY_PARAMETER			= 78,

	HEADER_CG_GUILD				= 80,
	HEADER_CG_ANSWER_MAKE_GUILD			= 81,

	HEADER_CG_FISHING				= 82,

	HEADER_CG_ITEM_GIVE				= 83,

	HEADER_CG_EMPIRE				= 90,

	HEADER_CG_REFINE				= 96,

	HEADER_CG_MARK_LOGIN			= 100,
	HEADER_CG_MARK_CRCLIST			= 101,
	HEADER_CG_MARK_UPLOAD			= 102,
	HEADER_CG_MARK_IDXLIST			= 104,

	HEADER_CG_HACK					= 105,
	HEADER_CG_CHANGE_NAME			= 106,
	HEADER_CG_LOGIN2				= 109,
	HEADER_CG_LOGIN3				= 111,

	HEADER_CG_GUILD_SYMBOL_UPLOAD	= 112,
	HEADER_CG_SYMBOL_CRC			= 113,

	// SCRIPT_SELECT_ITEM
	HEADER_CG_SCRIPT_SELECT_ITEM	= 114,
	// END_OF_SCRIPT_SELECT_ITEM

	HEADER_CG_LOGIN5_OPENID			= 116,	//OpenID : receives the OpenID auth key from the client.

//	HEADER_CG_ROULETTE				= 200,

	//

	//NOTE : what the f*** - who designed these packets? how can you code like this and sleep at night.
	//either keep the enums separate, or separate them with a namespace..
	//I don't even expect a packet generator. dammit
	//who's responsible when the numbers collide???

	HEADER_CG_DRAGON_SOUL_REFINE			= 205,
	HEADER_CG_STATE_CHECKER					= 206,

	HEADER_CG_CLIENT_VERSION			= 0xfd,
	HEADER_CG_CLIENT_VERSION2			= 0xf1,

	/********************************************************/
	HEADER_GC_KEY_AGREEMENT_COMPLETED = 0xfa, // _IMPROVED_PACKET_ENCRYPTION_
	HEADER_GC_KEY_AGREEMENT			= 0xfb, // _IMPROVED_PACKET_ENCRYPTION_
	HEADER_GC_TIME_SYNC				= 0xfc,
	HEADER_GC_PHASE					= 0xfd,
	HEADER_GC_HANDSHAKE				= 0xff,

	HEADER_GC_CHARACTER_ADD			= 1,
	HEADER_GC_CHARACTER_DEL			= 2,
	HEADER_GC_MOVE					= 3,
	HEADER_GC_CHAT					= 4,
	HEADER_GC_SYNC_POSITION			= 5,

	HEADER_GC_LOGIN_SUCCESS_NEWSLOT	= 32,
	HEADER_GC_LOGIN_FAILURE			= 7,

	HEADER_GC_CHARACTER_CREATE_SUCCESS		= 8,
	HEADER_GC_CHARACTER_CREATE_FAILURE		= 9,
	HEADER_GC_CHARACTER_DELETE_SUCCESS		= 10,
	HEADER_GC_CHARACTER_DELETE_WRONG_SOCIAL_ID	= 11,

	HEADER_GC_STUN				= 13,
	HEADER_GC_DEAD				= 14,

	HEADER_GC_CHARACTER_POINTS			= 16,
	HEADER_GC_CHARACTER_POINT_CHANGE		= 17,
	HEADER_GC_CHARACTER_UPDATE			= 19,

	HEADER_GC_ITEM_DEL				= 20,
	HEADER_GC_ITEM_SET				= 21,
	HEADER_GC_ITEM_UPDATE			= 25,

	HEADER_GC_ITEM_GROUND_ADD			= 26,
	HEADER_GC_ITEM_GROUND_DEL			= 27,

	HEADER_GC_QUICKSLOT_ADD			= 28,
	HEADER_GC_QUICKSLOT_DEL			= 29,
	HEADER_GC_QUICKSLOT_SWAP			= 30,

	HEADER_GC_ITEM_OWNERSHIP			= 31,

	HEADER_GC_WHISPER				= 34,

	HEADER_GC_SHOP				= 38,
	HEADER_GC_SHOP_SIGN				= 39,

	HEADER_GC_DUEL_START				= 40,
	HEADER_GC_PVP                               = 41,
	HEADER_GC_EXCHANGE				= 42,
	HEADER_GC_CHARACTER_POSITION		= 43,

	HEADER_GC_PING				= 44,
	HEADER_GC_SCRIPT				= 45,
	HEADER_GC_QUEST_CONFIRM			= 46,

	HEADER_GC_OWNERSHIP				= 62,
	HEADER_GC_TARGET			 	= 63,

	HEADER_GC_WARP				= 65,

	HEADER_GC_ADD_FLY_TARGETING			= 69,
	HEADER_GC_CREATE_FLY			= 70,
	HEADER_GC_FLY_TARGETING			= 71,
	HEADER_GC_SKILL_LEVEL			= 76,

	HEADER_GC_MESSENGER				= 74,
	HEADER_GC_GUILD				= 75,

	HEADER_GC_PARTY_INVITE			= 77,
	HEADER_GC_PARTY_ADD				= 78,
	HEADER_GC_PARTY_UPDATE			= 79,
	HEADER_GC_PARTY_REMOVE			= 80,
	HEADER_GC_QUEST_INFO			= 81,
	HEADER_GC_REQUEST_MAKE_GUILD		= 82,
	HEADER_GC_PARTY_PARAMETER			= 83,

	HEADER_GC_SAFEBOX_SET			= 85,
	HEADER_GC_SAFEBOX_DEL			= 86,
	HEADER_GC_SAFEBOX_WRONG_PASSWORD		= 87,
	HEADER_GC_SAFEBOX_SIZE			= 88,

	HEADER_GC_FISHING				= 89,

	HEADER_GC_EMPIRE				= 90,

	HEADER_GC_PARTY_LINK			= 91,
	HEADER_GC_PARTY_UNLINK			= 92,

	HEADER_GC_VIEW_EQUIP			= 99,

	HEADER_GC_MARK_BLOCK			= 100,
	HEADER_GC_MARK_IDXLIST			= 102,

	HEADER_GC_TIME					= 106,
	HEADER_GC_CHANGE_NAME			= 107,

	HEADER_GC_DUNGEON				= 110,

	HEADER_GC_WALK_MODE				= 111,
	HEADER_GC_SKILL_GROUP			= 112,
	HEADER_GC_MAIN_CHARACTER			= 113,

	//	HEADER_GC_USE_POTION			= 114,
	HEADER_GC_SEPCIAL_EFFECT		= 114,

	HEADER_GC_NPC_POSITION			= 115,

	HEADER_GC_REFINE_INFORMATION		= 119,
	HEADER_GC_CHANNEL				= 121,

	// 122 HEADER_GC_MALL_OPEN
	HEADER_GC_TARGET_UPDATE			= 123,
	HEADER_GC_TARGET_DELETE			= 124,
	HEADER_GC_TARGET_CREATE			= 125,

	HEADER_GC_AFFECT_ADD			= 126,
	HEADER_GC_AFFECT_REMOVE			= 127,

	HEADER_GC_MALL_OPEN				= 122,
	HEADER_GC_MALL_SET				= 128,
	HEADER_GC_MALL_DEL				= 129,

	HEADER_GC_LAND_LIST				= 130,
	HEADER_GC_LOVER_INFO			= 131,
	HEADER_GC_LOVE_POINT_UPDATE			= 132,

	HEADER_GC_SYMBOL_DATA			= 133,

	// MINING
	HEADER_GC_DIG_MOTION			= 134,
	// END_OF_MINING

	HEADER_GC_DAMAGE_INFO           = 135,
	HEADER_GC_CHAR_ADDITIONAL_INFO	= 136,

	// SUPPORT_BGM
	HEADER_GC_MAIN_CHARACTER3_BGM		= 137,
	HEADER_GC_MAIN_CHARACTER4_BGM_VOL	= 138,
	// END_OF_SUPPORT_BGM

	HEADER_GC_AUTH_SUCCESS			= 150,

	//HYBRID CRYPT
	//HYBRID CRYPT

	HEADER_GC_AUTH_SUCCESS_OPENID	= 154,

	// ROULETTE
	// END_ROULETTE			
	//

	HEADER_GC_SPECIFIC_EFFECT				= 208,

	HEADER_GC_DRAGON_SOUL_REFINE			= 209,
	HEADER_GC_RESPOND_CHANNELSTATUS			= 210,

	/////////////////////////////////////////////////////////////////////////////

	HEADER_GG_LOGIN				= 1,
	HEADER_GG_LOGOUT				= 2,
	HEADER_GG_RELAY				= 3,
	HEADER_GG_NOTICE				= 4,
	HEADER_GG_SHUTDOWN				= 5,
	HEADER_GG_GUILD				= 6,
	HEADER_GG_DISCONNECT			= 7,	// when forcibly disconnecting someone
	HEADER_GG_SHOUT				= 8,
	HEADER_GG_SETUP				= 9,
	HEADER_GG_MESSENGER_ADD                     = 10,
	HEADER_GG_MESSENGER_REMOVE                  = 11,
	HEADER_GG_FIND_POSITION			= 12,
	HEADER_GG_WARP_CHARACTER			= 13,
	HEADER_GG_MESSENGER_MOBILE			= 14,
	HEADER_GG_GUILD_WAR_ZONE_MAP_INDEX		= 15,
	HEADER_GG_TRANSFER				= 16,
	HEADER_GG_XMAS_WARP_SANTA			= 17,
	HEADER_GG_XMAS_WARP_SANTA_REPLY		= 18,
	HEADER_GG_RELOAD_CRC_LIST			= 19,
	HEADER_GG_CHECK_CLIENT_VERSION		= 21,
	HEADER_GG_BLOCK_CHAT			= 22,

	HEADER_GG_SIEGE					= 25,
	HEADER_GG_MONARCH_NOTICE		= 26,
	HEADER_GG_MONARCH_TRANSFER		= 27,

	HEADER_GG_CHECK_AWAKENESS		= 29,
};

#pragma pack(1)
struct TPacketGGSetup {
	uint8_t	bHeader;
	uint16_t	wPort;
	uint8_t	bChannel;
};

struct TPacketGGLogin {
	uint8_t	bHeader;
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
	uint32_t	dwPID;
	uint8_t	bEmpire;
	int32_t	lMapIndex;
	uint8_t	bChannel;
};

struct TPacketGGLogout {
	uint8_t	bHeader;
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
};

struct TPacketGGRelay {
	uint8_t	bHeader;
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
	int32_t	lSize;
};

struct TPacketGGNotice {
	uint8_t	bHeader;
	int32_t	lSize;
};

struct TPacketGGMonarchNotice {
	uint8_t	bHeader;
	uint8_t	bEmpire;
	int32_t	lSize;
};

//FORKED_ROAD
//END_FORKED_ROAD
struct TPacketGGShutdown {
	uint8_t	bHeader;
};

struct TPacketGGGuild {
	uint8_t	bHeader;
	uint8_t	bSubHeader;
	uint32_t	dwGuild;
};

enum
{
	GUILD_SUBHEADER_GG_CHAT,
	GUILD_SUBHEADER_GG_SET_MEMBER_COUNT_BONUS,
};

struct TPacketGGGuildChat {
	uint8_t	bHeader;
	uint8_t	bSubHeader;
	uint32_t	dwGuild;
	char	szText[CHAT_MAX_LEN + 1];
};

enum
{
	PARTY_SUBHEADER_GG_CREATE,
	PARTY_SUBHEADER_GG_DESTROY,
};

struct TPacketGGDisconnect {
	uint8_t	bHeader;
	char	szLogin[LOGIN_MAX_LEN + 1];
};

struct TPacketGGShout {
	uint8_t	bHeader;
	uint8_t	bEmpire;
	char	szText[CHAT_MAX_LEN + 1];
};

struct TPacketGGXmasWarpSanta {
	uint8_t	bHeader;
	uint8_t	bChannel;
	int32_t	lMapIndex;
};

struct TPacketGGXmasWarpSantaReply {
	uint8_t	bHeader;
	uint8_t	bChannel;
};

struct TPacketGGMessenger {   
	uint8_t        bHeader;
	char        szAccount[CHARACTER_NAME_MAX_LEN + 1];
	char        szCompanion[CHARACTER_NAME_MAX_LEN + 1];
};

struct TPacketGGMessengerMobile {   
	uint8_t        bHeader;
	char        szName[CHARACTER_NAME_MAX_LEN + 1];
	char        szMobile[MOBILE_MAX_LEN + 1];
};

struct TPacketGGFindPosition {
	uint8_t header;
	uint32_t dwFromPID; // the person trying to warp to that position
	uint32_t dwTargetPID; // the person being searched for
};

struct TPacketGGWarpCharacter {
	uint8_t header;
	uint32_t pid;
	int32_t x;
	int32_t y;
};

//  HEADER_GG_GUILD_WAR_ZONE_MAP_INDEX	    = 15,

struct TPacketGGGuildWarMapIndex {
	uint8_t bHeader;
	uint32_t dwGuildID1;
	uint32_t dwGuildID2;
	int32_t lMapIndex;
};

struct TPacketGGTransfer {
	uint8_t	bHeader;
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
	int32_t	lX, lY;
};

struct TPacketGGBlockChat {
	uint8_t	bHeader;
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
	int32_t	lBlockDuration;
};

/* packets sent from the client side */

struct TPacketCGText {
	uint8_t	bHeader;
};

/* login (1) */
struct TPacketCGHandshake {
	uint8_t	bHeader;
	uint32_t	dwHandshake;
	uint32_t	dwTime;
	int32_t	lDelta;
};

struct TPacketCGLogin {
	uint8_t	header;
	char	login[LOGIN_MAX_LEN + 1];
	char	passwd[PASSWD_MAX_LEN + 1];
};

struct TPacketCGLogin2 {
	uint8_t	header;
	char	login[LOGIN_MAX_LEN + 1];
	uint32_t	dwLoginKey;
	uint32_t	adwClientKey[4];
};

struct TPacketCGLogin3 {
	uint8_t	header;
	char	login[LOGIN_MAX_LEN + 1];
	char	passwd[PASSWD_MAX_LEN + 1];
	uint32_t	adwClientKey[4];
};

struct TPacketCGLogin5 {
	uint8_t	header;
	char	authKey[OPENID_AUTHKEY_LEN + 1];
	uint32_t	adwClientKey[4];
};

struct TPacketCGPlayerSelect {
	uint8_t	header;
	uint8_t	index;
};

struct TPacketCGPlayerDelete {
	uint8_t	header;
	uint8_t	index;
	char	private_code[8];
};

struct TPacketCGPlayerCreate {
	uint8_t        header;
	uint8_t        index;
	char        name[CHARACTER_NAME_MAX_LEN + 1];
	uint16_t        job;
	uint8_t	shape;
	uint8_t	Con;
	uint8_t	Int;
	uint8_t	Str;
	uint8_t	Dex;
};

struct TPacketGCPlayerCreateSuccess {
	uint8_t		header;
	uint8_t		bAccountCharacterIndex;
	TSimplePlayer	player;
};

// attack
struct TPacketCGAttack {
	uint8_t	bHeader;
	uint8_t	bType;
	uint32_t	dwVID;
	uint8_t	bCRCMagicCubeProcPiece;
	uint8_t	bCRCMagicCubeFilePiece;
};

enum EMoveFuncType
{
	FUNC_WAIT,
	FUNC_MOVE,
	FUNC_ATTACK,
	FUNC_COMBO,
	FUNC_MOB_SKILL,
	_FUNC_SKILL,
	FUNC_MAX_NUM,
	FUNC_SKILL = 0x80,
};

// move
struct TPacketCGMove {
	uint8_t	bHeader;
	uint8_t	bFunc;
	uint8_t	bArg;
	uint8_t	bRot;
	int32_t	lX;
	int32_t	lY;
	uint32_t	dwTime;
};

struct TPacketCGSyncPositionElement {
	uint32_t	dwVID;
	int32_t	lX;
	int32_t	lY;
};

// position sync
struct TPacketCGSyncPosition { // variable-length packet
	uint8_t	bHeader;
	uint16_t	wSize;
};

/* chat (3) */
struct TPacketCGChat { // variable-length packet
	uint8_t	header;
	uint16_t	size;
	uint8_t	type;
};

/* whisper */
struct TPacketCGWhisper {
	uint8_t	bHeader;
	uint16_t	wSize;
	char 	szNameTo[CHARACTER_NAME_MAX_LEN + 1];
};

struct TPacketCGEnterGame {
	uint8_t	header;
};

struct TPacketCGItemUse {
	uint8_t 	header;
	TItemPos 	Cell;
};

struct TPacketCGItemUseToItem {
	uint8_t	header;
	TItemPos	Cell;
	TItemPos	TargetCell;
};

struct TPacketCGItemDrop {
	uint8_t 	header;
	TItemPos 	Cell;
	uint32_t	gold;
};

struct TPacketCGItemDrop2 {
	uint8_t 	header;
	TItemPos 	Cell;
	uint32_t	gold;
	uint8_t	count;
};

struct TPacketCGItemMove {
	uint8_t 	header;
	TItemPos	Cell;
	TItemPos	CellTo;
	uint8_t	count;
};

struct TPacketCGItemPickup {
	uint8_t 	header;
	uint32_t	vid;
};

struct TPacketCGQuickslotAdd {
	uint8_t	header;
	uint8_t	pos;
	TQuickslot	slot;
};

struct TPacketCGQuickslotDel {
	uint8_t	header;
	uint8_t	pos;
};

struct TPacketCGQuickslotSwap {
	uint8_t	header;
	uint8_t	pos;
	uint8_t	change_pos;
};

enum
{
	SHOP_SUBHEADER_CG_END,
	SHOP_SUBHEADER_CG_BUY,
	SHOP_SUBHEADER_CG_SELL,
	SHOP_SUBHEADER_CG_SELL2
};

struct TPacketCGShop {
	uint8_t	header;
	uint8_t	subheader;
};

struct TPacketCGOnClick {
	uint8_t	header;
	uint32_t	vid;
};

enum
{
	EXCHANGE_SUBHEADER_CG_START,	/* arg1 == vid of target character */
	EXCHANGE_SUBHEADER_CG_ITEM_ADD,	/* arg1 == position of item */
	EXCHANGE_SUBHEADER_CG_ITEM_DEL,	/* arg1 == position of item */
	EXCHANGE_SUBHEADER_CG_ELK_ADD,	/* arg1 == amount of gold */
	EXCHANGE_SUBHEADER_CG_ACCEPT,	/* arg1 == not used */
	EXCHANGE_SUBHEADER_CG_CANCEL,	/* arg1 == not used */
};

struct TPacketCGExchange {
	uint8_t	header;
	uint8_t	sub_header;
	uint32_t	arg1;
	uint8_t	arg2;
	TItemPos	Pos;
};

struct TPacketCGPosition {
	uint8_t	header;
	uint8_t	position;
};

struct TPacketCGScriptAnswer {
	uint8_t	header;
	uint8_t	answer;
};

struct TPacketCGScriptButton {
	uint8_t        header;
	unsigned int	idx;
};

struct TPacketCGQuestInputString {
	uint8_t header;
	char msg[64+1];
};

struct TPacketCGQuestConfirm {
	uint8_t header;
	uint8_t answer;
	uint32_t requestPID;
};

/*
 * packets sent from the server side 
 */
struct TPacketGCQuestConfirm {
	uint8_t header;
	char msg[64+1]; 
	int32_t timeout;
	uint32_t requestPID;
};

struct TPacketGCHandshake {
	uint8_t	bHeader;
	uint32_t	dwHandshake;
	uint32_t	dwTime;
	int32_t	lDelta;
};

enum EPhase
{
	PHASE_CLOSE,
	PHASE_HANDSHAKE,
	PHASE_LOGIN,
	PHASE_SELECT,
	PHASE_LOADING,
	PHASE_GAME,
	PHASE_DEAD,

	PHASE_CLIENT_CONNECTING,
	PHASE_DBCLIENT,
	PHASE_P2P,
	PHASE_AUTH,
};

struct TPacketGCPhase {
	uint8_t	header;
	uint8_t	phase;
};

struct TPacketGCLoginSuccess {
	uint8_t		bHeader;
	TSimplePlayer	players[PLAYER_PER_ACCOUNT];
	uint32_t		guild_id[PLAYER_PER_ACCOUNT];
	char		guild_name[PLAYER_PER_ACCOUNT][GUILD_NAME_MAX_LEN+1];

	uint32_t		handle;
	uint32_t		random_key;
};

struct TPacketGCAuthSuccess {
	uint8_t	bHeader;
	uint32_t	dwLoginKey;
	uint8_t	bResult;
};

struct TPacketGCAuthSuccessOpenID {
	uint8_t	bHeader;
	uint32_t	dwLoginKey;
	uint8_t	bResult;
	char	login[LOGIN_MAX_LEN + 1];
};

struct TPacketGCLoginFailure {
	uint8_t	header;
	char	szStatus[ACCOUNT_STATUS_MAX_LEN + 1];
};

struct TPacketGCCreateFailure {
	uint8_t	header;
	uint8_t	bType;
};

enum
{
	ADD_CHARACTER_STATE_DEAD		= (1 << 0),
	ADD_CHARACTER_STATE_SPAWN		= (1 << 1),
	ADD_CHARACTER_STATE_KILLER		= (1 << 3),
	ADD_CHARACTER_STATE_PARTY		= (1 << 4),
};

enum ECharacterEquipmentPart
{
	CHR_EQUIPPART_ARMOR,
	CHR_EQUIPPART_WEAPON,
	CHR_EQUIPPART_HEAD,
	CHR_EQUIPPART_HAIR,
	CHR_EQUIPPART_NUM,
};

struct TPacketGCCharacterAdd {
	uint8_t	header;
	uint32_t	dwVID;

	float	angle;
	int32_t	x;
	int32_t	y;
	int32_t	z;

	uint8_t	bType;
	uint16_t	wRaceNum;
	uint8_t	bMovingSpeed;
	uint8_t	bAttackSpeed;

	uint8_t	bStateFlag;
	uint32_t	dwAffectFlag[2];	// effect
};

struct TPacketGCCharacterAdditionalInfo {
	uint8_t    header;
	uint32_t   dwVID;
	char    name[CHARACTER_NAME_MAX_LEN + 1];
	uint16_t    awPart[CHR_EQUIPPART_NUM];
	uint8_t	bEmpire;
	uint32_t   dwGuildID;
	uint32_t   dwLevel;
	short	sAlignment;
	uint8_t	bPKMode;
	uint32_t	dwMountVnum;
};

struct TPacketGCCharacterUpdate {
	uint8_t	header;
	uint32_t	dwVID;

	uint16_t        awPart[CHR_EQUIPPART_NUM];
	uint8_t	bMovingSpeed;
	uint8_t	bAttackSpeed;

	uint8_t	bStateFlag;
	uint32_t	dwAffectFlag[2];

	uint32_t	dwGuildID;
	short	sAlignment;
	uint8_t	bPKMode;
	uint32_t	dwMountVnum;
};

struct TPacketGCCharacterDelete {
	uint8_t	header;
	uint32_t	id;
};

struct TPacketGCChat { // variable-length packet
	uint8_t	header;
	uint16_t	size;
	uint8_t	type;
	uint32_t	id;
	uint8_t	bEmpire;
};

struct TPacketGCWhisper { // variable-length packet
	uint8_t	bHeader;
	uint16_t	wSize;
	uint8_t	bType;
	char	szNameFrom[CHARACTER_NAME_MAX_LEN + 1];
};

struct TPacketGCMainCharacter {
	uint8_t        header;
	uint32_t	dwVID;
	uint16_t	wRaceNum;
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
	int32_t	lx, ly, lz;
	uint8_t	empire;
	uint8_t	skill_group;
};

// SUPPORT_BGM
struct TPacketGCMainCharacter3_BGM {
	enum
	{
		MUSIC_NAME_LEN = 24,
	};

	uint8_t    header;
	uint32_t	dwVID;
	uint16_t	wRaceNum;
	char	szChrName[CHARACTER_NAME_MAX_LEN + 1];
	char	szBGMName[MUSIC_NAME_LEN + 1];
	int32_t	lx, ly, lz;
	uint8_t	empire;
	uint8_t	skill_group;
};

struct TPacketGCMainCharacter4_BGM_VOL {
	enum
	{
		MUSIC_NAME_LEN = 24,
	};

	uint8_t    header;
	uint32_t	dwVID;
	uint16_t	wRaceNum;
	char	szChrName[CHARACTER_NAME_MAX_LEN + 1];
	char	szBGMName[MUSIC_NAME_LEN + 1];
	float	fBGMVol;
	int32_t	lx, ly, lz;
	uint8_t	empire;
	uint8_t	skill_group;
};
// END_OF_SUPPORT_BGM

struct TPacketGCPoints {
	uint8_t	header;
	int32_t		points[POINT_MAX_NUM];
};

struct TPacketGCSkillLevel {
	uint8_t		bHeader;
	TPlayerSkill	skills[SKILL_MAX_NUM];
};

struct TPacketGCPointChange {
	int		header;
	uint32_t	dwVID;
	uint8_t	type;
	int32_t	amount;
	int32_t	value;
};

struct TPacketGCStun {
	uint8_t	header;
	uint32_t	vid;
};

struct TPacketGCDead {
	uint8_t	header;
	uint32_t	vid;
};

struct TPacketGCItemDelDeprecated
{
	uint8_t	header;
	TItemPos Cell;
	uint32_t	vnum;
	uint8_t	count;
	int32_t	alSockets[ITEM_SOCKET_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
};

struct TPacketGCItemSet {
	uint8_t	header;
	TItemPos Cell;
	uint32_t	vnum;
	uint8_t	count;
	uint32_t	flags;
	uint32_t	anti_flags;
	bool	highlight;
	int32_t	alSockets[ITEM_SOCKET_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
};

struct TPacketGCItemDel {
	uint8_t	header;
	uint8_t	pos;
};

struct TPacketGCItemUpdate {
	uint8_t	header;
	TItemPos Cell;
	uint8_t	count;
	int32_t	alSockets[ITEM_SOCKET_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
};

struct TPacketGCItemGroundAdd {
	uint8_t	bHeader;
	int32_t 	x, y, z;
	uint32_t	dwVID;
	uint32_t	dwVnum;
};

struct TPacketGCItemOwnership {
	uint8_t	bHeader;
	uint32_t	dwVID;
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
};

struct TPacketGCItemGroundDel {
	uint8_t	bHeader;
	uint32_t	dwVID;
};

struct packet_quickslot_add
{
	uint8_t	header;
	uint8_t	pos;
	TQuickslot	slot;
};

struct packet_quickslot_del
{
	uint8_t	header;
	uint8_t	pos;
};

struct packet_quickslot_swap
{
	uint8_t	header;
	uint8_t	pos;
	uint8_t	pos_to;
};

enum EPacketShopSubHeaders
{
	SHOP_SUBHEADER_GC_START,
	SHOP_SUBHEADER_GC_END,
	SHOP_SUBHEADER_GC_UPDATE_ITEM,
	SHOP_SUBHEADER_GC_UPDATE_PRICE,
	SHOP_SUBHEADER_GC_OK,
	SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY,
	SHOP_SUBHEADER_GC_SOLDOUT,
	SHOP_SUBHEADER_GC_INVENTORY_FULL,
	SHOP_SUBHEADER_GC_INVALID_POS,
	SHOP_SUBHEADER_GC_SOLD_OUT,
	SHOP_SUBHEADER_GC_START_EX,
	SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY_EX,
};

struct packet_shop_item
{   
	uint32_t       vnum;
	uint32_t       price;
	uint8_t        count;
	uint8_t		display_pos;
	int32_t	alSockets[ITEM_SOCKET_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
};

struct TPacketGCShopStart {
	uint32_t   owner_vid;
	struct packet_shop_item	items[SHOP_HOST_ITEM_MAX_NUM];
};

struct TPacketGCShopStartEx { // followed by TSubPacketShopTab* shop_tabs.
	struct TSubPacketShopTab {
		char name[SHOP_TAB_NAME_MAX];
		uint8_t coin_type;
		packet_shop_item items[SHOP_HOST_ITEM_MAX_NUM];
	};
	uint32_t owner_vid;
	uint8_t shop_tab_count;
};

struct TPacketGCShopUpdateItem {
	uint8_t			pos;
	struct packet_shop_item	item;
};

struct TPacketGCShop { // variable-length packet
	uint8_t        header;
	uint16_t	size; 
	uint8_t        subheader;
};

struct packet_exchange
{
	uint8_t	header;
	uint8_t	sub_header;
	uint8_t	is_me;
	uint32_t	arg1;	// vnum
	TItemPos	arg2;	// cell
	uint32_t	arg3;	// count
	int32_t	alSockets[ITEM_SOCKET_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
};

enum EPacketTradeSubHeaders
{
	EXCHANGE_SUBHEADER_GC_START,	/* arg1 == vid */
	EXCHANGE_SUBHEADER_GC_ITEM_ADD,	/* arg1 == vnum  arg2 == pos  arg3 == count */
	EXCHANGE_SUBHEADER_GC_ITEM_DEL,
	EXCHANGE_SUBHEADER_GC_GOLD_ADD,	/* arg1 == gold */
	EXCHANGE_SUBHEADER_GC_ACCEPT,	/* arg1 == accept */
	EXCHANGE_SUBHEADER_GC_END,		/* arg1 == not used */
	EXCHANGE_SUBHEADER_GC_ALREADY,	/* arg1 == not used */
	EXCHANGE_SUBHEADER_GC_LESS_GOLD,	/* arg1 == not used */
};

struct packet_position
{
	uint8_t	header;
	uint32_t	vid;
	uint8_t	position;
};

struct TPacketGCPing {
	uint8_t	header;
};

struct packet_script
{
	uint8_t	header;
	uint16_t	size;
	uint8_t	skin;
	uint16_t	src_size;
};

struct TPacketGCMove {	
	uint8_t		bHeader;
	uint8_t		bFunc;
	uint8_t		bArg;
	uint8_t		bRot;
	uint32_t		dwVID;
	int32_t		lX;
	int32_t		lY;
	uint32_t		dwTime;
	uint32_t		dwDuration;
};

// ownership
struct TPacketGCOwnership {
	uint8_t		bHeader;
	uint32_t		dwOwnerVID;
	uint32_t		dwVictimVID;
};

// unit appended bCount times to the position sync packet

// position sync
struct TPacketGCSyncPosition { // variable-length packet
	uint8_t	bHeader;
	uint16_t	wSize;	// count = (wSize - sizeof(TPacketGCSyncPosition)) / sizeof(TPacketGCSyncPositionElement) 
};

struct TPacketGCCreateFly {
	uint8_t	bHeader;
	uint8_t	bType;
	uint32_t	dwStartVID;
	uint32_t	dwEndVID;
};

struct TPacketCGFlyTargeting {
	uint8_t		bHeader;
	uint32_t		dwTargetVID;
	int32_t		x, y;
};

struct TPacketGCFlyTargeting {
	uint8_t		bHeader;
	uint32_t		dwShooterVID;
	uint32_t		dwTargetVID;
	int32_t		x, y;
};

struct TPacketCGShoot {
	uint8_t		bHeader;
	uint8_t		bType;
};

struct TPacketGCDuelStart {
	uint8_t	header;
	uint16_t	wSize;	// how many uint32_t? count = (wSize - sizeof(TPacketGCPVPList)) / 4
};

enum EPVPModes
{
	PVP_MODE_NONE,
	PVP_MODE_AGREE,
	PVP_MODE_FIGHT,
	PVP_MODE_REVENGE
};

struct TPacketGCPVP {
	uint8_t        bHeader;
	uint32_t       dwVIDSrc;
	uint32_t       dwVIDDst;
	uint8_t        bMode;	// 0 = off, 1 = on
};

struct TPacketCGUseSkill {
	uint8_t	bHeader;
	uint32_t	dwVnum;
	uint32_t	dwVID;
};

struct TPacketCGTarget {
	uint8_t	header;
	uint32_t	dwVID;
};

struct TPacketGCTarget {
	uint8_t	header;
	uint32_t	dwVID;
	uint8_t	bHPPercent;
};

struct TPacketGCWarp {
	uint8_t	bHeader;
	int32_t	lX;
	int32_t	lY;
	int32_t	lAddr;
	uint16_t	wPort;
};

struct TPacketCGWarp {
	uint8_t	bHeader;
};

struct packet_quest_info
{
	uint8_t header;
	uint16_t size;
	uint16_t index;
	uint8_t flag;
};

enum 
{
	MESSENGER_SUBHEADER_GC_LIST,
	MESSENGER_SUBHEADER_GC_LOGIN,
	MESSENGER_SUBHEADER_GC_LOGOUT,
};

struct TPacketGCMessenger {
	uint8_t header;
	uint16_t size;
	uint8_t subheader;
};

struct TPacketGCMessengerListOffline {
	uint8_t connected; // always 0
	uint8_t length;
};

struct TPacketGCMessengerListOnline {
	uint8_t connected; // always 1
	uint8_t length;
};

enum 
{
	MESSENGER_SUBHEADER_CG_ADD_BY_VID,
	MESSENGER_SUBHEADER_CG_ADD_BY_NAME,
	MESSENGER_SUBHEADER_CG_REMOVE,
};

struct TPacketCGMessenger {
	uint8_t header;
	uint8_t subheader;
};

struct TPacketCGMessengerAddByVID {
	uint32_t vid;
};

struct TPacketCGSafeboxCheckout {
	uint8_t	bHeader;
	uint8_t	bSafePos;
	TItemPos	ItemPos;
};

struct TPacketCGSafeboxCheckin {
	uint8_t	bHeader;
	uint8_t	bSafePos;
	TItemPos	ItemPos;
};

///////////////////////////////////////////////////////////////////////////////////
// Party

struct TPacketCGPartyParameter {
	uint8_t	bHeader;
	uint8_t	bDistributeMode;
};

struct TPacketGCPartyParameter {
	uint8_t	bHeader;
	uint8_t	bDistributeMode;
};

struct TPacketGCPartyAdd {
	uint8_t	header;
	uint32_t	pid;
	char	name[CHARACTER_NAME_MAX_LEN+1];
};

struct TPacketCGPartyInvite {
	uint8_t	header;
	uint32_t	vid;
};

struct TPacketGCPartyInvite {
	uint8_t	header;
	uint32_t	leader_vid;
};

struct TPacketCGPartyInviteAnswer {
	uint8_t	header;
	uint32_t	leader_vid;
	uint8_t	accept;
};

struct TPacketGCPartyUpdate {
	uint8_t	header;
	uint32_t	pid;
	uint8_t	role;
	uint8_t	percent_hp;
	short	affects[7];
};

struct TPacketGCPartyRemove {
	uint8_t header;
	uint32_t pid;
};

struct TPacketGCPartyLink {
	uint8_t header;
	uint32_t pid;
	uint32_t vid;
};

struct TPacketCGPartyRemove {
	uint8_t header;
	uint32_t pid;
};

struct TPacketCGPartySetState {
	uint8_t header;
	uint32_t pid;
	uint8_t byRole;
	uint8_t flag;
};

enum 
{
	PARTY_SKILL_HEAL = 1,
	PARTY_SKILL_WARP = 2
};

struct TPacketCGPartyUseSkill {
	uint8_t header;
	uint8_t bySkillIndex;
	uint32_t vid;
};

struct TPacketCGSafeboxSize {
	uint8_t bHeader;
	uint8_t bSize;
};

struct TPacketCGSafeboxWrongPassword {
	uint8_t	bHeader;
};

struct TPacketCGEmpire {
	uint8_t	bHeader;
	uint8_t	bEmpire;
};

struct TPacketGCEmpire {
	uint8_t	bHeader;
	uint8_t	bEmpire;
};

// Guild

enum
{
	GUILD_SUBHEADER_GC_LOGIN,
	GUILD_SUBHEADER_GC_LOGOUT,
	GUILD_SUBHEADER_GC_LIST,
	GUILD_SUBHEADER_GC_GRADE,
	GUILD_SUBHEADER_GC_ADD,
	GUILD_SUBHEADER_GC_REMOVE,
	GUILD_SUBHEADER_GC_GRADE_NAME,
	GUILD_SUBHEADER_GC_GRADE_AUTH,
	GUILD_SUBHEADER_GC_INFO,
	GUILD_SUBHEADER_GC_COMMENTS,
	GUILD_SUBHEADER_GC_CHANGE_EXP,
	GUILD_SUBHEADER_GC_CHANGE_MEMBER_GRADE,
	GUILD_SUBHEADER_GC_SKILL_INFO,
	GUILD_SUBHEADER_GC_CHANGE_MEMBER_GENERAL,
	GUILD_SUBHEADER_GC_GUILD_INVITE,
	GUILD_SUBHEADER_GC_WAR,
	GUILD_SUBHEADER_GC_GUILD_NAME,
	GUILD_SUBHEADER_GC_GUILD_WAR_LIST,
	GUILD_SUBHEADER_GC_GUILD_WAR_END_LIST,
	GUILD_SUBHEADER_GC_WAR_SCORE,
	GUILD_SUBHEADER_GC_MONEY_CHANGE,
};

enum GUILD_SUBHEADER_CG
{
	GUILD_SUBHEADER_CG_ADD_MEMBER,
	GUILD_SUBHEADER_CG_REMOVE_MEMBER,
	GUILD_SUBHEADER_CG_CHANGE_GRADE_NAME,
	GUILD_SUBHEADER_CG_CHANGE_GRADE_AUTHORITY,
	GUILD_SUBHEADER_CG_OFFER,
	GUILD_SUBHEADER_CG_POST_COMMENT,
	GUILD_SUBHEADER_CG_DELETE_COMMENT,    
	GUILD_SUBHEADER_CG_REFRESH_COMMENT,
	GUILD_SUBHEADER_CG_CHANGE_MEMBER_GRADE,
	GUILD_SUBHEADER_CG_USE_SKILL,
	GUILD_SUBHEADER_CG_CHANGE_MEMBER_GENERAL,
	GUILD_SUBHEADER_CG_GUILD_INVITE_ANSWER,
	GUILD_SUBHEADER_CG_CHARGE_GSP,
	GUILD_SUBHEADER_CG_DEPOSIT_MONEY,
	GUILD_SUBHEADER_CG_WITHDRAW_MONEY,
};

struct TPacketGCGuild {
	uint8_t header;
	uint16_t size;
	uint8_t subheader;
};

struct TPacketGCGuildName {
	uint8_t header;
	uint16_t size;
	uint8_t subheader;
	uint32_t	guildID;
	char	guildName[GUILD_NAME_MAX_LEN];
};

struct TPacketGCGuildWar {
	uint32_t	dwGuildSelf;
	uint32_t	dwGuildOpp;
	uint8_t	bType;
	uint8_t 	bWarState;
};

struct TPacketCGGuild {
	uint8_t header;
	uint8_t subheader;
};

struct TPacketCGAnswerMakeGuild {
	uint8_t header;
	char guild_name[GUILD_NAME_MAX_LEN+1];
	uint32_t key;	// from TPacketGCRequestMakeGuild
};

// Asks the client for a guild name; only an answer carrying the key creates a guild.
struct TPacketGCRequestMakeGuild {
	uint8_t header;
	uint32_t key;
};

struct TPacketCGGuildUseSkill {
	uint32_t	dwVnum;
	uint32_t	dwPID;
};

// Guild Mark
struct TPacketCGMarkLogin {
	uint8_t    header;
	uint32_t   handle;
	uint32_t   random_key;
};

struct TPacketCGMarkUpload {
	uint8_t	header;
	uint32_t	gid;
	uint8_t	image[16*12*4];
};

struct TPacketCGMarkIDXList {
	uint8_t	header;
};

struct TPacketCGMarkCRCList {
	uint8_t	header;
	uint8_t	imgIdx;
	uint32_t	crclist[80];
};

struct TPacketGCMarkIDXList {
	uint8_t    header;
	uint32_t	bufSize;
	uint16_t	count;
	//followed by size * (uint16_t + uint16_t) bytes of data
};

struct TPacketGCMarkBlock {
	uint8_t	header;
	uint32_t	bufSize;
	uint8_t	imgIdx;
	uint32_t	count;
	// followed by 64 x 48 x pixel size (4 bytes) = 12288 bytes of data
};

struct TPacketCGGuildSymbolUpload {
	uint8_t	header;
	uint16_t	size;
	uint32_t	guild_id;
};

struct TPacketCGSymbolCRC {
	uint8_t header;
	uint32_t guild_id;
	uint32_t crc;
	uint32_t size;
};

struct TPacketGCGuildSymbolData {
	uint8_t header;
	uint16_t size;
	uint32_t guild_id;
};

// Fishing

struct TPacketCGFishing {
	uint8_t header;
	uint8_t dir;
};

struct TPacketGCFishing {
	uint8_t header;
	uint8_t subheader;
	uint32_t info;
	uint8_t dir;
};

enum
{
	FISHING_SUBHEADER_GC_START,
	FISHING_SUBHEADER_GC_STOP,
	FISHING_SUBHEADER_GC_REACT,
	FISHING_SUBHEADER_GC_SUCCESS,
	FISHING_SUBHEADER_GC_FAIL,
	FISHING_SUBHEADER_GC_FISH,
};

struct TPacketCGGiveItem {
	uint8_t byHeader;
	uint32_t dwTargetVID;
	TItemPos ItemPos;
	uint8_t byItemCount;
};

struct TPacketCGHack {
	uint8_t	bHeader;
	char	szBuf[255 + 1];
};

// SubHeader - Dungeon
enum
{
	DUNGEON_SUBHEADER_GC_DESTINATION_POSITION = 1,
};

struct TPacketGCDungeon {
	uint8_t bHeader;
	uint16_t size;
	uint8_t subheader;
};

struct TPacketGCDungeonDestPosition {
	int32_t x;
	int32_t y;
};

struct TPacketGCShopSign {
	uint8_t	bHeader;
	uint32_t	dwVID;
	char	szSign[SHOP_SIGN_MAX_LEN + 1];
};

struct TPacketCGMyShop {
	uint8_t	bHeader;
	char	szSign[SHOP_SIGN_MAX_LEN + 1];
	uint8_t	bCount;
};

struct TPacketGCTime {
	uint8_t	bHeader;
	int32_t	time;
};

enum
{
	WALKMODE_RUN,
	WALKMODE_WALK,
};

struct TPacketGCWalkMode {
	uint8_t	header;
	uint32_t	vid;
	uint8_t	mode;
};

struct TPacketGCChangeSkillGroup {
	uint8_t        header;
	uint8_t        skill_group;
};

struct TPacketCGRefine {
	uint8_t	header;
	uint8_t	pos;
	uint8_t	type;
};

struct TPacketGCRefineInformation {
	uint8_t	header;
	uint8_t	type;
	uint8_t	pos;
	uint32_t	src_vnum;
	uint32_t	result_vnum;
	uint8_t	material_count;
	int		cost; // cost
	int		prob; // probability
	TRefineMaterial materials[REFINE_MATERIAL_MAX_NUM];
};

struct TNPCPosition
{
	uint8_t bType;
	char name[CHARACTER_NAME_MAX_LEN+1];
	int32_t x;
	int32_t y;
};

struct TPacketGCNPCPosition {
	uint8_t header;
	uint16_t size;
	uint16_t count;

	// array of TNPCPosition
};

struct TPacketGCSpecialEffect {
	uint8_t header;
	uint8_t type;
	uint32_t vid;
};

struct TPacketCGChangeName {
	uint8_t header;
	uint8_t index;
	char name[CHARACTER_NAME_MAX_LEN+1];
};

struct TPacketGCChangeName {
	uint8_t header;
	uint32_t pid;
	char name[CHARACTER_NAME_MAX_LEN+1];
};

struct TPacketCGClientVersion {
	uint8_t header;
	char filename[32+1];
	char timestamp[32+1];
};

struct TPacketCGClientVersion2 {
	uint8_t header;
	char filename[32+1];
	char timestamp[32+1];
};

struct TPacketGCChannel {
	uint8_t header;
	uint8_t channel;
};

struct TPacketViewEquip {
	uint8_t  header;
	uint32_t vid;
	struct {
		uint32_t	vnum;
		uint8_t	count;
		int32_t	alSockets[ITEM_SOCKET_MAX_NUM];
		TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
	} equips[WEAR_MAX_NUM];
};

struct TLandPacketElement {
	uint32_t	dwID;
	int32_t	x, y;
	int32_t	width, height;
	uint32_t	dwGuildID;
};

struct TPacketGCLandList {
	uint8_t	header;
	uint16_t	size;
};

struct TPacketGCTargetCreate {
	uint8_t	bHeader;
	int32_t	lID;
	char	szName[32+1];
	uint32_t	dwVID;
	uint8_t	bType;
};

struct TPacketGCTargetUpdate {
	uint8_t	bHeader;
	int32_t	lID;
	int32_t	lX, lY;
};

struct TPacketGCTargetDelete {
	uint8_t	bHeader;
	int32_t	lID;
};

struct TPacketGCAffectAdd {
	uint8_t		bHeader;
	TPacketAffectElement elem;
};

struct TPacketGCAffectRemove {
	uint8_t	bHeader;
	uint32_t	dwType;
	uint8_t	bApplyOn;
};

struct TPacketGCLoverInfo {
	uint8_t header;
	char name[CHARACTER_NAME_MAX_LEN + 1];
	uint8_t love_point;
};

struct TPacketGCLovePointUpdate {
	uint8_t header;
	uint8_t love_point;
};

// MINING
struct TPacketGCDigMotion {
	uint8_t header;
	uint32_t vid;
	uint32_t target_vid;
	uint8_t count;
};
// END_OF_MINING

// SCRIPT_SELECT_ITEM
struct TPacketCGScriptSelectItem {
	uint8_t header;
	uint32_t selection;
};
// END_OF_SCRIPT_SELECT_ITEM

struct TPacketGCDamageInfo {
	uint8_t header;
	uint32_t dwVID;
	uint8_t flag;
	int damage;
};

struct TPacketGGSiege {
	uint8_t	bHeader;
	uint8_t	bEmpire;
	uint8_t	bTowerCount;
};

struct TPacketMonarchGGTransfer {
	uint8_t	bHeader;
	uint32_t	dwTargetPID;
	int32_t	x;
	int32_t	y;
};

struct TPacketGGCheckAwakeness {
	uint8_t bHeader;
};

struct TPacketKeyAgreement
{
	static const int MAX_DATA_LEN = 256;
	uint8_t bHeader;
	uint16_t wAgreedLength;
	uint16_t wDataLength;
	uint8_t data[MAX_DATA_LEN];
};

struct TPacketKeyAgreementCompleted
{
	uint8_t bHeader;
	uint8_t data[3]; // dummy (not used)
};

inline constexpr int MAX_EFFECT_FILE_NAME = 128;
struct TPacketGCSpecificEffect {
	uint8_t header;
	uint32_t vid;
	char effect_file[MAX_EFFECT_FILE_NAME];
};

// dragon soul stone
enum EDragonSoulRefineWindowRefineType
{
	DragonSoulRefineWindow_UPGRADE,
};

enum EPacketCGDragonSoulSubHeaderType
{
	DS_SUB_HEADER_OPEN,
	DS_SUB_HEADER_CLOSE,
	DS_SUB_HEADER_DO_REFINE_GRADE,
	DS_SUB_HEADER_DO_REFINE_STEP,
	DS_SUB_HEADER_DO_REFINE_STRENGTH,
	DS_SUB_HEADER_REFINE_FAIL,
	DS_SUB_HEADER_REFINE_FAIL_MAX_REFINE,
	DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL,
	DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MONEY,
	DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL,
	DS_SUB_HEADER_REFINE_FAIL_TOO_MUCH_MATERIAL,
	DS_SUB_HEADER_REFINE_SUCCEED,
};
struct TPacketCGDragonSoulRefine {
	uint8_t header;
	uint8_t bSubType;
	TItemPos ItemGrid[DRAGON_SOUL_REFINE_GRID_SIZE];
};

struct TPacketGCDragonSoulRefine {
	TPacketGCDragonSoulRefine() : header(HEADER_GC_DRAGON_SOUL_REFINE)
	{}
	uint8_t header;
	uint8_t bSubType;
	TItemPos Pos;
};

#pragma pack()
