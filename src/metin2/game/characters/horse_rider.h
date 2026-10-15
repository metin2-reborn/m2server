#pragma once

#include <metin2/game/admin/cmd.h>
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

struct horse_stat
{
	int min_level;      // Minimum level to ride
	int npc_race;
	int max_health;     // Horse max HP
	int max_stamina;    // Horse max stamina
	int st;
	int dx;
	int ht;
	int iq;
	int dam_mean;
	int dam_min;
	int dam_max;
	int armor;
};

extern horse_stat c_a_horse_stat[HORSE_MAX_LEVEL+1];

class horse_rider
{
	public:
		horse_rider();
		virtual ~horse_rider();
		
		uint8_t		horse_level() const { return horse_.level; }
		uint8_t		horse_grade();
		short		horse_health() const	{ return horse_.s_health; }
		short		horse_stamina() const	{ return horse_.s_stamina; }
		short		horse_max_health();
		short		horse_max_stamina();

		int		horse_st()		{ return c_a_horse_stat[horse_level()].st; }
		int		horse_dx()		{ return c_a_horse_stat[horse_level()].dx; }
		int		horse_ht()		{ return c_a_horse_stat[horse_level()].ht; }
		int		horse_iq()		{ return c_a_horse_stat[horse_level()].iq; }
		int		horse_armor()		{ return c_a_horse_stat[horse_level()].armor; }

		virtual bool revive_horse();
		void feed_horse();
		virtual void horse_die();

		bool is_horse_riding() const		{ return horse_.riding; }

		void reset_horse_health_drop_time();

		virtual void set_horse_level(int level_value);

		void enter_horse();

		virtual void send_horse_info() {}
		virtual void clear_horse_info() {}

		virtual void update_ride_time(int interval) {}

	protected:
		void set_horse_data(const horse_info& cr_info);
		const horse_info& horse_data() const { return horse_; }

		void update_horse_data_by_logoff(uint32_t logoff_time);

		virtual bool start_riding();
		virtual bool stop_riding();

		virtual	uint32_t my_horse_vnum() const { return 20030; }

	private:
		void update_horse_stamina(int stamina, bool send = true);

		void start_stamina_consume_event();
		void start_stamina_regen_event();

		void update_horse_health(int health, bool send = true);
		void check_horse_health_drop_time(bool send = true);

		void initialize();
		void destroy();

		horse_info horse_;

		event_ptr	event_stamina_regen_;
		event_ptr	event_stamina_consume_;

		friend EVENTFUNC(horse_stamina_regen_event);
		friend EVENTFUNC(horse_stamina_consume_event);
		friend void admin::do_horse_set_stat(character *ch, const char *argument, int cmd, int subcmd);
};
}
