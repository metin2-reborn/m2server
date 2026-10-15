#include <metin2/game/characters/character.h>
#include <metin2/game/admin/cmd.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/events/ox_event.h>
#include <metin2/game/quest/questmanager.h>

#include <metin2/proto/length.h>

namespace metin2::game::inline admin {

ACMD(do_oxevent_show_quiz)
{
	ch->chat_packet(CHAT_TYPE_INFO, "===== OX QUIZ LIST =====");
	ox_event_manager::instance().show_quiz_list(ch);
	ch->chat_packet(CHAT_TYPE_INFO, "===== OX QUIZ LIST END =====");
}

ACMD(do_oxevent_log)
{
	if ( ox_event_manager::instance().log_winner() == false )
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("OX이벤트의 나머지 인원을 기록하였습니다."));
	}
	else
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("OX이벤트의 나머지 인원 기록을 실패했습니다."));
	}
}

ACMD(do_oxevent_get_attender)
{
	ch->chat_packet(CHAT_TYPE_INFO, lc_text("현재 남은 참가자수 : %d"), ox_event_manager::instance().attender_count());
}
}
