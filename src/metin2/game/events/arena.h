#pragma once

#include <metin2/game/characters/character_manager.h>
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
    arena(uint16_t start_a_x, uint16_t start_a_y, uint16_t start_b_x, uint16_t start_b_y);

    auto start_duel(character * char_from, character * char_to, int set_point, int minute = 5) -> bool;

    auto is_empty() const -> bool { return ((pida_ == 0) && (pidb_ == 0)); }
    auto is_member(uint32_t pid) const -> bool { return ((pida_ == pid) || (pidb_ == pid)); }

    auto check_area(uint16_t start_a_x, uint16_t start_a_y, uint16_t start_b_x, uint16_t start_b_y) -> bool;
    void clear();

    auto can_attack(uint32_t pida, uint32_t pidb) -> bool;
    auto on_dead(uint32_t pida, uint32_t pidb) -> bool;

    auto is_observer(uint32_t pid) -> bool;
    auto is_my_observer(uint16_t observer_x, uint16_t observer_y) -> bool;
    auto add_observer(character * ch) -> bool;
    auto register_observer_ptr(character * ch) -> bool;

  public:
    auto player_apid() -> uint32_t { return pida_; }
    auto player_bpid() -> uint32_t { return pidb_; }

    auto player_a() -> character * { return character_manager::instance().find_by_pid(pida_); }
    auto player_b() -> character * { return character_manager::instance().find_by_pid(pidb_); }

    auto start_point_a() -> pixel_position { return start_point_a_; }
    auto start_point_b() -> pixel_position { return start_point_b_; }

    auto observer_point() -> pixel_position { return observer_point_; }

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

    auto add_arena(uint32_t idx, uint16_t start_a_x, uint16_t start_a_y, uint16_t start_b_x, uint16_t start_b_y) -> bool;
    void send_arena_map_list_to(character * ch, uint32_t idx);

    auto start_duel(character * char_from, character * char_to, int set_point, int minute = 5) -> bool;
    void end_all_duel();
    auto end_duel(uint32_t pid) -> bool;

    auto duel_list(lua_State *lua, int index) -> int;

    auto can_attack(character * char_attacker, character * char_victim) -> bool;
    auto on_dead(character * char_killer, character * char_victim) -> bool;

    auto add_observer(character * ch, uint16_t observer_x, uint16_t observer_y) -> bool;
    auto register_observer_ptr(character * ch, uint32_t idx, uint16_t observer_x, uint16_t observer_y) -> bool;

    auto is_member(uint32_t pid) -> member_identity;
};

class arena_manager : public singleton<arena_manager> {
  private:
    std::map<uint32_t, std::unique_ptr<arena_map>> arena_map_;

  public:
    auto initialize() -> bool;
    void destroy();

    auto start_duel(character * char_from, character * char_to, int set_point, int minute = 5) -> bool;

    auto add_arena(uint32_t idx, uint16_t start_a_x, uint16_t start_a_y, uint16_t start_b_x, uint16_t start_b_y) -> bool;

    void send_arena_map_list_to(character * ch);

    void end_all_duel();
    auto end_duel(uint32_t pid) -> bool;

    void duel_list(lua_State *lua);

    auto can_attack(character * char_attacker, character * char_victim) -> bool;

    auto on_dead(character * char_killer, character * char_victim) -> bool;

    auto add_observer(character * ch, uint32_t idx, uint16_t observer_x, uint16_t observer_y) -> bool;
    auto register_observer_ptr(character * ch, uint32_t idx, uint16_t observer_x, uint16_t observer_y) -> bool;

    auto is_arena_map(uint32_t map_index) -> bool;
    auto is_member(uint32_t map_index, uint32_t pid) -> member_identity;

    auto is_limited_item(int32_t map_index, uint32_t vnum) -> bool;
};
}
