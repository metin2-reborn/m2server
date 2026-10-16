#pragma once

#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/combat/skill.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/sql/async_sql.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>

namespace metin2::game::inline guilds {

    enum {
        GUILD_GRADE_NAME_MAX_LEN = 8,
        GUILD_GRADE_COUNT = 15,
        GUILD_COMMENT_MAX_COUNT = 12,
        GUILD_COMMENT_MAX_LEN = 50,
        GUILD_LEADER_GRADE = 1,
        GUILD_BASE_POWER = 400,
        GUILD_POWER_PER_LEVEL = 100,
        GUILD_WAR_MIN_MEMBER_COUNT = 8,
        GUILD_LADDER_POINT_PER_LEVEL = 1000,
        GUILD_CREATE_ITEM_VNUM = 70101,
    };

    struct guild_member {
        uint32_t pid;
        uint8_t grade;
        uint8_t is_general;
        uint8_t job;
        uint8_t level;
        uint32_t offer_exp;
        uint8_t dummy;

        std::string name;

        guild_member(uint32_t pid, uint8_t grade, uint8_t is_general, uint8_t job, uint8_t level, uint32_t offer_exp, char *name);
    };

#pragma pack(1)
    struct guild_member_packet_data {
        uint32_t pid;
        uint8_t grade;
        uint8_t is_general;
        uint8_t job;
        uint8_t level;
        uint32_t offer;
        uint8_t name_flag;
        char name[CHARACTER_NAME_MAX_LEN + 1];
    };

    struct packet_gc_guild_info {
        uint16_t member_count;
        uint16_t max_member_count;
        uint32_t guild_id;
        uint32_t master_pid;
        uint32_t exp;
        uint8_t level;
        char name[GUILD_NAME_MAX_LEN + 1];
        uint32_t gold;
        uint8_t has_land;
    };

    struct guild_grade {
        char grade_name[GUILD_GRADE_NAME_MAX_LEN + 1];
        uint8_t auth_flag;
    };

    struct one_grade_name_packet {
        uint8_t grade;
        char grade_name[GUILD_GRADE_NAME_MAX_LEN + 1];
    };

    struct one_grade_auth_packet {
        uint8_t grade;
        uint8_t auth;
    };
#pragma pack()

    enum {
        GUILD_AUTH_ADD_MEMBER = (1 << 0),
        GUILD_AUTH_REMOVE_MEMBER = (1 << 1),
        GUILD_AUTH_NOTICE = (1 << 2),
        GUILD_AUTH_USE_SKILL = (1 << 3),
    };

    struct guild_data {
        uint32_t guild_id;
        uint32_t master_pid;
        uint32_t exp;
        uint8_t level;
        char name[GUILD_NAME_MAX_LEN + 1];

        guild_grade grade_array[GUILD_GRADE_COUNT];

        uint8_t skill_point;
        uint8_t aby_skill[GUILD_SKILL_COUNT];

        int power;
        int max_power;

        int ladder_point;

        int win;
        int draw;
        int loss;

        int gold;
    };

    struct guild_create_parameter {
        character *master;
        char name[GUILD_NAME_MAX_LEN + 1];
    };

    struct guild_war {
        uint32_t war_start_time;
        uint32_t score;
        uint32_t state;
        uint8_t type;
        uint32_t map_index;

        guild_war(uint8_t type)
            : war_start_time(0),
              score(0),
              state(GUILD_WAR_RECV_DECLARE),
              type(type),
              map_index(0) {
        }
        auto is_war_begin() const -> bool {
            return state == GUILD_WAR_ON_WAR;
        }
    };

    class guild {
      public:
        guild(guild_create_parameter &cp);
        explicit guild(uint32_t guild_id) { load(guild_id); }
        ~guild();

        auto id() const -> uint32_t { return data_.guild_id; }
        auto name() const -> const char * { return data_.name; }
        auto sp() const -> int { return data_.power; }
        auto master_pid() const -> uint32_t { return data_.master_pid; }
        auto master_character() -> character *;
        auto level() const -> uint8_t { return data_.level; }

        void reset() { data_.power = data_.max_power; }

        void request_disband(uint32_t pid);
        void disband();

        void request_add_member(character *ch, int grade = 15);
        void add_member(packet_dg_guild_member *p);

        auto request_remove_member(uint32_t pid) -> bool;
        auto remove_member(uint32_t pid) -> bool;

        void login_member(character *ch);
        void p2p_login_member(uint32_t pid);

        void logout_member(character *ch);
        void p2p_logout_member(uint32_t pid);

        void change_member_grade(uint32_t pid, uint8_t grade);
        auto offer_exp(character *ch, int amount) -> bool;
        void level_change(uint32_t pid, uint8_t level);
        void change_member_data(uint32_t pid, uint32_t offer, uint8_t level, uint8_t grade);

        void change_grade_name(uint8_t grade, const char *grade_name);
        void change_grade_auth(uint8_t grade, uint8_t auth);
        void p2p_change_grade(uint8_t grade);

        auto change_member_general(uint32_t pid, uint8_t is_general) -> bool;

        auto change_master_to(uint32_t pid) -> bool;

        void packet(const void *buf, int size);

        void send_online_remove_one_packet(uint32_t pid);
        void send_all_grade_packet(character *ch);
        void send_list_packet(character *ch);
        void send_list_one_to_all(uint32_t pid);
        void send_login_packet(character *ch, character *ch_login);
        void send_logout_packet(character *ch, character *ch_logout);
        void send_login_packet(character *ch, uint32_t pid);
        void send_logout_packet(character *ch, uint32_t pid);
        void send_guild_info_packet(character *ch);
        void send_guild_data_update_to_all_member(sql_msg &msg);

        void load(uint32_t guild_id);
        void save_level();
        void save_skill();
        void save_member(uint32_t pid);

        auto max_member_count() -> int;
        auto member_count() -> int { return member_.size(); }

        void set_member_count_bonus(int bonus);
        void broadcast_member_count_bonus();

        auto max_general_count() const -> int { return 1; }
        auto general_count() const -> int { return general_count_; }

        auto member(uint32_t pid) -> guild_member *;
        auto member_pid(const std::string &name_value) -> uint32_t;

        auto has_grade_auth(int grade, int auth_flag) const -> bool { return (bool)(data_.grade_array[grade - 1].auth_flag & auth_flag); }

        void add_comment(character *ch, const std::string &str);
        void delete_comment(character *ch, uint32_t comment_id);

        void refresh_comment(character *ch);
        void refresh_comment_force(uint32_t player_id);

        auto skill_level(uint32_t vnum) -> int;
        void skill_level_up(uint32_t vnum_value);
        void use_skill(uint32_t vnum_value, character *ch, uint32_t pid);

        void send_skill_info_packet(character *ch) const;
        void compute_guild_points();

        void guild_point_change(uint8_t type, int amount, bool save = false);

        void update_skill(uint8_t skill_point, uint8_t *skill_levels);
        void send_db_skill_update(int amount = 0);

        void skill_recharge();
        auto charge_sp(character *ch, int sp) -> bool;

        void chat(const char *text_value);
        void p2p_chat(const char *text);

        void skill_usable_change(uint32_t skill_vnum, bool usable);

        void request_deposit_money(character *ch, int gold_value);

        void recv_money_change(int gold_value);
        void recv_withdraw_money_give(int change_gold_value);

        auto guild_money() const -> int { return data_.gold; }

        void guild_war_packet(uint32_t opp_gid, uint8_t war_type, uint8_t war_state_value);
        void send_enemy_guild(character *ch);

        auto get_guild_war_state(uint32_t opp_gid) -> int;
        auto can_start_war(uint8_t guild_war_type) -> bool;
        auto war_start_time(uint32_t opp_gid) -> uint32_t;
        auto under_war(uint32_t opp_gid) -> bool;
        auto under_any_war(uint8_t type_value = GUILD_WAR_TYPE_MAX_NUM) -> uint32_t;

        void set_guild_war_map_index(uint32_t opp_gid, int32_t map_index_value);
        auto get_guild_war_type(uint32_t opp_gid) -> int;
        auto guild_war_map_index(uint32_t opp_gid) -> uint32_t;

        void guild_war_entry_ask(uint32_t opp_gid);
        void guild_war_entry_accept(uint32_t opp_gid, character *ch);

        void notify_guild_master(const char *msg);
        void request_declare_war(uint32_t opp_gid, uint8_t type);
        void request_refuse_war(uint32_t opp_gid);

        auto declare_war(uint32_t opp_gid, uint8_t type, uint8_t state) -> bool;
        void refuse_war(uint32_t opp_gid);
        auto wait_start_war(uint32_t opp_gid) -> bool;
        auto check_start_war(uint32_t opp_gid) -> bool;
        void start_war(uint32_t opp_gid);
        void end_war(uint32_t opp_gid);
        void reserve_war(uint32_t opp_gid, uint8_t type);

        void set_war_score_against_to(uint32_t opp_gid, int score_value);
        auto war_score_against_to(uint32_t opp_gid) -> int;

        auto ladder_point() const -> int { return data_.ladder_point; }
        void set_ladder_point(int point);

        void set_war_data(int win_value, int draw_value, int loss_value) { data_.win = win_value, data_.draw = draw_value, data_.loss = loss_value; }

        void change_ladder_point(int change_value);

        auto guild_war_win_count() const -> int { return data_.win; }
        auto guild_war_draw_count() const -> int { return data_.draw; }
        auto guild_war_loss_count() const -> int { return data_.loss; }

        auto has_land() -> bool;

        void invite(character *pch_inviter, character *pch_invitee);

        void invite_accept(character *pch_invitee);

        void invite_deny(uint32_t pid);

      private:
        void initialize();

        guild_data data_;
        int general_count_;

        int member_count_bonus_;

        typedef std::map<uint32_t, guild_member> guild_member_container;
        guild_member_container member_;

        typedef character_set guild_member_online_container;
        guild_member_online_container member_online_;

        typedef std::set<uint32_t> guild_member_p2p_online_container;
        guild_member_p2p_online_container member_p2p_online_;

        void load_guild_data(sql_msg &msg);
        void load_guild_grade_data(sql_msg &msg);
        void load_guild_member_data(sql_msg &msg);

        void p2p_update_grade(sql_msg &msg);

        typedef std::map<uint32_t, guild_war> enemy_guild_container;
        enemy_guild_container enemy_guild_;

        std::map<uint32_t, uint32_t> guild_war_end_time_;

        bool skill_usable_[GUILD_SKILL_COUNT];

        enum guild_join_err_code {
            GERR_NONE = 0,
            GERR_WITHDRAWPENALTY,
            GERR_COMMISSIONPENALTY,
            GERR_ALREADYJOIN,
            GERR_GUILDISFULL,
            GERR_GUILD_IS_IN_WAR,
            GERR_INVITE_LIMIT,
            GERR_MAX
        };

        auto verify_guild_joinable_condition(character *const pch_invitee) -> guild_join_err_code;

        typedef std::map<uint32_t, event_ptr> event_map;
        event_map guild_invite_event_map_;
    };
}
