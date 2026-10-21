#include <metin2/game/events/arena.h>

#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/empire/start_position.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/text/locale.hpp>

#include <metin2/log/log.h>
#include <metin2/proto/chat_types.h>

#include <cstdint>
#include <memory>
#include <utility>

namespace metin2::game::inline events {

    arena::arena(uint16_t start_a_x, uint16_t start_a_y, uint16_t start_b_x, uint16_t start_b_y) {
        start_point_a_.x = start_a_x;
        start_point_a_.y = start_a_y;
        start_point_a_.z = 0;

        start_point_b_.x = start_b_x;
        start_point_b_.y = start_b_y;
        start_point_b_.z = 0;

        observer_point_.x = (start_a_x + start_b_x) / 2;
        observer_point_.y = (start_a_y + start_b_y) / 2;
        observer_point_.z = 0;

        event_ = nullptr;
        time_out_event_ = nullptr;

        clear();
    }

    void arena::clear() {
        pida_ = 0;
        pidb_ = 0;

        if (event_ != nullptr) {
            event_cancel(&event_);
        }

        if (time_out_event_ != nullptr) {
            event_cancel(&time_out_event_);
        }

        set_count_ = 0;
        set_point_of_a_ = 0;
        set_point_of_b_ = 0;
    }

    auto arena_manager::add_arena(uint32_t idx, uint16_t start_a_x, uint16_t start_a_y, uint16_t start_b_x, uint16_t start_b_y) -> bool {
        arena_map *arena_map_value = nullptr;
        auto iter = arena_map_.find(idx);

        if (iter == arena_map_.end()) {
            arena_map_value = arena_map_.emplace(idx, std::make_unique<arena_map>()).first->second.get();
        } else {
            arena_map_value = iter->second.get();
        }

        if (arena_map_value->add_arena(idx, start_a_x, start_a_y, start_b_x, start_b_y) == false) {
            metin2::log::error("Failed to add arena to arena map {:d}", idx);
            return false;
        }

        return true;
    }

    auto arena_map::add_arena(uint32_t idx, uint16_t start_a_x, uint16_t start_a_y, uint16_t start_b_x, uint16_t start_b_y) -> bool {
        auto iter = arena_.begin();

        for (; iter != arena_.end(); iter++) {
            // settings.lua registers the arenas again on every quest reload.
            if ((*iter)->check_area(start_a_x, start_a_y, start_b_x, start_b_y) == false) {
                metin2::log::debug("Arena A ({:d}, {:d}), B ({:d}, {:d}) on map {:d} is already registered", start_a_x, start_a_y, start_b_x, start_b_y, idx);
                return true;
            }
        }

        map_index_ = idx;

        arena_.push_back(std::unique_ptr<arena>(new arena(start_a_x, start_a_y, start_b_x, start_b_y)));

        return true;
    }

    void arena_manager::destroy() {
        auto iter = arena_map_.begin();

        for (; iter != arena_map_.end(); iter++) {
            arena_map *arena_map = iter->second.get();
            arena_map->destroy();
        }
        arena_map_.clear();
    }

    void arena_map::destroy() {
        auto iter = arena_.begin();

        metin2::log::info("Arena: destroying arena map {:d}", map_index_);

        for (; iter != arena_.end(); iter++) {
            arena *arena = iter->get();
            arena->end_duel();
        }
        arena_.clear();
    }

    auto arena::check_area(uint16_t start_a_x, uint16_t start_a_y, uint16_t start_b_x, uint16_t start_b_y) -> bool {
        if (start_point_a_.x == start_a_x && start_point_a_.y == start_a_y &&
            start_point_b_.x == start_b_x && start_point_b_.y == start_b_y)
            return false;
        return true;
    }

    void arena_manager::send_arena_map_list_to(character *ch) {
        auto iter = arena_map_.begin();

        for (; iter != arena_map_.end(); iter++) {
            arena_map *arena = iter->second.get();
            arena->send_arena_map_list_to(ch, (iter->first));
        }
    }

    void arena_map::send_arena_map_list_to(character *ch, uint32_t idx) {
        if (ch == nullptr) return;

        auto iter = arena_.begin();

        for (; iter != arena_.end(); iter++) {
            ch->chat_packet(CHAT_TYPE_INFO, "ArenaMapInfo Map: %d stA(%d, %d) stB(%d, %d)", idx,
                static_cast<int>((*iter)->start_point_a().x), static_cast<int>((*iter)->start_point_a().y),
                static_cast<int>((*iter)->start_point_b().x), static_cast<int>((*iter)->start_point_b().y));
        }
    }

    auto arena_manager::start_duel(character *char_from, character *char_to, int set_point, int minute) -> bool {
        if (char_from == nullptr || char_to == nullptr) return false;

        auto iter = arena_map_.begin();

        for (; iter != arena_map_.end(); iter++) {
            arena_map *arena_map = iter->second.get();
            if (arena_map->start_duel(char_from, char_to, set_point, minute) == true) {
                return true;
            }
        }

        return false;
    }

    auto arena_map::start_duel(character *char_from, character *char_to, int set_point, int minute) -> bool {
        auto iter = arena_.begin();

        for (; iter != arena_.end(); iter++) {
            arena *arena = iter->get();
            if (arena->is_empty() == true) {
                return arena->start_duel(char_from, char_to, set_point, minute);
            }
        }

        return false;
    }

    EVENTINFO(arena_event_info) {
        game::events::arena *arena;
        uint8_t state;

        arena_event_info()
            : arena(nullptr), state(0) {
        }
    };

    EVENTFUNC(ready_to_start_event) {
        if (event == nullptr)
            return 0;

        if (event->info == nullptr)
            return 0;

        auto *info = dynamic_cast<arena_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Arena start event has no event info");
            return 0;
        }

        arena *arena_value = info->arena;

        if (arena_value == nullptr) {
            metin2::log::error("Arena start event has no arena");
            return 0;
        }

        character *ch_a = arena_value->player_a();
        character *ch_b = arena_value->player_b();

        if (ch_a == nullptr || ch_b == nullptr) {
            metin2::log::info("Arena: a duel participant is missing at duel start");

            if (ch_a != nullptr) {
                ch_a->chat_packet(CHAT_TYPE_INFO, lc_text("대련 상대가 사라져 대련을 종료합니다."));
                metin2::log::info("Arena: opponent of pid {:d} (pid {:d}) is gone, ending duel", arena_value->player_apid(), arena_value->player_bpid());
            }

            if (ch_b != nullptr) {
                ch_b->chat_packet(CHAT_TYPE_INFO, lc_text("대련 상대가 사라져 대련을 종료합니다."));
                metin2::log::info("Arena: opponent of pid {:d} (pid {:d}) is gone, ending duel", arena_value->player_bpid(), arena_value->player_apid());
            }

            arena_value->send_chat_packet_to_observer(CHAT_TYPE_NOTICE, lc_text("대련 상대가 사라져 대련을 종료합니다."));

            arena_value->end_duel();
            return 0;
        }

        switch (info->state) {
        case 0:
            {
                ch_a->set_arena(arena_value);
                ch_b->set_arena(arena_value);

                int count = quest::get_quest_manager().event_flag("arena_potion_limit_count");

                if (count > 10000) {
                    ch_a->chat_packet(CHAT_TYPE_INFO, lc_text("물약 제한이 없습니다."));
                    ch_b->chat_packet(CHAT_TYPE_INFO, lc_text("물약 제한이 없습니다."));
                } else {
                    ch_a->set_potion_limit(count);
                    ch_b->set_potion_limit(count);

                    ch_a->chat_packet(CHAT_TYPE_INFO, lc_text("물약을 %d 개 까지 사용 가능합니다."), ch_a->potion_limit());
                    ch_b->chat_packet(CHAT_TYPE_INFO, lc_text("물약을 %d 개 까지 사용 가능합니다."), ch_b->potion_limit());
                }
                ch_a->chat_packet(CHAT_TYPE_INFO, lc_text("10초뒤 대련이 시작됩니다."));
                ch_b->chat_packet(CHAT_TYPE_INFO, lc_text("10초뒤 대련이 시작됩니다."));
                arena_value->send_chat_packet_to_observer(CHAT_TYPE_INFO, lc_text("10초뒤 대련이 시작됩니다."));

                info->state++;
                return passes_per_sec(10);
            }
            break;

        case 1:
            {
                ch_a->chat_packet(CHAT_TYPE_INFO, lc_text("대련이 시작되었습니다."));
                ch_b->chat_packet(CHAT_TYPE_INFO, lc_text("대련이 시작되었습니다."));
                arena_value->send_chat_packet_to_observer(CHAT_TYPE_INFO, lc_text("대련이 시작되었습니다."));

                packet_gc_duel_start duel_start;
                duel_start.header = HEADER_GC_DUEL_START;
                duel_start.size = sizeof(packet_gc_duel_start) + 4;

                uint32_t opp_list[8];

                opp_list[0] = (uint32_t)ch_b->get_vid();
                temp_buffer buf;

                buf.write(&duel_start, sizeof(packet_gc_duel_start));
                buf.write(&opp_list[0], 4);
                ch_a->get_desc()->packet(buf.read_peek(), buf.size());

                opp_list[0] = (uint32_t)ch_a->get_vid();
                temp_buffer buf2;

                buf2.write(&duel_start, sizeof(packet_gc_duel_start));
                buf2.write(&opp_list[0], 4);
                ch_b->get_desc()->packet(buf2.read_peek(), buf2.size());

                return 0;
            }
            break;

        case 2:
            {
                arena_value->end_duel();
                return 0;
            }
            break;

        case 3:
            {
                ch_a->show(ch_a->map_index(), arena_value->start_point_a().x * 100, arena_value->start_point_a().y * 100);
                ch_b->show(ch_b->map_index(), arena_value->start_point_b().x * 100, arena_value->start_point_b().y * 100);

                ch_a->get_desc()->set_phase(PHASE_GAME);
                ch_a->start_recovery_event();
                ch_a->set_position(POS_STANDING);
                ch_a->point_change(POINT_HP, ch_a->max_hp() - ch_a->hp());
                ch_a->point_change(POINT_SP, ch_a->max_sp() - ch_a->sp());
                ch_a->view_reencode();

                ch_b->get_desc()->set_phase(PHASE_GAME);
                ch_b->start_recovery_event();
                ch_b->set_position(POS_STANDING);
                ch_b->point_change(POINT_HP, ch_b->max_hp() - ch_b->hp());
                ch_b->point_change(POINT_SP, ch_b->max_sp() - ch_b->sp());
                ch_b->view_reencode();

                temp_buffer buf;
                temp_buffer buf2;
                uint32_t opp_list[8];
                packet_gc_duel_start duel_start;
                duel_start.header = HEADER_GC_DUEL_START;
                duel_start.size = sizeof(packet_gc_duel_start) + 4;

                opp_list[0] = (uint32_t)ch_b->get_vid();
                buf.write(&duel_start, sizeof(packet_gc_duel_start));
                buf.write(&opp_list[0], 4);
                ch_a->get_desc()->packet(buf.read_peek(), buf.size());

                opp_list[0] = (uint32_t)ch_a->get_vid();
                buf2.write(&duel_start, sizeof(packet_gc_duel_start));
                buf2.write(&opp_list[0], 4);
                ch_b->get_desc()->packet(buf2.read_peek(), buf2.size());

                ch_a->chat_packet(CHAT_TYPE_INFO, lc_text("대련이 시작되었습니다."));
                ch_b->chat_packet(CHAT_TYPE_INFO, lc_text("대련이 시작되었습니다."));
                arena_value->send_chat_packet_to_observer(CHAT_TYPE_INFO, lc_text("대련이 시작되었습니다."));

                arena_value->clear_event();

                return 0;
            }
            break;

        default:
            {
                ch_a->chat_packet(CHAT_TYPE_INFO, lc_text("대련장 문제로 인하여 대련을 종료합니다."));
                ch_b->chat_packet(CHAT_TYPE_INFO, lc_text("대련장 문제로 인하여 대련을 종료합니다."));
                arena_value->send_chat_packet_to_observer(CHAT_TYPE_INFO, lc_text("대련장 문제로 인하여 대련을 종료합니다."));

                metin2::log::error("Arena: unexpected event state {:d}, ending duel", info->state);

                arena_value->end_duel();

                return 0;
            }
        }
    }

    EVENTFUNC(duel_time_out) {
        if (event == nullptr) return 0;
        if (event->info == nullptr) return 0;

        auto *info = dynamic_cast<arena_event_info *>(event->info.get());

        if (info == nullptr) {
            metin2::log::error("Arena timeout event has no event info");
            return 0;
        }

        arena *arena_value = info->arena;

        if (arena_value == nullptr) {
            metin2::log::error("Arena timeout event has no arena");
            return 0;
        }

        character *ch_a = arena_value->player_a();
        character *ch_b = arena_value->player_b();

        if (ch_a == nullptr || ch_b == nullptr) {
            if (ch_a != nullptr) {
                ch_a->chat_packet(CHAT_TYPE_INFO, lc_text("대련 상대가 사라져 대련을 종료합니다."));
                metin2::log::info("Arena: opponent of pid {:d} (pid {:d}) is gone, ending duel", arena_value->player_apid(), arena_value->player_bpid());
            }

            if (ch_b != nullptr) {
                ch_b->chat_packet(CHAT_TYPE_INFO, lc_text("대련 상대가 사라져 대련을 종료합니다."));
                metin2::log::info("Arena: opponent of pid {:d} (pid {:d}) is gone, ending duel", arena_value->player_bpid(), arena_value->player_apid());
            }

            arena_value->send_chat_packet_to_observer(CHAT_TYPE_INFO, lc_text("대련 상대가 사라져 대련을 종료합니다."));

            arena_value->end_duel();
            return 0;
        } else {
            switch (info->state) {
            case 0:
                arena_value->send_chat_packet_to_observer(CHAT_TYPE_NOTICE, lc_text("대련 시간 초과로 대련을 중단합니다."));
                arena_value->send_chat_packet_to_observer(CHAT_TYPE_NOTICE, lc_text("10초뒤 마을로 이동합니다."));

                ch_a->chat_packet(CHAT_TYPE_NOTICE, lc_text("대련 시간 초과로 대련을 중단합니다."));
                ch_a->chat_packet(CHAT_TYPE_NOTICE, lc_text("10초뒤 마을로 이동합니다."));

                ch_b->chat_packet(CHAT_TYPE_NOTICE, lc_text("대련 시간 초과로 대련을 중단합니다."));
                ch_b->chat_packet(CHAT_TYPE_NOTICE, lc_text("10초뒤 마을로 이동합니다."));

                packet_gc_duel_start duel_start;
                duel_start.header = HEADER_GC_DUEL_START;
                duel_start.size = sizeof(packet_gc_duel_start);

                ch_a->get_desc()->packet(&duel_start, sizeof(packet_gc_duel_start));
                ch_b->get_desc()->packet(&duel_start, sizeof(packet_gc_duel_start));

                info->state++;

                metin2::log::info("Arena: duel of pid {:d} vs pid {:d} ended, time is up", arena_value->player_apid(), arena_value->player_bpid());

                return passes_per_sec(10);
                break;

            case 1:
                arena_value->end_duel();
                break;
            }
        }

        return 0;
    }

    auto arena::start_duel(character *char_from, character *char_to, int set_point, int minute) -> bool {
        this->pida_ = char_from->player_id();
        this->pidb_ = char_to->player_id();
        this->set_count_ = set_point;

        char_from->warp_set(start_point_a().x * 100, start_point_a().y * 100);
        char_to->warp_set(start_point_b().x * 100, start_point_b().y * 100);

        if (event_ != nullptr) {
            event_cancel(&event_);
        }

        auto *info = alloc_event_info<arena_event_info>();

        info->arena = this;
        info->state = 0;

        event_ = event_create(ready_to_start_event, info, passes_per_sec(10));

        if (time_out_event_ != nullptr) {
            event_cancel(&time_out_event_);
        }

        info = alloc_event_info<arena_event_info>();

        info->arena = this;
        info->state = 0;

        time_out_event_ = event_create(duel_time_out, info, passes_per_sec(minute * 60));

        char_from->point_change(POINT_HP, char_from->max_hp() - char_from->hp());
        char_from->point_change(POINT_SP, char_from->max_sp() - char_from->sp());

        char_to->point_change(POINT_HP, char_to->max_hp() - char_to->hp());
        char_to->point_change(POINT_SP, char_to->max_sp() - char_to->sp());

        metin2::log::info("Arena: duel started, pid {:d} vs pid {:d}", player_apid(), player_bpid());
        return true;
    }

    void arena_manager::end_all_duel() {
        auto iter = arena_map_.begin();

        for (; iter != arena_map_.end(); iter++) {
            arena_map *arena_map = iter->second.get();
            if (arena_map != nullptr)
                arena_map->end_all_duel();
        }

        return;
    }

    void arena_map::end_all_duel() {
        auto iter = arena_.begin();

        for (; iter != arena_.end(); iter++) {
            arena *arena = iter->get();
            if (arena != nullptr)
                arena->end_duel();
        }
    }

    void arena::end_duel() {
        if (event_ != nullptr) {
            event_cancel(&event_);
        }
        if (time_out_event_ != nullptr) {
            event_cancel(&time_out_event_);
        }

        character *player_a_value = player_a();
        character *player_b_value = player_b();

        if (player_a_value != nullptr) {
            player_a_value->set_mode(PK_MODE_PEACE);
            player_a_value->start_recovery_event();
            player_a_value->set_position(POS_STANDING);
            player_a_value->point_change(POINT_HP, player_a_value->max_hp() - player_a_value->hp());
            player_a_value->point_change(POINT_SP, player_a_value->max_sp() - player_a_value->sp());

            player_a_value->set_arena(nullptr);

            player_a_value->warp_set(arena_return_point_x(player_a_value->empire()), arena_return_point_y(player_a_value->empire()));
        }

        if (player_b_value != nullptr) {
            player_b_value->set_mode(PK_MODE_PEACE);
            player_b_value->start_recovery_event();
            player_b_value->set_position(POS_STANDING);
            player_b_value->point_change(POINT_HP, player_b_value->max_hp() - player_b_value->hp());
            player_b_value->point_change(POINT_SP, player_b_value->max_sp() - player_b_value->sp());

            player_b_value->set_arena(nullptr);

            player_b_value->warp_set(arena_return_point_x(player_b_value->empire()), arena_return_point_y(player_b_value->empire()));
        }

        auto iter = observer_.begin();

        for (; iter != observer_.end(); iter++) {
            character *ch = get_character_manager().find_by_pid(iter->first);
            if (ch != nullptr) {
                ch->warp_set(arena_return_point_x(ch->empire()), arena_return_point_y(ch->empire()));
            }
        }

        observer_.clear();

        metin2::log::info("Arena: duel ended, pid {:d} vs pid {:d}", player_apid(), player_bpid());

        clear();
    }

    void arena_manager::duel_list(lua_State *lua) {
        auto iter = arena_map_.begin();

        int index = 1;
        lua_newtable(lua);

        for (; iter != arena_map_.end(); iter++) {
            arena_map *arena_map = iter->second.get();
            if (arena_map != nullptr)
                index = arena_map->duel_list(lua, index);
        }
    }

    auto arena_map::duel_list(lua_State *lua, int index) -> int {
        auto iter = arena_.begin();

        for (; iter != arena_.end(); iter++) {
            arena *arena = iter->get();

            if (arena == nullptr) continue;

            if (arena->is_empty() == false) {
                character *ch_a = arena->player_a();
                character *ch_b = arena->player_b();

                if (ch_a != nullptr && ch_b != nullptr) {
                    lua_newtable(lua);

                    lua_pushstring(lua, ch_a->name());
                    lua_rawseti(lua, -2, 1);

                    lua_pushstring(lua, ch_b->name());
                    lua_rawseti(lua, -2, 2);

                    lua_pushnumber(lua, map_index_);
                    lua_rawseti(lua, -2, 3);

                    lua_pushnumber(lua, arena->observer_point().x);
                    lua_rawseti(lua, -2, 4);

                    lua_pushnumber(lua, arena->observer_point().y);
                    lua_rawseti(lua, -2, 5);

                    lua_rawseti(lua, -2, index++);
                }
            }
        }

        return index;
    }

    auto arena_manager::can_attack(character *char_attacker, character *char_victim) -> bool {
        if (char_attacker == nullptr || char_victim == nullptr) return false;

        if (char_attacker == char_victim) return false;

        int32_t map_index = char_attacker->map_index();
        if (map_index != char_victim->map_index()) return false;

        auto iter = arena_map_.find(map_index);

        if (iter == arena_map_.end()) return false;

        arena_map *arena_map = iter->second.get();
        return arena_map->can_attack(char_attacker, char_victim);
    }

    auto arena_map::can_attack(character *char_attacker, character *char_victim) -> bool {
        if (char_attacker == nullptr || char_victim == nullptr) return false;

        uint32_t pida = char_attacker->player_id();
        uint32_t pidb = char_victim->player_id();

        auto iter = arena_.begin();

        for (; iter != arena_.end(); iter++) {
            arena *arena = iter->get();
            if (arena->can_attack(pida, pidb) == true) {
                return true;
            }
        }
        return false;
    }

    auto arena::can_attack(uint32_t pida, uint32_t pidb) -> bool {
        if (pida_ == pida && pidb_ == pidb) return true;
        if (pida_ == pidb && pidb_ == pida) return true;

        return false;
    }

    auto arena_manager::on_dead(character *char_killer, character *char_victim) -> bool {
        if (char_killer == nullptr || char_victim == nullptr) return false;

        int32_t map_index = char_killer->map_index();
        if (map_index != char_victim->map_index()) return false;

        auto iter = arena_map_.find(map_index);
        if (iter == arena_map_.end()) return false;

        arena_map *arena_map = iter->second.get();
        return arena_map->on_dead(char_killer, char_victim);
    }

    auto arena_map::on_dead(character *char_killer, character *char_victim) -> bool {
        uint32_t pida = char_killer->player_id();
        uint32_t pidb = char_victim->player_id();

        auto iter = arena_.begin();

        for (; iter != arena_.end(); iter++) {
            arena *arena = iter->get();

            if (arena->is_member(pida) == true && arena->is_member(pidb) == true) {
                arena->on_dead(pida, pidb);
                return true;
            }
        }
        return false;
    }

    auto arena::on_dead(uint32_t pida, uint32_t pidb) -> bool {
        bool restart = false;

        character *char_a = player_a();
        character *char_b = player_b();

        if (char_a == nullptr && char_b == nullptr) {
            send_chat_packet_to_observer(CHAT_TYPE_NOTICE, lc_text("대련자 문제로 인하여 대련을 중단합니다."));
            restart = false;
        } else if (char_a == nullptr && char_b != nullptr) {
            char_b->chat_packet(CHAT_TYPE_NOTICE, lc_text("상대방 캐릭터의 문제로 인하여 대련을 종료합니다."));
            send_chat_packet_to_observer(CHAT_TYPE_NOTICE, lc_text("대련자 문제로 인하여 대련을 종료합니다."));
            restart = false;
        } else if (char_a != nullptr && char_b == nullptr) {
            char_a->chat_packet(CHAT_TYPE_NOTICE, lc_text("상대방 캐릭터의 문제로 인하여 대련을 종료합니다."));
            send_chat_packet_to_observer(CHAT_TYPE_NOTICE, lc_text("대련자 문제로 인하여 대련을 종료합니다."));
            restart = false;
        } else if (char_a != nullptr && char_b != nullptr) {
            if (pida_ == pida) {
                set_point_of_a_++;

                if (set_point_of_a_ >= set_count_) {
                    char_a->chat_packet(CHAT_TYPE_NOTICE, lc_text("%s 님이 대련에서 승리하였습니다."), char_a->name());
                    char_b->chat_packet(CHAT_TYPE_NOTICE, lc_text("%s 님이 대련에서 승리하였습니다."), char_a->name());
                    send_chat_packet_to_observer(CHAT_TYPE_NOTICE, lc_text("%s 님이 대련에서 승리하였습니다."), char_a->name());

                    metin2::log::info("Arena: {} (pid {:d}) won the duel against {} (pid {:d})",
                        char_a->name(), player_apid(), char_b->name(), player_bpid());
                } else {
                    restart = true;
                    char_a->chat_packet(CHAT_TYPE_INFO, lc_text("%s 님이 승리하였습니다."), char_a->name());
                    char_a->chat_packet(CHAT_TYPE_NOTICE, "%s %d : %d %s", char_a->name(), set_point_of_a_, set_point_of_b_, char_b->name());

                    char_b->chat_packet(CHAT_TYPE_INFO, lc_text("%s 님이 승리하였습니다."), char_a->name());
                    char_b->chat_packet(CHAT_TYPE_NOTICE, "%s %d : %d %s", char_a->name(), set_point_of_a_, set_point_of_b_, char_b->name());

                    send_chat_packet_to_observer(CHAT_TYPE_NOTICE, "%s %d : %d %s", char_a->name(), set_point_of_a_, set_point_of_b_, char_b->name());

                    metin2::log::info("Arena: {} (pid {:d}) won a round against {} (pid {:d})",
                        char_a->name(), player_apid(), char_b->name(), player_bpid());
                }
            } else if (pidb_ == pida) {
                set_point_of_b_++;
                if (set_point_of_b_ >= set_count_) {
                    char_a->chat_packet(CHAT_TYPE_NOTICE, lc_text("%s 님이 대련에서 승리하였습니다."), char_b->name());
                    char_b->chat_packet(CHAT_TYPE_NOTICE, lc_text("%s 님이 대련에서 승리하였습니다."), char_b->name());
                    send_chat_packet_to_observer(CHAT_TYPE_NOTICE, lc_text("%s 님이 대련에서 승리하였습니다."), char_b->name());

                    metin2::log::info("Arena: pid {:d} won the duel against pid {:d}", player_bpid(), player_apid());
                } else {
                    restart = true;
                    char_a->chat_packet(CHAT_TYPE_INFO, lc_text("%s 님이 승리하였습니다."), char_b->name());
                    char_a->chat_packet(CHAT_TYPE_NOTICE, "%s %d : %d %s", char_a->name(), set_point_of_a_, set_point_of_b_, char_b->name());

                    char_b->chat_packet(CHAT_TYPE_INFO, lc_text("%s 님이 승리하였습니다."), char_b->name());
                    char_b->chat_packet(CHAT_TYPE_NOTICE, "%s %d : %d %s", char_a->name(), set_point_of_a_, set_point_of_b_, char_b->name());

                    send_chat_packet_to_observer(CHAT_TYPE_NOTICE, "%s %d : %d %s", char_a->name(), set_point_of_a_, set_point_of_b_, char_b->name());

                    metin2::log::info("Arena: pid {:d} won a round against pid {:d}", player_bpid(), player_apid());
                }
            } else {
                metin2::log::error("Arena: reported death does not match the duel, duel pids ({:d}, {:d}), reported ({:d}, {:d})", pida_, pidb_, pida, pidb);
            }

            int potion = quest::get_quest_manager().event_flag("arena_potion_limit_count");
            char_a->set_potion_limit(potion);
            char_b->set_potion_limit(potion);
        } else {
        }

        if (restart == false) {
            if (char_a != nullptr)
                char_a->chat_packet(CHAT_TYPE_INFO, lc_text("10초뒤 마을로 되돌아갑니다."));

            if (char_b != nullptr)
                char_b->chat_packet(CHAT_TYPE_INFO, lc_text("10초뒤 마을로 되돌아갑니다."));

            send_chat_packet_to_observer(CHAT_TYPE_INFO, lc_text("10초뒤 마을로 되돌아갑니다."));

            if (event_ != nullptr) {
                event_cancel(&event_);
            }

            auto *info = alloc_event_info<arena_event_info>();

            info->arena = this;
            info->state = 2;

            event_ = event_create(ready_to_start_event, info, passes_per_sec(10));
        } else {
            if (char_a != nullptr)
                char_a->chat_packet(CHAT_TYPE_INFO, lc_text("10초뒤 다음 판을 시작합니다."));

            if (char_b != nullptr)
                char_b->chat_packet(CHAT_TYPE_INFO, lc_text("10초뒤 다음 판을 시작합니다."));

            send_chat_packet_to_observer(CHAT_TYPE_INFO, lc_text("10초뒤 다음 판을 시작합니다."));

            if (event_ != nullptr) {
                event_cancel(&event_);
            }

            auto *info = alloc_event_info<arena_event_info>();

            info->arena = this;
            info->state = 3;

            event_ = event_create(ready_to_start_event, info, passes_per_sec(10));
        }

        return true;
    }

    auto arena_manager::add_observer(character *ch, uint32_t idx, uint16_t observer_x, uint16_t observer_y) -> bool {
        auto iter = arena_map_.find(idx);

        if (iter == arena_map_.end()) return false;

        arena_map *arena_map = iter->second.get();
        return arena_map->add_observer(ch, observer_x, observer_y);
    }

    auto arena_map::add_observer(character *ch, uint16_t observer_x, uint16_t observer_y) -> bool {
        auto iter = arena_.begin();

        for (; iter != arena_.end(); iter++) {
            arena *arena = iter->get();

            if (arena->is_my_observer(observer_x, observer_y) == true) {
                ch->set_arena(arena);
                return arena->add_observer(ch);
            }
        }

        return false;
    }

    auto arena::is_my_observer(uint16_t observer_x, uint16_t observer_y) -> bool {
        return ((observer_x == observer_point_.x) && (observer_y == observer_point_.y));
    }

    auto arena::add_observer(character *ch) -> bool {
        uint32_t pid = ch->player_id();

        observer_.insert(std::make_pair(pid, (character *)nullptr));

        ch->save_exit_location();
        ch->warp_set(observer_point_.x * 100, observer_point_.y * 100);

        return true;
    }

    auto arena_manager::is_arena_map(uint32_t map_index) -> bool {
        return arena_map_.find(map_index) != arena_map_.end();
    }

    auto arena_manager::is_member(uint32_t map_index, uint32_t pid) -> member_identity {
        auto iter = arena_map_.find(map_index);

        if (iter != arena_map_.end()) {
            arena_map *arena_map = iter->second.get();
            return arena_map->is_member(pid);
        }

        return MEMBER_NO;
    }

    auto arena_map::is_member(uint32_t pid) -> member_identity {
        auto iter = arena_.begin();

        for (; iter != arena_.end(); iter++) {
            arena *arena = iter->get();

            if (arena->is_observer(pid) == true) return MEMBER_OBSERVER;
            if (arena->is_member(pid) == true) return MEMBER_DUELIST;
        }
        return MEMBER_NO;
    }

    auto arena::is_observer(uint32_t pid) -> bool {
        auto iter = observer_.find(pid);

        return iter != observer_.end();
    }

    void arena::on_disconnect(uint32_t pid) {
        if (pida_ == pid) {
            if (player_b() != nullptr)
                player_b()->chat_packet(CHAT_TYPE_INFO, lc_text("상대방 캐릭터가 접속을 종료하여 대련을 중지합니다."));

            metin2::log::info("Arena: pid {:d} disconnected, ending duel with pid {:d}", player_apid(), player_bpid());
            end_duel();
        } else if (pidb_ == pid) {
            if (player_a() != nullptr)
                player_a()->chat_packet(CHAT_TYPE_INFO, lc_text("상대방 캐릭터가 접속을 종료하여 대련을 중지합니다."));

            metin2::log::info("Arena: pid {:d} disconnected, ending duel with pid {:d}", player_bpid(), player_apid());
            end_duel();
        }
    }

    void arena::remove_observer(uint32_t pid) {
        auto iter = observer_.find(pid);

        if (iter != observer_.end()) {
            observer_.erase(iter);
        }
    }

    void arena::send_chat_packet_to_observer(uint8_t type, const char *format, ...) {
    }

    auto arena_manager::end_duel(uint32_t pid) -> bool {
        auto iter = arena_map_.begin();

        for (; iter != arena_map_.end(); iter++) {
            arena_map *arena_map = iter->second.get();
            if (arena_map->end_duel(pid) == true) return true;
        }
        return false;
    }

    auto arena_map::end_duel(uint32_t pid) -> bool {
        auto iter = arena_.begin();

        for (; iter != arena_.end(); iter++) {
            arena *arena = iter->get();
            if (arena->is_member(pid) == true) {
                arena->end_duel();
                return true;
            }
        }
        return false;
    }

    auto arena_manager::register_observer_ptr(character *ch, uint32_t idx, uint16_t observer_x, uint16_t observer_y) -> bool {
        if (ch == nullptr) return false;

        auto iter = arena_map_.find(idx);

        if (iter == arena_map_.end()) {
            metin2::log::warn("Cannot add arena observer, arena map {:d} not found (position {:d} {:d})", idx, observer_x, observer_y);
            return false;
        }

        arena_map *arena_map = iter->second.get();
        return arena_map->register_observer_ptr(ch, idx, observer_x, observer_y);
    }

    auto arena_map::register_observer_ptr(character *ch, uint32_t idx, uint16_t observer_x, uint16_t observer_y) -> bool {
        auto iter = arena_.begin();

        for (; iter != arena_.end(); ++iter) {
            arena *arena = iter->get();

            if (arena->is_my_observer(observer_x, observer_y) == true) {
                return arena->register_observer_ptr(ch);
            }
        }

        return false;
    }

    auto arena::register_observer_ptr(character *ch) -> bool {
        uint32_t pid = ch->player_id();
        auto iter = observer_.find(pid);

        if (iter == observer_.end()) {
            metin2::log::warn("Pid {:d} is not in the arena observer list", pid);
            return false;
        }

        observer_[pid] = ch;
        return true;
    }

    auto arena_manager::is_limited_item(int32_t map_index, uint32_t vnum) -> bool {
        return false;
    }
}
