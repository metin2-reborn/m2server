
#include <metin2/config/config.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/affect_flag.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/characters/horsename_manager.h>
#include <metin2/game/combat/battle.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/events/arena.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/items/dragon_soul.h>
#include <metin2/game/items/item.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/text/locale.hpp>

#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/apply_types.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace metin2::game::inline characters {

    static auto is_no_save_affect(uint32_t type) -> bool { return (type == AFFECT_WAR_FLAG || type == AFFECT_REVIVE_INVISIBLE || (type >= AFFECT_PREMIUM_START && type <= AFFECT_PREMIUM_END)); }
    static auto is_no_clear_on_death_affect(uint32_t type) -> bool { return (type == AFFECT_BLOCK_CHAT || (type >= 500 && type < 600)); }

    void send_affect_remove_packet(desc *d, uint32_t pid, uint32_t type, uint8_t point) {
        packet_gc_affect_remove ptoc;
        ptoc.header = HEADER_GC_AFFECT_REMOVE;
        ptoc.type = type;
        ptoc.apply_on = point;
        d->packet(&ptoc, sizeof(packet_gc_affect_remove));

        packet_gd_remove_affect ptod;
        ptod.pid = pid;
        ptod.type = type;
        ptod.apply_on = point;
        db_clientdesc->db_packet(HEADER_GD_REMOVE_AFFECT, 0, &ptod, sizeof(ptod));
    }

    void send_affect_add_packet(desc *d, affect *aff) {
        packet_gc_affect_add ptoc;
        ptoc.header = HEADER_GC_AFFECT_ADD;
        ptoc.elem.type = aff->type;
        ptoc.elem.apply_on = aff->apply_on;
        ptoc.elem.apply_value = aff->apply_value;
        ptoc.elem.flag = aff->flag;
        ptoc.elem.duration = aff->duration;
        ptoc.elem.sp_cost = aff->sp_cost;
        d->packet(&ptoc, sizeof(packet_gc_affect_add));
    }
    auto character::find_affect(uint32_t type_value, uint8_t apply) const -> affect * {
        auto it = affects_.begin();

        while (it != affects_.end()) {
            affect *affect = *it++;

            if (affect->type == type_value && (apply == APPLY_NONE || apply == affect->apply_on))
                return affect;
        }

        return nullptr;
    }

    EVENTFUNC(affect_event) {
        auto *info = dynamic_cast<char_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Affect event fired without event info");
            return 0;
        }

        character *ch = info->ch;

        if (ch == nullptr) {
            return 0;
        }

        if (!ch->update_affect())
            return 0;
        else
            return metin2::config::server().passes_per_sec;
    }

    auto character::update_affect() -> bool {
        if (point(POINT_HP_RECOVERY) > 0) {
            if (max_hp() <= hp()) {
                point_change(POINT_HP_RECOVERY, -point(POINT_HP_RECOVERY));
            } else {
                int val = 0;

                val = std::min(point(POINT_HP_RECOVERY), max_hp() * 7 / 100);

                point_change(POINT_HP, val);
                point_change(POINT_HP_RECOVERY, -val);
            }
        }

        if (point(POINT_SP_RECOVERY) > 0) {
            if (max_sp() <= sp())
                point_change(POINT_SP_RECOVERY, -point(POINT_SP_RECOVERY));
            else {
                int val;

                val = std::min(point(POINT_SP_RECOVERY), max_sp() * 7 / 100);

                point_change(POINT_SP, val);
                point_change(POINT_SP_RECOVERY, -val);
            }
        }

        if (point(POINT_HP_RECOVER_CONTINUE) > 0) {
            point_change(POINT_HP, point(POINT_HP_RECOVER_CONTINUE));
        }

        if (point(POINT_SP_RECOVER_CONTINUE) > 0) {
            point_change(POINT_SP, point(POINT_SP_RECOVER_CONTINUE));
        }

        auto_recovery_item_process(AFFECT_AUTO_HP_RECOVERY);
        auto_recovery_item_process(AFFECT_AUTO_SP_RECOVERY);

        if (max_stamina() > stamina()) {
            int sec = (get_dword_time() - stop_time()) / 3000;
            if (sec)
                point_change(POINT_STAMINA, max_stamina() / 1);
        }

        if (process_affect())
            if (point(POINT_HP_RECOVERY) == 0 && point(POINT_SP_RECOVERY) == 0 && stamina() == max_stamina()) {
                affect_event = nullptr;
                return false;
            }

        return true;
    }

    void character::start_affect_event() {
        if (affect_event)
            return;

        auto *info = alloc_event_info<char_event_info>();
        info->ch = this;
        affect_event = event_create(game::characters::affect_event, info, metin2::config::server().passes_per_sec);
        metin2::log::debug("{} ({}): affect event started ({})", name(), static_cast<const void *>(this), static_cast<const void *>(affect_event.get()));
    }

    void character::clear_affect(bool save) {
        affect_flag af_old = af_affect_flag_;
        uint16_t mov_spd = point(POINT_MOV_SPEED);
        uint16_t att_spd = point(POINT_ATT_SPEED);

        auto it = affects_.begin();

        while (it != affects_.end()) {
            affect *aff = *it;

            if (save) {
                if (is_no_clear_on_death_affect(aff->type) || is_no_save_affect(aff->type)) {
                    ++it;
                    continue;
                }

                if (is_pc()) {
                    send_affect_remove_packet(get_desc(), player_id(), aff->type, aff->apply_on);
                }
            }

            compute_affect(aff, false);

            it = affects_.erase(it);
            delete aff;
        }

        if (af_old != af_affect_flag_ ||
            mov_spd != point(POINT_MOV_SPEED) ||
            att_spd != point(POINT_ATT_SPEED))
            update_packet();

        check_maximum_points();

        if (affects_.empty())
            event_cancel(&affect_event);
    }

    auto character::process_affect() -> int {
        bool diff = false;
        affect *aff = nullptr;

        for (int i = 0; i <= PREMIUM_MAX_NUM; ++i) {
            int aff_idx = i + AFFECT_PREMIUM_START;

            aff = find_affect(aff_idx);

            if (!aff)
                continue;

            int remain = premium_remain_seconds(i);

            if (remain < 0) {
                remove_affect(aff_idx);
                diff = true;
            } else
                aff->duration = remain + 1;
        }

        aff = find_affect(AFFECT_HAIR);
        if (aff) {
            if (this->quest_flag("hair.limit_time") < get_global_time()) {
                this->set_part(PART_HAIR, 0);
                remove_affect(AFFECT_HAIR);
            } else {
                ++(aff->duration);
            }
        }

        get_horse_name_manager().validate(this);

        affect_flag af_old = af_affect_flag_;
        int32_t mov_spd = point(POINT_MOV_SPEED);
        int32_t att_spd = point(POINT_ATT_SPEED);

        decltype(affects_.begin()) it;

        it = affects_.begin();

        while (it != affects_.end()) {
            aff = *it;

            bool end = false;

            if (aff->type >= GUILD_SKILL_START && aff->type <= GUILD_SKILL_END) {
                if (!get_guild() || !get_guild()->under_any_war())
                    end = true;
            }

            if (aff->sp_cost > 0) {
                if (sp() < aff->sp_cost)
                    end = true;
                else
                    point_change(POINT_SP, -aff->sp_cost);
            }

            if (--aff->duration <= 0) {
                end = true;
            }

            if (end) {
                it = affects_.erase(it);
                compute_affect(aff, false);
                diff = true;
                if (is_pc()) {
                    send_affect_remove_packet(get_desc(), player_id(), aff->type, aff->apply_on);
                }

                delete aff;

                continue;
            }

            ++it;
        }

        if (diff) {
            if (af_old != af_affect_flag_ ||
                mov_spd != point(POINT_MOV_SPEED) ||
                att_spd != point(POINT_ATT_SPEED)) {
                update_packet();
            }

            check_maximum_points();
        }

        if (affects_.empty())
            return true;

        return false;
    }

    void character::save_affect() {
        packet_gd_add_affect p;

        auto it = affects_.begin();

        while (it != affects_.end()) {
            affect *aff = *it++;

            if (is_no_save_affect(aff->type))
                continue;

            metin2::log::debug("Saving affect type {:d}, apply {:d}, value {:d}, duration {:d}", aff->type, aff->apply_on, aff->apply_value, aff->duration);

            p.pid = player_id();
            p.elem.type = aff->type;
            p.elem.apply_on = aff->apply_on;
            p.elem.apply_value = aff->apply_value;
            p.elem.flag = aff->flag;
            p.elem.duration = aff->duration;
            p.elem.sp_cost = aff->sp_cost;
            db_clientdesc->db_packet(HEADER_GD_ADD_AFFECT, 0, &p, sizeof(p));
        }
    }

    EVENTINFO(load_affect_login_event_info) {
        uint32_t pid;
        uint32_t count;
        std::vector<packet_affect_element> data;

        load_affect_login_event_info()
            : pid(0), count(0) {
        }
    };

    EVENTFUNC(load_affect_login_event) {
        auto *info = dynamic_cast<load_affect_login_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Affect login load event fired without event info");
            return 0;
        }

        uint32_t pid_value = info->pid;
        character *ch = get_character_manager().find_by_pid(pid_value);

        if (!ch) {
            return 0;
        }

        desc *d = ch->get_desc();

        if (!d) {
            return 0;
        }

        if (d->is_phase(PHASE_HANDSHAKE) ||
            d->is_phase(PHASE_LOGIN) ||
            d->is_phase(PHASE_SELECT) ||
            d->is_phase(PHASE_DEAD) ||
            d->is_phase(PHASE_LOADING)) {
            return passes_per_sec(1);
        } else if (d->is_phase(PHASE_CLOSE)) {
            return 0;
        } else if (d->is_phase(PHASE_GAME)) {
            metin2::log::debug("{}: loading affects from event", ch->name());
            ch->load_affect(info->count, info->data.data());
            return 0;
        } else {
            metin2::log::error("Cannot load affects for pid {:d}: player is in an invalid phase", ch->player_id());
            return 0;
        }
    }

    void character::load_affect(uint32_t count_value, packet_affect_element *elements) {

        if (!get_desc()->is_phase(PHASE_GAME)) {
            if (metin2::config::test_server())
                metin2::log::debug("{}: deferring load of {:d} affects to an event", name(), count_value);

            auto *info = alloc_event_info<load_affect_login_event_info>();

            info->pid = player_id();
            info->count = count_value;
            info->data.assign(elements, elements + count_value);

            event_create(load_affect_login_event, info, passes_per_sec(1));

            return;
        }

        clear_affect(true);

        if (metin2::config::test_server())
            metin2::log::debug("{}: loading {:d} affects", name(), count_value);

        affect_flag af_old = af_affect_flag_;

        int32_t mov_spd = point(POINT_MOV_SPEED);
        int32_t att_spd = point(POINT_ATT_SPEED);

        for (uint32_t i = 0; i < count_value; ++i, ++elements) {
            if (elements->type == SKILL_MUYEONG)
                continue;

            if (AFFECT_AUTO_HP_RECOVERY == elements->type || AFFECT_AUTO_SP_RECOVERY == elements->type) {
                item *item = find_item_by_id(elements->flag);

                if (nullptr == item)
                    continue;

                item->lock(true);
            }

            if (elements->apply_on >= POINT_MAX_NUM) {
                metin2::log::error("Invalid affect data for {} (apply {:d}, value {:d})",
                    name(), elements->apply_on, elements->apply_value);
                continue;
            }

            if (metin2::config::test_server()) {
                metin2::log::debug("{}: loaded affect type {:d}, apply {:d}", name(), elements->type, elements->apply_on);
            }

            auto *aff = new affect{};
            affects_.push_back(aff);

            aff->type = elements->type;
            aff->apply_on = elements->apply_on;
            aff->apply_value = elements->apply_value;
            aff->flag = elements->flag;
            aff->duration = elements->duration;
            aff->sp_cost = elements->sp_cost;

            send_affect_add_packet(get_desc(), aff);

            compute_affect(aff, true);
        }

        if (get_arena_manager().is_arena_map(map_index()) == true) {
            remove_good_affect();
        }

        if (af_old != af_affect_flag_ || mov_spd != point(POINT_MOV_SPEED) || att_spd != point(POINT_ATT_SPEED)) {
            update_packet();
        }

        start_affect_event();

        dragon_soul_initialize();
    }

    auto character::add_affect(uint32_t type_value, uint8_t apply_on_value, int32_t apply_value_value, uint32_t flag_value, int32_t duration_value, int32_t sp_cost_value, bool override, bool is_cube) -> bool {
        if (type_value == AFFECT_BLOCK_CHAT && duration_value > 1) {
            chat_packet(CHAT_TYPE_INFO, lc_text("운영자 제제로 채팅이 금지 되었습니다."));
        }

        if (duration_value == 0) {
            metin2::log::error("Affect type {:d} added with zero duration; using 1", type_value);
            duration_value = 1;
        }

        affect *aff = nullptr;

        if (is_cube)
            aff = find_affect(type_value, apply_on_value);
        else
            aff = find_affect(type_value);

        if (flag_value == AFF_STUN) {
            if (pos_dest_.x != x() || pos_dest_.y != y()) {
                pos_dest_.x = pos_start_.x = x();
                pos_dest_.y = pos_start_.y = y();
                battle_end(this);

                sync_packet();
            }
        }

        if (aff && override) {
            compute_affect(aff, false);

            if (get_desc())
                send_affect_remove_packet(get_desc(), player_id(), aff->type, aff->apply_on);
        } else {
            aff = new affect{};
            affects_.push_back(aff);
        }

        metin2::log::debug("{}: add affect type {:d}, apply {:d}, value {:d}, flag {:d}, duration {:d}", name(), type_value, apply_on_value, apply_value_value, flag_value, duration_value);

        aff->type = type_value;
        aff->apply_on = apply_on_value;
        aff->apply_value = apply_value_value;
        aff->flag = flag_value;
        aff->duration = duration_value;
        aff->sp_cost = sp_cost_value;

        uint16_t mov_spd = point(POINT_MOV_SPEED);
        uint16_t att_spd = point(POINT_ATT_SPEED);

        compute_affect(aff, true);

        if (aff->flag || mov_spd != point(POINT_MOV_SPEED) || att_spd != point(POINT_ATT_SPEED))
            update_packet();

        start_affect_event();

        if (is_pc()) {
            send_affect_add_packet(get_desc(), aff);

            if (is_no_save_affect(aff->type))
                return true;

            packet_gd_add_affect p;
            p.pid = player_id();
            p.elem.type = aff->type;
            p.elem.apply_on = aff->apply_on;
            p.elem.apply_value = aff->apply_value;
            p.elem.flag = aff->flag;
            p.elem.duration = aff->duration;
            p.elem.sp_cost = aff->sp_cost;
            db_clientdesc->db_packet(HEADER_GD_ADD_AFFECT, 0, &p, sizeof(p));
        }

        return true;
    }

    void character::refresh_affect() {
        auto it = affects_.begin();

        while (it != affects_.end()) {
            affect *aff = *it++;
            compute_affect(aff, true);
        }
    }

    void character::compute_affect(affect *aff, bool add) {
        if (add && aff->type >= GUILD_SKILL_START && aff->type <= GUILD_SKILL_END) {
            if (!get_guild())
                return;

            if (!get_guild()->under_any_war())
                return;
        }

        if (aff->flag) {
            if (!add)
                af_affect_flag_.reset(aff->flag);
            else
                af_affect_flag_.set(aff->flag);
        }

        if (add)
            point_change(aff->apply_on, aff->apply_value);
        else
            point_change(aff->apply_on, -aff->apply_value);

        if (aff->type == SKILL_MUYEONG) {
            if (add)
                start_muyeong_event();
            else
                stop_muyeong_event();
        }
    }

    auto character::remove_affect(affect *aff) -> bool {
        if (!aff)
            return false;

        affects_.remove(aff);

        compute_affect(aff, false);

        // Removing AFFECT_REVIVE_INVISIBLE skips compute_points, as expiry does: recomputing while
        // polymorphed after a buff would wrongly apply the buffs.
        if (AFFECT_REVIVE_INVISIBLE != aff->type) {
            compute_points();
        } else {
            update_packet();
        }

        check_maximum_points();

        if (metin2::config::test_server())
            metin2::log::debug("{}: remove affect (flag {:d}, apply {:d})", name(), aff->flag, aff->apply_on);

        if (is_pc()) {
            send_affect_remove_packet(get_desc(), player_id(), aff->type, aff->apply_on);
        }

        delete aff;
        return true;
    }

    auto character::remove_affect(uint32_t type) -> bool {
        if (type == AFFECT_BLOCK_CHAT) {
            chat_packet(CHAT_TYPE_INFO, lc_text("채팅 금지가 풀렸습니다."));
        }

        bool flag = false;

        affect *aff;

        while ((aff = find_affect(type))) {
            remove_affect(aff);
            flag = true;
        }

        return flag;
    }

    auto character::is_affect_flag(uint32_t aff) const -> bool {
        return af_affect_flag_.is_set(aff);
    }

    void character::remove_good_affect() {
        remove_affect(AFFECT_MOV_SPEED);
        remove_affect(AFFECT_ATT_SPEED);
        remove_affect(AFFECT_STR);
        remove_affect(AFFECT_DEX);
        remove_affect(AFFECT_INT);
        remove_affect(AFFECT_CON);
        remove_affect(AFFECT_CHINA_FIREWORK);

        remove_affect(SKILL_JEONGWI);
        remove_affect(SKILL_GEOMKYUNG);
        remove_affect(SKILL_CHUNKEON);
        remove_affect(SKILL_EUNHYUNG);
        remove_affect(SKILL_GYEONGGONG);
        remove_affect(SKILL_GWIGEOM);
        remove_affect(SKILL_TERROR);
        remove_affect(SKILL_JUMAGAP);
        remove_affect(SKILL_MANASHILED);
        remove_affect(SKILL_HOSIN);
        remove_affect(SKILL_REFLECT);
        remove_affect(SKILL_KWAESOK);
        remove_affect(SKILL_JEUNGRYEOK);
        remove_affect(SKILL_GICHEON);
    }

    auto character::is_good_affect(uint8_t affect_type) const -> bool {
        switch (affect_type) {
        case (AFFECT_MOV_SPEED):
        case (AFFECT_ATT_SPEED):
        case (AFFECT_STR):
        case (AFFECT_DEX):
        case (AFFECT_INT):
        case (AFFECT_CON):
        case (AFFECT_CHINA_FIREWORK):

        case (SKILL_JEONGWI):
        case (SKILL_GEOMKYUNG):
        case (SKILL_CHUNKEON):
        case (SKILL_EUNHYUNG):
        case (SKILL_GYEONGGONG):
        case (SKILL_GWIGEOM):
        case (SKILL_TERROR):
        case (SKILL_JUMAGAP):
        case (SKILL_MANASHILED):
        case (SKILL_HOSIN):
        case (SKILL_REFLECT):
        case (SKILL_KWAESOK):
        case (SKILL_JEUNGRYEOK):
        case (SKILL_GICHEON):
            return true;
        }
        return false;
    }

    void character::remove_bad_affect() {
        metin2::log::debug("{}: removing bad affects", name());
        remove_poison();
        remove_fire();

        remove_affect(AFFECT_STUN);

        remove_affect(AFFECT_SLOW);

        remove_affect(SKILL_TUSOK);
    }
}
