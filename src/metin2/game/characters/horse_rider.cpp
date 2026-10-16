#include <metin2/game/characters/horse_rider.h>

#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/utils.h>

#include <metin2/log/log.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace metin2::game::inline characters {

    const int HORSE_HEALTH_DROP_INTERVAL = 3 * 24 * 60 * 60;
    const int HORSE_STAMINA_CONSUME_INTERVAL = 6 * 60;
    const int HORSE_STAMINA_REGEN_INTERVAL = 12 * 60;

    horse_stat horse_stats[HORSE_MAX_LEVEL + 1] =
        {
            {0,  0,     1,  1,   0,  0,  0,  0,  0,  0,  0,   0 },
            {25, 20101, 3,  4,   26, 35, 18, 9,  54, 43, 64,  32},
            {25, 20101, 4,  4,   27, 36, 18, 9,  55, 44, 66,  33},
            {25, 20101, 5,  5,   28, 38, 19, 9,  56, 44, 67,  33},
            {25, 20101, 7,  5,   29, 39, 19, 10, 57, 45, 68,  34},
            {25, 20101, 8,  6,   30, 40, 20, 10, 58, 46, 69,  34},
            {25, 20101, 9,  6,   31, 41, 21, 10, 59, 47, 70,  35},
            {25, 20101, 11, 7,   32, 42, 21, 11, 60, 48, 72,  36},
            {25, 20101, 12, 7,   33, 44, 22, 11, 61, 48, 73,  36},
            {25, 20101, 13, 8,   34, 45, 22, 11, 62, 49, 74,  37},
            {25, 20101, 15, 10,  35, 46, 23, 12, 63, 50, 75,  37},
            {35, 20104, 18, 30,  40, 53, 27, 13, 69, 55, 82,  41},
            {35, 20104, 19, 35,  41, 54, 27, 14, 70, 56, 84,  42},
            {35, 20104, 21, 40,  42, 56, 28, 14, 71, 56, 85,  42},
            {35, 20104, 22, 50,  43, 57, 28, 14, 72, 57, 86,  43},
            {35, 20104, 24, 55,  44, 58, 29, 15, 73, 58, 87,  43},
            {35, 20104, 25, 60,  44, 59, 30, 15, 74, 59, 88,  44},
            {35, 20104, 27, 65,  45, 60, 30, 15, 75, 60, 90,  45},
            {35, 20104, 28, 70,  46, 62, 31, 15, 76, 60, 91,  45},
            {35, 20104, 30, 80,  47, 63, 31, 16, 77, 61, 92,  46},
            {35, 20104, 32, 100, 48, 64, 32, 16, 78, 62, 93,  46},
            {50, 20107, 35, 120, 53, 71, 36, 18, 84, 67, 100, 50},
            {50, 20107, 36, 125, 55, 74, 37, 18, 86, 68, 103, 51},
            {50, 20107, 37, 130, 57, 76, 38, 19, 88, 70, 105, 52},
            {50, 20107, 38, 135, 59, 78, 39, 20, 90, 72, 108, 54},
            {50, 20107, 40, 140, 60, 80, 40, 20, 91, 72, 109, 54},
            {50, 20107, 42, 145, 61, 81, 40, 20, 92, 73, 110, 55},
            {50, 20107, 44, 150, 62, 83, 42, 21, 94, 75, 112, 56},
            {50, 20107, 46, 160, 63, 84, 42, 21, 95, 76, 114, 57},
            {50, 20107, 48, 170, 65, 87, 43, 22, 97, 77, 116, 58},
            {50, 20107, 50, 200, 67, 89, 45, 22, 99, 79, 118, 59}
    };

    horse_rider::horse_rider() {
        initialize();
    }

    horse_rider::~horse_rider() {
        destroy();
    }

    void horse_rider::initialize() {
        event_stamina_regen_ = nullptr;
        event_stamina_consume_ = nullptr;
        memset(&horse_, 0, sizeof(horse_));
    }

    void horse_rider::destroy() {
        event_cancel(&event_stamina_regen_);
        event_cancel(&event_stamina_consume_);
    }

    void horse_rider::enter_horse() {
        if (horse_level() <= 0)
            return;

        if (horse_health() <= 0)
            return;

        if (is_horse_riding()) {
            horse_.riding = !horse_.riding;
            start_riding();
        } else {
            start_stamina_regen_event();
        }
        check_horse_health_drop_time(false);
    }

    auto horse_rider::revive_horse() -> bool {
        if (horse_level() <= 0)
            return false;

        if (horse_health() > 0)
            return false;

        int level = horse_level();

        horse_.s_health = horse_stats[level].max_health;
        horse_.s_stamina = horse_stats[level].max_stamina;

        reset_horse_health_drop_time();

        start_stamina_regen_event();
        return true;
    }

    auto horse_rider::horse_max_health() -> short {
        int level = horse_level();
        return horse_stats[level].max_health;
    }

    auto horse_rider::horse_max_stamina() -> short {
        int level = horse_level();
        return horse_stats[level].max_stamina;
    }

    void horse_rider::feed_horse() {
        if (horse_level() > 0 && horse_health() > 0) {
            update_horse_health(+1);
            reset_horse_health_drop_time();
        }
    }

    void horse_rider::set_horse_data(const horse_info &cr_info) {
        horse_ = cr_info;
    }

    void horse_rider::update_horse_data_by_logoff(uint32_t logoff_time) {
        if (horse_level() <= 0)
            return;

        if (logoff_time >= 12 * 60)
            update_horse_stamina(logoff_time / 12 / 60, false);
    }

    void horse_rider::update_horse_stamina(int stamina, bool send) {
        int level = horse_level();

        horse_.s_stamina = std::clamp(horse_.s_stamina + stamina, 0, horse_stats[level].max_stamina);

        if (horse_stamina() == 0 && is_horse_riding()) {
            stop_riding();
        }

        if (send)
            send_horse_info();
    }

    auto horse_rider::start_riding() -> bool {
        if (horse_.riding)
            return false;

        if (horse_level() <= 0)
            return false;

        if (horse_health() <= 0)
            return false;

        if (horse_stamina() <= 0)
            return false;

        horse_.riding = true;
        start_stamina_consume_event();
        send_horse_info();
        return true;
    }

    auto horse_rider::stop_riding() -> bool {
        if (!horse_.riding)
            return false;

        horse_.riding = false;
        start_stamina_regen_event();
        return true;
    }

    EVENTINFO(horserider_info) {
        horse_rider *hr;

        horserider_info()
            : hr(nullptr) {
        }
    };

    EVENTFUNC(horse_stamina_consume_event) {
        horserider_info *info = dynamic_cast<horserider_info *>(event->info);

        if (info == nullptr) {
            metin2::log::error("Horse stamina consume event fired without event info");
            return 0;
        }

        horse_rider *hr = info->hr;

        if (hr->horse_health() <= 0) {
            hr->event_stamina_consume_ = nullptr;
            return 0;
        }

        hr->update_horse_stamina(-1);
        hr->update_ride_time(HORSE_STAMINA_CONSUME_INTERVAL);

        int delta = passes_per_sec(HORSE_STAMINA_CONSUME_INTERVAL);

        if (hr->horse_stamina() == 0) {
            hr->event_stamina_consume_ = nullptr;
            delta = 0;
        }

        hr->check_horse_health_drop_time();
        metin2::log::debug("Horse stamina consumed (event {})", static_cast<const void *>(event.get()));
        return delta;
    }

    EVENTFUNC(horse_stamina_regen_event) {
        horserider_info *info = dynamic_cast<horserider_info *>(event->info);

        if (info == nullptr) {
            metin2::log::error("Horse stamina regen event fired without event info");
            return 0;
        }

        horse_rider *hr = info->hr;

        if (hr->horse_health() <= 0) {
            hr->event_stamina_regen_ = nullptr;
            return 0;
        }

        hr->update_horse_stamina(+1);
        int delta = passes_per_sec(HORSE_STAMINA_REGEN_INTERVAL);
        if (hr->horse_stamina() == hr->horse_max_stamina()) {
            delta = 0;
            hr->event_stamina_regen_ = nullptr;
        }

        hr->check_horse_health_drop_time();
        metin2::log::debug("Horse stamina regenerated (event {})", static_cast<const void *>(event.get()));

        return delta;
    }

    void horse_rider::start_stamina_consume_event() {
        if (horse_level() <= 0)
            return;

        if (horse_health() <= 0)
            return;

        metin2::log::debug("Horse stamina regen event cancelled ({})", static_cast<const void *>(event_stamina_regen_.get()));
        event_cancel(&event_stamina_regen_);

        if (event_stamina_consume_)
            return;

        horserider_info *info = alloc_event_info<horserider_info>();

        info->hr = this;
        event_stamina_consume_ = event_create(horse_stamina_consume_event, info, passes_per_sec(HORSE_STAMINA_CONSUME_INTERVAL));
        metin2::log::debug("Horse stamina consume event created ({})", static_cast<const void *>(event_stamina_consume_.get()));
    }

    void horse_rider::start_stamina_regen_event() {
        if (horse_level() <= 0)
            return;

        if (horse_health() <= 0)
            return;

        metin2::log::debug("Horse stamina consume event cancelled ({})", static_cast<const void *>(event_stamina_consume_.get()));
        event_cancel(&event_stamina_consume_);

        if (event_stamina_regen_)
            return;

        horserider_info *info = alloc_event_info<horserider_info>();

        info->hr = this;
        event_stamina_regen_ = event_create(horse_stamina_regen_event, info, passes_per_sec(HORSE_STAMINA_REGEN_INTERVAL));
        metin2::log::debug("Horse stamina regen event created ({})", static_cast<const void *>(event_stamina_regen_.get()));
    }

    void horse_rider::reset_horse_health_drop_time() {
        horse_.horse_health_drop_time = get_global_time() + HORSE_HEALTH_DROP_INTERVAL;
    }

    void horse_rider::check_horse_health_drop_time(bool send) {
        uint32_t now = get_global_time();

        while (horse_.horse_health_drop_time < now) {
            horse_.horse_health_drop_time += HORSE_HEALTH_DROP_INTERVAL;
            update_horse_health(-1, send);
        }
    }

    void horse_rider::update_horse_health(int health, bool send) {
        int level = horse_level();

        horse_.s_health = std::clamp(horse_.s_health + health, 0, horse_stats[level].max_health);

        if (level && horse_.s_health == 0)
            horse_die();

        if (send)
            send_horse_info();
    }

    void horse_rider::horse_die() {
        metin2::log::debug("Horse died (regen event {}, consume event {})", static_cast<const void *>(event_stamina_regen_.get()), static_cast<const void *>(event_stamina_consume_.get()));
        update_horse_stamina(-horse_.s_stamina);
        event_cancel(&event_stamina_regen_);
        event_cancel(&event_stamina_consume_);
    }

    void horse_rider::set_horse_level(int level_value) {
        horse_.level = level_value = std::clamp(level_value, 0, HORSE_MAX_LEVEL);

        horse_.s_stamina = horse_stats[level_value].max_stamina;
        horse_.s_health = horse_stats[level_value].max_health;
        horse_.horse_health_drop_time = 0;

        reset_horse_health_drop_time();

        send_horse_info();
    }

    auto horse_rider::horse_grade() -> uint8_t {
        uint8_t grade = 0;

        if (horse_level())
            grade = (horse_level() - 1) / 10 + 1;

        return grade;
    }
}
