#include "questmanager.h"

#include "buffer_manager.h"
#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "dungeon.h"
#include "event.h"
#include "item.h"
#include "item_manager.h"
#include "locale.hpp"
#include "lua_incl.h"
#include "packet.h"
#include "party.h"
#include "quest.h"
#include "questnpc.h"
#include "questpc.h"
#include "sectree_manager.h"
#include "target.h"
#include "typedef.h"
#include "xmas_event.h"

#include <common/length.h>
#include <common/tables.h>
#include <common/utils.h>
#include <libthecore/log.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <dirent.h>

uint32_t g_GoldDropTimeLimitValue = 0;

namespace quest
{
	using namespace std;

	CQuestManager::CQuestManager()
		: m_pSelectedDungeon(nullptr), m_dwServerTimerArg(0), m_iRunningEventIndex(0), L(nullptr), m_bNoSend (false),
		m_CurrentRunningState(nullptr), m_pCurrentCharacter(nullptr), m_pCurrentPartyMember(nullptr),
		m_pCurrentPC(nullptr),  m_iCurrentSkin(0), m_bError(false), m_pOtherPCBlockRootPC(nullptr)
	{
	}

	CQuestManager::~CQuestManager()
	{
		Destroy();
	}

	void CQuestManager::Destroy()
	{
		if (L)
		{
			lua_close(L);
			L = nullptr;
		}
	}	

	bool CQuestManager::Initialize()
	{
		if (g_bAuthServer)
			return true;

		if (!InitializeLua())
			return false;

		m_pSelectedDungeon = nullptr;

		m_mapEventName.insert(TEventNameMap::value_type("click", QUEST_CLICK_EVENT));		// Clicked an NPC
		m_mapEventName.insert(TEventNameMap::value_type("kill", QUEST_KILL_EVENT));		// Hunted a mob
		m_mapEventName.insert(TEventNameMap::value_type("timer", QUEST_TIMER_EVENT));		// Preset time has passed
		m_mapEventName.insert(TEventNameMap::value_type("levelup", QUEST_LEVELUP_EVENT));	// Leveled up
		m_mapEventName.insert(TEventNameMap::value_type("login", QUEST_LOGIN_EVENT));		// On login
		m_mapEventName.insert(TEventNameMap::value_type("logout", QUEST_LOGOUT_EVENT));		// On logout
		m_mapEventName.insert(TEventNameMap::value_type("button", QUEST_BUTTON_EVENT));		// Pressed the quest button
		m_mapEventName.insert(TEventNameMap::value_type("info", QUEST_INFO_EVENT));		// Opened the quest info window
		m_mapEventName.insert(TEventNameMap::value_type("chat", QUEST_CHAT_EVENT));		// Talked with a specific keyword
		m_mapEventName.insert(TEventNameMap::value_type("in", QUEST_ATTR_IN_EVENT));		// Entered a specific map attribute
		m_mapEventName.insert(TEventNameMap::value_type("out", QUEST_ATTR_OUT_EVENT));		// Left a specific map attribute
		m_mapEventName.insert(TEventNameMap::value_type("use", QUEST_ITEM_USE_EVENT));		// Used a quest item
		m_mapEventName.insert(TEventNameMap::value_type("server_timer", QUEST_SERVER_TIMER_EVENT));	// Server timer (not tested yet)
		m_mapEventName.insert(TEventNameMap::value_type("enter", QUEST_ENTER_STATE_EVENT));	// Became the current state
		m_mapEventName.insert(TEventNameMap::value_type("leave", QUEST_LEAVE_STATE_EVENT));	// Changed from the current state to another state
		m_mapEventName.insert(TEventNameMap::value_type("letter", QUEST_LETTER_EVENT));		// Info must be set anew after login or a state change
		m_mapEventName.insert(TEventNameMap::value_type("take", QUEST_ITEM_TAKE_EVENT));	// Received an item
		m_mapEventName.insert(TEventNameMap::value_type("target", QUEST_TARGET_EVENT));		// Target
		m_mapEventName.insert(TEventNameMap::value_type("party_kill", QUEST_PARTY_KILL_EVENT));	// Party member hunted a monster (sent to leader)
		m_mapEventName.insert(TEventNameMap::value_type("unmount", QUEST_UNMOUNT_EVENT));
		m_mapEventName.insert(TEventNameMap::value_type("pick", QUEST_ITEM_PICK_EVENT));	// Picked up a dropped item.
		m_mapEventName.insert(TEventNameMap::value_type("sig_use", QUEST_SIG_USE_EVENT));		// Used an item belonging to a Special item group.
		m_mapEventName.insert(TEventNameMap::value_type("item_informer", QUEST_ITEM_INFORMER_EVENT));	// German gift feature test

		m_bNoSend = false;

		m_iCurrentSkin = QUEST_SKIN_NORMAL;

		{
			ifstream inf((g_stQuestDir + "/questnpc.txt").c_str());
			int line = 0;

			if (!inf.is_open())
				sys_err( "QUEST Cannot open 'questnpc.txt'");
			else
				sys_log(0, "QUEST can open 'questnpc.txt' (%s)", g_stQuestDir.c_str() );

			while (1)
			{
				unsigned int vnum;

				inf >> vnum;

				line++;

				if (inf.fail())
					break;

				string s;
				getline(inf, s);
				unsigned int li = 0, ri = s.size()-1;
				while (li < s.size() && isspace(s[li])) li++;
				while (ri > 0 && isspace(s[ri])) ri--;

				if (ri < li) 
				{
					sys_err("QUEST questnpc.txt:%d:npc name error",line);
					continue;
				}

				s = s.substr(li, ri-li+1);

				int	n = 0;
				str_to_number(n, s.c_str());
				if (n)
					continue;

				if ( test_server )
					sys_log(0, "QUEST reading script of %s(%d)", s.c_str(), vnum);
				m_mapNPC[vnum].Set(vnum, s);
				m_mapNPCNameID[s] = vnum;
			}

			// notarget quest
			m_mapNPC[0].Set(0, "notarget");
		}

		SetEventFlag("guild_withdraw_delay", 1);
		SetEventFlag("guild_disband_delay", 1);
		return true;
	}

	unsigned int CQuestManager::FindNPCIDByName(const string& name)
	{
		map<string, unsigned int>::iterator it = m_mapNPCNameID.find(name);
		return it != m_mapNPCNameID.end() ? it->second : 0;
	}

	void CQuestManager::SelectItem(unsigned int pc, unsigned int selection)
	{
		PC* pPC = GetPC(pc);
		if (pPC && pPC->IsRunning() && pPC->GetRunningQuestState()->suspend_state == SUSPEND_STATE_SELECT_ITEM)
		{
			pPC->SetSendDoneFlag();
			pPC->GetRunningQuestState()->args=1;
			lua_pushnumber(pPC->GetRunningQuestState()->co,selection);

			if (!RunState(*pPC->GetRunningQuestState()))
			{
				CloseState(*pPC->GetRunningQuestState());
				pPC->EndRunning();
			}
		}
	}

	void CQuestManager::Confirm(unsigned int pc, EQuestConfirmType confirm, unsigned int pc2)
	{
		PC* pPC = GetPC(pc);

		if (!pPC->IsRunning())
		{
			sys_err("no quest running for pc, cannot process input : %u", pc);
			return;
		}

		if (pPC->GetRunningQuestState()->suspend_state != SUSPEND_STATE_CONFIRM)
		{
			sys_err("not wait for a confirm : %u %d", pc, pPC->GetRunningQuestState()->suspend_state);
			return;
		}

		if (pc2 && !pPC->IsConfirmWait(pc2))
		{
			sys_err("not wait for a confirm : %u %d", pc, pPC->GetRunningQuestState()->suspend_state);
			return;
		}

		pPC->ClearConfirmWait();

		pPC->SetSendDoneFlag();

		pPC->GetRunningQuestState()->args=1;
		lua_pushnumber(pPC->GetRunningQuestState()->co, confirm);

		AddScript("[END_CONFIRM_WAIT]");
		SetSkinStyle(QUEST_SKIN_NOWINDOW);
		SendScript();

		if (!RunState(*pPC->GetRunningQuestState()))
		{
			CloseState(*pPC->GetRunningQuestState());
			pPC->EndRunning();
		}

	}

	void CQuestManager::Input(unsigned int pc, const char* msg)
	{
		PC* pPC = GetPC(pc);
		if (!pPC)
		{
			sys_err("no pc! : %u",pc);
			return;
		}

		if (!pPC->IsRunning())
		{
			sys_err("no quest running for pc, cannot process input : %u", pc);
			return;
		}

		if (pPC->GetRunningQuestState()->suspend_state != SUSPEND_STATE_INPUT)
		{
			sys_err("not wait for a input : %u %d", pc, pPC->GetRunningQuestState()->suspend_state);
			return;
		}

		pPC->SetSendDoneFlag();

		pPC->GetRunningQuestState()->args=1;
		lua_pushstring(pPC->GetRunningQuestState()->co,msg);

		if (!RunState(*pPC->GetRunningQuestState()))
		{
			CloseState(*pPC->GetRunningQuestState());
			pPC->EndRunning();
		}
	}

	void CQuestManager::Select(unsigned int pc, unsigned int selection)
	{
		PC* pPC;

		if ((pPC = GetPC(pc)) && pPC->IsRunning() && pPC->GetRunningQuestState()->suspend_state==SUSPEND_STATE_SELECT)
		{
			pPC->SetSendDoneFlag();

			if (!pPC->GetRunningQuestState()->chat_scripts.empty())
			{
				// For chat events
				// The current quest is one that picks which quest to run, so
				// finish it and run the selected quest.
				QuestState& old_qs = *pPC->GetRunningQuestState();
				CloseState(old_qs);

				if (selection >= pPC->GetRunningQuestState()->chat_scripts.size())
				{
					pPC->SetSendDoneFlag();
					GotoEndState(old_qs);
					pPC->EndRunning();
				}
				else
				{
					AArgScript* pas = pPC->GetRunningQuestState()->chat_scripts[selection];
					ExecuteQuestScript(*pPC, pas->quest_index, pas->state_index, pas->script.GetCode(), pas->script.GetSize());
				}
			}
			else
			{
				// on default 
				pPC->GetRunningQuestState()->args=1;
				lua_pushnumber(pPC->GetRunningQuestState()->co,selection+1);

				if (!RunState(*pPC->GetRunningQuestState()))
				{
					CloseState(*pPC->GetRunningQuestState());
					pPC->EndRunning();
				}
			}
		}
		else
		{
			sys_err("wrong QUEST_SELECT request! : %d",pc);
		}
	}

	void CQuestManager::Resume(unsigned int pc)
	{
		PC * pPC;

		if ((pPC = GetPC(pc)) && pPC->IsRunning() && pPC->GetRunningQuestState()->suspend_state == SUSPEND_STATE_PAUSE)
		{
			pPC->SetSendDoneFlag();
			pPC->GetRunningQuestState()->args = 0;

			if (!RunState(*pPC->GetRunningQuestState()))
			{
				CloseState(*pPC->GetRunningQuestState());
				pPC->EndRunning();
			}
		}
		else
		{
			sys_err("wrong QUEST_WAIT request! : %d",pc);
		}
	}

	void CQuestManager::EnterState(uint32_t pc, uint32_t quest_index, int state)
	{
		PC* pPC;
		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
				return;

			m_mapNPC[QUEST_NO_NPC].OnEnterState(*pPC, quest_index, state);
		}
		else
			sys_err("QUEST no such pc id : %d", pc);
	}

	void CQuestManager::LeaveState(uint32_t pc, uint32_t quest_index, int state)
	{
		PC* pPC;
		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
				return;

			m_mapNPC[QUEST_NO_NPC].OnLeaveState(*pPC, quest_index, state);
		}
		else
			sys_err("QUEST no such pc id : %d", pc);
	}

	void CQuestManager::Letter(uint32_t pc, uint32_t quest_index, int state)
	{
		PC* pPC;
		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
				return;

			m_mapNPC[QUEST_NO_NPC].OnLetter(*pPC, quest_index, state);
		}
		else
			sys_err("QUEST no such pc id : %d", pc);
	}

	void CQuestManager::LogoutPC(CHARACTER * ch)
	{
		PC * pPC = GetPC(ch->GetPlayerID());

		if (pPC && pPC->IsRunning())
		{
			CloseState(*pPC->GetRunningQuestState());
			pPC->CancelRunning();
		}

		// Log out before deleting.
		Logout(ch->GetPlayerID());

		if (ch == m_pCurrentCharacter)
		{
			m_pCurrentCharacter = nullptr;
			m_pCurrentPC = nullptr;
		}
	}

	///////////////////////////////////////////////////////////////////////////////////////////
	//
	// Quest Event related
	//
	///////////////////////////////////////////////////////////////////////////////////////////
	void CQuestManager::Login(unsigned int pc, const char * c_pszQuest)
	{
		PC * pPC;

		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
				return;

			m_mapNPC[QUEST_NO_NPC].OnLogin(*pPC, c_pszQuest);
		}
		else
		{
			sys_err("QUEST no such pc id : %d", pc);
		}
	}

	void CQuestManager::Logout(unsigned int pc)
	{
		PC * pPC;

		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
				return;

			m_mapNPC[QUEST_NO_NPC].OnLogout(*pPC);
		}
		else
			sys_err("QUEST no such pc id : %d", pc);
	}

	void CQuestManager::Kill(unsigned int pc, unsigned int npc)
	{
		PC * pPC;

		sys_log(0, "CQuestManager::Kill QUEST_KILL_EVENT (pc=%d, npc=%d)", pc, npc);

		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
				return;

			/* [hyo] fix for duplicate counting on mob kill
			   even if a script was handled due to code like when 171.kill begin ... in the quest script,
			   changed to not return immediately and also perform other checks. (2011/07/21)
			*/   
			// call script
			
			if(npc > 0) // Fix
			{
				m_mapNPC[npc].OnKill(*pPC);
			}

			CHARACTER * ch = GetCurrentCharacterPtr();
			CParty * pParty = ch->GetParty();
			CHARACTER * leader = pParty ? pParty->GetLeaderCharacter() : ch;

			if (leader)
			{
				m_pCurrentPartyMember = ch;

				if (m_mapNPC[npc].OnPartyKill(*GetPC(leader->GetPlayerID())))
					return;

				pPC = GetPC(pc);
			}

			if (m_mapNPC[QUEST_NO_NPC].OnKill(*pPC))
				return;

			if (leader)
			{
				m_pCurrentPartyMember = ch;
				m_mapNPC[QUEST_NO_NPC].OnPartyKill(*GetPC(leader->GetPlayerID()));
			}
		}
		else
			sys_err("QUEST: no such pc id : %d", pc);
	}

	bool CQuestManager::ServerTimer(unsigned int npc, unsigned int arg)
	{
		SetServerTimerArg(arg);
		sys_log(0, "XXX ServerTimer Call NPC %p", GetPCForce(0));
		m_pCurrentPC = GetPCForce(0);
		m_pCurrentCharacter = nullptr;
		m_pSelectedDungeon = nullptr;
		return m_mapNPC[npc].OnServerTimer(*m_pCurrentPC);
	}

	bool CQuestManager::Timer(unsigned int pc, unsigned int npc)
	{
		PC* pPC;

		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
			{
				return false;
			}
			// call script
			return m_mapNPC[npc].OnTimer(*pPC);
		}
		else
		{
			sys_err("QUEST TIMER_EVENT no such pc id : %d", pc);
			return false;
		}
	}

	void CQuestManager::LevelUp(unsigned int pc)
	{
		PC * pPC;

		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
				return;

			m_mapNPC[QUEST_NO_NPC].OnLevelUp(*pPC);
		}
		else
		{
			sys_err("QUEST LEVELUP_EVENT no such pc id : %d", pc);
		}
	}

	void CQuestManager::AttrIn(unsigned int pc, CHARACTER * ch, int attr)
	{
		PC* pPC;
		if ((pPC = GetPC(pc)))
		{
			m_pCurrentPartyMember = ch;
			if (!CheckQuestLoaded(pPC))
				return;

			// call script
			m_mapNPC[attr+QUEST_ATTR_NPC_START].OnAttrIn(*pPC);
		}
		else
		{
			sys_err("QUEST no such pc id : %d", pc);
		}
	}

	void CQuestManager::AttrOut(unsigned int pc, CHARACTER * ch, int attr)
	{
		PC* pPC;
		if ((pPC = GetPC(pc)))
		{
			m_pCurrentPartyMember = ch;
			if (!CheckQuestLoaded(pPC))
				return;

			// call script
			m_mapNPC[attr+QUEST_ATTR_NPC_START].OnAttrOut(*pPC);
		}
		else
		{
			sys_err("QUEST no such pc id : %d", pc);
		}
	}

	bool CQuestManager::Target(unsigned int pc, uint32_t dwQuestIndex, const char * c_pszTargetName, const char * c_pszVerb)
	{
		PC * pPC;

		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
				return false;

			bool bRet;
			return m_mapNPC[QUEST_NO_NPC].OnTarget(*pPC, dwQuestIndex, c_pszTargetName, c_pszVerb, bRet);
		}

		return false;
	}

	void CQuestManager::QuestInfo(unsigned int pc, unsigned int quest_index)
	{
		PC* pPC;

		if ((pPC = GetPC(pc)))
		{
			// call script
			if (!CheckQuestLoaded(pPC))
			{
				CHARACTER * ch = CHARACTER_MANAGER::instance().FindByPID(pc);

				if (ch)
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("퀘스트를 로드하는 중입니다. 잠시만 기다려 주십시오."));

				return;
			}

			m_mapNPC[QUEST_NO_NPC].OnInfo(*pPC, quest_index);
		}
		else
		{
			sys_err("QUEST INFO_EVENT no such pc id : %d", pc);
		}
	}

	void CQuestManager::QuestButton(unsigned int pc, unsigned int quest_index)
	{
		PC* pPC;
		if ((pPC = GetPC(pc)))
		{
			// call script
			if (!CheckQuestLoaded(pPC))
			{
				CHARACTER * ch = CHARACTER_MANAGER::instance().FindByPID(pc);
				if (ch)
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("퀘스트를 로드하는 중입니다. 잠시만 기다려 주십시오."));
				}
				return;
			}
			m_mapNPC[QUEST_NO_NPC].OnButton(*pPC, quest_index);
		}
		else
		{
			sys_err("QUEST CLICK_EVENT no such pc id : %d", pc);
		}
	}

	bool CQuestManager::TakeItem(unsigned int pc, unsigned int npc, CItem * item)
	{
		PC* pPC;

		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
			{
				CHARACTER * ch = CHARACTER_MANAGER::instance().FindByPID(pc);
				if (ch)
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("퀘스트를 로드하는 중입니다. 잠시만 기다려 주십시오."));
				}
				return false;
			}
			// call script
			SetCurrentItem(item);
			return m_mapNPC[npc].OnTakeItem(*pPC);
		}
		else
		{
			sys_err("QUEST USE_ITEM_EVENT no such pc id : %d", pc);
			return false;
		}
	}

	bool CQuestManager::UseItem(unsigned int pc, CItem * item, bool bReceiveAll)
	{
		if (test_server)
			sys_log( 0, "questmanager::UseItem Start : itemVnum : %d PC : %d", item->GetOriginalVnum(), pc);
		PC* pPC;
		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
			{
				CHARACTER * ch = CHARACTER_MANAGER::instance().FindByPID(pc);
				if (ch)
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("퀘스트를 로드하는 중입니다. 잠시만 기다려 주십시오."));
				}
				return false;
			}
			// call script
			SetCurrentItem(item);

			return m_mapNPC[item->GetVnum()].OnUseItem(*pPC, bReceiveAll);
		}
		else
		{
			sys_err("QUEST USE_ITEM_EVENT no such pc id : %d", pc);
			return false;
		}
	}

	// Group Use defined in Special Item Group
	bool CQuestManager::SIGUse(unsigned int pc, uint32_t sig_vnum, CItem * item, bool bReceiveAll)
	{
		if (test_server)
			sys_log( 0, "questmanager::SIGUse Start : itemVnum : %d PC : %d", item->GetOriginalVnum(), pc);
		PC* pPC;
		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
			{
				CHARACTER * ch = CHARACTER_MANAGER::instance().FindByPID(pc);
				if (ch)
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("퀘스트를 로드하는 중입니다. 잠시만 기다려 주십시오."));
				}
				return false;
			}
			// call script
			SetCurrentItem(item);

			return m_mapNPC[sig_vnum].OnSIGUse(*pPC, bReceiveAll);
		}
		else
		{
			sys_err("QUEST USE_ITEM_EVENT no such pc id : %d", pc);
			return false;
		}
	}

	bool CQuestManager::GiveItemToPC(unsigned int pc, CHARACTER * pkChr)
	{
		if (!pkChr->IsPC())
			return false;

		PC * pPC = GetPC(pc);

		if (pPC)
		{
			if (!CheckQuestLoaded(pPC))
				return false;

			TargetInfo * pInfo = CTargetManager::instance().GetTargetInfo(pc, TARGET_TYPE_VID, pkChr->GetVID());

			if (pInfo)
			{
				bool bRet;

				if (m_mapNPC[QUEST_NO_NPC].OnTarget(*pPC, pInfo->dwQuestIndex, pInfo->szTargetName, "click", bRet))
					return true;
			}
		}

		return false;
	}

	bool CQuestManager::Click(unsigned int pc, CHARACTER * pkChrTarget)
	{
		PC * pPC = GetPC(pc);

		if (pPC)
		{
			if (!CheckQuestLoaded(pPC))
			{
				CHARACTER * ch = CHARACTER_MANAGER::instance().FindByPID(pc);

				if (ch)
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("퀘스트를 로드하는 중입니다. 잠시만 기다려 주십시오."));

				return false;
			}

			TargetInfo * pInfo = CTargetManager::instance().GetTargetInfo(pc, TARGET_TYPE_VID, pkChrTarget->GetVID());
			if (test_server)
			{
				sys_log(0, "CQuestManager::Click(pid=%d, npc_name=%s) - target_info(%p)", pc, pkChrTarget->GetName(), static_cast<void*>(pInfo));
			}

			if (pInfo)
			{
				bool bRet;
				if (m_mapNPC[QUEST_NO_NPC].OnTarget(*pPC, pInfo->dwQuestIndex, pInfo->szTargetName, "click", bRet))
					return bRet;
			}

			uint32_t dwCurrentNPCRace = pkChrTarget->GetRaceNum();

			if (pkChrTarget->IsNPC())
			{
				map<unsigned int, NPC>::iterator it = m_mapNPC.find(dwCurrentNPCRace);

				if (it == m_mapNPC.end())
				{
					sys_err("CQuestManager::Click(pid=%d, target_npc_name=%s) - NOT EXIST NPC RACE VNUM[%d]",
							pc, 
							pkChrTarget->GetName(), 
							dwCurrentNPCRace);
					return false;
				}

				// call script
				if (it->second.HasChat())
				{
					// if have chat, give chat
					if (test_server)
						sys_log(0, "CQuestManager::Click->OnChat");

					if (!it->second.OnChat(*pPC))
					{
						if (test_server)
							sys_log(0, "CQuestManager::Click->OnChat Failed");

						return it->second.OnClick(*pPC);
					}

					return true;
				}
				else
				{
					// else click
					return it->second.OnClick(*pPC);
				}
			}
			return false;
		}
		else
		{
			sys_err("QUEST CLICK_EVENT no such pc id : %d", pc);
			return false;
		}
	}

	void CQuestManager::Unmount(unsigned int pc)
	{
		PC * pPC;

		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
				return;

			m_mapNPC[QUEST_NO_NPC].OnUnmount(*pPC);
		}
		else
			sys_err("QUEST no such pc id : %d", pc);
	}
	//German gift feature test
	void CQuestManager::ItemInformer(unsigned int pc,unsigned int vnum)
	{
		
		PC* pPC;
		pPC = GetPC(pc);
		
		m_mapNPC[QUEST_NO_NPC].OnItemInformer(*pPC,vnum);
	}
	///////////////////////////////////////////////////////////////////////////////////////////
	// END OF quest event handling
	///////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////
	void CQuestManager::LoadStartQuest(const string& quest_name, unsigned int idx)
	{
		for (auto it = g_setQuestObjectDir.begin(); it != g_setQuestObjectDir.end(); ++it)
		{
			const string& stQuestObjectDir = *it;
			string full_name = stQuestObjectDir + "/begin_condition/" + quest_name;
			ifstream inf(full_name.c_str());

			if (inf.is_open())
			{
				sys_log(0, "QUEST loading begin condition for %s", quest_name.c_str());

				istreambuf_iterator<char> ib(inf), ie;
				copy(ib, ie, back_inserter(m_hmQuestStartScript[idx]));
			}
		}
	}

	bool CQuestManager::CanStartQuest(unsigned int quest_index, const PC& pc)
	{
		return CanStartQuest(quest_index);
	}

	bool CQuestManager::CanStartQuest(unsigned int quest_index)
	{
		THashMapQuestStartScript::iterator it;

		if ((it = m_hmQuestStartScript.find(quest_index)) == m_hmQuestStartScript.end())
			return true;
		else
		{
			int x = lua_gettop(L);
			lua_dobuffer(L, &(it->second[0]), it->second.size(), "StartScript");
			int bStart = lua_toboolean(L, -1);
			lua_settop(L, x);
			return bStart != 0;
		}
	}

	void CQuestManager::DisconnectPC(CHARACTER * ch)
	{
		m_mapPC.erase(ch->GetPlayerID());
	}

	PC * CQuestManager::GetPCForce(unsigned int pc)
	{
		PCMap::iterator it;

		if ((it = m_mapPC.find(pc)) == m_mapPC.end())
		{
			PC * pPC = &m_mapPC[pc];
			pPC->SetID(pc);
			return pPC;
		}

		return &it->second;
	}

	PC * CQuestManager::GetPC(unsigned int pc)
	{
		PCMap::iterator it;

		CHARACTER * pkChr = CHARACTER_MANAGER::instance().FindByPID(pc);

		if (!pkChr)
			return nullptr;

		m_pCurrentPC = GetPCForce(pc);
		m_pCurrentCharacter = pkChr;
		m_pSelectedDungeon = nullptr;
		return (m_pCurrentPC);
	}

	void CQuestManager::ClearScript()
	{
		m_strScript.clear();
		m_iCurrentSkin = QUEST_SKIN_NORMAL;
	}

	void CQuestManager::AddScript(const string& str)
	{
		m_strScript+=str;
	}

	void CQuestManager::SendScript()
	{
		if (m_bNoSend)
		{
			m_bNoSend = false;
			ClearScript();
			return;
		}

		if (m_strScript=="[DONE]" || m_strScript == "[NEXT]")
		{
			if (m_pCurrentPC && !m_pCurrentPC->GetAndResetDoneFlag() && m_strScript=="[DONE]" && m_iCurrentSkin == QUEST_SKIN_NORMAL && !IsError())
			{
				ClearScript();
				return;
			}
			m_iCurrentSkin = QUEST_SKIN_NOWINDOW;
		}

		//send -_-!
		struct ::packet_script packet_script;

		packet_script.header = HEADER_GC_SCRIPT;
		packet_script.skin = m_iCurrentSkin;
		packet_script.src_size = m_strScript.size();
		packet_script.size = packet_script.src_size + sizeof(struct packet_script);

		TEMP_BUFFER buf;
		buf.write(&packet_script, sizeof(struct packet_script));
		buf.write(&m_strScript[0], m_strScript.size());

		GetCurrentCharacterPtr()->GetDesc()->Packet(buf.read_peek(), buf.size());

		if (test_server)
			sys_log(0, "m_strScript %s size %d", m_strScript.c_str(), buf.size());

		ClearScript();
	}

	const char* CQuestManager::GetQuestStateName(const string& quest_name, const int state_index)
	{
		int x = lua_gettop(L);
		lua_getglobal(L, quest_name.c_str());
		if (lua_isnil(L,-1))
		{
			sys_err("QUEST wrong quest state file %s.%d", quest_name.c_str(), state_index);
			lua_settop(L,x);
			return "";
		}
		lua_pushnumber(L, state_index);
		lua_gettable(L, -2);

		const char* str = lua_tostring(L, -1);
		lua_settop(L, x);
		return str;
	}

	int CQuestManager::GetQuestStateIndex(const string& quest_name, const string& state_name)
	{
		int x = lua_gettop(L);
		lua_getglobal(L, quest_name.c_str());
		if (lua_isnil(L,-1))
		{
			sys_err("QUEST wrong quest state file %s.%s",quest_name.c_str(),state_name.c_str()  );
			lua_settop(L,x);
			return 0;
		}
		lua_pushstring(L, state_name.c_str());
		lua_gettable(L, -2);

		int v = (int)rint(lua_tonumber(L,-1));
		lua_settop(L, x);
		if ( test_server )
			sys_log( 0,"[QUESTMANAGER] GetQuestStateIndex x(%d) v(%d) %s %s", v,x, quest_name.c_str(), state_name.c_str() );
		return v;
	}

	void CQuestManager::SetSkinStyle(int iStyle)
	{
		if (iStyle<0 || iStyle >= QUEST_SKIN_COUNT)
		{
			m_iCurrentSkin = QUEST_SKIN_NORMAL;
		}
		else
			m_iCurrentSkin = iStyle;
	}

	unsigned int CQuestManager::LoadTimerScript(const string& name)
	{
		map<string, unsigned int>::iterator it;
		if ((it = m_mapTimerID.find(name)) != m_mapTimerID.end())
		{
			return it->second;
		}
		else
		{
			unsigned int new_id = UINT_MAX - m_mapTimerID.size();

			m_mapNPC[new_id].Set(new_id, name);
			m_mapTimerID.insert(make_pair(name, new_id));

			return new_id;
		}
	}

	unsigned int CQuestManager::GetCurrentNPCRace()
	{
		return GetCurrentNPCCharacterPtr() ? GetCurrentNPCCharacterPtr()->GetRaceNum() : 0;
	}

	CItem * CQuestManager::GetCurrentItem()
	{
		return GetCurrentCharacterPtr() ? GetCurrentCharacterPtr()->GetQuestItemPtr() : nullptr; 
	}

	void CQuestManager::ClearCurrentItem()
	{
		if (GetCurrentCharacterPtr())
			GetCurrentCharacterPtr()->ClearQuestItemPtr();
	}

	void CQuestManager::SetCurrentItem(CItem * item)
	{
		if (GetCurrentCharacterPtr())
			GetCurrentCharacterPtr()->SetQuestItemPtr(item);
	}

	CHARACTER * CQuestManager::GetCurrentNPCCharacterPtr()
	{ 
		return GetCurrentCharacterPtr() ? GetCurrentCharacterPtr()->GetQuestNPC() : nullptr; 
	}

	const string & CQuestManager::GetCurrentQuestName()
	{
		return GetCurrentPC()->GetCurrentQuestName();
	}

	CDungeon * CQuestManager::GetCurrentDungeon()
	{
		CHARACTER * ch = GetCurrentCharacterPtr();

		if (!ch)
		{
			if (m_pSelectedDungeon)
				return m_pSelectedDungeon;
			return nullptr;
		}

		return ch->GetDungeonForce();
	}

	void CQuestManager::RegisterQuest(const string & stQuestName, unsigned int idx)
	{
		assert(idx > 0);

		decltype(m_hmQuestName.begin()) it;

		if ((it = m_hmQuestName.find(stQuestName)) != m_hmQuestName.end())
			return;

		m_hmQuestName.insert(make_pair(stQuestName, idx));
		LoadStartQuest(stQuestName, idx);
		m_mapQuestNameByIndex.insert(make_pair(idx, stQuestName));

		sys_log(0, "QUEST: Register %4u %s", idx, stQuestName.c_str());
	}

	unsigned int CQuestManager::GetQuestIndexByName(const string& name)
	{
		THashMapQuestName::iterator it = m_hmQuestName.find(name);

		if (it == m_hmQuestName.end())
			return 0; // RESERVED

		return it->second;
	}

	const string & CQuestManager::GetQuestNameByIndex(unsigned int idx)
	{
		decltype(m_mapQuestNameByIndex.begin()) it;

		if ((it = m_mapQuestNameByIndex.find(idx)) == m_mapQuestNameByIndex.end())
		{
			sys_err("cannot find quest name by index %u", idx);
			assert(!"cannot find quest name by index");

			static std::string st = "";
			return st;
		}

		return it->second;
	}

	void CQuestManager::SendEventFlagList(CHARACTER * ch)
	{
		decltype(m_mapEventFlag.begin()) it;
		for (it = m_mapEventFlag.begin(); it != m_mapEventFlag.end(); ++it)
		{
			const string& flagname = it->first;
			int value = it->second;

			if (!test_server && value == 1 && flagname == "valentine_drop")
				ch->ChatPacket(CHAT_TYPE_INFO, "%s %d prob 800", flagname.c_str(), value);
			else if (!test_server && value == 1 && flagname == "newyear_wonso")
				ch->ChatPacket(CHAT_TYPE_INFO, "%s %d prob 500", flagname.c_str(), value);
			else if (!test_server && value == 1 && flagname == "newyear_fire")
				ch->ChatPacket(CHAT_TYPE_INFO, "%s %d prob 1000", flagname.c_str(), value);
			else
				ch->ChatPacket(CHAT_TYPE_INFO, "%s %d", flagname.c_str(), value);
		}
	}

	void CQuestManager::RequestSetEventFlag(const string& name, int value)
	{
		TPacketSetEventFlag p;
		strlcpy(p.szFlagName, name.c_str(), sizeof(p.szFlagName));
		p.lValue = value;
		db_clientdesc->DBPacket(HEADER_GD_SET_EVENT_FLAG, 0, &p, sizeof(TPacketSetEventFlag));
	}

	void CQuestManager::SetEventFlag(const string& name, int value)
	{
		static const char*	DROPEVENT_CHARTONE_NAME		= "drop_char_stone";
		static const int	DROPEVENT_CHARTONE_NAME_LEN = strlen(DROPEVENT_CHARTONE_NAME);

		int prev_value = m_mapEventFlag[name];

		sys_log(0, "QUEST eventflag %s %d prev_value %d", name.c_str(), value, m_mapEventFlag[name]);
		m_mapEventFlag[name] = value;

		if (name == "mob_item")
		{
			CHARACTER_MANAGER::instance().SetMobItemRate(value);
		}
		else if (name == "mob_dam")
		{
			CHARACTER_MANAGER::instance().SetMobDamageRate(value);
		}
		else if (name == "mob_gold")
		{
			CHARACTER_MANAGER::instance().SetMobGoldAmountRate(value);
		}
		else if (name == "mob_gold_pct")
		{
			CHARACTER_MANAGER::instance().SetMobGoldDropRate(value);
		}
		else if (name == "mob_exp")
		{
			CHARACTER_MANAGER::instance().SetMobExpRate(value);
		}
		else if (name == "mob_item_buyer")
		{
			CHARACTER_MANAGER::instance().SetMobItemRatePremium(value);
		}
		else if (name == "mob_exp_buyer")
		{
			CHARACTER_MANAGER::instance().SetMobExpRatePremium(value);
		}
		else if (name == "mob_gold_buyer")
		{
			CHARACTER_MANAGER::instance().SetMobGoldAmountRatePremium(value);
		}
		else if (name == "mob_gold_pct_buyer")
		{
			CHARACTER_MANAGER::instance().SetMobGoldDropRatePremium(value);
		}
		else if (!name.compare(0,5,"xmas_"))
		{
			xmas::ProcessEventFlag(name, prev_value, value);
		}
		else if (name == "newyear_boom")
		{
			const DESC_MANAGER::DESC_SET & c_ref_set = DESC_MANAGER::instance().GetClientSet();

			for (auto it = c_ref_set.begin(); it != c_ref_set.end(); ++it)
			{
				CHARACTER * ch = (*it)->GetCharacter();

				if (!ch)
					continue;

				ch->ChatPacket(CHAT_TYPE_COMMAND, "newyear_boom %d", value);
			}
		}
		else if ( name == "eclipse" )
		{
			std::string mode("");

			if ( value == 1 )
			{
				mode = "dark";
			}
			else
			{
				mode = "light";
			}
			
			const DESC_MANAGER::DESC_SET & c_ref_set = DESC_MANAGER::instance().GetClientSet();

			for (auto it = c_ref_set.begin(); it != c_ref_set.end(); ++it)
			{
				CHARACTER * ch = (*it)->GetCharacter();
				if (!ch)
					continue;

				ch->ChatPacket(CHAT_TYPE_COMMAND, "DayMode %s", mode.c_str());
			}
		}
		else if (name == "day")
		{
			const DESC_MANAGER::DESC_SET & c_ref_set = DESC_MANAGER::instance().GetClientSet();

			for (auto it = c_ref_set.begin(); it != c_ref_set.end(); ++it)
			{
				CHARACTER * ch = (*it)->GetCharacter();
				if (!ch)
					continue;
				if (value)
				{
					// Night
					ch->ChatPacket(CHAT_TYPE_COMMAND, "DayMode dark");
				}
				else
				{
					// Day
					ch->ChatPacket(CHAT_TYPE_COMMAND, "DayMode light");
				}
			}

			if (value && !prev_value)
			{
				// Create if missing
				struct SNPCSellFireworkPosition
				{
					int32_t lMapIndex;
					int x;
					int y;
				} positions[] = {
					{	1,	615,	618 },
					{	3,	500,	625 },
					{	21,	598,	665 },
					{	23,	476,	360 },
					{	41,	318,	629 },
					{	43,	478,	375 },
					{	0,	0,	0   },
				};

				SNPCSellFireworkPosition* p = positions;
				while (p->lMapIndex)
				{
					if (map_allow_find(p->lMapIndex))
					{
						PIXEL_POSITION posBase;
						if (!SECTREE_MANAGER::instance().GetMapBasePositionByMapIndex(p->lMapIndex, posBase))
						{
							sys_err("cannot get map base position %d", p->lMapIndex);
							++p;
							continue;
						}

						CHARACTER_MANAGER::instance().SpawnMob(xmas::MOB_XMAS_FIRWORK_SELLER_VNUM, p->lMapIndex, posBase.x + p->x * 100, posBase.y + p->y * 100, 0, false, -1);
					}
					p++;
				}
			}
			else if (!value && prev_value)
			{
				// Delete if present
				CharacterVectorInteractor i;

				if (CHARACTER_MANAGER::instance().GetCharactersByRaceNum(xmas::MOB_XMAS_FIRWORK_SELLER_VNUM, i))
				{
					CharacterVectorInteractor::iterator it = i.begin();

					while (it != i.end()) {
						CHARACTER_MANAGER::instance().DestroyCharacter(*it++);
					}
				}
			}
		}
		else if (name == "pre_event_hc")
		{
			const uint32_t EventNPC = 20090;

			struct SEventNPCPosition
			{
				int32_t lMapIndex;
				int x;
				int y;
			} positions[] = {
				{ 3, 588, 617 },
				{ 23, 397, 250 },
				{ 43, 567, 426 },
				{ 0, 0, 0 },
			};

			if (value && !prev_value)
			{
				SEventNPCPosition* pPosition = positions;

				while (pPosition->lMapIndex)
				{
					if (map_allow_find(pPosition->lMapIndex))
					{
						PIXEL_POSITION pos;

						if (!SECTREE_MANAGER::instance().GetMapBasePositionByMapIndex(pPosition->lMapIndex, pos))
						{
							sys_err("cannot get map base position %d", pPosition->lMapIndex);
							++pPosition;
							continue;
						}

						CHARACTER_MANAGER::instance().SpawnMob(EventNPC, pPosition->lMapIndex, pos.x+pPosition->x*100, pos.y+pPosition->y*100, 0, false, -1);
					}
					pPosition++;
				}
			}
			else if (!value && prev_value)
			{
				CharacterVectorInteractor i;

				if (CHARACTER_MANAGER::instance().GetCharactersByRaceNum(EventNPC, i))
				{
					CharacterVectorInteractor::iterator it = i.begin();

					while (it != i.end())
					{
						CHARACTER * ch = *it++;

						switch (ch->GetMapIndex())
						{
							case 3:
							case 23:
							case 43:
								CHARACTER_MANAGER::instance().DestroyCharacter(ch);
								break;
						}
					}
				}
			}
		}
		else if (name.compare(0, DROPEVENT_CHARTONE_NAME_LEN, DROPEVENT_CHARTONE_NAME)== 0)
		{
			DropEvent_CharStone_SetValue(name, value);
		}
		else if (name.compare(0, strlen("refine_box"), "refine_box")== 0)
		{
			DropEvent_RefineBox_SetValue(name, value);
		}
		else if (name == "gold_drop_limit_time")
		{
			g_GoldDropTimeLimitValue = value * 1000;
		}
		else if (name == "new_xmas_event")
		{
			// 20126 new Santa.
			static uint32_t new_santa = 20126;
			if (value != 0)
			{
				CharacterVectorInteractor i;
				bool map1_santa_exist = false;
				bool map21_santa_exist = false;
				bool map41_santa_exist = false;
				
				if (CHARACTER_MANAGER::instance().GetCharactersByRaceNum(new_santa, i))
				{
					CharacterVectorInteractor::iterator it = i.begin();

					while (it != i.end())
					{
						CHARACTER * tch = *(it++);

						if (tch->GetMapIndex() == 1)
						{
							map1_santa_exist = true;
						}
						else if (tch->GetMapIndex() == 21)
						{
							map21_santa_exist = true;
						}
						else if (tch->GetMapIndex() == 41)
						{
							map41_santa_exist = true;
						}
					}
				}

				if (map_allow_find(1) && !map1_santa_exist)
				{
					SECTREE_MAP * pkSectreeMap = SECTREE_MANAGER::instance().GetMap(1);
					CHARACTER_MANAGER::instance().SpawnMob(new_santa, 1, pkSectreeMap->m_setting.iBaseX + 60800, pkSectreeMap->m_setting.iBaseY + 61700, 0, false, 90, true);
				}
				if (map_allow_find(21) && !map21_santa_exist)
				{
					SECTREE_MAP * pkSectreeMap = SECTREE_MANAGER::instance().GetMap(21);
					CHARACTER_MANAGER::instance().SpawnMob(new_santa, 21, pkSectreeMap->m_setting.iBaseX + 59600, pkSectreeMap->m_setting.iBaseY + 61000, 0, false, 110, true);
				}
				if (map_allow_find(41) && !map41_santa_exist)
				{
					SECTREE_MAP * pkSectreeMap = SECTREE_MANAGER::instance().GetMap(41);
					CHARACTER_MANAGER::instance().SpawnMob(new_santa, 41, pkSectreeMap->m_setting.iBaseX + 35700, pkSectreeMap->m_setting.iBaseY + 74300, 0, false, 140, true);
				}
			}
			else
			{
				CharacterVectorInteractor i;
				CHARACTER_MANAGER::instance().GetCharactersByRaceNum(new_santa, i);
				
				for (CharacterVectorInteractor::iterator it = i.begin(); it != i.end(); it++)
				{
					CHARACTER_MANAGER::instance().DestroyCharacter(*it);
				}
			}
		}
	}

	int	CQuestManager::GetEventFlag(const string& name)
	{
		map<string,int>::iterator it = m_mapEventFlag.find(name);

		if (it == m_mapEventFlag.end())
			return 0;

		return it->second;
	}

	void CQuestManager::BroadcastEventFlagOnLogin(CHARACTER * ch)
	{
		int iEventFlagValue;

		if ((iEventFlagValue = quest::CQuestManager::instance().GetEventFlag("xmas_snow")))
		{
			ch->ChatPacket(CHAT_TYPE_COMMAND, "xmas_snow %d", iEventFlagValue);
		}

		if ((iEventFlagValue = quest::CQuestManager::instance().GetEventFlag("xmas_boom")))
		{
			ch->ChatPacket(CHAT_TYPE_COMMAND, "xmas_boom %d", iEventFlagValue);
		}

		if ((iEventFlagValue = quest::CQuestManager::instance().GetEventFlag("xmas_tree")))
		{
			ch->ChatPacket(CHAT_TYPE_COMMAND, "xmas_tree %d", iEventFlagValue);
		}

		if ((iEventFlagValue = quest::CQuestManager::instance().GetEventFlag("day")))
		{
			ch->ChatPacket(CHAT_TYPE_COMMAND, "DayMode dark");
		}

		if ((iEventFlagValue = quest::CQuestManager::instance().GetEventFlag("newyear_boom")))
		{
			ch->ChatPacket(CHAT_TYPE_COMMAND, "newyear_boom %d", iEventFlagValue);
		}

		if ( (iEventFlagValue = quest::CQuestManager::instance().GetEventFlag("eclipse")) )
		{
			std::string mode;

			if ( iEventFlagValue == 1 ) mode = "dark";
			else mode = "light";

			ch->ChatPacket(CHAT_TYPE_COMMAND, "DayMode %s", mode.c_str());
		}
	}

	void CQuestManager::Reload()
	{
		lua_close(L);
		m_mapNPC.clear();
		m_mapNPCNameID.clear();
		m_hmQuestName.clear();
		m_mapTimerID.clear();
		m_hmQuestStartScript.clear();
		m_mapEventName.clear();
		L = nullptr;
		Initialize();

		for (auto it = m_registeredNPCVnum.begin(); it != m_registeredNPCVnum.end(); ++it)
		{
			char buf[256];
			uint32_t dwVnum = *it;
			snprintf(buf, sizeof(buf), "%u", dwVnum);
			m_mapNPC[dwVnum].Set(dwVnum, buf);
		}
	}

	bool CQuestManager::ExecuteQuestScript(PC& pc, uint32_t quest_index, const int state, const char* code, const int code_size, vector<AArgScript*>* pChatScripts, bool bUseCache)
	{
		return ExecuteQuestScript(pc, CQuestManager::instance().GetQuestNameByIndex(quest_index), state, code, code_size, pChatScripts, bUseCache);
	}

	bool CQuestManager::ExecuteQuestScript(PC& pc, const string& quest_name, const int state, const char* code, const int code_size, vector<AArgScript*>* pChatScripts, bool bUseCache)
	{
		// Create the execution space
		QuestState qs = CQuestManager::instance().OpenState(quest_name, state);
		if (pChatScripts)
			qs.chat_scripts.swap(*pChatScripts);

		// Load the code
		if (bUseCache)
		{
			lua_getglobal(qs.co, "__codecache");
			// stack : __codecache
			lua_pushlightuserdata(qs.co, const_cast<char*>(code));
			// stack : __codecache (codeptr)
			lua_rawget(qs.co, -2);
			// stack : __codecache (compiled-code)
			if (lua_isnil(qs.co, -1))
			{
				// cache miss

				// load code to lua,
				// save it to cache
				// and only function remain in stack
				lua_pop(qs.co, 1);
				// stack : __codecache
				luaL_loadbuffer(qs.co, code, code_size, quest_name.c_str());
				// stack : __codecache (compiled-code)
				lua_pushlightuserdata(qs.co, const_cast<char*>(code));
				// stack : __codecache (compiled-code) (codeptr)
				lua_pushvalue(qs.co, -2);
				// stack : __codecache (compiled-code) (codeptr) (compiled_code)
				lua_rawset(qs.co, -4);
				// stack : __codecache (compiled-code)
				lua_remove(qs.co, -2);
				// stack : (compiled-code)
			}
			else
			{
				// cache hit
				lua_remove(qs.co, -2);
				// stack : (compiled-code)
			}
		}
		else
			luaL_loadbuffer(qs.co, code, code_size, quest_name.c_str());

		// Attach to the player
		pc.SetQuest(quest_name, qs);

		// Execute
		QuestState& rqs = *pc.GetRunningQuestState();
		if (!CQuestManager::instance().RunState(rqs))
		{
			CQuestManager::instance().CloseState(rqs);
			pc.EndRunning();
			return false;
		}
		return true;
	}

	void CQuestManager::RegisterNPCVnum(uint32_t dwVnum)
	{
		if (m_registeredNPCVnum.find(dwVnum) != m_registeredNPCVnum.end())
			return;

		m_registeredNPCVnum.insert(dwVnum);

		char buf[256];
		DIR* dir;

		for (auto it = g_setQuestObjectDir.begin(); it != g_setQuestObjectDir.end(); ++it)
		{
			const string& stQuestObjectDir = *it;
			snprintf(buf, sizeof(buf), "%s/%u", stQuestObjectDir.c_str(), dwVnum);
			sys_log(0, "%s", buf);

			if ((dir = opendir(buf)))
			{
				closedir(dir);
				snprintf(buf, sizeof(buf), "%u", dwVnum);
				sys_log(0, "%s", buf);

				m_mapNPC[dwVnum].Set(dwVnum, buf);
			}
		}
	}

	void CQuestManager::WriteRunningStateToSyserr()
	{
		const char * state_name = GetQuestStateName(GetCurrentQuestName(), GetCurrentState()->st);

		string event_index_name = "";
		for (auto it = m_mapEventName.begin(); it != m_mapEventName.end(); ++it)
		{
			if (it->second == m_iRunningEventIndex)
			{
				event_index_name = it->first;
				break;
			}
		}

		sys_err("LUA_ERROR: quest %s.%s %s", GetCurrentQuestName().c_str(), state_name, event_index_name.c_str() );
		if (GetCurrentCharacterPtr() && test_server)
			GetCurrentCharacterPtr()->ChatPacket(CHAT_TYPE_PARTY, "LUA_ERROR: quest %s.%s %s", GetCurrentQuestName().c_str(), state_name, event_index_name.c_str() );
	}

	void CQuestManager::QuestError(const char* func, int line, const char* fmt, ...)
	{
		char szMsg[4096];
		va_list args;

		va_start(args, fmt);
		vsnprintf(szMsg, sizeof(szMsg), fmt, args);
		va_end(args);

		_sys_err(func, line, "%s", szMsg);
		if (test_server)
		{
			CHARACTER * ch = GetCurrentCharacterPtr();
			if (ch)
			{
				ch->ChatPacket(CHAT_TYPE_PARTY, "error occurred on [%s:%d]", func,line);
				ch->ChatPacket(CHAT_TYPE_PARTY, "%s", szMsg);
			}
		}
	}

	void CQuestManager::AddServerTimer(const std::string& name, uint32_t arg, LPEVENT event)
	{
		sys_log(0, "XXX AddServerTimer %s %d %p", name.c_str(), arg, event.get());
		if (m_mapServerTimer.find(make_pair(name, arg)) != m_mapServerTimer.end())
		{
			sys_err("already registered server timer name:%s arg:%u", name.c_str(), arg);
			return;
		}
		m_mapServerTimer.insert(make_pair(make_pair(name, arg), event));
	}

	void CQuestManager::ClearServerTimerNotCancel(const std::string& name, uint32_t arg)
	{
		m_mapServerTimer.erase(make_pair(name, arg));
	}

	void CQuestManager::ClearServerTimer(const std::string& name, uint32_t arg)
	{
		auto it = m_mapServerTimer.find(make_pair(name, arg));
		if (it != m_mapServerTimer.end())
		{
			LPEVENT event = it->second;
			event_cancel(&event);
			m_mapServerTimer.erase(it);
		}
	}

	// Old
	
	// Fix (New)
	void CQuestManager::CancelServerTimers(uint32_t arg)
	{
		vector<pair<string, uint32_t>> ServerTimersToDelete;

		for (const auto& kv : m_mapServerTimer) {
			if (kv.first.second == arg) {
				LPEVENT event = kv.second;
				event_cancel(&event);
				ServerTimersToDelete.push_back(kv.first);
			}
		}

		// Delete all the required server timers
		for (const auto &timer : ServerTimersToDelete)
			m_mapServerTimer.erase(timer);

		// Clean up
		ServerTimersToDelete.clear();
	}

	void CQuestManager::SetServerTimerArg(uint32_t dwArg)
	{
		m_dwServerTimerArg = dwArg;
	}

	uint32_t CQuestManager::GetServerTimerArg()
	{
		return m_dwServerTimerArg;
	}

	void CQuestManager::SelectDungeon(CDungeon * pDungeon)
	{
		m_pSelectedDungeon = pDungeon;
	}
	
	bool CQuestManager::PickupItem(unsigned int pc, CItem * item)
	{
		if (test_server)
			sys_log( 0, "questmanager::PickupItem Start : itemVnum : %d PC : %d", item->GetOriginalVnum(), pc);
		PC* pPC;
		if ((pPC = GetPC(pc)))
		{
			if (!CheckQuestLoaded(pPC))
			{
				CHARACTER * ch = CHARACTER_MANAGER::instance().FindByPID(pc);
				if (ch)
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("퀘스트를 로드하는 중입니다. 잠시만 기다려 주십시오."));
				}
				return false;
			}
			// call script
			SetCurrentItem(item);

			return m_mapNPC[item->GetVnum()].OnPickupItem(*pPC);
		}
		else
		{
			sys_err("QUEST PICK_ITEM_EVENT no such pc id : %d", pc);
			return false;
		}
	}
	void CQuestManager::BeginOtherPCBlock(uint32_t pid)
	{
		CHARACTER * ch = GetCurrentCharacterPtr();
		if (nullptr == ch)
		{
			sys_err("NULL?");
			return;
		}
		/*
		# 1. current pid = pid0 <- It will be m_pOtherPCBlockRootPC.
		begin_other_pc_block(pid1)
			# 2. current pid = pid1
			begin_other_pc_block(pid2)
				# 3. current_pid = pid2
			end_other_pc_block()
		end_other_pc_block()
		*/
		// when begin_other_pc_block(pid1)
		if (m_vecPCStack.empty())
		{
			m_pOtherPCBlockRootPC = GetCurrentPC();
		}
		m_vecPCStack.push_back(GetCurrentCharacterPtr()->GetPlayerID());
		GetPC(pid);
	}

	void CQuestManager::EndOtherPCBlock()
	{
		if (m_vecPCStack.size() == 0)
		{
			sys_err("m_vecPCStack is alread empty. CurrentQuest{Name(%s), State(%s)}", GetCurrentQuestName().c_str(), GetCurrentState()->_title.c_str());
			return;
		}
		uint32_t pc = m_vecPCStack.back();
		m_vecPCStack.pop_back();
		GetPC(pc);

		if (m_vecPCStack.empty())
		{
			m_pOtherPCBlockRootPC = nullptr;
		}
	}

	bool CQuestManager::IsInOtherPCBlock()
	{
		return !m_vecPCStack.empty();
	}

	PC*	CQuestManager::GetOtherPCBlockRootPC()
	{
		return m_pOtherPCBlockRootPC;
	}
}

