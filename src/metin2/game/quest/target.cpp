#include <metin2/game/quest/target.h>

#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/app/event.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/app/utils.h>

#include <metin2/log/log.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <list>

namespace metin2::game {

void send_target_create_packet(desc * d, target_info * info)
{
if (!info->send_to_client)
return;

packet_gc_target_create pck;

pck.header = HEADER_GC_TARGET_CREATE;
pck.id = info->id;
pck.type = info->type;
pck.vid = info->arg1;
strlcpy(pck.name, info->target_desc, sizeof(pck.name));
d->packet(&pck, sizeof(packet_gc_target_create));
}

void send_target_update_packet(desc * d, int id_value, int x, int y)
{
packet_gc_target_update pck;
pck.header = HEADER_GC_TARGET_UPDATE;
pck.id = id_value;
pck.x = x;
pck.y = y;
d->packet(&pck, sizeof(packet_gc_target_update));
metin2::log::info( "SendTargetUpdatePacket {:d} {:d}x{:d}", id_value, x, y);
}

void send_target_delete_packet(desc * d, int id_value)
{
packet_gc_target_delete pck;
pck.header = HEADER_GC_TARGET_DELETE;
pck.id = id_value;
d->packet(&pck, sizeof(packet_gc_target_delete));
}
target_manager::target_manager() : id_(0)
{
}

target_manager::~target_manager()
{
}

EVENTFUNC(target_event)
{
	target_info * info = dynamic_cast<target_info *>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "target_event> <Factor> Null pointer" );
		return 0;
	}

	character * pkChr = character_manager::instance().find_by_pid(info->pid);
	if (pkChr == nullptr) {
		return 0;
	}
	character * tch = nullptr;
	int x = 0, y = 0;
	int iDist = 5000;

	if (info->map_index != pkChr->map_index())
		return std::clamp(iDist / (1500 / metin2::config::server().passes_per_sec), metin2::config::server().passes_per_sec / 2, metin2::config::server().passes_per_sec * 5);

	switch (info->type)
	{
		case TARGET_TYPE_POS:
			x = info->arg1;
			y = info->arg2;
			iDist = distance_approx(pkChr->x() - x, pkChr->y() - y);
			break;

		case TARGET_TYPE_VID:
			{
				tch = character_manager::instance().find(info->arg1);

				if (tch && tch->map_index() == pkChr->map_index())
				{
					x = tch->x();
					y = tch->y();
					iDist = distance_approx(pkChr->x() - x, pkChr->y() - y);
				}
			}
			break;
	}

	bool bRet = true;

	if (iDist <= 500)
		bRet = quest::quest_manager::instance().target(pkChr->player_id(), info->quest_index, info->target_name, "arrive");

	if (!tch && info->type == TARGET_TYPE_VID)
	{
		quest::quest_manager::instance().target(pkChr->player_id(), info->quest_index, info->target_name, "die");
		target_manager::instance().delete_target(pkChr->player_id(), info->quest_index, info->target_name);
	}

	if (event->is_force_to_end)
	{
		metin2::log::info( "target_event: event canceled");
		return 0;
	}

	if (x != info->old_x || y != info->old_y)
	{
		if (info->send_to_client)
			send_target_update_packet(pkChr->get_desc(), info->id, x, y);

		info->old_x = x;
		info->old_y = y;
	}

	if (!bRet)
		return metin2::config::server().passes_per_sec;
	else
		return std::clamp(iDist / (1500 / metin2::config::server().passes_per_sec), metin2::config::server().passes_per_sec / 2, metin2::config::server().passes_per_sec * 5);
}

void target_manager::create_target(uint32_t pid_value,
		uint32_t quest_index_value,
		const char * target_name_value,
		int type_value,
		int arg1_value,
		int arg2_value,
		int map_index_value,
		const char * target_desc_value, 
		int send_flag)
{
	metin2::log::info( "CreateTarget : target pid {:d} quest {:d} name {} arg {:d} {:d} {:d}",
			pid_value, quest_index_value, target_name_value, type_value, arg1_value, arg2_value);

	character * pkChr = character_manager::instance().find_by_pid(pid_value);

	if (!pkChr)
	{
		metin2::log::error("Cannot find character ptr by PID {:d}", pid_value);
		return;
	}

	if (pkChr->map_index() != map_index_value)
		return;

	auto it = map_k_list_event_.find(pid_value);

	if (it != map_k_list_event_.end())
	{
		std::list<event_ptr>::const_iterator it2 = it->second.begin();

		while (it2 != it->second.end())
		{
			event_ptr pkEvent = *(it2++);
			target_info* existInfo = dynamic_cast<target_info*>(pkEvent->info);

			if (nullptr == existInfo)
			{
				metin2::log::error("CreateTarget : event already exist, but have no info");
				return;
			}

			if (existInfo->quest_index == quest_index_value && !strcmp(existInfo->target_name, target_name_value))
			{
				metin2::log::info( "CreateTarget : same target will be replaced");

				if (existInfo->send_to_client)
					send_target_delete_packet(pkChr->get_desc(), existInfo->id);

				if (target_desc_value)
				{
					strlcpy(existInfo->target_desc, target_desc_value, sizeof(existInfo->target_desc));
				}
				else
				{
					*existInfo->target_desc = '\0';
				}

				existInfo->id = ++id_;
				existInfo->type = type_value;
				existInfo->arg1 = arg1_value;
				existInfo->arg2 = arg2_value;
				existInfo->old_x = 0;
				existInfo->old_y = 0;
				existInfo->send_to_client = send_flag ? true : false;

				send_target_create_packet(pkChr->get_desc(), existInfo);
				return;
			}
		}
	}

	target_info* newInfo = alloc_event_info<target_info>();

	if (target_desc_value)
	{
		strlcpy(newInfo->target_desc, target_desc_value, sizeof(newInfo->target_desc));
	}
	else
	{
		*newInfo->target_desc = '\0';
	}

	newInfo->id = ++id_;
	newInfo->pid = pid_value;
	newInfo->quest_index = quest_index_value;
	strlcpy(newInfo->target_name, target_name_value, sizeof(newInfo->target_name));
	newInfo->type = type_value;
	newInfo->arg1 = arg1_value;
	newInfo->arg2 = arg2_value;
	newInfo->map_index = map_index_value;
	newInfo->old_x = 0;
	newInfo->old_y = 0;
	newInfo->send_to_client = send_flag ? true : false;

	event_ptr event = event_create(game::target_event, newInfo, passes_per_sec(1));

	if (nullptr != event)
	{
		map_k_list_event_[pid_value].push_back(event);

		send_target_create_packet(pkChr->get_desc(), newInfo);
	}
}

void target_manager::delete_target(uint32_t pid_value, uint32_t quest_index_value, const char * target_name_value)
{
	auto it = map_k_list_event_.find(pid_value);

	if (it == map_k_list_event_.end())
		return;

	std::list<event_ptr>::iterator it2 = it->second.begin();

	while (it2 != it->second.end())
	{
		event_ptr pkEvent = *it2;
		target_info * info = dynamic_cast<target_info*>(pkEvent->info);

		if ( info == nullptr )
		{
			metin2::log::error( "CTargetManager::DeleteTarget> <Factor> Null pointer" );
			++it2;
			continue;
		}

		if (quest_index_value == info->quest_index)
		{
			if (!target_name_value || !strcmp(info->target_name, target_name_value))
			{
				if (info->send_to_client) {
					character * pkChr = character_manager::instance().find_by_pid(info->pid);
					if (pkChr != nullptr) {
						send_target_delete_packet(pkChr->get_desc(), info->id);
					}
				}

				event_cancel(&pkEvent);
				it2 = it->second.erase(it2);
				continue;
			}
		}

		it2++;
	}
}

auto target_manager::target_event(uint32_t pid, uint32_t quest_index_value, const char * target_name_value) -> event_ptr
{
	auto it = map_k_list_event_.find(pid);

	if (it == map_k_list_event_.end())
		return nullptr;

	std::list<event_ptr>::iterator it2 = it->second.begin();

	while (it2 != it->second.end())
	{
		event_ptr pkEvent = *(it2++);
		target_info * info = dynamic_cast<target_info*>(pkEvent->info);

		if ( info == nullptr )
		{
			metin2::log::error( "CTargetManager::GetTargetEvent> <Factor> Null pointer" );

			continue;
		}

		if (info->quest_index != quest_index_value)
			continue;

		if (strcmp(info->target_name, target_name_value))
			continue;

		return pkEvent;
	}

	return nullptr;
}

auto target_manager::get_target_info(uint32_t pid, int type_value, int arg1_value) -> target_info *
{
	auto it = map_k_list_event_.find(pid);

	if (it == map_k_list_event_.end())
		return nullptr;

	std::list<event_ptr>::iterator it2 = it->second.begin();

	while (it2 != it->second.end())
	{
		event_ptr pkEvent = *(it2++);
		target_info * info = dynamic_cast<target_info*>(pkEvent->info);

		if ( info == nullptr )
		{
			metin2::log::error( "CTargetManager::GetTargetInfo> <Factor> Null pointer" );

			continue;
		}

		if (!is_set(info->type, type_value))
			continue;

		if (info->arg1 != arg1_value)
			continue;

		return info;
	}

	return nullptr;
}

void target_manager::logout(uint32_t pid)
{
	auto it = map_k_list_event_.find(pid);

	if (it == map_k_list_event_.end())
		return;

	std::list<event_ptr>::iterator it2 = it->second.begin();

	while (it2 != it->second.end())
		event_cancel(&(*(it2++)));

	map_k_list_event_.erase(it);
}
}
