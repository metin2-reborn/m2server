#include <metin2/config/config.h>
#include <metin2/game/quest/target.h>

#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>

#include <metin2/log/log.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <list>

namespace metin2::game {

    void send_target_create_packet(desc *d, target_info *info) {
        if (!info->send_to_client)
            return;

        packet_gc_target_create pck;

        pck.header = HEADER_GC_TARGET_CREATE;
        pck.id = info->id;
        pck.type = info->type;
        pck.vid = info->arg1;
        strlcpy(pck.name, info->target_desc, sizeof(pck.name));
        d->packet(&pck, sizeof(packet_gc_target_create));
    }

    void send_target_update_packet(desc *d, int id_value, int x, int y) {
        packet_gc_target_update pck;
        pck.header = HEADER_GC_TARGET_UPDATE;
        pck.id = id_value;
        pck.x = x;
        pck.y = y;
        d->packet(&pck, sizeof(packet_gc_target_update));
        metin2::log::debug("Target: update {:d} at ({:d}, {:d})", id_value, x, y);
    }

    void send_target_delete_packet(desc *d, int id_value) {
        packet_gc_target_delete pck;
        pck.header = HEADER_GC_TARGET_DELETE;
        pck.id = id_value;
        d->packet(&pck, sizeof(packet_gc_target_delete));
    }
    target_manager::target_manager() : id_(0) {
    }

    target_manager::~target_manager() = default;

    EVENTFUNC(target_event) {
        auto *info = dynamic_cast<target_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Target event has no info");
            return 0;
        }

        character *chr = character_manager::instance().find_by_pid(info->pid);
        if (chr == nullptr) {
            return 0;
        }
        character *tch = nullptr;
        int x = 0, y = 0;
        int dist = 5000;

        if (info->map_index != chr->map_index())
            return std::clamp(dist / (1500 / metin2::config::server().passes_per_sec), metin2::config::server().passes_per_sec / 2, metin2::config::server().passes_per_sec * 5);

        switch (info->type) {
        case TARGET_TYPE_POS:
            x = info->arg1;
            y = info->arg2;
            dist = distance_approx(chr->x() - x, chr->y() - y);
            break;

        case TARGET_TYPE_VID:
            {
                tch = character_manager::instance().find(info->arg1);

                if (tch && tch->map_index() == chr->map_index()) {
                    x = tch->x();
                    y = tch->y();
                    dist = distance_approx(chr->x() - x, chr->y() - y);
                }
            }
            break;
        }

        bool ret = true;

        if (dist <= 500)
            ret = quest::quest_manager::instance().target(chr->player_id(), info->quest_index, info->target_name, "arrive");

        if (!tch && info->type == TARGET_TYPE_VID) {
            quest::quest_manager::instance().target(chr->player_id(), info->quest_index, info->target_name, "die");
            target_manager::instance().delete_target(chr->player_id(), info->quest_index, info->target_name);
        }

        if (event->is_force_to_end) {
            metin2::log::debug("Target: event cancelled");
            return 0;
        }

        if (x != info->old_x || y != info->old_y) {
            if (info->send_to_client)
                send_target_update_packet(chr->get_desc(), info->id, x, y);

            info->old_x = x;
            info->old_y = y;
        }

        if (!ret)
            return metin2::config::server().passes_per_sec;
        else
            return std::clamp(dist / (1500 / metin2::config::server().passes_per_sec), metin2::config::server().passes_per_sec / 2, metin2::config::server().passes_per_sec * 5);
    }

    void target_manager::create_target(uint32_t pid_value,
        uint32_t quest_index_value,
        const char *target_name_value,
        int type_value,
        int arg1_value,
        int arg2_value,
        int map_index_value,
        const char *target_desc_value,
        int send_flag) {
        metin2::log::debug("Target: create for pid {:d}, quest {:d}, name {}, type {:d}, args {:d} {:d}",
            pid_value, quest_index_value, target_name_value, type_value, arg1_value, arg2_value);

        character *chr = character_manager::instance().find_by_pid(pid_value);

        if (!chr) {
            metin2::log::error("Player (pid {:d}) not found", pid_value);
            return;
        }

        if (chr->map_index() != map_index_value)
            return;

        auto it = map_list_event_.find(pid_value);

        if (it != map_list_event_.end()) {
            auto it2 = it->second.begin();

            while (it2 != it->second.end()) {
                event_ptr event_value = *(it2++);
                auto *exist_info = dynamic_cast<target_info *>(event_value->info.get());

                if (nullptr == exist_info) {
                    metin2::log::error("Existing target event has no info");
                    return;
                }

                if (exist_info->quest_index == quest_index_value && !strcmp(exist_info->target_name, target_name_value)) {
                    metin2::log::debug("Target: replacing existing target");

                    if (exist_info->send_to_client)
                        send_target_delete_packet(chr->get_desc(), exist_info->id);

                    if (target_desc_value) {
                        strlcpy(exist_info->target_desc, target_desc_value, sizeof(exist_info->target_desc));
                    } else {
                        *exist_info->target_desc = '\0';
                    }

                    exist_info->id = ++id_;
                    exist_info->type = type_value;
                    exist_info->arg1 = arg1_value;
                    exist_info->arg2 = arg2_value;
                    exist_info->old_x = 0;
                    exist_info->old_y = 0;
                    exist_info->send_to_client = send_flag ? true : false;

                    send_target_create_packet(chr->get_desc(), exist_info);
                    return;
                }
            }
        }

        auto *new_info = alloc_event_info<target_info>();

        if (target_desc_value) {
            strlcpy(new_info->target_desc, target_desc_value, sizeof(new_info->target_desc));
        } else {
            *new_info->target_desc = '\0';
        }

        new_info->id = ++id_;
        new_info->pid = pid_value;
        new_info->quest_index = quest_index_value;
        strlcpy(new_info->target_name, target_name_value, sizeof(new_info->target_name));
        new_info->type = type_value;
        new_info->arg1 = arg1_value;
        new_info->arg2 = arg2_value;
        new_info->map_index = map_index_value;
        new_info->old_x = 0;
        new_info->old_y = 0;
        new_info->send_to_client = send_flag ? true : false;

        event_ptr event = event_create(game::target_event, new_info, passes_per_sec(1));

        if (nullptr != event) {
            map_list_event_[pid_value].push_back(event);

            send_target_create_packet(chr->get_desc(), new_info);
        }
    }

    void target_manager::delete_target(uint32_t pid_value, uint32_t quest_index_value, const char *target_name_value) {
        auto it = map_list_event_.find(pid_value);

        if (it == map_list_event_.end())
            return;

        auto it2 = it->second.begin();

        while (it2 != it->second.end()) {
            event_ptr event_value = *it2;
            auto *info = dynamic_cast<target_info *>(event_value->info.get());

            if (info == nullptr) {
                metin2::log::error("Target event has no info");
                ++it2;
                continue;
            }

            if (quest_index_value == info->quest_index) {
                if (!target_name_value || !strcmp(info->target_name, target_name_value)) {
                    if (info->send_to_client) {
                        character *chr = character_manager::instance().find_by_pid(info->pid);
                        if (chr != nullptr) {
                            send_target_delete_packet(chr->get_desc(), info->id);
                        }
                    }

                    event_cancel(&event_value);
                    it2 = it->second.erase(it2);
                    continue;
                }
            }

            it2++;
        }
    }

    auto target_manager::target_event(uint32_t pid, uint32_t quest_index_value, const char *target_name_value) -> event_ptr {
        auto it = map_list_event_.find(pid);

        if (it == map_list_event_.end())
            return nullptr;

        auto it2 = it->second.begin();

        while (it2 != it->second.end()) {
            event_ptr event_value = *(it2++);
            auto *info = dynamic_cast<target_info *>(event_value->info.get());

            if (info == nullptr) {
                metin2::log::error("Target event has no info");

                continue;
            }

            if (info->quest_index != quest_index_value)
                continue;

            if (strcmp(info->target_name, target_name_value))
                continue;

            return event_value;
        }

        return nullptr;
    }

    auto target_manager::get_target_info(uint32_t pid, int type_value, int arg1_value) -> target_info * {
        auto it = map_list_event_.find(pid);

        if (it == map_list_event_.end())
            return nullptr;

        auto it2 = it->second.begin();

        while (it2 != it->second.end()) {
            event_ptr event_value = *(it2++);
            auto *info = dynamic_cast<target_info *>(event_value->info.get());

            if (info == nullptr) {
                metin2::log::error("Target event has no info");

                continue;
            }

            if (!is_set(info->type, type_value))
                continue;

            if (info->arg1 != arg1_value)
                continue;

            return info;
        }

        return nullptr;
    }

    void target_manager::logout(uint32_t pid) {
        auto it = map_list_event_.find(pid);

        if (it == map_list_event_.end())
            return;

        auto it2 = it->second.begin();

        while (it2 != it->second.end())
            event_cancel(&(*(it2++)));

        map_list_event_.erase(it);
    }
}
