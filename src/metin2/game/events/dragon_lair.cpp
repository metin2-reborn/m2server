
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

    struct f_warp_to_dragron_lair_with_guild_members {
        uint32_t guild_id;
        int32_t index;
        int32_t x, y;

        f_warp_to_dragron_lair_with_guild_members(uint32_t guild_id_value, int32_t map, int32_t x_value, int32_t y_value)
            : guild_id(guild_id_value), index(map), x(x_value), y(y_value) {
        }

        void operator()(entity *ent) {
            if (nullptr != ent && true == ent->is_type(ENTITY_CHARACTER)) {
                character *pChar = static_cast<character *>(ent);

                if (true == pChar->is_pc()) {
                    if (nullptr != pChar->get_guild()) {
                        if (guild_id == pChar->get_guild()->id()) {
                            pChar->warp_set(x, y, index);
                        }
                    }
                }
            }
        }
    };

    struct f_warp_to_village {
        void operator()(entity *ent) {
            if (nullptr != ent) {
                character *pChar = static_cast<character *>(ent);

                if (nullptr != pChar) {
                    if (true == pChar->is_pc()) {
                        pChar->go_home();
                    }
                }
            }
        }
    };

    EVENTINFO(tag_dragon_lair_collapse_event_info) {
        int step;
        std::unique_ptr<dragon_lair> lair;
        int32_t instance_map_index;

        tag_dragon_lair_collapse_event_info()
            : step(0), lair(nullptr), instance_map_index(0) {
        }
    };

    EVENTFUNC(dragon_lair_collapse_event) {
        tag_dragon_lair_collapse_event_info *pInfo = dynamic_cast<tag_dragon_lair_collapse_event_info *>(event->info);

        if (pInfo == nullptr) {
            metin2::log::error("Dragon lair collapse event has no event info");
            return 0;
        }

        if (0 == pInfo->step) {
            char buf[512];
            snprintf(buf, 512, lc_text("용가리가 %d 초만에 죽어써효ㅠㅠ"), pInfo->lair->estimated_time());
            send_notice_map(buf, pInfo->instance_map_index, true);

            pInfo->step++;

            return passes_per_sec(30);
        } else if (1 == pInfo->step) {
            sectree_map *pMap = sectree_manager::instance().map(pInfo->instance_map_index);

            if (nullptr != pMap) {
                f_warp_to_village f;
                pMap->for_each(f);
            }

            pInfo->step++;

            return passes_per_sec(30);
        } else {
            sectree_manager::instance().destroy_private_map(pInfo->instance_map_index);
            pInfo->lair.reset();
        }

        return 0;
    }

    dragon_lair::dragon_lair(uint32_t guild_id)
        : guild_id_(guild_id) {
        start_time_ = get_global_time();
    }

    dragon_lair::~dragon_lair() {
    }

    auto dragon_lair::estimated_time() const -> uint32_t {
        return get_global_time() - start_time_;
    }

    void dragon_lair::on_dragon_dead(character *dragon) {
        metin2::log::info("Dragon lair: dragon of guild {:d} killed", guild_id_);

        log_manager::instance().dragon_slay_log(guild_id_, dragon->get_mob_table().vnum, start_time_, get_global_time());
    }

    dragon_lair_manager::dragon_lair_manager() {
    }

    dragon_lair_manager::~dragon_lair_manager() = default;

    auto dragon_lair_manager::start(int32_t map_index_from, int32_t base_map_index, uint32_t guild_id) -> bool {
        int32_t instanceMapIndex = sectree_manager::instance().create_private_map(base_map_index);
        if (instanceMapIndex == 0) {
            metin2::log::error("Cannot start dragon lair, no private map index available");
            return false;
        }

        sectree_map *pMap = sectree_manager::instance().map(map_index_from);

        if (nullptr != pMap) {
            sectree_map *pTargetMap = sectree_manager::instance().map(base_map_index);

            if (nullptr == pTargetMap) {
                return false;
            }

            const map_region *pRegionInfo = sectree_manager::instance().get_map_region(pTargetMap->setting.index);

            if (nullptr != pRegionInfo) {
                f_warp_to_dragron_lair_with_guild_members f(guild_id, instanceMapIndex, 844000, 1066900);

                pMap->for_each(f);

                lair_map_.try_emplace(guild_id, std::make_unique<dragon_lair>(guild_id));

                std::string strMapBasePath(locale_service_get_map_path());

                strMapBasePath += "/" + pRegionInfo->map_name + "/instance_regen.txt";

                metin2::log::info("Dragon lair: spawning regen {}", strMapBasePath.c_str());

                regen_do(strMapBasePath.c_str(), instanceMapIndex, pTargetMap->setting.base_x, pTargetMap->setting.base_y, nullptr, true);

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

        sectree_map *pMap = sectree_manager::instance().map(dragon->map_index());

        if (nullptr == iter->second || nullptr == pMap) {
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
