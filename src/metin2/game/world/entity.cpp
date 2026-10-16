#include <metin2/game/world/entity.h>

#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/protocol/desc.h>

#include <metin2/proto/chat_types.h>

#include <algorithm>
#include <cassert>
#include <utility>

namespace metin2::game::inline world {

    entity::entity() {
        initialize();
    }

    entity::~entity() {
        if (!is_destroyed_)
            assert(!"You must call CEntity::destroy() method in your derived class destructor");
    }

    void entity::initialize(int type) {
        is_destroyed_ = false;

        type_ = type;
        view_age_ = 0;
        pos_.x = pos_.y = pos_.z = 0;
        map_view_.clear();

        sectree_ = nullptr;
        desc_ = nullptr;
        map_index_ = 0;
        is_observer_ = false;
        observer_mode_change_ = false;
    }

    void entity::destroy() {
        if (is_destroyed_) {
            return;
        }
        view_cleanup();
        is_destroyed_ = true;
    }

    auto entity::type() const -> int {
        return type_;
    }

    auto entity::is_type(int type) const -> bool {
        return (type_ == type ? true : false);
    }

    void entity::packet_around(const void *data, int bytes, entity *except) {
        packet_view(data, bytes, except);
    }

    void entity::packet_view(const void *data, int bytes, entity *except) {
        if (!get_sectree())
            return;

        auto send = [data, bytes, except](entity *ent) {
            if (ent == except)
                return;

            if (ent->get_desc())
                ent->get_desc()->packet(data, bytes);
        };

        if (!is_observer_)
            for (const auto &[ent, age] : map_view_)
                send(ent);

        send(this);
    }

    void entity::set_observer_mode(bool flag) {
        if (is_observer_ == flag)
            return;

        is_observer_ = flag;
        observer_mode_change_ = true;
        update_sectree();

        if (is_type(ENTITY_CHARACTER)) {
            auto *ch = (character *)this;
            ch->chat_packet(CHAT_TYPE_COMMAND, "ObserverMode %d", is_observer_ ? 1 : 0);
        }
    }
}
