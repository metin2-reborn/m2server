#pragma once

#include "config.h"
#include "quest.h"
#include "questpc.h"

#include <libthecore/log.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace quest
{
	enum
	{
		QUEST_START_STATE_INDEX = 0,
		QUEST_FISH_REFINE_STATE_INDEX = -2,
	};

	class PC;

	class NPC
	{
		public:
			// scripts without arguments
			// first: state number
			typedef std::map<int, AStateScriptType> AQuestScriptType;
			// first: quest number
			typedef std::map<unsigned int, AQuestScriptType> QuestMapType;

			// scripts with arguments
			// first: state number
			typedef std::map<int, std::vector<AArgScript> > AArgQuestScriptType;
			// first: quest number
			typedef std::map<unsigned int, AArgQuestScriptType> ArgQuestMapType;

			NPC();
			~NPC();

			void	Set(unsigned int vnum, const std::string & script_name);

			static bool HasStartState(const AQuestScriptType & q)
			{
				return q.find(0) != q.end();
			}

			static bool HasStartState(const AArgQuestScriptType& q)
			{
				return q.find(0) != q.end();
			}

			bool	OnServerTimer(PC& pc);

			bool	OnClick(PC& pc);
			bool	OnKill(PC& pc);
			bool	OnPartyKill(PC& pc);
			bool	OnTimer(PC& pc);
			bool	OnLevelUp(PC& pc);
			bool	OnLogin(PC& pc, const char * c_pszQuestName = nullptr);
			bool	OnLogout(PC& pc);
			bool	OnButton(PC& pc, unsigned int quest_index);
			bool	OnInfo(PC& pc, unsigned int quest_index);
			bool	OnAttrIn(PC& pc);
			bool	OnAttrOut(PC& pc);
			bool	OnUseItem(PC& pc, bool bReceiveAll);
			bool	OnTakeItem(PC& pc);
			bool	OnEnterState(PC& pc, uint32_t quest_index, int state);
			bool	OnLeaveState(PC& pc, uint32_t quest_index, int state);
			bool	OnLetter(PC& pc, uint32_t quest_index, int state);
			bool	OnChat(PC& pc);
			bool	HasChat();

			bool	OnItemInformer(PC& pc,unsigned int vnum);	// Germany gift feature test

			bool	OnTarget(PC& pc, uint32_t dwQuestIndex, const char * c_pszTargetName, const char * c_pszVerb, bool & bRet);
			bool	OnUnmount(PC& pc);

			// ITEM_PICK EVENT
			bool	OnPickupItem(PC& pc);
			// Special item group USE EVENT
			bool	OnSIGUse(PC& pc, bool bReceiveAll);

			bool	HandleEvent(PC& pc, int EventIndex);
			bool	HandleReceiveAllEvent(PC& pc, int EventIndex);
			bool	HandleReceiveAllNoWaitEvent(PC& pc, int EventIndex);

			bool	ExecuteEventScript(PC& pc, int EventIndex, uint32_t dwQuestIndex, int iState);

		protected:
			template <typename TQuestMapType, typename FuncMatch, typename FuncMiss>
				void MatchingQuest(PC& pc, TQuestMapType& QuestMap, FuncMatch& fMatch, FuncMiss& fMiss);

			// true if quest still running, false if ended

			void LoadStateScript(int idx, const char* filename, const char* script_name);

			unsigned int m_vnum;
			QuestMapType m_mapOwnQuest[QUEST_EVENT_COUNT];
			ArgQuestMapType m_mapOwnArgQuest[QUEST_EVENT_COUNT];
	};

	template <typename TQuestMapType, typename FuncMatch, typename FuncMiss>
		void NPC::MatchingQuest(PC& pc, TQuestMapType& QuestMap, FuncMatch& fMatch, FuncMiss& fMiss)
		{
			if (test_server)
				sys_log(0, "Click Quest : MatchingQuest");

			PC::QuestInfoIterator itPCQuest = pc.quest_begin();
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
