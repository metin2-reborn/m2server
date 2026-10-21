#pragma once
#include <metin2/game/app/typedef.h>

#include <metin2/proto/limits.h>
#include <metin2/proto/log_types.h>

#include <cstdint>
#include <ctime>
#include <map>

namespace metin2::game::inline empire {
    class priv_manager {
      public:
        priv_manager();

        void request_give_guild_priv(uint32_t guild_id, uint8_t type, int value, time_t duration_sec);
        void request_give_empire_priv(uint8_t empire, uint8_t type, int value, time_t duration_sec);
        void request_give_character_priv(uint32_t pid, uint8_t type, int value);

        void give_guild_priv(uint32_t guild_id, uint8_t type, int value, uint8_t log, time_t end_time_sec);
        void give_empire_priv(uint8_t empire, uint8_t type, int value, uint8_t log, time_t end_time_sec);
        void give_character_priv(uint32_t pid, uint8_t type, int value, uint8_t log);

        auto priv(character *ch, uint8_t type) -> int;
        auto priv_by_empire(uint8_t empire, uint8_t type) -> int;
        auto priv_by_guild(uint32_t guild_id, uint8_t type) -> int;
        auto priv_by_character(uint32_t pid, uint8_t type) -> int;

      public:
        struct priv_empire_data {
            int value;
            time_t end_time_sec;
        };

        auto priv_by_empire_ex(uint8_t empire, uint8_t type) -> priv_empire_data *;

        struct priv_guild_data {
            int value;
            time_t end_time_sec;
        };

        auto priv_by_guild_ex(uint32_t guild_id, uint8_t type) const -> const priv_guild_data *;

      private:
        priv_empire_data aak_priv_empire_data_[MAX_PRIV_NUM][EMPIRE_MAX_NUM];
        std::map<uint32_t, priv_guild_data> priv_guild_[MAX_PRIV_NUM];
        std::map<uint32_t, int> priv_char_[MAX_PRIV_NUM];
    };

    inline auto get_priv_manager() -> priv_manager & {
        static priv_manager instance;
        return instance;
    }
}
