#include <metin2/config/config.h>
#include <metin2/game/quest/questmanager.h>

#include <metin2/game/app/config.h>
#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/events/xmas_event.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/quest/quest.h>
#include <metin2/game/quest/questnpc.h>
#include <metin2/game/quest/questpc.h>
#include <metin2/game/quest/target.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/world/dungeon.h>
#include <metin2/game/world/sectree_manager.h>

#include <metin2/common/utils.h>
#include <metin2/log/log.h>
#include <metin2/proto/chat_types.h>
#include <metin2/proto/tables.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace metin2::game {

    namespace quest {
        quest_manager::quest_manager()
            : selected_dungeon_(nullptr), server_timer_arg_(0), running_event_index_(0), l_(nullptr), no_send_(false),
              current_running_state_(nullptr), current_character_(nullptr), current_party_member_(nullptr),
              current_pc_(nullptr), current_skin_(0), error_(false), other_pc_block_root_pc_(nullptr) {
        }

        quest_manager::~quest_manager() {
            destroy();
        }

        void quest_manager::destroy() {
            if (l_) {
                lua_close(l_);
                l_ = nullptr;
            }
        }

        auto quest_manager::initialize() -> bool {
            if (metin2::config::is_auth())
                return true;

            if (!initialize_lua())
                return false;

            selected_dungeon_ = nullptr;

            event_name.insert(event_name_map::value_type("click", QUEST_CLICK_EVENT));
            event_name.insert(event_name_map::value_type("kill", QUEST_KILL_EVENT));
            event_name.insert(event_name_map::value_type("timer", QUEST_TIMER_EVENT));
            event_name.insert(event_name_map::value_type("levelup", QUEST_LEVELUP_EVENT));
            event_name.insert(event_name_map::value_type("login", QUEST_LOGIN_EVENT));
            event_name.insert(event_name_map::value_type("logout", QUEST_LOGOUT_EVENT));
            event_name.insert(event_name_map::value_type("button", QUEST_BUTTON_EVENT));
            event_name.insert(event_name_map::value_type("info", QUEST_INFO_EVENT));
            event_name.insert(event_name_map::value_type("chat", QUEST_CHAT_EVENT));
            event_name.insert(event_name_map::value_type("in", QUEST_ATTR_IN_EVENT));
            event_name.insert(event_name_map::value_type("out", QUEST_ATTR_OUT_EVENT));
            event_name.insert(event_name_map::value_type("use", QUEST_ITEM_USE_EVENT));
            event_name.insert(event_name_map::value_type("server_timer", QUEST_SERVER_TIMER_EVENT));
            event_name.insert(event_name_map::value_type("enter", QUEST_ENTER_STATE_EVENT));
            event_name.insert(event_name_map::value_type("leave", QUEST_LEAVE_STATE_EVENT));
            event_name.insert(event_name_map::value_type("letter", QUEST_LETTER_EVENT));
            event_name.insert(event_name_map::value_type("take", QUEST_ITEM_TAKE_EVENT));
            event_name.insert(event_name_map::value_type("target", QUEST_TARGET_EVENT));
            event_name.insert(event_name_map::value_type("party_kill", QUEST_PARTY_KILL_EVENT));
            event_name.insert(event_name_map::value_type("unmount", QUEST_UNMOUNT_EVENT));
            event_name.insert(event_name_map::value_type("pick", QUEST_ITEM_PICK_EVENT));
            event_name.insert(event_name_map::value_type("sig_use", QUEST_SIG_USE_EVENT));
            event_name.insert(event_name_map::value_type("item_informer", QUEST_ITEM_INFORMER_EVENT));

            no_send_ = false;

            current_skin_ = QUEST_SKIN_NORMAL;

            {
                std::ifstream inf((quest_dir + "/questnpc.txt").c_str());
                int line = 0;

                if (!inf.is_open())
                    metin2::log::error("Cannot open questnpc.txt in {}", quest_dir);
                else
                    metin2::log::info("Quest: reading questnpc.txt from {}", quest_dir);

                while (1) {
                    unsigned int vnum;

                    inf >> vnum;

                    line++;

                    if (inf.fail())
                        break;

                    std::string s;
                    getline(inf, s);
                    unsigned int li = 0, ri = s.size() - 1;
                    while (li < s.size() && isspace(s[li]))
                        li++;
                    while (ri > 0 && isspace(s[ri]))
                        ri--;

                    if (ri < li) {
                        metin2::log::error("questnpc.txt line {:d}: bad npc name", line);
                        continue;
                    }

                    s = s.substr(li, ri - li + 1);

                    int n = 0;
                    str_to_number(n, s.c_str());
                    if (n)
                        continue;

                    if (metin2::config::test_server())
                        metin2::log::debug("Quest: reading scripts of npc {} (vnum {:d})", s, vnum);
                    npc_[vnum].set(vnum, s);
                    npc_name_id_[s] = vnum;
                }

                npc_[0].set(0, "notarget");
            }

            set_event_flag("guild_withdraw_delay", 1);
            set_event_flag("guild_disband_delay", 1);
            return true;
        }

        auto quest_manager::find_npcid_by_name(const std::string &name) -> unsigned int {
            auto it = npc_name_id_.find(name);
            return it != npc_name_id_.end() ? it->second : 0;
        }

        void quest_manager::select_item(unsigned int pc, unsigned int selection) {
            game::quest::pc *value = get_pc(pc);
            if (value && value->is_running() && value->running_quest_state()->suspend_state == SUSPEND_STATE_SELECT_ITEM) {
                value->set_send_done_flag();
                value->running_quest_state()->args = 1;
                lua_pushnumber(value->running_quest_state()->co, selection);

                if (!run_state(*value->running_quest_state())) {
                    close_state(*value->running_quest_state());
                    value->end_running();
                }
            }
        }

        void quest_manager::confirm(unsigned int pc, quest_confirm_type confirm, unsigned int pc2) {
            game::quest::pc *value = get_pc(pc);

            if (!value || !value->is_running()) {
                metin2::log::warn("Player (pid {:d}) sent quest input but no quest is running", pc);
                return;
            }

            if (value->running_quest_state()->suspend_state != SUSPEND_STATE_CONFIRM) {
                metin2::log::warn("Player (pid {:d}) sent a confirm but quest is not waiting for one (state {:d})", pc, value->running_quest_state()->suspend_state);
                return;
            }

            if (pc2 && !value->is_confirm_wait(pc2)) {
                metin2::log::warn("Player (pid {:d}) sent a confirm but quest is not waiting for one (state {:d})", pc, value->running_quest_state()->suspend_state);
                return;
            }

            value->clear_confirm_wait();

            value->set_send_done_flag();

            value->running_quest_state()->args = 1;
            lua_pushnumber(value->running_quest_state()->co, confirm);

            add_script("[END_CONFIRM_WAIT]");
            set_skin_style(QUEST_SKIN_NOWINDOW);
            send_script();

            if (!run_state(*value->running_quest_state())) {
                close_state(*value->running_quest_state());
                value->end_running();
            }
        }

        void quest_manager::input(unsigned int pc, const char *msg) {
            game::quest::pc *value = get_pc(pc);
            if (!value) {
                metin2::log::warn("Player (pid {:d}) has no quest state", pc);
                return;
            }

            if (!value->is_running()) {
                metin2::log::warn("Player (pid {:d}) sent quest input but no quest is running", pc);
                return;
            }

            if (value->running_quest_state()->suspend_state != SUSPEND_STATE_INPUT) {
                metin2::log::warn("Player (pid {:d}) sent input but quest is not waiting for it (state {:d})", pc, value->running_quest_state()->suspend_state);
                return;
            }

            value->set_send_done_flag();

            value->running_quest_state()->args = 1;
            lua_pushstring(value->running_quest_state()->co, msg);

            if (!run_state(*value->running_quest_state())) {
                close_state(*value->running_quest_state());
                value->end_running();
            }
        }

        void quest_manager::select(unsigned int pc, unsigned int selection) {
            game::quest::pc *value;

            if ((value = get_pc(pc)) && value->is_running() && value->running_quest_state()->suspend_state == SUSPEND_STATE_SELECT) {
                value->set_send_done_flag();

                if (!value->running_quest_state()->chat_scripts.empty()) {
                    quest_state &old_qs = *value->running_quest_state();
                    close_state(old_qs);

                    if (selection >= value->running_quest_state()->chat_scripts.size()) {
                        value->set_send_done_flag();
                        goto_end_state(old_qs);
                        value->end_running();
                    } else {
                        a_arg_script *pas = value->running_quest_state()->chat_scripts[selection];
                        execute_quest_script(*value, pas->quest_index, pas->state_index, pas->script.get_code(), pas->script.size());
                    }
                } else {
                    value->running_quest_state()->args = 1;
                    lua_pushnumber(value->running_quest_state()->co, selection + 1);

                    if (!run_state(*value->running_quest_state())) {
                        close_state(*value->running_quest_state());
                        value->end_running();
                    }
                }
            } else {
                metin2::log::warn("Player (pid {:d}) sent an unexpected quest select", pc);
            }
        }

        void quest_manager::resume(unsigned int pc) {
            game::quest::pc *value;

            if ((value = get_pc(pc)) && value->is_running() && value->running_quest_state()->suspend_state == SUSPEND_STATE_PAUSE) {
                value->set_send_done_flag();
                value->running_quest_state()->args = 0;

                if (!run_state(*value->running_quest_state())) {
                    close_state(*value->running_quest_state());
                    value->end_running();
                }
            } else {
                metin2::log::warn("Player (pid {:d}) sent an unexpected quest wait reply", pc);
            }
        }

        void quest_manager::enter_state(uint32_t pc, uint32_t quest_index, int state) {
            game::quest::pc *value;
            if ((value = get_pc(pc))) {
                if (!check_quest_loaded(value))
                    return;

                npc_[QUEST_NO_NPC].on_enter_state(*value, quest_index, state);
            } else
                metin2::log::error("No quest state for player (pid {:d})", pc);
        }

        void quest_manager::leave_state(uint32_t pc, uint32_t quest_index, int state) {
            game::quest::pc *value;
            if ((value = get_pc(pc))) {
                if (!check_quest_loaded(value))
                    return;

                npc_[QUEST_NO_NPC].on_leave_state(*value, quest_index, state);
            } else
                metin2::log::error("No quest state for player (pid {:d})", pc);
        }

        void quest_manager::letter(uint32_t pc, uint32_t quest_index, int state) {
            game::quest::pc *value;
            if ((value = get_pc(pc))) {
                if (!check_quest_loaded(value))
                    return;

                npc_[QUEST_NO_NPC].on_letter(*value, quest_index, state);
            } else
                metin2::log::error("No quest state for player (pid {:d})", pc);
        }

        void quest_manager::logout_pc(character *ch) {
            pc *pc = get_pc(ch->player_id());

            if (pc && pc->is_running()) {
                close_state(*pc->running_quest_state());
                pc->cancel_running();
            }

            logout(ch->player_id());

            if (ch == current_character_) {
                current_character_ = nullptr;
                current_pc_ = nullptr;
            }
        }

        void quest_manager::login(unsigned int pc, const char *quest) {
            game::quest::pc *value;

            if ((value = get_pc(pc))) {
                if (!check_quest_loaded(value))
                    return;

                npc_[QUEST_NO_NPC].on_login(*value, quest);
            } else {
                metin2::log::error("No quest state for player (pid {:d})", pc);
            }
        }

        void quest_manager::logout(unsigned int pc) {
            game::quest::pc *value;

            if ((value = get_pc(pc))) {
                if (!check_quest_loaded(value))
                    return;

                npc_[QUEST_NO_NPC].on_logout(*value);
            } else
                metin2::log::error("No quest state for player (pid {:d})", pc);
        }

        void quest_manager::kill(unsigned int pc, unsigned int npc) {
            game::quest::pc *value;

            metin2::log::debug("Quest: kill event, pid {:d}, npc {:d}", pc, npc);

            if ((value = get_pc(pc))) {
                if (!check_quest_loaded(value))
                    return;

                if (npc > 0) {
                    npc_[npc].on_kill(*value);
                }

                character *ch = current_character_ptr();
                party *party = ch->get_party();
                character *leader = party ? party->leader_character() : ch;

                if (leader) {
                    current_party_member_ = ch;

                    if (npc_[npc].on_party_kill(*get_pc(leader->player_id())))
                        return;

                    value = get_pc(pc);
                }

                if (npc_[QUEST_NO_NPC].on_kill(*value))
                    return;

                if (leader) {
                    current_party_member_ = ch;
                    npc_[QUEST_NO_NPC].on_party_kill(*get_pc(leader->player_id()));
                }
            } else
                metin2::log::error("No quest state for player (pid {:d})", pc);
        }

        auto quest_manager::server_timer(unsigned int npc, unsigned int arg) -> bool {
            set_server_timer_arg(arg);
            metin2::log::debug("Quest: server timer fired, npc {}", static_cast<const void *>(pc_force(0)));
            current_pc_ = pc_force(0);
            current_character_ = nullptr;
            selected_dungeon_ = nullptr;
            return npc_[npc].on_server_timer(*current_pc_);
        }

        auto quest_manager::timer(unsigned int pc, unsigned int npc) -> bool {
            game::quest::pc *value;

            if ((value = get_pc(pc))) {
                if (!check_quest_loaded(value)) {
                    return false;
                }
                return npc_[npc].on_timer(*value);
            } else {
                metin2::log::error("No quest state for player (pid {:d})", pc);
                return false;
            }
        }

        void quest_manager::level_up(unsigned int pc) {
            game::quest::pc *value;

            if ((value = get_pc(pc))) {
                if (!check_quest_loaded(value))
                    return;

                npc_[QUEST_NO_NPC].on_level_up(*value);
            } else {
                metin2::log::error("No quest state for player (pid {:d})", pc);
            }
        }

        void quest_manager::attr_in(unsigned int pc, character *ch, int attr) {
            game::quest::pc *value;
            if ((value = get_pc(pc))) {
                current_party_member_ = ch;
                if (!check_quest_loaded(value))
                    return;

                npc_[attr + QUEST_ATTR_NPC_START].on_attr_in(*value);
            } else {
                metin2::log::error("No quest state for player (pid {:d})", pc);
            }
        }

        void quest_manager::attr_out(unsigned int pc, character *ch, int attr) {
            game::quest::pc *value;
            if ((value = get_pc(pc))) {
                current_party_member_ = ch;
                if (!check_quest_loaded(value))
                    return;

                npc_[attr + QUEST_ATTR_NPC_START].on_attr_out(*value);
            } else {
                metin2::log::error("No quest state for player (pid {:d})", pc);
            }
        }

        auto quest_manager::target(unsigned int pc, uint32_t quest_index, const char *target_name, const char *verb) -> bool {
            game::quest::pc *value;

            if ((value = get_pc(pc))) {
                if (!check_quest_loaded(value))
                    return false;

                bool ret;
                return npc_[QUEST_NO_NPC].on_target(*value, quest_index, target_name, verb, ret);
            }

            return false;
        }

        void quest_manager::quest_info(unsigned int pc, unsigned int quest_index) {
            game::quest::pc *value;

            if ((value = get_pc(pc))) {
                if (!check_quest_loaded(value)) {
                    character *ch = get_character_manager().find_by_pid(pc);

                    if (ch)
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("퀘스트를 로드하는 중입니다. 잠시만 기다려 주십시오."));

                    return;
                }

                value->mark_quest_opened(quest_index);
                npc_[QUEST_NO_NPC].on_info(*value, quest_index);
            } else {
                metin2::log::error("No quest state for player (pid {:d})", pc);
            }
        }

        void quest_manager::quest_button(unsigned int pc, unsigned int quest_index) {
            game::quest::pc *value;
            if ((value = get_pc(pc))) {
                if (!check_quest_loaded(value)) {
                    character *ch = get_character_manager().find_by_pid(pc);
                    if (ch) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("퀘스트를 로드하는 중입니다. 잠시만 기다려 주십시오."));
                    }
                    return;
                }
                value->mark_quest_opened(quest_index);
                npc_[QUEST_NO_NPC].on_button(*value, quest_index);
            } else {
                metin2::log::error("No quest state for player (pid {:d})", pc);
            }
        }

        auto quest_manager::take_item(unsigned int pc, unsigned int npc, item *item) -> bool {
            game::quest::pc *value;

            if ((value = get_pc(pc))) {
                if (!check_quest_loaded(value)) {
                    character *ch = get_character_manager().find_by_pid(pc);
                    if (ch) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("퀘스트를 로드하는 중입니다. 잠시만 기다려 주십시오."));
                    }
                    return false;
                }
                set_current_item(item);
                return npc_[npc].on_take_item(*value);
            } else {
                metin2::log::error("No quest state for player (pid {:d})", pc);
                return false;
            }
        }

        auto quest_manager::use_item(unsigned int pc, item *item, bool receive_all) -> bool {
            if (metin2::config::test_server())
                metin2::log::info("Quest: use item (vnum {:d}) by pid {:d}", item->original_vnum(), pc);
            game::quest::pc *value;
            if ((value = get_pc(pc))) {
                if (!check_quest_loaded(value)) {
                    character *ch = get_character_manager().find_by_pid(pc);
                    if (ch) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("퀘스트를 로드하는 중입니다. 잠시만 기다려 주십시오."));
                    }
                    return false;
                }
                set_current_item(item);

                return npc_[item->vnum()].on_use_item(*value, receive_all);
            } else {
                metin2::log::error("No quest state for player (pid {:d})", pc);
                return false;
            }
        }

        auto quest_manager::sig_use(unsigned int pc, uint32_t sig_vnum, item *item, bool receive_all) -> bool {
            if (metin2::config::test_server())
                metin2::log::info("Quest: sig use item (vnum {:d}) by pid {:d}", item->original_vnum(), pc);
            game::quest::pc *value;
            if ((value = get_pc(pc))) {
                if (!check_quest_loaded(value)) {
                    character *ch = get_character_manager().find_by_pid(pc);
                    if (ch) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("퀘스트를 로드하는 중입니다. 잠시만 기다려 주십시오."));
                    }
                    return false;
                }
                set_current_item(item);

                return npc_[sig_vnum].on_sig_use(*value, receive_all);
            } else {
                metin2::log::error("No quest state for player (pid {:d})", pc);
                return false;
            }
        }

        auto quest_manager::give_item_to_pc(unsigned int pc, character *chr) -> bool {
            if (!chr->is_pc())
                return false;

            game::quest::pc *value = get_pc(pc);

            if (value) {
                if (!check_quest_loaded(value))
                    return false;

                target_info *info = get_target_manager().get_target_info(pc, TARGET_TYPE_VID, chr->get_vid());

                if (info) {
                    bool ret;

                    if (npc_[QUEST_NO_NPC].on_target(*value, info->quest_index, info->target_name, "click", ret))
                        return true;
                }
            }

            return false;
        }

        auto quest_manager::click(unsigned int pc, character *chr_target) -> bool {
            game::quest::pc *value = get_pc(pc);

            if (value) {
                if (!check_quest_loaded(value)) {
                    character *ch = get_character_manager().find_by_pid(pc);

                    if (ch)
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("퀘스트를 로드하는 중입니다. 잠시만 기다려 주십시오."));

                    return false;
                }

                target_info *info_value = get_target_manager().get_target_info(pc, TARGET_TYPE_VID, chr_target->get_vid());
                if (metin2::config::test_server()) {
                    metin2::log::info("Quest: pid {:d} clicked {}, target info {}", pc, chr_target->name(), static_cast<const void *>(static_cast<void *>(info_value)));
                }

                if (info_value) {
                    bool ret;
                    if (npc_[QUEST_NO_NPC].on_target(*value, info_value->quest_index, info_value->target_name, "click", ret))
                        return ret;
                }

                uint32_t current_npc_race = chr_target->race_num();

                if (chr_target->is_npc()) {
                    auto it = npc_.find(current_npc_race);

                    if (it == npc_.end()) {
                        metin2::log::error("Player (pid {:d}) clicked {} but npc race {:d} has no quests",
                            pc,
                            chr_target->name(),
                            current_npc_race);
                        return false;
                    }

                    if (it->second.has_chat()) {
                        if (metin2::config::test_server())
                            metin2::log::info("Quest: click opens chat");

                        if (!it->second.on_chat(*value)) {
                            if (metin2::config::test_server())
                                metin2::log::info("Quest: click chat found nothing");

                            return it->second.on_click(*value);
                        }

                        return true;
                    } else {
                        return it->second.on_click(*value);
                    }
                }
                return false;
            } else {
                metin2::log::error("No quest state for player (pid {:d})", pc);
                return false;
            }
        }

        void quest_manager::unmount(unsigned int pc) {
            game::quest::pc *value;

            if ((value = get_pc(pc))) {
                if (!check_quest_loaded(value))
                    return;

                npc_[QUEST_NO_NPC].on_unmount(*value);
            } else
                metin2::log::error("No quest state for player (pid {:d})", pc);
        }
        void quest_manager::item_informer(unsigned int pc, unsigned int vnum) {

            game::quest::pc *value;
            value = get_pc(pc);

            npc_[QUEST_NO_NPC].on_item_informer(*value, vnum);
        }

        void quest_manager::load_start_quest(const std::string &quest_name, unsigned int idx) {
            if (const std::string *condition = scripts_.find("begin_condition", quest_name)) {
                metin2::log::debug("Quest: loading begin condition for {}", quest_name);
                std::ranges::copy(*condition, back_inserter(hm_quest_start_script_[idx]));
            }
        }

        auto quest_manager::can_start_quest(unsigned int quest_index, const pc &pc) -> bool {
            return can_start_quest(quest_index);
        }

        auto quest_manager::can_start_quest(unsigned int quest_index) -> bool {
            hash_map_quest_start_script::iterator it;

            if ((it = hm_quest_start_script_.find(quest_index)) == hm_quest_start_script_.end())
                return true;
            else {
                int x = lua_gettop(l_);
                lua_dobuffer(l_, &(it->second[0]), it->second.size(), "StartScript");
                int start = lua_toboolean(l_, -1);
                lua_settop(l_, x);
                return start != 0;
            }
        }

        void quest_manager::disconnect_pc(character *ch) {
            pc_.erase(ch->player_id());
        }

        auto quest_manager::pc_force(unsigned int pid) -> pc * {
            pc_map::iterator it;

            if ((it = pc_.find(pid)) == pc_.end()) {
                game::quest::pc *pc = &pc_[pid];
                pc->set_id(pid);
                return pc;
            }

            return &it->second;
        }

        auto quest_manager::get_pc(unsigned int pid) -> pc * {
            pc_map::iterator it;

            character *chr = get_character_manager().find_by_pid(pid);

            if (!chr)
                return nullptr;

            current_pc_ = pc_force(pid);
            current_character_ = chr;
            selected_dungeon_ = nullptr;
            return (current_pc_);
        }

        void quest_manager::clear_script() {
            script_.clear();
            current_skin_ = QUEST_SKIN_NORMAL;
        }

        void quest_manager::add_script(const std::string &str) {
            script_ += str;
        }

        void quest_manager::send_script() {
            if (no_send_) {
                no_send_ = false;
                clear_script();
                return;
            }

            // A run that only set a letter: nothing to show.
            if (script_ == "[DONE]" && current_skin_ == QUEST_SKIN_NOWINDOW) {
                clear_script();
                return;
            }

            if (script_ == "[DONE]" || script_ == "[NEXT]") {
                if (current_pc_ && !current_pc_->and_reset_done_flag() && script_ == "[DONE]" && current_skin_ == QUEST_SKIN_NORMAL && !is_error()) {
                    clear_script();
                    return;
                }
                current_skin_ = QUEST_SKIN_NOWINDOW;
            }

            // The dialog names the quest it belongs to and the NPC speaking; a cinema must stay the
            // first command.
            if (current_skin_ != QUEST_SKIN_NOWINDOW && current_pc_ && !script_.starts_with("[RUN_CINEMA")) {
                const int quest_index = current_pc_->is_running() ? current_pc_->running_quest_state()->index : 0;
                character *npc = current_npc_character_ptr();
                const unsigned int npc_vnum = npc && !npc->is_pc() ? npc->race_num() : 0;
                if (quest_index || npc_vnum)
                    script_.insert(0, std::format("[QUEST idx;{:d}|npc;{:d}]", quest_index, npc_vnum));
            }

            struct metin2::game::packet_script packet_script;

            packet_script.header = HEADER_GC_SCRIPT;
            packet_script.skin = current_skin_;
            packet_script.src_size = script_.size();
            packet_script.size = packet_script.src_size + sizeof(struct game::protocol::packet_script);

            temp_buffer buf;
            buf.write(&packet_script, sizeof(struct game::protocol::packet_script));
            buf.write(&script_[0], script_.size());

            current_character_ptr()->get_desc()->packet(buf.read_peek(), buf.size());

            if (metin2::config::test_server())
                metin2::log::info("Quest: script {} (size {:d})", script_, buf.size());

            clear_script();
        }

        auto quest_manager::quest_state_name(const std::string &quest_name, const int state_index) -> const char * {
            int x = lua_gettop(l_);
            lua_getglobal(l_, quest_name.c_str());
            if (lua_isnil(l_, -1)) {
                metin2::log::error("Bad state file for quest {} state {:d}", quest_name, state_index);
                lua_settop(l_, x);
                return "";
            }
            lua_pushnumber(l_, state_index);
            lua_gettable(l_, -2);

            const char *str = lua_tostring(l_, -1);
            lua_settop(l_, x);
            return str;
        }

        auto quest_manager::quest_state_index(const std::string &quest_name, const std::string &state_name) -> int {
            int x = lua_gettop(l_);
            lua_getglobal(l_, quest_name.c_str());
            if (lua_isnil(l_, -1)) {
                metin2::log::error("Bad state file for quest {} state {}", quest_name, state_name);
                lua_settop(l_, x);
                return 0;
            }
            lua_pushstring(l_, state_name.c_str());
            lua_gettable(l_, -2);

            int v = (int)rint(lua_tonumber(l_, -1));
            lua_settop(l_, x);
            if (metin2::config::test_server())
                metin2::log::info("Quest: state {3} of quest {2} has index {0:d}", v, x, quest_name, state_name);
            return v;
        }

        void quest_manager::set_skin_style(int style) {
            if (style < 0 || style >= QUEST_SKIN_COUNT) {
                current_skin_ = QUEST_SKIN_NORMAL;
            } else
                current_skin_ = style;
        }

        auto quest_manager::load_timer_script(const std::string &name) -> unsigned int {
            std::map<std::string, unsigned int>::iterator it;
            if ((it = timer_id_.find(name)) != timer_id_.end()) {
                return it->second;
            } else {
                unsigned int new_id = UINT_MAX - timer_id_.size();

                npc_[new_id].set(new_id, name);
                timer_id_.insert(make_pair(name, new_id));

                return new_id;
            }
        }

        auto quest_manager::current_npc_race() -> unsigned int {
            return current_npc_character_ptr() ? current_npc_character_ptr()->race_num() : 0;
        }

        auto quest_manager::current_item() -> item * {
            return current_character_ptr() ? current_character_ptr()->quest_item_ptr() : nullptr;
        }

        void quest_manager::clear_current_item() {
            if (current_character_ptr())
                current_character_ptr()->clear_quest_item_ptr();
        }

        void quest_manager::set_current_item(item *item) {
            if (current_character_ptr())
                current_character_ptr()->set_quest_item_ptr(item);
        }

        auto quest_manager::current_npc_character_ptr() -> character * {
            return current_character_ptr() ? current_character_ptr()->quest_npc() : nullptr;
        }

        auto quest_manager::current_quest_name() -> const std::string & {
            return current_pc()->current_quest_name();
        }

        auto quest_manager::current_dungeon() -> dungeon * {
            character *ch = current_character_ptr();

            if (!ch) {
                if (selected_dungeon_)
                    return selected_dungeon_;
                return nullptr;
            }

            return ch->dungeon_force();
        }

        void quest_manager::register_quest(const std::string &quest_name, unsigned int idx) {
            assert(idx > 0);

            decltype(hm_quest_name_.begin()) it;

            if ((it = hm_quest_name_.find(quest_name)) != hm_quest_name_.end())
                return;

            hm_quest_name_.insert(make_pair(quest_name, idx));
            load_start_quest(quest_name, idx);
            quest_name_by_index_.insert(make_pair(idx, quest_name));

            metin2::log::debug("Quest: registered {} as {:d}", quest_name, idx);
        }

        auto quest_manager::quest_index_by_name(const std::string &name) -> unsigned int {
            auto it = hm_quest_name_.find(name);

            if (it == hm_quest_name_.end())
                return 0;

            return it->second;
        }

        auto quest_manager::quest_name_by_index(unsigned int idx) -> const std::string & {
            decltype(quest_name_by_index_.begin()) it;

            if ((it = quest_name_by_index_.find(idx)) == quest_name_by_index_.end()) {
                metin2::log::error("No quest with index {:d}", idx);
                assert(!"cannot find quest name by index");

                static std::string st = "";
                return st;
            }

            return it->second;
        }

        void quest_manager::send_event_flag_list(character *ch) {
            decltype(event_flag_.begin()) it;
            for (it = event_flag_.begin(); it != event_flag_.end(); ++it) {
                const std::string &flagname = it->first;
                int value = it->second;

                if (!metin2::config::test_server() && value == 1 && flagname == "valentine_drop")
                    ch->chat_packet(CHAT_TYPE_INFO, "%s %d prob 800", flagname.c_str(), value);
                else if (!metin2::config::test_server() && value == 1 && flagname == "newyear_wonso")
                    ch->chat_packet(CHAT_TYPE_INFO, "%s %d prob 500", flagname.c_str(), value);
                else if (!metin2::config::test_server() && value == 1 && flagname == "newyear_fire")
                    ch->chat_packet(CHAT_TYPE_INFO, "%s %d prob 1000", flagname.c_str(), value);
                else
                    ch->chat_packet(CHAT_TYPE_INFO, "%s %d", flagname.c_str(), value);
            }
        }

        void quest_manager::request_set_event_flag(const std::string &name, int value) {
            packet_set_event_flag p;
            strlcpy(p.flag_name, name.c_str(), sizeof(p.flag_name));
            p.value = value;
            db_clientdesc->db_packet(HEADER_GD_SET_EVENT_FLAG, 0, &p, sizeof(packet_set_event_flag));
        }

        void quest_manager::set_event_flag(const std::string &name, int value) {
            static const char *dropevent_chartone_name = "drop_char_stone";
            static const int dropevent_chartone_name_len = strlen(dropevent_chartone_name);

            int prev_value = event_flag_[name];

            metin2::log::info("Event flag {} set to {:d} (was {:d})", name, value, event_flag_[name]);
            event_flag_[name] = value;

            if (name == "mob_item") {
                get_character_manager().set_mob_item_rate(value);
            } else if (name == "mob_dam") {
                get_character_manager().set_mob_damage_rate(value);
            } else if (name == "mob_gold") {
                get_character_manager().set_mob_gold_amount_rate(value);
            } else if (name == "mob_gold_pct") {
                get_character_manager().set_mob_gold_drop_rate(value);
            } else if (name == "mob_exp") {
                get_character_manager().set_mob_exp_rate(value);
            } else if (name == "mob_item_buyer") {
                get_character_manager().set_mob_item_rate_premium(value);
            } else if (name == "mob_exp_buyer") {
                get_character_manager().set_mob_exp_rate_premium(value);
            } else if (name == "mob_gold_buyer") {
                get_character_manager().set_mob_gold_amount_rate_premium(value);
            } else if (name == "mob_gold_pct_buyer") {
                get_character_manager().set_mob_gold_drop_rate_premium(value);
            } else if (!name.compare(0, 5, "xmas_")) {
                xmas::process_event_flag(name, prev_value, value);
            } else if (name == "newyear_boom") {
                const desc_manager::desc_set &descs = get_desc_manager().client_set();

                for (auto it = descs.begin(); it != descs.end(); ++it) {
                    character *ch = (*it)->get_character();

                    if (!ch)
                        continue;

                    ch->chat_packet(CHAT_TYPE_COMMAND, "newyear_boom %d", value);
                }
            } else if (name == "eclipse") {
                std::string mode("");

                if (value == 1) {
                    mode = "dark";
                } else {
                    mode = "light";
                }

                const desc_manager::desc_set &descs = get_desc_manager().client_set();

                for (auto it = descs.begin(); it != descs.end(); ++it) {
                    character *ch = (*it)->get_character();
                    if (!ch)
                        continue;

                    ch->chat_packet(CHAT_TYPE_COMMAND, "DayMode %s", mode.c_str());
                }
            } else if (name == "day") {
                const desc_manager::desc_set &descs = get_desc_manager().client_set();

                for (auto it = descs.begin(); it != descs.end(); ++it) {
                    character *ch = (*it)->get_character();
                    if (!ch)
                        continue;
                    if (value) {
                        ch->chat_packet(CHAT_TYPE_COMMAND, "DayMode dark");
                    } else {
                        ch->chat_packet(CHAT_TYPE_COMMAND, "DayMode light");
                    }
                }

                if (value && !prev_value) {
                    struct npc_sell_firework_position {
                        int32_t map_index;
                        int x;
                        int y;
                    } positions[] = {
                        {1,  615, 618},
                        {3,  500, 625},
                        {21, 598, 665},
                        {23, 476, 360},
                        {41, 318, 629},
                        {43, 478, 375},
                        {0,  0,   0  },
                    };

                    npc_sell_firework_position *p = positions;
                    while (p->map_index) {
                        if (map_allow_find(p->map_index)) {
                            pixel_position pos_base;
                            if (!get_sectree_manager().map_base_position_by_map_index(p->map_index, pos_base)) {
                                metin2::log::error("No base position for map {:d}", p->map_index);
                                ++p;
                                continue;
                            }

                            get_character_manager().spawn_mob(xmas::MOB_XMAS_FIRWORK_SELLER_VNUM, p->map_index, pos_base.x + p->x * 100, pos_base.y + p->y * 100, 0, false, -1);
                        }
                        p++;
                    }
                } else if (!value && prev_value) {
                    character_vector_interactor i;

                    if (get_character_manager().characters_by_race_num(xmas::MOB_XMAS_FIRWORK_SELLER_VNUM, i)) {
                        auto it = i.begin();

                        while (it != i.end()) {
                            get_character_manager().destroy_character(*it++);
                        }
                    }
                }
            } else if (name == "pre_event_hc") {
                const uint32_t event_npc = 20090;

                struct event_npc_position {
                    int32_t map_index;
                    int x;
                    int y;
                } positions[] = {
                    {3,  588, 617},
                    {23, 397, 250},
                    {43, 567, 426},
                    {0,  0,   0  },
                };

                if (value && !prev_value) {
                    event_npc_position *position_value = positions;

                    while (position_value->map_index) {
                        if (map_allow_find(position_value->map_index)) {
                            pixel_position pos;

                            if (!get_sectree_manager().map_base_position_by_map_index(position_value->map_index, pos)) {
                                metin2::log::error("No base position for map {:d}", position_value->map_index);
                                ++position_value;
                                continue;
                            }

                            get_character_manager().spawn_mob(event_npc, position_value->map_index, pos.x + position_value->x * 100, pos.y + position_value->y * 100, 0, false, -1);
                        }
                        position_value++;
                    }
                } else if (!value && prev_value) {
                    character_vector_interactor i;

                    if (get_character_manager().characters_by_race_num(event_npc, i)) {
                        auto it = i.begin();

                        while (it != i.end()) {
                            character *ch = *it++;

                            switch (ch->map_index()) {
                            case 3:
                            case 23:
                            case 43:
                                get_character_manager().destroy_character(ch);
                                break;
                            }
                        }
                    }
                }
            } else if (name.compare(0, dropevent_chartone_name_len, dropevent_chartone_name) == 0) {
                drop_event_char_stone_set_value(name, value);
            } else if (name.compare(0, strlen("refine_box"), "refine_box") == 0) {
                drop_event_refine_box_set_value(name, value);
            } else if (name == "gold_drop_limit_time") {
                gold_drop_time_limit_value = value * 1000;
            } else if (name == "new_xmas_event") {
                static uint32_t new_santa = 20126;
                if (value != 0) {
                    character_vector_interactor i;
                    bool map1_santa_exist = false;
                    bool map21_santa_exist = false;
                    bool map41_santa_exist = false;

                    if (get_character_manager().characters_by_race_num(new_santa, i)) {
                        auto it = i.begin();

                        while (it != i.end()) {
                            character *tch = *(it++);

                            if (tch->map_index() == 1) {
                                map1_santa_exist = true;
                            } else if (tch->map_index() == 21) {
                                map21_santa_exist = true;
                            } else if (tch->map_index() == 41) {
                                map41_santa_exist = true;
                            }
                        }
                    }

                    if (map_allow_find(1) && !map1_santa_exist) {
                        sectree_map *sectree_map = get_sectree_manager().map(1);
                        get_character_manager().spawn_mob(new_santa, 1, sectree_map->setting.base_x + 60800, sectree_map->setting.base_y + 61700, 0, false, 90, true);
                    }
                    if (map_allow_find(21) && !map21_santa_exist) {
                        sectree_map *sectree_map = get_sectree_manager().map(21);
                        get_character_manager().spawn_mob(new_santa, 21, sectree_map->setting.base_x + 59600, sectree_map->setting.base_y + 61000, 0, false, 110, true);
                    }
                    if (map_allow_find(41) && !map41_santa_exist) {
                        sectree_map *sectree_map = get_sectree_manager().map(41);
                        get_character_manager().spawn_mob(new_santa, 41, sectree_map->setting.base_x + 35700, sectree_map->setting.base_y + 74300, 0, false, 140, true);
                    }
                } else {
                    character_vector_interactor i;
                    get_character_manager().characters_by_race_num(new_santa, i);

                    for (auto it = i.begin(); it != i.end(); it++) {
                        get_character_manager().destroy_character(*it);
                    }
                }
            }
        }

        auto quest_manager::event_flag(const std::string &name) -> int {
            auto it = event_flag_.find(name);

            if (it == event_flag_.end())
                return 0;

            return it->second;
        }

        void quest_manager::broadcast_event_flag_on_login(character *ch) {
            int event_flag_value;

            if ((event_flag_value = quest::get_quest_manager().event_flag("xmas_snow"))) {
                ch->chat_packet(CHAT_TYPE_COMMAND, "xmas_snow %d", event_flag_value);
            }

            if ((event_flag_value = quest::get_quest_manager().event_flag("xmas_boom"))) {
                ch->chat_packet(CHAT_TYPE_COMMAND, "xmas_boom %d", event_flag_value);
            }

            if ((event_flag_value = quest::get_quest_manager().event_flag("xmas_tree"))) {
                ch->chat_packet(CHAT_TYPE_COMMAND, "xmas_tree %d", event_flag_value);
            }

            if ((event_flag_value = quest::get_quest_manager().event_flag("day"))) {
                ch->chat_packet(CHAT_TYPE_COMMAND, "DayMode dark");
            }

            if ((event_flag_value = quest::get_quest_manager().event_flag("newyear_boom"))) {
                ch->chat_packet(CHAT_TYPE_COMMAND, "newyear_boom %d", event_flag_value);
            }

            if ((event_flag_value = quest::get_quest_manager().event_flag("eclipse"))) {
                std::string mode;

                if (event_flag_value == 1)
                    mode = "dark";
                else
                    mode = "light";

                ch->chat_packet(CHAT_TYPE_COMMAND, "DayMode %s", mode.c_str());
            }
        }

        void quest_manager::reload() {
            lua_close(l_);
            npc_.clear();
            npc_name_id_.clear();
            hm_quest_name_.clear();
            timer_id_.clear();
            hm_quest_start_script_.clear();
            event_name.clear();
            l_ = nullptr;
            initialize();

            for (auto it = registered_npc_vnum_.begin(); it != registered_npc_vnum_.end(); ++it) {
                uint32_t vnum = *it;
                npc_[vnum].set(vnum, std::format("{}", vnum));
            }
        }

        auto quest_manager::execute_quest_script(pc &pc, uint32_t quest_index, const int state, const char *code, const int code_size, std::vector<a_arg_script *> *chat_scripts, bool use_cache) -> bool {
            return execute_quest_script(pc, get_quest_manager().quest_name_by_index(quest_index), state, code, code_size, chat_scripts, use_cache);
        }

        auto quest_manager::execute_quest_script(pc &pc, const std::string &quest_name, const int state, const char *code, const int code_size, std::vector<a_arg_script *> *chat_scripts_value, bool use_cache) -> bool {
            quest_state qs = get_quest_manager().open_state(quest_name, state);
            if (chat_scripts_value)
                qs.chat_scripts.swap(*chat_scripts_value);

            if (use_cache) {
                lua_getglobal(qs.co, "__codecache");
                lua_pushlightuserdata(qs.co, const_cast<char *>(code));
                lua_rawget(qs.co, -2);
                if (lua_isnil(qs.co, -1)) {

                    lua_pop(qs.co, 1);
                    luaL_loadbuffer(qs.co, code, code_size, quest_name.c_str());
                    lua_pushlightuserdata(qs.co, const_cast<char *>(code));
                    lua_pushvalue(qs.co, -2);
                    lua_rawset(qs.co, -4);
                    lua_remove(qs.co, -2);
                } else {
                    lua_remove(qs.co, -2);
                }
            } else
                luaL_loadbuffer(qs.co, code, code_size, quest_name.c_str());

            pc.set_quest(quest_name, qs);

            quest_state &rqs = *pc.running_quest_state();
            if (!get_quest_manager().run_state(rqs)) {
                get_quest_manager().close_state(rqs);
                pc.end_running();
                return false;
            }
            return true;
        }

        void quest_manager::register_npc_vnum(uint32_t vnum) {
            if (registered_npc_vnum_.find(vnum) != registered_npc_vnum_.end())
                return;

            registered_npc_vnum_.insert(vnum);

            const std::string script_name = std::format("{}", vnum);
            if (scripts_.has_npc(script_name)) {
                metin2::log::debug("Quest: found scripts for npc {}", script_name);
                npc_[vnum].set(vnum, script_name);
            }
        }

        // The old quest compiler refused calls to functions it did not know; check them against the loaded Lua state.
        void quest_manager::report_unknown_functions() {
            for (const std::string &name : scripts_.called_functions) {
                const auto dot = name.find('.');
                lua_getglobal(l_, name.substr(0, dot).c_str());
                if (dot != std::string::npos && lua_istable(l_, -1)) {
                    lua_pushstring(l_, name.substr(dot + 1).c_str());
                    lua_rawget(l_, -2);
                    lua_remove(l_, -2);
                }

                if (lua_isnil(l_, -1)) {
                    metin2::log::warn("Quest scripts call {}, which is not defined", name);
                }
                lua_pop(l_, 1);
            }
        }

        void quest_manager::write_running_state_to_syserr() {
            const char *state_name = quest_state_name(current_quest_name(), current_state()->st);

            std::string event_index_name = "";
            for (auto it = event_name.begin(); it != event_name.end(); ++it) {
                if (it->second == running_event_index_) {
                    event_index_name = it->first;
                    break;
                }
            }

            metin2::log::error("Lua error in quest {} state {} event {}", current_quest_name(), state_name, event_index_name);
            if (current_character_ptr() && metin2::config::test_server())
                current_character_ptr()->chat_packet(CHAT_TYPE_PARTY, "LUA_ERROR: quest %s.%s %s", current_quest_name().c_str(), state_name, event_index_name.c_str());
        }

        void report_error(const std::source_location &where, std::string_view message) {
            metin2::log::write(metin2::log::level::error, where, message);

            if (!metin2::config::test_server())
                return;

            if (character *ch = get_quest_manager().current_character_ptr()) {
                const std::string text(message);
                ch->chat_packet(CHAT_TYPE_PARTY, "error occurred on [%s:%u]", where.function_name(), static_cast<unsigned>(where.line()));
                ch->chat_packet(CHAT_TYPE_PARTY, "%s", text.c_str());
            }
        }

        void quest_manager::add_server_timer(const std::string &name, uint32_t arg, event_ptr event) {
            metin2::log::debug("Quest: add server timer {} arg {:d} ({})", name, arg, static_cast<const void *>(event.get()));
            if (server_timer_.find(make_pair(name, arg)) != server_timer_.end()) {
                metin2::log::error("Server timer {} arg {:d} is already registered", name, arg);
                return;
            }
            server_timer_.insert(make_pair(make_pair(name, arg), event));
        }

        void quest_manager::clear_server_timer_not_cancel(const std::string &name, uint32_t arg) {
            server_timer_.erase(make_pair(name, arg));
        }

        void quest_manager::clear_server_timer(const std::string &name, uint32_t arg) {
            auto it = server_timer_.find(make_pair(name, arg));
            if (it != server_timer_.end()) {
                event_ptr event = it->second;
                event_cancel(&event);
                server_timer_.erase(it);
            }
        }

        void quest_manager::cancel_server_timers(uint32_t arg) {
            std::vector<std::pair<std::string, uint32_t>> server_timers_to_delete;

            for (const auto &kv : server_timer_) {
                if (kv.first.second == arg) {
                    event_ptr event = kv.second;
                    event_cancel(&event);
                    server_timers_to_delete.push_back(kv.first);
                }
            }

            for (const auto &timer : server_timers_to_delete)
                server_timer_.erase(timer);

            server_timers_to_delete.clear();
        }

        void quest_manager::set_server_timer_arg(uint32_t arg) {
            server_timer_arg_ = arg;
        }

        auto quest_manager::server_timer_arg() -> uint32_t {
            return server_timer_arg_;
        }

        void quest_manager::select_dungeon(dungeon *dungeon_value) {
            selected_dungeon_ = dungeon_value;
        }

        auto quest_manager::pickup_item(unsigned int pc, item *item) -> bool {
            if (metin2::config::test_server())
                metin2::log::info("Quest: pick up item (vnum {:d}) by pid {:d}", item->original_vnum(), pc);
            game::quest::pc *value;
            if ((value = get_pc(pc))) {
                if (!check_quest_loaded(value)) {
                    character *ch = get_character_manager().find_by_pid(pc);
                    if (ch) {
                        ch->chat_packet(CHAT_TYPE_INFO, lc_text("퀘스트를 로드하는 중입니다. 잠시만 기다려 주십시오."));
                    }
                    return false;
                }
                set_current_item(item);

                return npc_[item->vnum()].on_pickup_item(*value);
            } else {
                metin2::log::error("No quest state for player (pid {:d})", pc);
                return false;
            }
        }
        void quest_manager::begin_other_pc_block(uint32_t pid) {
            character *ch = current_character_ptr();
            if (nullptr == ch) {
                metin2::log::error("No current player for other_pc_block");
                return;
            }
            if (pc_stack_.empty()) {
                other_pc_block_root_pc_ = current_pc();
            }
            pc_stack_.push_back(current_character_ptr()->player_id());
            get_pc(pid);
        }

        void quest_manager::end_other_pc_block() {
            if (pc_stack_.size() == 0) {
                metin2::log::error("Player stack is already empty (quest {}, state {})", current_quest_name(), current_state()->title);
                return;
            }
            uint32_t pc = pc_stack_.back();
            pc_stack_.pop_back();
            get_pc(pc);

            if (pc_stack_.empty()) {
                other_pc_block_root_pc_ = nullptr;
            }
        }

        auto quest_manager::is_in_other_pc_block() -> bool {
            return !pc_stack_.empty();
        }

        auto quest_manager::other_pc_block_root_pc() -> pc * {
            return other_pc_block_root_pc_;
        }
    }
}
