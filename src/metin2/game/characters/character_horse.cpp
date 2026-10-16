#include <metin2/config/config.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/characters/horse_rider.h>
#include <metin2/game/characters/horsename_manager.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/items/item.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/world/vector.h>

#include <metin2/core/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/inventory.h>
#include <metin2/proto/vnum_helper.h>

#include <cstdint>
#include <cstring>

namespace metin2::game::inline characters {

    auto character::start_riding() -> bool {
        if (is_dead() == true) {
            chat_packet(CHAT_TYPE_INFO, lc_text("쓰러진 상태에서는 말에 탈 수 없습니다."));
            return false;
        }

        if (is_polymorphed()) {
            chat_packet(CHAT_TYPE_INFO, lc_text("변신 상태에서는 말에 탈 수 없습니다."));
            return false;
        }

        item *armor = wear(WEAR_BODY);

        if (armor && (armor->vnum() >= 11901 && armor->vnum() <= 11904)) {
            chat_packet(CHAT_TYPE_INFO, lc_text("예복을 입은 상태에서 말을 탈 수 없습니다."));
            return false;
        }

        uint32_t dwMountVnum = ch_horse_ ? ch_horse_->race_num() : my_horse_vnum();

        if (false == horse_rider::start_riding()) {
            if (horse_level() <= 0)
                chat_packet(CHAT_TYPE_INFO, lc_text("말을 소유하고 있지 않습니다."));
            else if (horse_health() <= 0)
                chat_packet(CHAT_TYPE_INFO, lc_text("말이 죽어있는 상태 입니다."));
            else if (horse_stamina() <= 0)
                chat_packet(CHAT_TYPE_INFO, lc_text("말의 스테미너가 부족하여 말을 탈 수 없습니다."));

            return false;
        }

        horse_summon(false);

        mount_vnum(dwMountVnum);

        if (metin2::config::test_server())
            metin2::log::debug("{} mounts a horse", name());

        return true;
    }

    auto character::stop_riding() -> bool {
        if (horse_rider::stop_riding()) {
            quest::quest_manager::instance().unmount(player_id());

            if (!is_dead() && !is_stun()) {
                uint32_t dwOldVnum = get_mount_vnum();
                mount_vnum(0);

                horse_summon(true, false, dwOldVnum);
            } else {
                mount_vnum_ = 0;
                compute_points();
                update_packet();
            }

            point_change(POINT_ST, 0);
            point_change(POINT_DX, 0);
            point_change(POINT_HT, 0);
            point_change(POINT_IQ, 0);

            return true;
        }

        return false;
    }

    EVENTFUNC(horse_dead_event) {
        char_event_info *info = dynamic_cast<char_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Horse dead event fired without event info");
            return 0;
        }

        character *ch = info->ch;
        if (ch == nullptr) {
            return 0;
        }
        ch->horse_summon(false);
        return 0;
    }

    void character::set_rider(character *ch) {
        if (ch_rider_)
            ch_rider_->clear_horse_info();

        ch_rider_ = ch;

        if (ch_rider_)
            ch_rider_->send_horse_info();
    }

    auto character::rider() const -> character * {
        return ch_rider_;
    }

    void character::horse_summon(bool summon, bool from_far, uint32_t vnum, const char *pet_name) {
        if (summon) {
            if (ch_horse_ != nullptr)
                return;

            if (horse_level() <= 0)
                return;

            if (is_riding())
                return;

            metin2::log::debug("Horse summon: {} level {:d}, summon {:d}, from far {:d}", name(), level(), summon, from_far);

            int32_t x_value = x();
            int32_t y_value = y();

            if (horse_health() <= 0)
                from_far = false;

            if (from_far) {
                x_value += (number(0, 1) * 2 - 1) * number(2000, 2500);
                y_value += (number(0, 1) * 2 - 1) * number(2000, 2500);
            } else {
                x_value += number(-100, 100);
                y_value += number(-100, 100);
            }

            ch_horse_ = character_manager::instance().spawn_mob(
                (0 == vnum) ? my_horse_vnum() : vnum,
                map_index(),
                x_value, y_value,
                z(), false, (int)(rotation() + 180), false);

            if (!ch_horse_) {
                chat_packet(CHAT_TYPE_INFO, lc_text("말 소환에 실패하였습니다."));
                return;
            }

            if (horse_health() <= 0) {
                ch_horse_->set_position(POS_DEAD);

                char_event_info *info = alloc_event_info<char_event_info>();
                info->ch = this;
                ch_horse_->dead_event = event_create(horse_dead_event, info, passes_per_sec(60));
            }

            ch_horse_->set_level(horse_level());

            const char *pHorseName = horse_name_manager::instance().horse_name(player_id());

            if (pHorseName != nullptr && strlen(pHorseName) != 0) {
                ch_horse_->name_ = pHorseName;
            } else {
                ch_horse_->name_ = name();
                ch_horse_->name_ += lc_text("님의 말");
            }

            if (!ch_horse_->show(map_index(), x_value, y_value, z())) {
                character_manager::instance().destroy_character(ch_horse_);
                metin2::log::error("Cannot show summoned horse");
                ch_horse_ = nullptr;
                return;
            }

            if ((horse_health() <= 0)) {
                packet_gc_dead pack;
                pack.header = HEADER_GC_DEAD;
                pack.vid = ch_horse_->get_vid();
                packet_around(&pack, sizeof(pack));
            }

            ch_horse_->set_rider(this);
        } else {
            if (!ch_horse_)
                return;

            character *chHorse = ch_horse_;

            chHorse->set_rider(nullptr);

            if (!from_far) {
                character_manager::instance().destroy_character(chHorse);
            } else {
                chHorse->set_now_walking(false);
                float fx, fy;
                chHorse->set_rotation(get_degree_from_position_xy(chHorse->x(), chHorse->y(), x(), y()) + 180);
                get_delta_by_degree(chHorse->rotation(), 3500, &fx, &fy);
                chHorse->go_to((int32_t)(chHorse->x() + fx), (int32_t)(chHorse->y() + fy));
                chHorse->send_move_packet(FUNC_WAIT, 0, 0, 0, 0);
            }

            ch_horse_ = nullptr;
        }
    }

    auto character::my_horse_vnum() const -> uint32_t {
        int delta = 0;

        if (get_guild()) {
            ++delta;

            if (get_guild()->master_pid() == player_id())
                ++delta;
        }

        return horse_stats[horse_level()].npc_race + delta;
    }

    void character::horse_die() {
        horse_rider::horse_die();
        horse_summon(false);
    }

    auto character::revive_horse() -> bool {
        if (horse_rider::revive_horse()) {
            horse_summon(false);
            horse_summon(true);
            return true;
        }
        return false;
    }

    void character::clear_horse_info() {
        if (!is_horse_riding()) {
            chat_packet(CHAT_TYPE_COMMAND, "hide_horse_state");

            send_horse_level_ = 0;
            send_horse_health_grade_ = 0;
            send_horse_stamina_grade_ = 0;
        }

        ch_horse_ = nullptr;
    }

    void character::send_horse_info() {
        if (ch_horse_ || is_horse_riding()) {
            int iHealthGrade;
            int iStaminaGrade;
            if (horse_health() == 0)
                iHealthGrade = 0;
            else if (horse_health() * 10 <= horse_max_health() * 3)
                iHealthGrade = 1;
            else if (horse_health() * 10 <= horse_max_health() * 7)
                iHealthGrade = 2;
            else
                iHealthGrade = 3;

            if (horse_stamina() * 10 <= horse_max_stamina())
                iStaminaGrade = 0;
            else if (horse_stamina() * 10 <= horse_max_stamina() * 3)
                iStaminaGrade = 1;
            else if (horse_stamina() * 10 <= horse_max_stamina() * 7)
                iStaminaGrade = 2;
            else
                iStaminaGrade = 3;

            if (send_horse_level_ != horse_level() ||
                send_horse_health_grade_ != iHealthGrade ||
                send_horse_stamina_grade_ != iStaminaGrade) {
                chat_packet(CHAT_TYPE_COMMAND, "horse_state %d %d %d", horse_level(), iHealthGrade, iStaminaGrade);

                // Returning before this makes horses summon endlessly.
                send_horse_level_ = horse_level();
                send_horse_health_grade_ = iHealthGrade;
                send_horse_stamina_grade_ = iStaminaGrade;
            }
        }
    }

    auto character::can_use_horse_skill() -> bool {
        if (is_riding()) {
            if (horse_grade() == 3)
                return true;
            else
                return false;

            if (get_mount_vnum()) {
                if (get_mount_vnum() >= 20209 && get_mount_vnum() <= 20212)
                    return true;

                if (mob_vnum::is_ramadan_black_horse(get_mount_vnum()))
                    return true;
            } else
                return false;
        }

        return false;
    }

    void character::set_horse_level(int level) {
        horse_rider::set_horse_level(level);
        set_skill_level(SKILL_HORSE, horse_level());
    }
}
