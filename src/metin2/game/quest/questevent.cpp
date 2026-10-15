#include <metin2/game/quest/questevent.h>

#include <metin2/game/characters/char_manager.h>
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
	void CancelTimerEvent(LPEVENT * ppEvent)
	{
		quest_event_info* info = dynamic_cast<quest_event_info*>((*ppEvent)->info);

		if (info)
		{
			delete[] info->name;
			info->name = nullptr;
		}

		event_cancel(ppEvent);
	}

	EVENTFUNC(quest_server_timer_event)
	{
		quest_server_event_info * info = dynamic_cast<quest_server_event_info *>( event->info );

		if ( info == nullptr )
		{
			metin2::log::error( "quest_server_timer_event> <Factor> Null pointer" );
			return 0;
		}

		CQuestManager& q = CQuestManager::instance();

		if (!q.ServerTimer(info->npc_id, info->arg))
			return metin2::config::server().passes_per_sec / 2 + 1;

		if (0 == info->time_cycle)	// Terminate if not looping.
		{
			q.ClearServerTimerNotCancel(info->name, info->arg);
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

		CQuestManager & q = CQuestManager::instance();

		if (CHARACTER_MANAGER::instance().FindByPID(info->player_id))
		{
			if (!CQuestManager::instance().Timer(info->player_id, info->npc_id))
				return (metin2::config::server().passes_per_sec / 2 + 1);

			if (0 == info->time_cycle)	// Terminate if not looping.
				goto END_OF_TIMER_EVENT;
		}
		else
		{
END_OF_TIMER_EVENT:
			PC * pPC = q.GetPC(info->player_id);

			if (pPC)
				pPC->RemoveTimerNotCancel(info->name);
			else
				metin2::log::error("quest::PC pointer null. player_id: {:d}", info->player_id);

			delete[] info->name;
			info->name = nullptr;
			return 0;
		}

		return info->time_cycle;
	}

	LPEVENT quest_create_server_timer_event(const char * name, double when, unsigned int timernpc, bool loop, unsigned int arg)
	{
		const int nameCapacity = strlen(name) + 1;

		int32_t ltime_cycle = (int32_t) (rint(PASSES_PER_SEC(when)));

		quest_server_event_info* info = AllocEventInfo<quest_server_event_info>();

		info->npc_id = timernpc;
		info->time_cycle = loop ? ltime_cycle : 0;
		info->arg = arg;
		info->name		= new char[nameCapacity];

		if (info->name)
			strlcpy(info->name, name, nameCapacity);

		return event_create(quest_server_timer_event, info, ltime_cycle);
	}

	LPEVENT quest_create_timer_event(const char * name, unsigned int player_id, double when, unsigned int npc_id, bool loop)
	{
		const int nameCapacity = strlen(name) + 1;

		int32_t ltime_cycle = (int) (rint(PASSES_PER_SEC(when)));

		quest_event_info* info = AllocEventInfo<quest_event_info>();

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
