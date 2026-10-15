#include <metin2/game/events/arena.h>

#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/characters/char.h>
#include <metin2/game/characters/char_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/app/event.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/empire/start_position.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/log/log.h>

#include <cstdint>
#include <memory>
#include <utility>

namespace metin2::game::inline events {

arena::arena(uint16_t startA_X, uint16_t startA_Y, uint16_t startB_X, uint16_t startB_Y)
{
	start_point_a_.x = startA_X;
	start_point_a_.y = startA_Y;
	start_point_a_.z = 0;

	start_point_b_.x = startB_X;
	start_point_b_.y = startB_Y;
	start_point_b_.z = 0;

	observer_point_.x = (startA_X + startB_X) / 2;
	observer_point_.y = (startA_Y + startB_Y) / 2;
	observer_point_.z = 0;

	event_ = nullptr;
	time_out_event_ = nullptr;

	clear();
}

void arena::clear()
{
	pida_ = 0;
	pidb_ = 0;

	if (event_ != nullptr)
	{
		event_cancel(&event_);
	}

	if (time_out_event_ != nullptr)
	{
		event_cancel(&time_out_event_);
	}

	set_count_ = 0;
	set_point_of_a_ = 0;
	set_point_of_b_ = 0;
}

bool arena_manager::add_arena(uint32_t mapIdx, uint16_t startA_X, uint16_t startA_Y, uint16_t startB_X, uint16_t startB_Y)
{
	arena_map *pArenaMap = nullptr;
	auto iter = arena_map_.find(mapIdx);

	if (iter == arena_map_.end())
	{
		pArenaMap = arena_map_.emplace(mapIdx, std::make_unique<arena_map>()).first->second.get();
	}
	else
	{
		pArenaMap = iter->second.get();
	}

	if (pArenaMap->add_arena(mapIdx, startA_X, startA_Y, startB_X, startB_Y) == false)
	{
		metin2::log::info( "CArenaManager::AddArena - AddMap Error MapID: {:d}", mapIdx);
		return false;
	}

	return true;
}

bool arena_map::add_arena(uint32_t mapIdx, uint16_t startA_X, uint16_t startA_Y, uint16_t startB_X, uint16_t startB_Y)
{
	auto iter = arena_.begin();

	for (; iter != arena_.end(); iter++)
	{
		if ((*iter)->check_area(startA_X, startA_Y, startB_X, startB_Y) == false)
		{
			metin2::log::info( "CArenaMap::AddArena - Same Start Position set. stA({:d}, {:d}) stB({:d}, {:d})", startA_X, startA_Y, startB_X, startB_Y);
			return false;
		}
	}

	map_index_ = mapIdx;

	arena_.push_back(std::unique_ptr<arena>(new arena(startA_X, startA_Y, startB_X, startB_Y)));

	return true;
}

void arena_manager::destroy()
{
	auto iter = arena_map_.begin();

	for (; iter != arena_map_.end(); iter++)
	{
		arena_map* pArenaMap = iter->second.get();
		pArenaMap->destroy();
	}
	arena_map_.clear();
}

void arena_map::destroy()
{
	auto iter = arena_.begin();

	metin2::log::info( "ARENA: ArenaMap will be destroy. mapIndex({:d})", map_index_);

	for (; iter != arena_.end(); iter++)
	{
		arena* pArena = iter->get();
		pArena->end_duel();
	}
	arena_.clear();
}

bool arena::check_area(uint16_t startA_X, uint16_t startA_Y, uint16_t startB_X, uint16_t startB_Y)
{
	if (start_point_a_.x == startA_X && start_point_a_.y == startA_Y &&
			start_point_b_.x == startB_X && start_point_b_.y == startB_Y)
		return false;	
	return true;
}

void arena_manager::send_arena_map_list_to(character * pChar)
{
	auto iter = arena_map_.begin();

	for (; iter != arena_map_.end(); iter++)
	{
		arena_map* pArena = iter->second.get();
		pArena->send_arena_map_list_to(pChar, (iter->first));
	}
}

void arena_map::send_arena_map_list_to(character * pChar, uint32_t mapIdx)
{
	if (pChar == nullptr) return;

	auto iter = arena_.begin();

	for (; iter != arena_.end(); iter++)
	{
		pChar->chat_packet(CHAT_TYPE_INFO, "ArenaMapInfo Map: %d stA(%d, %d) stB(%d, %d)", mapIdx, 
				static_cast<int>((*iter)->start_point_a().x), static_cast<int>((*iter)->start_point_a().y),
				static_cast<int>((*iter)->start_point_b().x), static_cast<int>((*iter)->start_point_b().y));
	}
}

bool arena_manager::start_duel(character * pCharFrom, character * pCharTo, int nSetPoint, int nMinute)
{
	if (pCharFrom == nullptr || pCharTo == nullptr) return false;

	auto iter = arena_map_.begin();

	for (; iter != arena_map_.end(); iter++)
	{
		arena_map* pArenaMap = iter->second.get();
		if (pArenaMap->start_duel(pCharFrom, pCharTo, nSetPoint, nMinute) == true)
		{
			return true;
		}
	}

	return false;
}

bool arena_map::start_duel(character * pCharFrom, character * pCharTo, int nSetPoint, int nMinute)
{
	auto iter = arena_.begin();

	for (; iter != arena_.end(); iter++)
	{
		arena* pArena = iter->get();
		if (pArena->is_empty() == true)
		{
			return pArena->start_duel(pCharFrom, pCharTo, nSetPoint, nMinute);
		}
	}

	return false;
}

EVENTINFO(arena_event_info)
{
	game::events::arena *arena;
	uint8_t state;

	arena_event_info()
	: arena(nullptr)
	, state(0)
	{
	}
};

EVENTFUNC(ready_to_start_event)
{
	if (event == nullptr)
		return 0;

	if (event->info == nullptr)
		return 0;

	arena_event_info* info = dynamic_cast<arena_event_info*>(event->info);

	if ( info == nullptr )
	{
		metin2::log::error( "ready_to_start_event> <Factor> Null pointer" );
		return 0;
	}

	arena* pArena = info->arena;

	if (pArena == nullptr)
	{
		metin2::log::error("ARENA: Arena start event info is null.");
		return 0;
	}

	character * chA = pArena->player_a();
	character * chB = pArena->player_b();

	if (chA == nullptr || chB == nullptr)
	{
		metin2::log::error("ARENA: Player err in event func ready_start_event");

		if (chA != nullptr)
		{
			chA->chat_packet(CHAT_TYPE_INFO, lc_text("대련 상대가 사라져 대련을 종료합니다."));
			metin2::log::info( "ARENA: Oppernent is disappered. MyPID({:d}) OppPID({:d})", pArena->player_apid(), pArena->player_bpid());
		}

		if (chB != nullptr)
		{
			chB->chat_packet(CHAT_TYPE_INFO, lc_text("대련 상대가 사라져 대련을 종료합니다."));
			metin2::log::info( "ARENA: Oppernent is disappered. MyPID({:d}) OppPID({:d})", pArena->player_bpid(), pArena->player_apid());
		}

		pArena->send_chat_packet_to_observer(CHAT_TYPE_NOTICE, lc_text("대련 상대가 사라져 대련을 종료합니다."));

		pArena->end_duel();
		return 0;
	}

	switch (info->state)
	{
		case 0:
			{
				chA->set_arena(pArena);
				chB->set_arena(pArena);

				int count = quest::quest_manager::instance().event_flag("arena_potion_limit_count");

				if (count > 10000)
				{
					chA->chat_packet(CHAT_TYPE_INFO, lc_text("물약 제한이 없습니다."));
					chB->chat_packet(CHAT_TYPE_INFO, lc_text("물약 제한이 없습니다."));
				}
				else
				{
					chA->set_potion_limit(count);
					chB->set_potion_limit(count);

					chA->chat_packet(CHAT_TYPE_INFO, lc_text("물약을 %d 개 까지 사용 가능합니다."), chA->potion_limit());
					chB->chat_packet(CHAT_TYPE_INFO, lc_text("물약을 %d 개 까지 사용 가능합니다."), chB->potion_limit());
				}
				chA->chat_packet(CHAT_TYPE_INFO, lc_text("10초뒤 대련이 시작됩니다."));
				chB->chat_packet(CHAT_TYPE_INFO, lc_text("10초뒤 대련이 시작됩니다."));
				pArena->send_chat_packet_to_observer(CHAT_TYPE_INFO, lc_text("10초뒤 대련이 시작됩니다."));

				info->state++;
				return passes_per_sec(10);
			}
			break;

		case 1:
			{
				chA->chat_packet(CHAT_TYPE_INFO, lc_text("대련이 시작되었습니다."));
				chB->chat_packet(CHAT_TYPE_INFO, lc_text("대련이 시작되었습니다."));
				pArena->send_chat_packet_to_observer(CHAT_TYPE_INFO, lc_text("대련이 시작되었습니다."));

				packet_gc_duel_start duelStart;
				duelStart.header = HEADER_GC_DUEL_START;
				duelStart.size = sizeof(packet_gc_duel_start) + 4;

				uint32_t dwOppList[8]; // Since the max party size is 8..

				dwOppList[0] = (uint32_t)chB->get_vid();
				temp_buffer buf;

				buf.write(&duelStart, sizeof(packet_gc_duel_start));
				buf.write(&dwOppList[0], 4);
				chA->get_desc()->packet(buf.read_peek(), buf.size());


				dwOppList[0] = (uint32_t)chA->get_vid();
				temp_buffer buf2;

				buf2.write(&duelStart, sizeof(packet_gc_duel_start));
				buf2.write(&dwOppList[0], 4);
				chB->get_desc()->packet(buf2.read_peek(), buf2.size());

				return 0;
			}
			break;

		case 2:
			{
				pArena->end_duel();
				return 0;
			}
			break;

		case 3:
			{
				chA->show(chA->map_index(), pArena->start_point_a().x * 100, pArena->start_point_a().y * 100);
				chB->show(chB->map_index(), pArena->start_point_b().x * 100, pArena->start_point_b().y * 100);

				chA->get_desc()->set_phase(PHASE_GAME);
				chA->start_recovery_event();
				chA->set_position(POS_STANDING);
				chA->point_change(POINT_HP, chA->max_hp() - chA->hp());
				chA->point_change(POINT_SP, chA->max_sp() - chA->sp());
				chA->view_reencode();

				chB->get_desc()->set_phase(PHASE_GAME);
				chB->start_recovery_event();
				chB->set_position(POS_STANDING);
				chB->point_change(POINT_HP, chB->max_hp() - chB->hp());
				chB->point_change(POINT_SP, chB->max_sp() - chB->sp());
				chB->view_reencode();

				temp_buffer buf;
				temp_buffer buf2;
				uint32_t dwOppList[8]; // Since the max party size is 8..
				packet_gc_duel_start duelStart;
				duelStart.header = HEADER_GC_DUEL_START;
				duelStart.size = sizeof(packet_gc_duel_start) + 4;

				dwOppList[0] = (uint32_t)chB->get_vid();
				buf.write(&duelStart, sizeof(packet_gc_duel_start));
				buf.write(&dwOppList[0], 4);
				chA->get_desc()->packet(buf.read_peek(), buf.size());

				dwOppList[0] = (uint32_t)chA->get_vid();
				buf2.write(&duelStart, sizeof(packet_gc_duel_start));
				buf2.write(&dwOppList[0], 4);
				chB->get_desc()->packet(buf2.read_peek(), buf2.size());

				chA->chat_packet(CHAT_TYPE_INFO, lc_text("대련이 시작되었습니다."));
				chB->chat_packet(CHAT_TYPE_INFO, lc_text("대련이 시작되었습니다."));
				pArena->send_chat_packet_to_observer(CHAT_TYPE_INFO, lc_text("대련이 시작되었습니다."));

				pArena->clear_event();

				return 0;
			}
			break;

		default:
			{
				chA->chat_packet(CHAT_TYPE_INFO, lc_text("대련장 문제로 인하여 대련을 종료합니다."));
				chB->chat_packet(CHAT_TYPE_INFO, lc_text("대련장 문제로 인하여 대련을 종료합니다."));
				pArena->send_chat_packet_to_observer(CHAT_TYPE_INFO, lc_text("대련장 문제로 인하여 대련을 종료합니다."));

				metin2::log::info( "ARENA: Something wrong in event func. info->state({:d})", info->state);

				pArena->end_duel();

				return 0;
			}
	}
}

EVENTFUNC(duel_time_out)
{
	if (event == nullptr) return 0;
	if (event->info == nullptr) return 0;

	arena_event_info* info = dynamic_cast<arena_event_info*>(event->info);

	if ( info == nullptr )
	{
		metin2::log::error( "duel_time_out> <Factor> Null pointer" );
		return 0;
	}

	arena* pArena = info->arena;

	if (pArena == nullptr)
	{
		metin2::log::error("ARENA: Time out event error");
		return 0;
	}

	character * chA = pArena->player_a();
	character * chB = pArena->player_b();

	if (chA == nullptr || chB == nullptr)
	{
		if (chA != nullptr)
		{
			chA->chat_packet(CHAT_TYPE_INFO, lc_text("대련 상대가 사라져 대련을 종료합니다."));
			metin2::log::info( "ARENA: Oppernent is disappered. MyPID({:d}) OppPID({:d})", pArena->player_apid(), pArena->player_bpid());
		}

		if (chB != nullptr)
		{
			chB->chat_packet(CHAT_TYPE_INFO, lc_text("대련 상대가 사라져 대련을 종료합니다."));
			metin2::log::info( "ARENA: Oppernent is disappered. MyPID({:d}) OppPID({:d})", pArena->player_bpid(), pArena->player_apid());
		}

		pArena->send_chat_packet_to_observer(CHAT_TYPE_INFO, lc_text("대련 상대가 사라져 대련을 종료합니다."));

		pArena->end_duel();
		return 0;
	}
	else
	{
		switch (info->state)
		{
			case 0:
				pArena->send_chat_packet_to_observer(CHAT_TYPE_NOTICE, lc_text("대련 시간 초과로 대련을 중단합니다."));
				pArena->send_chat_packet_to_observer(CHAT_TYPE_NOTICE, lc_text("10초뒤 마을로 이동합니다."));

				chA->chat_packet(CHAT_TYPE_NOTICE, lc_text("대련 시간 초과로 대련을 중단합니다."));
				chA->chat_packet(CHAT_TYPE_NOTICE, lc_text("10초뒤 마을로 이동합니다."));

				chB->chat_packet(CHAT_TYPE_NOTICE, lc_text("대련 시간 초과로 대련을 중단합니다."));
				chB->chat_packet(CHAT_TYPE_NOTICE, lc_text("10초뒤 마을로 이동합니다."));

				packet_gc_duel_start duelStart;
				duelStart.header = HEADER_GC_DUEL_START;
				duelStart.size = sizeof(packet_gc_duel_start);

				chA->get_desc()->packet(&duelStart, sizeof(packet_gc_duel_start));
				chA->get_desc()->packet(&duelStart, sizeof(packet_gc_duel_start));

				info->state++;

				metin2::log::info( "ARENA: Because of time over, duel is end. PIDA({:d}) vs PIDB({:d})", pArena->player_apid(), pArena->player_bpid());

				return passes_per_sec(10);
				break;

			case 1:
				pArena->end_duel();
				break;
		}
	}

	return 0; 
}

bool arena::start_duel(character * pCharFrom, character * pCharTo, int nSetPoint, int nMinute)
{
	this->pida_ = pCharFrom->player_id();
	this->pidb_ = pCharTo->player_id();
	this->set_count_ = nSetPoint;

	pCharFrom->warp_set(start_point_a().x * 100, start_point_a().y * 100);
	pCharTo->warp_set(start_point_b().x * 100, start_point_b().y * 100);

	if (event_ != nullptr) {
		event_cancel(&event_);
	}

	arena_event_info* info = alloc_event_info<arena_event_info>();

	info->arena = this;
	info->state = 0;

	event_ = event_create(ready_to_start_event, info, passes_per_sec(10));

	if (time_out_event_ != nullptr) {
		event_cancel(&time_out_event_);
	}

	info = alloc_event_info<arena_event_info>();

	info->arena = this;
	info->state = 0;

	time_out_event_ = event_create(duel_time_out, info, passes_per_sec(nMinute*60));

	pCharFrom->point_change(POINT_HP, pCharFrom->max_hp() - pCharFrom->hp());
	pCharFrom->point_change(POINT_SP, pCharFrom->max_sp() - pCharFrom->sp());

	pCharTo->point_change(POINT_HP, pCharTo->max_hp() - pCharTo->hp());
	pCharTo->point_change(POINT_SP, pCharTo->max_sp() - pCharTo->sp());

	metin2::log::info( "ARENA: Start Duel with PID_A({:d}) vs PID_B({:d})", player_apid(), player_bpid());
	return true;
}

void arena_manager::end_all_duel()
{
	auto iter = arena_map_.begin();

	for (; iter != arena_map_.end(); iter++)
	{
		arena_map *pArenaMap = iter->second.get();
		if (pArenaMap != nullptr)
			pArenaMap->end_all_duel();
	}

	return;
}

void arena_map::end_all_duel()
{
	auto iter = arena_.begin();

	for (; iter != arena_.end(); iter++)
	{
		arena *pArena = iter->get();
		if (pArena != nullptr)
			pArena->end_duel();
	}
}

void arena::end_duel()
{
	if (event_ != nullptr) {
		event_cancel(&event_);
	}
	if (time_out_event_ != nullptr) {
		event_cancel(&time_out_event_);
	}

	character * playerA = player_a();
	character * playerB = player_b();

	if (playerA != nullptr)
	{
		playerA->set_pk_mode(PK_MODE_PEACE);
		playerA->start_recovery_event();
		playerA->set_position(POS_STANDING);
		playerA->point_change(POINT_HP, playerA->max_hp() - playerA->hp());
		playerA->point_change(POINT_SP, playerA->max_sp() - playerA->sp());

		playerA->set_arena(nullptr);

		playerA->warp_set(arena_return_point_x(playerA->empire()), arena_return_point_y(playerA->empire()));
	}

	if (playerB != nullptr)
	{
		playerB->set_pk_mode(PK_MODE_PEACE);
		playerB->start_recovery_event();
		playerB->set_position(POS_STANDING);
		playerB->point_change(POINT_HP, playerB->max_hp() - playerB->hp());
		playerB->point_change(POINT_SP, playerB->max_sp() - playerB->sp());

		playerB->set_arena(nullptr);

		playerB->warp_set(arena_return_point_x(playerB->empire()), arena_return_point_y(playerB->empire()));
	}

	auto iter = observer_.begin();

	for (; iter != observer_.end(); iter++)
	{
		character * pChar = character_manager::instance().find_by_pid(iter->first);
		if (pChar != nullptr)
		{
			pChar->warp_set(arena_return_point_x(pChar->empire()), arena_return_point_y(pChar->empire()));
		}
	}

	observer_.clear();

	metin2::log::info( "ARENA: End Duel PID_A({:d}) vs PID_B({:d})", player_apid(), player_bpid());

	clear();
}

void arena_manager::duel_list(lua_State* L)
{
	auto iter = arena_map_.begin();

	int index = 1;
	lua_newtable(L);

	for (; iter != arena_map_.end(); iter++)
	{
		arena_map* pArenaMap = iter->second.get();
		if (pArenaMap != nullptr)
			index = pArenaMap->duel_list(L, index);
	}
}

int arena_map::duel_list(lua_State* L, int index)
{
	auto iter = arena_.begin();

	for (; iter != arena_.end(); iter++)
	{
		arena* pArena = iter->get();

		if (pArena == nullptr) continue;

		if (pArena->is_empty() == false)
		{
			character * chA = pArena->player_a();
			character * chB = pArena->player_b();

			if (chA != nullptr && chB != nullptr)
			{
				lua_newtable(L);

				lua_pushstring(L, chA->name());
				lua_rawseti(L, -2, 1);

				lua_pushstring(L, chB->name());
				lua_rawseti(L, -2, 2);

				lua_pushnumber(L, map_index_);
				lua_rawseti(L, -2, 3);

				lua_pushnumber(L, pArena->observer_point().x);
				lua_rawseti(L, -2, 4);

				lua_pushnumber(L, pArena->observer_point().y);
				lua_rawseti(L, -2, 5);

				lua_rawseti(L, -2, index++);
			}
		}
	}

	return index;
}

bool arena_manager::can_attack(character * pCharAttacker, character * pCharVictim)
{
	if (pCharAttacker == nullptr || pCharVictim == nullptr) return false;

	if (pCharAttacker == pCharVictim) return false;

	int32_t mapIndex = pCharAttacker->map_index();
	if (mapIndex != pCharVictim->map_index()) return false;

	auto iter = arena_map_.find(mapIndex);

	if (iter == arena_map_.end()) return false;

	arena_map* pArenaMap = iter->second.get();
	return pArenaMap->can_attack(pCharAttacker, pCharVictim);
}

bool arena_map::can_attack(character * pCharAttacker, character * pCharVictim)
{
	if (pCharAttacker == nullptr || pCharVictim == nullptr) return false;

	uint32_t dwPIDA = pCharAttacker->player_id();
	uint32_t dwPIDB = pCharVictim->player_id();

	auto iter = arena_.begin();

	for (; iter != arena_.end(); iter++)
	{
		arena* pArena = iter->get();
		if (pArena->can_attack(dwPIDA, dwPIDB) == true)
		{
			return true;
		}
	}
	return false;
}

bool arena::can_attack(uint32_t dwPIDA, uint32_t dwPIDB)
{
	// 1:1 only; needs changes for many-vs-many
	if (pida_ == dwPIDA && pidb_ == dwPIDB) return true;
	if (pida_ == dwPIDB && pidb_ == dwPIDA) return true;

	return false;
}

bool arena_manager::on_dead(character * pCharKiller, character * pCharVictim)
{
	if (pCharKiller == nullptr || pCharVictim == nullptr) return false;

	int32_t mapIndex = pCharKiller->map_index();
	if (mapIndex != pCharVictim->map_index()) return false;

	auto iter = arena_map_.find(mapIndex);
	if (iter == arena_map_.end()) return false;

	arena_map* pArenaMap = iter->second.get();
	return pArenaMap->on_dead(pCharKiller,  pCharVictim);
}

bool arena_map::on_dead(character * pCharKiller, character * pCharVictim)
{
	uint32_t dwPIDA = pCharKiller->player_id();
	uint32_t dwPIDB = pCharVictim->player_id();

	auto iter = arena_.begin();

	for (; iter != arena_.end(); iter++)
	{
		arena* pArena = iter->get();

		if (pArena->is_member(dwPIDA) == true && pArena->is_member(dwPIDB) == true)
		{
			pArena->on_dead(dwPIDA, dwPIDB);
			return true;
		}
	}
	return false;
}

bool arena::on_dead(uint32_t dwPIDA, uint32_t dwPIDB)
{
	bool restart = false;

	character * pCharA = player_a();
	character * pCharB = player_b();

	if (pCharA == nullptr && pCharB == nullptr)
	{
		// Both disconnected?!
		send_chat_packet_to_observer(CHAT_TYPE_NOTICE, lc_text("대련자 문제로 인하여 대련을 중단합니다."));
		restart = false;
	}
	else if (pCharA == nullptr && pCharB != nullptr)
	{
		pCharB->chat_packet(CHAT_TYPE_NOTICE, lc_text("상대방 캐릭터의 문제로 인하여 대련을 종료합니다."));
		send_chat_packet_to_observer(CHAT_TYPE_NOTICE, lc_text("대련자 문제로 인하여 대련을 종료합니다."));
		restart = false;
	}
	else if (pCharA != nullptr && pCharB == nullptr)
	{
		pCharA->chat_packet(CHAT_TYPE_NOTICE, lc_text("상대방 캐릭터의 문제로 인하여 대련을 종료합니다."));
		send_chat_packet_to_observer(CHAT_TYPE_NOTICE, lc_text("대련자 문제로 인하여 대련을 종료합니다."));
		restart = false;
	}
	else if (pCharA != nullptr && pCharB != nullptr)
	{
		if (pida_ == dwPIDA)
		{
			set_point_of_a_++;

			if (set_point_of_a_ >= set_count_)
			{
				pCharA->chat_packet(CHAT_TYPE_NOTICE, lc_text("%s 님이 대련에서 승리하였습니다."), pCharA->name());
				pCharB->chat_packet(CHAT_TYPE_NOTICE, lc_text("%s 님이 대련에서 승리하였습니다."), pCharA->name());
				send_chat_packet_to_observer(CHAT_TYPE_NOTICE, lc_text("%s 님이 대련에서 승리하였습니다."), pCharA->name());

				metin2::log::info( "ARENA: Duel is end. Winner {}({:d}) Loser {}({:d})",
						pCharA->name(), player_apid(), pCharB->name(), player_bpid());
			}
			else
			{
				restart = true;
				pCharA->chat_packet(CHAT_TYPE_INFO, lc_text("%s 님이 승리하였습니다."), pCharA->name());
				pCharA->chat_packet(CHAT_TYPE_NOTICE, "%s %d : %d %s", pCharA->name(), set_point_of_a_, set_point_of_b_, pCharB->name());

				pCharB->chat_packet(CHAT_TYPE_INFO, lc_text("%s 님이 승리하였습니다."), pCharA->name());
				pCharB->chat_packet(CHAT_TYPE_NOTICE, "%s %d : %d %s", pCharA->name(), set_point_of_a_, set_point_of_b_, pCharB->name());

				send_chat_packet_to_observer(CHAT_TYPE_NOTICE, "%s %d : %d %s", pCharA->name(), set_point_of_a_, set_point_of_b_, pCharB->name());

				metin2::log::info( "ARENA: {}({:d}) won a round vs {}({:d})",
						pCharA->name(), player_apid(), pCharB->name(), player_bpid());
			}
		}
		else if (pidb_ == dwPIDA)
		{
			set_point_of_b_++;
			if (set_point_of_b_ >= set_count_)
			{
				pCharA->chat_packet(CHAT_TYPE_NOTICE, lc_text("%s 님이 대련에서 승리하였습니다."), pCharB->name());
				pCharB->chat_packet(CHAT_TYPE_NOTICE, lc_text("%s 님이 대련에서 승리하였습니다."), pCharB->name());
				send_chat_packet_to_observer(CHAT_TYPE_NOTICE, lc_text("%s 님이 대련에서 승리하였습니다."), pCharB->name());

				metin2::log::info( "ARENA: Duel is end. Winner({:d}) Loser({:d})", player_bpid(), player_apid());
			}
			else
			{
				restart = true;
				pCharA->chat_packet(CHAT_TYPE_INFO, lc_text("%s 님이 승리하였습니다."), pCharB->name());
				pCharA->chat_packet(CHAT_TYPE_NOTICE, "%s %d : %d %s", pCharA->name(), set_point_of_a_, set_point_of_b_, pCharB->name());

				pCharB->chat_packet(CHAT_TYPE_INFO, lc_text("%s 님이 승리하였습니다."), pCharB->name());
				pCharB->chat_packet(CHAT_TYPE_NOTICE, "%s %d : %d %s", pCharA->name(), set_point_of_a_, set_point_of_b_, pCharB->name());

				send_chat_packet_to_observer(CHAT_TYPE_NOTICE, "%s %d : %d %s", pCharA->name(), set_point_of_a_, set_point_of_b_, pCharB->name());

				metin2::log::info( "ARENA : PID({:d}) won a round. Opp({:d})", player_bpid(), player_apid());
			}
		}
		else
		{
			// wtf
			metin2::log::info( "ARENA : OnDead Error ({:d}, {:d}) ({:d}, {:d})", pida_, pidb_, dwPIDA, dwPIDB);
		}

		int potion = quest::quest_manager::instance().event_flag("arena_potion_limit_count");
		pCharA->set_potion_limit(potion);
		pCharB->set_potion_limit(potion);
	}
	else
	{
		// Should never get here?!
	}

	if (restart == false)
	{
		if (pCharA != nullptr)
			pCharA->chat_packet(CHAT_TYPE_INFO, lc_text("10초뒤 마을로 되돌아갑니다."));

		if (	pCharB != nullptr)
			pCharB->chat_packet(CHAT_TYPE_INFO, lc_text("10초뒤 마을로 되돌아갑니다."));

		send_chat_packet_to_observer(CHAT_TYPE_INFO, lc_text("10초뒤 마을로 되돌아갑니다."));

		if (event_ != nullptr) {
			event_cancel(&event_);
		}

		arena_event_info* info = alloc_event_info<arena_event_info>();

		info->arena = this;
		info->state = 2;

		event_ = event_create(ready_to_start_event, info, passes_per_sec(10));
	}
	else
	{
		if (pCharA != nullptr)
			pCharA->chat_packet(CHAT_TYPE_INFO, lc_text("10초뒤 다음 판을 시작합니다."));

		if (pCharB != nullptr)
			pCharB->chat_packet(CHAT_TYPE_INFO, lc_text("10초뒤 다음 판을 시작합니다."));

		send_chat_packet_to_observer(CHAT_TYPE_INFO, lc_text("10초뒤 다음 판을 시작합니다."));

		if (event_ != nullptr) {
			event_cancel(&event_);
		}

		arena_event_info* info = alloc_event_info<arena_event_info>();

		info->arena = this;
		info->state = 3;

		event_ = event_create(ready_to_start_event, info, passes_per_sec(10));
	}

	return true;
}

bool arena_manager::add_observer(character * pChar, uint32_t mapIdx, uint16_t ObserverX, uint16_t ObserverY)
{
	auto iter = arena_map_.find(mapIdx);

	if (iter == arena_map_.end()) return false;

	arena_map* pArenaMap = iter->second.get();
	return pArenaMap->add_observer(pChar, ObserverX, ObserverY);
}

bool arena_map::add_observer(character * pChar, uint16_t ObserverX, uint16_t ObserverY)
{
	auto iter = arena_.begin();

	for (; iter != arena_.end(); iter++)
	{
		arena* pArena = iter->get();

		if (pArena->is_my_observer(ObserverX, ObserverY) == true)
		{
			pChar->set_arena(pArena);
			return pArena->add_observer(pChar);
		}
	}

	return false;
}

bool arena::is_my_observer(uint16_t ObserverX, uint16_t ObserverY)
{
	return ((ObserverX == observer_point_.x) && (ObserverY == observer_point_.y));
}

bool arena::add_observer(character * pChar)
{
	uint32_t pid = pChar->player_id();

	observer_.insert(std::make_pair(pid, (character *)nullptr));

	pChar->save_exit_location();
	pChar->warp_set(observer_point_.x * 100, observer_point_.y * 100);

	return true;
}

bool arena_manager::is_arena_map(uint32_t dwMapIndex)
{
	return arena_map_.find(dwMapIndex) != arena_map_.end();
}

member_identity arena_manager::is_member(uint32_t dwMapIndex, uint32_t PID)
{
	auto iter = arena_map_.find(dwMapIndex);

	if (iter != arena_map_.end())
	{
		arena_map* pArenaMap = iter->second.get();
		return pArenaMap->is_member(PID);
	}

	return MEMBER_NO;
}

member_identity arena_map::is_member(uint32_t PID)
{
	auto iter = arena_.begin();

	for (; iter != arena_.end(); iter++)
	{
		arena* pArena = iter->get();

		if (pArena->is_observer(PID) == true) return MEMBER_OBSERVER;
		if (pArena->is_member(PID) == true) return MEMBER_DUELIST;
	}
	return MEMBER_NO;
}

bool arena::is_observer(uint32_t PID)
{
	auto iter = observer_.find(PID);

	return iter != observer_.end();
}

void arena::on_disconnect(uint32_t pid)
{
	if (pida_ == pid)
	{
		if (player_b() != nullptr)
			player_b()->chat_packet(CHAT_TYPE_INFO, lc_text("상대방 캐릭터가 접속을 종료하여 대련을 중지합니다."));

		metin2::log::info( "ARENA : Duel is end because of Opp({:d}) is disconnect. MyPID({:d})", player_apid(), player_bpid());
		end_duel();
	}
	else if (pidb_ == pid)
	{
		if (player_a() != nullptr)
			player_a()->chat_packet(CHAT_TYPE_INFO, lc_text("상대방 캐릭터가 접속을 종료하여 대련을 중지합니다."));

		metin2::log::info( "ARENA : Duel is end because of Opp({:d}) is disconnect. MyPID({:d})", player_bpid(), player_apid());
		end_duel();
	}
}

void arena::remove_observer(uint32_t pid)
{
	auto iter = observer_.find(pid);

	if (iter != observer_.end())
	{
		observer_.erase(iter);
	}
}

void arena::send_chat_packet_to_observer(uint8_t type, const char * format, ...)
{
}

bool arena_manager::end_duel(uint32_t pid)
{
	auto iter = arena_map_.begin();

	for (; iter != arena_map_.end(); iter++)
	{
		arena_map* pArenaMap = iter->second.get();
		if (pArenaMap->end_duel(pid) == true) return true;
	}
	return false;
}

bool arena_map::end_duel(uint32_t pid)
{
	auto iter = arena_.begin();

	for (; iter != arena_.end(); iter++)
	{
		arena* pArena = iter->get();
		if (pArena->is_member(pid) == true)
		{
			pArena->end_duel();
			return true;
		}
	}
	return false;
}

bool arena_manager::register_observer_ptr(character * pChar, uint32_t mapIdx, uint16_t ObserverX, uint16_t ObserverY)
{
	if (pChar == nullptr) return false;

	auto iter = arena_map_.find(mapIdx);

	if (iter == arena_map_.end())
	{
		metin2::log::info( "ARENA : Cannot find ArenaMap. {:d} {:d} {:d}", mapIdx, ObserverX, ObserverY);
		return false;
	}

	arena_map* pArenaMap = iter->second.get();
	return pArenaMap->register_observer_ptr(pChar, mapIdx, ObserverX, ObserverY);
}

bool arena_map::register_observer_ptr(character * pChar, uint32_t mapIdx, uint16_t ObserverX, uint16_t ObserverY)
{
	auto iter = arena_.begin();

	for (; iter != arena_.end(); ++iter)
	{
		arena* pArena = iter->get();

		if (pArena->is_my_observer(ObserverX, ObserverY) == true)
		{
			return pArena->register_observer_ptr(pChar);
		}
	}

	return false;
}

bool arena::register_observer_ptr(character * pChar)
{
	uint32_t pid = pChar->player_id();
	auto iter = observer_.find(pid);

	if (iter == observer_.end())
	{
		metin2::log::info( "ARENA : not in ob list");
		return false;
	}

	observer_[pid] = pChar;
	return true;
}

bool arena_manager::is_limited_item( int32_t lMapIndex, uint32_t dwVnum )
{
	return false;
}
}
