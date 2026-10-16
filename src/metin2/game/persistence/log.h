#pragma once

#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>
#include <metin2/sql/async_sql.h>

#include <cstdint>
#include <string>

namespace metin2::game::inline persistence {

    enum goldbar_how {
        PERSONAL_SHOP_BUY = 1,
        PERSONAL_SHOP_SELL = 2,
        SHOP_BUY = 3,
        SHOP_SELL = 4,
        EXCHANGE_TAKE = 5,
        EXCHANGE_GIVE = 6,
        QUEST = 7,
    };

    class log_manager : public singleton<log_manager> {
      public:
        log_manager();
        ~log_manager() override;

        auto is_connected() -> bool;

        auto connect(const char *host, const int port, const char *user, const char *pwd, const char *db) -> bool;

        void item_log(uint32_t pid, uint32_t x, uint32_t y, uint32_t item_id, const char *text, const char *hint, const char *ip, uint32_t vnum);
        void item_log(character *ch, item *item, const char *text, const char *hint);
        void item_log(character *ch, int item_id, int item_vnum, const char *text, const char *hint);

        void char_log(uint32_t pid, uint32_t x, uint32_t y, uint32_t value, const char *text, const char *hint, const char *ip);
        void char_log(character *ch, uint32_t dw, const char *text, const char *hint);

        void money_log(uint8_t type, uint32_t vnum, int gold);
        void hack_log(const char *hack_name, const char *login, const char *name, const char *ip);
        void hack_log(const char *hack_name, character *ch);
        void gold_bar_log(uint32_t pid, uint32_t item_id, goldbar_how how, const char *hint);
        void cube_log(uint32_t pid, uint32_t x, uint32_t y, uint32_t item_vnum, uint32_t item_uid, int item_count, bool success);
        void gm_command_log(uint32_t pid, const char *name, const char *ip, uint8_t channel, const char *command);
        void speed_hack_log(uint32_t pid, uint32_t x, uint32_t y, int hack_count);
        void change_name_log(uint32_t pid, const char *old_name, const char *new_name, const char *ip);
        void refine_log(uint32_t pid, const char *item_name, uint32_t item_id, int item_refine_level, int is_success, const char *how);
        void shout_log(uint8_t channel, uint8_t empire, const char *text);
        void level_log(character *ch, unsigned int level, unsigned int playhour);
        void boot_log(const char *host_name, uint8_t channel);
        void fish_log(uint32_t pid, int prob_idx, int fish_id, int fish_level, uint32_t miliseconds, uint32_t vnum = false, uint32_t value = 0);
        void quest_reward_log(const char *quest_name, uint32_t pid, uint32_t level, int value1, int value2);
        void detail_login_log(bool is_login, character *ch);
        void dragon_slay_log(uint32_t guild_id, uint32_t dragon_vnum, uint32_t start_time, uint32_t end_time);

      private:
        void query(const std::string &query);

        async_sql sql_;
        bool is_connect_;
    };
}
