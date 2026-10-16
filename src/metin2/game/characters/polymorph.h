
#pragma once

#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>

#include <metin2/common/singleton.h>

#include <cstdint>
#include <unordered_map>

namespace metin2::game::inline characters {

    inline constexpr int POLYMORPH_SKILL_ID = 129;
    inline constexpr int POLYMORPH_BOOK_ID = 50322;
    enum polymorph_bonus_type {
        POLYMORPH_NO_BONUS,
        POLYMORPH_ATK_BONUS,
        POLYMORPH_DEF_BONUS,
        POLYMORPH_SPD_BONUS,
    };

    class polymorph_utils : public singleton<polymorph_utils> {
      private:
        std::unordered_map<uint32_t, uint32_t> spd_type_;
        std::unordered_map<uint32_t, uint32_t> atk_type_;
        std::unordered_map<uint32_t, uint32_t> def_type_;

      public:
        polymorph_utils();

        auto bonus_type(uint32_t vnum) -> polymorph_bonus_type;

        auto polymorph_character(character *ch, item *item_value, const mob *mob_value) -> bool;
        auto update_book_practice_grade(character *ch, item *item_value) -> bool;
        auto give_book(character *ch, uint32_t mob_vnum, uint32_t practice_count, uint8_t book_level, uint8_t level_limit) -> bool;
        auto book_upgrade(character *ch, item *item_value) -> bool;
    };
}
