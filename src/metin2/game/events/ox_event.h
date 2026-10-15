#pragma once

#include <metin2/game/app/event.h>
#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace metin2::game::inline events {

inline constexpr int OXEVENT_MAP_INDEX = 113;
struct tag_quiz
{
	char level;
	std::string quiz;
	bool answer;
};

enum ox_event_status
{
	OXEVENT_FINISH = 0, // OX event fully finished
	OXEVENT_OPEN = 1,	// OX event started. Entry possible through Euldoji (20012)
	OXEVENT_CLOSE = 2,	// OX event registration closed. Entry through Euldoji (20012) is blocked
	OXEVENT_QUIZ = 3,	// A quiz question has been asked.

	OXEVENT_ERR = 0xff
};

class ox_event_manager : public singleton<ox_event_manager>
{
	private :
		std::map<uint32_t, uint32_t> map_char_;
		std::map<uint32_t, uint32_t> map_attender_;
		std::map<uint32_t, uint32_t> map_miss_;

		std::vector<std::vector<tag_quiz> > vec_quiz_;

		event_ptr timed_event_;

	protected :
		bool check_answer();

		bool enter_audience(character * ch);
		bool enter_attender(character * ch);

	public :
		bool initialize();
		void destroy();

		ox_event_status status();
		void set_status(ox_event_status status);


		bool enter(character * ch);
		
		bool close_event();

		void clear_quiz();
		bool add_quiz(unsigned char level, const char* question, bool answer);
		bool show_quiz_list(character * ch);

		bool quiz(unsigned char level, int timelimit);
		bool give_item_to_attender(uint32_t item_vnum, uint8_t count);

		bool check_answer(bool answer);
		void warp_to_audience();

		bool log_winner();

		uint32_t attender_count() { return map_attender_.size(); }
};
}
