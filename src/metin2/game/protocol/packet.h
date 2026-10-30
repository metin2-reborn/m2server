#pragma once

#include <metin2/proto/character_types.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/guild_types.h>
#include <metin2/proto/inventory.h>
#include <metin2/proto/item_types.h>
#include <metin2/proto/limits.h>
#include <metin2/proto/shop_types.h>
#include <metin2/proto/tables.h>

#include <cstdint>

namespace metin2::game::inline protocol {

    enum {
        HEADER_CG_HANDSHAKE = 0xff,
        HEADER_CG_PONG = 0xfe,
        HEADER_CG_TIME_SYNC = 0xfc,
        HEADER_CG_KEY_AGREEMENT = 0xfb,

        HEADER_CG_LOGIN = 1,
        HEADER_CG_ATTACK = 2,
        HEADER_CG_CHAT = 3,
        HEADER_CG_CHARACTER_CREATE = 4,
        HEADER_CG_CHARACTER_DELETE = 5,
        HEADER_CG_CHARACTER_SELECT = 6,
        HEADER_CG_MOVE = 7,
        HEADER_CG_SYNC_POSITION = 8,
        HEADER_CG_ENTERGAME = 10,

        HEADER_CG_ITEM_USE = 11,
        HEADER_CG_ITEM_DROP = 12,
        HEADER_CG_ITEM_MOVE = 13,
        HEADER_CG_ITEM_PICKUP = 15,

        HEADER_CG_QUICKSLOT_ADD = 16,
        HEADER_CG_QUICKSLOT_DEL = 17,
        HEADER_CG_QUICKSLOT_SWAP = 18,
        HEADER_CG_WHISPER = 19,
        HEADER_CG_ITEM_DROP2 = 20,

        HEADER_CG_ON_CLICK = 26,
        HEADER_CG_EXCHANGE = 27,
        HEADER_CG_CHARACTER_POSITION = 28,
        HEADER_CG_SCRIPT_ANSWER = 29,
        HEADER_CG_QUEST_INPUT_STRING = 30,
        HEADER_CG_QUEST_CONFIRM = 31,

        HEADER_CG_SHOP = 50,
        HEADER_CG_FLY_TARGETING = 51,
        HEADER_CG_USE_SKILL = 52,
        HEADER_CG_ADD_FLY_TARGETING = 53,
        HEADER_CG_SHOOT = 54,
        HEADER_CG_MYSHOP = 55,

        HEADER_CG_ITEM_USE_TO_ITEM = 60,
        HEADER_CG_TARGET = 61,

        HEADER_CG_TEXT = 64,
        HEADER_CG_WARP = 65,
        HEADER_CG_SCRIPT_BUTTON = 66,
        HEADER_CG_MESSENGER = 67,

        HEADER_CG_MALL_CHECKOUT = 69,
        HEADER_CG_SAFEBOX_CHECKIN = 70,
        HEADER_CG_SAFEBOX_CHECKOUT = 71,

        HEADER_CG_PARTY_INVITE = 72,
        HEADER_CG_PARTY_INVITE_ANSWER = 73,
        HEADER_CG_PARTY_REMOVE = 74,
        HEADER_CG_PARTY_SET_STATE = 75,
        HEADER_CG_PARTY_USE_SKILL = 76,
        HEADER_CG_SAFEBOX_ITEM_MOVE = 77,
        HEADER_CG_PARTY_PARAMETER = 78,

        HEADER_CG_GUILD = 80,
        HEADER_CG_ANSWER_MAKE_GUILD = 81,

        HEADER_CG_FISHING = 82,

        HEADER_CG_ITEM_GIVE = 83,

        HEADER_CG_EMPIRE = 90,

        HEADER_CG_REFINE = 96,

        HEADER_CG_MARK_LOGIN = 100,
        HEADER_CG_MARK_CRCLIST = 101,
        HEADER_CG_MARK_UPLOAD = 102,
        HEADER_CG_MARK_IDXLIST = 104,

        HEADER_CG_HACK = 105,
        HEADER_CG_CHANGE_NAME = 106,
        HEADER_CG_LOGIN2 = 109,
        HEADER_CG_LOGIN3 = 111,

        HEADER_CG_GUILD_SYMBOL_UPLOAD = 112,
        HEADER_CG_SYMBOL_CRC = 113,

        HEADER_CG_SCRIPT_SELECT_ITEM = 114,

        HEADER_CG_DRAGON_SOUL_REFINE = 205,
        HEADER_CG_STATE_CHECKER = 206,

        HEADER_CG_CLIENT_VERSION = 0xfd,
        HEADER_CG_CLIENT_VERSION2 = 0xf1,

        HEADER_GC_KEY_AGREEMENT_COMPLETED = 0xfa,
        HEADER_GC_KEY_AGREEMENT = 0xfb,
        HEADER_GC_TIME_SYNC = 0xfc,
        HEADER_GC_PHASE = 0xfd,
        HEADER_GC_HANDSHAKE = 0xff,

        HEADER_GC_CHARACTER_ADD = 1,
        HEADER_GC_CHARACTER_DEL = 2,
        HEADER_GC_MOVE = 3,
        HEADER_GC_CHAT = 4,
        HEADER_GC_SYNC_POSITION = 5,

        HEADER_GC_LOGIN_SUCCESS_NEWSLOT = 32,
        HEADER_GC_LOGIN_FAILURE = 7,

        HEADER_GC_CHARACTER_CREATE_SUCCESS = 8,
        HEADER_GC_CHARACTER_CREATE_FAILURE = 9,
        HEADER_GC_CHARACTER_DELETE_SUCCESS = 10,
        HEADER_GC_CHARACTER_DELETE_WRONG_SOCIAL_ID = 11,

        HEADER_GC_STUN = 13,
        HEADER_GC_DEAD = 14,

        HEADER_GC_CHARACTER_POINTS = 16,
        HEADER_GC_CHARACTER_POINT_CHANGE = 17,
        HEADER_GC_CHARACTER_UPDATE = 19,

        HEADER_GC_ITEM_DEL = 20,
        HEADER_GC_ITEM_SET = 21,
        HEADER_GC_ITEM_UPDATE = 25,

        HEADER_GC_ITEM_GROUND_ADD = 26,
        HEADER_GC_ITEM_GROUND_DEL = 27,

        HEADER_GC_QUICKSLOT_ADD = 28,
        HEADER_GC_QUICKSLOT_DEL = 29,
        HEADER_GC_QUICKSLOT_SWAP = 30,

        HEADER_GC_ITEM_OWNERSHIP = 31,

        HEADER_GC_WHISPER = 34,

        HEADER_GC_SHOP = 38,
        HEADER_GC_SHOP_SIGN = 39,

        HEADER_GC_DUEL_START = 40,
        HEADER_GC_PVP = 41,
        HEADER_GC_EXCHANGE = 42,
        HEADER_GC_CHARACTER_POSITION = 43,

        HEADER_GC_PING = 44,
        HEADER_GC_SCRIPT = 45,
        HEADER_GC_QUEST_CONFIRM = 46,

        HEADER_GC_OWNERSHIP = 62,
        HEADER_GC_TARGET = 63,

        HEADER_GC_WARP = 65,

        HEADER_GC_ADD_FLY_TARGETING = 69,
        HEADER_GC_CREATE_FLY = 70,
        HEADER_GC_FLY_TARGETING = 71,
        HEADER_GC_SKILL_LEVEL = 76,

        HEADER_GC_MESSENGER = 74,
        HEADER_GC_GUILD = 75,

        HEADER_GC_PARTY_INVITE = 77,
        HEADER_GC_PARTY_ADD = 78,
        HEADER_GC_PARTY_UPDATE = 79,
        HEADER_GC_PARTY_REMOVE = 80,
        HEADER_GC_QUEST_INFO = 81,
        HEADER_GC_REQUEST_MAKE_GUILD = 82,
        HEADER_GC_PARTY_PARAMETER = 83,

        HEADER_GC_SAFEBOX_SET = 85,
        HEADER_GC_SAFEBOX_DEL = 86,
        HEADER_GC_SAFEBOX_WRONG_PASSWORD = 87,
        HEADER_GC_SAFEBOX_SIZE = 88,

        HEADER_GC_FISHING = 89,

        HEADER_GC_EMPIRE = 90,

        HEADER_GC_PARTY_LINK = 91,
        HEADER_GC_PARTY_UNLINK = 92,

        HEADER_GC_VIEW_EQUIP = 99,

        HEADER_GC_MARK_BLOCK = 100,
        HEADER_GC_MARK_IDXLIST = 102,

        HEADER_GC_TIME = 106,
        HEADER_GC_CHANGE_NAME = 107,

        HEADER_GC_DUNGEON = 110,

        HEADER_GC_WALK_MODE = 111,
        HEADER_GC_SKILL_GROUP = 112,
        HEADER_GC_MAIN_CHARACTER = 113,

        HEADER_GC_SEPCIAL_EFFECT = 114,

        HEADER_GC_NPC_POSITION = 115,

        HEADER_GC_REFINE_INFORMATION = 119,
        HEADER_GC_CHANNEL = 121,

        HEADER_GC_TARGET_UPDATE = 123,
        HEADER_GC_TARGET_DELETE = 124,
        HEADER_GC_TARGET_CREATE = 125,

        HEADER_GC_AFFECT_ADD = 126,
        HEADER_GC_AFFECT_REMOVE = 127,

        HEADER_GC_MALL_OPEN = 122,
        HEADER_GC_MALL_SET = 128,
        HEADER_GC_MALL_DEL = 129,

        HEADER_GC_LAND_LIST = 130,
        HEADER_GC_LOVER_INFO = 131,
        HEADER_GC_LOVE_POINT_UPDATE = 132,

        HEADER_GC_SYMBOL_DATA = 133,

        HEADER_GC_DIG_MOTION = 134,

        HEADER_GC_DAMAGE_INFO = 135,
        HEADER_GC_CHAR_ADDITIONAL_INFO = 136,

        HEADER_GC_MAIN_CHARACTER3_BGM = 137,
        HEADER_GC_MAIN_CHARACTER4_BGM_VOL = 138,

        HEADER_GC_AUTH_SUCCESS = 150,

        HEADER_GC_SPECIFIC_EFFECT = 208,

        HEADER_GC_DRAGON_SOUL_REFINE = 209,
        HEADER_GC_RESPOND_CHANNELSTATUS = 210,

        HEADER_GG_LOGIN = 1,
        HEADER_GG_LOGOUT = 2,
        HEADER_GG_RELAY = 3,
        HEADER_GG_NOTICE = 4,
        HEADER_GG_SHUTDOWN = 5,
        HEADER_GG_GUILD = 6,
        HEADER_GG_DISCONNECT = 7,
        HEADER_GG_SHOUT = 8,
        HEADER_GG_SETUP = 9,
        HEADER_GG_MESSENGER_ADD = 10,
        HEADER_GG_MESSENGER_REMOVE = 11,
        HEADER_GG_FIND_POSITION = 12,
        HEADER_GG_WARP_CHARACTER = 13,
        HEADER_GG_MESSENGER_MOBILE = 14,
        HEADER_GG_GUILD_WAR_ZONE_MAP_INDEX = 15,
        HEADER_GG_TRANSFER = 16,
        HEADER_GG_XMAS_WARP_SANTA = 17,
        HEADER_GG_XMAS_WARP_SANTA_REPLY = 18,
        HEADER_GG_CHECK_CLIENT_VERSION = 21,
        HEADER_GG_BLOCK_CHAT = 22,

        HEADER_GG_SIEGE = 25,
        HEADER_GG_MONARCH_NOTICE = 26,
        HEADER_GG_MONARCH_TRANSFER = 27,

        HEADER_GG_CHECK_AWAKENESS = 29,
    };

#pragma pack(1)
    struct packet_gg_setup {
        uint8_t header;
        uint16_t port;
        uint8_t channel;
    };

    struct packet_gg_login {
        uint8_t header;
        char name[CHARACTER_NAME_MAX_LEN + 1];
        uint32_t pid;
        uint8_t empire;
        int32_t map_index;
        uint8_t channel;
    };

    struct packet_gg_logout {
        uint8_t header;
        char name[CHARACTER_NAME_MAX_LEN + 1];
    };

    struct packet_gg_relay {
        uint8_t header;
        char name[CHARACTER_NAME_MAX_LEN + 1];
        int32_t size;
    };

    struct packet_gg_notice {
        uint8_t header;
        int32_t size;
    };

    struct packet_gg_monarch_notice {
        uint8_t header;
        uint8_t empire;
        int32_t size;
    };

    struct packet_gg_shutdown {
        uint8_t header;
    };

    struct packet_gg_guild {
        uint8_t header;
        uint8_t sub_header;
        uint32_t guild;
    };

    enum {
        GUILD_SUBHEADER_GG_CHAT,
        GUILD_SUBHEADER_GG_SET_MEMBER_COUNT_BONUS,
    };

    struct packet_gg_guild_chat {
        uint8_t header;
        uint8_t sub_header;
        uint32_t guild;
        char text[CHAT_MAX_LEN + 1];
    };

    enum {
        PARTY_SUBHEADER_GG_CREATE,
        PARTY_SUBHEADER_GG_DESTROY,
    };

    struct packet_gg_disconnect {
        uint8_t header;
        char login[LOGIN_MAX_LEN + 1];
    };

    struct packet_gg_shout {
        uint8_t header;
        uint8_t empire;
        char text[CHAT_MAX_LEN + 1];
    };

    struct packet_gg_xmas_warp_santa {
        uint8_t header;
        uint8_t channel;
        int32_t map_index;
    };

    struct packet_gg_xmas_warp_santa_reply {
        uint8_t header;
        uint8_t channel;
    };

    struct packet_gg_messenger {
        uint8_t header;
        char account[CHARACTER_NAME_MAX_LEN + 1];
        char companion[CHARACTER_NAME_MAX_LEN + 1];
    };

    struct packet_gg_messenger_mobile {
        uint8_t header;
        char name[CHARACTER_NAME_MAX_LEN + 1];
        char mobile[MOBILE_MAX_LEN + 1];
    };

    struct packet_gg_find_position {
        uint8_t header;
        uint32_t from_pid;
        uint32_t target_pid;
    };

    struct packet_gg_warp_character {
        uint8_t header;
        uint32_t pid;
        int32_t x;
        int32_t y;
    };

    struct packet_gg_guild_war_map_index {
        uint8_t header;
        uint32_t guild_id1;
        uint32_t guild_id2;
        int32_t map_index;
    };

    struct packet_gg_transfer {
        uint8_t header;
        char name[CHARACTER_NAME_MAX_LEN + 1];
        int32_t x, y;
    };

    struct packet_gg_block_chat {
        uint8_t header;
        char name[CHARACTER_NAME_MAX_LEN + 1];
        int32_t block_duration;
    };

    struct packet_cg_text {
        uint8_t header;
    };

    struct packet_cg_handshake {
        uint8_t header;
        uint32_t handshake;
        uint32_t time;
        int32_t delta;
    };

    struct packet_cg_login {
        uint8_t header;
        char login[LOGIN_MAX_LEN + 1];
        char passwd[PASSWD_MAX_LEN + 1];
    };

    struct packet_cg_login2 {
        uint8_t header;
        char login[LOGIN_MAX_LEN + 1];
        uint32_t login_key;
        uint32_t client_key[4];
    };

    struct packet_cg_login3 {
        uint8_t header;
        char login[LOGIN_MAX_LEN + 1];
        char passwd[PASSWD_MAX_LEN + 1];
        uint32_t client_key[4];
    };

    struct packet_cg_player_select {
        uint8_t header;
        uint8_t index;
    };

    struct packet_cg_player_delete {
        uint8_t header;
        uint8_t index;
        char private_code[8];
    };

    struct packet_cg_player_create {
        uint8_t header;
        uint8_t index;
        char name[CHARACTER_NAME_MAX_LEN + 1];
        uint16_t job;
        uint8_t shape;
        uint8_t con;
        uint8_t iq;
        uint8_t str;
        uint8_t dex;
    };

    struct packet_gc_player_create_success {
        uint8_t header;
        uint8_t account_character_index;
        simple_player player;
    };

    struct packet_cg_attack {
        uint8_t header;
        uint8_t type;
        uint32_t vid;
        uint8_t crc_magic_cube_proc_piece;
        uint8_t crc_magic_cube_file_piece;
    };

    enum move_func_type {
        FUNC_WAIT,
        FUNC_MOVE,
        FUNC_ATTACK,
        FUNC_COMBO,
        FUNC_MOB_SKILL,
        _FUNC_SKILL,
        FUNC_MAX_NUM,
        FUNC_SKILL = 0x80,
    };

    struct packet_cg_move {
        uint8_t header;
        uint8_t func;
        uint8_t arg;
        uint8_t rot;
        int32_t x;
        int32_t y;
        uint32_t time;
    };

    struct packet_cg_sync_position_element {
        uint32_t vid;
        int32_t x;
        int32_t y;
    };

    struct packet_cg_sync_position {
        uint8_t header;
        uint16_t size;
    };

    struct packet_cg_chat {
        uint8_t header;
        uint16_t size;
        uint8_t type;
    };

    struct packet_cg_whisper {
        uint8_t header;
        uint16_t size;
        char name_to[CHARACTER_NAME_MAX_LEN + 1];
    };

    struct packet_cg_enter_game {
        uint8_t header;
    };

    struct packet_cg_item_use {
        uint8_t header;
        item_pos cell;
    };

    struct packet_cg_item_use_to_item {
        uint8_t header;
        item_pos cell;
        item_pos target_cell;
    };

    struct packet_cg_item_drop {
        uint8_t header;
        item_pos cell;
        uint32_t gold;
    };

    struct packet_cg_item_drop2 {
        uint8_t header;
        item_pos cell;
        uint32_t gold;
        uint8_t count;
    };

    struct packet_cg_item_move {
        uint8_t header;
        item_pos cell;
        item_pos cell_to;
        uint8_t count;
    };

    struct packet_cg_item_pickup {
        uint8_t header;
        uint32_t vid;
    };

    struct packet_cg_quickslot_add {
        uint8_t header;
        uint8_t pos;
        quickslot slot;
    };

    struct packet_cg_quickslot_del {
        uint8_t header;
        uint8_t pos;
    };

    struct packet_cg_quickslot_swap {
        uint8_t header;
        uint8_t pos;
        uint8_t change_pos;
    };

    enum {
        SHOP_SUBHEADER_CG_END,
        SHOP_SUBHEADER_CG_BUY,
        SHOP_SUBHEADER_CG_SELL,
        SHOP_SUBHEADER_CG_SELL2
    };

    struct packet_cg_shop {
        uint8_t header;
        uint8_t subheader;
    };

    struct packet_cg_on_click {
        uint8_t header;
        uint32_t vid;
    };

    enum {
        EXCHANGE_SUBHEADER_CG_START,
        EXCHANGE_SUBHEADER_CG_ITEM_ADD,
        EXCHANGE_SUBHEADER_CG_ITEM_DEL,
        EXCHANGE_SUBHEADER_CG_ELK_ADD,
        EXCHANGE_SUBHEADER_CG_ACCEPT,
        EXCHANGE_SUBHEADER_CG_CANCEL,
    };

    struct packet_cg_exchange {
        uint8_t header;
        uint8_t sub_header;
        uint32_t arg1;
        uint8_t arg2;
        item_pos pos;
    };

    struct packet_cg_position {
        uint8_t header;
        uint8_t position;
    };

    struct packet_cg_script_answer {
        uint8_t header;
        uint8_t answer;
    };

    struct packet_cg_script_button {
        uint8_t header;
        unsigned int idx;
    };

    struct packet_cg_quest_input_string {
        uint8_t header;
        char msg[64 + 1];
    };

    struct packet_cg_quest_confirm {
        uint8_t header;
        uint8_t answer;
        uint32_t request_pid;
    };

    struct packet_gc_quest_confirm {
        uint8_t header;
        char msg[64 + 1];
        int32_t timeout;
        uint32_t request_pid;
    };

    struct packet_gc_handshake {
        uint8_t header;
        uint32_t handshake;
        uint32_t time;
        int32_t delta;
    };

    enum phase {
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

    struct packet_gc_phase {
        uint8_t header;
        uint8_t phase;
    };

    struct packet_gc_login_success {
        uint8_t header;
        simple_player players[PLAYER_PER_ACCOUNT];
        uint32_t guild_id[PLAYER_PER_ACCOUNT];
        char guild_name[PLAYER_PER_ACCOUNT][GUILD_NAME_MAX_LEN + 1];

        uint32_t handle;
        uint32_t random_key;
    };

    struct packet_gc_auth_success {
        uint8_t header;
        uint32_t login_key;
        uint8_t result;
    };

    struct packet_gc_login_failure {
        uint8_t header;
        char status[ACCOUNT_STATUS_MAX_LEN + 1];
    };

    struct packet_gc_create_failure {
        uint8_t header;
        uint8_t type;
    };

    enum {
        ADD_CHARACTER_STATE_DEAD = (1 << 0),
        ADD_CHARACTER_STATE_SPAWN = (1 << 1),
        ADD_CHARACTER_STATE_KILLER = (1 << 3),
        ADD_CHARACTER_STATE_PARTY = (1 << 4),
    };

    enum character_equipment_part {
        CHR_EQUIPPART_ARMOR,
        CHR_EQUIPPART_WEAPON,
        CHR_EQUIPPART_HEAD,
        CHR_EQUIPPART_HAIR,
        CHR_EQUIPPART_NUM,
    };

    struct packet_gc_character_add {
        uint8_t header;
        uint32_t vid;

        float angle;
        int32_t x;
        int32_t y;
        int32_t z;

        uint8_t type;
        uint16_t race_num;
        uint16_t moving_speed;
        uint8_t attack_speed;

        uint8_t state_flag;
        uint32_t affect_flag[2];
    };

    struct packet_gc_character_additional_info {
        uint8_t header;
        uint32_t vid;
        char name[CHARACTER_NAME_MAX_LEN + 1];
        uint16_t part[CHR_EQUIPPART_NUM];
        uint8_t empire;
        uint32_t guild_id;
        uint32_t level;
        short s_alignment;
        uint8_t pk_mode;
        uint32_t mount_vnum;
    };

    struct packet_gc_character_update {
        uint8_t header;
        uint32_t vid;

        uint16_t part[CHR_EQUIPPART_NUM];
        uint16_t moving_speed;
        uint8_t attack_speed;

        uint8_t state_flag;
        uint32_t affect_flag[2];

        uint32_t guild_id;
        short s_alignment;
        uint8_t pk_mode;
        uint32_t mount_vnum;
    };

    struct packet_gc_character_delete {
        uint8_t header;
        uint32_t id;
    };

    struct packet_gc_chat {
        uint8_t header;
        uint16_t size;
        uint8_t type;
        uint32_t id;
        uint8_t empire;
    };

    struct packet_gc_whisper {
        uint8_t header;
        uint16_t size;
        uint8_t type;
        char name_from[CHARACTER_NAME_MAX_LEN + 1];
    };

    struct packet_gc_main_character {
        uint8_t header;
        uint32_t vid;
        uint16_t race_num;
        char name[CHARACTER_NAME_MAX_LEN + 1];
        int32_t lx, ly, lz;
        uint8_t empire;
        uint8_t skill_group;
    };

    struct packet_gc_main_character3_bgm {
        enum {
            MUSIC_NAME_LEN = 24,
        };

        uint8_t header;
        uint32_t vid;
        uint16_t race_num;
        char chr_name[CHARACTER_NAME_MAX_LEN + 1];
        char bgm_name[MUSIC_NAME_LEN + 1];
        int32_t lx, ly, lz;
        uint8_t empire;
        uint8_t skill_group;
    };

    struct packet_gc_main_character4_bgm_vol {
        enum {
            MUSIC_NAME_LEN = 24,
        };

        uint8_t header;
        uint32_t vid;
        uint16_t race_num;
        char chr_name[CHARACTER_NAME_MAX_LEN + 1];
        char bgm_name[MUSIC_NAME_LEN + 1];
        float bgm_vol;
        int32_t lx, ly, lz;
        uint8_t empire;
        uint8_t skill_group;
    };

    struct packet_gc_points {
        uint8_t header;
        int32_t points[POINT_MAX_NUM];
    };

    struct packet_gc_skill_level {
        uint8_t header;
        player_skill skills[SKILL_MAX_NUM];
    };

    struct packet_gc_point_change {
        int header;
        uint32_t vid;
        uint8_t type;
        int32_t amount;
        int32_t value;
    };

    struct packet_gc_stun {
        uint8_t header;
        uint32_t vid;
    };

    struct packet_gc_dead {
        uint8_t header;
        uint32_t vid;
    };

    struct packet_gc_item_del_deprecated {
        uint8_t header;
        item_pos cell;
        uint32_t vnum;
        uint8_t count;
        int32_t sockets[ITEM_SOCKET_MAX_NUM];
        player_item_attribute attr[ITEM_ATTRIBUTE_MAX_NUM];
    };

    struct packet_gc_item_set {
        uint8_t header;
        item_pos cell;
        uint32_t vnum;
        uint8_t count;
        uint32_t flags;
        uint32_t anti_flags;
        bool highlight;
        int32_t sockets[ITEM_SOCKET_MAX_NUM];
        player_item_attribute attr[ITEM_ATTRIBUTE_MAX_NUM];
    };

    struct packet_gc_item_del {
        uint8_t header;
        uint8_t pos;
    };

    struct packet_gc_item_update {
        uint8_t header;
        item_pos cell;
        uint8_t count;
        int32_t sockets[ITEM_SOCKET_MAX_NUM];
        player_item_attribute attr[ITEM_ATTRIBUTE_MAX_NUM];
    };

    struct packet_gc_item_ground_add {
        uint8_t header;
        int32_t x, y, z;
        uint32_t vid;
        uint32_t vnum;
        uint32_t count; // the amount for yang
    };

    struct packet_gc_item_ownership {
        uint8_t header;
        uint32_t vid;
        char name[CHARACTER_NAME_MAX_LEN + 1];
    };

    struct packet_gc_item_ground_del {
        uint8_t header;
        uint32_t vid;
    };

    struct packet_quickslot_add {
        uint8_t header;
        uint8_t pos;
        quickslot slot;
    };

    struct packet_quickslot_del {
        uint8_t header;
        uint8_t pos;
    };

    struct packet_quickslot_swap {
        uint8_t header;
        uint8_t pos;
        uint8_t pos_to;
    };

    enum packet_shop_sub_headers {
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

    struct packet_shop_item {
        uint32_t vnum;
        uint32_t price;
        uint8_t count;
        uint8_t display_pos;
        int32_t sockets[ITEM_SOCKET_MAX_NUM];
        player_item_attribute attr[ITEM_ATTRIBUTE_MAX_NUM];
    };

    struct packet_gc_shop_start {
        uint32_t owner_vid;
        struct packet_shop_item items[SHOP_HOST_ITEM_MAX_NUM];
    };

    struct packet_gc_shop_start_ex {
        struct sub_packet_shop_tab {
            char name[SHOP_TAB_NAME_MAX];
            uint8_t coin_type;
            packet_shop_item items[SHOP_HOST_ITEM_MAX_NUM];
        };
        uint32_t owner_vid;
        uint8_t shop_tab_count;
    };

    struct packet_gc_shop_update_item {
        uint8_t pos;
        struct packet_shop_item item;
    };

    struct packet_gc_shop {
        uint8_t header;
        uint16_t size;
        uint8_t subheader;
    };

    struct packet_exchange {
        uint8_t header;
        uint8_t sub_header;
        uint8_t is_me;
        uint32_t arg1;
        item_pos arg2;
        uint32_t arg3;
        int32_t sockets[ITEM_SOCKET_MAX_NUM];
        player_item_attribute attr[ITEM_ATTRIBUTE_MAX_NUM];
    };

    enum packet_trade_sub_headers {
        EXCHANGE_SUBHEADER_GC_START,
        EXCHANGE_SUBHEADER_GC_ITEM_ADD,
        EXCHANGE_SUBHEADER_GC_ITEM_DEL,
        EXCHANGE_SUBHEADER_GC_GOLD_ADD,
        EXCHANGE_SUBHEADER_GC_ACCEPT,
        EXCHANGE_SUBHEADER_GC_END,
        EXCHANGE_SUBHEADER_GC_ALREADY,
        EXCHANGE_SUBHEADER_GC_LESS_GOLD,
    };

    struct packet_position {
        uint8_t header;
        uint32_t vid;
        uint8_t position;
    };

    struct packet_gc_ping {
        uint8_t header;
    };

    struct packet_script {
        uint8_t header;
        uint16_t size;
        uint8_t skin;
        uint16_t src_size;
    };

    struct packet_gc_move {
        uint8_t header;
        uint8_t func;
        uint8_t arg;
        uint8_t rot;
        uint32_t vid;
        int32_t x;
        int32_t y;
        uint32_t time;
        uint32_t duration;
    };

    struct packet_gc_ownership {
        uint8_t header;
        uint32_t owner_vid;
        uint32_t victim_vid;
    };

    struct packet_gc_sync_position {
        uint8_t header;
        uint16_t size;
    };

    struct packet_gc_create_fly {
        uint8_t header;
        uint8_t type;
        uint32_t start_vid;
        uint32_t end_vid;
    };

    struct packet_cg_fly_targeting {
        uint8_t header;
        uint32_t target_vid;
        int32_t x, y;
    };

    struct packet_gc_fly_targeting {
        uint8_t header;
        uint32_t shooter_vid;
        uint32_t target_vid;
        int32_t x, y;
    };

    struct packet_cg_shoot {
        uint8_t header;
        uint8_t type;
    };

    struct packet_gc_duel_start {
        uint8_t header;
        uint16_t size;
    };

    enum pvp_modes {
        PVP_MODE_NONE,
        PVP_MODE_AGREE,
        PVP_MODE_FIGHT,
        PVP_MODE_REVENGE
    };

    struct packet_gcpvp {
        uint8_t header;
        uint32_t vid_src;
        uint32_t vid_dst;
        uint8_t mode;
    };

    struct packet_cg_use_skill {
        uint8_t header;
        uint32_t vnum;
        uint32_t vid;
    };

    struct packet_cg_target {
        uint8_t header;
        uint32_t vid;
    };

    struct packet_gc_target {
        uint8_t header;
        uint32_t vid;
        uint8_t hp_percent;
    };

    struct packet_gc_warp {
        uint8_t header;
        int32_t x;
        int32_t y;
        int32_t addr;
        uint16_t port;
    };

    struct packet_cg_warp {
        uint8_t header;
    };

    struct packet_quest_info {
        uint8_t header;
        uint16_t size;
        uint16_t index;
        uint16_t flag;
    };

    enum {
        MESSENGER_SUBHEADER_GC_LIST,
        MESSENGER_SUBHEADER_GC_LOGIN,
        MESSENGER_SUBHEADER_GC_LOGOUT,
    };

    struct packet_gc_messenger {
        uint8_t header;
        uint16_t size;
        uint8_t subheader;
    };

    struct packet_gc_messenger_list_offline {
        uint8_t connected; // always 0
        uint8_t length;
    };

    struct packet_gc_messenger_list_online {
        uint8_t connected; // always 1
        uint8_t length;
    };

    enum {
        MESSENGER_SUBHEADER_CG_ADD_BY_VID,
        MESSENGER_SUBHEADER_CG_ADD_BY_NAME,
        MESSENGER_SUBHEADER_CG_REMOVE,
    };

    struct packet_cg_messenger {
        uint8_t header;
        uint8_t subheader;
    };

    struct packet_cg_messenger_add_by_vid {
        uint32_t vid;
    };

    struct packet_cg_safebox_checkout {
        uint8_t header;
        uint8_t safe_pos;
        proto::item_pos item_pos;
    };

    struct packet_cg_safebox_checkin {
        uint8_t header;
        uint8_t safe_pos;
        proto::item_pos item_pos;
    };

    struct packet_cg_party_parameter {
        uint8_t header;
        uint8_t distribute_mode;
    };

    struct packet_gc_party_parameter {
        uint8_t header;
        uint8_t distribute_mode;
    };

    struct packet_gc_party_add {
        uint8_t header;
        uint32_t pid;
        char name[CHARACTER_NAME_MAX_LEN + 1];
    };

    struct packet_cg_party_invite {
        uint8_t header;
        uint32_t vid;
    };

    struct packet_gc_party_invite {
        uint8_t header;
        uint32_t leader_vid;
    };

    struct packet_cg_party_invite_answer {
        uint8_t header;
        uint32_t leader_vid;
        uint8_t accept;
    };

    struct packet_gc_party_update {
        uint8_t header;
        uint32_t pid;
        uint8_t role;
        uint8_t percent_hp;
        short affects[7];
    };

    struct packet_gc_party_remove {
        uint8_t header;
        uint32_t pid;
    };

    struct packet_gc_party_link {
        uint8_t header;
        uint32_t pid;
        uint32_t vid;
    };

    struct packet_cg_party_remove {
        uint8_t header;
        uint32_t pid;
    };

    struct packet_cg_party_set_state {
        uint8_t header;
        uint32_t pid;
        uint8_t role;
        uint8_t flag;
    };

    enum {
        PARTY_SKILL_HEAL = 1,
        PARTY_SKILL_WARP = 2
    };

    struct packet_cg_party_use_skill {
        uint8_t header;
        uint8_t skill_index;
        uint32_t vid;
    };

    struct packet_cg_safebox_size {
        uint8_t header;
        uint8_t size;
    };

    struct packet_cg_safebox_wrong_password {
        uint8_t header;
    };

    struct packet_cg_empire {
        uint8_t header;
        uint8_t empire;
    };

    struct packet_gc_empire {
        uint8_t header;
        uint8_t empire;
    };

    enum {
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

    enum guild_subheader_cg {
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

    struct packet_gc_guild {
        uint8_t header;
        uint16_t size;
        uint8_t subheader;
    };

    struct packet_gc_guild_name {
        uint8_t header;
        uint16_t size;
        uint8_t subheader;
        uint32_t guild_id;
        char guild_name[GUILD_NAME_MAX_LEN];
    };

    struct packet_gc_guild_war {
        uint32_t guild_self;
        uint32_t guild_opp;
        uint8_t type;
        uint8_t war_state;
    };

    struct packet_cg_guild {
        uint8_t header;
        uint8_t subheader;
    };

    struct packet_cg_answer_make_guild {
        uint8_t header;
        char guild_name[GUILD_NAME_MAX_LEN + 1];
        uint32_t key;
    };

    struct packet_gc_request_make_guild {
        uint8_t header;
        uint32_t key;
    };

    struct packet_cg_guild_use_skill {
        uint32_t vnum;
        uint32_t pid;
    };

    struct packet_cg_mark_login {
        uint8_t header;
        uint32_t handle;
        uint32_t random_key;
    };

    struct packet_cg_mark_upload {
        uint8_t header;
        uint32_t gid;
        uint8_t image[16 * 12 * 4];
    };

    struct packet_cg_mark_idx_list {
        uint8_t header;
    };

    struct packet_cg_mark_crc_list {
        uint8_t header;
        uint8_t img_idx;
        uint32_t crclist[80];
    };

    struct packet_gc_mark_idx_list {
        uint8_t header;
        uint32_t buf_size;
        uint16_t count;
    };

    struct packet_gc_mark_block {
        uint8_t header;
        uint32_t buf_size;
        uint8_t img_idx;
        uint32_t count;
    };

    struct packet_cg_guild_symbol_upload {
        uint8_t header;
        uint16_t size;
        uint32_t guild_id;
    };

    struct packet_cg_symbol_crc {
        uint8_t header;
        uint32_t guild_id;
        uint32_t crc;
        uint32_t size;
    };

    struct packet_gc_guild_symbol_data {
        uint8_t header;
        uint16_t size;
        uint32_t guild_id;
    };

    struct packet_cg_fishing {
        uint8_t header;
        uint8_t dir;
    };

    struct packet_gc_fishing {
        uint8_t header;
        uint8_t subheader;
        uint32_t info;
        uint8_t dir;
    };

    enum {
        FISHING_SUBHEADER_GC_START,
        FISHING_SUBHEADER_GC_STOP,
        FISHING_SUBHEADER_GC_REACT,
        FISHING_SUBHEADER_GC_SUCCESS,
        FISHING_SUBHEADER_GC_FAIL,
        FISHING_SUBHEADER_GC_FISH,
    };

    struct packet_cg_give_item {
        uint8_t header;
        uint32_t target_vid;
        proto::item_pos item_pos;
        uint8_t item_count;
    };

    struct packet_cg_hack {
        uint8_t header;
        char buf[255 + 1];
    };

    enum {
        DUNGEON_SUBHEADER_GC_DESTINATION_POSITION = 1,
    };

    struct packet_gc_dungeon {
        uint8_t header;
        uint16_t size;
        uint8_t subheader;
    };

    struct packet_gc_dungeon_dest_position {
        int32_t x;
        int32_t y;
    };

    struct packet_gc_shop_sign {
        uint8_t header;
        uint32_t vid;
        char sign[SHOP_SIGN_MAX_LEN + 1];
    };

    struct packet_cg_my_shop {
        uint8_t header;
        char sign[SHOP_SIGN_MAX_LEN + 1];
        uint8_t count;
    };

    struct packet_gc_time {
        uint8_t header;
        int32_t time;
    };

    enum {
        WALKMODE_RUN,
        WALKMODE_WALK,
    };

    struct packet_gc_walk_mode {
        uint8_t header;
        uint32_t vid;
        uint8_t mode;
    };

    struct packet_gc_change_skill_group {
        uint8_t header;
        uint8_t skill_group;
    };

    struct packet_cg_refine {
        uint8_t header;
        uint8_t pos;
        uint8_t type;
    };

    struct packet_gc_refine_information {
        uint8_t header;
        uint8_t type;
        uint8_t pos;
        uint32_t src_vnum;
        uint32_t result_vnum;
        uint8_t material_count;
        int cost;
        int prob;
        refine_material materials[REFINE_MATERIAL_MAX_NUM];
    };

    struct npc_position {
        uint8_t type;
        char name[CHARACTER_NAME_MAX_LEN + 1];
        int32_t x;
        int32_t y;
    };

    struct packet_gcnpc_position {
        uint8_t header;
        uint16_t size;
        uint16_t count;
    };

    struct packet_gc_special_effect {
        uint8_t header;
        uint8_t type;
        uint32_t vid;
    };

    struct packet_cg_change_name {
        uint8_t header;
        uint8_t index;
        char name[CHARACTER_NAME_MAX_LEN + 1];
    };

    struct packet_gc_change_name {
        uint8_t header;
        uint32_t pid;
        char name[CHARACTER_NAME_MAX_LEN + 1];
    };

    struct packet_cg_client_version {
        uint8_t header;
        char filename[32 + 1];
        char timestamp[32 + 1];
    };

    struct packet_cg_client_version2 {
        uint8_t header;
        char filename[32 + 1];
        char timestamp[32 + 1];
    };

    struct packet_gc_channel {
        uint8_t header;
        uint8_t channel;
    };

    struct packet_view_equip {
        uint8_t header;
        uint32_t vid;
        struct {
            uint32_t vnum;
            uint8_t count;
            int32_t sockets[ITEM_SOCKET_MAX_NUM];
            player_item_attribute attr[ITEM_ATTRIBUTE_MAX_NUM];
        } equips[WEAR_MAX_NUM];
    };

    struct land_packet_element {
        uint32_t id;
        int32_t x, y;
        int32_t width, height;
        uint32_t guild_id;
    };

    struct packet_gc_land_list {
        uint8_t header;
        uint16_t size;
    };

    struct packet_gc_target_create {
        uint8_t header;
        int32_t id;
        char name[32 + 1];
        uint32_t vid;
        uint8_t type;
    };

    struct packet_gc_target_update {
        uint8_t header;
        int32_t id;
        int32_t x, y;
    };

    struct packet_gc_target_delete {
        uint8_t header;
        int32_t id;
    };

    // The client's layout: the flag stays 32-bit (the client does not use it).
    struct packet_gc_affect_element {
        uint32_t type;
        uint8_t apply_on;
        int32_t apply_value;
        uint32_t flag;
        int32_t duration;
        int32_t sp_cost;
    };

    struct packet_gc_affect_add {
        uint8_t header;
        packet_gc_affect_element elem;
    };

    struct packet_gc_affect_remove {
        uint8_t header;
        uint32_t type;
        uint8_t apply_on;
    };

    struct packet_gc_lover_info {
        uint8_t header;
        char name[CHARACTER_NAME_MAX_LEN + 1];
        uint8_t love_point;
    };

    struct packet_gc_love_point_update {
        uint8_t header;
        uint8_t love_point;
    };

    struct packet_gc_dig_motion {
        uint8_t header;
        uint32_t vid;
        uint32_t target_vid;
        uint8_t count;
    };

    struct packet_cg_script_select_item {
        uint8_t header;
        uint32_t selection;
    };

    struct packet_gc_damage_info {
        uint8_t header;
        uint32_t vid;
        uint8_t flag;
        int damage;
    };

    struct packet_gg_siege {
        uint8_t header;
        uint8_t empire;
        uint8_t tower_count;
    };

    struct packet_monarch_gg_transfer {
        uint8_t header;
        uint32_t target_pid;
        int32_t x;
        int32_t y;
    };

    struct packet_gg_check_awakeness {
        uint8_t header;
    };

    struct packet_key_agreement {
        static const int max_data_len = 256;
        uint8_t header;
        uint16_t agreed_length;
        uint16_t data_length;
        uint8_t data[max_data_len];
    };

    struct packet_key_agreement_completed {
        uint8_t header;
        uint8_t data[3];
    };

    inline constexpr int MAX_EFFECT_FILE_NAME = 128;
    struct packet_gc_specific_effect {
        uint8_t header;
        uint32_t vid;
        char effect_file[MAX_EFFECT_FILE_NAME];
    };

    enum dragon_soul_refine_window_refine_type {
        DragonSoulRefineWindow_UPGRADE,
    };

    enum packet_cg_dragon_soul_sub_header_type {
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
    struct packet_cg_dragon_soul_refine {
        uint8_t header;
        uint8_t sub_type;
        item_pos item_grid[DRAGON_SOUL_REFINE_GRID_SIZE];
    };

    struct packet_gc_dragon_soul_refine {
        packet_gc_dragon_soul_refine() : header(HEADER_GC_DRAGON_SOUL_REFINE) {}
        uint8_t header;
        uint8_t sub_type;
        item_pos pos;
    };

#pragma pack()

    // Shared with the client and the other processes; a size change breaks them.
    static_assert(sizeof(packet_gg_setup) == 4);
    static_assert(sizeof(packet_gg_login) == 36);
    static_assert(sizeof(packet_gg_logout) == 26);
    static_assert(sizeof(packet_gg_relay) == 30);
    static_assert(sizeof(packet_gg_notice) == 5);
    static_assert(sizeof(packet_gg_monarch_notice) == 6);
    static_assert(sizeof(packet_gg_shutdown) == 1);
    static_assert(sizeof(packet_gg_guild) == 6);
    static_assert(sizeof(packet_gg_guild_chat) == 519);
    static_assert(sizeof(packet_gg_disconnect) == 32);
    static_assert(sizeof(packet_gg_shout) == 515);
    static_assert(sizeof(packet_gg_xmas_warp_santa) == 6);
    static_assert(sizeof(packet_gg_xmas_warp_santa_reply) == 2);
    static_assert(sizeof(packet_gg_messenger) == 51);
    static_assert(sizeof(packet_gg_messenger_mobile) == 59);
    static_assert(sizeof(packet_gg_find_position) == 9);
    static_assert(sizeof(packet_gg_warp_character) == 13);
    static_assert(sizeof(packet_gg_guild_war_map_index) == 13);
    static_assert(sizeof(packet_gg_transfer) == 34);
    static_assert(sizeof(packet_gg_block_chat) == 30);
    static_assert(sizeof(packet_cg_text) == 1);
    static_assert(sizeof(packet_cg_handshake) == 13);
    static_assert(sizeof(packet_cg_login) == 49);
    static_assert(sizeof(packet_cg_login2) == 52);
    static_assert(sizeof(packet_cg_login3) == 65);
    static_assert(sizeof(packet_cg_player_select) == 2);
    static_assert(sizeof(packet_cg_player_delete) == 10);
    static_assert(sizeof(packet_cg_player_create) == 34);
    static_assert(sizeof(packet_gc_player_create_success) == 65);
    static_assert(sizeof(packet_cg_attack) == 8);
    static_assert(sizeof(packet_cg_move) == 16);
    static_assert(sizeof(packet_cg_sync_position_element) == 12);
    static_assert(sizeof(packet_cg_sync_position) == 3);
    static_assert(sizeof(packet_cg_chat) == 4);
    static_assert(sizeof(packet_cg_whisper) == 28);
    static_assert(sizeof(packet_cg_enter_game) == 1);
    static_assert(sizeof(packet_cg_item_use) == 4);
    static_assert(sizeof(packet_cg_item_use_to_item) == 7);
    static_assert(sizeof(packet_cg_item_drop) == 8);
    static_assert(sizeof(packet_cg_item_drop2) == 9);
    static_assert(sizeof(packet_cg_item_move) == 8);
    static_assert(sizeof(packet_cg_item_pickup) == 5);
    static_assert(sizeof(packet_cg_quickslot_add) == 4);
    static_assert(sizeof(packet_cg_quickslot_del) == 2);
    static_assert(sizeof(packet_cg_quickslot_swap) == 3);
    static_assert(sizeof(packet_cg_shop) == 2);
    static_assert(sizeof(packet_cg_on_click) == 5);
    static_assert(sizeof(packet_cg_exchange) == 10);
    static_assert(sizeof(packet_cg_position) == 2);
    static_assert(sizeof(packet_cg_script_answer) == 2);
    static_assert(sizeof(packet_cg_script_button) == 5);
    static_assert(sizeof(packet_cg_quest_input_string) == 66);
    static_assert(sizeof(packet_cg_quest_confirm) == 6);
    static_assert(sizeof(packet_gc_quest_confirm) == 74);
    static_assert(sizeof(packet_gc_handshake) == 13);
    static_assert(sizeof(packet_gc_phase) == 2);
    static_assert(sizeof(packet_gc_login_success) == 329);
    static_assert(sizeof(packet_gc_auth_success) == 6);
    static_assert(sizeof(packet_gc_login_failure) == 10);
    static_assert(sizeof(packet_gc_create_failure) == 2);
    static_assert(sizeof(packet_gc_character_add) == 36);
    static_assert(sizeof(packet_gc_character_additional_info) == 54);
    static_assert(sizeof(packet_gc_character_update) == 36);
    static_assert(sizeof(packet_gc_character_delete) == 5);
    static_assert(sizeof(packet_gc_chat) == 9);
    static_assert(sizeof(packet_gc_whisper) == 29);
    static_assert(sizeof(packet_gc_main_character) == 46);
    static_assert(sizeof(packet_gc_main_character3_bgm) == 71);
    static_assert(sizeof(packet_gc_main_character4_bgm_vol) == 75);
    static_assert(sizeof(packet_gc_points) == 1021);
    static_assert(sizeof(packet_gc_skill_level) == 1531);
    static_assert(sizeof(packet_gc_point_change) == 17);
    static_assert(sizeof(packet_gc_stun) == 5);
    static_assert(sizeof(packet_gc_dead) == 5);
    static_assert(sizeof(packet_gc_item_del_deprecated) == 42);
    static_assert(sizeof(packet_gc_item_set) == 51);
    static_assert(sizeof(packet_gc_item_del) == 2);
    static_assert(sizeof(packet_gc_item_update) == 38);
    static_assert(sizeof(packet_gc_item_ground_add) == 25);
    static_assert(sizeof(packet_gc_item_ownership) == 30);
    static_assert(sizeof(packet_gc_item_ground_del) == 5);
    static_assert(sizeof(packet_quickslot_add) == 4);
    static_assert(sizeof(packet_quickslot_del) == 2);
    static_assert(sizeof(packet_quickslot_swap) == 3);
    static_assert(sizeof(packet_shop_item) == 43);
    static_assert(sizeof(packet_gc_shop_start) == 1724);
    static_assert(sizeof(packet_gc_shop_start_ex) == 5);
    static_assert(sizeof(packet_gc_shop_update_item) == 44);
    static_assert(sizeof(packet_gc_shop) == 4);
    static_assert(sizeof(packet_exchange) == 47);
    static_assert(sizeof(packet_position) == 6);
    static_assert(sizeof(packet_gc_ping) == 1);
    static_assert(sizeof(packet_script) == 6);
    static_assert(sizeof(packet_gc_move) == 24);
    static_assert(sizeof(packet_gc_ownership) == 9);
    static_assert(sizeof(packet_gc_sync_position) == 3);
    static_assert(sizeof(packet_gc_create_fly) == 10);
    static_assert(sizeof(packet_cg_fly_targeting) == 13);
    static_assert(sizeof(packet_gc_fly_targeting) == 17);
    static_assert(sizeof(packet_cg_shoot) == 2);
    static_assert(sizeof(packet_gc_duel_start) == 3);
    static_assert(sizeof(packet_gcpvp) == 10);
    static_assert(sizeof(packet_cg_use_skill) == 9);
    static_assert(sizeof(packet_cg_target) == 5);
    static_assert(sizeof(packet_gc_target) == 6);
    static_assert(sizeof(packet_gc_warp) == 15);
    static_assert(sizeof(packet_cg_warp) == 1);
    static_assert(sizeof(packet_quest_info) == 7);
    static_assert(sizeof(packet_gc_messenger) == 4);
    static_assert(sizeof(packet_gc_messenger_list_offline) == 2);
    static_assert(sizeof(packet_gc_messenger_list_online) == 2);
    static_assert(sizeof(packet_cg_messenger) == 2);
    static_assert(sizeof(packet_cg_messenger_add_by_vid) == 4);
    static_assert(sizeof(packet_cg_safebox_checkout) == 5);
    static_assert(sizeof(packet_cg_safebox_checkin) == 5);
    static_assert(sizeof(packet_cg_party_parameter) == 2);
    static_assert(sizeof(packet_gc_party_parameter) == 2);
    static_assert(sizeof(packet_gc_party_add) == 30);
    static_assert(sizeof(packet_cg_party_invite) == 5);
    static_assert(sizeof(packet_gc_party_invite) == 5);
    static_assert(sizeof(packet_cg_party_invite_answer) == 6);
    static_assert(sizeof(packet_gc_party_update) == 21);
    static_assert(sizeof(packet_gc_party_remove) == 5);
    static_assert(sizeof(packet_gc_party_link) == 9);
    static_assert(sizeof(packet_cg_party_remove) == 5);
    static_assert(sizeof(packet_cg_party_set_state) == 7);
    static_assert(sizeof(packet_cg_party_use_skill) == 6);
    static_assert(sizeof(packet_cg_safebox_size) == 2);
    static_assert(sizeof(packet_cg_safebox_wrong_password) == 1);
    static_assert(sizeof(packet_cg_empire) == 2);
    static_assert(sizeof(packet_gc_empire) == 2);
    static_assert(sizeof(packet_gc_guild) == 4);
    static_assert(sizeof(packet_gc_guild_name) == 20);
    static_assert(sizeof(packet_gc_guild_war) == 10);
    static_assert(sizeof(packet_cg_guild) == 2);
    static_assert(sizeof(packet_cg_answer_make_guild) == 18);
    static_assert(sizeof(packet_gc_request_make_guild) == 5);
    static_assert(sizeof(packet_cg_guild_use_skill) == 8);
    static_assert(sizeof(packet_cg_mark_login) == 9);
    static_assert(sizeof(packet_cg_mark_upload) == 773);
    static_assert(sizeof(packet_cg_mark_idx_list) == 1);
    static_assert(sizeof(packet_cg_mark_crc_list) == 322);
    static_assert(sizeof(packet_gc_mark_idx_list) == 7);
    static_assert(sizeof(packet_gc_mark_block) == 10);
    static_assert(sizeof(packet_cg_guild_symbol_upload) == 7);
    static_assert(sizeof(packet_cg_symbol_crc) == 13);
    static_assert(sizeof(packet_gc_guild_symbol_data) == 7);
    static_assert(sizeof(packet_cg_fishing) == 2);
    static_assert(sizeof(packet_gc_fishing) == 7);
    static_assert(sizeof(packet_cg_give_item) == 9);
    static_assert(sizeof(packet_cg_hack) == 257);
    static_assert(sizeof(packet_gc_dungeon) == 4);
    static_assert(sizeof(packet_gc_dungeon_dest_position) == 8);
    static_assert(sizeof(packet_gc_shop_sign) == 38);
    static_assert(sizeof(packet_cg_my_shop) == 35);
    static_assert(sizeof(packet_gc_time) == 5);
    static_assert(sizeof(packet_gc_walk_mode) == 6);
    static_assert(sizeof(packet_gc_change_skill_group) == 2);
    static_assert(sizeof(packet_cg_refine) == 3);
    static_assert(sizeof(packet_gc_refine_information) == 60);
    static_assert(sizeof(npc_position) == 34);
    static_assert(sizeof(packet_gcnpc_position) == 5);
    static_assert(sizeof(packet_gc_special_effect) == 6);
    static_assert(sizeof(packet_cg_change_name) == 27);
    static_assert(sizeof(packet_gc_change_name) == 30);
    static_assert(sizeof(packet_cg_client_version) == 67);
    static_assert(sizeof(packet_cg_client_version2) == 67);
    static_assert(sizeof(packet_gc_channel) == 2);
    static_assert(sizeof(packet_view_equip) == 1221);
    static_assert(sizeof(land_packet_element) == 24);
    static_assert(sizeof(packet_gc_land_list) == 3);
    static_assert(sizeof(packet_gc_target_create) == 43);
    static_assert(sizeof(packet_gc_target_update) == 13);
    static_assert(sizeof(packet_gc_target_delete) == 5);
    static_assert(sizeof(packet_gc_affect_add) == 22);
    static_assert(sizeof(packet_gc_affect_remove) == 6);
    static_assert(sizeof(packet_gc_lover_info) == 27);
    static_assert(sizeof(packet_gc_love_point_update) == 2);
    static_assert(sizeof(packet_gc_dig_motion) == 10);
    static_assert(sizeof(packet_cg_script_select_item) == 5);
    static_assert(sizeof(packet_gc_damage_info) == 10);
    static_assert(sizeof(packet_gg_siege) == 3);
    static_assert(sizeof(packet_monarch_gg_transfer) == 13);
    static_assert(sizeof(packet_gg_check_awakeness) == 1);
    static_assert(sizeof(packet_key_agreement) == 261);
    static_assert(sizeof(packet_key_agreement_completed) == 4);
    static_assert(sizeof(packet_gc_specific_effect) == 133);
    static_assert(sizeof(packet_cg_dragon_soul_refine) == 47);
    static_assert(sizeof(packet_gc_dragon_soul_refine) == 5);
}
