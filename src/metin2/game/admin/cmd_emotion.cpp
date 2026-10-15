#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/admin/cmd.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/social/marriage.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/social/wedding.h>

#include <metin2/proto/length.h>
#include <metin2/common/utils.h>
#include <metin2/log/log.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <utility>

namespace metin2::game::inline admin {

constexpr int NEED_TARGET = (1 << 0);
constexpr int NEED_PC = (1 << 1);
constexpr int WOMAN_ONLY = (1 << 2);
constexpr int OTHER_SEX_ONLY = (1 << 3);
constexpr int SELF_DISARM = (1 << 4);
constexpr int TARGET_DISARM = (1 << 5);
constexpr int BOTH_DISARM = (SELF_DISARM | TARGET_DISARM);
struct emotion_type_s
{
	const char *	command;
	const char *	command_to_client;
	int32_t	flag;
	float	extra_delay;
} emotion_types[] = {
	{ "키스",	"french_kiss",	NEED_PC | OTHER_SEX_ONLY | BOTH_DISARM,		2.0f },
	{ "뽀뽀",	"kiss",		NEED_PC | OTHER_SEX_ONLY | BOTH_DISARM,		1.5f },
	{ "따귀",	"slap",		NEED_PC | SELF_DISARM,				1.5f },
	{ "박수",	"clap",		0,						1.0f },
	{ "와",		"cheer1",	0,						1.0f },
	{ "만세",	"cheer2",	0,						1.0f },
	
	// DANCE
	{ "댄스1",	"dance1",	0,						1.0f },
	{ "댄스2",	"dance2",	0,						1.0f },
	{ "댄스3",	"dance3",	0,						1.0f },
	{ "댄스4",	"dance4",	0,						1.0f },
	{ "댄스5",	"dance5",	0,						1.0f },
	{ "댄스6",	"dance6",	0,						1.0f },
	// END_OF_DANCE
	{ "축하",	"congratulation",	0,				1.0f	},
	{ "용서",	"forgive",			0,				1.0f	},
	{ "화남",	"angry",			0,				1.0f	},
	{ "유혹",	"attractive",		0,				1.0f	},
	{ "슬픔",	"sad",				0,				1.0f	},
	{ "브끄",	"shy",				0,				1.0f	},
	{ "응원",	"cheerup",			0,				1.0f	},
	{ "질투",	"banter",			0,				1.0f	},
	{ "기쁨",	"joy",				0,				1.0f	},
	{ "\n",	"\n",		0,						0.0f },
	/*
	//{ "키스",		NEED_PC | OTHER_SEX_ONLY | BOTH_DISARM,		MOTION_ACTION_FRENCH_KISS,	 1.0f },
	{ "뽀뽀",		NEED_PC | OTHER_SEX_ONLY | BOTH_DISARM,		MOTION_ACTION_KISS,		 1.0f },
	{ "껴안기",		NEED_PC | OTHER_SEX_ONLY | BOTH_DISARM,		MOTION_ACTION_SHORT_HUG,	 1.0f },
	{ "포옹",		NEED_PC | OTHER_SEX_ONLY | BOTH_DISARM,		MOTION_ACTION_LONG_HUG,		 1.0f },
	{ "어깨동무",	NEED_PC | SELF_DISARM,				MOTION_ACTION_PUT_ARMS_SHOULDER, 0.0f },
	{ "팔짱",		NEED_PC	| WOMAN_ONLY | SELF_DISARM,		MOTION_ACTION_FOLD_ARM,		 0.0f },
	{ "따귀",		NEED_PC | SELF_DISARM,				MOTION_ACTION_SLAP,		 1.5f },

	{ "휘파람",		0,						MOTION_ACTION_CHEER_01,		 0.0f },
	{ "만세",		0,						MOTION_ACTION_CHEER_02,		 0.0f },
	{ "박수",		0,						MOTION_ACTION_CHEER_03,		 0.0f },

	{ "호호",		0,						MOTION_ACTION_LAUGH_01,		 0.0f },
	{ "킥킥",		0,						MOTION_ACTION_LAUGH_02,		 0.0f },
	{ "우하하",		0,						MOTION_ACTION_LAUGH_03,		 0.0f },

	{ "엉엉",		0,						MOTION_ACTION_CRY_01,		 0.0f },
	{ "흑흑",		0,						MOTION_ACTION_CRY_02,		 0.0f },

	{ "인사",		0,						MOTION_ACTION_GREETING_01,	0.0f },
	{ "바이",		0,						MOTION_ACTION_GREETING_02,	0.0f },
	{ "정중인사",	0,						MOTION_ACTION_GREETING_03,	0.0f },

	{ "비난",		0,						MOTION_ACTION_INSULT_01,	0.0f },
	{ "모욕",		SELF_DISARM,					MOTION_ACTION_INSULT_02,	0.0f },
	{ "우웩",		0,						MOTION_ACTION_INSULT_03,	0.0f },

	{ "갸우뚱",		0,						MOTION_ACTION_ETC_01,		0.0f },
	{ "끄덕끄덕",	0,						MOTION_ACTION_ETC_02,		0.0f },
	{ "도리도리",	0,						MOTION_ACTION_ETC_03,		0.0f },
	{ "긁적긁적",	0,						MOTION_ACTION_ETC_04,		0.0f },
	{ "퉤",		0,						MOTION_ACTION_ETC_05,		0.0f },
	{ "뿡",		0,						MOTION_ACTION_ETC_06,		0.0f },
	 */
};


std::set<std::pair<uint32_t, uint32_t> > emotion_set;

ACMD(do_emotion_allow)
{
	if ( ch->get_arena() )
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("대련장에서 사용하실 수 없습니다."));
		return;
	}

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	uint32_t	val = 0; str_to_number(val, arg1);
	emotion_set.insert(std::make_pair(ch->get_vid(), val));
}

bool character_can_emotion(character& rch)
{
	// Can be used on the wedding map.
	if (marriage::wedding_manager::instance().is_wedding_map(rch.map_index()))
		return true;

	// Can be used while wearing the Mask of Passion.
	if (rch.is_equip_unique_item(UNIQUE_ITEM_EMOTION_MASK))
		return true;

	if (rch.is_equip_unique_item(UNIQUE_ITEM_EMOTION_MASK2))
		return true;

	return false;
}

ACMD(do_emotion)
{
	int i;
	{
		if (ch->is_riding())
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("말을 탄 상태에서 감정표현을 할 수 없습니다."));
			return;
		}
	}

	for (i = 0; *emotion_types[i].command != '\n'; ++i)
	{
		if (!strcmp(cmd_info[cmd].command, emotion_types[i].command))
			break;

		if (!strcmp(cmd_info[cmd].command, emotion_types[i].command_to_client))
			break;
	}

	if (*emotion_types[i].command == '\n')
	{
		metin2::log::error("cannot find emotion");
		return;
	}

	if (!character_can_emotion(*ch))
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("열정의 가면을 착용시에만 할 수 있습니다."));
		return;
	}

	if (is_set(emotion_types[i].flag, WOMAN_ONLY) && SEX_MALE==get_sex(ch))
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("여자만 할 수 있습니다."));
		return;
	}

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	character * victim = nullptr;

	if (*arg1)
		victim = ch->find_character_in_view(arg1, is_set(emotion_types[i].flag, NEED_PC));

	if (is_set(emotion_types[i].flag, NEED_TARGET | NEED_PC))
	{
		if (!victim)
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("그런 사람이 없습니다."));
			return;
		}
	}

	if (victim)
	{
		if (!victim->is_pc() || victim == ch)
			return;

		if (victim->is_riding())
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("말을 탄 상대와 감정표현을 할 수 없습니다."));
			return;
		}

		int32_t distance = distance_approx(ch->x() - victim->x(), ch->y() - victim->y());

		if (distance < 10)
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("너무 가까이 있습니다."));
			return;
		}

		if (distance > 500)
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("너무 멀리 있습니다"));
			return;
		}

		if (is_set(emotion_types[i].flag, OTHER_SEX_ONLY))
		{
			if (get_sex(ch)==get_sex(victim))
			{
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("이성간에만 할 수 있습니다."));
				return;
			}
		}

		if (is_set(emotion_types[i].flag, NEED_PC))
		{
			if (emotion_set.find(std::make_pair(victim->get_vid(), ch->get_vid())) == emotion_set.end())
			{
				if (true == marriage::manager::instance().is_married( ch->player_id() ))
				{
					const marriage::marriage* marriageInfo = marriage::manager::instance().get( ch->player_id() );

					const uint32_t other = marriageInfo->other( ch->player_id() );

					if (0 == other || other != victim->player_id())
					{
						ch->chat_packet(CHAT_TYPE_INFO, lc_text("이 행동은 상호동의 하에 가능 합니다."));
						return;
					}
				}
				else
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("이 행동은 상호동의 하에 가능 합니다."));
					return;
				}
			}

			emotion_set.insert(std::make_pair(ch->get_vid(), victim->get_vid()));
		}
	}

	char chatbuf[256+1];
	int len = snprintf(chatbuf, sizeof(chatbuf), "%s %u %u", 
			emotion_types[i].command_to_client,
			(uint32_t) ch->get_vid(), victim ? (uint32_t) victim->get_vid() : 0);

	if (len < 0 || len >= (int) sizeof(chatbuf))
		len = sizeof(chatbuf) - 1;

	++len;  // including the \0 character

	packet_gc_chat pack_chat;
	pack_chat.header = HEADER_GC_CHAT;
	pack_chat.size = sizeof(packet_gc_chat) + len;
	pack_chat.type = CHAT_TYPE_COMMAND;
	pack_chat.id = 0;
	temp_buffer buf;
	buf.write(&pack_chat, sizeof(packet_gc_chat));
	buf.write(chatbuf, len);

	ch->packet_around(buf.read_peek(), buf.size());

	if (victim)
		metin2::log::debug( "ACTION: {} TO {}", emotion_types[i].command, victim->name());
	else
		metin2::log::debug( "ACTION: {}", emotion_types[i].command);
}
}
