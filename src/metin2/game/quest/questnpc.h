#pragma once

#include <metin2/config/config.h>
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
			typedef std::map<int, a_state_script_type> a_quest_script_type;
			typedef std::map<unsigned int, a_quest_script_type> quest_map_type;

			typedef std::map<int, std::vector<a_arg_script> > a_arg_quest_script_type;
			typedef std::map<unsigned int, a_arg_quest_script_type> arg_quest_map_type;

			npc();
			~npc();

			void	set(unsigned int vnum, const std::string & script_name);

			static auto has_start_state(const a_quest_script_type & q) -> bool
			{
				return q.find(0) != q.end();
			}

			static auto has_start_state(const a_arg_quest_script_type& q) -> bool
			{
				return q.find(0) != q.end();
			}

			auto	on_server_timer(pc& pc) -> bool;

			auto	on_click(pc& pc) -> bool;
			auto	on_kill(pc& pc) -> bool;
			auto	on_party_kill(pc& pc) -> bool;
			auto	on_timer(pc& pc) -> bool;
			auto	on_level_up(pc& pc) -> bool;
			auto	on_login(pc& pc, const char * quest_name = nullptr) -> bool;
			auto	on_logout(pc& pc) -> bool;
			auto	on_button(pc& pc, unsigned int quest_index) -> bool;
			auto	on_info(pc& pc, unsigned int quest_index) -> bool;
			auto	on_attr_in(pc& pc) -> bool;
			auto	on_attr_out(pc& pc) -> bool;
			auto	on_use_item(pc& pc, bool receive_all) -> bool;
			auto	on_take_item(pc& pc) -> bool;
			auto	on_enter_state(pc& pc, uint32_t quest_index, int state) -> bool;
			auto	on_leave_state(pc& pc, uint32_t quest_index, int state) -> bool;
			auto	on_letter(pc& pc, uint32_t quest_index, int state) -> bool;
			auto	on_chat(pc& pc) -> bool;
			auto	has_chat() -> bool;

			auto	on_item_informer(pc& pc,unsigned int vnum) -> bool;

			auto	on_target(pc& pc, uint32_t quest_index, const char * target_name, const char * verb, bool & ret) -> bool;
			auto	on_unmount(pc& pc) -> bool;

			auto	on_pickup_item(pc& pc) -> bool;
			auto	on_sig_use(pc& pc, bool receive_all) -> bool;

			auto	handle_event(pc& pc, int event_index) -> bool;
			auto	handle_receive_all_event(pc& pc, int event_index) -> bool;
			auto	handle_receive_all_no_wait_event(pc& pc, int event_index) -> bool;

			auto	execute_event_script(pc& pc, int event_index, uint32_t quest_index, int state) -> bool;

		protected:
			template <typename TQuestMapType, typename FuncMatch, typename FuncMiss>
				void matching_quest(pc& pc, TQuestMapType& quest_map, FuncMatch& match, FuncMiss& miss);


			void load_state_script(int event_index, const char* filename, const char* script_name);

			unsigned int vnum_;
			quest_map_type own_quest_[QUEST_EVENT_COUNT];
			arg_quest_map_type own_arg_quest_[QUEST_EVENT_COUNT];
	};

	template <typename TQuestMapType, typename FuncMatch, typename FuncMiss>
		void npc::matching_quest(pc& pc, TQuestMapType& quest_map, FuncMatch& match, FuncMiss& miss)
		{
			if (metin2::config::test_server())
				metin2::log::info( "Click Quest : MatchingQuest");

			game::quest::pc::quest_info_iterator itPCQuest = pc.quest_begin();
			typename TQuestMapType::iterator itQuestMap = quest_map.begin();

			while (itQuestMap != quest_map.end())
			{
				if (itPCQuest == pc.quest_end() || itPCQuest->first > itQuestMap->first)
				{
					miss(itPCQuest, itQuestMap);
					++itQuestMap;
				}
				else if (itPCQuest->first < itQuestMap->first)
				{
					++itPCQuest;
				}
				else
				{
					match(itPCQuest, itQuestMap);
					++itPCQuest;
					++itQuestMap;
				}
			}
		}
}
}
