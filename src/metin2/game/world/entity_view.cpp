
#include <metin2/config/config.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/world/entity.h>
#include <metin2/game/world/sectree.h>

#include <metin2/log/log.h>

#include <algorithm>

namespace metin2::game::inline world {

    namespace {
        // The view search covers the 3x3 sectrees around an entity, so a range beyond one sectree
        // would find entities in some directions only and make them flicker.
        auto effective_view_range() -> int {
            const int configured = metin2::config::game().view_range + VIEW_BONUS_RANGE;
            const int range = std::min(configured, static_cast<int>(SECTREE_SIZE));
            static bool warned = false;
            if (range < configured && !warned) {
                warned = true;
                metin2::log::warn("view_range {:d} + {:d} exceeds the sectree size {:d}; using {:d}", metin2::config::game().view_range, VIEW_BONUS_RANGE, static_cast<int>(SECTREE_SIZE), range);
            }
            return range;
        }
    }

    void entity::announce_to_players_around() {
        if (!get_sectree())
            return;

        get_sectree()->for_each_around([me = this, view_range = effective_view_range()](entity *ent) {
            if (!ent->is_type(ENTITY_CHARACTER) || !((character *)ent)->is_pc())
                return;
            if (distance_approx(ent->x() - me->x(), ent->y() - me->y()) > view_range)
                return;

            ent->view_insert(me);
        });
    }

    void entity::view_cleanup() {
        auto it = views_.begin();

        while (it != views_.end()) {
            entity *entity = it->first;
            ++it;

            entity->view_remove(this, false);
        }

        views_.clear();
    }

    void entity::view_reencode() {
        if (is_observer_)
            return;

        encode_remove_packet(this);
        encode_insert_packet(this);

        auto it = views_.begin();

        while (it != views_.end()) {
            entity *entity = (it++)->first;

            encode_remove_packet(entity);
            if (!is_observer_)
                encode_insert_packet(entity);

            if (!entity->is_observer_)
                entity->encode_insert_packet(this);
        }
    }

    void entity::view_insert(entity *entity, bool recursive) {
        if (this == entity)
            return;

        auto it = views_.find(entity);

        if (views_.end() != it) {
            it->second = view_age_;
            return;
        }

        views_.insert(entity_map::value_type(entity, view_age_));

        if (!entity->is_observer_)
            entity->encode_insert_packet(this);

        if (recursive)
            entity->view_insert(this, false);
    }

    void entity::view_remove(entity *entity, bool recursive) {
        auto it = views_.find(entity);

        if (it == views_.end())
            return;

        views_.erase(it);

        if (!entity->is_observer_)
            entity->encode_remove_packet(this);

        if (recursive)
            entity->view_remove(this, false);
    }

    void entity::update_sectree() {
        if (!get_sectree()) {
            if (is_type(ENTITY_CHARACTER)) {
                auto *tch = (character *)this;
                metin2::log::error("Character {} at {:d} {:d} has no sectree", tch->name(), x(), y());
            }

            return;
        }

        ++view_age_;

        get_sectree()->for_each_around([me = this, view_range = effective_view_range()](entity *ent) {
            if (!ent->is_type(ENTITY_OBJECT))
                if (distance_approx(ent->x() - me->x(), ent->y() - me->y()) > view_range)
                    return;

            me->view_insert(ent);

            if (ent->is_type(ENTITY_CHARACTER) && me->is_type(ENTITY_CHARACTER)) {
                auto *ch_me = (character *)me;
                auto *ch_ent = (character *)ent;

                if (ch_me->is_pc() && !ch_ent->is_pc() && !ch_ent->is_warp() && !ch_ent->is_goto())
                    ch_ent->start_state_machine();
            }
        });

        entity_map::iterator it, this_it;

        if (observer_mode_change_) {
            if (is_observer_) {
                it = views_.begin();

                while (it != views_.end()) {
                    this_it = it++;
                    if (this_it->second < view_age_) {
                        entity *ent = this_it->first;

                        ent->encode_remove_packet(this);
                        views_.erase(this_it);

                        ent->view_remove(this, false);
                    } else {

                        entity *ent = this_it->first;

                        encode_remove_packet(ent);
                    }
                }
            } else {
                it = views_.begin();

                while (it != views_.end()) {
                    this_it = it++;

                    if (this_it->second < view_age_) {
                        entity *ent = this_it->first;

                        ent->encode_remove_packet(this);
                        views_.erase(this_it);

                        ent->view_remove(this, false);
                    } else {
                        entity *ent = this_it->first;
                        ent->encode_insert_packet(this);
                        encode_insert_packet(ent);

                        ent->view_insert(this, true);
                    }
                }
            }

            observer_mode_change_ = false;
        } else {
            if (!is_observer_) {
                it = views_.begin();

                while (it != views_.end()) {
                    this_it = it++;

                    if (this_it->second < view_age_) {
                        entity *ent = this_it->first;

                        ent->encode_remove_packet(this);
                        views_.erase(this_it);

                        ent->view_remove(this, false);
                    }
                }
            }
        }
    }
}
