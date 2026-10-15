#include <metin2/game/quest/questevent.h>

#include <metin2/game/characters/character_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/quest/questpc.h>

#include <metin2/log/log.h>

#include <cmath>
#include <cstdint>
#include <cstring>

namespace metin2::game {

namespace quest
{
	void cancel_timer_event(event_ptr * pp_event)
	{
		quest_event_info* info = dynamic_cast<quest_event_info*>((*pp_event)->info);

		if (info)
		{
			delete[] info->name;
			info->name = nullptr;
		}

		event_cancel(pp_event);
	}

	EVENTFUNC(quest_server_timer_event)
	{
		quest_server_event_info * info = dynamic_cast<quest_server_event_info *>( event->info );

		if ( info == nullptr )
		{
			metin2::log::error( "quest_server_timer_event> <Factor> Null pointer" );
			return 0;
		}

		quest_manager& q = quest_manager::instance();

		if (!q.server_timer(info->npc_id, info->arg))
			return metin2::config::server().passes_per_sec / 2 + 1;

		if (0 == info->time_cycle)
		{
			q.clear_server_timer_not_cancel(info->name, info->arg);
			delete[] info->name;
			info->name = nullptr;
		}

		return info->time_cycle;
	}

	EVENTFUNC(quest_timer_event)
	{
		quest_event_info * info = dynamic_cast<quest_event_info *>( event->info );

		if ( info == nullptr )
		{
			metin2::log::error( "quest_timer_event> <Factor> Null pointer" );
			return 0;
		}

		quest_manager & q = quest_manager::instance();

		if (character_manager::instance().find_by_pid(info->player_id))
		{
			if (!quest_manager::instance().timer(info->player_id, info->npc_id))
				return (metin2::config::server().passes_per_sec / 2 + 1);

			if (0 == info->time_cycle)
				goto END_OF_TIMER_EVENT;
		}
		else
		{
END_OF_TIMER_EVENT:
			pc * pPC = q.get_pc(info->player_id);

			if (pPC)
				pPC->remove_timer_not_cancel(info->name);
			else
				metin2::log::error("quest::PC pointer null. player_id: {:d}", info->player_id);

			delete[] info->name;
			info->name = nullptr;
			return 0;
		}

		return info->time_cycle;
	}

	auto quest_create_server_timer_event(const char * name, double when, unsigned int timernpc, bool loop, unsigned int arg) -> event_ptr
	{
		const int nameCapacity = strlen(name) + 1;

		int32_t ltime_cycle = (int32_t) (rint(passes_per_sec(when)));

		quest_server_event_info* info = alloc_event_info<quest_server_event_info>();

		info->npc_id = timernpc;
		info->time_cycle = loop ? ltime_cycle : 0;
		info->arg = arg;
		info->name		= new char[nameCapacity];

		if (info->name)
			strlcpy(info->name, name, nameCapacity);

		return event_create(quest_server_timer_event, info, ltime_cycle);
	}

	auto quest_create_timer_event(const char * name, unsigned int player_id, double when, unsigned int npc_id, bool loop) -> event_ptr
	{
		const int nameCapacity = strlen(name) + 1;

		int32_t ltime_cycle = (int) (rint(passes_per_sec(when)));

		quest_event_info* info = alloc_event_info<quest_event_info>();

		info->player_id		= player_id;
		info->npc_id		= npc_id;
		info->name		= new char[nameCapacity];

		if (info->name)
			strlcpy(info->name, name, nameCapacity);

		metin2::log::info( "QUEST timer name {} cycle {:d} pc {:d} npc {:d} loop? {:d}", name ? name : "<noname>", ltime_cycle, player_id, npc_id, loop ? 1 : 0);

		info->time_cycle	= loop ? ltime_cycle : 0;
		return event_create(quest_timer_event, info, ltime_cycle);
	}
}
}
