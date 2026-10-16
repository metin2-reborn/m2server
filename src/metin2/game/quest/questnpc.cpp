#include <metin2/config/config.h>
#include <metin2/game/quest/questnpc.h>

#include <metin2/game/app/config.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/quest/quest.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/quest/questpc.h>
#include <metin2/game/text/locale.hpp>

#include <metin2/log/log.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <strings.h>

namespace metin2::game {

    namespace quest {
        namespace {
            auto event_in_range(int event_index) -> bool {
                if (event_index >= 0 && event_index < QUEST_EVENT_COUNT)
                    return true;
                metin2::log::error("Quest event index {:d} is out of range", event_index);
                return false;
            }

            auto can_start_quest(pc &pc) -> bool {
                if (!pc.is_running())
                    return true;

                if (metin2::config::test_server()) {
                    quest_manager &mgr = quest_manager::instance();

                    metin2::log::warn("Cannot start a quest while quest {} is suspended for player {}",
                        pc.current_quest_name(),
                        mgr.current_character_ptr() ? mgr.current_character_ptr()->name() : "<none>");
                }
                return false;
            }

            auto validate_event(pc &pc, int event_index) -> bool {
                return event_in_range(event_index) && can_start_quest(pc);
            }
        }

        npc::npc() {
            vnum_ = 0;
        }

        npc::~npc() = default;

        void npc::set(unsigned int vnum, const std::string &script_name) {
            vnum_ = vnum;

            for (const auto &[event_name, event_index] : quest_manager::instance().event_name) {
                for (const std::string &object_dir : quest_object_dir) {
                    const std::string event_dir = std::format("{}/{}/{}/", object_dir, script_name, event_name);

                    for (const std::string &name : list_directory(event_dir)) {
                        if (!strncasecmp(name.c_str(), "CVS", 3))
                            continue;

                        metin2::log::debug("Quest: reading {}", name);
                        load_state_script(event_index, (event_dir + name).c_str(), name.c_str());
                    }
                }
            }
        }

        void npc::load_state_script(int event_index, const char *filename, const char *script_name) {
            std::ifstream inf(filename);
            const std::string s(script_name);

            size_t i = s.find('.');

            quest_manager &q = quest_manager::instance();

            const std::string quest_name = s.substr(0, i);

            int quest_index = q.quest_index_by_name(quest_name);

            if (quest_index == 0) {
                fprintf(stderr, "cannot find quest index for %s\n", quest_name.c_str());
                assert(!"cannot find quest index");
                return;
            }

            std::string state_name;

            size_t j = i;
            i = s.find('.', i + 1);

            if (i == s.npos)
                state_name = s.substr(j + 1, s.npos);
            else
                state_name = s.substr(j + 1, i - j - 1);

            int state_index = q.quest_state_index(quest_name, state_name);

            metin2::log::debug("Quest: loading {}: quest {} state {}",
                filename, quest_name, state_name);

            if (i == s.npos) {
                std::istreambuf_iterator<char> ib(inf), ie;
                copy(ib, ie, back_inserter(own_quest_[event_index][quest_index][q.quest_state_index(quest_name, state_name)].code));
            } else {

                j = i;
                i = s.find('.', i + 1);

                if (i == s.npos) {
                    metin2::log::error("Bad quest state index in {} (script {})", filename, script_name);
                    return;
                }

                const int index = strtol(s.substr(j + 1, i - j - 1).c_str(), nullptr, 10);
                j = i;
                i = s.find('.', i + 1);

                if (i != s.npos) {
                    metin2::log::error("Bad quest state name in {} (script {})", filename, script_name);
                    return;
                }

                const std::string type_name = s.substr(j + 1, i - j - 1);

                std::istreambuf_iterator<char> ib(inf), ie;

                own_arg_quest_[event_index][quest_index][state_index].resize(std::max<int>(index + 1, own_arg_quest_[event_index][quest_index][state_index].size()));

                if (type_name == "when") {
                    copy(ib, ie, back_inserter(own_arg_quest_[event_index][quest_index][state_index][index].when_condition));
                } else if (type_name == "arg") {
                    std::string s;
                    getline(inf, s);
                    own_arg_quest_[event_index][quest_index][state_index][index].arg.clear();

                    for (std::string::iterator it = s.begin(); it != s.end(); ++it) {
                        own_arg_quest_[event_index][quest_index][state_index][index].arg += *it;
                    }
                } else if (type_name == "script") {
                    copy(ib, ie, back_inserter(own_arg_quest_[event_index][quest_index][state_index][index].script.code));
                    own_arg_quest_[event_index][quest_index][state_index][index].quest_index = quest_index;
                    own_arg_quest_[event_index][quest_index][state_index][index].state_index = state_index;
                }
            }
        }

        auto npc::on_enter_state(pc &pc, uint32_t quest_index, int state) -> bool {
            return execute_event_script(pc, QUEST_ENTER_STATE_EVENT, quest_index, state);
        }

        auto npc::on_leave_state(pc &pc, uint32_t quest_index, int state) -> bool {
            return execute_event_script(pc, QUEST_LEAVE_STATE_EVENT, quest_index, state);
        }

        auto npc::on_letter(pc &pc, uint32_t quest_index, int state) -> bool {
            return execute_event_script(pc, QUEST_LETTER_EVENT, quest_index, state);
        }

        auto npc::on_target(pc &pc, uint32_t quest_index, const char *target_name, const char *verb, bool &ret) -> bool {
            metin2::log::debug("Quest target: {} verb {} quest {:d}", target_name, verb, quest_index);

            ret = false;

            auto pc_quest = pc.quest_find(quest_index);

            if (pc_quest == pc.quest_end()) {
                metin2::log::debug("Quest target: player has no such quest");
                return false;
            }

            int state_value = pc_quest->second.st;

            a_arg_quest_script_type &r = own_arg_quest_[QUEST_TARGET_EVENT][quest_index];
            auto it = r.find(state_value);

            if (it == r.end()) {
                metin2::log::debug("Quest target: no target event in state {:d}", state_value);
                return false;
            }

            auto it_vec = it->second.begin();

            int target_len = strlen(target_name);

            while (it_vec != it->second.end()) {
                a_arg_script &arg_script = *(it_vec++);
                const char *c_psz_arg = arg_script.arg.c_str();

                metin2::log::debug("Quest target: compare with {} (length {:d})", c_psz_arg, arg_script.arg.length());

                if (strncmp(c_psz_arg, target_name, target_len))
                    continue;

                const char *c_psz_arg_verb = strchr(c_psz_arg, '.');

                if (!c_psz_arg_verb)
                    continue;

                if (strcmp(++c_psz_arg_verb, verb))
                    continue;

                if (arg_script.when_condition.size() > 0)
                    metin2::log::debug("Quest target: when {} (size {:d})", &arg_script.when_condition[0], arg_script.when_condition.size());

                if (arg_script.when_condition.size() != 0 && !is_script_true(&arg_script.when_condition[0], arg_script.when_condition.size()))
                    continue;

                metin2::log::debug("Quest target: run quest {:d} state {:d} code {}", quest_index, state_value, (const char *)arg_script.script.get_code());
                ret = quest_manager::execute_quest_script(pc, quest_index, state_value, arg_script.script.get_code(), arg_script.script.size());
                ret = true;
                return true;
            }

            return false;
        }

        auto npc::on_attr_in(pc &pc) -> bool {
            return handle_event(pc, QUEST_ATTR_IN_EVENT);
        }

        auto npc::on_attr_out(pc &pc) -> bool {
            return handle_event(pc, QUEST_ATTR_OUT_EVENT);
        }

        auto npc::on_take_item(pc &pc) -> bool {
            return handle_event(pc, QUEST_ITEM_TAKE_EVENT);
        }

        auto npc::on_use_item(pc &pc, bool receive_all) -> bool {
            if (receive_all)
                return handle_receive_all_event(pc, QUEST_ITEM_USE_EVENT);
            else
                return handle_event(pc, QUEST_ITEM_USE_EVENT);
        }

        auto npc::on_sig_use(pc &pc, bool receive_all) -> bool {
            if (receive_all)
                return handle_receive_all_event(pc, QUEST_SIG_USE_EVENT);
            else
                return handle_event(pc, QUEST_SIG_USE_EVENT);
        }

        auto npc::on_click(pc &pc) -> bool {
            return handle_event(pc, QUEST_CLICK_EVENT);
        }

        auto npc::on_server_timer(pc &pc) -> bool {
            return handle_receive_all_event(pc, QUEST_SERVER_TIMER_EVENT);
        }

        auto npc::on_timer(pc &pc) -> bool {
            return handle_event(pc, QUEST_TIMER_EVENT);
        }

        auto npc::on_kill(pc &pc) -> bool {
            if (vnum_) {
                return handle_event(pc, QUEST_KILL_EVENT);
            } else {
                return handle_receive_all_event(pc, QUEST_KILL_EVENT);
            }
        }

        auto npc::on_party_kill(pc &pc) -> bool {
            if (vnum_) {
                return handle_event(pc, QUEST_PARTY_KILL_EVENT);
            } else {
                return handle_receive_all_event(pc, QUEST_PARTY_KILL_EVENT);
            }
        }

        auto npc::on_level_up(pc &pc) -> bool {
            return handle_receive_all_event(pc, QUEST_LEVELUP_EVENT);
        }

        auto npc::on_login(pc &pc, const char *quest_name) -> bool {
            bool ret = handle_receive_all_no_wait_event(pc, QUEST_LOGIN_EVENT);
            handle_receive_all_event(pc, QUEST_LETTER_EVENT);
            return ret;
        }

        auto npc::on_logout(pc &pc) -> bool {
            return handle_receive_all_event(pc, QUEST_LOGOUT_EVENT);
        }

        auto npc::on_unmount(pc &pc) -> bool {
            return handle_receive_all_event(pc, QUEST_UNMOUNT_EVENT);
        }

        auto npc::handle_event(pc &pc, int event_index) -> bool {
            if (!validate_event(pc, event_index))
                return false;

            struct matched_script {
                uint32_t quest_index;
                int state;
                const a_state_script_type *script;
            };

            std::vector<uint32_t> new_start_quest_indices;
            std::vector<matched_script> matched;

            matching_quest(pc, own_quest_[event_index], [&](auto &pc_quest, auto &quest_map) {
                    int state = pc_quest->second.st;
                    auto quest_script = quest_map->second.find(state);
                    if (quest_script != quest_map->second.end())
                        matched.push_back({quest_map->first, state, &quest_script->second}); }, [&](auto &, auto &quest_map) {
                    uint32_t quest_index_value = quest_map->first;

                    if (has_start_state(quest_map->second) && quest_manager::instance().can_start_quest(quest_index_value)) {
                        new_start_quest_indices.push_back(quest_index_value);
                    } });

            for (size_t i = 0; i < matched.size(); i++) {
                if (i != 0)
                    quest_manager::instance().get_pc(pc.id());

                const matched_script &m = matched[i];
                quest_manager::execute_quest_script(pc, m.quest_index, m.state, m.script->get_code(), m.script->size());
            }

            quest_map_type &rmap_event_own_quest = own_quest_[event_index];
            for (uint32_t quest_index : new_start_quest_indices) {
                a_state_script_type &script = rmap_event_own_quest[quest_index][0];
                quest_manager::execute_quest_script(pc, quest_index, 0, script.get_code(), script.size());
            }

            return !matched.empty() || !new_start_quest_indices.empty();
        }

        auto npc::handle_receive_all_event(pc &pc, int event_index) -> bool {
            if (!validate_event(pc, event_index))
                return false;

            bool miss_handled = false;
            bool match_handled = false;

            matching_quest(pc, own_quest_[event_index], [&](auto &pc_quest, auto &quest_map) {
                    const a_quest_script_type &quest_script = quest_map->second;
                    int pc_state = pc_quest->second.st;
                    auto quest_script_value = quest_script.find(pc_state);

                    if (quest_script_value != quest_script.end()) {
                        match_handled = true;

                        quest_manager::execute_quest_script(
                            *quest_manager::instance().current_pc(),
                            quest_map->first,
                            pc_state,
                            quest_script_value->second.get_code(),
                            quest_script_value->second.size());
                    } }, [&](auto &, auto &quest_map) {
                    uint32_t quest_index = quest_map->first;

                    if (has_start_state(quest_map->second) && quest_manager::instance().can_start_quest(quest_index)) {
                        const a_quest_script_type &quest_script = quest_map->second;
                        auto it = quest_script.find(QUEST_START_STATE_INDEX);

                        if (it != quest_script.end()) {
                            miss_handled = true;
                            quest_manager::execute_quest_script(
                                *quest_manager::instance().current_pc(),
                                quest_index,
                                QUEST_START_STATE_INDEX,
                                it->second.get_code(),
                                it->second.size());
                        }
                    } });
            return miss_handled || match_handled;
        }

        auto npc::handle_receive_all_no_wait_event(pc &pc, int event_index) -> bool {
            if (!event_in_range(event_index))
                return false;

            bool miss_handled = false;
            bool match_handled = false;

            quest_map_type &rmap_event_own_quest = own_quest_[event_index];
            matching_quest(pc, rmap_event_own_quest, [&](auto &pc_quest, auto &quest_map) {
                    const a_quest_script_type &quest_script = quest_map->second;
                    int pc_state = pc_quest->second.st;
                    auto quest_script_value = quest_script.find(pc_state);

                    if (quest_script_value != quest_script.end()) {
                        auto *pc = quest_manager::instance().current_pc();

                        if (quest_manager::execute_quest_script(
                                *pc,
                                quest_map->first,
                                pc_state,
                                quest_script_value->second.get_code(),
                                quest_script_value->second.size())) {
                            metin2::log::error("Quest {} did not finish its login/logout script",
                                quest_manager::instance().quest_name_by_index(quest_map->first));

                            quest_state &rqs = *pc->running_quest_state();
                            quest_manager::instance().close_state(rqs);
                            pc->end_running();
                        }
                        match_handled = true;
                    } }, [&](auto &, auto &quest_map) {
                    uint32_t quest_index = quest_map->first;

                    if (has_start_state(quest_map->second) && quest_manager::instance().can_start_quest(quest_index)) {
                        const a_quest_script_type &quest_script = quest_map->second;
                        auto it = quest_script.find(QUEST_START_STATE_INDEX);
                        if (it != quest_script.end()) {
                            miss_handled = true;
                            auto *pc = quest_manager::instance().current_pc();
                            if (quest_manager::execute_quest_script(
                                    *pc,
                                    quest_index,
                                    QUEST_START_STATE_INDEX,
                                    it->second.get_code(),
                                    it->second.size())) {
                                metin2::log::error("Quest {} did not finish its login/logout script",
                                    quest_manager::instance().quest_name_by_index(quest_map->first));

                                quest_state &rqs = *pc->running_quest_state();
                                quest_manager::instance().close_state(rqs);
                                pc->end_running();
                            }
                        }
                    } });

            return match_handled || miss_handled;
        }

        auto npc::on_info(pc &pc, unsigned int quest_index) -> bool {
            const int event_index = QUEST_INFO_EVENT;

            if (!can_start_quest(pc))
                return false;

            auto pc_quest = pc.quest_find(quest_index);

            if (pc.quest_end() == pc_quest) {
                metin2::log::error("Player has no state for quest {:d}", quest_index);
                return false;
            }

            quest_map_type &rmap_event_own_quest = own_quest_[event_index];
            auto quest_map = rmap_event_own_quest.find(quest_index);

            const char *quest_name = quest_manager::instance().quest_name_by_index(quest_index).c_str();

            if (quest_map == rmap_event_own_quest.end()) {
                metin2::log::error("Quest {} has no info event", quest_name);
                return false;
            }

            auto quest_script = quest_map->second.find(pc_quest->second.st);

            if (quest_script == quest_map->second.end()) {
                metin2::log::error("Quest {1} has no info script for state {0:d}", pc_quest->second.st, quest_name);
                return false;
            }

            quest_manager::execute_quest_script(pc, quest_index, pc_quest->second.st, quest_script->second.get_code(), quest_script->second.size());
            return true;
        }

        auto npc::on_button(pc &pc, unsigned int quest_index) -> bool {
            const int event_index = QUEST_BUTTON_EVENT;

            if (!can_start_quest(pc))
                return false;

            auto pc_quest = pc.quest_find(quest_index);

            quest_map_type &rmap_event_own_quest = own_quest_[event_index];
            auto quest_map = rmap_event_own_quest.find(quest_index);

            if (quest_map == rmap_event_own_quest.end())
                return false;

            int state = 0;

            if (pc_quest != pc.quest_end()) {
                state = pc_quest->second.st;
            } else {
                if (quest_manager::instance().can_start_quest(quest_map->first, pc) && has_start_state(quest_map->second))
                    state = 0;
                else
                    return false;
            }

            auto quest_script = quest_map->second.find(state);

            if (quest_script == quest_map->second.end())
                return false;

            quest_manager::execute_quest_script(pc, quest_index, state, quest_script->second.get_code(), quest_script->second.size());
            return true;
        }

        auto npc::on_chat(pc &pc) -> bool {
            if (!can_start_quest(pc))
                return false;

            const int event_index = QUEST_CHAT_EVENT;
            std::vector<a_arg_script *> avail_script;

            matching_quest(pc, own_arg_quest_[event_index], [&](auto &pc_quest, auto &quest_map) {
                    int state = pc_quest->second.st;
                    if (quest_map->second.find(state) != quest_map->second.end()) {
                        for (size_t i = 0; i < quest_map->second[state].size(); i++) {
                            if (quest_map->second[state][i].when_condition.size() == 0 ||
                                is_script_true(&quest_map->second[state][i].when_condition[0], quest_map->second[state][i].when_condition.size()))
                                avail_script.push_back(&quest_map->second[state][i]);
                        }
                    } }, [&](auto &, auto &quest_map) {
                    if (quest_manager::instance().can_start_quest(quest_map->first) && has_start_state(quest_map->second)) {
                        for (size_t i = 0; i < quest_map->second[QUEST_START_STATE_INDEX].size(); ++i) {
                            if (quest_map->second[QUEST_START_STATE_INDEX][i].when_condition.size() == 0 ||
                                is_script_true(&quest_map->second[QUEST_START_STATE_INDEX][i].when_condition[0], quest_map->second[QUEST_START_STATE_INDEX][i].when_condition.size()))
                                avail_script.push_back(&quest_map->second[QUEST_START_STATE_INDEX][i]);
                        }
                    } });

            if (avail_script.empty())
                return false;

            {

                std::ostringstream os;
                os << "select(";
                os << '"' << script_to_string(avail_script[0]->arg.c_str()) << '"';
                for (size_t i = 1; i < avail_script.size(); i++) {
                    os << ",\"" << script_to_string(avail_script[i]->arg.c_str()) << '"';
                }
                os << ", '" << lc_text("닫기") << "'";
                os << ")";

                quest_manager::execute_quest_script(pc, "QUEST_CHAT_TEMP_QUEST", 0, os.str().c_str(), os.str().size(), &avail_script, false);
            }

            return true;
        }

        auto npc::has_chat() -> bool {
            return !own_arg_quest_[QUEST_CHAT_EVENT].empty();
        }

        auto npc::execute_event_script(pc &pc, int event_index, uint32_t quest_index, int state) -> bool {
            quest_map_type &r_quest = own_quest_[event_index];

            auto quest_value = r_quest.find(quest_index);
            if (quest_value == r_quest.end()) {
                metin2::log::debug("Quest: no quest for event {:d}, quest {:d}, state {:d}", event_index, quest_index, state);
                return false;
            }

            a_quest_script_type &script_value = quest_value->second;
            auto state_value = script_value.find(state);
            if (state_value == script_value.end()) {
                metin2::log::debug("Quest: no script for event {:d}, quest {:d}, state {:d}", event_index, quest_index, state);
                return false;
            }

            metin2::log::debug("Quest: run event {:d}, quest {:d}, state {:d}", event_index, quest_index, state);
            quest_manager::instance().set_current_event_index(event_index);
            return quest_manager::execute_quest_script(pc, quest_index, state, state_value->second.get_code(), state_value->second.size());
        }

        auto npc::on_pickup_item(pc &pc) -> bool {
            if (vnum_ == 0)
                return handle_receive_all_event(pc, QUEST_ITEM_PICK_EVENT);
            else
                return handle_event(pc, QUEST_ITEM_PICK_EVENT);
        }
        auto npc::on_item_informer(pc &pc, unsigned int vnum) -> bool {
            return handle_event(pc, QUEST_ITEM_INFORMER_EVENT);
        }
    }
}
