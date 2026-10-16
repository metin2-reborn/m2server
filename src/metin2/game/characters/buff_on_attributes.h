#pragma once

#include <metin2/game/app/typedef.h>

#include <cstdint>
#include <map>
#include <vector>

namespace metin2::game::inline characters {

    class character;

    class buff_on_attributes {
      public:
        buff_on_attributes(character *owner, uint8_t point_type, std::vector<uint8_t> *p_vec_buff_wear_targets);
        ~buff_on_attributes();

        void remove_buff_from_item(item *item_value);
        void add_buff_from_item(item *item_value);
        void change_buff_value(uint8_t new_value);
        void give_all_attributes();

        auto on(uint8_t value) -> bool;
        void off();

        void initialize();

      private:
        character *buff_owner_;
        uint8_t point_type_;
        uint8_t buff_value_;
        std::vector<uint8_t> *p_vec_buff_wear_targets_;

        typedef std::map<uint8_t, int> map_attr;
        map_attr map_additional_attrs_;
    };
}
