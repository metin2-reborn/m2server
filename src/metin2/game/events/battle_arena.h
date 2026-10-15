#pragma once

#include <metin2/game/app/event.h>

#include <metin2/common/singleton.h>

#include <string>

namespace metin2::game::inline events {

const static int battle_arena_map[] = { 0, 190, 191, 192 };
const static std::string str_regen[] =
{
	"",
	"/metin2_map_battlearena01/regen00.txt",
	"/metin2_map_battlearena02/regen00.txt",
	"/metin2_map_battlearena03/regen00.txt",
};

enum battlearena_status
{
	STATUS_CLOSE = 0,
	STATUS_BATTLE,
	STATUS_END,
};

class battle_arena : public singleton<battle_arena>
{
	private :
		event_ptr event_;
		battlearena_status status_;
		int map_index_;
		int empire_;
		bool force_end_;

	public :
		battle_arena();

		static bool is_battle_arena_map(int nMapIndex);

		bool is_running();
		void set_status(battlearena_status status);

		bool start(int nEmpire);
		void force_end();
		void end();

		void spawn_last_boss();
		void spawn_random_stone();
};
}
