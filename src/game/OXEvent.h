#pragma once

#include "event.h"
#include "typedef.h"

#include <common/singleton.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace metin2::game {

inline constexpr int OXEVENT_MAP_INDEX = 113;
struct tag_Quiz
{
	char level;
	std::string Quiz;
	bool answer;
};

enum OXEventStatus
{
	OXEVENT_FINISH = 0, // OX event fully finished
	OXEVENT_OPEN = 1,	// OX event started. Entry possible through Euldoji (20012)
	OXEVENT_CLOSE = 2,	// OX event registration closed. Entry through Euldoji (20012) is blocked
	OXEVENT_QUIZ = 3,	// A quiz question has been asked.

	OXEVENT_ERR = 0xff
};

class COXEventManager : public singleton<COXEventManager>
{
	private :
		std::map<uint32_t, uint32_t> m_map_char;
		std::map<uint32_t, uint32_t> m_map_attender;
		std::map<uint32_t, uint32_t> m_map_miss;

		std::vector<std::vector<tag_Quiz> > m_vec_quiz;

		LPEVENT m_timedEvent;

	protected :
		bool CheckAnswer();

		bool EnterAudience(CHARACTER * pChar);
		bool EnterAttender(CHARACTER * pChar);

	public :
		bool Initialize();
		void Destroy();

		OXEventStatus GetStatus();
		void SetStatus(OXEventStatus status);


		bool Enter(CHARACTER * pChar);
		
		bool CloseEvent();

		void ClearQuiz();
		bool AddQuiz(unsigned char level, const char* pszQuestion, bool answer);
		bool ShowQuizList(CHARACTER * pChar);

		bool Quiz(unsigned char level, int timelimit);
		bool GiveItemToAttender(uint32_t dwItemVnum, uint8_t count);

		bool CheckAnswer(bool answer);
		void WarpToAudience();

		bool LogWinner();

		uint32_t GetAttenderCount() { return m_map_attender.size(); }
};
}
