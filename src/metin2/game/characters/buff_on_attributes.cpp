#include <metin2/game/characters/buff_on_attributes.h>

#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/items/item.h>

#include <metin2/log/log.h>
#include <metin2/proto/inventory.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace metin2::game::inline characters {

    buff_on_attributes::buff_on_attributes(character *owner, uint8_t point_type, std::vector<uint8_t> *p_vec_buff_wear_targets)
        : buff_owner_(owner), point_type_(point_type), p_vec_buff_wear_targets_(p_vec_buff_wear_targets) {
        initialize();
    }

    buff_on_attributes::~buff_on_attributes() {
        off();
    }

    void buff_on_attributes::initialize() {
        buff_value_ = 0;
        map_additional_attrs_.clear();
    }

    void buff_on_attributes::remove_buff_from_item(item *item_value) {
        if (0 == buff_value_)
            return;
        if (nullptr != item_value) {
            if (item_value->cell() < INVENTORY_MAX_NUM)
                return;
            auto it = find(p_vec_buff_wear_targets_->begin(), p_vec_buff_wear_targets_->end(), item_value->cell() - INVENTORY_MAX_NUM);
            if (p_vec_buff_wear_targets_->end() == it)
                return;

            int m = item_value->attribute_count();
            for (int j = 0; j < m; j++) {
                player_item_attribute attr = item_value->get_attribute(j);
                auto it = map_additional_attrs_.find(attr.type);
                if (it != map_additional_attrs_.end()) {
                    int &sum_of_attr_value = it->second;
                    int old_value = sum_of_attr_value * buff_value_ / 100;
                    int new_value = (sum_of_attr_value - attr.s_value) * buff_value_ / 100;
                    buff_owner_->apply_point(attr.type, new_value - old_value);
                    sum_of_attr_value -= attr.s_value;
                } else {
                    metin2::log::error("Buff for point type {:d}: attribute of item {:d} (type {:d}) is not in the buff pool", point_type_, item_value->vnum(), attr.type);
                    return;
                }
            }
        }
    }

    void buff_on_attributes::add_buff_from_item(item *item_value) {
        if (0 == buff_value_)
            return;
        if (nullptr != item_value) {
            if (item_value->cell() < INVENTORY_MAX_NUM)
                return;
            auto it = find(p_vec_buff_wear_targets_->begin(), p_vec_buff_wear_targets_->end(), item_value->cell() - INVENTORY_MAX_NUM);
            if (p_vec_buff_wear_targets_->end() == it)
                return;

            int m = item_value->attribute_count();
            for (int j = 0; j < m; j++) {
                player_item_attribute attr = item_value->get_attribute(j);
                auto it = map_additional_attrs_.find(attr.type);

                if (it == map_additional_attrs_.end()) {
                    buff_owner_->apply_point(attr.type, attr.s_value * buff_value_ / 100);
                    map_additional_attrs_.insert(map_attr::value_type(attr.type, attr.s_value));
                } else {
                    int &sum_of_attr_value = it->second;
                    int old_value = sum_of_attr_value * buff_value_ / 100;
                    int new_value = (sum_of_attr_value + attr.s_value) * buff_value_ / 100;
                    buff_owner_->apply_point(attr.type, new_value - old_value);
                    sum_of_attr_value += attr.s_value;
                }
            }
        }
    }

    void buff_on_attributes::change_buff_value(uint8_t new_value) {
        if (0 == buff_value_)
            on(new_value);
        else if (0 == new_value)
            off();
        else {
            for (auto it = map_additional_attrs_.begin(); it != map_additional_attrs_.end(); it++) {
                int &sum_of_attr_value = it->second;

                buff_owner_->apply_point(it->first, -sum_of_attr_value * buff_value_ / 100);
            }
            buff_value_ = new_value;
        }
    }

    void buff_on_attributes::give_all_attributes() {
        if (0 == buff_value_)
            return;
        for (auto it = map_additional_attrs_.begin(); it != map_additional_attrs_.end(); it++) {
            uint8_t apply_type = it->first;
            int apply_value = it->second * buff_value_ / 100;

            buff_owner_->apply_point(apply_type, apply_value);
        }
    }

    auto buff_on_attributes::on(uint8_t value) -> bool {
        if (0 != buff_value_ || 0 == value)
            return false;

        int n = p_vec_buff_wear_targets_->size();
        map_additional_attrs_.clear();
        for (int i = 0; i < n; i++) {
            item *pItem = buff_owner_->wear(p_vec_buff_wear_targets_->at(i));
            if (nullptr != pItem) {
                int m = pItem->attribute_count();
                for (int j = 0; j < m; j++) {
                    player_item_attribute attr = pItem->get_attribute(j);
                    auto it = map_additional_attrs_.find(attr.type);
                    if (it != map_additional_attrs_.end()) {
                        it->second += attr.s_value;
                    } else {
                        map_additional_attrs_.insert(map_attr::value_type(attr.type, attr.s_value));
                    }
                }
            }
        }

        for (auto it = map_additional_attrs_.begin(); it != map_additional_attrs_.end(); it++) {
            buff_owner_->apply_point(it->first, it->second * value / 100);
        }

        buff_value_ = value;

        return true;
    }

    void buff_on_attributes::off() {
        if (0 == buff_value_)
            return;

        for (auto it = map_additional_attrs_.begin(); it != map_additional_attrs_.end(); it++) {
            buff_owner_->apply_point(it->first, -it->second * buff_value_ / 100);
        }
        initialize();
    }
}
