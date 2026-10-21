#pragma once

#include <metin2/game/app/typedef.h>

#include <cstdint>
#include <map>
#include <string>

namespace metin2::game::inline characters {

    class horse_name_manager {
      private:
        std::map<uint32_t, std::string> horse_names_;

        void broadcast_horse_name(uint32_t player_id_value, const char *horse_name_value);

      public:
        horse_name_manager();

        auto horse_name(uint32_t player_id) -> const char *;

        void update_horse_name(uint32_t player_id, const char *horse_name, bool broadcast = false);

        void validate(character *ch);
    };

    inline auto get_horse_name_manager() -> horse_name_manager & {
        static horse_name_manager instance;
        return instance;
    }
}
