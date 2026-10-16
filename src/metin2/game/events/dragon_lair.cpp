
#include <metin2/game/events/dragon_lair.h>

#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/world/entity.h>
#include <metin2/game/world/regen.h>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/log/log.h>

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

namespace metin2::game::inline events {

    EVENTINFO(tag_dragon_lair_collapse_event_info) {
        int step;
        std::unique_ptr<dragon_lair> lair;
        int32_t instance_map_index;

        tag_dragon_lair_collapse_event_info()
            : step(0), lair(nullptr), instance_map_index(0) {
        }
    };

    EVENTFUNC(dragon_lair_collapse_event) {
        auto *info_value = dynamic_cast<tag_dragon_lair_collapse_event_info *>(event->info.get());

        if (info_value == nullptr) {
            metin2::log::error("Dragon lair collapse event has no event info");
            return 0;
        }

        if (0 == info_value->step) {
            char buf[512];
            snprintf(buf, 512, lc_text("용가리가 %d 초만에 죽어써효ㅠㅠ"), info_value->lair->estimated_time());
            send_notice_map(buf, info_value->instance_map_index, true);

            info_value->step++;

            return passes_per_sec(30);
        } else if (1 == info_value->step) {
            sectree_map *map = sectree_manager::instance().map(info_value->instance_map_index);

            if (nullptr != map) {
                map->for_each([](entity *ent) {
                    if (nullptr != ent) {
                        auto *ch = static_cast<character *>(ent);

                        if (nullptr != ch) {
                            if (true == ch->is_pc()) {
                                ch->go_home();
                            }
                        }
                    }
                });
            }

            info_value->step++;

            return passes_per_sec(30);
        } else {
            sectree_manager::instance().destroy_private_map(info_value->instance_map_index);
            info_value->lair.reset();
        }

        return 0;
    }

    dragon_lair::dragon_lair(uint32_t guild_id)
        : guild_id_(guild_id) {
        start_time_ = get_global_time();
    }

    dragon_lair::~dragon_lair() = default;

    auto dragon_lair::estimated_time() const -> uint32_t {
        return get_global_time() - start_time_;
    }

    void dragon_lair::on_dragon_dead(character *dragon) {
        metin2::log::info("Dragon lair: dragon of guild {:d} killed", guild_id_);

        log_manager::instance().dragon_slay_log(guild_id_, dragon->get_mob_table().vnum, start_time_, get_global_time());
    }

    dragon_lair_manager::dragon_lair_manager() = default;

    dragon_lair_manager::~dragon_lair_manager() = default;

    auto dragon_lair_manager::start(int32_t map_index_from, int32_t base_map_index, uint32_t guild_id) -> bool {
        int32_t instance_map_index = sectree_manager::instance().create_private_map(base_map_index);
        if (instance_map_index == 0) {
            metin2::log::error("Cannot start dragon lair, no private map index available");
            return false;
        }

        sectree_map *map = sectree_manager::instance().map(map_index_from);

        if (nullptr != map) {
            sectree_map *target_map = sectree_manager::instance().map(base_map_index);

            if (nullptr == target_map) {
                return false;
            }

            const map_region *region_info = sectree_manager::instance().get_map_region(target_map->setting.index);

            if (nullptr != region_info) {
                map->for_each([guild_id, instance_map_index](entity *ent) {
                    if (nullptr != ent && true == ent->is_type(ENTITY_CHARACTER)) {
                        auto *ch = static_cast<character *>(ent);

                        if (true == ch->is_pc()) {
                            if (nullptr != ch->get_guild()) {
                                if (guild_id == ch->get_guild()->id()) {
                                    ch->warp_set(844000, 1066900, instance_map_index);
                                }
                            }
                        }
                    }
                });

                lair_map_.try_emplace(guild_id, std::make_unique<dragon_lair>(guild_id));

                std::string map_base_path(locale_service_get_map_path());

                map_base_path += "/" + region_info->map_name + "/instance_regen.txt";

                metin2::log::info("Dragon lair: spawning regen {}", map_base_path);

                regen_do(map_base_path.c_str(), instance_map_index, target_map->setting.base_x, target_map->setting.base_y, nullptr, true);

                return true;
            }
        }

        return false;
    }

    void dragon_lair_manager::on_dragon_dead(character *dragon, uint32_t killer_guild_id) {
        if (nullptr == dragon)
            return;

        if (false == dragon->is_monster())
            return;

        auto iter = lair_map_.find(killer_guild_id);

        if (lair_map_.end() == iter) {
            return;
        }

        sectree_map *map = sectree_manager::instance().map(dragon->map_index());

        if (nullptr == iter->second || nullptr == map) {
            lair_map_.erase(iter);
            return;
        }

        iter->second->on_dragon_dead(dragon);

        tag_dragon_lair_collapse_event_info *info;
        info = alloc_event_info<tag_dragon_lair_collapse_event_info>();

        info->step = 0;
        info->lair = std::move(iter->second);
        info->instance_map_index = dragon->map_index();

        event_create(dragon_lair_collapse_event, info, passes_per_sec(10));

        lair_map_.erase(iter);
    }
}
