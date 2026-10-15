#pragma once

#include <metin2/game/app/config.h>
#include <metin2/game/quest/quest.h>
#include <metin2/game/quest/questpc.h>

#include <metin2/log/log.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace metin2::game {

namespace quest
{
	enum
	{
		QUEST_START_STATE_INDEX = 0,
		QUEST_FISH_REFINE_STATE_INDEX = -2,
	};

	class pc;

	class npc
	{
		public:
			// scripts without arguments
			// first: state number
			typedef std::map<int, a_state_script_type> a_quest_script_type;
			// first: quest number
			typedef std::map<unsigned int, a_quest_script_type> quest_map_type;

			// scripts with arguments
			// first: state number
			typedef std::map<int, std::vector<a_arg_script> > a_arg_quest_script_type;
			// first: quest number
			typedef std::map<unsigned int, a_arg_quest_script_type> arg_quest_map_type;

			npc();
			~npc();

			void	set(unsigned int vnum, const std::string & script_name);

			static bool has_start_state(const a_quest_script_type & q)
			{
				return q.find(0) != q.end();
			}

			static bool has_start_state(const a_arg_quest_script_type& q)
			{
				return q.find(0) != q.end();
			}

			bool	on_server_timer(pc& pc);

			bool	on_click(pc& pc);
			bool	on_kill(pc& pc);
			bool	on_party_kill(pc& pc);
			bool	on_timer(pc& pc);
			bool	on_level_up(pc& pc);
			bool	on_login(pc& pc, const char * c_pszQuestName = nullptr);
			bool	on_logout(pc& pc);
			bool	on_button(pc& pc, unsigned int quest_index);
			bool	on_info(pc& pc, unsigned int quest_index);
			bool	on_attr_in(pc& pc);
			bool	on_attr_out(pc& pc);
			bool	on_use_item(pc& pc, bool bReceiveAll);
			bool	on_take_item(pc& pc);
			bool	on_enter_state(pc& pc, uint32_t quest_index, int state);
			bool	on_leave_state(pc& pc, uint32_t quest_index, int state);
			bool	on_letter(pc& pc, uint32_t quest_index, int state);
			bool	on_chat(pc& pc);
			bool	has_chat();

			bool	on_item_informer(pc& pc,unsigned int vnum);	// Germany gift feature test

			bool	on_target(pc& pc, uint32_t dwQuestIndex, const char * c_pszTargetName, const char * c_pszVerb, bool & bRet);
			bool	on_unmount(pc& pc);

			// ITEM_PICK EVENT
			bool	on_pickup_item(pc& pc);
			// Special item group USE EVENT
			bool	on_sig_use(pc& pc, bool bReceiveAll);

			bool	handle_event(pc& pc, int EventIndex);
			bool	handle_receive_all_event(pc& pc, int EventIndex);
			bool	handle_receive_all_no_wait_event(pc& pc, int EventIndex);

			bool	execute_event_script(pc& pc, int EventIndex, uint32_t dwQuestIndex, int iState);

		protected:
			template <typename TQuestMapType, typename FuncMatch, typename FuncMiss>
				void matching_quest(pc& pc, TQuestMapType& QuestMap, FuncMatch& fMatch, FuncMiss& fMiss);

			// true if quest still running, false if ended

			void load_state_script(int idx, const char* filename, const char* script_name);

			unsigned int vnum_;
			quest_map_type own_quest_[QUEST_EVENT_COUNT];
			arg_quest_map_type own_arg_quest_[QUEST_EVENT_COUNT];
	};

	template <typename TQuestMapType, typename FuncMatch, typename FuncMiss>
		void npc::matching_quest(pc& pc, TQuestMapType& QuestMap, FuncMatch& fMatch, FuncMiss& fMiss)
		{
			if (metin2::config::test_server())
				metin2::log::info( "Click Quest : MatchingQuest");

			game::quest::pc::quest_info_iterator itPCQuest = pc.quest_begin();
			typename TQuestMapType::iterator itQuestMap = QuestMap.begin();

			while (itQuestMap != QuestMap.end())
			{
				if (itPCQuest == pc.quest_end() || itPCQuest->first > itQuestMap->first)
				{
					fMiss(itPCQuest, itQuestMap);
					++itQuestMap;
				}
				else if (itPCQuest->first < itQuestMap->first)
				{
					++itPCQuest;
				}
				else
				{
					fMatch(itPCQuest, itQuestMap);
					++itPCQuest;
					++itQuestMap;
				}
			}
		}
}
}
