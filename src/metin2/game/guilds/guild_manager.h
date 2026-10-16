#pragma once
#include <metin2/game/app/typedef.h>

#include <metin2/proto/tables.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace metin2::game::inline guilds {

    class guild;
    struct guild_create_parameter;

    inline constexpr const char GUILD_SYMBOL_FILENAME[] = "guild_symbol.tga";
    class guild_war_reserve_for_game {
      public:
        guild_war_reserve data;
        std::map<std::string, std::pair<uint32_t, uint32_t>> bet;
    };

    class guild_manager {
      public:
        guild_manager();
        ~guild_manager();

        auto create_guild(guild_create_parameter &gcp) -> uint32_t;
        auto find_guild(uint32_t guild_id) -> guild *;
        auto find_guild_by_name(const std::string guild_name) -> guild *;
        void load_guild(uint32_t guild_id);
        auto touch_guild(uint32_t guild_id) -> guild *;
        void disband_guild(uint32_t guild_id);

        void initialize();

        void link(uint32_t pid, guild *guild);
        void unlink(uint32_t pid);
        auto linked_guild(uint32_t pid) -> guild *;

        void login_member(character *ch);
        void p2p_login_member(uint32_t pid);
        void p2p_logout_member(uint32_t pid);

        void skill_recharge();

        void show_guild_war_list(character *ch);
        void send_guild_war(character *ch);

        void request_end_war(uint32_t guild_id1, uint32_t guild_id2);
        void request_cancel_war(uint32_t guild_id1, uint32_t guild_id2);
        void request_war_over(uint32_t guild1, uint32_t guild2, uint32_t guild_winner, int32_t reward);

        void declare_war(uint32_t guild_id1, uint32_t guild_id2, uint8_t type);
        void refuse_war(uint32_t guild_id1, uint32_t guild_id2);
        void start_war(uint32_t guild_id1, uint32_t guild_id2);
        void wait_start_war(uint32_t guild_id1, uint32_t guild_id2);
        void war_over(uint32_t guild_id1, uint32_t guild_id2, bool draw);
        void cancel_war(uint32_t guild_id1, uint32_t guild_id2);
        auto end_war(uint32_t guild_id1, uint32_t guild_id2) -> bool;
        void reserve_war(uint32_t guild1, uint32_t guild2, uint8_t type);

        void reserve_war_add(guild_war_reserve *p);
        void reserve_war_delete(uint32_t id);
        auto reserve_war_ref() -> std::vector<guild_war_reserve_for_game *> &;
        void reserve_war_bet(packet_gd_guild_war_bet *p);
        auto is_bet(uint32_t id, const char *login) -> bool;

        void stop_all_guild_war();

        void kill(character *killer, character *victim);

        auto rank(guild *g) -> int;
        void high_rank_string(uint32_t my_guild, char *buffer, size_t buflen);
        void around_rank_string(uint32_t my_guild, char *buffer, size_t buflen);

        template <typename Func>
        void for_each_war(Func &&f);

        auto disband_delay() -> int;
        auto withdraw_delay() -> int;

        void change_master(uint32_t gid);

      private:
        auto sorted_by_rank() -> std::vector<guild *>;

        using guild_map = std::map<uint32_t, guild *>;
        std::map<uint32_t, std::unique_ptr<guild>> guild_;

        using guild_war_container = std::set<std::pair<uint32_t, uint32_t>>;
        guild_war_container guild_war_;

        using guild_war_end_time_container = std::map<std::pair<uint32_t, uint32_t>, uint32_t>;
        guild_war_end_time_container guild_war_end_time_;

        guild_map guilds_by_pid_;

        std::map<uint32_t, std::unique_ptr<guild_war_reserve_for_game>> reserve_wars_;
        std::vector<guild_war_reserve_for_game *> reserve_war_list_;

        friend class guild;
    };

    inline auto get_guild_manager() -> guild_manager & {
        static guild_manager instance;
        return instance;
    }

    template <typename Func>
    void guild_manager::for_each_war(Func &&f) {
        for (auto it = guild_war_.begin(); it != guild_war_.end(); ++it) {
            f(it->first, it->second);
        }
    }

    extern void send_guild_war_score(uint32_t guild, uint32_t guild_opp, int delta, int bet_score_delta = 0);
}
