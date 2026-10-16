#pragma once

#include <metin2/proto/length.h>

#include <cstdint>

namespace metin2::inline proto {

    // Internal game/DB routing capacity; all processes must use the same build.
    constexpr int MAX_MAPS_PER_SERVER = 128;

    enum {
        HEADER_GD_LOGIN = 1,
        HEADER_GD_LOGOUT = 2,

        HEADER_GD_PLAYER_LOAD = 3,
        HEADER_GD_PLAYER_SAVE = 4,
        HEADER_GD_PLAYER_CREATE = 5,
        HEADER_GD_PLAYER_DELETE = 6,

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

        HEADER_GD_MYSHOP_PRICELIST_UPDATE = 108,
        HEADER_GD_MYSHOP_PRICELIST_REQ = 109,

        HEADER_GD_BLOCK_CHAT = 110,

        HEADER_GD_HAMMER_OF_TOR = 114,
        HEADER_GD_RELOAD_ADMIN = 115,
        HEADER_GD_BREAK_MARRIAGE = 116,
        HEADER_GD_ELECT_MONARCH = 117,
        HEADER_GD_CANDIDACY = 118,
        HEADER_GD_ADD_MONARCH_MONEY = 119,
        HEADER_GD_TAKE_MONARCH_MONEY = 120,
        HEADER_GD_COME_TO_VOTE = 121,
        HEADER_GD_RMCANDIDACY = 122,
        HEADER_GD_SETMONARCH = 123,
        HEADER_GD_RMMONARCH = 124,
        HEADER_GD_DEC_MONARCH_MONEY = 125,

        HEADER_GD_CHANGE_MONARCH_LORD = 126,

        HEADER_GD_REQ_CHANGE_GUILD_MASTER = 129,

        HEADER_GD_REQ_SPARE_ITEM_ID_RANGE = 130,

        HEADER_GD_UPDATE_HORSE_NAME = 131,
        HEADER_GD_REQ_HORSE_NAME = 132,

        HEADER_GD_DC = 133,

        HEADER_GD_VALID_LOGOUT = 134,

        HEADER_GD_REQUEST_CHARGE_CASH = 137,

        HEADER_GD_DELETE_AWARDID = 138,

        HEADER_GD_UPDATE_CHANNELSTATUS = 139,
        HEADER_GD_REQUEST_CHANNELSTATUS = 140,

        HEADER_GD_SETUP = 0xff,

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

        HEADER_DG_MYSHOP_PRICELIST_RES = 157,
        HEADER_DG_RELOAD_ADMIN = 158,
        HEADER_DG_ELECT_MONARCH = 160,
        HEADER_DG_CANDIDACY = 161,
        HEADER_DG_ADD_MONARCH_MONEY = 162,
        HEADER_DG_TAKE_MONARCH_MONEY = 163,
        HEADER_DG_RMCANDIDACY = 165,
        HEADER_DG_RMMONARCH = 167,
        HEADER_DG_DEC_MONARCH_MONEY = 168,

        HEADER_DG_CHANGE_MONARCH_LORD_ACK = 169,
        HEADER_DG_UPDATE_MONARCH_INFO = 170,

        HEADER_DG_ACK_CHANGE_GUILD_MASTER = 173,

        HEADER_DG_ACK_SPARE_ITEM_ID_RANGE = 174,

        HEADER_DG_UPDATE_HORSE_NAME = 175,
        HEADER_DG_ACK_HORSE_NAME = 176,

        HEADER_DG_NEED_LOGIN_LOG = 177,
        HEADER_DG_ITEMAWARD_INFORMER = 180,
        HEADER_DG_RESPOND_CHANNELSTATUS = 181,

        HEADER_DG_MAP_LOCATIONS = 0xfe,
        HEADER_DG_P2P = 0xff,

    };

#pragma pack(1)
    enum request_charge_type {
        ERequestCharge_Cash = 0,
        ERequestCharge_Mileage,
    };

    struct request_charge_cash {
        uint32_t aid;
        uint32_t amount;
        request_charge_type charge_type;
    };

    struct simple_player {
        uint32_t id;
        char name[CHARACTER_NAME_MAX_LEN + 1];
        uint8_t job;
        uint8_t level;
        uint32_t play_minutes;
        uint8_t st, ht, dx, iq;
        uint16_t main_part;
        uint8_t change_name;
        uint16_t hair_part;
        uint8_t dummy[4];
        int32_t x, y;
        int32_t addr;
        uint16_t port;
        uint8_t skill_group;
    };

    struct account_table {
        uint32_t id;
        char login[LOGIN_MAX_LEN + 1];
        char passwd[PASSWD_MAX_LEN + 1];
        char social_id[SOCIAL_ID_MAX_LEN + 1];
        char status[ACCOUNT_STATUS_MAX_LEN + 1];
        uint8_t empire;
        simple_player players[PLAYER_PER_ACCOUNT];
    };

    struct packet_dg_create_success {
        uint8_t account_character_index;
        simple_player player;
    };

    struct player_item_attribute {
        uint8_t type;
        short s_value;
    };

    struct player_item {
        uint32_t id;
        uint8_t window;
        uint16_t pos;
        uint32_t count;

        uint32_t vnum;
        int32_t sockets[ITEM_SOCKET_MAX_NUM];

        player_item_attribute attr[ITEM_ATTRIBUTE_MAX_NUM];

        uint32_t owner;
    };

    struct quickslot {
        uint8_t type;
        uint8_t pos;
    };

    struct player_skill {
        uint8_t master_type;
        uint8_t level;
        int32_t t_next_read;
    };

    struct horse_info {
        uint8_t level;
        uint8_t riding;
        short s_stamina;
        short s_health;
        uint32_t horse_health_drop_time;
    };

    struct player_table {
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
        int32_t map_index;

        int32_t exit_x, exit_y;
        int32_t exit_map_index;

        int hp;
        int sp;

        int s_random_hp;
        int s_random_sp;

        int playtime;

        short stat_point;
        short skill_point;
        short sub_skill_point;
        short horse_skill_point;

        player_skill skills[SKILL_MAX_NUM];

        proto::quickslot quickslot[QUICKSLOT_MAX_NUM];

        uint8_t part_base;
        uint16_t parts[PART_MAX_NUM];

        short stamina;

        uint8_t skill_group;
        int32_t alignment;
        char mobile[MOBILE_MAX_LEN + 1];

        short stat_reset_count;

        horse_info horse;

        uint32_t logoff_interval;

        int premium_times[PREMIUM_MAX_NUM];
    };

    struct mob_skill_level {
        uint32_t vnum;
        uint8_t level;
    };

    struct entity_table {
        uint32_t vnum;
    };

    struct mob_table : public entity_table {
        char name[PROTO_NAME_MAX_LEN + 1];
        char locale_name[PROTO_NAME_MAX_LEN + 1];

        uint8_t type;
        uint8_t rank;
        uint8_t battle_type;
        uint8_t level;
        uint8_t size;

        uint32_t gold_min;
        uint32_t gold_max;
        uint32_t exp;
        uint32_t max_hp;
        uint8_t regen_cycle;
        uint8_t regen_percent;
        uint16_t def;

        uint32_t ai_flag;
        uint32_t race_flag;
        uint32_t immune_flag;

        uint8_t str, dex, con, iq;
        uint32_t damage_range[2];

        short s_attack_speed;
        short s_moving_speed;
        uint8_t aggresive_hp_pct;
        uint16_t aggressive_sight;
        uint16_t attack_range;

        char c_enchants[MOB_ENCHANTS_MAX_NUM];
        char c_resists[MOB_RESISTS_MAX_NUM];

        uint32_t resurrection_vnum;
        uint32_t drop_item_vnum;

        uint8_t mount_capacity;
        uint8_t on_click_type;

        uint8_t empire;
        char folder[64 + 1];

        float dam_multiply;

        uint32_t summon_vnum;
        uint32_t drain_sp;
        uint32_t mob_color;
        uint32_t polymorph_item_vnum;

        mob_skill_level skills[MOB_SKILL_MAX_NUM];

        uint8_t berserk_point;
        uint8_t stone_skin_point;
        uint8_t god_speed_point;
        uint8_t death_blow_point;
        uint8_t revive_point;
    };

    struct skill_table {
        uint32_t vnum;
        char name[32 + 1];
        uint8_t type;
        uint8_t max_level;
        uint32_t splash_range;

        char point_on[64];
        char point_poly[100 + 1];
        char sp_cost_poly[100 + 1];
        char duration_poly[100 + 1];
        char duration_sp_cost_poly[100 + 1];
        char cooldown_poly[100 + 1];
        char master_bonus_poly[100 + 1];
        char grand_master_add_sp_cost_poly[100 + 1];
        uint32_t flag;
        uint32_t affect_flag;

        char point_on2[64];
        char point_poly2[100 + 1];
        char duration_poly2[100 + 1];
        uint32_t affect_flag2;

        char point_on3[64];
        char point_poly3[100 + 1];
        char duration_poly3[100 + 1];

        uint8_t level_step;
        uint8_t level_limit;
        uint32_t pre_skill_vnum;
        uint8_t pre_skill_level;

        int32_t max_hit;
        char splash_around_damage_adjust_poly[100 + 1];

        uint8_t skill_attr_type;

        uint32_t target_range;
    };

    struct shop_item_table {
        uint32_t vnum;
        uint8_t count;

        item_pos pos;
        uint32_t price;
        uint8_t display_pos;
    };

    struct shop_table {
        uint32_t vnum;
        uint32_t npc_vnum;

        uint8_t item_count;
        shop_item_table items[SHOP_HOST_ITEM_MAX_NUM];
    };

    inline constexpr int QUEST_NAME_MAX_LEN = 32;
    inline constexpr int QUEST_STATE_MAX_LEN = 64;

    struct quest_table {
        uint32_t pid;
        char name[QUEST_NAME_MAX_LEN + 1];
        char state[QUEST_STATE_MAX_LEN + 1];
        int32_t value;
    };

    struct item_limit {
        uint8_t type;
        int32_t value;
    };

    struct item_apply {
        uint8_t type;
        int32_t value;
    };

    struct item_table : public entity_table {
        uint32_t vnum_range;
        char name[PROTO_NAME_MAX_LEN + 1];
        char locale_name[PROTO_NAME_MAX_LEN + 1];
        uint8_t type;
        uint8_t sub_type;

        uint8_t weight;
        uint8_t size;

        uint32_t anti_flags;
        uint32_t flags;
        uint32_t wear_flags;
        uint32_t immune_flag;

        uint32_t gold;
        uint32_t shop_buy_price;

        item_limit limits[ITEM_LIMIT_MAX_NUM];
        item_apply applies[ITEM_APPLY_MAX_NUM];
        int32_t values[ITEM_VALUES_MAX_NUM];
        int32_t sockets[ITEM_SOCKET_MAX_NUM];
        uint32_t refined_vnum;
        uint16_t refine_set;
        uint8_t alter_to_magic_item_pct;
        uint8_t specular;
        uint8_t gain_socket_pct;

        short int s_addon_type;

        char c_limit_real_time_first_use_index;
        char c_limit_timer_based_on_wear_index;
    };

    struct item_attr_table {
        item_attr_table() : apply{}, apply_index(0), prob(0), values{}, max_level_by_set{} {}

        char apply[APPLY_NAME_MAX_LEN + 1];
        uint32_t apply_index;
        uint32_t prob;
        int32_t values[5];
        uint8_t max_level_by_set[ATTRIBUTE_SET_MAX_NUM];
    };

    struct login_packet {
        char login[LOGIN_MAX_LEN + 1];
        char passwd[PASSWD_MAX_LEN + 1];
    };

    struct player_load_packet {
        uint32_t account_id;
        uint32_t player_id;
        uint8_t account_index;
    };

    struct player_create_packet {
        char login[LOGIN_MAX_LEN + 1];
        char passwd[PASSWD_MAX_LEN + 1];
        uint32_t account_id;
        uint8_t account_index;
        proto::player_table player_table;
    };

    struct player_delete_packet {
        char login[LOGIN_MAX_LEN + 1];
        uint32_t player_id;
        uint8_t account_index;
        char private_code[8];
    };

    struct logout_packet {
        char login[LOGIN_MAX_LEN + 1];
        char passwd[PASSWD_MAX_LEN + 1];
    };

    struct player_count_packet {
        uint32_t count;
    };

    inline constexpr int SAFEBOX_MAX_NUM = 135;
    inline constexpr int SAFEBOX_PASSWORD_MAX_LEN = 6;

    struct safebox_table {
        uint32_t id;
        uint8_t size;
        uint32_t gold;
        uint16_t item_count;
    };

    struct safebox_change_size_packet {
        uint32_t id;
        uint8_t size;
    };

    struct safebox_load_packet {
        uint32_t id;
        char login[LOGIN_MAX_LEN + 1];
        char password[SAFEBOX_PASSWORD_MAX_LEN + 1];
    };

    struct safebox_change_password_packet {
        uint32_t id;
        char old_password[SAFEBOX_PASSWORD_MAX_LEN + 1];
        char new_password[SAFEBOX_PASSWORD_MAX_LEN + 1];
    };

    struct safebox_change_password_packet_answer {
        uint8_t flag;
    };

    struct empire_select_packet {
        uint32_t account_id;
        uint8_t empire;
    };

    struct packet_gd_setup {
        char public_ip[16];
        uint8_t channel;
        uint16_t listen_port;
        uint16_t p2_p_port;
        int32_t maps[MAX_MAPS_PER_SERVER];
        uint32_t login_count;
        uint8_t auth_server;
    };

    struct map_location {
        int32_t maps[MAX_MAPS_PER_SERVER];
        char host[MAX_HOST_LENGTH + 1];
        uint16_t port;
    };

    struct packet_dg_p2_p {
        char host[MAX_HOST_LENGTH + 1];
        uint16_t port;
        uint8_t channel;
    };

    struct packet_guild_skill_update {
        uint32_t guild_id;
        int amount;
        uint8_t skill_levels[12];
        uint8_t skill_point;
        uint8_t save;
    };

    struct packet_guild_exp_update {
        uint32_t guild_id;
        int amount;
    };

    struct packet_guild_change_member_data {
        uint32_t guild_id;
        uint32_t pid;
        uint32_t offer;
        uint8_t level;
        uint8_t grade;
    };

    struct packet_dg_login_already {
        char login[LOGIN_MAX_LEN + 1];
    };

    struct packet_affect_element {
        uint32_t type;
        uint8_t apply_on;
        int32_t apply_value;
        uint32_t flag;
        int32_t duration;
        int32_t sp_cost;
    };

    struct packet_gd_add_affect {
        uint32_t pid;
        packet_affect_element elem;
    };

    struct packet_gd_remove_affect {
        uint32_t pid;
        uint32_t type;
        uint8_t apply_on;
    };

    struct packet_gd_highscore {
        uint32_t pid;
        int32_t value;
        char c_dir;
        char board[21];
    };

    struct packet_party_create {
        uint32_t leader_pid;
    };

    struct packet_party_delete {
        uint32_t leader_pid;
    };

    struct packet_party_add {
        uint32_t leader_pid;
        uint32_t pid;
        uint8_t state;
    };

    struct packet_party_remove {
        uint32_t leader_pid;
        uint32_t pid;
    };

    struct packet_party_state_change {
        uint32_t leader_pid;
        uint32_t pid;
        uint8_t role;
        uint8_t flag;
    };

    struct packet_party_set_member_level {
        uint32_t leader_pid;
        uint32_t pid;
        uint8_t level;
    };

    struct packet_gd_boot {
        uint32_t item_id_range[2];
        char ip[16];
    };

    struct packet_guild {
        uint32_t guild;
        uint32_t info;
    };

    struct packet_gd_guild_add_member {
        uint32_t pid;
        uint32_t guild;
        uint8_t grade;
    };

    struct packet_dg_guild_member {
        uint32_t pid;
        uint32_t guild;
        uint8_t grade;
        uint8_t is_general;
        uint8_t job;
        uint8_t level;
        uint32_t offer;
        char name[CHARACTER_NAME_MAX_LEN + 1];
    };

    struct packet_guild_war {
        uint8_t type;
        uint8_t war;
        uint32_t guild_from;
        uint32_t guild_to;
        int32_t war_price;
        int32_t initial_score;
    };

    struct packet_guild_war_score {
        uint32_t guild_gain_point;
        uint32_t guild_opponent;
        int32_t score;
        int32_t bet_score;
    };

    struct refine_material {
        uint32_t vnum;
        int count;
    };

    struct refine_table {
        uint32_t id;
        uint8_t material_count;
        int cost;
        int prob;
        refine_material materials[REFINE_MATERIAL_MAX_NUM];
    };

    struct banword_table {
        char word[BANWORD_MAX_LEN + 1];
    };

    struct packet_gd_change_name {
        uint32_t pid;
        char name[CHARACTER_NAME_MAX_LEN + 1];
    };

    struct packet_dg_change_name {
        uint32_t pid;
        char name[CHARACTER_NAME_MAX_LEN + 1];
    };

    struct packet_guild_ladder {
        uint32_t guild;
        int32_t ladder_point;
        int32_t win;
        int32_t draw;
        int32_t loss;
    };

    struct packet_guild_ladder_point {
        uint32_t guild;
        int32_t change;
    };

    struct packet_gdsms {
        char from[CHARACTER_NAME_MAX_LEN + 1];
        char to[CHARACTER_NAME_MAX_LEN + 1];
        char mobile[MOBILE_MAX_LEN + 1];
        char msg[SMS_MAX_LEN + 1];
    };

    struct packet_guild_use_skill {
        uint32_t guild;
        uint32_t skill_vnum;
        uint32_t cooltime;
    };

    struct packet_guild_skill_usable_change {
        uint32_t guild;
        uint32_t skill_vnum;
        uint8_t usable;
    };

    struct packet_gd_auth_login {
        uint32_t id;
        uint32_t login_key;
        char login[LOGIN_MAX_LEN + 1];
        char social_id[SOCIAL_ID_MAX_LEN + 1];
        uint32_t client_key[4];
        int premium_times[PREMIUM_MAX_NUM];
    };

    struct packet_gd_login_by_key {
        char login[LOGIN_MAX_LEN + 1];
        uint32_t login_key;
        uint32_t client_key[4];
        char ip[MAX_HOST_LENGTH + 1];
    };

    struct packet_give_guild_priv {
        uint8_t type;
        int value;
        uint32_t guild_id;
        int32_t duration_sec;
    };
    struct packet_give_empire_priv {
        uint8_t type;
        int value;
        uint8_t empire;
        int32_t duration_sec;
    };
    struct packet_give_character_priv {
        uint8_t type;
        int value;
        uint32_t pid;
    };

    struct packet_dg_change_character_priv {
        uint8_t type;
        int value;
        uint32_t pid;
        uint8_t log;
    };

    struct packet_dg_change_guild_priv {
        uint8_t type;
        int value;
        uint32_t guild_id;
        uint8_t log;
        int32_t end_time_sec;
    };

    struct packet_dg_change_empire_priv {
        uint8_t type;
        int value;
        uint8_t empire;
        uint8_t log;
        int32_t end_time_sec;
    };

    struct packet_money_log {
        uint8_t type;
        uint32_t vnum;
        int32_t gold;
    };

    struct packet_gd_guild_money {
        uint32_t guild;
        int32_t gold;
    };

    struct packet_dg_guild_money_change {
        uint32_t guild;
        int32_t total_gold;
    };

    struct packet_dg_guild_money_withdraw {
        uint32_t guild;
        int32_t change_gold;
    };

    struct packet_gd_guild_money_withdraw_give_reply {
        uint32_t guild;
        int32_t change_gold;
        uint8_t give_success;
    };

    struct packet_set_event_flag {
        char flag_name[EVENT_FLAG_NAME_MAX_LEN + 1];
        int32_t value;
    };

    struct packet_login_on_setup {
        uint32_t id;
        char login[LOGIN_MAX_LEN + 1];
        char social_id[SOCIAL_ID_MAX_LEN + 1];
        char host[MAX_HOST_LENGTH + 1];
        uint32_t login_key;
        uint32_t client_key[4];
    };

    struct packet_gd_create_object {
        uint32_t vnum;
        uint32_t land_id;
        int32_t map_index;
        int32_t x, y;
        float x_rot;
        float y_rot;
        float z_rot;
    };

    struct guild_war_reserve {
        uint32_t id;
        uint32_t guild_from;
        uint32_t guild_to;
        uint32_t time;
        uint8_t type;
        int32_t war_price;
        int32_t initial_score;
        bool started;
        uint32_t bet_from;
        uint32_t bet_to;
        int32_t power_from;
        int32_t power_to;
        int32_t handicap;
    };

    struct packet_gd_guild_war_bet {
        uint32_t war_id;
        char login[LOGIN_MAX_LEN + 1];
        uint32_t gold;
        uint32_t guild;
    };

    struct packet_marriage_add {
        uint32_t pi_d1;
        uint32_t pi_d2;
        int32_t t_marry_time;
        char name1[CHARACTER_NAME_MAX_LEN + 1];
        char name2[CHARACTER_NAME_MAX_LEN + 1];
    };

    struct packet_marriage_update {
        uint32_t pi_d1;
        uint32_t pi_d2;
        int32_t love_point;
        uint8_t married;
    };

    struct packet_marriage_remove {
        uint32_t pi_d1;
        uint32_t pi_d2;
    };

    struct packet_wedding_request {
        uint32_t pi_d1;
        uint32_t pi_d2;
    };

    struct packet_wedding_ready {
        uint32_t pi_d1;
        uint32_t pi_d2;
        uint32_t map_index;
    };

    struct packet_wedding_start {
        uint32_t pi_d1;
        uint32_t pi_d2;
    };

    struct packet_wedding_end {
        uint32_t pi_d1;
        uint32_t pi_d2;
    };

    struct packet_myshop_pricelist_header {
        uint32_t owner_id;
        uint8_t count;
    };

    struct item_price_info {
        uint32_t vnum;
        uint32_t price;
    };

    struct item_price_list_table {
        uint32_t owner_id;
        uint8_t count;

        item_price_info price_info[SHOP_PRICELIST_MAX_NUM];
    };

    struct packet_block_chat {
        char name[CHARACTER_NAME_MAX_LEN + 1];
        int32_t duration;
    };

    struct t_admin_info {
        int id;
        char account[32];
        char name[32];
        char contact_ip[16];
        char server_ip[16];
        int authority;
    };

    struct t_locale {
        char value[32];
        char key[32];
    };

    struct packet_reload_admin {
        char ip[16];
    };

    struct monarch_info {
        uint32_t pid[4];
        int64_t money[4];
        char name[4][32];
        char date[4][32];
    };

    struct monarch_election_info {
        uint32_t pid;
        uint32_t selectedpid;
        char date[32];
    };

    struct monarch_candidacy {
        uint32_t pid;
        char name[32];
        char date[32];
    };

    struct packet_change_monarch_lord {
        uint8_t empire;
        uint32_t pid;
    };

    struct packet_change_monarch_lord_ack {
        uint8_t empire;
        uint32_t pid;
        char name[32];
        char date[32];
    };

    struct packet_change_guild_master {
        uint32_t guild_id;
        uint32_t id_from;
        uint32_t id_to;
    };

    struct item_id_range_table {
        uint32_t min;
        uint32_t max;
        uint32_t usable_item_id_min;
    };

    struct packet_update_horse_name {
        uint32_t player_id;
        char horse_name[CHARACTER_NAME_MAX_LEN + 1];
    };

    struct packet_dc {
        char login[LOGIN_MAX_LEN + 1];
    };

    struct packet_need_login_log_info {
        uint32_t player_id;
    };

    struct packet_item_award_infromer {
        char login[LOGIN_MAX_LEN + 1];
        char command[20];
        unsigned int vnum;
    };

    struct packet_delete_award_id {
        uint32_t id;
    };

    struct channel_status {
        short port;
        uint8_t status;
    };

#pragma pack()
}
