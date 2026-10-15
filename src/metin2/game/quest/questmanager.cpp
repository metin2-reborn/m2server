#include <metin2/game/quest/questmanager.h>

#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/world/dungeon.h>
#include <metin2/game/app/event.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/quest/quest.h>
#include <metin2/game/quest/questnpc.h>
#include <metin2/game/quest/questpc.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/quest/target.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/events/xmas_event.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/common/utils.h>
#include <metin2/log/log.h>

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

namespace metin2::game {


namespace quest
{
	quest_manager::quest_manager()
		: selected_dungeon_(nullptr), server_timer_arg_(0), running_event_index_(0), l_(nullptr), no_send_ (false),
		current_running_state_(nullptr), current_character_(nullptr), current_party_member_(nullptr),
		current_pc_(nullptr),  current_skin_(0), error_(false), other_pc_block_root_pc_(nullptr)
	{
	}

	quest_manager::~quest_manager()
	{
		destroy();
	}

	void quest_manager::destroy()
	{
		if (l_)
		{
			lua_close(l_);
			l_ = nullptr;
		}
	}	

	bool quest_manager::initialize()
	{
		if (metin2::config::is_auth())
			return true;

		if (!initialize_lua())
			return false;

		selected_dungeon_ = nullptr;

		event_name.insert(event_name_map::value_type("click", QUEST_CLICK_EVENT));		// Clicked an NPC
		event_name.insert(event_name_map::value_type("kill", QUEST_KILL_EVENT));		// Hunted a mob
		event_name.insert(event_name_map::value_type("timer", QUEST_TIMER_EVENT));		// Preset time has passed
		event_name.insert(event_name_map::value_type("levelup", QUEST_LEVELUP_EVENT));	// Leveled up
		event_name.insert(event_name_map::value_type("login", QUEST_LOGIN_EVENT));		// On login
		event_name.insert(event_name_map::value_type("logout", QUEST_LOGOUT_EVENT));		// On logout
		event_name.insert(event_name_map::value_type("button", QUEST_BUTTON_EVENT));		// Pressed the quest button
		event_name.insert(event_name_map::value_type("info", QUEST_INFO_EVENT));		// Opened the quest info window
		event_name.insert(event_name_map::value_type("chat", QUEST_CHAT_EVENT));		// Talked with a specific keyword
		event_name.insert(event_name_map::value_type("in", QUEST_ATTR_IN_EVENT));		// Entered a specific map attribute
		event_name.insert(event_name_map::value_type("out", QUEST_ATTR_OUT_EVENT));		// Left a specific map attribute
		event_name.insert(event_name_map::value_type("use", QUEST_ITEM_USE_EVENT));		// Used a quest item
		event_name.insert(event_name_map::value_type("server_timer", QUEST_SERVER_TIMER_EVENT));	// Server timer (not tested yet)
		event_name.insert(event_name_map::value_type("enter", QUEST_ENTER_STATE_EVENT));	// Became the current state
		event_name.insert(event_name_map::value_type("leave", QUEST_LEAVE_STATE_EVENT));	// Changed from the current state to another state
		event_name.insert(event_name_map::value_type("letter", QUEST_LETTER_EVENT));		// Info must be set anew after login or a state change
		event_name.insert(event_name_map::value_type("take", QUEST_ITEM_TAKE_EVENT));	// Received an item
		event_name.insert(event_name_map::value_type("target", QUEST_TARGET_EVENT));		// Target
		event_name.insert(event_name_map::value_type("party_kill", QUEST_PARTY_KILL_EVENT));	// Party member hunted a monster (sent to leader)
		event_name.insert(event_name_map::value_type("unmount", QUEST_UNMOUNT_EVENT));
		event_name.insert(event_name_map::value_type("pick", QUEST_ITEM_PICK_EVENT));	// Picked up a dropped item.
		event_name.insert(event_name_map::value_type("sig_use", QUEST_SIG_USE_EVENT));		// Used an item belonging to a Special item group.
		event_name.insert(event_name_map::value_type("item_informer", QUEST_ITEM_INFORMER_EVENT));	// German gift feature test

		no_send_ = false;

		current_skin_ = QUEST_SKIN_NORMAL;

		{
			std::ifstream inf((quest_dir + "/questnpc.txt").c_str());
			int line = 0;

			if (!inf.is_open())
				metin2::log::error( "QUEST Cannot open 'questnpc.txt'");
			else
				metin2::log::info( "QUEST can open 'questnpc.txt' ({})", quest_dir.c_str() );

			while (1)
			{
				unsigned int vnum;

				inf >> vnum;

				line++;

				if (inf.fail())
					break;

				std::string s;
				getline(inf, s);
				unsigned int li = 0, ri = s.size()-1;
				while (li < s.size() && isspace(s[li])) li++;
				while (ri > 0 && isspace(s[ri])) ri--;

				if (ri < li) 
				{
					metin2::log::error("QUEST questnpc.txt:{:d}:npc name error",line);
					continue;
				}

				s = s.substr(li, ri-li+1);

				int	n = 0;
				str_to_number(n, s.c_str());
				if (n)
					continue;

				if ( metin2::config::test_server() )
					metin2::log::info( "QUEST reading script of {}({:d})", s.c_str(), vnum);
				npc_[vnum].set(vnum, s);
				npc_name_id_[s] = vnum;
			}

			// notarget quest
			npc_[0].set(0, "notarget");
		}

		set_event_flag("guild_withdraw_delay", 1);
		set_event_flag("guild_disband_delay", 1);
		return true;
	}

	unsigned int quest_manager::find_npcid_by_name(const std::string& name)
	{
		std::map<std::string, unsigned int>::iterator it = npc_name_id_.find(name);
		return it != npc_name_id_.end() ? it->second : 0;
	}

	void quest_manager::select_item(unsigned int pc, unsigned int selection)
	{
		game::quest::pc* pPC = get_pc(pc);
		if (pPC && pPC->is_running() && pPC->running_quest_state()->suspend_state == SUSPEND_STATE_SELECT_ITEM)
		{
			pPC->set_send_done_flag();
			pPC->running_quest_state()->args=1;
			lua_pushnumber(pPC->running_quest_state()->co,selection);

			if (!run_state(*pPC->running_quest_state()))
			{
				close_state(*pPC->running_quest_state());
				pPC->end_running();
			}
		}
	}

	void quest_manager::confirm(unsigned int pc, quest_confirm_type confirm, unsigned int pc2)
	{
		game::quest::pc* pPC = get_pc(pc);

		if (!pPC || !pPC->is_running())
		{
			metin2::log::error("no quest running for pc, cannot process input : {:d}", pc);
			return;
		}

		if (pPC->running_quest_state()->suspend_state != SUSPEND_STATE_CONFIRM)
		{
			metin2::log::error("not wait for a confirm : {:d} {:d}", pc, pPC->running_quest_state()->suspend_state);
			return;
		}

		if (pc2 && !pPC->is_confirm_wait(pc2))
		{
			metin2::log::error("not wait for a confirm : {:d} {:d}", pc, pPC->running_quest_state()->suspend_state);
			return;
		}

		pPC->clear_confirm_wait();

		pPC->set_send_done_flag();

		pPC->running_quest_state()->args=1;
		lua_pushnumber(pPC->running_quest_state()->co, confirm);

		add_script("[END_CONFIRM_WAIT]");
		set_skin_style(QUEST_SKIN_NOWINDOW);
		send_script();

		if (!run_state(*pPC->running_quest_state()))
		{
			close_state(*pPC->running_quest_state());
			pPC->end_running();
		}

	}

	void quest_manager::input(unsigned int pc, const char* msg)
	{
		game::quest::pc* pPC = get_pc(pc);
		if (!pPC)
		{
			metin2::log::error("no pc! : {:d}",pc);
			return;
		}

		if (!pPC->is_running())
		{
			metin2::log::error("no quest running for pc, cannot process input : {:d}", pc);
			return;
		}

		if (pPC->running_quest_state()->suspend_state != SUSPEND_STATE_INPUT)
		{
			metin2::log::error("not wait for a input : {:d} {:d}", pc, pPC->running_quest_state()->suspend_state);
			return;
		}

		pPC->set_send_done_flag();

		pPC->running_quest_state()->args=1;
		lua_pushstring(pPC->running_quest_state()->co,msg);

		if (!run_state(*pPC->running_quest_state()))
		{
			close_state(*pPC->running_quest_state());
			pPC->end_running();
		}
	}

	void quest_manager::select(unsigned int pc, unsigned int selection)
	{
		game::quest::pc* pPC;

		if ((pPC = get_pc(pc)) && pPC->is_running() && pPC->running_quest_state()->suspend_state==SUSPEND_STATE_SELECT)
		{
			pPC->set_send_done_flag();

			if (!pPC->running_quest_state()->chat_scripts.empty())
			{
				// For chat events
				// The current quest is one that picks which quest to run, so
				// finish it and run the selected quest.
				quest_state& old_qs = *pPC->running_quest_state();
				close_state(old_qs);

				if (selection >= pPC->running_quest_state()->chat_scripts.size())
				{
					pPC->set_send_done_flag();
					goto_end_state(old_qs);
					pPC->end_running();
				}
				else
				{
					a_arg_script* pas = pPC->running_quest_state()->chat_scripts[selection];
					execute_quest_script(*pPC, pas->quest_index, pas->state_index, pas->script.get_code(), pas->script.size());
				}
			}
			else
			{
				// on default 
				pPC->running_quest_state()->args=1;
				lua_pushnumber(pPC->running_quest_state()->co,selection+1);

				if (!run_state(*pPC->running_quest_state()))
				{
					close_state(*pPC->running_quest_state());
					pPC->end_running();
				}
			}
		}
		else
		{
			metin2::log::error("wrong QUEST_SELECT request! : {:d}",pc);
		}
	}

	void quest_manager::resume(unsigned int pc)
	{
		game::quest::pc * pPC;

		if ((pPC = get_pc(pc)) && pPC->is_running() && pPC->running_quest_state()->suspend_state == SUSPEND_STATE_PAUSE)
		{
			pPC->set_send_done_flag();
			pPC->running_quest_state()->args = 0;

			if (!run_state(*pPC->running_quest_state()))
			{
				close_state(*pPC->running_quest_state());
				pPC->end_running();
			}
		}
		else
		{
			metin2::log::error("wrong QUEST_WAIT request! : {:d}",pc);
		}
	}

	void quest_manager::enter_state(uint32_t pc, uint32_t quest_index, int state)
	{
		game::quest::pc* pPC;
		if ((pPC = get_pc(pc)))
		{
			if (!check_quest_loaded(pPC))
				return;

			npc_[QUEST_NO_NPC].on_enter_state(*pPC, quest_index, state);
		}
		else
			metin2::log::error("QUEST no such pc id : {:d}", pc);
	}

	void quest_manager::leave_state(uint32_t pc, uint32_t quest_index, int state)
	{
		game::quest::pc* pPC;
		if ((pPC = get_pc(pc)))
		{
			if (!check_quest_loaded(pPC))
				return;

			npc_[QUEST_NO_NPC].on_leave_state(*pPC, quest_index, state);
		}
		else
			metin2::log::error("QUEST no such pc id : {:d}", pc);
	}

	void quest_manager::letter(uint32_t pc, uint32_t quest_index, int state)
	{
		game::quest::pc* pPC;
		if ((pPC = get_pc(pc)))
		{
			if (!check_quest_loaded(pPC))
				return;

			npc_[QUEST_NO_NPC].on_letter(*pPC, quest_index, state);
		}
		else
			metin2::log::error("QUEST no such pc id : {:d}", pc);
	}

	void quest_manager::logout_pc(character * ch)
	{
		pc * pPC = get_pc(ch->player_id());

		if (pPC && pPC->is_running())
		{
			close_state(*pPC->running_quest_state());
			pPC->cancel_running();
		}

		// Log out before deleting.
		logout(ch->player_id());

		if (ch == current_character_)
		{
			current_character_ = nullptr;
			current_pc_ = nullptr;
		}
	}

	///////////////////////////////////////////////////////////////////////////////////////////
	//
	// Quest Event related
	//
	///////////////////////////////////////////////////////////////////////////////////////////
	void quest_manager::login(unsigned int pc, const char * quest)
	{
		game::quest::pc * pPC;

		if ((pPC = get_pc(pc)))
		{
			if (!check_quest_loaded(pPC))
				return;

			npc_[QUEST_NO_NPC].on_login(*pPC, quest);
		}
		else
		{
			metin2::log::error("QUEST no such pc id : {:d}", pc);
		}
	}

	void quest_manager::logout(unsigned int pc)
	{
		game::quest::pc * pPC;

		if ((pPC = get_pc(pc)))
		{
			if (!check_quest_loaded(pPC))
				return;

			npc_[QUEST_NO_NPC].on_logout(*pPC);
		}
		else
			metin2::log::error("QUEST no such pc id : {:d}", pc);
	}

	void quest_manager::kill(unsigned int pc, unsigned int npc)
	{
		game::quest::pc * pPC;

		metin2::log::info( "CQuestManager::Kill QUEST_KILL_EVENT (pc={:d}, npc={:d})", pc, npc);

		if ((pPC = get_pc(pc)))
		{
			if (!check_quest_loaded(pPC))
				return;

			/* [hyo] fix for duplicate counting on mob kill
			   even if a script was handled due to code like when 171.kill begin ... in the quest script,
			   changed to not return immediately and also perform other checks. (2011/07/21)
			*/   
			// call script
			
			if(npc > 0) // Fix
			{
				npc_[npc].on_kill(*pPC);
			}

			character * ch = current_character_ptr();
			party * pParty = ch->get_party();
			character * leader = pParty ? pParty->leader_character() : ch;

			if (leader)
			{
				current_party_member_ = ch;

				if (npc_[npc].on_party_kill(*get_pc(leader->player_id())))
					return;

				pPC = get_pc(pc);
			}

			if (npc_[QUEST_NO_NPC].on_kill(*pPC))
				return;

			if (leader)
			{
				current_party_member_ = ch;
				npc_[QUEST_NO_NPC].on_party_kill(*get_pc(leader->player_id()));
			}
		}
		else
			metin2::log::error("QUEST: no such pc id : {:d}", pc);
	}

	bool quest_manager::server_timer(unsigned int npc, unsigned int arg)
	{
		set_server_timer_arg(arg);
		metin2::log::info( "XXX ServerTimer Call NPC {}", static_cast<const void *>(pc_force(0)));
		current_pc_ = pc_force(0);
		current_character_ = nullptr;
		selected_dungeon_ = nullptr;
		return npc_[npc].on_server_timer(*current_pc_);
	}

	bool quest_manager::timer(unsigned int pc, unsigned int npc)
	{
		game::quest::pc* pPC;

		if ((pPC = get_pc(pc)))
		{
			if (!check_quest_loaded(pPC))
			{
				return false;
			}
			// call script
			return npc_[npc].on_timer(*pPC);
		}
		else
		{
			metin2::log::error("QUEST TIMER_EVENT no such pc id : {:d}", pc);
			return false;
		}
	}

	void quest_manager::level_up(unsigned int pc)
	{
		game::quest::pc * pPC;

		if ((pPC = get_pc(pc)))
		{
			if (!check_quest_loaded(pPC))
				return;

			npc_[QUEST_NO_NPC].on_level_up(*pPC);
		}
		else
		{
			metin2::log::error("QUEST LEVELUP_EVENT no such pc id : {:d}", pc);
		}
	}

	void quest_manager::attr_in(unsigned int pc, character * ch, int attr)
	{
		game::quest::pc* pPC;
		if ((pPC = get_pc(pc)))
		{
			current_party_member_ = ch;
			if (!check_quest_loaded(pPC))
				return;

			// call script
			npc_[attr+QUEST_ATTR_NPC_START].on_attr_in(*pPC);
		}
		else
		{
			metin2::log::error("QUEST no such pc id : {:d}", pc);
		}
	}

	void quest_manager::attr_out(unsigned int pc, character * ch, int attr)
	{
		game::quest::pc* pPC;
		if ((pPC = get_pc(pc)))
		{
			current_party_member_ = ch;
			if (!check_quest_loaded(pPC))
				return;

			// call script
			npc_[attr+QUEST_ATTR_NPC_START].on_attr_out(*pPC);
		}
		else
		{
			metin2::log::error("QUEST no such pc id : {:d}", pc);
		}
	}

	bool quest_manager::target(unsigned int pc, uint32_t quest_index, const char * target_name, const char * verb)
	{
		game::quest::pc * pPC;

		if ((pPC = get_pc(pc)))
		{
			if (!check_quest_loaded(pPC))
				return false;

			bool bRet;
			return npc_[QUEST_NO_NPC].on_target(*pPC, quest_index, target_name, verb, bRet);
		}

		return false;
	}

	void quest_manager::quest_info(unsigned int pc, unsigned int quest_index)
	{
		game::quest::pc* pPC;

		if ((pPC = get_pc(pc)))
		{
			// call script
			if (!check_quest_loaded(pPC))
			{
				character * ch = character_manager::instance().find_by_pid(pc);

				if (ch)
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("퀘스트를 로드하는 중입니다. 잠시만 기다려 주십시오."));

				return;
			}

			npc_[QUEST_NO_NPC].on_info(*pPC, quest_index);
		}
		else
		{
			metin2::log::error("QUEST INFO_EVENT no such pc id : {:d}", pc);
		}
	}

	void quest_manager::quest_button(unsigned int pc, unsigned int quest_index)
	{
		game::quest::pc* pPC;
		if ((pPC = get_pc(pc)))
		{
			// call script
			if (!check_quest_loaded(pPC))
			{
				character * ch = character_manager::instance().find_by_pid(pc);
				if (ch)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("퀘스트를 로드하는 중입니다. 잠시만 기다려 주십시오."));
				}
				return;
			}
			npc_[QUEST_NO_NPC].on_button(*pPC, quest_index);
		}
		else
		{
			metin2::log::error("QUEST CLICK_EVENT no such pc id : {:d}", pc);
		}
	}

	bool quest_manager::take_item(unsigned int pc, unsigned int npc, item * item)
	{
		game::quest::pc* pPC;

		if ((pPC = get_pc(pc)))
		{
			if (!check_quest_loaded(pPC))
			{
				character * ch = character_manager::instance().find_by_pid(pc);
				if (ch)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("퀘스트를 로드하는 중입니다. 잠시만 기다려 주십시오."));
				}
				return false;
			}
			// call script
			set_current_item(item);
			return npc_[npc].on_take_item(*pPC);
		}
		else
		{
			metin2::log::error("QUEST USE_ITEM_EVENT no such pc id : {:d}", pc);
			return false;
		}
	}

	bool quest_manager::use_item(unsigned int pc, item * item, bool receive_all)
	{
		if (metin2::config::test_server())
			metin2::log::info( "questmanager::UseItem Start : itemVnum : {:d} PC : {:d}", item->original_vnum(), pc);
		game::quest::pc* pPC;
		if ((pPC = get_pc(pc)))
		{
			if (!check_quest_loaded(pPC))
			{
				character * ch = character_manager::instance().find_by_pid(pc);
				if (ch)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("퀘스트를 로드하는 중입니다. 잠시만 기다려 주십시오."));
				}
				return false;
			}
			// call script
			set_current_item(item);

			return npc_[item->vnum()].on_use_item(*pPC, receive_all);
		}
		else
		{
			metin2::log::error("QUEST USE_ITEM_EVENT no such pc id : {:d}", pc);
			return false;
		}
	}

	// Group Use defined in Special Item Group
	bool quest_manager::sig_use(unsigned int pc, uint32_t sig_vnum, item * item, bool receive_all)
	{
		if (metin2::config::test_server())
			metin2::log::info( "questmanager::SIGUse Start : itemVnum : {:d} PC : {:d}", item->original_vnum(), pc);
		game::quest::pc* pPC;
		if ((pPC = get_pc(pc)))
		{
			if (!check_quest_loaded(pPC))
			{
				character * ch = character_manager::instance().find_by_pid(pc);
				if (ch)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("퀘스트를 로드하는 중입니다. 잠시만 기다려 주십시오."));
				}
				return false;
			}
			// call script
			set_current_item(item);

			return npc_[sig_vnum].on_sig_use(*pPC, receive_all);
		}
		else
		{
			metin2::log::error("QUEST USE_ITEM_EVENT no such pc id : {:d}", pc);
			return false;
		}
	}

	bool quest_manager::give_item_to_pc(unsigned int pc, character * chr)
	{
		if (!chr->is_pc())
			return false;

		game::quest::pc * pPC = get_pc(pc);

		if (pPC)
		{
			if (!check_quest_loaded(pPC))
				return false;

			target_info * pInfo = target_manager::instance().get_target_info(pc, TARGET_TYPE_VID, chr->get_vid());

			if (pInfo)
			{
				bool bRet;

				if (npc_[QUEST_NO_NPC].on_target(*pPC, pInfo->quest_index, pInfo->target_name, "click", bRet))
					return true;
			}
		}

		return false;
	}

	bool quest_manager::click(unsigned int pc, character * chr_target)
	{
		game::quest::pc * pPC = get_pc(pc);

		if (pPC)
		{
			if (!check_quest_loaded(pPC))
			{
				character * ch = character_manager::instance().find_by_pid(pc);

				if (ch)
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("퀘스트를 로드하는 중입니다. 잠시만 기다려 주십시오."));

				return false;
			}

			target_info * pInfo = target_manager::instance().get_target_info(pc, TARGET_TYPE_VID, chr_target->get_vid());
			if (metin2::config::test_server())
			{
				metin2::log::info( "CQuestManager::Click(pid={:d}, npc_name={}) - target_info({})", pc, chr_target->name(), static_cast<const void *>(static_cast<void*>(pInfo)));
			}

			if (pInfo)
			{
				bool bRet;
				if (npc_[QUEST_NO_NPC].on_target(*pPC, pInfo->quest_index, pInfo->target_name, "click", bRet))
					return bRet;
			}

			uint32_t dwCurrentNPCRace = chr_target->race_num();

			if (chr_target->is_npc())
			{
				std::map<unsigned int, npc>::iterator it = npc_.find(dwCurrentNPCRace);

				if (it == npc_.end())
				{
					metin2::log::error("CQuestManager::Click(pid={:d}, target_npc_name={}) - NOT EXIST NPC RACE VNUM[{:d}]",
							pc, 
							chr_target->name(), 
							dwCurrentNPCRace);
					return false;
				}

				// call script
				if (it->second.has_chat())
				{
					// if have chat, give chat
					if (metin2::config::test_server())
						metin2::log::info( "CQuestManager::Click->OnChat");

					if (!it->second.on_chat(*pPC))
					{
						if (metin2::config::test_server())
							metin2::log::info( "CQuestManager::Click->OnChat Failed");

						return it->second.on_click(*pPC);
					}

					return true;
				}
				else
				{
					// else click
					return it->second.on_click(*pPC);
				}
			}
			return false;
		}
		else
		{
			metin2::log::error("QUEST CLICK_EVENT no such pc id : {:d}", pc);
			return false;
		}
	}

	void quest_manager::unmount(unsigned int pc)
	{
		game::quest::pc * pPC;

		if ((pPC = get_pc(pc)))
		{
			if (!check_quest_loaded(pPC))
				return;

			npc_[QUEST_NO_NPC].on_unmount(*pPC);
		}
		else
			metin2::log::error("QUEST no such pc id : {:d}", pc);
	}
	//German gift feature test
	void quest_manager::item_informer(unsigned int pc,unsigned int vnum)
	{
		
		game::quest::pc* pPC;
		pPC = get_pc(pc);
		
		npc_[QUEST_NO_NPC].on_item_informer(*pPC,vnum);
	}
	///////////////////////////////////////////////////////////////////////////////////////////
	// END OF quest event handling
	///////////////////////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////////////////////
	void quest_manager::load_start_quest(const std::string& quest_name, unsigned int idx)
	{
		for (auto it = quest_object_dir.begin(); it != quest_object_dir.end(); ++it)
		{
			const std::string& stQuestObjectDir = *it;
			std::string full_name = stQuestObjectDir + "/begin_condition/" + quest_name;
			std::ifstream inf(full_name.c_str());

			if (inf.is_open())
			{
				metin2::log::info( "QUEST loading begin condition for {}", quest_name.c_str());

				std::istreambuf_iterator<char> ib(inf), ie;
				copy(ib, ie, back_inserter(hm_quest_start_script_[idx]));
			}
		}
	}

	bool quest_manager::can_start_quest(unsigned int quest_index, const pc& pc)
	{
		return can_start_quest(quest_index);
	}

	bool quest_manager::can_start_quest(unsigned int quest_index)
	{
		hash_map_quest_start_script::iterator it;

		if ((it = hm_quest_start_script_.find(quest_index)) == hm_quest_start_script_.end())
			return true;
		else
		{
			int x = lua_gettop(l_);
			lua_dobuffer(l_, &(it->second[0]), it->second.size(), "StartScript");
			int bStart = lua_toboolean(l_, -1);
			lua_settop(l_, x);
			return bStart != 0;
		}
	}

	void quest_manager::disconnect_pc(character * ch)
	{
		pc_.erase(ch->player_id());
	}

	pc * quest_manager::pc_force(unsigned int pc)
	{
		pc_map::iterator it;

		if ((it = pc_.find(pc)) == pc_.end())
		{
			game::quest::pc * pPC = &pc_[pc];
			pPC->set_id(pc);
			return pPC;
		}

		return &it->second;
	}

	pc * quest_manager::get_pc(unsigned int pc)
	{
		pc_map::iterator it;

		character * pkChr = character_manager::instance().find_by_pid(pc);

		if (!pkChr)
			return nullptr;

		current_pc_ = pc_force(pc);
		current_character_ = pkChr;
		selected_dungeon_ = nullptr;
		return (current_pc_);
	}

	void quest_manager::clear_script()
	{
		script_.clear();
		current_skin_ = QUEST_SKIN_NORMAL;
	}

	void quest_manager::add_script(const std::string& str)
	{
		script_+=str;
	}

	void quest_manager::send_script()
	{
		if (no_send_)
		{
			no_send_ = false;
			clear_script();
			return;
		}

		if (script_=="[DONE]" || script_ == "[NEXT]")
		{
			if (current_pc_ && !current_pc_->and_reset_done_flag() && script_=="[DONE]" && current_skin_ == QUEST_SKIN_NORMAL && !is_error())
			{
				clear_script();
				return;
			}
			current_skin_ = QUEST_SKIN_NOWINDOW;
		}

		//send -_-!
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
			metin2::log::info( "m_strScript {} size {:d}", script_.c_str(), buf.size());

		clear_script();
	}

	const char* quest_manager::quest_state_name(const std::string& quest_name, const int state_index)
	{
		int x = lua_gettop(l_);
		lua_getglobal(l_, quest_name.c_str());
		if (lua_isnil(l_,-1))
		{
			metin2::log::error("QUEST wrong quest state file {}.{:d}", quest_name.c_str(), state_index);
			lua_settop(l_,x);
			return "";
		}
		lua_pushnumber(l_, state_index);
		lua_gettable(l_, -2);

		const char* str = lua_tostring(l_, -1);
		lua_settop(l_, x);
		return str;
	}

	int quest_manager::quest_state_index(const std::string& quest_name, const std::string& state_name)
	{
		int x = lua_gettop(l_);
		lua_getglobal(l_, quest_name.c_str());
		if (lua_isnil(l_,-1))
		{
			metin2::log::error("QUEST wrong quest state file {}.{}",quest_name.c_str(),state_name.c_str()  );
			lua_settop(l_,x);
			return 0;
		}
		lua_pushstring(l_, state_name.c_str());
		lua_gettable(l_, -2);

		int v = (int)rint(lua_tonumber(l_,-1));
		lua_settop(l_, x);
		if ( metin2::config::test_server() )
			metin2::log::info("[QUESTMANAGER] GetQuestStateIndex x({:d}) v({:d}) {} {}", v,x, quest_name.c_str(), state_name.c_str() );
		return v;
	}

	void quest_manager::set_skin_style(int style)
	{
		if (style<0 || style >= QUEST_SKIN_COUNT)
		{
			current_skin_ = QUEST_SKIN_NORMAL;
		}
		else
			current_skin_ = style;
	}

	unsigned int quest_manager::load_timer_script(const std::string& name)
	{
		std::map<std::string, unsigned int>::iterator it;
		if ((it = timer_id_.find(name)) != timer_id_.end())
		{
			return it->second;
		}
		else
		{
			unsigned int new_id = UINT_MAX - timer_id_.size();

			npc_[new_id].set(new_id, name);
			timer_id_.insert(make_pair(name, new_id));

			return new_id;
		}
	}

	unsigned int quest_manager::current_npc_race()
	{
		return current_npc_character_ptr() ? current_npc_character_ptr()->race_num() : 0;
	}

	item * quest_manager::current_item()
	{
		return current_character_ptr() ? current_character_ptr()->quest_item_ptr() : nullptr; 
	}

	void quest_manager::clear_current_item()
	{
		if (current_character_ptr())
			current_character_ptr()->clear_quest_item_ptr();
	}

	void quest_manager::set_current_item(item * item)
	{
		if (current_character_ptr())
			current_character_ptr()->set_quest_item_ptr(item);
	}

	character * quest_manager::current_npc_character_ptr()
	{ 
		return current_character_ptr() ? current_character_ptr()->quest_npc() : nullptr; 
	}

	const std::string & quest_manager::current_quest_name()
	{
		return current_pc()->current_quest_name();
	}

	dungeon * quest_manager::current_dungeon()
	{
		character * ch = current_character_ptr();

		if (!ch)
		{
			if (selected_dungeon_)
				return selected_dungeon_;
			return nullptr;
		}

		return ch->dungeon_force();
	}

	void quest_manager::register_quest(const std::string & quest_name, unsigned int idx)
	{
		assert(idx > 0);

		decltype(hm_quest_name_.begin()) it;

		if ((it = hm_quest_name_.find(quest_name)) != hm_quest_name_.end())
			return;

		hm_quest_name_.insert(make_pair(quest_name, idx));
		load_start_quest(quest_name, idx);
		quest_name_by_index_.insert(make_pair(idx, quest_name));

		metin2::log::info( "QUEST: Register {:4d} {}", idx, quest_name.c_str());
	}

	unsigned int quest_manager::quest_index_by_name(const std::string& name)
	{
		hash_map_quest_name::iterator it = hm_quest_name_.find(name);

		if (it == hm_quest_name_.end())
			return 0; // RESERVED

		return it->second;
	}

	const std::string & quest_manager::quest_name_by_index(unsigned int idx)
	{
		decltype(quest_name_by_index_.begin()) it;

		if ((it = quest_name_by_index_.find(idx)) == quest_name_by_index_.end())
		{
			metin2::log::error("cannot find quest name by index {:d}", idx);
			assert(!"cannot find quest name by index");

			static std::string st = "";
			return st;
		}

		return it->second;
	}

	void quest_manager::send_event_flag_list(character * ch)
	{
		decltype(event_flag_.begin()) it;
		for (it = event_flag_.begin(); it != event_flag_.end(); ++it)
		{
			const std::string& flagname = it->first;
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

	void quest_manager::request_set_event_flag(const std::string& name, int value)
	{
		packet_set_event_flag p;
		strlcpy(p.flag_name, name.c_str(), sizeof(p.flag_name));
		p.value = value;
		db_clientdesc->db_packet(HEADER_GD_SET_EVENT_FLAG, 0, &p, sizeof(packet_set_event_flag));
	}

	void quest_manager::set_event_flag(const std::string& name, int value)
	{
		static const char*	DROPEVENT_CHARTONE_NAME		= "drop_char_stone";
		static const int	DROPEVENT_CHARTONE_NAME_LEN = strlen(DROPEVENT_CHARTONE_NAME);

		int prev_value = event_flag_[name];

		metin2::log::info( "QUEST eventflag {} {:d} prev_value {:d}", name.c_str(), value, event_flag_[name]);
		event_flag_[name] = value;

		if (name == "mob_item")
		{
			character_manager::instance().set_mob_item_rate(value);
		}
		else if (name == "mob_dam")
		{
			character_manager::instance().set_mob_damage_rate(value);
		}
		else if (name == "mob_gold")
		{
			character_manager::instance().set_mob_gold_amount_rate(value);
		}
		else if (name == "mob_gold_pct")
		{
			character_manager::instance().set_mob_gold_drop_rate(value);
		}
		else if (name == "mob_exp")
		{
			character_manager::instance().set_mob_exp_rate(value);
		}
		else if (name == "mob_item_buyer")
		{
			character_manager::instance().set_mob_item_rate_premium(value);
		}
		else if (name == "mob_exp_buyer")
		{
			character_manager::instance().set_mob_exp_rate_premium(value);
		}
		else if (name == "mob_gold_buyer")
		{
			character_manager::instance().set_mob_gold_amount_rate_premium(value);
		}
		else if (name == "mob_gold_pct_buyer")
		{
			character_manager::instance().set_mob_gold_drop_rate_premium(value);
		}
		else if (!name.compare(0,5,"xmas_"))
		{
			xmas::process_event_flag(name, prev_value, value);
		}
		else if (name == "newyear_boom")
		{
			const desc_manager::desc_set & c_ref_set = desc_manager::instance().client_set();

			for (auto it = c_ref_set.begin(); it != c_ref_set.end(); ++it)
			{
				character * ch = (*it)->get_character();

				if (!ch)
					continue;

				ch->chat_packet(CHAT_TYPE_COMMAND, "newyear_boom %d", value);
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
			
			const desc_manager::desc_set & c_ref_set = desc_manager::instance().client_set();

			for (auto it = c_ref_set.begin(); it != c_ref_set.end(); ++it)
			{
				character * ch = (*it)->get_character();
				if (!ch)
					continue;

				ch->chat_packet(CHAT_TYPE_COMMAND, "DayMode %s", mode.c_str());
			}
		}
		else if (name == "day")
		{
			const desc_manager::desc_set & c_ref_set = desc_manager::instance().client_set();

			for (auto it = c_ref_set.begin(); it != c_ref_set.end(); ++it)
			{
				character * ch = (*it)->get_character();
				if (!ch)
					continue;
				if (value)
				{
					// Night
					ch->chat_packet(CHAT_TYPE_COMMAND, "DayMode dark");
				}
				else
				{
					// Day
					ch->chat_packet(CHAT_TYPE_COMMAND, "DayMode light");
				}
			}

			if (value && !prev_value)
			{
				// Create if missing
				struct npc_sell_firework_position
				{
					int32_t map_index;
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

				npc_sell_firework_position* p = positions;
				while (p->map_index)
				{
					if (map_allow_find(p->map_index))
					{
						pixel_position posBase;
						if (!sectree_manager::instance().map_base_position_by_map_index(p->map_index, posBase))
						{
							metin2::log::error("cannot get map base position {:d}", p->map_index);
							++p;
							continue;
						}

						character_manager::instance().spawn_mob(xmas::MOB_XMAS_FIRWORK_SELLER_VNUM, p->map_index, posBase.x + p->x * 100, posBase.y + p->y * 100, 0, false, -1);
					}
					p++;
				}
			}
			else if (!value && prev_value)
			{
				// Delete if present
				character_vector_interactor i;

				if (character_manager::instance().characters_by_race_num(xmas::MOB_XMAS_FIRWORK_SELLER_VNUM, i))
				{
					character_vector_interactor::iterator it = i.begin();

					while (it != i.end()) {
						character_manager::instance().destroy_character(*it++);
					}
				}
			}
		}
		else if (name == "pre_event_hc")
		{
			const uint32_t EventNPC = 20090;

			struct event_npc_position
			{
				int32_t map_index;
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
				event_npc_position* pPosition = positions;

				while (pPosition->map_index)
				{
					if (map_allow_find(pPosition->map_index))
					{
						pixel_position pos;

						if (!sectree_manager::instance().map_base_position_by_map_index(pPosition->map_index, pos))
						{
							metin2::log::error("cannot get map base position {:d}", pPosition->map_index);
							++pPosition;
							continue;
						}

						character_manager::instance().spawn_mob(EventNPC, pPosition->map_index, pos.x+pPosition->x*100, pos.y+pPosition->y*100, 0, false, -1);
					}
					pPosition++;
				}
			}
			else if (!value && prev_value)
			{
				character_vector_interactor i;

				if (character_manager::instance().characters_by_race_num(EventNPC, i))
				{
					character_vector_interactor::iterator it = i.begin();

					while (it != i.end())
					{
						character * ch = *it++;

						switch (ch->map_index())
						{
							case 3:
							case 23:
							case 43:
								character_manager::instance().destroy_character(ch);
								break;
						}
					}
				}
			}
		}
		else if (name.compare(0, DROPEVENT_CHARTONE_NAME_LEN, DROPEVENT_CHARTONE_NAME)== 0)
		{
			drop_event_char_stone_set_value(name, value);
		}
		else if (name.compare(0, strlen("refine_box"), "refine_box")== 0)
		{
			drop_event_refine_box_set_value(name, value);
		}
		else if (name == "gold_drop_limit_time")
		{
			gold_drop_time_limit_value = value * 1000;
		}
		else if (name == "new_xmas_event")
		{
			// 20126 new Santa.
			static uint32_t new_santa = 20126;
			if (value != 0)
			{
				character_vector_interactor i;
				bool map1_santa_exist = false;
				bool map21_santa_exist = false;
				bool map41_santa_exist = false;
				
				if (character_manager::instance().characters_by_race_num(new_santa, i))
				{
					character_vector_interactor::iterator it = i.begin();

					while (it != i.end())
					{
						character * tch = *(it++);

						if (tch->map_index() == 1)
						{
							map1_santa_exist = true;
						}
						else if (tch->map_index() == 21)
						{
							map21_santa_exist = true;
						}
						else if (tch->map_index() == 41)
						{
							map41_santa_exist = true;
						}
					}
				}

				if (map_allow_find(1) && !map1_santa_exist)
				{
					sectree_map * pkSectreeMap = sectree_manager::instance().map(1);
					character_manager::instance().spawn_mob(new_santa, 1, pkSectreeMap->setting.base_x + 60800, pkSectreeMap->setting.base_y + 61700, 0, false, 90, true);
				}
				if (map_allow_find(21) && !map21_santa_exist)
				{
					sectree_map * pkSectreeMap = sectree_manager::instance().map(21);
					character_manager::instance().spawn_mob(new_santa, 21, pkSectreeMap->setting.base_x + 59600, pkSectreeMap->setting.base_y + 61000, 0, false, 110, true);
				}
				if (map_allow_find(41) && !map41_santa_exist)
				{
					sectree_map * pkSectreeMap = sectree_manager::instance().map(41);
					character_manager::instance().spawn_mob(new_santa, 41, pkSectreeMap->setting.base_x + 35700, pkSectreeMap->setting.base_y + 74300, 0, false, 140, true);
				}
			}
			else
			{
				character_vector_interactor i;
				character_manager::instance().characters_by_race_num(new_santa, i);
				
				for (character_vector_interactor::iterator it = i.begin(); it != i.end(); it++)
				{
					character_manager::instance().destroy_character(*it);
				}
			}
		}
	}

	int	quest_manager::event_flag(const std::string& name)
	{
		std::map<std::string,int>::iterator it = event_flag_.find(name);

		if (it == event_flag_.end())
			return 0;

		return it->second;
	}

	void quest_manager::broadcast_event_flag_on_login(character * ch)
	{
		int iEventFlagValue;

		if ((iEventFlagValue = quest::quest_manager::instance().event_flag("xmas_snow")))
		{
			ch->chat_packet(CHAT_TYPE_COMMAND, "xmas_snow %d", iEventFlagValue);
		}

		if ((iEventFlagValue = quest::quest_manager::instance().event_flag("xmas_boom")))
		{
			ch->chat_packet(CHAT_TYPE_COMMAND, "xmas_boom %d", iEventFlagValue);
		}

		if ((iEventFlagValue = quest::quest_manager::instance().event_flag("xmas_tree")))
		{
			ch->chat_packet(CHAT_TYPE_COMMAND, "xmas_tree %d", iEventFlagValue);
		}

		if ((iEventFlagValue = quest::quest_manager::instance().event_flag("day")))
		{
			ch->chat_packet(CHAT_TYPE_COMMAND, "DayMode dark");
		}

		if ((iEventFlagValue = quest::quest_manager::instance().event_flag("newyear_boom")))
		{
			ch->chat_packet(CHAT_TYPE_COMMAND, "newyear_boom %d", iEventFlagValue);
		}

		if ( (iEventFlagValue = quest::quest_manager::instance().event_flag("eclipse")) )
		{
			std::string mode;

			if ( iEventFlagValue == 1 ) mode = "dark";
			else mode = "light";

			ch->chat_packet(CHAT_TYPE_COMMAND, "DayMode %s", mode.c_str());
		}
	}

	void quest_manager::reload()
	{
		lua_close(l_);
		npc_.clear();
		npc_name_id_.clear();
		hm_quest_name_.clear();
		timer_id_.clear();
		hm_quest_start_script_.clear();
		event_name.clear();
		l_ = nullptr;
		initialize();

		for (auto it = registered_npc_vnum_.begin(); it != registered_npc_vnum_.end(); ++it)
		{
			char buf[256];
			uint32_t dwVnum = *it;
			snprintf(buf, sizeof(buf), "%u", dwVnum);
			npc_[dwVnum].set(dwVnum, buf);
		}
	}

	bool quest_manager::execute_quest_script(pc& pc, uint32_t quest_index, const int state, const char* code, const int code_size, std::vector<a_arg_script*>* chat_scripts, bool use_cache)
	{
		return execute_quest_script(pc, quest_manager::instance().quest_name_by_index(quest_index), state, code, code_size, chat_scripts, use_cache);
	}

	bool quest_manager::execute_quest_script(pc& pc, const std::string& quest_name, const int state, const char* code, const int code_size, std::vector<a_arg_script*>* chat_scripts_value, bool use_cache)
	{
		// Create the execution space
		quest_state qs = quest_manager::instance().open_state(quest_name, state);
		if (chat_scripts_value)
			qs.chat_scripts.swap(*chat_scripts_value);

		// Load the code
		if (use_cache)
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
		pc.set_quest(quest_name, qs);

		// Execute
		quest_state& rqs = *pc.running_quest_state();
		if (!quest_manager::instance().run_state(rqs))
		{
			quest_manager::instance().close_state(rqs);
			pc.end_running();
			return false;
		}
		return true;
	}

	void quest_manager::register_npc_vnum(uint32_t vnum)
	{
		if (registered_npc_vnum_.find(vnum) != registered_npc_vnum_.end())
			return;

		registered_npc_vnum_.insert(vnum);

		char buf[256];
		DIR* dir;

		for (auto it = quest_object_dir.begin(); it != quest_object_dir.end(); ++it)
		{
			const std::string& stQuestObjectDir = *it;
			snprintf(buf, sizeof(buf), "%s/%u", stQuestObjectDir.c_str(), vnum);
			metin2::log::info( "{}", buf);

			if ((dir = opendir(buf)))
			{
				closedir(dir);
				snprintf(buf, sizeof(buf), "%u", vnum);
				metin2::log::info( "{}", buf);

				npc_[vnum].set(vnum, buf);
			}
		}
	}

	void quest_manager::write_running_state_to_syserr()
	{
		const char * state_name = quest_state_name(current_quest_name(), current_state()->st);

		std::string event_index_name = "";
		for (auto it = event_name.begin(); it != event_name.end(); ++it)
		{
			if (it->second == running_event_index_)
			{
				event_index_name = it->first;
				break;
			}
		}

		metin2::log::error("LUA_ERROR: quest {}.{} {}", current_quest_name().c_str(), state_name, event_index_name.c_str() );
		if (current_character_ptr() && metin2::config::test_server())
			current_character_ptr()->chat_packet(CHAT_TYPE_PARTY, "LUA_ERROR: quest %s.%s %s", current_quest_name().c_str(), state_name, event_index_name.c_str() );
	}

	void report_error(const std::source_location & where, std::string_view message)
	{
		metin2::log::write(metin2::log::level::error, where, message);

		if (!metin2::config::test_server())
			return;

		if (character * ch = quest_manager::instance().current_character_ptr())
		{
			const std::string text(message);
			ch->chat_packet(CHAT_TYPE_PARTY, "error occurred on [%s:%u]", where.function_name(), static_cast<unsigned>(where.line()));
			ch->chat_packet(CHAT_TYPE_PARTY, "%s", text.c_str());
		}
	}

	void quest_manager::add_server_timer(const std::string& name, uint32_t arg, event_ptr event)
	{
		metin2::log::info( "XXX AddServerTimer {} {:d} {}", name.c_str(), arg, static_cast<const void *>(event.get()));
		if (server_timer_.find(make_pair(name, arg)) != server_timer_.end())
		{
			metin2::log::error("already registered server timer name:{} arg:{:d}", name.c_str(), arg);
			return;
		}
		server_timer_.insert(make_pair(make_pair(name, arg), event));
	}

	void quest_manager::clear_server_timer_not_cancel(const std::string& name, uint32_t arg)
	{
		server_timer_.erase(make_pair(name, arg));
	}

	void quest_manager::clear_server_timer(const std::string& name, uint32_t arg)
	{
		auto it = server_timer_.find(make_pair(name, arg));
		if (it != server_timer_.end())
		{
			event_ptr event = it->second;
			event_cancel(&event);
			server_timer_.erase(it);
		}
	}

	// Old
	
	// Fix (New)
	void quest_manager::cancel_server_timers(uint32_t arg)
	{
		std::vector<std::pair<std::string, uint32_t>> ServerTimersToDelete;

		for (const auto& kv : server_timer_) {
			if (kv.first.second == arg) {
				event_ptr event = kv.second;
				event_cancel(&event);
				ServerTimersToDelete.push_back(kv.first);
			}
		}

		// Delete all the required server timers
		for (const auto &timer : ServerTimersToDelete)
			server_timer_.erase(timer);

		// Clean up
		ServerTimersToDelete.clear();
	}

	void quest_manager::set_server_timer_arg(uint32_t arg)
	{
		server_timer_arg_ = arg;
	}

	uint32_t quest_manager::server_timer_arg()
	{
		return server_timer_arg_;
	}

	void quest_manager::select_dungeon(dungeon * dungeon_value)
	{
		selected_dungeon_ = dungeon_value;
	}
	
	bool quest_manager::pickup_item(unsigned int pc, item * item)
	{
		if (metin2::config::test_server())
			metin2::log::info( "questmanager::PickupItem Start : itemVnum : {:d} PC : {:d}", item->original_vnum(), pc);
		game::quest::pc* pPC;
		if ((pPC = get_pc(pc)))
		{
			if (!check_quest_loaded(pPC))
			{
				character * ch = character_manager::instance().find_by_pid(pc);
				if (ch)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("퀘스트를 로드하는 중입니다. 잠시만 기다려 주십시오."));
				}
				return false;
			}
			// call script
			set_current_item(item);

			return npc_[item->vnum()].on_pickup_item(*pPC);
		}
		else
		{
			metin2::log::error("QUEST PICK_ITEM_EVENT no such pc id : {:d}", pc);
			return false;
		}
	}
	void quest_manager::begin_other_pc_block(uint32_t pid)
	{
		character * ch = current_character_ptr();
		if (nullptr == ch)
		{
			metin2::log::error("NULL?");
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
		if (pc_stack_.empty())
		{
			other_pc_block_root_pc_ = current_pc();
		}
		pc_stack_.push_back(current_character_ptr()->player_id());
		get_pc(pid);
	}

	void quest_manager::end_other_pc_block()
	{
		if (pc_stack_.size() == 0)
		{
			metin2::log::error("m_vecPCStack is alread empty. CurrentQuest{{Name({}), State({})}}", current_quest_name().c_str(), current_state()->title.c_str());
			return;
		}
		uint32_t pc = pc_stack_.back();
		pc_stack_.pop_back();
		get_pc(pc);

		if (pc_stack_.empty())
		{
			other_pc_block_root_pc_ = nullptr;
		}
	}

	bool quest_manager::is_in_other_pc_block()
	{
		return !pc_stack_.empty();
	}

	pc*	quest_manager::other_pc_block_root_pc()
	{
		return other_pc_block_root_pc_;
	}
}
}
