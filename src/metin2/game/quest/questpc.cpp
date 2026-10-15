#include <metin2/game/quest/questpc.h>

#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/app/config.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/app/event.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/quest.h>
#include <metin2/game/quest/questevent.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/log/log.h>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace metin2::game {


namespace quest
{
	pc::pc() :
		is_given_reward_(false),
		should_send_done_(false),
		id_(0),
		running_quest_state_(nullptr),
		send_to_client_(0),
		loaded_(false),
		last_state_(0),
		wait_confirm_from_pid_(0),
		confirm_wait_(false)
	{
	}

	pc::~pc()
	{
		destroy();
	}

	void pc::destroy()
	{
		clear_timer();
	}

	void pc::set_id(uint32_t dwID)
	{
		id_ = dwID;
		should_send_done_ = false;
	}

	const std::string & pc::current_quest_name() const
	{
		return cur_quest_;
	}

	int	pc::current_quest_index()
	{
		return quest_manager::instance().quest_index_by_name(current_quest_name());
	}

	void pc::set_flag(const std::string& name, int value, bool bSkipSave)
	{
		if ( metin2::config::test_server() )
			metin2::log::info( "QUEST Setting flag {} {:d}", name.c_str(),value);
		else
			metin2::log::debug( "QUEST Setting flag {} {:d}", name.c_str(),value);

		if (value == 0)
		{
			delete_flag(name);
			return;
		}

		flag_map::iterator it = flag_map_.find(name);

		if (it == flag_map_.end())
			flag_map_.insert(make_pair(name, value));
		else if (it->second != value)
			it->second = value;
		else
			bSkipSave = true;

		if (!bSkipSave)
			save_flag(name, value);
	}

	bool pc::delete_flag(const std::string & name)
	{
		flag_map::iterator it = flag_map_.find(name);

		if (it != flag_map_.end())
		{
			flag_map_.erase(it);
			save_flag(name, 0);
			return true;
		}

		return false;
	}

	int pc::flag(const std::string & name)
	{
		flag_map::iterator it = flag_map_.find(name);

		if (it != flag_map_.end())
		{
			metin2::log::debug( "QUEST getting flag {} {:d}", name.c_str(),it->second);
			return it->second;
		}
		return 0;
	}

	void pc::save_flag(const std::string & name, int value)
	{
		flag_map::iterator it = flag_save_map_.find(name);

		if (it == flag_save_map_.end())
			flag_save_map_.insert(make_pair(name, value));
		else if (it->second != value)
			it->second = value;
	}

	// only from lua call
	void pc::set_current_quest_state_name(const std::string& state_name) 
	{
		set_flag(cur_quest_ + ".__status", quest_manager::instance().quest_state_index(cur_quest_,state_name));
	}

	void pc::set_quest_state(const std::string& quest_name, const std::string& state_name)
	{
		set_quest_state(quest_name, quest_manager::instance().quest_state_index(quest_name, state_name));
	}

	void pc::set_quest_state(const std::string& quest_name, int new_state_index)
	{
		int iNowState = flag(quest_name + ".__status");

		if (iNowState != new_state_index)
			add_quest_state_change(quest_name, iNowState, new_state_index);
	}

	void pc::add_quest_state_change(const std::string& quest_name, int prev_state, int next_state)
	{
		uint32_t dwQuestIndex = quest_manager::instance().quest_index_by_name(quest_name);
		metin2::log::info( "QUEST reserve Quest State Change quest {}[{:d}] from {:d} to {:d}", quest_name.c_str(), dwQuestIndex, prev_state, next_state);
		quest_state_change_.push_back(quest_state_change_info(dwQuestIndex, prev_state, next_state));
	}

	void pc::set_quest(const std::string& quest_name, quest_state& qs)
	{
		unsigned int qi = quest_manager::instance().quest_index_by_name(quest_name);
		quest_info::iterator it = quest_info_.find(qi);

		if (it == quest_info_.end())
			quest_info_.insert(std::make_pair(qi, qs));
		else
			it->second = qs;

		cur_quest_ = quest_name;
		running_quest_state_ = &quest_info_[qi];
		send_to_client_ = 0;

		last_state_ = qs.st;
		set_flag(quest_name + ".__status", qs.st);

		running_quest_state_->index = qi;
		should_send_done_ = false;
	}

	void pc::add_timer(const std::string & name, event_ptr pEvent)
	{
		remove_timer(name);
		timer_map_.insert(make_pair(name, pEvent));
		metin2::log::info( "QUEST add timer {} {:d}", static_cast<const void *>(pEvent.get()), timer_map_.size());
	}

	void pc::remove_timer_not_cancel(const std::string & name)
	{
		timer_map::iterator it = timer_map_.find(name);

		if (it != timer_map_.end())
		{
			metin2::log::info( "QUEST remove with no cancel {}", static_cast<const void *>(it->second.get()));
			timer_map_.erase(it);
		}

		metin2::log::debug( "QUEST timer map size {:d} by RemoveTimerNotCancel", timer_map_.size());
	}

	void pc::remove_timer(const std::string & name)
	{
		timer_map::iterator it = timer_map_.find(name);

		if (it != timer_map_.end())
		{
			metin2::log::info( "QUEST remove timer {}", static_cast<const void *>(it->second.get()));
			cancel_timer_event(&it->second);
			timer_map_.erase(it);
		}

		metin2::log::debug( "QUEST timer map size {:d} by RemoveTimer", timer_map_.size());
	}

	void pc::clear_timer()
	{
		metin2::log::info( "QUEST clear timer {:d}", timer_map_.size());
		timer_map::iterator it = timer_map_.begin();

		while (it != timer_map_.end())
		{
			cancel_timer_event(&it->second);
			++it;
		}

		timer_map_.clear();
	}

	void pc::set_current_quest_start_flag()
	{
		if (!current_quest_begin_flag())
		{
			set_current_quest_begin_flag();
		}
	}

	void pc::set_current_quest_done_flag()
	{
		if (current_quest_begin_flag())
		{
			clear_current_quest_begin_flag();
		}
	}

	void pc::send_quest_info_pakcet()
	{
		assert(send_to_client_);
		assert(running_quest_state_);

		packet_quest_info qi;

		qi.header = HEADER_GC_QUEST_INFO;
		qi.size = sizeof(struct packet_quest_info);
		qi.index = running_quest_state_->index;
		qi.flag = send_to_client_;

		temp_buffer buf;
		buf.write(&qi, sizeof(qi));

		if (send_to_client_ & QUEST_SEND_ISBEGIN)
		{
			uint8_t temp = running_quest_state_->start?1:0;
			buf.write(&temp,1);		
			qi.size+=1;

			metin2::log::debug( "QUEST BeginFlag {:d}", (int)temp);
		}
		if (send_to_client_ & QUEST_SEND_TITLE)
		{
			running_quest_state_->title.reserve(30+1);
			buf.write(running_quest_state_->title.c_str(), 30+1);
			qi.size+=30+1;

			metin2::log::debug( "QUEST Title {}", running_quest_state_->title.c_str());
		}
		if (send_to_client_ & QUEST_SEND_CLOCK_NAME)
		{
			running_quest_state_->clock_name.reserve(16+1);
			buf.write(running_quest_state_->clock_name.c_str(), 16+1);
			qi.size+=16+1;

			metin2::log::debug( "QUEST Clock Name {}", running_quest_state_->clock_name.c_str());
		}
		if (send_to_client_ & QUEST_SEND_CLOCK_VALUE)
		{
			buf.write(&running_quest_state_->clock_value, sizeof(int));
			qi.size+=4;

			metin2::log::debug( "QUEST Clock Value {:d}", running_quest_state_->clock_value);
		}
		if (send_to_client_ & QUEST_SEND_COUNTER_NAME)
		{
			running_quest_state_->counter_name.reserve(16+1);
			buf.write(running_quest_state_->counter_name.c_str(), 16+1);
			qi.size+=16+1;

			metin2::log::debug( "QUEST Counter Name {}", running_quest_state_->counter_name.c_str());
		}
		if (send_to_client_ & QUEST_SEND_COUNTER_VALUE)
		{
			buf.write(&running_quest_state_->counter_value, sizeof(int));
			qi.size+=4;

			metin2::log::debug( "QUEST Counter Value {:d}", running_quest_state_->counter_value);
		}
		if (send_to_client_ & QUEST_SEND_ICON_FILE)
		{
			running_quest_state_->icon_file.reserve(24+1);
			buf.write(running_quest_state_->icon_file.c_str(), 24+1);
			qi.size+=24+1;

			metin2::log::debug( "QUEST Icon File {}", running_quest_state_->icon_file.c_str());
		}

		quest_manager::instance().current_character_ptr()->get_desc()->packet(buf.read_peek(),buf.size());

		send_to_client_ = 0;

	}

	void pc::end_running()
	{
		// unlocked locked npc
		{
			character * npc = quest_manager::instance().current_npc_character_ptr();
			character * ch = quest_manager::instance().current_character_ptr();
			// If there was an npc
			if (npc && !npc->is_pc())
			{
				// If that npc is locked to me
				if (ch->player_id() == npc->quest_npcid())
				{
					npc->set_quest_npcid(0);
					metin2::log::info( "QUEST NPC lock isn't unlocked : pid {:d}", ch->player_id());
					quest_manager::instance().write_running_state_to_syserr();
				}
			}
		}

		// commit data
		if (has_reward())
		{
			save();

			character * ch = quest_manager::instance().current_character_ptr();
			if (ch != nullptr) {
				reward(ch);
				ch->save();
			}
		}
		is_given_reward_ = false;

		if (send_to_client_)
		{
			metin2::log::debug( "QUEST end running {:d}", send_to_client_);
			send_quest_info_pakcet();
		}

		if (running_quest_state_ == nullptr) {
			metin2::log::info( "Entered PC::EndRunning() with invalid running quest state");
			return;
		}
		quest_state * pOldState = running_quest_state_;
		int iNowState = running_quest_state_->st;

		running_quest_state_ = nullptr;

		if (last_state_ != iNowState)
		{
			character * ch = quest_manager::instance().current_character_ptr();
			uint32_t dwQuestIndex = quest_manager::instance().quest_index_by_name(cur_quest_);
			if (ch)
			{
				set_flag(cur_quest_ + ".__status", last_state_);
				quest_manager::instance().leave_state(ch->player_id(), dwQuestIndex, last_state_);
				pOldState->st = iNowState;
				set_flag(cur_quest_ + ".__status", iNowState);
				quest_manager::instance().enter_state(ch->player_id(), dwQuestIndex, iNowState);
				if (flag(cur_quest_ + ".__status") == iNowState)
					quest_manager::instance().letter(ch->player_id(), dwQuestIndex, iNowState);
			}
		}


		do_quest_state_change();
	}

	void pc::do_quest_state_change()
	{
		character * ch = quest_manager::instance().current_character_ptr();

		std::vector<quest_state_change_info> vecQuestStateChange;
		quest_state_change_.swap(vecQuestStateChange);

		for (uint32_t i=0; i<vecQuestStateChange.size(); ++i)
		{
			const quest_state_change_info& rInfo = vecQuestStateChange[i];
			if (rInfo.quest_idx == 0)
				continue;

			uint32_t dwQuestIdx = rInfo.quest_idx;
			quest_info_iterator it = quest_find(dwQuestIdx);
			const std::string stQuestName = quest_manager::instance().quest_name_by_index(dwQuestIdx);

			if (it == quest_end())
			{
				quest_state qs;
				qs.st = 0;

				quest_info_.insert(std::make_pair(dwQuestIdx, qs));
				set_flag(stQuestName + ".__status", 0);

				it = quest_find(dwQuestIdx);
			}

			metin2::log::info( "QUEST change reserved Quest State Change quest {:d} from {:d} to {:d} ({:d} {:d})", dwQuestIdx, rInfo.prev_state, rInfo.next_state, it->second.st, rInfo.prev_state );

			assert(it->second.st == rInfo.prev_state);

			quest_manager::instance().leave_state(ch->player_id(), dwQuestIdx, rInfo.prev_state);
			it->second.st = rInfo.next_state;
			set_flag(stQuestName + ".__status", rInfo.next_state);

			quest_manager::instance().enter_state(ch->player_id(), dwQuestIdx, rInfo.next_state);

			if (flag(stQuestName + ".__status")==rInfo.next_state)
				quest_manager::instance().letter(ch->player_id(), dwQuestIdx, rInfo.next_state);
		}
	}

	void pc::cancel_running()
	{
		// cancel data
		running_quest_state_ = nullptr;
		send_to_client_ = 0;
		should_send_done_ = false;
	}

	void pc::set_send_flag(int idx)
	{
		send_to_client_ |= idx;
	}

	void pc::clear_current_quest_begin_flag()
	{
		set_send_flag(QUEST_SEND_ISBEGIN);
		running_quest_state_->start = false;
	}

	void pc::set_current_quest_begin_flag()
	{
		quest_manager& q = quest_manager::instance();
		int iQuestIndex = q.quest_index_by_name(cur_quest_);
		running_quest_state_->start = true;
		running_quest_state_->index = iQuestIndex;

		set_send_flag(QUEST_SEND_ISBEGIN);
	}

	int pc::current_quest_begin_flag()
	{
		return running_quest_state_?running_quest_state_->index:0;
	}

	void pc::set_current_quest_title(const std::string& title)
	{
		set_send_flag(QUEST_SEND_TITLE);
		running_quest_state_->title = title;
	}

	void pc::set_quest_title(const std::string& quest, const std::string& title)
	{
		quest_info::iterator it = quest_info_.find(quest_manager::instance().quest_index_by_name(quest));

		if (it != quest_info_.end()) 
		{
			quest_state* old = running_quest_state_;
			int old2 = send_to_client_;
			std::string oldquestname = cur_quest_;
			cur_quest_ = quest;
			running_quest_state_ = &it->second;
			send_to_client_ = QUEST_SEND_TITLE;
			running_quest_state_->index = current_quest_begin_flag();

			set_current_quest_title(title);

			send_quest_info_pakcet();

			cur_quest_ = oldquestname;
			running_quest_state_ = old;
			send_to_client_ = old2;
		}
	}

	void pc::set_current_quest_clock_name(const std::string& name)
	{
		set_send_flag(QUEST_SEND_CLOCK_NAME);
		running_quest_state_->clock_name = name;
	}

	void pc::set_current_quest_clock_value(int value)
	{
		set_send_flag(QUEST_SEND_CLOCK_VALUE);
		running_quest_state_->clock_value = value;
	}

	void pc::set_current_quest_counter_name(const std::string& name)
	{
		set_send_flag(QUEST_SEND_COUNTER_NAME);
		running_quest_state_->counter_name = name;
	}

	void pc::set_current_quest_counter_value(int value)
	{
		set_send_flag(QUEST_SEND_COUNTER_VALUE);
		running_quest_state_->counter_value = value;
	}

	void pc::set_current_quest_icon_file(const std::string& icon_file)
	{
		set_send_flag(QUEST_SEND_ICON_FILE);
		running_quest_state_->icon_file = icon_file;
	}

	void pc::save()
	{
		if (flag_save_map_.empty())
			return;

		static std::vector<quest_table> s_table;
		s_table.resize(flag_save_map_.size());

		int i = 0;

		flag_map::iterator it = flag_save_map_.begin();

		while (it != flag_save_map_.end())
		{
			const std::string & stComp = it->first;
			int32_t lValue = it->second;

			++it;

			int iPos = stComp.find(".");

			if (iPos < 0)
			{
				metin2::log::error("quest::PC::Save : cannot find . in FlagMap");
				continue;
			}

			std::string stName;
			stName.assign(stComp, 0, iPos);

			std::string stState;
			stState.assign(stComp, iPos + 1, stComp.length());

			if (stName.length() == 0 || stState.length() == 0)
			{
				metin2::log::error("quest::PC::Save : invalid quest data: {}", stComp.c_str());
				continue;
			}

			metin2::log::debug( "QUEST Save Flag {}, {} {:d} ({:d})", stName.c_str(), stState.c_str(), lValue, i);

			if (stName.length() >= QUEST_NAME_MAX_LEN)
			{
				metin2::log::error("quest::PC::Save : quest name overflow");
				continue;
			}

			if (stState.length() >= QUEST_STATE_MAX_LEN)
			{
				metin2::log::error("quest::PC::Save : quest state overflow");
				continue;
			}

			quest_table & r = s_table[i++];

			r.pid = id_;
			strlcpy(r.name, stName.c_str(), sizeof(r.name));
			strlcpy(r.state, stState.c_str(), sizeof(r.state));
			r.value = lValue;
		}

		if (i > 0)
		{
			metin2::log::debug( "QuestPC::Save {:d}", i);
			db_clientdesc->db_packet_header(HEADER_GD_QUEST_SAVE, 0, sizeof(quest_table) * i);
			db_clientdesc->packet(&s_table[0], sizeof(quest_table) * i);
		}

		flag_save_map_.clear();
	}

	void pc::give_item(const std::string& label, uint32_t dwVnum, int count)
	{
		metin2::log::debug( "QUEST GiveItem {} {:d} {:d}", label.c_str(),dwVnum,count);
		if (!flag(cur_quest_+"."+label))
		{
			reward_data_.push_back(reward_data(reward_data::REWARD_TYPE_ITEM, dwVnum, count));
		}
		else
			is_given_reward_ = true;
	}

	void pc::give_exp(const std::string& label, uint32_t exp)
	{
		metin2::log::debug( "QUEST GiveExp {} {:d}", label.c_str(),exp);

		if (!flag(cur_quest_+"."+label))
		{
			reward_data_.push_back(reward_data(reward_data::REWARD_TYPE_EXP, exp));
		}
		else
			is_given_reward_ = true;
	}

	void pc::reward(character * ch)
	{
		if (is_given_reward_)
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("<퀘스트> 이전에 같은 보상을 받은 적이 있어 다시 받지 않습니다."));
			is_given_reward_ = false;
		}

		for (std::vector<reward_data>::iterator it = reward_data_.begin(); it != reward_data_.end(); ++it)
		{
			switch (it->type)
			{
				case reward_data::REWARD_TYPE_EXP:
					metin2::log::info( "EXP cur {:d} add {:d} next {:d}",ch->get_exp(), it->value1, ch->next_exp());

					if (ch->get_exp() + it->value1 > ch->next_exp())
						ch->point_change(POINT_EXP, ch->next_exp() - 1 - ch->get_exp());
					else
						ch->point_change(POINT_EXP, it->value1);

					break;

				case reward_data::REWARD_TYPE_ITEM:
					ch->auto_give_item(it->value1, it->value2);
					break;

				case reward_data::REWARD_TYPE_NONE:
				default:
					metin2::log::error("Invalid RewardData type");
					break;
			}
		}

		reward_data_.clear();
	}

	void pc::build()
	{
		decltype(flag_map_.begin()) it;
		for (it = flag_map_.begin(); it != flag_map_.end(); ++it)
		{
			if (it->first.size()>9 && it->first.compare(it->first.size()-9,9, ".__status") == 0)
			{
				uint32_t dwQuestIndex = quest_manager::instance().quest_index_by_name(it->first.substr(0, it->first.size()-9));
				int state = it->second;
				quest_state qs;
				qs.st = state;

				quest_info_.insert(std::make_pair(dwQuestIndex, qs));
			}
		}
	}

	void pc::clear_quest(const std::string& quest_name)
	{
		std::string quest_name_with_dot = quest_name + '.';
		for (auto it = flag_map_.begin(); it!= flag_map_.end();)
		{
			auto itNow = it++;
			if (itNow->second != 0 && itNow->first.compare(0, quest_name_with_dot.size(), quest_name_with_dot) == 0)
			{
				set_flag(itNow->first, 0);
			}
		}

		clear_timer();

		quest::pc::quest_info_iterator it = quest_begin();
		unsigned int questindex = quest::quest_manager::instance().quest_index_by_name(quest_name);

		while (it!= quest_end())
		{
			if (it->first == questindex)
			{
				it->second.st = 0;
				break;
			}

			++it;
		}
	}

	void pc::send_flag_list(character * ch)
	{
		for (auto it = flag_map_.begin(); it!= flag_map_.end(); ++it)
		{
			if (it->first.size()>9 && it->first.compare(it->first.size()-9,9, ".__status") == 0)
			{
				const std::string quest_name = it->first.substr(0, it->first.size()-9);
				const char* state_name = quest_manager::instance().quest_state_name(quest_name, it->second);
				ch->chat_packet(CHAT_TYPE_INFO, "%s %s (%d)", quest_name.c_str(), state_name, it->second);
			}
			else
			{
				ch->chat_packet(CHAT_TYPE_INFO, "%s %d", it->first.c_str(), it->second);
			}
		}
	}
}
}
