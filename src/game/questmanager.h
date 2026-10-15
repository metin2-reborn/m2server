#pragma once

#include "event.h"
#include "lua_incl.h"
#include "quest.h"
#include "questnpc.h"
#include "typedef.h"

#include <common/singleton.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class ITEM;
class CHARACTER;
class CDungeon;

namespace quest
{
	bool IsScriptTrue(const char* code, int size);
	std::string ScriptToString(const std::string& str);

	class CQuestManager : public singleton<CQuestManager>
	{
		public:
			enum
			{
				QUEST_SKIN_NOWINDOW,
				QUEST_SKIN_NORMAL,
				//QUEST_SKIN_CINEMATIC,
				QUEST_SKIN_CINEMATIC=5,
				QUEST_SKIN_COUNT
			};

			typedef std::map<std::string, int>		TEventNameMap;
			typedef std::map<unsigned int, PC>	PCMap;

		public:
			CQuestManager();
			~CQuestManager() override;

			bool		Initialize();
			void		Destroy();

			bool		InitializeLua();
			lua_State *		GetLuaState() { return L; }
			void		AddLuaFunctionTable(const char * c_pszName, luaL_reg * preg);

			TEventNameMap	m_mapEventName;

			QuestState		OpenState(const std::string& quest_name, int state_index);
			void		CloseState(QuestState& qs);
			bool		RunState(QuestState& qs);

			PC *		GetPC(unsigned int pc);
			PC *		GetPCForce(unsigned int pc);	// Gets the PC pointer without changing the current PC.

			unsigned int	GetCurrentNPCRace();
			const std::string & 	GetCurrentQuestName();
			unsigned int	FindNPCIDByName(const std::string& name);

			//void		SetCurrentNPCCharacterPtr(CHARACTER * ch) { m_pkCurrentNPC = ch; }
			CHARACTER *		GetCurrentNPCCharacterPtr();

			void		SetCurrentEventIndex(int index) { m_iRunningEventIndex = index; }

			bool		UseItem(unsigned int pc, CItem * item, bool bReceiveAll);
			bool		PickupItem(unsigned int pc, CItem * item);
			bool		SIGUse(unsigned int pc, uint32_t sig_vnum, CItem * item, bool bReceiveAll);
			bool		TakeItem(unsigned int pc, unsigned int npc, CItem * item);
			CItem *		GetCurrentItem();
			void		ClearCurrentItem();
			void		SetCurrentItem(CItem * item);
			void		AddServerTimer(const std::string& name, uint32_t arg, LPEVENT event);
			void		ClearServerTimer(const std::string& name, uint32_t arg);
			void		ClearServerTimerNotCancel(const std::string& name, uint32_t arg);
			void		CancelServerTimers(uint32_t arg);

			void		SetServerTimerArg(uint32_t dwArg);
			uint32_t		GetServerTimerArg();

			// event over state and stae
			bool		ServerTimer(unsigned int npc, unsigned int arg);

			void		Login(unsigned int pc, const char * c_pszQuestName = nullptr);
			void		Logout(unsigned int pc);
			bool		Timer(unsigned int pc, unsigned int npc);
			bool		Click(unsigned int pc, CHARACTER * pkNPC);
			void		Kill(unsigned int pc, unsigned int npc);
			void		LevelUp(unsigned int pc);
			void		AttrIn(unsigned int pc, CHARACTER * ch, int attr);
			void		AttrOut(unsigned int pc, CHARACTER * ch, int attr);
			bool		Target(unsigned int pc, uint32_t dwQuestIndex, const char * c_pszTargetName, const char * c_pszVerb);
			bool		GiveItemToPC(unsigned int pc, CHARACTER * pkChr);
			void		Unmount(unsigned int pc);

			void		QuestButton(unsigned int pc, unsigned int quest_index);
			void		QuestInfo(unsigned int pc, unsigned int quest_index);

			void		EnterState(uint32_t pc, uint32_t quest_index, int state);
			void		LeaveState(uint32_t pc, uint32_t quest_index, int state);

			void		Letter(uint32_t pc);
			void		Letter(uint32_t pc, uint32_t quest_index, int state);
			
			void		ItemInformer(unsigned int pc, unsigned int vnum);	//German gift feature

			//

			bool		CheckQuestLoaded(PC* pc) { return pc && pc->IsLoaded(); }

			// event occurs in one state
			void		Select(unsigned int pc, unsigned int selection);
			void		Resume(unsigned int pc);
			void		Input(unsigned int pc, const char* msg);
			void		Confirm(unsigned int pc, EQuestConfirmType confirm, unsigned int pc2 = 0);
			void		SelectItem(unsigned int pc, unsigned int selection);

			void		LogoutPC(CHARACTER * ch);
			void		DisconnectPC(CHARACTER * ch);

			QuestState *	GetCurrentState()	{ return m_CurrentRunningState; }

			void 		LoadStartQuest(const std::string& quest_name, unsigned int idx);
			bool		CanStartQuest(unsigned int quest_index, const PC& pc);
			bool		CanStartQuest(unsigned int quest_index);

			CHARACTER *		GetCurrentCharacterPtr() { return m_pCurrentCharacter; }
			CHARACTER *		GetCurrentPartyMember() { return m_pCurrentPartyMember; }
			PC *		GetCurrentPC() { return m_pCurrentPC; }

			CDungeon *		GetCurrentDungeon();
			void		SelectDungeon(CDungeon * pDungeon);

			void		ClearScript();
			void		SendScript();
			void		AddScript(const std::string& str);

			void		BuildStateIndexToName(const char* questName);

			int			GetQuestStateIndex(const std::string& quest_name, const std::string& state_name);
			const char*		GetQuestStateName(const std::string& quest_name, const int state_index);

			void		SetSkinStyle(int iStyle);

			void		SetNoSend() { m_bNoSend = true; }

			unsigned int	LoadTimerScript(const std::string& name);

			void		RegisterQuest(const std::string & name, unsigned int idx);
			unsigned int 	GetQuestIndexByName(const std::string& name);
			const std::string& 	GetQuestNameByIndex(unsigned int idx);

			void		RequestSetEventFlag(const std::string& name, int value);

			void		SetEventFlag(const std::string& name, int value);
			int			GetEventFlag(const std::string& name);
			void		BroadcastEventFlagOnLogin(CHARACTER * ch);

			void		SendEventFlagList(CHARACTER * ch);

			void		Reload();

			void		SetError() { m_bError = true; }
			void		ClearError() { m_bError = false; }
			bool		IsError() { return m_bError; }
			void		WriteRunningStateToSyserr();
			void		QuestError(const char* func, int line, const char* fmt, ...) __attribute__((format(printf, 4, 5)));

			void		RegisterNPCVnum(uint32_t dwVnum);

		private:
			CDungeon *			m_pSelectedDungeon;
			uint32_t			m_dwServerTimerArg;

			std::map<std::pair<std::string, uint32_t>, LPEVENT>	m_mapServerTimer;

			int				m_iRunningEventIndex;

			std::map<std::string, int>		m_mapEventFlag;

			void			GotoSelectState(QuestState& qs);
			void			GotoPauseState(QuestState& qs);
			void			GotoEndState(QuestState& qs);
			void			GotoInputState(QuestState& qs);
			void			GotoConfirmState(QuestState& qs);
			void			GotoSelectItemState(QuestState& qs);

			lua_State *		L;

			bool			m_bNoSend;

			std::set<unsigned int>			m_registeredNPCVnum;
			std::map<unsigned int, NPC>		m_mapNPC;
			std::map<std::string, unsigned int>	m_mapNPCNameID;
			std::map<std::string, unsigned int>	m_mapTimerID;

			QuestState *	m_CurrentRunningState;

			PCMap			m_mapPC;

			CHARACTER *		m_pCurrentCharacter;
			CHARACTER *		m_pCurrentPartyMember;
			PC*				m_pCurrentPC;

			std::string			m_strScript;

			int				m_iCurrentSkin;

			struct stringhash
			{
				size_t operator () (const std::string& str) const
				{
					const unsigned char * s = (const unsigned char*) str.c_str();
					const unsigned char * end = s + str.size();
					size_t h = 0;

					while (s < end)
					{
						h *= 16777619;
						h ^= (unsigned char) *(unsigned char *) (s++);
					}

					return h;

				}
			};

			typedef std::unordered_map<std::string, int, stringhash> THashMapQuestName;
			typedef std::unordered_map<unsigned int, std::vector<char> > THashMapQuestStartScript;

			THashMapQuestName			m_hmQuestName;
			THashMapQuestStartScript	m_hmQuestStartScript;
			std::map<unsigned int, std::string>	m_mapQuestNameByIndex;

			bool						m_bError;

		public:
			static bool ExecuteQuestScript(PC& pc, const std::string& quest_name, const int state, const char* code, const int code_size, std::vector<AArgScript*>* pChatScripts = nullptr, bool bUseCache = true);
			static bool ExecuteQuestScript(PC& pc, uint32_t quest_index, const int state, const char* code, const int code_size, std::vector<AArgScript*>* pChatScripts = nullptr, bool bUseCache = true);
		

		// Objects for begin_other_pc_blcok, end_other_pc_block.
		public:
			void		BeginOtherPCBlock(uint32_t pid);
			void		EndOtherPCBlock();
			bool		IsInOtherPCBlock();
			PC*			GetOtherPCBlockRootPC();
		private:
			PC*			m_pOtherPCBlockRootPC;
			std::vector <uint32_t>	m_vecPCStack;
	};
};

// sys_err for quest functions: the message goes to the quest error log with the running quest's context.
#define quest_err(fmt, ...) quest::CQuestManager::instance().QuestError(__FUNCTION__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)
