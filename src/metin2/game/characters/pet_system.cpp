#include <metin2/game/characters/pet_system.h>

#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/world/vector.h>

#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/apply_types.h>
#include <metin2/proto/item_types.h>
#include <metin2/proto/tables.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace metin2::game::inline characters {

    EVENTINFO(petsystem_event_info) {
        game::characters::pet_system *pet_system;
    };

    EVENTFUNC(petsystem_update_event) {
        auto *info = dynamic_cast<petsystem_event_info *>(event->info.get());
        if (info == nullptr) {
            metin2::log::error("Pet speed hack check event fired without event info");
            return 0;
        }

        pet_system *pet_system = info->pet_system;

        if (nullptr == pet_system)
            return 0;

        pet_system->update(0);
        return passes_per_sec(1) / 4;
    }

    pet_actor::pet_actor(character *owner, uint32_t vnum, uint32_t options) {
        vnum_ = vnum;
        vid_ = 0;
        options_ = options;

        character_ = nullptr;
        owner_ = owner;

        original_move_speed_ = 0;

        summon_item_vid_ = 0;
        summon_item_vnum_ = 0;
    }

    pet_actor::~pet_actor() {
        this->unsummon();

        owner_ = nullptr;
    }

    void pet_actor::set_name(const char *name) {
        std::string pet_name = owner_->name();

        if (nullptr != owner_ &&
            nullptr == name &&
            nullptr != owner_->name()) {
            pet_name += "'s Pet";
        } else
            pet_name += name;

        if (true == is_summoned())
            character_->set_name(pet_name);

        name_ = pet_name;
    }

    void pet_actor::unsummon() {
        if (true == this->is_summoned()) {
            this->clear_buff();
            this->set_summon_item(nullptr);
            if (nullptr != owner_)
                owner_->compute_points();

            if (nullptr != character_)
                get_character_manager().destroy_character(character_);

            character_ = nullptr;
            vid_ = 0;
        }
    }

    auto pet_actor::summon(const char *pet_name, item *summon_item, bool spawn_far) -> uint32_t {
        int32_t x = owner_->x();
        int32_t y = owner_->y();
        int32_t z = owner_->z();

        if (true == spawn_far) {
            x += (number(0, 1) * 2 - 1) * number(2000, 2500);
            y += (number(0, 1) * 2 - 1) * number(2000, 2500);
        } else {
            x += number(-100, 100);
            y += number(-100, 100);
        }

        if (nullptr != character_) {
            character_->show(owner_->map_index(), x, y);
            vid_ = character_->get_vid();

            return vid_;
        }

        character_ = get_character_manager().spawn_mob(
            vnum_,
            owner_->map_index(),
            x, y, z,
            false, (int)(owner_->rotation() + 180), false);

        if (nullptr == character_) {
            metin2::log::error("Cannot summon pet (vnum {:d})", vnum_);
            return 0;
        }

        character_->set_pet();

        character_->set_empire(owner_->empire());

        vid_ = character_->get_vid();

        this->set_name(pet_name);

        this->set_summon_item(summon_item);
        owner_->compute_points();
        character_->show(owner_->map_index(), x, y, z);

        return vid_;
    }

    auto pet_actor::update_follow_ai() -> bool {
        if (nullptr == character_->mob_data) {
            return false;
        }

        if (0 == original_move_speed_) {
            const mob *mob_data = get_mob_manager().get(vnum_);

            if (nullptr != mob_data)
                original_move_speed_ = mob_data->table.s_moving_speed;
        }
        float start_follow_distance = 300.0f;
        float start_run_distance = 900.0f;

        float respawn_distance = 4500.f;
        int approach = 200;

        bool run = false;

        uint32_t current_time = get_dword_time();

        int32_t owner_x = owner_->x();
        int32_t owner_y = owner_->y();
        int32_t char_x = character_->x();
        int32_t char_y = character_->y();

        float dist = distance_approx(char_x - owner_x, char_y - owner_y);

        if (dist >= respawn_distance) {
            float owner_rot = owner_->rotation() * 3.141592f / 180.f;
            float fx = -approach * cos(owner_rot);
            float fy = -approach * sin(owner_rot);
            if (character_->show(owner_->map_index(), owner_x + fx, owner_y + fy)) {
                return true;
            }
        }

        if (dist >= start_follow_distance) {
            if (dist >= start_run_distance) {
                run = true;
            }

            character_->set_now_walking(!run);

            follow(approach);

            character_->set_last_attacked(current_time);
        } else
            character_->send_move_packet(FUNC_WAIT, 0, 0, 0, 0);

        return true;
    }

    auto pet_actor::update(uint32_t delta_time) -> bool {
        bool result = true;

        if (owner_->is_dead() || (is_summoned() && character_->is_dead()) || nullptr == get_item_manager().find_by_vid(this->summon_item_vid()) || get_item_manager().find_by_vid(this->summon_item_vid())->owner() != this->owner()) {
            this->unsummon();
            return true;
        }

        if (this->is_summoned() && has_option(EPetOption_Followable))
            result = result && this->update_follow_ai();

        return result;
    }

    auto pet_actor::follow(float min_distance) -> bool {
        if (!owner_ || !character_)
            return false;

        float owner_x = owner_->x();
        float owner_y = owner_->y();

        float pet_x = character_->x();
        float pet_y = character_->y();

        float dist = distance_sqrt(owner_x - pet_x, owner_y - pet_y);
        if (dist <= min_distance)
            return false;

        character_->set_rotation_to_xy(owner_x, owner_y);

        float fx, fy;

        float dist_to_go = dist - min_distance;
        get_delta_by_degree(character_->rotation(), dist_to_go, &fx, &fy);

        if (!character_->go_to((int)(pet_x + fx + 0.5f), (int)(pet_y + fy + 0.5f)))
            return false;

        character_->send_move_packet(FUNC_WAIT, 0, 0, 0, 0, 0);

        return true;
    }

    void pet_actor::set_summon_item(item *item_value) {
        if (nullptr == item_value) {
            summon_item_vid_ = 0;
            summon_item_vnum_ = 0;
            return;
        }

        summon_item_vid_ = item_value->get_vid();
        summon_item_vnum_ = item_value->vnum();
    }

    void pet_actor::give_buff() {
        if (34004 == vnum_ || 34009 == vnum_) {
            if (nullptr == owner_->get_dungeon()) {
                return;
            }
        }
        item *item = get_item_manager().find_by_vid(summon_item_vid_);
        if (nullptr != item)
            item->modify_points(true);
        return;
    }

    void pet_actor::clear_buff() {
        if (nullptr == owner_)
            return;
        item_table *item_proto = get_item_manager().table(summon_item_vnum_);
        if (nullptr == item_proto)
            return;
        for (int i = 0; i < ITEM_APPLY_MAX_NUM; i++) {
            if (item_proto->applies[i].type == APPLY_NONE)
                continue;
            owner_->apply_point(item_proto->applies[i].type, -item_proto->applies[i].value);
        }

        return;
    }

    pet_system::pet_system(character *owner) {

        owner_ = owner;
        update_period_ = 400;

        last_update_time_ = 0;
    }

    pet_system::~pet_system() {
        destroy();
    }

    void pet_system::destroy() {
        pet_actor_map actors;
        actors.swap(pet_actor_map_);
        actors.clear();
        event_cancel(&pet_system_update_event_);
    }

    auto pet_system::update(uint32_t delta_time) -> bool {
        bool result = true;

        uint32_t current_time = get_dword_time();

        if (update_period_ > current_time - last_update_time_)
            return true;

        std::vector<pet_actor *> v_garbage_actor;

        for (auto iter = pet_actor_map_.begin(); iter != pet_actor_map_.end(); ++iter) {
            pet_actor *pet_actor = iter->second.get();

            if (nullptr != pet_actor && pet_actor->is_summoned()) {
                character *pet = pet_actor->get_character();

                if (nullptr == get_character_manager().find(pet->get_vid())) {
                    v_garbage_actor.push_back(pet_actor);
                } else {
                    result = result && pet_actor->update(delta_time);
                }
            }
        }
        for (auto it = v_garbage_actor.begin(); it != v_garbage_actor.end(); it++)
            delete_pet(*it);

        last_update_time_ = current_time;

        return result;
    }

    void pet_system::delete_pet(pet_actor *pet_actor_value) {
        if (std::erase_if(pet_actor_map_, [pet_actor_value](const auto &entry) { return entry.second.get() == pet_actor_value; }))
            return;

        metin2::log::error("Pet actor {} not found in pet list (size {:d})", static_cast<const void *>(static_cast<void *>(pet_actor_value)), pet_actor_map_.size());
    }

    void pet_system::unsummon(uint32_t vnum, bool delete_from_list) {
        pet_actor *actor = this->by_vnum(vnum);

        if (nullptr == actor) {
            metin2::log::error("Pet actor for vnum {:d} is null", vnum);
            return;
        }
        actor->unsummon();

        if (true == delete_from_list)
            this->delete_pet(actor);

        bool active = false;
        for (auto it = pet_actor_map_.begin(); it != pet_actor_map_.end(); it++) {
            active |= it->second->is_summoned();
        }
        if (false == active) {
            event_cancel(&pet_system_update_event_);
            pet_system_update_event_ = nullptr;
        }
    }

    auto pet_system::summon(uint32_t mob_vnum, item *summon_item, const char *pet_name, bool spawn_far, uint32_t options) -> pet_actor * {
        pet_actor *pet_actor_value = this->by_vnum(mob_vnum);

        if (nullptr == pet_actor_value) {
            auto pet_actor_owner = std::unique_ptr<pet_actor>(new pet_actor(owner_, mob_vnum, options));
            pet_actor_value = pet_actor_owner.get();
            pet_actor_map_.try_emplace(mob_vnum, std::move(pet_actor_owner));
        }

        pet_actor_value->summon(pet_name, summon_item, spawn_far);

        if (nullptr == pet_system_update_event_) {
            auto *info = alloc_event_info<petsystem_event_info>();

            info->pet_system = this;

            pet_system_update_event_ = event_create(petsystem_update_event, info, passes_per_sec(1) / 4);
        }

        return pet_actor_value;
    }

    auto pet_system::by_vnum(uint32_t vnum) const -> pet_actor * {
        pet_actor *pet_actor = nullptr;

        auto iter = pet_actor_map_.find(vnum);

        if (pet_actor_map_.end() != iter)
            pet_actor = iter->second.get();

        return pet_actor;
    }

    auto pet_system::count_summoned() const -> size_t {
        size_t count = 0;

        for (auto iter = pet_actor_map_.begin(); iter != pet_actor_map_.end(); ++iter) {
            pet_actor *pet_actor = iter->second.get();

            if (nullptr != pet_actor) {
                if (pet_actor->is_summoned())
                    ++count;
            }
        }

        return count;
    }

    void pet_system::refresh_buff() {
        for (auto iter = pet_actor_map_.begin(); iter != pet_actor_map_.end(); ++iter) {
            pet_actor *pet_actor = iter->second.get();

            if (nullptr != pet_actor) {
                if (pet_actor->is_summoned()) {
                    pet_actor->give_buff();
                }
            }
        }
    }
}
