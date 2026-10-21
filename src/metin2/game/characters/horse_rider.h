#pragma once

#include <metin2/game/app/event.h>

#include <metin2/proto/tables.h>

#include <cstdint>

namespace metin2::game::inline characters {
    class character;
}
namespace metin2::game::inline admin {
    void do_horse_set_stat(character *ch, const char *argument, int cmd, int subcmd);
}

namespace metin2::game::inline characters {

    const int HORSE_MAX_LEVEL = 30;

    struct horse_stat {
        int min_level;
        int npc_race;
        int max_health;
        int max_stamina;
        int st;
        int dx;
        int ht;
        int iq;
        int dam_mean;
        int dam_min;
        int dam_max;
        int armor;
    };

    extern horse_stat horse_stats[HORSE_MAX_LEVEL + 1];

    class horse_rider {
      public:
        horse_rider();
        virtual ~horse_rider();

        auto horse_level() const -> uint8_t { return horse_.level; }
        auto horse_grade() -> uint8_t;
        auto horse_health() const -> short { return horse_.s_health; }
        auto horse_stamina() const -> short { return horse_.s_stamina; }
        auto horse_max_health() -> short;
        auto horse_max_stamina() -> short;

        auto horse_st() -> int { return horse_stats[horse_level()].st; }
        auto horse_dx() -> int { return horse_stats[horse_level()].dx; }
        auto horse_ht() -> int { return horse_stats[horse_level()].ht; }
        auto horse_iq() -> int { return horse_stats[horse_level()].iq; }
        auto horse_armor() -> int { return horse_stats[horse_level()].armor; }

        virtual auto revive_horse() -> bool;
        void feed_horse();
        virtual void horse_die();

        auto is_horse_riding() const -> bool { return horse_.riding; }

        void reset_horse_health_drop_time();

        virtual void set_horse_level(int level_value);

        void enter_horse();

        virtual void send_horse_info() {}
        virtual void clear_horse_info() {}

        virtual void update_ride_time(int interval) {}

      protected:
        void set_horse_data(const horse_info &cr_info);
        auto horse_data() const -> const horse_info & { return horse_; }

        void update_horse_data_by_logoff(uint32_t logoff_time);

        virtual auto start_riding() -> bool;
        virtual auto stop_riding() -> bool;

        virtual auto my_horse_vnum() const -> uint32_t { return 20030; }

      private:
        void update_horse_stamina(int stamina, bool send = true);

        void start_stamina_consume_event();
        void start_stamina_regen_event();

        void update_horse_health(int health, bool send = true);
        void check_horse_health_drop_time(bool send = true);

        void initialize();
        void destroy();

        horse_info horse_;

        event_ptr event_stamina_regen_;
        event_ptr event_stamina_consume_;

        friend EVENTFUNC(horse_stamina_regen_event);
        friend EVENTFUNC(horse_stamina_consume_event);
        friend void admin::do_horse_set_stat(character *ch, const char *argument, int cmd, int subcmd);
    };
}
