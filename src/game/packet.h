#ifndef __INC_PACKET_H__
#define __INC_PACKET_H__

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
//	HEADER_CG_RUNUP_MATRIX_ANSWER	= 201,

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

	HEADER_GC_MATRIX_CARD			= 116,
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
typedef struct SPacketGGSetup
{
	uint8_t	bHeader;
	uint16_t	wPort;
	uint8_t	bChannel;
} TPacketGGSetup;

typedef struct SPacketGGLogin
{
	uint8_t	bHeader;
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
	uint32_t	dwPID;
	uint8_t	bEmpire;
	int32_t	lMapIndex;
	uint8_t	bChannel;
} TPacketGGLogin;

typedef struct SPacketGGLogout
{
	uint8_t	bHeader;
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
} TPacketGGLogout;

typedef struct SPacketGGRelay
{
	uint8_t	bHeader;
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
	int32_t	lSize;
} TPacketGGRelay;

typedef struct SPacketGGNotice
{
	uint8_t	bHeader;
	int32_t	lSize;
} TPacketGGNotice;

typedef struct SPacketGGMonarchNotice
{
	uint8_t	bHeader;
	uint8_t	bEmpire;
	int32_t	lSize;
} TPacketGGMonarchNotice;

//FORKED_ROAD
//END_FORKED_ROAD
typedef struct SPacketGGShutdown
{
	uint8_t	bHeader;
} TPacketGGShutdown;

typedef struct SPacketGGGuild
{
	uint8_t	bHeader;
	uint8_t	bSubHeader;
	uint32_t	dwGuild;
} TPacketGGGuild;

enum
{
	GUILD_SUBHEADER_GG_CHAT,
	GUILD_SUBHEADER_GG_SET_MEMBER_COUNT_BONUS,
};

typedef struct SPacketGGGuildChat
{
	uint8_t	bHeader;
	uint8_t	bSubHeader;
	uint32_t	dwGuild;
	char	szText[CHAT_MAX_LEN + 1];
} TPacketGGGuildChat;

enum
{
	PARTY_SUBHEADER_GG_CREATE,
	PARTY_SUBHEADER_GG_DESTROY,
};

typedef struct SPacketGGDisconnect
{
	uint8_t	bHeader;
	char	szLogin[LOGIN_MAX_LEN + 1];
} TPacketGGDisconnect;

typedef struct SPacketGGShout
{
	uint8_t	bHeader;
	uint8_t	bEmpire;
	char	szText[CHAT_MAX_LEN + 1];
} TPacketGGShout;

typedef struct SPacketGGXmasWarpSanta
{
	uint8_t	bHeader;
	uint8_t	bChannel;
	int32_t	lMapIndex;
} TPacketGGXmasWarpSanta;

typedef struct SPacketGGXmasWarpSantaReply
{
	uint8_t	bHeader;
	uint8_t	bChannel;
} TPacketGGXmasWarpSantaReply;

typedef struct SPacketGGMessenger
{   
	uint8_t        bHeader;
	char        szAccount[CHARACTER_NAME_MAX_LEN + 1];
	char        szCompanion[CHARACTER_NAME_MAX_LEN + 1];
} TPacketGGMessenger;

typedef struct SPacketGGMessengerMobile
{   
	uint8_t        bHeader;
	char        szName[CHARACTER_NAME_MAX_LEN + 1];
	char        szMobile[MOBILE_MAX_LEN + 1];
} TPacketGGMessengerMobile;

typedef struct SPacketGGFindPosition
{
	uint8_t header;
	uint32_t dwFromPID; // the person trying to warp to that position
	uint32_t dwTargetPID; // the person being searched for
} TPacketGGFindPosition;

typedef struct SPacketGGWarpCharacter
{
	uint8_t header;
	uint32_t pid;
	int32_t x;
	int32_t y;
} TPacketGGWarpCharacter;

//  HEADER_GG_GUILD_WAR_ZONE_MAP_INDEX	    = 15,

typedef struct SPacketGGGuildWarMapIndex
{
	uint8_t bHeader;
	uint32_t dwGuildID1;
	uint32_t dwGuildID2;
	int32_t lMapIndex;
} TPacketGGGuildWarMapIndex;

typedef struct SPacketGGTransfer
{
	uint8_t	bHeader;
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
	int32_t	lX, lY;
} TPacketGGTransfer;

typedef struct SPacketGGBlockChat
{
	uint8_t	bHeader;
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
	int32_t	lBlockDuration;
} TPacketGGBlockChat;

/* packets sent from the client side */

typedef struct command_text
{
	uint8_t	bHeader;
} TPacketCGText;

/* login (1) */
typedef struct command_handshake
{
	uint8_t	bHeader;
	uint32_t	dwHandshake;
	uint32_t	dwTime;
	int32_t	lDelta;
} TPacketCGHandshake;

typedef struct command_login
{
	uint8_t	header;
	char	login[LOGIN_MAX_LEN + 1];
	char	passwd[PASSWD_MAX_LEN + 1];
} TPacketCGLogin;

typedef struct command_login2
{
	uint8_t	header;
	char	login[LOGIN_MAX_LEN + 1];
	uint32_t	dwLoginKey;
	uint32_t	adwClientKey[4];
} TPacketCGLogin2;

typedef struct command_login3
{
	uint8_t	header;
	char	login[LOGIN_MAX_LEN + 1];
	char	passwd[PASSWD_MAX_LEN + 1];
	uint32_t	adwClientKey[4];
} TPacketCGLogin3;

typedef struct command_login5
{
	uint8_t	header;
	char	authKey[OPENID_AUTHKEY_LEN + 1];
	uint32_t	adwClientKey[4];
} TPacketCGLogin5;

typedef struct packet_matrix_card
{
	uint8_t	bHeader;
	uint32_t	dwRows;
	uint32_t	dwCols;
} TPacketGCMatrixCard;

typedef struct command_player_select
{
	uint8_t	header;
	uint8_t	index;
} TPacketCGPlayerSelect;

typedef struct command_player_delete
{
	uint8_t	header;
	uint8_t	index;
	char	private_code[8];
} TPacketCGPlayerDelete;

typedef struct command_player_create
{
	uint8_t        header;
	uint8_t        index;
	char        name[CHARACTER_NAME_MAX_LEN + 1];
	uint16_t        job;
	uint8_t	shape;
	uint8_t	Con;
	uint8_t	Int;
	uint8_t	Str;
	uint8_t	Dex;
} TPacketCGPlayerCreate;

typedef struct command_player_create_success
{
	uint8_t		header;
	uint8_t		bAccountCharacterIndex;
	TSimplePlayer	player;
} TPacketGCPlayerCreateSuccess;

// attack
typedef struct command_attack
{
	uint8_t	bHeader;
	uint8_t	bType;
	uint32_t	dwVID;
	uint8_t	bCRCMagicCubeProcPiece;
	uint8_t	bCRCMagicCubeFilePiece;
} TPacketCGAttack;

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
typedef struct command_move
{
	uint8_t	bHeader;
	uint8_t	bFunc;
	uint8_t	bArg;
	uint8_t	bRot;
	int32_t	lX;
	int32_t	lY;
	uint32_t	dwTime;
} TPacketCGMove;

typedef struct command_sync_position_element
{
	uint32_t	dwVID;
	int32_t	lX;
	int32_t	lY;
} TPacketCGSyncPositionElement;

// position sync
typedef struct command_sync_position	// variable-length packet
{
	uint8_t	bHeader;
	uint16_t	wSize;
} TPacketCGSyncPosition;

/* chat (3) */
typedef struct command_chat	// variable-length packet
{
	uint8_t	header;
	uint16_t	size;
	uint8_t	type;
} TPacketCGChat;

/* whisper */
typedef struct command_whisper
{
	uint8_t	bHeader;
	uint16_t	wSize;
	char 	szNameTo[CHARACTER_NAME_MAX_LEN + 1];
} TPacketCGWhisper;

typedef struct command_entergame
{
	uint8_t	header;
} TPacketCGEnterGame;

typedef struct command_item_use
{
	uint8_t 	header;
	TItemPos 	Cell;
} TPacketCGItemUse;

typedef struct command_item_use_to_item
{
	uint8_t	header;
	TItemPos	Cell;
	TItemPos	TargetCell;
} TPacketCGItemUseToItem;

typedef struct command_item_drop
{
	uint8_t 	header;
	TItemPos 	Cell;
	uint32_t	gold;
} TPacketCGItemDrop;

typedef struct command_item_drop2
{
	uint8_t 	header;
	TItemPos 	Cell;
	uint32_t	gold;
	uint8_t	count;
} TPacketCGItemDrop2;

typedef struct command_item_move
{
	uint8_t 	header;
	TItemPos	Cell;
	TItemPos	CellTo;
	uint8_t	count;
} TPacketCGItemMove;

typedef struct command_item_pickup
{
	uint8_t 	header;
	uint32_t	vid;
} TPacketCGItemPickup;

typedef struct command_quickslot_add
{
	uint8_t	header;
	uint8_t	pos;
	TQuickslot	slot;
} TPacketCGQuickslotAdd;

typedef struct command_quickslot_del
{
	uint8_t	header;
	uint8_t	pos;
} TPacketCGQuickslotDel;

typedef struct command_quickslot_swap
{
	uint8_t	header;
	uint8_t	pos;
	uint8_t	change_pos;
} TPacketCGQuickslotSwap;

enum
{
	SHOP_SUBHEADER_CG_END,
	SHOP_SUBHEADER_CG_BUY,
	SHOP_SUBHEADER_CG_SELL,
	SHOP_SUBHEADER_CG_SELL2
};

typedef struct command_shop
{
	uint8_t	header;
	uint8_t	subheader;
} TPacketCGShop;

typedef struct command_on_click
{
	uint8_t	header;
	uint32_t	vid;
} TPacketCGOnClick;

enum
{
	EXCHANGE_SUBHEADER_CG_START,	/* arg1 == vid of target character */
	EXCHANGE_SUBHEADER_CG_ITEM_ADD,	/* arg1 == position of item */
	EXCHANGE_SUBHEADER_CG_ITEM_DEL,	/* arg1 == position of item */
	EXCHANGE_SUBHEADER_CG_ELK_ADD,	/* arg1 == amount of gold */
	EXCHANGE_SUBHEADER_CG_ACCEPT,	/* arg1 == not used */
	EXCHANGE_SUBHEADER_CG_CANCEL,	/* arg1 == not used */
};

typedef struct command_exchange
{
	uint8_t	header;
	uint8_t	sub_header;
	uint32_t	arg1;
	uint8_t	arg2;
	TItemPos	Pos;
} TPacketCGExchange;

typedef struct command_position
{
	uint8_t	header;
	uint8_t	position;
} TPacketCGPosition;

typedef struct command_script_answer
{
	uint8_t	header;
	uint8_t	answer;
} TPacketCGScriptAnswer;

typedef struct command_script_button
{
	uint8_t        header;
	unsigned int	idx;
} TPacketCGScriptButton;

typedef struct command_quest_input_string
{
	uint8_t header;
	char msg[64+1];
} TPacketCGQuestInputString;

typedef struct command_quest_confirm
{
	uint8_t header;
	uint8_t answer;
	uint32_t requestPID;
} TPacketCGQuestConfirm;

/*
 * packets sent from the server side 
 */
typedef struct packet_quest_confirm
{
	uint8_t header;
	char msg[64+1]; 
	int32_t timeout;
	uint32_t requestPID;
} TPacketGCQuestConfirm;

typedef struct packet_handshake
{
	uint8_t	bHeader;
	uint32_t	dwHandshake;
	uint32_t	dwTime;
	int32_t	lDelta;
} TPacketGCHandshake;

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

typedef struct packet_phase
{
	uint8_t	header;
	uint8_t	phase;
} TPacketGCPhase;

enum
{
};

typedef struct packet_login_success
{
	uint8_t		bHeader;
	TSimplePlayer	players[PLAYER_PER_ACCOUNT];
	uint32_t		guild_id[PLAYER_PER_ACCOUNT];
	char		guild_name[PLAYER_PER_ACCOUNT][GUILD_NAME_MAX_LEN+1];

	uint32_t		handle;
	uint32_t		random_key;
} TPacketGCLoginSuccess;

typedef struct packet_auth_success
{
	uint8_t	bHeader;
	uint32_t	dwLoginKey;
	uint8_t	bResult;
} TPacketGCAuthSuccess;

typedef struct packet_auth_success_openid
{
	uint8_t	bHeader;
	uint32_t	dwLoginKey;
	uint8_t	bResult;
	char	login[LOGIN_MAX_LEN + 1];
} TPacketGCAuthSuccessOpenID;

typedef struct packet_login_failure
{
	uint8_t	header;
	char	szStatus[ACCOUNT_STATUS_MAX_LEN + 1];
} TPacketGCLoginFailure;

typedef struct packet_create_failure
{
	uint8_t	header;
	uint8_t	bType;
} TPacketGCCreateFailure;

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

typedef struct packet_add_char
{
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
} TPacketGCCharacterAdd;

typedef struct packet_char_additional_info
{
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
} TPacketGCCharacterAdditionalInfo;

typedef struct packet_update_char
{
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
} TPacketGCCharacterUpdate;

typedef struct packet_del_char
{
	uint8_t	header;
	uint32_t	id;
} TPacketGCCharacterDelete;

typedef struct packet_chat	// variable-length packet
{
	uint8_t	header;
	uint16_t	size;
	uint8_t	type;
	uint32_t	id;
	uint8_t	bEmpire;
} TPacketGCChat;

typedef struct packet_whisper	// variable-length packet
{
	uint8_t	bHeader;
	uint16_t	wSize;
	uint8_t	bType;
	char	szNameFrom[CHARACTER_NAME_MAX_LEN + 1];
} TPacketGCWhisper;

typedef struct packet_main_character
{
	uint8_t        header;
	uint32_t	dwVID;
	uint16_t	wRaceNum;
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
	int32_t	lx, ly, lz;
	uint8_t	empire;
	uint8_t	skill_group;
} TPacketGCMainCharacter;

// SUPPORT_BGM
typedef struct packet_main_character3_bgm
{
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
} TPacketGCMainCharacter3_BGM;

typedef struct packet_main_character4_bgm_vol
{
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
} TPacketGCMainCharacter4_BGM_VOL;
// END_OF_SUPPORT_BGM

typedef struct packet_points
{
	uint8_t	header;
	int32_t		points[POINT_MAX_NUM];
} TPacketGCPoints;

typedef struct packet_skill_level
{
	uint8_t		bHeader;
	TPlayerSkill	skills[SKILL_MAX_NUM];
} TPacketGCSkillLevel;

typedef struct packet_point_change
{
	int		header;
	uint32_t	dwVID;
	uint8_t	type;
	int32_t	amount;
	int32_t	value;
} TPacketGCPointChange;

typedef struct packet_stun
{
	uint8_t	header;
	uint32_t	vid;
} TPacketGCStun;

typedef struct packet_dead
{
	uint8_t	header;
	uint32_t	vid;
} TPacketGCDead;

struct TPacketGCItemDelDeprecated
{
	uint8_t	header;
	TItemPos Cell;
	uint32_t	vnum;
	uint8_t	count;
	int32_t	alSockets[ITEM_SOCKET_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
};

typedef struct packet_item_set
{
	uint8_t	header;
	TItemPos Cell;
	uint32_t	vnum;
	uint8_t	count;
	uint32_t	flags;
	uint32_t	anti_flags;
	bool	highlight;
	int32_t	alSockets[ITEM_SOCKET_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
} TPacketGCItemSet;

typedef struct packet_item_del
{
	uint8_t	header;
	uint8_t	pos;
} TPacketGCItemDel;

typedef struct packet_item_update
{
	uint8_t	header;
	TItemPos Cell;
	uint8_t	count;
	int32_t	alSockets[ITEM_SOCKET_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
} TPacketGCItemUpdate;

typedef struct packet_item_ground_add
{
	uint8_t	bHeader;
	int32_t 	x, y, z;
	uint32_t	dwVID;
	uint32_t	dwVnum;
} TPacketGCItemGroundAdd;

typedef struct packet_item_ownership
{
	uint8_t	bHeader;
	uint32_t	dwVID;
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
} TPacketGCItemOwnership;

typedef struct packet_item_ground_del
{
	uint8_t	bHeader;
	uint32_t	dwVID;
} TPacketGCItemGroundDel;

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

typedef struct packet_shop_start
{
	uint32_t   owner_vid;
	struct packet_shop_item	items[SHOP_HOST_ITEM_MAX_NUM];
} TPacketGCShopStart;

typedef struct packet_shop_start_ex // followed by TSubPacketShopTab* shop_tabs.
{
	typedef struct sub_packet_shop_tab 
	{
		char name[SHOP_TAB_NAME_MAX];
		uint8_t coin_type;
		packet_shop_item items[SHOP_HOST_ITEM_MAX_NUM];
	} TSubPacketShopTab;
	uint32_t owner_vid;
	uint8_t shop_tab_count;
} TPacketGCShopStartEx;

typedef struct packet_shop_update_item
{
	uint8_t			pos;
	struct packet_shop_item	item;
} TPacketGCShopUpdateItem;

typedef struct packet_shop	// variable-length packet
{
	uint8_t        header;
	uint16_t	size; 
	uint8_t        subheader;
} TPacketGCShop;

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

typedef struct packet_ping
{
	uint8_t	header;
} TPacketGCPing;

struct packet_script
{
	uint8_t	header;
	uint16_t	size;
	uint8_t	skin;
	uint16_t	src_size;
};

typedef struct packet_move
{	
	uint8_t		bHeader;
	uint8_t		bFunc;
	uint8_t		bArg;
	uint8_t		bRot;
	uint32_t		dwVID;
	int32_t		lX;
	int32_t		lY;
	uint32_t		dwTime;
	uint32_t		dwDuration;
} TPacketGCMove;

// ownership
typedef struct packet_ownership
{
	uint8_t		bHeader;
	uint32_t		dwOwnerVID;
	uint32_t		dwVictimVID;
} TPacketGCOwnership;

// unit appended bCount times to the position sync packet

// position sync
typedef struct packet_sync_position	// variable-length packet
{
	uint8_t	bHeader;
	uint16_t	wSize;	// count = (wSize - sizeof(TPacketGCSyncPosition)) / sizeof(TPacketGCSyncPositionElement) 
} TPacketGCSyncPosition;

typedef struct packet_fly
{
	uint8_t	bHeader;
	uint8_t	bType;
	uint32_t	dwStartVID;
	uint32_t	dwEndVID;
} TPacketGCCreateFly;

typedef struct command_fly_targeting
{
	uint8_t		bHeader;
	uint32_t		dwTargetVID;
	int32_t		x, y;
} TPacketCGFlyTargeting;

typedef struct packet_fly_targeting
{
	uint8_t		bHeader;
	uint32_t		dwShooterVID;
	uint32_t		dwTargetVID;
	int32_t		x, y;
} TPacketGCFlyTargeting;

typedef struct packet_shoot
{
	uint8_t		bHeader;
	uint8_t		bType;
} TPacketCGShoot;

typedef struct packet_duel_start
{
	uint8_t	header;
	uint16_t	wSize;	// how many uint32_t? count = (wSize - sizeof(TPacketGCPVPList)) / 4
} TPacketGCDuelStart;

enum EPVPModes
{
	PVP_MODE_NONE,
	PVP_MODE_AGREE,
	PVP_MODE_FIGHT,
	PVP_MODE_REVENGE
};

typedef struct packet_pvp
{
	uint8_t        bHeader;
	uint32_t       dwVIDSrc;
	uint32_t       dwVIDDst;
	uint8_t        bMode;	// 0 = off, 1 = on
} TPacketGCPVP;

typedef struct command_use_skill
{
	uint8_t	bHeader;
	uint32_t	dwVnum;
	uint32_t	dwVID;
} TPacketCGUseSkill;

typedef struct command_target
{
	uint8_t	header;
	uint32_t	dwVID;
} TPacketCGTarget;

typedef struct packet_target
{
	uint8_t	header;
	uint32_t	dwVID;
	uint8_t	bHPPercent;
} TPacketGCTarget;

typedef struct packet_warp
{
	uint8_t	bHeader;
	int32_t	lX;
	int32_t	lY;
	int32_t	lAddr;
	uint16_t	wPort;
} TPacketGCWarp;

typedef struct command_warp
{
	uint8_t	bHeader;
} TPacketCGWarp;

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

typedef struct packet_messenger
{
	uint8_t header;
	uint16_t size;
	uint8_t subheader;
} TPacketGCMessenger;

typedef struct packet_messenger_list_offline
{
	uint8_t connected; // always 0
	uint8_t length;
} TPacketGCMessengerListOffline;

typedef struct packet_messenger_list_online
{
	uint8_t connected; // always 1
	uint8_t length;
} TPacketGCMessengerListOnline;

enum 
{
	MESSENGER_SUBHEADER_CG_ADD_BY_VID,
	MESSENGER_SUBHEADER_CG_ADD_BY_NAME,
	MESSENGER_SUBHEADER_CG_REMOVE,
};

typedef struct command_messenger
{
	uint8_t header;
	uint8_t subheader;
} TPacketCGMessenger;

typedef struct command_messenger_add_by_vid
{
	uint32_t vid;
} TPacketCGMessengerAddByVID;

typedef struct command_safebox_checkout
{
	uint8_t	bHeader;
	uint8_t	bSafePos;
	TItemPos	ItemPos;
} TPacketCGSafeboxCheckout;

typedef struct command_safebox_checkin
{
	uint8_t	bHeader;
	uint8_t	bSafePos;
	TItemPos	ItemPos;
} TPacketCGSafeboxCheckin;

///////////////////////////////////////////////////////////////////////////////////
// Party

typedef struct command_party_parameter
{
	uint8_t	bHeader;
	uint8_t	bDistributeMode;
} TPacketCGPartyParameter;

typedef struct paryt_parameter
{
	uint8_t	bHeader;
	uint8_t	bDistributeMode;
} TPacketGCPartyParameter;

typedef struct packet_party_add
{
	uint8_t	header;
	uint32_t	pid;
	char	name[CHARACTER_NAME_MAX_LEN+1];
} TPacketGCPartyAdd;

typedef struct command_party_invite
{
	uint8_t	header;
	uint32_t	vid;
} TPacketCGPartyInvite;

typedef struct packet_party_invite
{
	uint8_t	header;
	uint32_t	leader_vid;
} TPacketGCPartyInvite;

typedef struct command_party_invite_answer
{
	uint8_t	header;
	uint32_t	leader_vid;
	uint8_t	accept;
} TPacketCGPartyInviteAnswer;

typedef struct packet_party_update
{
	uint8_t	header;
	uint32_t	pid;
	uint8_t	role;
	uint8_t	percent_hp;
	short	affects[7];
} TPacketGCPartyUpdate;

typedef struct packet_party_remove
{
	uint8_t header;
	uint32_t pid;
} TPacketGCPartyRemove;

typedef struct packet_party_link
{
	uint8_t header;
	uint32_t pid;
	uint32_t vid;
} TPacketGCPartyLink;

typedef struct command_party_remove
{
	uint8_t header;
	uint32_t pid;
} TPacketCGPartyRemove;

typedef struct command_party_set_state
{
	uint8_t header;
	uint32_t pid;
	uint8_t byRole;
	uint8_t flag;
} TPacketCGPartySetState;

enum 
{
	PARTY_SKILL_HEAL = 1,
	PARTY_SKILL_WARP = 2
};

typedef struct command_party_use_skill
{
	uint8_t header;
	uint8_t bySkillIndex;
	uint32_t vid;
} TPacketCGPartyUseSkill;

typedef struct packet_safebox_size
{
	uint8_t bHeader;
	uint8_t bSize;
} TPacketCGSafeboxSize;

typedef struct packet_safebox_wrong_password
{
	uint8_t	bHeader;
} TPacketCGSafeboxWrongPassword;

typedef struct command_empire
{
	uint8_t	bHeader;
	uint8_t	bEmpire;
} TPacketCGEmpire;

typedef struct packet_empire
{
	uint8_t	bHeader;
	uint8_t	bEmpire;
} TPacketGCEmpire;

enum
{
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

typedef struct packet_guild
{
	uint8_t header;
	uint16_t size;
	uint8_t subheader;
} TPacketGCGuild;

typedef struct packet_guild_name_t
{
	uint8_t header;
	uint16_t size;
	uint8_t subheader;
	uint32_t	guildID;
	char	guildName[GUILD_NAME_MAX_LEN];
} TPacketGCGuildName;

typedef struct packet_guild_war
{
	uint32_t	dwGuildSelf;
	uint32_t	dwGuildOpp;
	uint8_t	bType;
	uint8_t 	bWarState;
} TPacketGCGuildWar;

typedef struct command_guild
{
	uint8_t header;
	uint8_t subheader;
} TPacketCGGuild;

typedef struct command_guild_answer_make_guild
{
	uint8_t header;
	char guild_name[GUILD_NAME_MAX_LEN+1];
} TPacketCGAnswerMakeGuild;

typedef struct command_guild_use_skill
{
	uint32_t	dwVnum;
	uint32_t	dwPID;
} TPacketCGGuildUseSkill;

// Guild Mark
typedef struct command_mark_login
{
	uint8_t    header;
	uint32_t   handle;
	uint32_t   random_key;
} TPacketCGMarkLogin;

typedef struct command_mark_upload
{
	uint8_t	header;
	uint32_t	gid;
	uint8_t	image[16*12*4];
} TPacketCGMarkUpload;

typedef struct command_mark_idxlist
{
	uint8_t	header;
} TPacketCGMarkIDXList;

typedef struct command_mark_crclist
{
	uint8_t	header;
	uint8_t	imgIdx;
	uint32_t	crclist[80];
} TPacketCGMarkCRCList;

typedef struct packet_mark_idxlist
{
	uint8_t    header;
	uint32_t	bufSize;
	uint16_t	count;
	//followed by size * (uint16_t + uint16_t) bytes of data
} TPacketGCMarkIDXList;

typedef struct packet_mark_block
{
	uint8_t	header;
	uint32_t	bufSize;
	uint8_t	imgIdx;
	uint32_t	count;
	// followed by 64 x 48 x pixel size (4 bytes) = 12288 bytes of data
} TPacketGCMarkBlock;

typedef struct command_symbol_upload
{
	uint8_t	header;
	uint16_t	size;
	uint32_t	guild_id;
} TPacketCGGuildSymbolUpload;

typedef struct command_symbol_crc
{
	uint8_t header;
	uint32_t guild_id;
	uint32_t crc;
	uint32_t size;
} TPacketCGSymbolCRC;

typedef struct packet_symbol_data
{
	uint8_t header;
	uint16_t size;
	uint32_t guild_id;
} TPacketGCGuildSymbolData;

// Fishing

typedef struct command_fishing
{
	uint8_t header;
	uint8_t dir;
} TPacketCGFishing;

typedef struct packet_fishing
{
	uint8_t header;
	uint8_t subheader;
	uint32_t info;
	uint8_t dir;
} TPacketGCFishing;

enum
{
	FISHING_SUBHEADER_GC_START,
	FISHING_SUBHEADER_GC_STOP,
	FISHING_SUBHEADER_GC_REACT,
	FISHING_SUBHEADER_GC_SUCCESS,
	FISHING_SUBHEADER_GC_FAIL,
	FISHING_SUBHEADER_GC_FISH,
};

typedef struct command_give_item
{
	uint8_t byHeader;
	uint32_t dwTargetVID;
	TItemPos ItemPos;
	uint8_t byItemCount;
} TPacketCGGiveItem;

typedef struct SPacketCGHack
{
	uint8_t	bHeader;
	char	szBuf[255 + 1];
} TPacketCGHack;

// SubHeader - Dungeon
enum
{
	DUNGEON_SUBHEADER_GC_DESTINATION_POSITION = 1,
};

typedef struct packet_dungeon
{
	uint8_t bHeader;
	uint16_t size;
	uint8_t subheader;
} TPacketGCDungeon;

typedef struct packet_dungeon_dest_position
{
	int32_t x;
	int32_t y;
} TPacketGCDungeonDestPosition;

typedef struct SPacketGCShopSign
{
	uint8_t	bHeader;
	uint32_t	dwVID;
	char	szSign[SHOP_SIGN_MAX_LEN + 1];
} TPacketGCShopSign;

typedef struct SPacketCGMyShop
{
	uint8_t	bHeader;
	char	szSign[SHOP_SIGN_MAX_LEN + 1];
	uint8_t	bCount;
} TPacketCGMyShop;

typedef struct SPacketGCTime
{
	uint8_t	bHeader;
	int32_t	time;
} TPacketGCTime;

enum
{
	WALKMODE_RUN,
	WALKMODE_WALK,
};

typedef struct SPacketGCWalkMode
{
	uint8_t	header;
	uint32_t	vid;
	uint8_t	mode;
} TPacketGCWalkMode;

typedef struct SPacketGCChangeSkillGroup
{
	uint8_t        header;
	uint8_t        skill_group;
} TPacketGCChangeSkillGroup;

typedef struct SPacketCGRefine
{
	uint8_t	header;
	uint8_t	pos;
	uint8_t	type;
} TPacketCGRefine;

typedef struct SPacketGCRefineInformaion
{
	uint8_t	header;
	uint8_t	type;
	uint8_t	pos;
	uint32_t	src_vnum;
	uint32_t	result_vnum;
	uint8_t	material_count;
	int		cost; // cost
	int		prob; // probability
	TRefineMaterial materials[REFINE_MATERIAL_MAX_NUM];
} TPacketGCRefineInformation;

struct TNPCPosition
{
	uint8_t bType;
	char name[CHARACTER_NAME_MAX_LEN+1];
	int32_t x;
	int32_t y;
};

typedef struct SPacketGCNPCPosition
{
	uint8_t header;
	uint16_t size;
	uint16_t count;

	// array of TNPCPosition
} TPacketGCNPCPosition;

typedef struct SPacketGCSpecialEffect
{
	uint8_t header;
	uint8_t type;
	uint32_t vid;
} TPacketGCSpecialEffect;

typedef struct SPacketCGChangeName
{
	uint8_t header;
	uint8_t index;
	char name[CHARACTER_NAME_MAX_LEN+1];
} TPacketCGChangeName;

typedef struct SPacketGCChangeName
{
	uint8_t header;
	uint32_t pid;
	char name[CHARACTER_NAME_MAX_LEN+1];
} TPacketGCChangeName;

typedef struct command_client_version
{
	uint8_t header;
	char filename[32+1];
	char timestamp[32+1];
} TPacketCGClientVersion;

typedef struct command_client_version2
{
	uint8_t header;
	char filename[32+1];
	char timestamp[32+1];
} TPacketCGClientVersion2;

typedef struct packet_channel
{
	uint8_t header;
	uint8_t channel;
} TPacketGCChannel;

typedef struct pakcet_view_equip
{
	uint8_t  header;
	uint32_t vid;
	struct {
		uint32_t	vnum;
		uint8_t	count;
		int32_t	alSockets[ITEM_SOCKET_MAX_NUM];
		TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
	} equips[WEAR_MAX_NUM];
} TPacketViewEquip;

typedef struct 
{
	uint32_t	dwID;
	int32_t	x, y;
	int32_t	width, height;
	uint32_t	dwGuildID;
} TLandPacketElement;

typedef struct packet_land_list
{
	uint8_t	header;
	uint16_t	size;
} TPacketGCLandList;

typedef struct
{
	uint8_t	bHeader;
	int32_t	lID;
	char	szName[32+1];
	uint32_t	dwVID;
	uint8_t	bType;
} TPacketGCTargetCreate;

typedef struct
{
	uint8_t	bHeader;
	int32_t	lID;
	int32_t	lX, lY;
} TPacketGCTargetUpdate;

typedef struct
{
	uint8_t	bHeader;
	int32_t	lID;
} TPacketGCTargetDelete;

typedef struct
{
	uint8_t		bHeader;
	TPacketAffectElement elem;
} TPacketGCAffectAdd;

typedef struct
{
	uint8_t	bHeader;
	uint32_t	dwType;
	uint8_t	bApplyOn;
} TPacketGCAffectRemove;

typedef struct packet_lover_info
{
	uint8_t header;
	char name[CHARACTER_NAME_MAX_LEN + 1];
	uint8_t love_point;
} TPacketGCLoverInfo;

typedef struct packet_love_point_update
{
	uint8_t header;
	uint8_t love_point;
} TPacketGCLovePointUpdate;

// MINING
typedef struct packet_dig_motion
{
	uint8_t header;
	uint32_t vid;
	uint32_t target_vid;
	uint8_t count;
} TPacketGCDigMotion;
// END_OF_MINING

// SCRIPT_SELECT_ITEM
typedef struct command_script_select_item
{
	uint8_t header;
	uint32_t selection;
} TPacketCGScriptSelectItem;
// END_OF_SCRIPT_SELECT_ITEM

typedef struct packet_damage_info
{
	uint8_t header;
	uint32_t dwVID;
	uint8_t flag;
	int damage;
} TPacketGCDamageInfo;

enum 
{
	RUNUP_MATRIX_ANSWER_MAX_LEN = 4,
	RUNUP_MATRIX_QUIZ_MAX_LEN = 8 ,

};

typedef struct tag_GGSiege
{
	uint8_t	bHeader;
	uint8_t	bEmpire;
	uint8_t	bTowerCount;
} TPacketGGSiege;

typedef struct SPacketGGMonarchTransfer
{
	uint8_t	bHeader;
	uint32_t	dwTargetPID;
	int32_t	x;
	int32_t	y;
} TPacketMonarchGGTransfer;

typedef struct SPacketGGCheckAwakeness
{
	uint8_t bHeader;
} TPacketGGCheckAwakeness;

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
typedef struct SPacketGCSpecificEffect
{
	uint8_t header;
	uint32_t vid;
	char effect_file[MAX_EFFECT_FILE_NAME];
} TPacketGCSpecificEffect;

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
typedef struct SPacketCGDragonSoulRefine
{
	uint8_t header;
	uint8_t bSubType;
	TItemPos ItemGrid[DRAGON_SOUL_REFINE_GRID_SIZE];
} TPacketCGDragonSoulRefine;

typedef struct SPacketGCDragonSoulRefine
{
	SPacketGCDragonSoulRefine() : header(HEADER_GC_DRAGON_SOUL_REFINE)
	{}
	uint8_t header;
	uint8_t bSubType;
	TItemPos Pos;
} TPacketGCDragonSoulRefine;

#pragma pack()
#endif
