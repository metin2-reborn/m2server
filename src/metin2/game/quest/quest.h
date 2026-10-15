#pragma once

#include <metin2/game/quest/lua_incl.h>

#include <cstdint>
#include <string>
#include <vector>

namespace metin2::game {

namespace quest
{
	enum
	{
		QUEST_NO_NPC,
		QUEST_ATTR_NPC_START = 100000,
	};

	enum
	{
		QUEST_CLICK_EVENT,
		QUEST_KILL_EVENT,
		QUEST_TIMER_EVENT,
		QUEST_LEVELUP_EVENT,
		QUEST_LOGIN_EVENT,
		QUEST_LOGOUT_EVENT,
		QUEST_BUTTON_EVENT,
		QUEST_INFO_EVENT,
		QUEST_CHAT_EVENT,
		QUEST_ATTR_IN_EVENT,
		QUEST_ATTR_OUT_EVENT,
		QUEST_ITEM_USE_EVENT,
		QUEST_SERVER_TIMER_EVENT,
		QUEST_ENTER_STATE_EVENT,
		QUEST_LEAVE_STATE_EVENT,
		QUEST_LETTER_EVENT,
		QUEST_ITEM_TAKE_EVENT,
		QUEST_TARGET_EVENT,
		QUEST_PARTY_KILL_EVENT,
		QUEST_UNMOUNT_EVENT,
		QUEST_ITEM_PICK_EVENT,
		QUEST_SIG_USE_EVENT,
		QUEST_ITEM_INFORMER_EVENT,
		QUEST_EVENT_COUNT
	};

	enum 
	{
		SUSPEND_STATE_NONE,
		SUSPEND_STATE_PAUSE,
		SUSPEND_STATE_SELECT,
		SUSPEND_STATE_INPUT,
		SUSPEND_STATE_CONFIRM,
		SUSPEND_STATE_SELECT_ITEM,
	};

	enum quest_confirm_type
	{
		CONFIRM_NO,
		CONFIRM_YES,
		CONFIRM_TIMEOUT,
	};

	struct a_state_script_type
	{
		int		size() const { return code.size(); }
		const char*	get_code() const { return &code[0]; }

		std::vector<char> code;
	};

	struct a_arg_script
	{
		//
		// script syntax example
		// 
		// when namespace.func.arg with when_condition begin ...
		//                     ---      --------------
		//                      |             + 
		std::string arg;  // <--+             |
		std::vector<char> when_condition;// <-+
		a_state_script_type script;
		unsigned int quest_index;
		int state_index;

		a_arg_script()
			: quest_index(0), state_index(0)
		{}
	};

	struct quest_state
	{
		lua_State *	co;
		int		ico;
		short int	args;
		uint8_t		suspend_state;
		int		index;
		bool		start;
		int		st;

		std::string	title;
		std::string	clock_name;
		std::string	counter_name;
		int		clock_value;
		int		counter_value;
		std::string	icon_file;

		std::vector<a_arg_script *> chat_scripts;

		quest_state()
			: co(nullptr), ico(0), args(0), suspend_state(SUSPEND_STATE_NONE), index(0), start(false), st(-1),
			clock_value(0), counter_value(0)
		{}
	};
}
}
