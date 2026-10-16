#include <metin2/config/config.h>
#include <metin2/game/items/item_manager.h>

#include <metin2/game/app/constants.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/empire/priv_manager.h>
#include <metin2/game/items/blend_item.h>
#include <metin2/game/items/dragon_soul.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager_private_types.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/trade/safebox.h>

#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/inventory.h>
#include <metin2/proto/item_types.h>
#include <metin2/proto/log_types.h>
#include <metin2/proto/mob_types.h>
#include <metin2/proto/tables.h>
#include <metin2/sql/query.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <format>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <strings.h>

namespace metin2::game::inline items {

    item_manager::item_manager()
        : vid_count_(0), current_id_(0) {
        item_id_range_.min = item_id_range_.max = item_id_range_.usable_item_id_min = 0;
        item_id_spare_range_.min = item_id_spare_range_.max = item_id_spare_range_.usable_item_id_min = 0;
    }

    item_manager::~item_manager() {
        destroy();
    }

    void item_manager::destroy() {
        auto it = vid_map_.begin();
        for (; it != vid_map_.end(); ++it) {
            delete it->second;
        }
        vid_map_.clear();
    }

    void item_manager::graceful_shutdown() {
        auto it = set_item_for_delayed_save_.begin();

        while (it != set_item_for_delayed_save_.end())
            save_single_item(*(it++));

        set_item_for_delayed_save_.clear();
    }

    auto item_manager::initialize(item_table *table_value, int size) -> bool {
        if (!vec_prototype_.empty())
            vec_prototype_.clear();

        int i;

        vec_prototype_.resize(size);
        std::memcpy(&vec_prototype_[0], table_value, sizeof(item_table) * size);
        for (int i = 0; i < size; i++) {
            if (0 != vec_prototype_[i].vnum_range) {
                vec_item_vnum_range_info_.push_back(&vec_prototype_[i]);
            }
        }

        map_item_refine_from_.clear();
        for (i = 0; i < size; ++i) {

            if (vec_prototype_[i].refined_vnum)
                map_item_refine_from_.insert(std::make_pair(vec_prototype_[i].refined_vnum, vec_prototype_[i].vnum));

            if (vec_prototype_[i].type == ITEM_QUEST || is_set(vec_prototype_[i].flags, ITEM_FLAG_QUEST_USE | ITEM_FLAG_QUEST_USE_MULTIPLE))
                quest::get_quest_manager().register_npc_vnum(vec_prototype_[i].vnum);

            map_vid.insert(std::map<uint32_t, item_table>::value_type(vec_prototype_[i].vnum, vec_prototype_[i]));
            if (metin2::config::test_server())
                metin2::log::debug("Item proto: vnum {:d} {}", vec_prototype_[i].vnum, vec_prototype_[i].name);
        }

        std::string buf;

        for (i = 0; i < size; ++i) {
            buf += std::format("{:5} {:<16}", vec_prototype_[i].vnum, vec_prototype_[i].locale_name);

            if (!((i + 1) % 4)) {
                if (!metin2::config::test_server())
                    metin2::log::debug("Item protos: {}", buf);
                buf.clear();
            } else {
                buf += '\t';
            }
        }

        if ((i + 1) % 4) {
            if (!metin2::config::test_server())
                metin2::log::debug("Item protos: {}", buf);
        }

        auto it = vid_map_.begin();

        metin2::log::debug("Item: refreshing {:d} live items", vid_map_.size());

        while (it != vid_map_.end()) {
            item *item = it->second;
            ++it;

            const item_table *table_info = table(item->original_vnum());

            if (nullptr == table_info) {
                metin2::log::error("No item proto for item (id {:d}, vnum {:d})", item->id(), item->original_vnum());
                item->set_proto(nullptr);
            }

            item->set_proto(table_info);
        }

        return true;
    }

    auto item_manager::create_item(uint32_t vnum, uint32_t count, uint32_t id, bool try_magic, int rare_pct, bool skip_save) -> item * {
        if (0 == vnum)
            return nullptr;

        uint32_t mask_vnum_value = 0;
        if (mask_vnum(vnum)) {
            mask_vnum_value = mask_vnum(vnum);
        }

        const item_table *table_value = table(vnum);

        if (nullptr == table_value)
            return nullptr;

        item *item = nullptr;

        if (map_item_by_id_.find(id) != map_item_by_id_.end()) {
            item = map_item_by_id_[id];
            character *owner = item->owner();
            metin2::log::error("Duplicate item id {:d} ({}, owner {})", id, item->name(), static_cast<const void *>(owner));
            return nullptr;
        }

        item = new game::items::item(vnum);

        bool is_new_item = (0 == id);

        item->initialize();
        item->set_proto(table_value);
        item->set_mask_vnum(mask_vnum_value);

        if (item->type() == ITEM_ELK)
            item->set_skip_save(true);

        else if (!is_new_item) {
            item->set_id(id);
            item->set_skip_save(true);
        } else {
            item->set_id(new_id());

            if (item->type() == ITEM_UNIQUE) {
                if (item->value(2) == 0)
                    item->set_socket(ITEM_SOCKET_UNIQUE_REMAIN_TIME, item->value(0));
                else {
                    item->set_socket(ITEM_SOCKET_UNIQUE_REMAIN_TIME, get_global_time() + item->value(0));
                }
            }
        }

        switch (item->vnum()) {
        case ITEM_AUTO_HP_RECOVERY_S:
        case ITEM_AUTO_HP_RECOVERY_M:
        case ITEM_AUTO_HP_RECOVERY_L:
        case ITEM_AUTO_HP_RECOVERY_X:
        case ITEM_AUTO_SP_RECOVERY_S:
        case ITEM_AUTO_SP_RECOVERY_M:
        case ITEM_AUTO_SP_RECOVERY_L:
        case ITEM_AUTO_SP_RECOVERY_X:
        case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_XS:
        case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_S:
        case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_XS:
        case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_S:
        case FUCKING_BRAZIL_ITEM_AUTO_SP_RECOVERY_S:
        case FUCKING_BRAZIL_ITEM_AUTO_HP_RECOVERY_S:
            if (is_new_item)
                item->set_socket(2, item->value(0), true);
            else
                item->set_socket(2, item->value(0), false);
            break;
        }

        if (item->type() == ITEM_ELK)
            ;
        else if (item->is_stackable()) {
            count = std::clamp<int>(count, 1, ITEM_MAX_COUNT);

            if (try_magic && count <= 1 && is_set(item->flag(), ITEM_FLAG_MAKECOUNT))
                count = item->value(1);
        } else
            count = 1;

        item->set_vid(++vid_count_);

        if (skip_save == false)
            vid_map_.insert(item_vid_map::value_type(item->get_vid(), item));

        if (item->id() != 0 && skip_save == false)
            map_item_by_id_.insert(std::map<uint32_t, game::items::item *>::value_type(item->id(), item));

        if (!item->set_count(count))
            return nullptr;

        item->set_skip_save(false);

        if (item->type() == ITEM_UNIQUE && item->value(2) != 0)
            item->start_unique_expire_event();

        for (int i = 0; i < ITEM_LIMIT_MAX_NUM; i++) {
            if (LIMIT_REAL_TIME == item->limit_type(i)) {
                if (item->limit_value(i)) {
                    item->set_socket(0, get_global_time() + item->limit_value(i));
                } else {
                    item->set_socket(0, get_global_time() + 60 * 60 * 24 * 7);
                }

                item->start_real_time_expire_event();
            }

            else if (LIMIT_TIMER_BASED_ON_WEAR == item->limit_type(i)) {
                if (true == item->is_equipped()) {
                    item->start_timer_based_on_wear_expire_event();
                } else if (0 == id) {
                    int32_t duration = item->get_socket(0);
                    if (0 == duration)
                        duration = item->limit_value(i);

                    if (0 == duration)
                        duration = 60 * 60 * 10;

                    item->set_socket(0, duration);
                }
            }
        }

        if (id == 0) {
            if (ITEM_BLEND == item->type()) {
                if (blend_item_find(item->vnum())) {
                    blend_item_set_value(item);
                    return item;
                }
            }

            if (table_value->s_addon_type) {
                item->apply_addon(table_value->s_addon_type);
            }

            if (try_magic) {
                if (rare_pct == -1)
                    rare_pct = table_value->alter_to_magic_item_pct;

                if (prob(rare_pct))
                    item->alter_to_magic_item();
            }

            if (table_value->gain_socket_pct)
                item->alter_to_socket_item(table_value->gain_socket_pct);

            if (vnum == 50300 || vnum == ITEM_SKILLFORGET_VNUM) {
                uint32_t skill_vnum;

                do {
                    skill_vnum = number(1, 111);

                    if (nullptr != get_skill_manager().get(skill_vnum))
                        break;
                } while (true);

                item->set_socket(0, skill_vnum);
            } else if (ITEM_SKILLFORGET2_VNUM == vnum) {
                uint32_t skill_vnum;

                do {
                    skill_vnum = number(112, 119);

                    if (nullptr != get_skill_manager().get(skill_vnum))
                        break;
                } while (true);

                item->set_socket(0, skill_vnum);
            }
        }

        if (item->type() == ITEM_QUEST) {
            for (auto it = map_quest_item_group_.begin(); it != map_quest_item_group_.end(); it++) {
                if (it->second->type == special_item_group::QUEST && it->second->contains(vnum)) {
                    item->set_sig_vnum(it->first);
                }
            }
        } else if (item->type() == ITEM_UNIQUE) {
            for (auto it = map_special_item_group_.begin(); it != map_special_item_group_.end(); it++) {
                if (it->second->type == special_item_group::SPECIAL && it->second->contains(vnum)) {
                    item->set_sig_vnum(it->first);
                }
            }
        }

        if (item->is_dragon_soul() && 0 == id) {
            get_ds_manager().dragon_soul_item_initialize(item);
        }
        return item;
    }

    void item_manager::delayed_save(item *item) {
        if (item->id() != 0)
            set_item_for_delayed_save_.insert(item);
    }

    void item_manager::flush_delayed_save(item *item) {
        auto it = set_item_for_delayed_save_.find(item);

        if (it == set_item_for_delayed_save_.end()) {
            return;
        }

        set_item_for_delayed_save_.erase(it);
        save_single_item(item);
    }

    void item_manager::save_single_item(item *item) {
        if (!item->owner()) {
            uint32_t id_value = item->id();
            uint32_t owner_id = item->last_owner_pid();

            db_clientdesc->db_packet_header(HEADER_GD_ITEM_DESTROY, 0, sizeof(uint32_t) + sizeof(uint32_t));
            db_clientdesc->packet(&id_value, sizeof(uint32_t));
            db_clientdesc->packet(&owner_id, sizeof(uint32_t));

            metin2::log::debug("Item: delete {} (id {:d})", item->name(), id_value);
            return;
        }

        metin2::log::debug("Item: save {} (id {:d}) of {} in window {:d}", item->name(), item->id(), item->owner()->name(), item->window());

        player_item t;

        t.id = item->id();
        t.window = item->window();
        t.pos = t.window == EQUIPMENT ? item->cell() - INVENTORY_MAX_NUM : item->cell();
        t.count = item->count();
        t.vnum = item->original_vnum();
        t.owner = (t.window == SAFEBOX || t.window == MALL) ? item->owner()->get_desc()->get_account_table().id : item->owner()->player_id();
        std::memcpy(t.sockets, item->sockets(), sizeof(t.sockets));
        std::memcpy(t.attr, item->attributes(), sizeof(t.attr));

        db_clientdesc->db_packet_header(HEADER_GD_ITEM_SAVE, 0, sizeof(player_item));
        db_clientdesc->packet(&t, sizeof(player_item));
    }

    void item_manager::update() {
        auto it = set_item_for_delayed_save_.begin();
        std::unordered_set<item *>::iterator this_it;

        while (it != set_item_for_delayed_save_.end()) {
            this_it = it++;
            item *item = *this_it;

            if (item->owner() && is_set(item->flag(), ITEM_FLAG_SLOW_QUERY))
                continue;

            save_single_item(item);

            set_item_for_delayed_save_.erase(this_it);
        }
    }

    void item_manager::remove_item(item *item, const char *reason) {
        character *o;

        if ((o = item->owner())) {
            const std::string hint = std::format("{} {} ", item->name(), item->count());
            get_log_manager().item_log(o, item, reason ? reason : "REMOVE", hint.c_str());

            if (item->window() == MALL || item->window() == SAFEBOX) {
                safebox *safebox = item->window() == MALL ? o->mall() : o->get_safebox();
                if (safebox) {
                    safebox->remove(item->cell());
                }
            } else {
                o->sync_quickslot(QUICKSLOT_TYPE_ITEM, item->cell(), 255);
                item->remove_from_character();
            }
        }

        get_item_manager().destroy_item(item);
    }

    void item_manager::destroy_item(item *item) {
        if (item->get_sectree())
            item->remove_from_ground();

        if (item->owner()) {
            if (get_character_manager().find(item->owner()->player_id()) != nullptr) {
                metin2::log::error("Destroying item {} still owned by player {}", item->name(), item->owner()->name());
                item->remove_from_character();
            } else {
                metin2::log::error("Destroying item {} with a stale owner {}", item->name(), static_cast<const void *>(item->owner()));
            }
        }

        auto it = set_item_for_delayed_save_.find(item);

        if (it != set_item_for_delayed_save_.end())
            set_item_for_delayed_save_.erase(it);

        uint32_t id_value = item->id();
        metin2::log::debug("Item: destroy {} (id {:d})", item->name(), id_value);

        if (!item->skip_save() && id_value) {
            uint32_t owner_id = item->last_owner_pid();

            db_clientdesc->db_packet_header(HEADER_GD_ITEM_DESTROY, 0, sizeof(uint32_t) + sizeof(uint32_t));
            db_clientdesc->packet(&id_value, sizeof(uint32_t));
            db_clientdesc->packet(&owner_id, sizeof(uint32_t));
        } else {
            metin2::log::debug("Item: destroy {} (id {:d}) without saving (skip {:d})", item->name(), id_value, item->skip_save());
        }

        if (id_value)
            map_item_by_id_.erase(id_value);

        vid_map_.erase(item->get_vid());

        delete item;
    }

    auto item_manager::find(uint32_t id) -> item * {
        auto it = map_item_by_id_.find(id);
        if (it == map_item_by_id_.end())
            return nullptr;
        return it->second;
    }

    auto item_manager::find_by_vid(uint32_t vid) -> item * {
        auto it = vid_map_.find(vid);

        if (it == vid_map_.end())
            return nullptr;

        return (it->second);
    }

    auto item_manager::table(uint32_t vnum) -> item_table * {
        int rnum = real_number(vnum);

        if (rnum < 0) {
            for (size_t i = 0; i < vec_item_vnum_range_info_.size(); i++) {
                item_table *p = vec_item_vnum_range_info_[i];
                if ((p->vnum < vnum) &&
                    vnum < (p->vnum + p->vnum_range)) {
                    return p;
                }
            }

            return nullptr;
        }

        return &vec_prototype_[rnum];
    }

    auto item_manager::real_number(uint32_t vnum) -> int {
        int bot, top, mid;

        bot = 0;
        top = vec_prototype_.size();

        item_table *table = &vec_prototype_[0];

        while (1) {
            mid = (bot + top) >> 1;

            if ((table + mid)->vnum == vnum)
                return (mid);

            if (bot >= top)
                return (-1);

            if ((table + mid)->vnum > vnum)
                top = mid - 1;
            else
                bot = mid + 1;
        }
    }

    auto item_manager::vnum(const char *name, uint32_t &r_dw_vnum) -> bool {
        int len = strlen(name);

        item_table *table = &vec_prototype_[0];

        for (uint32_t i = 0; i < vec_prototype_.size(); ++i, ++table) {
            if (!strncasecmp(name, table->locale_name, len)) {
                r_dw_vnum = table->vnum;
                return true;
            }
        }

        return false;
    }

    auto item_manager::vnum_by_original_name(const char *name_value, uint32_t &r_dw_vnum) -> bool {
        int len = strlen(name_value);

        item_table *table = &vec_prototype_[0];

        for (uint32_t i = 0; i < vec_prototype_.size(); ++i, ++table) {
            if (!strncasecmp(name_value, table->name, len)) {
                r_dw_vnum = table->vnum;
                return true;
            }
        }

        return false;
    }

    std::set<uint32_t> set_lotto;

    auto get_drop_per_kill_pct(int minimum, int fallback, int delta_percent, const char *flag) -> int {
        int val = 0;

        if ((val = quest::get_quest_manager().event_flag(flag))) {
            if (!metin2::config::test_server()) {
                if (val < minimum)
                    val = fallback;

                if (val < 0)
                    val = fallback;
            }
        }

        if (val == 0)
            return 0;

        return (40000 * delta_percent / val);
    }

    auto item_manager::drop_pct(character *chr, character *killer, int &delta_percent, int &rand_range) -> bool {
        if (nullptr == chr || nullptr == killer)
            return false;

        int level_value = killer->level();
        delta_percent = 100;

        if (!chr->is_stone() && chr->get_mob_rank() >= MOB_RANK_BOSS)
            delta_percent = percent_lvdelta_boss(killer->level(), chr->level());
        else
            delta_percent = percent_lvdelta(killer->level(), chr->level());

        uint8_t rank_value = chr->get_mob_rank();

        if (1 == number(1, 50000))
            delta_percent += 1000;
        else if (1 == number(1, 10000))
            delta_percent += 500;

        metin2::log::debug("Drop: level {:d}, rank {:d}, chance {:d}", level_value, rank_value, delta_percent);
        delta_percent = delta_percent * get_character_manager().mob_item_rate(killer) / 100;

        if (killer->premium_remain_seconds(PREMIUM_ITEM) > 0 ||
            killer->is_equip_unique_group(UNIQUE_GROUP_DOUBLE_ITEM))
            delta_percent += delta_percent;

        rand_range = 4000000;
        rand_range = rand_range * 100 /
                     (100 +
                         get_priv_manager().priv(killer, PRIV_ITEM_DROP) +
                         (killer->is_equip_unique_item(UNIQUE_ITEM_DOUBLE_ITEM) ? 100 : 0));

        if (metin2::config::game().distribution_test_server) rand_range /= 3;

        return true;
    }

    auto item_manager::create_drop_item(character *chr, character *killer, std::vector<item *> &vec_item) -> bool {
        int level = killer->level();

        int delta_percent, rand_range;
        if (!drop_pct(chr, killer, delta_percent, rand_range))
            return false;

        uint8_t rank = chr->get_mob_rank();
        item *item = nullptr;

        auto it = vec_common_drop_item[rank].begin();

        while (it != vec_common_drop_item[rank].end()) {
            const item_drop_info &c_r_info = *(it++);

            if (level < c_r_info.level_start || level > c_r_info.level_end)
                continue;

            int percent = (c_r_info.percent * delta_percent) / 100;
            metin2::log::debug("Drop: levels {:d}-{:d}, vnum {:d}, chance {:d} (base {:d})", c_r_info.level_start, c_r_info.level_end, c_r_info.vnum, percent, c_r_info.percent);

            if (percent >= number(1, rand_range)) {
                item_table *table_value = table(c_r_info.vnum);

                if (!table_value)
                    continue;

                item = nullptr;

                if (table_value->type == ITEM_POLYMORPH) {
                    if (c_r_info.vnum == chr->polymorph_item_vnum()) {
                        item = create_item(c_r_info.vnum, 1, 0, true);

                        if (item)
                            item->set_socket(0, chr->race_num());
                    }
                } else
                    item = create_item(c_r_info.vnum, 1, 0, true);

                if (item) vec_item.push_back(item);
            }
        }

        {
            decltype(map_drop_item_group_.begin()) it;
            it = map_drop_item_group_.find(chr->race_num());

            if (it != map_drop_item_group_.end()) {
                __typeof(it->second->get_vector()) v = it->second->get_vector();

                for (uint32_t i = 0; i < v.size(); ++i) {
                    int percent = (v[i].pct * delta_percent) / 100;

                    if (percent >= number(1, rand_range)) {
                        item = create_item(v[i].vnum, v[i].count, 0, true);

                        if (item) {
                            if (item->type() == ITEM_POLYMORPH) {
                                if (item->vnum() == chr->polymorph_item_vnum()) {
                                    item->set_socket(0, chr->race_num());
                                }
                            }

                            vec_item.push_back(item);
                        }
                    }
                }
            }
        }

        {
            decltype(map_mob_item_group_.begin()) it;
            it = map_mob_item_group_.find(chr->race_num());

            if (it != map_mob_item_group_.end()) {
                mob_item_group *group = it->second.get();

                if (group && !group->is_empty()) {
                    int percent = 40000 * delta_percent / group->kill_per_drop();
                    if (percent >= number(1, rand_range)) {
                        const mob_item_group::mob_item_group_info &info = group->one();
                        item = create_item(info.item_vnum, info.count, 0, true, info.rare_pct);

                        if (item) vec_item.push_back(item);
                    }
                }
            }
        }

        {
            decltype(map_level_item_group_.begin()) it;
            it = map_level_item_group_.find(chr->race_num());

            if (it != map_level_item_group_.end()) {
                if (it->second->level_limit() <= (uint32_t)level) {
                    __typeof(it->second->get_vector()) v = it->second->get_vector();

                    for (uint32_t i = 0; i < v.size(); i++) {
                        if (v[i].pct >= (uint32_t)number(1, 1000000)) {
                            uint32_t vnum = v[i].v_num;
                            item = create_item(vnum, v[i].count, 0, true);
                            if (item) vec_item.push_back(item);
                        }
                    }
                }
            }
        }

        {
            if (killer->premium_remain_seconds(PREMIUM_ITEM) > 0 ||
                killer->is_equip_unique_group(UNIQUE_GROUP_DOUBLE_ITEM)) {
                decltype(map_glove_item_group_.begin()) it;
                it = map_glove_item_group_.find(chr->race_num());

                if (it != map_glove_item_group_.end()) {
                    __typeof(it->second->get_vector()) v = it->second->get_vector();

                    for (uint32_t i = 0; i < v.size(); ++i) {
                        int percent = (v[i].pct * delta_percent) / 100;

                        if (percent >= number(1, rand_range)) {
                            uint32_t vnum = v[i].vnum;
                            item = create_item(vnum, v[i].count, 0, true);
                            if (item) vec_item.push_back(item);
                        }
                    }
                }
            }
        }

        if (chr->mob_drop_item_vnum()) {
            auto it = map_etc_item_drop_prob_.find(chr->mob_drop_item_vnum());

            if (it != map_etc_item_drop_prob_.end()) {
                int percent = (it->second * delta_percent) / 100;

                if (percent >= number(1, rand_range)) {
                    item = create_item(chr->mob_drop_item_vnum(), 1, 0, true);
                    if (item) vec_item.push_back(item);
                }
            }
        }

        if (chr->is_stone()) {
            if (chr->drop_metin_stone_vnum()) {
                int percent = (chr->drop_metin_stone_pct() * delta_percent) * 400;

                if (percent >= number(1, rand_range)) {
                    item = create_item(chr->drop_metin_stone_vnum(), 1, 0, true);
                    if (item) vec_item.push_back(item);
                }
            }
        }

        if (killer->is_horse_riding() &&
            get_drop_per_kill_pct(1000, 1000000, delta_percent, "horse_skill_book_drop") >= number(1, rand_range)) {
            metin2::log::debug("Drop event: horse skill book");

            if ((item = create_item(ITEM_HORSE_SKILL_TRAIN_BOOK, 1, 0, true)))
                vec_item.push_back(item);
        }

        if (get_drop_per_kill_pct(100, 1000, delta_percent, "lotto_drop") >= number(1, rand_range)) {
            get_db_manager().func_query([pid = killer->player_id(), round = static_cast<uint32_t>(quest::get_quest_manager().event_flag("lotto_round"))](sql_msg &msg) { give_lotto_ticket_result(msg, pid, 50001, 1, round); },
                sql::format("INSERT INTO lotto_list VALUES(0, 'server', {}, NOW())", killer->player_id()));
        }

        create_quest_drop_item(chr, killer, vec_item, delta_percent, rand_range);

        for (auto it = vec_item.begin(); it != vec_item.end(); ++it) {
            game::items::item *item = *it;
            get_db_manager().send_money_log(MONEY_LOG_DROP, item->vnum(), item->count());
        }

        return vec_item.size();
    }

    auto get_three_skill_level_adjust(int level) -> int {
        if (level < 40)
            return 32;
        if (level < 45)
            return 16;
        if (level < 50)
            return 8;
        if (level < 55)
            return 4;
        if (level < 60)
            return 2;
        return 1;
    }

    static struct drop_event_char_stone {
        int percent_lv01_10;
        int percent_lv11_30;
        int percent_lv31_mx;
        int level_range;
        bool alive;

        drop_event_char_stone() {
            percent_lv01_10 = 100;
            percent_lv11_30 = 200;
            percent_lv31_mx = 300;
            level_range = 10;
            alive = false;
        }
    } gs_drop_event_char_stone;

    static auto drop_event_char_stone_get_drop_percent(int killer_level) -> int {
        int killer_level_step = (killer_level - 1) / 10;

        switch (killer_level_step) {
        case 0:
            return gs_drop_event_char_stone.percent_lv01_10;

        case 1:
        case 2:
            return gs_drop_event_char_stone.percent_lv11_30;
        }

        return gs_drop_event_char_stone.percent_lv31_mx;
    }

    static void drop_event_char_stone_drop_item(character &killer, character &victim, item_manager &item_mgr, std::vector<item *> &vec_item) {
        if (!gs_drop_event_char_stone.alive)
            return;

        int killer_level = killer.level();
        int drop_percent = drop_event_char_stone_get_drop_percent(killer_level);

        int max_range = 10000;

        if (number(1, max_range) <= drop_percent) {
            int victim_level = victim.level();
            int level_diff = victim_level - killer_level;

            if (level_diff >= +gs_drop_event_char_stone.level_range || level_diff <= -gs_drop_event_char_stone.level_range) {
                metin2::log::debug(
                    "Drop event: no char stone, level gap too large (killer {} lv {:d}, victim {} lv {:d}, diff {:d})",
                    killer.name(), killer.level(), victim.name(), victim.level(), level_diff);
                return;
            }

            static const int stones[] = {30210, 30211, 30212, 30213, 30214, 30215, 30216, 30217, 30218, 30219, 30258, 30259, 30260, 30261, 30262, 30263};
            int item_vnum = stones[number(0, static_cast<int>(std::size(stones)) - 1)];

            item *p_item = nullptr;

            if ((p_item = item_mgr.create_item(item_vnum, 1, 0, true))) {
                vec_item.push_back(p_item);

                metin2::log::debug(
                    "Drop event: char stone (killer {} lv {:d}, victim {} lv {:d}, item {})",
                    killer.name(), killer.level(), victim.name(), victim.level(), p_item->name());
            }
        }
    }

    auto drop_event_char_stone_set_value(const std::string &name, int value) -> bool {
        if (name == "drop_char_stone") {
            gs_drop_event_char_stone.alive = value;

            if (value)
                metin2::log::info("Drop event: char stone on");
            else
                metin2::log::info("Drop event: char stone off");

        } else if (name == "drop_char_stone.percent_lv01_10")
            gs_drop_event_char_stone.percent_lv01_10 = value;
        else if (name == "drop_char_stone.percent_lv11_30")
            gs_drop_event_char_stone.percent_lv11_30 = value;
        else if (name == "drop_char_stone.percent_lv31_MX")
            gs_drop_event_char_stone.percent_lv31_mx = value;
        else if (name == "drop_char_stone.level_range")
            gs_drop_event_char_stone.level_range = value;
        else
            return false;

        metin2::log::info("Drop event: char stone active {:d}", gs_drop_event_char_stone.alive ? true : false);
        metin2::log::info("Drop event: char stone chance lv 1-10 {:f}%", gs_drop_event_char_stone.percent_lv01_10 / 100.0f);
        metin2::log::info("Drop event: char stone chance lv 11-30 {:f}%", gs_drop_event_char_stone.percent_lv11_30 / 100.0f);
        metin2::log::info("Drop event: char stone chance lv 31+ {:f}%", gs_drop_event_char_stone.percent_lv31_mx / 100.0f);
        metin2::log::info("Drop event: char stone level range {:d}", gs_drop_event_char_stone.level_range);

        return true;
    }

    static struct drop_event_refine_box {
        int percent_low;
        int low;
        int percent_mid;
        int mid;
        int percent_high;
        bool alive;

        drop_event_refine_box() {
            percent_low = 100;
            low = 20;
            percent_mid = 100;
            mid = 45;
            percent_high = 100;
            alive = false;
        }
    } gs_drop_event_refine_box;

    static auto drop_event_refine_box_get_drop_item(character &killer, character &victim, item_manager &item_mgr) -> item * {
        static const int lower_box[] = {50197, 50198, 50199};
        static const int lower_box_range = 3;
        static const int midder_box[] = {50203, 50204, 50205, 50206};
        static const int midder_box_range = 4;
        static const int higher_box[] = {50207, 50208, 50209, 50210, 50211};
        static const int higher_box_range = 5;

        if (victim.get_mob_rank() < MOB_RANK_KNIGHT)
            return nullptr;

        int killer_level = killer.level();

        if (killer_level <= gs_drop_event_refine_box.low) {
            if (number(1, gs_drop_event_refine_box.percent_low) == 1) {
                return item_mgr.create_item(lower_box[number(1, lower_box_range) - 1], 1, 0, true);
            }
        } else if (killer_level <= gs_drop_event_refine_box.mid) {
            if (number(1, gs_drop_event_refine_box.percent_mid) == 1) {
                return item_mgr.create_item(midder_box[number(1, midder_box_range) - 1], 1, 0, true);
            }
        } else {
            if (number(1, gs_drop_event_refine_box.percent_high) == 1) {
                return item_mgr.create_item(higher_box[number(1, higher_box_range) - 1], 1, 0, true);
            }
        }
        return nullptr;
    }

    static void drop_event_refine_box_drop_item(character &killer, character &victim, item_manager &item_mgr, std::vector<item *> &vec_item) {
        if (!gs_drop_event_refine_box.alive)
            return;

        item *p_item = drop_event_refine_box_get_drop_item(killer, victim, item_mgr);

        if (p_item) {
            vec_item.push_back(p_item);

            metin2::log::debug(
                "Drop event: refine box (killer {} lv {:d}, victim {} lv {:d}, item {})",
                killer.name(), killer.level(), victim.name(), victim.level(), p_item->name());
        }
    }

    auto drop_event_refine_box_set_value(const std::string &name, int value) -> bool {
        if (name == "refine_box_drop") {
            gs_drop_event_refine_box.alive = value;

            if (value)
                metin2::log::info("Drop event: refine box on");
            else
                metin2::log::info("Drop event: refine box off");

        } else if (name == "refine_box_low")
            gs_drop_event_refine_box.percent_low = value < 100 ? 100 : value;
        else if (name == "refine_box_mid")
            gs_drop_event_refine_box.percent_mid = value < 100 ? 100 : value;
        else if (name == "refine_box_high")
            gs_drop_event_refine_box.percent_high = value < 100 ? 100 : value;
        else
            return false;

        metin2::log::info("Drop event: refine box active {:d}", gs_drop_event_refine_box.alive ? true : false);
        metin2::log::info("Drop event: refine box low chance {:d}", gs_drop_event_refine_box.percent_low);
        metin2::log::info("Drop event: refine box mid chance {:d}", gs_drop_event_refine_box.percent_mid);
        metin2::log::info("Drop event: refine box high chance {:d}", gs_drop_event_refine_box.percent_high);

        return true;
    }

    void item_manager::create_quest_drop_item(character *chr, character *killer, std::vector<item *> &vec_item, int delta_percent, int rand_range) {
        item *item = nullptr;

        if (!chr)
            return;

        if (!killer)
            return;

        metin2::log::debug("Drop: quest drops for {} killed by {}", chr->name(), killer->name());

        drop_event_char_stone_drop_item(*killer, *chr, *this, vec_item);
        drop_event_refine_box_drop_item(*killer, *chr, *this, vec_item);

        if (quest::get_quest_manager().event_flag("xmas_sock")) {
            uint32_t sock_item_vnum = 0;
            sock_item_vnum = 50010;

            int drop_per_kill[MOB_RANK_MAX_NUM] =
                {
                    2000,
                    1000,
                    300,
                    50,
                    0,
                    0,
            };

            if (drop_per_kill[chr->get_mob_rank()] != 0) {
                int percent = 40000 * delta_percent / drop_per_kill[chr->get_mob_rank()];

                metin2::log::debug("Drop event: sock chance {:d} of {:d}", percent, rand_range);
                if (percent >= number(1, rand_range)) {
                    if ((item = create_item(sock_item_vnum, 1, 0, true)))
                        vec_item.push_back(item);
                }
            }
        }

        if (quest::get_quest_manager().event_flag("drop_moon")) {
            const uint32_t item_vnum = 50011;

            int drop_per_kill[MOB_RANK_MAX_NUM] =
                {
                    2000,
                    1000,
                    300,
                    50,
                    0,
                    0,
            };

            if (drop_per_kill[chr->get_mob_rank()]) {
                int percent = 40000 * delta_percent / drop_per_kill[chr->get_mob_rank()];

                if (percent >= number(1, rand_range)) {
                    if ((item = create_item(item_vnum, 1, 0, true)))
                        vec_item.push_back(item);
                }
            }
        }

        if (killer->level() >= 15 && abs(killer->level() - chr->level()) <= 5) {
            int pct = quest::get_quest_manager().event_flag("hc_drop");

            if (pct > 0) {
                const uint32_t item_vnum = 30178;

                if (prob(pct)) {
                    if ((item = create_item(item_vnum, 1, 0, true)))
                        vec_item.push_back(item);
                }
            }
        }

        if (get_drop_per_kill_pct(100, 2000, delta_percent, "2006_drop") >= number(1, rand_range)) {
            metin2::log::debug("Drop event: hexagonal box");

            const static uint32_t vnum = 50037;

            if ((item = create_item(vnum, 1, 0, true)))
                vec_item.push_back(item);
        }

        if (get_drop_per_kill_pct(100, 2000, delta_percent, "2007_drop") >= number(1, rand_range)) {
            metin2::log::debug("Drop event: hexagonal box");

            const static uint32_t vnum = 50043;

            if ((item = create_item(vnum, 1, 0, true)))
                vec_item.push_back(item);
        }

        if (get_drop_per_kill_pct(100, 1000, delta_percent, "newyear_fire") >= number(1, rand_range)) {
            const uint32_t item_vnum_fire = 50107;

            if ((item = create_item(item_vnum_fire, 1, 0, true)))
                vec_item.push_back(item);
        }

        if (get_drop_per_kill_pct(100, 500, delta_percent, "newyear_moon") >= number(1, rand_range)) {
            metin2::log::debug("Drop event: new year moon");

            const static uint32_t wonso_items[6] = {
                50016,
                50017,
                50018,
                50019,
                50019,
                50019,
            };
            uint32_t vnum = wonso_items[number(0, 5)];

            if ((item = create_item(vnum, 1, 0, true)))
                vec_item.push_back(item);
        }

        if (get_drop_per_kill_pct(1, 2000, delta_percent, "valentine_drop") >= number(1, rand_range)) {
            metin2::log::debug("Drop event: valentine");

            const static uint32_t valentine_items[2] = {50024, 50025};
            uint32_t vnum = valentine_items[number(0, 1)];

            if ((item = create_item(vnum, 1, 0, true)))
                vec_item.push_back(item);
        }

        if (get_drop_per_kill_pct(100, 2000, delta_percent, "icecream_drop") >= number(1, rand_range)) {
            const static uint32_t icecream = 50123;

            if ((item = create_item(icecream, 1, 0, true)))
                vec_item.push_back(item);
        }

        if ((killer->count_specify_item(53002) > 0) && (get_drop_per_kill_pct(50, 100, delta_percent, "new_xmas_event") >= number(1, rand_range))) {
            const static uint32_t xmas_sock = 50010;
            killer->auto_give_item(xmas_sock, 1);
        }

        if ((killer->count_specify_item(53007) > 0) && (get_drop_per_kill_pct(50, 100, delta_percent, "new_xmas_event") >= number(1, rand_range))) {
            const static uint32_t xmas_sock = 50010;
            killer->auto_give_item(xmas_sock, 1);
        }

        if (get_drop_per_kill_pct(100, 2000, delta_percent, "halloween_drop") >= number(1, rand_range)) {
            const static uint32_t halloween_item = 30321;

            if ((item = create_item(halloween_item, 1, 0, true)))
                vec_item.push_back(item);
        }

        if (get_drop_per_kill_pct(100, 2000, delta_percent, "ramadan_drop") >= number(1, rand_range)) {
            const static uint32_t ramadan_item = 30315;

            if ((item = create_item(ramadan_item, 1, 0, true)))
                vec_item.push_back(item);
        }

        if (get_drop_per_kill_pct(100, 2000, delta_percent, "easter_drop") >= number(1, rand_range)) {
            const static uint32_t easter_item_base = 50160;

            if ((item = create_item(easter_item_base + number(0, 19), 1, 0, true)))
                vec_item.push_back(item);
        }

        if (get_drop_per_kill_pct(100, 2000, delta_percent, "football_drop") >= number(1, rand_range)) {
            const static uint32_t football_item = 50096;

            if ((item = create_item(football_item, 1, 0, true)))
                vec_item.push_back(item);
        }

        if (get_drop_per_kill_pct(100, 2000, delta_percent, "whiteday_drop") >= number(1, rand_range)) {
            metin2::log::debug("Drop event: white day");
            const static uint32_t whiteday_items[2] = {ITEM_WHITEDAY_ROSE, ITEM_WHITEDAY_CANDY};
            uint32_t vnum = whiteday_items[number(0, 1)];

            if ((item = create_item(vnum, 1, 0, true)))
                vec_item.push_back(item);
        }

        if (killer->level() >= 50) {
            if (get_drop_per_kill_pct(100, 1000, delta_percent, "kids_day_drop_high") >= number(1, rand_range)) {
                uint32_t item_quiz_box = 50034;

                if ((item = create_item(item_quiz_box, 1, 0, true)))
                    vec_item.push_back(item);
            }
        } else {
            if (get_drop_per_kill_pct(100, 1000, delta_percent, "kids_day_drop") >= number(1, rand_range)) {
                uint32_t item_quiz_box = 50034;

                if ((item = create_item(item_quiz_box, 1, 0, true)))
                    vec_item.push_back(item);
            }
        }

        if (chr->level() >= 30 && get_drop_per_kill_pct(50, 100, delta_percent, "medal_part_drop") >= number(1, rand_range)) {
            const static uint32_t drop_items[] = {30265, 30266, 30267, 30268, 30269};
            int i = number(0, 4);
            item = create_item(drop_items[i]);
            if (item != nullptr)
                vec_item.push_back(item);
        }

        if (chr->level() >= 40 && chr->get_mob_rank() >= MOB_RANK_BOSS && get_drop_per_kill_pct(1, 1000, delta_percent, "three_skill_item") / get_three_skill_level_adjust(chr->level()) >= number(1, rand_range)) {
            const uint32_t item_vnum = 50513;

            if ((item = create_item(item_vnum, 1, 0, true)))
                vec_item.push_back(item);
        }

        if (get_drop_per_kill_pct(100, 1000, delta_percent, "dragon_boat_festival_drop") >= number(1, rand_range)) {
            const uint32_t item_seed = 50085;

            if ((item = create_item(item_seed, 1, 0, true)))
                vec_item.push_back(item);
        }

        if (killer->level() >= 15 && quest::get_quest_manager().event_flag("mars_drop")) {
            const uint32_t item_haniron = 70035;
            int drop_multiply[MOB_RANK_MAX_NUM] =
                {
                    50,
                    30,
                    5,
                    1,
                    0,
                    0,
            };

            if (drop_multiply[chr->get_mob_rank()] &&
                get_drop_per_kill_pct(1000, 1500, delta_percent, "mars_drop") >= number(1, rand_range) * drop_multiply[chr->get_mob_rank()]) {
                if ((item = create_item(item_haniron, 1, 0, true)))
                    vec_item.push_back(item);
            }
        }
    }

    auto item_manager::refine_from_vnum(uint32_t vnum) -> uint32_t {
        auto it = map_item_refine_from_.find(vnum);
        if (it != map_item_refine_from_.end())
            return it->second;
        return 0;
    }

    auto item_manager::get_special_item_group(uint32_t vnum) -> const special_item_group * {
        auto it = map_special_item_group_.find(vnum);
        if (it != map_special_item_group_.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    auto item_manager::get_special_attr_group(uint32_t vnum) -> const special_attr_group * {
        auto it = map_special_attr_group_.find(vnum);
        if (it != map_special_attr_group_.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    auto item_manager::mask_vnum(uint32_t vnum) -> uint32_t {
        auto it = map_new_to_ori_.find(vnum);
        if (it != map_new_to_ori_.end()) {
            return it->second;
        } else
            return 0;
    }

    void item_manager::copy_all_attr_to(item *old_item, item *new_item) {
        if (old_item->is_accessory_for_socket()) {
            for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i) {
                new_item->set_socket(i, old_item->get_socket(i));
            }
        } else {
            for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i) {
                if (!old_item->get_socket(i))
                    break;
                else
                    new_item->set_socket(i, 1);
            }

            int slot = 0;

            for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i) {
                int32_t socket = old_item->get_socket(i);
                const int item_broken_metin_vnum = 28960;
                if (socket > 2 && socket != item_broken_metin_vnum)
                    new_item->set_socket(slot++, socket);
            }
        }

        old_item->copy_attribute_to(new_item);
    }
}
