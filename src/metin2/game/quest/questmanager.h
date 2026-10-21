#pragma once

#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/quest.h>
#include <metin2/game/quest/quest_loader.h>
#include <metin2/game/quest/questnpc.h>

#include <metin2/log/log.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <source_location>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace metin2::game::inline characters {
    class character;
}
namespace metin2::game::inline world {
    class dungeon;
}

namespace metin2::game {

    namespace quest {
        auto is_script_true(const char *code, int size) -> bool;
        auto script_to_string(const std::string &str) -> std::string;

        class quest_manager {
          public:
            enum {
                QUEST_SKIN_NOWINDOW,
                QUEST_SKIN_NORMAL,
                QUEST_SKIN_CINEMATIC = 5,
                QUEST_SKIN_COUNT
            };

            using event_name_map = std::map<std::string, int>;
            using pc_map = std::map<unsigned int, pc>;

          public:
            quest_manager();
            ~quest_manager();

            auto initialize() -> bool;
            void destroy();

            auto initialize_lua() -> bool;
            void report_unknown_functions();
            auto lua_state() -> lua_State * { return l_; }
            void add_lua_function_table(const char *name_value, luaL_reg *preg);

            event_name_map event_name;

            auto open_state(const std::string &quest_name, int state_index) -> quest_state;
            void close_state(quest_state &qs);
            auto run_state(quest_state &qs) -> bool;

            auto get_pc(unsigned int pid) -> pc *;
            auto pc_force(unsigned int pid) -> pc *;

            auto current_npc_race() -> unsigned int;
            auto current_quest_name() -> const std::string &;
            auto find_npcid_by_name(const std::string &name) -> unsigned int;

            auto current_npc_character_ptr() -> character *;

            void set_current_event_index(int index) { running_event_index_ = index; }

            auto use_item(unsigned int pc, item *item, bool receive_all) -> bool;
            auto pickup_item(unsigned int pc, item *item) -> bool;
            auto sig_use(unsigned int pc, uint32_t sig_vnum, item *item, bool receive_all) -> bool;
            auto take_item(unsigned int pc, unsigned int npc, item *item) -> bool;
            auto current_item() -> item *;
            void clear_current_item();
            void set_current_item(item *item);
            void add_server_timer(const std::string &name, uint32_t arg, event_ptr event);
            void clear_server_timer(const std::string &name, uint32_t arg);
            void clear_server_timer_not_cancel(const std::string &name, uint32_t arg);
            void cancel_server_timers(uint32_t arg);

            void set_server_timer_arg(uint32_t arg);
            auto server_timer_arg() -> uint32_t;

            auto server_timer(unsigned int npc, unsigned int arg) -> bool;

            void login(unsigned int pc, const char *quest = nullptr);
            void logout(unsigned int pc);
            auto timer(unsigned int pc, unsigned int npc) -> bool;
            auto click(unsigned int pc, character *chr_target) -> bool;
            void kill(unsigned int pc, unsigned int npc);
            void level_up(unsigned int pc);
            void attr_in(unsigned int pc, character *ch, int attr);
            void attr_out(unsigned int pc, character *ch, int attr);
            auto target(unsigned int pc, uint32_t quest_index, const char *target_name, const char *verb) -> bool;
            auto give_item_to_pc(unsigned int pc, character *chr) -> bool;
            void unmount(unsigned int pc);

            void quest_button(unsigned int pc, unsigned int quest_index);
            void quest_info(unsigned int pc, unsigned int quest_index);

            void enter_state(uint32_t pc, uint32_t quest_index, int state);
            void leave_state(uint32_t pc, uint32_t quest_index, int state);

            void letter(uint32_t pc);
            void letter(uint32_t pc, uint32_t quest_index, int state);

            void item_informer(unsigned int pc, unsigned int vnum);

            auto check_quest_loaded(pc *pc) -> bool { return pc && pc->is_loaded(); }

            void select(unsigned int pc, unsigned int selection);
            void resume(unsigned int pc);
            void input(unsigned int pc, const char *msg);
            void confirm(unsigned int pc, quest_confirm_type confirm, unsigned int pc2 = 0);
            void select_item(unsigned int pc, unsigned int selection);

            void logout_pc(character *ch);
            void disconnect_pc(character *ch);

            auto current_state() -> quest_state * { return current_running_state_; }

            void load_start_quest(const std::string &quest_name, unsigned int idx);
            auto can_start_quest(unsigned int quest_index, const pc &pc) -> bool;
            auto can_start_quest(unsigned int quest_index) -> bool;

            auto current_character_ptr() -> character * { return current_character_; }
            auto current_party_member() -> character * { return current_party_member_; }
            auto current_pc() -> pc * { return current_pc_; }

            auto current_dungeon() -> dungeon *;
            void select_dungeon(dungeon *dungeon_value);

            void clear_script();
            void send_script();
            void add_script(const std::string &str);

            void build_state_index_to_name(const char *quest_name);

            auto quest_state_index(const std::string &quest_name, const std::string &state_name) -> int;
            auto quest_state_name(const std::string &quest_name, const int state_index) -> const char *;

            void set_skin_style(int style);

            void set_no_send() { no_send_ = true; }

            auto load_timer_script(const std::string &name) -> unsigned int;

            void register_quest(const std::string &quest_name, unsigned int idx);
            auto quest_index_by_name(const std::string &name) -> unsigned int;
            auto quest_name_by_index(unsigned int idx) -> const std::string &;

            void request_set_event_flag(const std::string &name, int value);

            void set_event_flag(const std::string &name, int value);
            auto event_flag(const std::string &name) -> int;
            void broadcast_event_flag_on_login(character *ch);

            void send_event_flag_list(character *ch);

            void reload();

            void set_error() { error_ = true; }
            void clear_error() { error_ = false; }
            auto is_error() -> bool { return error_; }
            void write_running_state_to_syserr();

            void register_npc_vnum(uint32_t vnum);

            [[nodiscard]] auto scripts() const -> const quest_scripts & { return scripts_; }

          private:
            dungeon *selected_dungeon_;
            uint32_t server_timer_arg_;

            std::map<std::pair<std::string, uint32_t>, event_ptr> server_timer_;

            int running_event_index_;

            std::map<std::string, int> event_flag_;

            void goto_select_state(quest_state &qs);
            void goto_pause_state(quest_state &qs);
            void goto_end_state(quest_state &qs);
            void goto_input_state(quest_state &qs);
            void goto_confirm_state(quest_state &qs);
            void goto_select_item_state(quest_state &qs);

            lua_State *l_;
            quest_scripts scripts_;

            bool no_send_;

            std::set<unsigned int> registered_npc_vnum_;
            std::map<unsigned int, npc> npc_;
            std::map<std::string, unsigned int> npc_name_id_;
            std::map<std::string, unsigned int> timer_id_;

            quest_state *current_running_state_;

            pc_map pc_;

            character *current_character_;
            character *current_party_member_;
            pc *current_pc_;

            std::string script_;

            int current_skin_;

            struct stringhash {
                auto operator()(const std::string &str) const -> size_t {
                    const auto *s = (const unsigned char *)str.c_str();
                    const unsigned char *end = s + str.size();
                    size_t h = 0;

                    while (s < end) {
                        h *= 16777619;
                        h ^= (unsigned char)*(unsigned char *)(s++);
                    }

                    return h;
                }
            };

            using hash_map_quest_name = std::unordered_map<std::string, int, stringhash>;
            using hash_map_quest_start_script = std::unordered_map<unsigned int, std::vector<char>>;

            hash_map_quest_name hm_quest_name_;
            hash_map_quest_start_script hm_quest_start_script_;
            std::map<unsigned int, std::string> quest_name_by_index_;

            bool error_;

          public:
            static auto execute_quest_script(pc &pc, const std::string &quest_name, const int state, const char *code, const int code_size, std::vector<a_arg_script *> *chat_scripts_value = nullptr, bool use_cache = true) -> bool;
            static auto execute_quest_script(pc &pc, uint32_t quest_index, const int state, const char *code, const int code_size, std::vector<a_arg_script *> *chat_scripts = nullptr, bool use_cache = true) -> bool;

          public:
            void begin_other_pc_block(uint32_t pid);
            void end_other_pc_block();
            auto is_in_other_pc_block() -> bool;
            auto other_pc_block_root_pc() -> pc *;

          private:
            pc *other_pc_block_root_pc_;
            std::vector<uint32_t> pc_stack_;
        };

        inline auto get_quest_manager() -> quest_manager & {
            static quest_manager instance;
            return instance;
        }
    };

    namespace quest {
        void report_error(const std::source_location &where, std::string_view message);

        template <typename... Args>
        void error(metin2::log::format_string<Args...> format, const Args &...args) {
            report_error(format.location, metin2::log::format(format, args...));
        }

    }
}
