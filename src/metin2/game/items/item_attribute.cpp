#include <metin2/game/app/constants.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/desc.h>

#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/apply_types.h>
#include <metin2/proto/item_types.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

namespace metin2::game::inline items {

    const int MAX_NORM_ATTR_NUM = item_manager::max_norm_attr_num;
    const int MAX_RARE_ATTR_NUM = item_manager::max_rare_attr_num;

    auto item::attribute_set_index() -> int {
        if (type() == ITEM_WEAPON) {
            if (sub_type() == WEAPON_ARROW)
                return -1;

            return ATTRIBUTE_SET_WEAPON;
        }

        if (type() == ITEM_ARMOR || type() == ITEM_COSTUME) {
            switch (sub_type()) {
            case ARMOR_BODY:
                return ATTRIBUTE_SET_BODY;

            case ARMOR_WRIST:
                return ATTRIBUTE_SET_WRIST;

            case ARMOR_FOOTS:
                return ATTRIBUTE_SET_FOOTS;

            case ARMOR_NECK:
                return ATTRIBUTE_SET_NECK;

            case ARMOR_HEAD:
                return ATTRIBUTE_SET_HEAD;

            case ARMOR_SHIELD:
                return ATTRIBUTE_SET_SHIELD;

            case ARMOR_EAR:
                return ATTRIBUTE_SET_EAR;
            }
        }

        return -1;
    }

    auto item::has_attr(uint8_t apply) -> bool {
        for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
            if (proto_->applies[i].type == apply)
                return true;

        for (int i = 0; i < MAX_NORM_ATTR_NUM; ++i)
            if (attribute_type(i) == apply)
                return true;

        return false;
    }

    auto item::has_rare_attr(uint8_t apply) -> bool {
        for (int i = 0; i < MAX_RARE_ATTR_NUM; ++i)
            if (attribute_type(i + 5) == apply)
                return true;

        return false;
    }

    void item::add_attribute(uint8_t apply, short s_value) {
        if (has_attr(apply))
            return;

        int i = attribute_count();

        if (i >= MAX_NORM_ATTR_NUM)
            metin2::log::error("Item {} (id {:d}) has no free attribute slot", name(), id());
        else {
            if (s_value)
                set_attribute(i, apply, s_value);
        }
    }

    void item::add_attr(uint8_t apply, uint8_t level) {
        if (has_attr(apply))
            return;

        if (level <= 0)
            return;

        int i = attribute_count();

        if (i == MAX_NORM_ATTR_NUM)
            metin2::log::error("Item {} (id {:d}) has no free attribute slot", name(), id());
        else {
            const item_attr_table &r = map_item_attr[apply];
            int32_t lVal = r.values[std::min(4, level - 1)];

            if (lVal)
                set_attribute(i, apply, lVal);
        }
    }

    void item::put_attribute_with_level(uint8_t level) {
        int iAttributeSet = attribute_set_index();

        if (iAttributeSet < 0)
            return;

        if (level > ITEM_ATTRIBUTE_MAX_LEVEL)
            return;

        std::vector<int> avail;

        int total = 0;

        for (int i = 0; i < MAX_APPLY_NUM; ++i) {
            const item_attr_table &r = map_item_attr[i];

            if (r.max_level_by_set[iAttributeSet] && !has_attr(i)) {
                avail.push_back(i);
                total += r.prob;
            }
        }

        unsigned int prob = number(1, total);
        int attr_idx = APPLY_NONE;

        for (uint32_t i = 0; i < avail.size(); ++i) {
            const item_attr_table &r = map_item_attr[avail[i]];

            if (prob <= r.prob) {
                attr_idx = avail[i];
                break;
            }

            prob -= r.prob;
        }

        if (!attr_idx) {
            metin2::log::error("No attribute to put on item {} (id {:d}), attribute set {:d}, level {:d}", name(), id(), iAttributeSet, level);
            return;
        }

        const item_attr_table &r = map_item_attr[attr_idx];

        if (level > r.max_level_by_set[iAttributeSet])
            level = r.max_level_by_set[iAttributeSet];

        add_attr(attr_idx, level);
    }

    void item::put_attribute(const int *attr_percent_table) {
        int iAttrLevelPercent = number(1, 100);
        int i;

        for (i = 0; i < ITEM_ATTRIBUTE_MAX_LEVEL; ++i) {
            if (iAttrLevelPercent <= attr_percent_table[i])
                break;

            iAttrLevelPercent -= attr_percent_table[i];
        }

        put_attribute_with_level(i + 1);
    }

    void item::change_attribute(const int *change_prob) {
        int iAttributeCount = attribute_count();

        clear_attribute();

        if (iAttributeCount == 0)
            return;

        item_table const *pProto = proto();

        if (pProto && pProto->s_addon_type) {
            apply_addon(pProto->s_addon_type);
        }

        static const int tmpChangeProb[ITEM_ATTRIBUTE_MAX_LEVEL] =
            {
                0,
                10,
                40,
                35,
                15,
        };

        for (int i = attribute_count(); i < iAttributeCount; ++i) {
            if (change_prob == nullptr) {
                put_attribute(tmpChangeProb);
            } else {
                put_attribute(change_prob);
            }
        }
    }

    void item::add_attribute() {
        static const int aiItemAddAttributePercent[ITEM_ATTRIBUTE_MAX_LEVEL] =
            {
                40, 50, 10, 0, 0};

        if (attribute_count() < MAX_NORM_ATTR_NUM)
            put_attribute(aiItemAddAttributePercent);
    }

    void item::clear_attribute() {
        for (int i = 0; i < MAX_NORM_ATTR_NUM; ++i) {
            attr_[i].type = 0;
            attr_[i].s_value = 0;
        }
    }

    auto item::attribute_count() -> int {
        int i;

        for (i = 0; i < MAX_NORM_ATTR_NUM; ++i) {
            if (attribute_type(i) == 0)
                break;
        }

        return i;
    }

    auto item::find_attribute(uint8_t type) -> int {
        for (int i = 0; i < MAX_NORM_ATTR_NUM; ++i) {
            if (attribute_type(i) == type)
                return i;
        }

        return -1;
    }

    auto item::remove_attribute_type(uint8_t type) -> bool {
        int index = find_attribute(type);
        return index != -1 && remove_attribute_type(index);
    }

    void item::set_attributes(const player_item_attribute *attribute) {
        std::memcpy(attr_, attribute, sizeof(attr_));
        save();
    }

    void item::set_attribute(int i, uint8_t type_value, short s_value_value) {
        assert(i < MAX_NORM_ATTR_NUM);

        attr_[i].type = type_value;
        attr_[i].s_value = s_value_value;
        update_packet();
        save();

        if (type_value) {
            const char *pszIP = nullptr;

            if (owner() && owner()->get_desc())
                pszIP = owner()->get_desc()->host_name();

            log_manager::instance().item_log(i, type_value, s_value_value, id(), "SET_ATTR", "", pszIP ? pszIP : "", original_vnum());
        }
    }

    void item::set_force_attribute(int i, uint8_t type_value, short s_value_value) {
        assert(i < ITEM_ATTRIBUTE_MAX_NUM);

        attr_[i].type = type_value;
        attr_[i].s_value = s_value_value;
        update_packet();
        save();

        if (type_value) {
            const char *pszIP = nullptr;

            if (owner() && owner()->get_desc())
                pszIP = owner()->get_desc()->host_name();

            log_manager::instance().item_log(i, type_value, s_value_value, id(), "SET_FORCE_ATTR", "", pszIP ? pszIP : "", original_vnum());
        }
    }

    void item::copy_attribute_to(item *item_value) {
        item_value->set_attributes(attr_);
    }

    auto item::rare_attr_count() -> int {
        int ret = 0;

        if (attr_[5].type != 0)
            ret++;

        if (attr_[6].type != 0)
            ret++;

        return ret;
    }

    auto item::change_rare_attribute() -> bool {
        if (rare_attr_count() == 0)
            return false;

        int cnt = rare_attr_count();

        for (int i = 0; i < cnt; ++i) {
            attr_[i + 5].type = 0;
            attr_[i + 5].s_value = 0;
        }

        if (owner() && owner()->get_desc())
            log_manager::instance().item_log(owner(), this, "SET_RARE_CHANGE", "");
        else
            log_manager::instance().item_log(0, 0, 0, id(), "SET_RARE_CHANGE", "", "", original_vnum());

        for (int i = 0; i < cnt; ++i) {
            add_rare_attribute();
        }

        return true;
    }

    auto item::add_rare_attribute() -> bool {
        int count = rare_attr_count();

        if (count >= 2)
            return false;

        int pos = count + 5;
        player_item_attribute &attr = attr_[pos];

        int nAttrSet = attribute_set_index();
        std::vector<int> avail;

        for (int i = 0; i < MAX_APPLY_NUM; ++i) {
            const item_attr_table &r = map_item_rare[i];

            if (r.apply_index != 0 && r.max_level_by_set[nAttrSet] > 0 && has_rare_attr(i) != true) {
                avail.push_back(i);
            }
        }

        const item_attr_table &r = map_item_rare[avail[number(0, avail.size() - 1)]];
        int nAttrLevel = 5;

        if (nAttrLevel > r.max_level_by_set[nAttrSet])
            nAttrLevel = r.max_level_by_set[nAttrSet];

        attr.type = r.apply_index;
        attr.s_value = r.values[nAttrLevel - 1];

        update_packet();

        save();

        const char *pszIP = nullptr;

        if (owner() && owner()->get_desc())
            pszIP = owner()->get_desc()->host_name();

        log_manager::instance().item_log(pos, attr.type, attr.s_value, id(), "SET_RARE", "", pszIP ? pszIP : "", original_vnum());
        return true;
    }
}
