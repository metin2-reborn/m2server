#include <metin2/config/config.h>
#include <metin2/game/quest/questnpc.h>

#include <metin2/game/characters/character.h>
#include <metin2/game/app/config.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/quest/quest.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/quest/questpc.h>

#include <metin2/log/log.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <dirent.h>
#include <strings.h>

namespace metin2::game {



namespace quest
{
	npc::npc()
	{
		vnum_ = 0;
	}

	npc::~npc()
	{
	}

	void npc::set(unsigned int vnum, const std::string & script_name)
	{
		vnum_ = vnum;

		char buf[PATH_MAX];

		quest_manager::event_name_map::iterator itEventName = quest_manager::instance().event_name.begin();

		while (itEventName != quest_manager::instance().event_name.end())
		{
			__typeof(itEventName) it = itEventName;
			++itEventName;

			for (auto itObjectDir = quest_object_dir.begin(); itObjectDir != quest_object_dir.end(); ++itObjectDir)
			{	
				int is = snprintf(buf, sizeof(buf), "%s/%s/%s/", itObjectDir->c_str(), script_name.c_str(), it->first.c_str());

				if (is < 0 || is >= (int) sizeof(buf))
					is = sizeof(buf) - 1;

				int event_index = it->second;

				DIR * pdir = opendir(buf);

				if (!pdir)
					continue;

				dirent * pde;

				while ((pde = readdir(pdir)))
				{
					if (pde->d_name[0] == '.')
						continue;

					if (!strncasecmp(pde->d_name, "CVS", 3))
						continue;

					metin2::log::debug("Quest: reading {}", pde->d_name);
					strlcpy(buf + is, pde->d_name, sizeof(buf) - is);
					load_state_script(event_index, buf, pde->d_name);
				}

				closedir(pdir);
			}
		}
	}

	void npc::load_state_script(int event_index, const char* filename, const char* script_name)
	{
		std::ifstream inf(filename);
		const std::string s(script_name);

		size_t i = s.find('.');

		quest_manager & q = quest_manager::instance();


		const std::string stQuestName = s.substr(0, i); 

		int quest_index = q.quest_index_by_name(stQuestName);

		if (quest_index == 0)
		{
			fprintf(stderr, "cannot find quest index for %s\n", stQuestName.c_str());
			assert(!"cannot find quest index");
			return;
		}

		std::string stStateName;

		size_t j = i;
		i = s.find('.', i + 1);

		if (i == s.npos)
			stStateName = s.substr(j + 1, s.npos);
		else
			stStateName = s.substr(j + 1, i - j - 1);

		int state_index = q.quest_state_index(stQuestName, stStateName);

		metin2::log::debug("Quest: loading {}: quest {} state {}",
				filename, stQuestName.c_str(), stStateName.c_str());

		if (i == s.npos)
		{
			std::istreambuf_iterator<char> ib(inf), ie;
			copy(ib, ie, back_inserter(own_quest_[event_index][quest_index][q.quest_state_index(stQuestName, stStateName)].code));
		}
		else
		{

			j = i;
			i = s.find('.', i + 1);

			if (i == s.npos)
			{
				metin2::log::error("Bad quest state index in {} (script {})", filename, script_name);
				return;
			}

			const int index = strtol(s.substr(j + 1, i - j - 1).c_str(), nullptr, 10); 
			j = i;
			i = s.find('.', i + 1);

			if (i != s.npos)
			{
				metin2::log::error("Bad quest state name in {} (script {})", filename, script_name);
				return;
			}

			const std::string type_name = s.substr(j + 1, i - j - 1);

			std::istreambuf_iterator<char> ib(inf), ie;

			own_arg_quest_[event_index][quest_index][state_index].resize(std::max<int>(index + 1, own_arg_quest_[event_index][quest_index][state_index].size()));

			if (type_name == "when")
			{
				copy(ib, ie, back_inserter(own_arg_quest_[event_index][quest_index][state_index][index].when_condition));
			}
			else if (type_name == "arg")
			{
				std::string s;
				getline(inf, s);
				own_arg_quest_[event_index][quest_index][state_index][index].arg.clear();

				for (std::string::iterator it = s.begin(); it != s.end(); ++it)
				{
					own_arg_quest_[event_index][quest_index][state_index][index].arg+=*it;
				}
			}
			else if (type_name == "script")
			{
				copy(ib, ie, back_inserter(own_arg_quest_[event_index][quest_index][state_index][index].script.code));
				own_arg_quest_[event_index][quest_index][state_index][index].quest_index = quest_index;
				own_arg_quest_[event_index][quest_index][state_index][index].state_index = state_index;
			}
		}
	}

	auto npc::on_enter_state(pc& pc, uint32_t quest_index, int state) -> bool
	{
		return execute_event_script(pc, QUEST_ENTER_STATE_EVENT, quest_index, state);
	}

	auto npc::on_leave_state(pc& pc, uint32_t quest_index, int state) -> bool
	{
		return execute_event_script(pc, QUEST_LEAVE_STATE_EVENT, quest_index, state);
	}

	auto npc::on_letter(pc& pc, uint32_t quest_index, int state) -> bool
	{
		return execute_event_script(pc, QUEST_LETTER_EVENT, quest_index, state);
	}

	auto npc::on_target(pc & pc, uint32_t quest_index, const char * target_name, const char * verb, bool & ret) -> bool
	{
		metin2::log::debug("Quest target: {} verb {} quest {:d}", target_name, verb, quest_index);

		ret = false;

		game::quest::pc::quest_info_iterator itPCQuest = pc.quest_find(quest_index);

		if (itPCQuest == pc.quest_end())
		{
			metin2::log::debug("Quest target: player has no such quest");
			return false;
		}

		int iState = itPCQuest->second.st;

		a_arg_quest_script_type & r = own_arg_quest_[QUEST_TARGET_EVENT][quest_index];
		a_arg_quest_script_type::iterator it = r.find(iState);

		if (it == r.end())
		{
			metin2::log::debug("Quest target: no target event in state {:d}", iState);
			return false;
		}

		std::vector<a_arg_script>::iterator it_vec = it->second.begin();

		int iTargetLen = strlen(target_name);

		while (it_vec != it->second.end())
		{
			a_arg_script & argScript = *(it_vec++);
			const char * c_pszArg = argScript.arg.c_str();

			metin2::log::debug("Quest target: compare with {} (length {:d})", c_pszArg, argScript.arg.length());

			if (strncmp(c_pszArg, target_name, iTargetLen))
				continue;

			const char * c_pszArgVerb = strchr(c_pszArg, '.');

			if (!c_pszArgVerb)
				continue;

			if (strcmp(++c_pszArgVerb, verb))
				continue;

			if (argScript.when_condition.size() > 0)
				metin2::log::debug("Quest target: when {} (size {:d})", &argScript.when_condition[0], argScript.when_condition.size());
	
			if (argScript.when_condition.size() != 0 && !is_script_true(&argScript.when_condition[0], argScript.when_condition.size()))
				continue;

			metin2::log::debug("Quest target: run quest {:d} state {:d} code {}", quest_index, iState, (const char *) argScript.script.get_code());
			ret = quest_manager::execute_quest_script(pc, quest_index, iState, argScript.script.get_code(), argScript.script.size());
			ret = true;
			return true;
		}

		return false;
	}

	auto npc::on_attr_in(pc& pc) -> bool
	{
		return handle_event(pc, QUEST_ATTR_IN_EVENT);
	}

	auto npc::on_attr_out(pc& pc) -> bool
	{
		return handle_event(pc, QUEST_ATTR_OUT_EVENT);
	}

	auto npc::on_take_item(pc& pc) -> bool
	{
		return handle_event(pc, QUEST_ITEM_TAKE_EVENT);
	}

	auto npc::on_use_item(pc& pc, bool receive_all) -> bool
	{
		if (receive_all)
			return handle_receive_all_event(pc, QUEST_ITEM_USE_EVENT);
		else
			return handle_event(pc, QUEST_ITEM_USE_EVENT);
	}

	auto npc::on_sig_use(pc& pc, bool receive_all) -> bool
	{
		if (receive_all)
			return handle_receive_all_event(pc, QUEST_SIG_USE_EVENT);
		else
			return handle_event(pc, QUEST_SIG_USE_EVENT);
	}

	auto npc::on_click(pc& pc) -> bool
	{
		return handle_event(pc, QUEST_CLICK_EVENT);
	}

	auto npc::on_server_timer(pc& pc) -> bool
	{
		return handle_receive_all_event(pc, QUEST_SERVER_TIMER_EVENT);
	}

	auto npc::on_timer(pc& pc) -> bool
	{
		return handle_event(pc, QUEST_TIMER_EVENT);
	}

	auto npc::on_kill(pc & pc) -> bool
	{
		if (vnum_)
		{
			return handle_event(pc, QUEST_KILL_EVENT);
		}
		else
		{
			return handle_receive_all_event(pc, QUEST_KILL_EVENT);
		}
	}

	auto npc::on_party_kill(pc & pc) -> bool
	{
		if (vnum_)
		{
			return handle_event(pc, QUEST_PARTY_KILL_EVENT);
		}
		else
		{
			return handle_receive_all_event(pc, QUEST_PARTY_KILL_EVENT);
		}
	}

	auto npc::on_level_up(pc& pc) -> bool
	{
		return handle_receive_all_event(pc, QUEST_LEVELUP_EVENT);
	}

	auto npc::on_login(pc& pc, const char * quest_name) -> bool
	{
		bool bRet = handle_receive_all_no_wait_event(pc, QUEST_LOGIN_EVENT);
		handle_receive_all_event(pc, QUEST_LETTER_EVENT);
		return bRet;
	}

	auto npc::on_logout(pc& pc) -> bool
	{
		return handle_receive_all_event(pc, QUEST_LOGOUT_EVENT);
	}

	auto npc::on_unmount(pc& pc) -> bool
	{
		return handle_receive_all_event(pc, QUEST_UNMOUNT_EVENT);
	}

	struct func_miss_handle_event
	{
		std::vector <uint32_t> vdw_new_start_quest_indices;
		int size;

		func_miss_handle_event() : vdw_new_start_quest_indices(0), size(0)
		{}

		auto matched() -> bool
		{
			return vdw_new_start_quest_indices.size() != 0;
		}

		void operator()(pc::quest_info_iterator& pc_quest, npc::quest_map_type::iterator& quest_map)
		{
			uint32_t dwQuestIndex = quest_map->first;

			if (npc::has_start_state(quest_map->second) && quest_manager::instance().can_start_quest(dwQuestIndex))
			{
				size++;
				vdw_new_start_quest_indices.push_back(dwQuestIndex);
			}
		}
	};

	struct func_match_handle_event
	{
		bool found;

		std::vector <uint32_t> vdw_ques_indices;
		std::vector <int> vi_pc_states;
		std::vector <const char*> vcodes;
		std::vector <int> vcode_sizes;
		int size;

		func_match_handle_event()
			: found(false), vdw_ques_indices(0), vi_pc_states(0), vcodes(0), vcode_sizes(0), size(0)
		{}

		auto matched() -> bool
		{
			return found;
		}

		void operator()(pc::quest_info_iterator& pc_quest, npc::quest_map_type::iterator& quest_map)
		{
			npc::a_quest_script_type::iterator itQuestScript;

			int iState = pc_quest->second.st;
			if ((itQuestScript = quest_map->second.find(iState)) != quest_map->second.end())
			{
				found = true;
				size++;
				vdw_ques_indices.push_back(quest_map->first);
				vi_pc_states.push_back(iState);
				vcodes.push_back(itQuestScript->second.get_code());
				vcode_sizes.push_back(itQuestScript->second.size());
			}
		}
	};

	auto npc::handle_event(pc& pc, int event_index) -> bool
	{
		if (event_index < 0 || event_index >= QUEST_EVENT_COUNT)
		{
			metin2::log::error("Quest event index {:d} is out of range", event_index);
			return false;
		}

		if (pc.is_running()) 
		{
			if (metin2::config::test_server())
			{
				quest_manager & mgr = quest_manager::instance();

				metin2::log::warn("Cannot start a quest while quest {} is suspended for player {}",
						pc.current_quest_name().c_str(),
						mgr.current_character_ptr() ? mgr.current_character_ptr()->name() : "<none>");
			}

			return false;
		}

		func_miss_handle_event fMiss;
		func_match_handle_event fMatch;
		matching_quest(pc, own_quest_[event_index], fMatch, fMiss);

		bool r = false;
		if (fMatch.matched())
		{
			for (int i = 0; i < fMatch.size; i++)
			{
				if ( i != 0 ) {
					quest_manager::instance().get_pc(pc.id());		
				}
				
				quest_manager::execute_quest_script(pc, fMatch.vdw_ques_indices[i], fMatch.vi_pc_states[i],
					fMatch.vcodes[i], fMatch.vcode_sizes[i]);
			}
			r = true;
		}
		if (fMiss.matched())
		{
			quest_map_type& rmapEventOwnQuest = own_quest_[event_index];
			
			for (int i = 0; i < fMiss.size; i++)
			{
				a_state_script_type& script = rmapEventOwnQuest[fMiss.vdw_new_start_quest_indices[i]][0];
				quest_manager::execute_quest_script(pc, fMiss.vdw_new_start_quest_indices[i], 0, script.get_code(), script.size());
			}
			r = true;
		}
		else
		{
			return r;
		}
		return true;
	}

	struct func_miss_handle_receive_all_event
	{
		bool handled;

		func_miss_handle_receive_all_event()
		{
			handled = false;
		}

		void operator() (pc::quest_info_iterator& pc_quest, npc::quest_map_type::iterator& quest_map)
		{
			uint32_t dwQuestIndex = quest_map->first;

			if (npc::has_start_state(quest_map->second) && quest_manager::instance().can_start_quest(dwQuestIndex))
			{
				const npc::a_quest_script_type & QuestScript = quest_map->second;
				auto it = QuestScript.find(QUEST_START_STATE_INDEX);

				if (it != QuestScript.end())
				{
					handled = true;
					quest_manager::execute_quest_script(
							*quest_manager::instance().current_pc(), 
							dwQuestIndex,
							QUEST_START_STATE_INDEX, 
							it->second.get_code(), 
							it->second.size());
				}
			}
		}
	};

	struct func_match_handle_receive_all_event
	{
		bool handled;

		func_match_handle_receive_all_event()
		{
			handled = false;
		}

		void operator() (pc::quest_info_iterator& pc_quest, npc::quest_map_type::iterator& quest_map)
		{
			const npc::a_quest_script_type& QuestScript = quest_map->second;
			int iPCState = pc_quest->second.st;
			auto itQuestScript = QuestScript.find(iPCState);

			if (itQuestScript != QuestScript.end())
			{
				handled = true;

				quest_manager::execute_quest_script(
						*quest_manager::instance().current_pc(), 
						quest_map->first, 
						iPCState, 
						itQuestScript->second.get_code(), 
						itQuestScript->second.size());
			}
		}
	};

	auto npc::handle_receive_all_event(pc& pc, int event_index) -> bool
	{
		if (event_index < 0 || event_index >= QUEST_EVENT_COUNT)
		{
			metin2::log::error("Quest event index {:d} is out of range", event_index);
			return false;
		}

		if (pc.is_running()) 
		{
			if (metin2::config::test_server())
			{
				quest_manager & mgr = quest_manager::instance();

				metin2::log::warn("Cannot start a quest while quest {} is suspended for player {}",
						pc.current_quest_name().c_str(),
						mgr.current_character_ptr() ? mgr.current_character_ptr()->name() : "<none>");
			}

			return false;
		}

		func_miss_handle_receive_all_event fMiss;
		func_match_handle_receive_all_event fMatch;

		matching_quest(pc, own_quest_[event_index], fMatch, fMiss);
		return fMiss.handled || fMatch.handled;
	}


	struct func_miss_handle_receive_all_no_wait_event
	{
		bool handled;

		func_miss_handle_receive_all_no_wait_event()
		{
			handled = false;
		}


		void operator()(pc::quest_info_iterator& pc_quest, npc::quest_map_type::iterator& quest_map)
		{
			uint32_t dwQuestIndex = quest_map->first;

			if (npc::has_start_state(quest_map->second) && quest_manager::instance().can_start_quest(dwQuestIndex))
			{
				const npc::a_quest_script_type& QuestScript = quest_map->second;
				auto it = QuestScript.find(QUEST_START_STATE_INDEX);
				if (it != QuestScript.end())
				{
					handled = true;
					pc* pPC = quest_manager::instance().current_pc();
					if (quest_manager::execute_quest_script(
								*pPC,
								dwQuestIndex,
								QUEST_START_STATE_INDEX, 
								it->second.get_code(), 
								it->second.size()))
					{
						metin2::log::error("Quest {} did not finish its login/logout script",
								quest_manager::instance().quest_name_by_index(quest_map->first).c_str());

						quest_state& rqs = *pPC->running_quest_state();
						quest_manager::instance().close_state(rqs);
						pPC->end_running();
					}
				}
			}
		}
	};

	struct func_match_handle_receive_all_no_wait_event
	{
		bool handled;

		func_match_handle_receive_all_no_wait_event()
		{
			handled = false;
		}

		void operator()(pc::quest_info_iterator & pc_quest, npc::quest_map_type::iterator & quest_map)
		{
			const npc::a_quest_script_type & QuestScript = quest_map->second;
			int iPCState = pc_quest->second.st;
			auto itQuestScript = QuestScript.find(iPCState);

			if (itQuestScript != QuestScript.end())
			{
				pc * pPC = quest_manager::instance().current_pc();

				if (quest_manager::execute_quest_script(
							*pPC,
							quest_map->first, 
							iPCState, 
							itQuestScript->second.get_code(), 
							itQuestScript->second.size()))
				{
					metin2::log::error("Quest {} did not finish its login/logout script",
							quest_manager::instance().quest_name_by_index(quest_map->first).c_str());

					quest_state& rqs = *pPC->running_quest_state();
					quest_manager::instance().close_state(rqs);
					pPC->end_running();
				}
				handled = true;
			}
		}
	};

	auto npc::handle_receive_all_no_wait_event(pc& pc, int event_index) -> bool
	{
		if (event_index<0 || event_index>=QUEST_EVENT_COUNT)
		{
			metin2::log::error("Quest event index {:d} is out of range", event_index);
			return false;
		}

		func_miss_handle_receive_all_no_wait_event fMiss;
		func_match_handle_receive_all_no_wait_event fMatch;

		quest_map_type& rmapEventOwnQuest = own_quest_[event_index];
		matching_quest(pc, rmapEventOwnQuest, fMatch, fMiss);

		return fMatch.handled || fMiss.handled;
	}

	auto npc::on_info(pc & pc, unsigned int quest_index) -> bool
	{
		const int EventIndex = QUEST_INFO_EVENT;

		if (pc.is_running()) 
		{
			if (metin2::config::test_server())
			{
				quest_manager & mgr = quest_manager::instance();

				metin2::log::warn("Cannot start a quest while quest {} is suspended for player {}",
						pc.current_quest_name().c_str(),
						mgr.current_character_ptr() ? mgr.current_character_ptr()->name() : "<none>");
			}

			return false;
		}

		game::quest::pc::quest_info_iterator itPCQuest = pc.quest_find(quest_index);

		if (pc.quest_end() == itPCQuest)
		{
			metin2::log::error("Player has no state for quest {:d}", quest_index);
			return false;
		}

		quest_map_type & rmapEventOwnQuest = own_quest_[EventIndex];
		quest_map_type::iterator itQuestMap = rmapEventOwnQuest.find(quest_index);

		const char * questName = quest_manager::instance().quest_name_by_index(quest_index).c_str();

		if (itQuestMap == rmapEventOwnQuest.end())
		{
			metin2::log::error("Quest {} has no info event", questName);
			return false;
		}

		a_quest_script_type::iterator itQuestScript = itQuestMap->second.find(itPCQuest->second.st);

		if (itQuestScript == itQuestMap->second.end())
		{
			metin2::log::error("Quest {1} has no info script for state {0:d}", itPCQuest->second.st, questName);
			return false;
		}

		quest_manager::execute_quest_script(pc, quest_index, itPCQuest->second.st, itQuestScript->second.get_code(), itQuestScript->second.size());
		return true;
	}

	auto npc::on_button(pc & pc, unsigned int quest_index) -> bool
	{
		const int EventIndex = QUEST_BUTTON_EVENT;

		if (pc.is_running()) 
		{
			if (metin2::config::test_server())
			{
				quest_manager & mgr = quest_manager::instance();

				metin2::log::warn("Cannot start a quest while quest {} is suspended for player {}",
						pc.current_quest_name().c_str(),
						mgr.current_character_ptr() ? mgr.current_character_ptr()->name() : "<none>");
			}

			return false;
		}

		game::quest::pc::quest_info_iterator itPCQuest = pc.quest_find(quest_index);

		quest_map_type & rmapEventOwnQuest = own_quest_[EventIndex];
		quest_map_type::iterator itQuestMap = rmapEventOwnQuest.find(quest_index);

		if (itQuestMap == rmapEventOwnQuest.end())
			return false;

		int iState = 0;

		if (itPCQuest != pc.quest_end())
		{
			iState = itPCQuest->second.st;
		}
		else
		{
			if (quest_manager::instance().can_start_quest(itQuestMap->first, pc) && has_start_state(itQuestMap->second))
				iState = 0;
			else
				return false;
		}

		a_quest_script_type::iterator itQuestScript=itQuestMap->second.find(iState);

		if (itQuestScript==itQuestMap->second.end())
			return false;

		quest_manager::execute_quest_script(pc, quest_index, iState, itQuestScript->second.get_code(), itQuestScript->second.size());
		return true;
	}

	struct func_miss_chat_event
	{
		func_miss_chat_event(std::vector<a_arg_script*>& avail_script_value)
			: avail_script(avail_script_value)
			{}

		void operator()(pc::quest_info_iterator& pc_quest, npc::arg_quest_map_type::iterator& quest_map)
		{
			if (quest_manager::instance().can_start_quest(quest_map->first) && npc::has_start_state(quest_map->second))
			{
				size_t i;
				for (i = 0; i < quest_map->second[QUEST_START_STATE_INDEX].size(); ++i)
				{
					if (quest_map->second[QUEST_START_STATE_INDEX][i].when_condition.size() == 0 || 
							is_script_true(&quest_map->second[QUEST_START_STATE_INDEX][i].when_condition[0], quest_map->second[QUEST_START_STATE_INDEX][i].when_condition.size()))
						avail_script.push_back(&quest_map->second[QUEST_START_STATE_INDEX][i]);
				}
			}
		}

		std::vector<a_arg_script*>& avail_script;
	};

	struct func_match_chat_event
	{
		func_match_chat_event(std::vector<a_arg_script*>& avail_script_value)
			: avail_script(avail_script_value)
			{}

		void operator()(pc::quest_info_iterator& pc_quest, npc::arg_quest_map_type::iterator& quest_map)
		{
			int iState = pc_quest->second.st;
			std::map<int, std::vector<a_arg_script> >::iterator itQuestScript = quest_map->second.find(iState);
			if (itQuestScript != quest_map->second.end())
			{
				size_t i;
				for (i = 0; i < quest_map->second[iState].size(); i++)
				{
					if ( quest_map->second[iState][i].when_condition.size() == 0 ||
							is_script_true(&quest_map->second[iState][i].when_condition[0], quest_map->second[iState][i].when_condition.size()))
						avail_script.push_back(&quest_map->second[iState][i]);
				}
			}
		}

		std::vector<a_arg_script*>& avail_script;
	};

	auto npc::on_chat(pc& pc) -> bool
	{
		if (pc.is_running()) 
		{
			if (metin2::config::test_server())
			{
				quest_manager & mgr = quest_manager::instance();

				metin2::log::warn("Cannot start a quest while quest {} is suspended for player {}",
						pc.current_quest_name().c_str(),
						mgr.current_character_ptr() ? mgr.current_character_ptr()->name() : "<none>");
			}

			return false;
		}

		const int EventIndex = QUEST_CHAT_EVENT;
		std::vector<a_arg_script*> AvailScript;

		func_match_chat_event fMatch(AvailScript);
		func_miss_chat_event fMiss(AvailScript);
		matching_quest(pc, own_arg_quest_[EventIndex], fMatch, fMiss);


		if (AvailScript.empty())
			return false;

		{

			std::ostringstream os;
			os << "select(";
			os << '"' << script_to_string(AvailScript[0]->arg.c_str()) << '"';
			for (size_t i = 1; i < AvailScript.size(); i++)
			{
				os << ",\"" << script_to_string(AvailScript[i]->arg.c_str()) << '"';
			}
			os << ", '"<<lc_text("닫기")<<"'";
			os << ")";

			quest_manager::execute_quest_script(pc, "QUEST_CHAT_TEMP_QUEST", 0, os.str().c_str(), os.str().size(), &AvailScript, false);
		}

		return true;
	}

	auto npc::has_chat() -> bool
	{
		return !own_arg_quest_[QUEST_CHAT_EVENT].empty();
	}

	auto npc::execute_event_script(pc& pc, int event_index, uint32_t quest_index, int state) -> bool
	{
		quest_map_type& rQuest = own_quest_[event_index];

		auto itQuest = rQuest.find(quest_index);
		if (itQuest == rQuest.end())
		{
			metin2::log::debug("Quest: no quest for event {:d}, quest {:d}, state {:d}", event_index, quest_index, state);
			return false;
		}

		a_quest_script_type& rScript = itQuest->second;
		auto itState = rScript.find(state);
		if (itState == rScript.end())
		{
			metin2::log::debug("Quest: no script for event {:d}, quest {:d}, state {:d}", event_index, quest_index, state);
			return false;
		}

		metin2::log::debug("Quest: run event {:d}, quest {:d}, state {:d}", event_index, quest_index, state);
		quest_manager::instance().set_current_event_index(event_index);
		return quest_manager::execute_quest_script(pc, quest_index, state, itState->second.get_code(), itState->second.size());
	}

	auto npc::on_pickup_item(pc& pc) -> bool
	{
		if (vnum_ == 0)
			return handle_receive_all_event(pc, QUEST_ITEM_PICK_EVENT);
		else
			return handle_event(pc, QUEST_ITEM_PICK_EVENT);
	}
	auto npc::on_item_informer(pc& pc, unsigned int vnum) -> bool
	{
		return handle_event(pc, QUEST_ITEM_INFORMER_EVENT);
	}
}
}
