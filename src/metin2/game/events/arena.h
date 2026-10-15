#pragma once

#include <metin2/game/characters/char_manager.h>
#include <metin2/game/app/event.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>

#include <cstdint>
#include <list>
#include <map>
#include <memory>

namespace metin2::game::inline events {

enum member_identity {
    MEMBER_NO,
    MEMBER_DUELIST,
    MEMBER_OBSERVER,

    MEMBER_MAX
};

class arena {
    friend class arena_map;

  private:
    uint32_t pida_;
    uint32_t pidb_;

    event_ptr event_;
    event_ptr time_out_event_;

    pixel_position start_point_a_;
    pixel_position start_point_b_;
    pixel_position observer_point_;

    uint32_t set_count_;
    uint32_t set_point_of_a_;
    uint32_t set_point_of_b_;

    std::map<uint32_t, character *> observer_;

  protected:
    arena(uint16_t startA_X, uint16_t startA_Y, uint16_t startB_X, uint16_t startB_Y);

    bool start_duel(character * pCharFrom, character * pCharTo, int nSetPoint, int nMinute = 5);

    bool is_empty() const { return ((pida_ == 0) && (pidb_ == 0)); }
    bool is_member(uint32_t dwPID) const { return ((pida_ == dwPID) || (pidb_ == dwPID)); }

    bool check_area(uint16_t startA_X, uint16_t startA_Y, uint16_t startB_X, uint16_t startB_Y);
    void clear();

    bool can_attack(uint32_t dwPIDA, uint32_t dwPIDB);
    bool on_dead(uint32_t dwPIDA, uint32_t dwPIDB);

    bool is_observer(uint32_t pid);
    bool is_my_observer(uint16_t ObserverX, uint16_t ObserverY);
    bool add_observer(character * pChar);
    bool register_observer_ptr(character * pChar);

  public:
    uint32_t player_apid() { return pida_; }
    uint32_t player_bpid() { return pidb_; }

    character * player_a() { return character_manager::instance().find_by_pid(pida_); }
    character * player_b() { return character_manager::instance().find_by_pid(pidb_); }

    pixel_position start_point_a() { return start_point_a_; }
    pixel_position start_point_b() { return start_point_b_; }

    pixel_position observer_point() { return observer_point_; }

    void end_duel();
    void clear_event() { event_ = nullptr; }
    void on_disconnect(uint32_t pid);
    void remove_observer(uint32_t pid);

    void send_chat_packet_to_observer(uint8_t type, const char *format, ...);
};

class arena_map {
    friend class arena_manager;

  private:
    uint32_t map_index_;
    std::list<std::unique_ptr<arena>> arena_;

  protected:
    void destroy();

    bool add_arena(uint32_t mapIdx, uint16_t startA_X, uint16_t startA_Y, uint16_t startB_X, uint16_t startB_Y);
    void send_arena_map_list_to(character * pChar, uint32_t dwMapIndex);

    bool start_duel(character * pCharFrom, character * pCharTo, int nSetPoint, int nMinute = 5);
    void end_all_duel();
    bool end_duel(uint32_t pid);

    int duel_list(lua_State *L, int index);

    bool can_attack(character * pCharAttacker, character * pCharVictim);
    bool on_dead(character * pCharKiller, character * pCharVictim);

    bool add_observer(character * pChar, uint16_t ObserverX, uint16_t ObserverY);
    bool register_observer_ptr(character * pChar, uint32_t mapIdx, uint16_t ObserverX, uint16_t ObserverY);

    member_identity is_member(uint32_t PID);
};

class arena_manager : public singleton<arena_manager> {
  private:
    std::map<uint32_t, std::unique_ptr<arena_map>> arena_map_;

  public:
    bool initialize();
    void destroy();

    bool start_duel(character * pCharFrom, character * pCharTo, int nSetPoint, int nMinute = 5);

    bool add_arena(uint32_t mapIdx, uint16_t startA_X, uint16_t startA_Y, uint16_t startB_X, uint16_t startB_Y);

    void send_arena_map_list_to(character * pChar);

    void end_all_duel();
    bool end_duel(uint32_t pid);

    void duel_list(lua_State *L);

    bool can_attack(character * pCharAttacker, character * pCharVictim);

    bool on_dead(character * pCharKiller, character * pCharVictim);

    bool add_observer(character * pChar, uint32_t mapIdx, uint16_t ObserverX, uint16_t ObserverY);
    bool register_observer_ptr(character * pChar, uint32_t mapIdx, uint16_t ObserverX, uint16_t ObserverY);

    bool is_arena_map(uint32_t dwMapIndex);
    member_identity is_member(uint32_t dwMapIndex, uint32_t PID);

    bool is_limited_item(int32_t lMapIndex, uint32_t dwVnum);
};
}
