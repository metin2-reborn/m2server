#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/ani.h>
#include <metin2/game/text/banword.h>
#include <metin2/game/items/belt_inventory_helper.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/guilds/building.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/config.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/items/dragon_soul.h>
#include <metin2/game/empire/empire_text_convert.h>
#include <metin2/game/app/event.h>
#include <metin2/game/trade/exchange.h>
#include <metin2/game/admin/gm.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/protocol/input.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/social/messenger_manager.h>
#include <metin2/game/characters/motion.h>
#include <metin2/game/events/ox_event.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/quest/quest.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/quest/questpc.h>
#include <metin2/game/trade/safebox.h>
#include <metin2/game/trade/shop.h>
#include <metin2/game/trade/shop_manager.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/text/spam.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/app/utils.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/log/log.h>
#include <metin2/core/main.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <sys/time.h>

namespace metin2::game::inline protocol {

static int deposit_limit()
{
	return (1000*10000); // 10 million
}

void send_block_chat_info(character * ch, int sec)
{
	if (sec <= 0)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("채팅 금지 상태입니다."));
		return;
	}

	int32_t hour = sec / 3600;
	sec -= hour * 3600;

	int32_t min = (sec / 60);
	sec -= min * 60;

	char buf[128+1];

	if (hour > 0 && min > 0)
		snprintf(buf, sizeof(buf), lc_text("%d 시간 %d 분 %d 초 동안 채팅금지 상태입니다"), hour, min, sec);
	else if (hour > 0 && min == 0)
		snprintf(buf, sizeof(buf), lc_text("%d 시간 %d 초 동안 채팅금지 상태입니다"), hour, sec);
	else if (hour == 0 && min > 0)
		snprintf(buf, sizeof(buf), lc_text("%d 분 %d 초 동안 채팅금지 상태입니다"), min, sec);
	else
		snprintf(buf, sizeof(buf), lc_text("%d 초 동안 채팅금지 상태입니다"), sec);

	ch->chat_packet(CHAT_TYPE_INFO, buf);
}

EVENTINFO(spam_event_info)
{
	char host[MAX_HOST_LENGTH+1];

	spam_event_info()
	{
		::memset( host, 0, MAX_HOST_LENGTH+1 );
	}
};

typedef std::unordered_map<std::string, std::pair<unsigned int, event_ptr> > spam_score_of_ip_t;
spam_score_of_ip_t spam_score_of_ip;

EVENTFUNC(block_chat_by_ip_event)
{
	spam_event_info* info = dynamic_cast<spam_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "block_chat_by_ip_event> <Factor> Null pointer" );
		return 0;
	}

	const char * host = info->host;

	spam_score_of_ip_t::iterator it = spam_score_of_ip.find(host);

	if (it != spam_score_of_ip.end())
	{
		it->second.first = 0;
		it->second.second = nullptr;
	}

	return 0;
}

bool spam_block_check(character * ch, const char* const buf, const size_t buflen)
{

	if (ch->level() < metin2::config::game().spam.block_max_level)
	{
		spam_score_of_ip_t::iterator it = spam_score_of_ip.find(ch->get_desc()->host_name());

		if (it == spam_score_of_ip.end())
		{
			spam_score_of_ip.insert(std::make_pair(ch->get_desc()->host_name(), std::make_pair(0, (event_ptr) nullptr)));
			it = spam_score_of_ip.find(ch->get_desc()->host_name());
		}

		if (it->second.second)
		{
			send_block_chat_info(ch, event_time(it->second.second) / metin2::config::server().passes_per_sec);
			return true;
		}

		unsigned int score;
		const char * word = spam_manager::instance().spam_score(buf, buflen, score);

		it->second.first += score;

		if (word)
			metin2::log::info( "SPAM_SCORE: {} text: {} score: {:d} total: {:d} word: {}", ch->name(), buf, score, it->second.first, word);

		if (it->second.first >= metin2::config::game().spam.block_score)
		{
			spam_event_info* info = alloc_event_info<spam_event_info>();
			strlcpy(info->host, ch->get_desc()->host_name(), sizeof(info->host));

			it->second.second = event_create(block_chat_by_ip_event, info, passes_per_sec(metin2::config::game().spam.block_duration));
			metin2::log::info( "SPAM_IP: {} for {:d} seconds", info->host, metin2::config::game().spam.block_duration);

			log_manager::instance().char_log(ch, 0, "SPAM", word);

			send_block_chat_info(ch, event_time(it->second.second) / metin2::config::server().passes_per_sec);

			return true;
		}
	}

	return false;
}

enum
{
	TEXT_TAG_PLAIN,
	TEXT_TAG_TAG, // ||
	TEXT_TAG_COLOR, // |cffffffff
	TEXT_TAG_HYPERLINK_START, // |H
	TEXT_TAG_HYPERLINK_END, // |h ex) |Hitem:1234:1:1:1|h
};

int get_text_tag(const char * src, int maxLen, int & tagLen, std::string & extraInfo)
{
	tagLen = 1;

	if (maxLen < 2 || *src != '|')
		return TEXT_TAG_PLAIN;

	const char * cur = ++src;

	if (*cur == '|') // || is displayed as |.
	{
		tagLen = 2;
		return TEXT_TAG_TAG;
	}
	else if (*cur == 'c') // color |cffffffffblahblah|r
	{
		tagLen = 2;
		return TEXT_TAG_COLOR;
	}
	else if (*cur == 'H') // hyperlink |Hitem:10000:0:0:0:0|h[name]|h
	{
		tagLen = 2;
		return TEXT_TAG_HYPERLINK_START;
	}
	else if (*cur == 'h') // end of hyperlink
	{
		tagLen = 2;
		return TEXT_TAG_HYPERLINK_END;
	}

	return TEXT_TAG_PLAIN;
}

void get_text_tag_info(const char * src, int src_len, int & hyperlinks, bool & colored)
{
	colored = false;
	hyperlinks = 0;

	int len;
	std::string extraInfo;

	for (int i = 0; i < src_len;)
	{
		int tag = get_text_tag(&src[i], src_len - i, len, extraInfo);

		if (tag == TEXT_TAG_HYPERLINK_START)
			++hyperlinks;

		if (tag == TEXT_TAG_COLOR)
			colored = true;

		i += len;
	}
}

int process_text_tag(character * ch, const char * c_pszText, size_t len)
{
	//2012.05.17 Kim Yong-uk
	//0 : used normally
	//1 : not enough Diamond Sutras
	//2 : has Diamond Sutra, but it is in use in a private shop
	//3 : trading
	//4 : error
	int hyperlinks;
	bool colored;

	get_text_tag_info(c_pszText, len, hyperlinks, colored);

	if (colored == true && hyperlinks == 0)
		return 4;

	if (ch->get_exchange())
	{
		if (hyperlinks == 0)
			return 0;
		else
			return 3;
	}

	int nPrismCount = ch->count_specify_item(ITEM_PRISM);

	if (nPrismCount < hyperlinks)
		return 1;

	if (!ch->my_shop())
	{
		ch->remove_specify_item(ITEM_PRISM, hyperlinks);
		return 0;
	} else
	{
		int sellingNumber = ch->my_shop()->number_by_vnum(ITEM_PRISM);
		if(nPrismCount - sellingNumber < hyperlinks)
		{
			return 2;
		} else
		{
			ch->remove_specify_item(ITEM_PRISM, hyperlinks);
			return 0;
		}
	}

	return 4;
}

int input_main::whisper(character * ch, const char * data, size_t uiBytes)
{
	const packet_cg_whisper* pinfo = reinterpret_cast<const packet_cg_whisper*>(data);

	if (uiBytes < pinfo->size)
		return -1;

	int iExtraLen = pinfo->size - sizeof(packet_cg_whisper);

	if (iExtraLen < 0)
	{
		metin2::log::error("invalid packet length (len {:d} size {:d} buffer {:d})", iExtraLen, pinfo->size, uiBytes);
		ch->get_desc()->set_phase(PHASE_CLOSE);
		return -1;
	}

	if (ch->last_pm_pulse() < thecore_pulse())
		ch->clear_pm_counter();

	if (ch->pm_counter() > 3 && ch->last_pm_pulse() > thecore_pulse())
	{
		ch->get_desc()->set_phase(PHASE_CLOSE);
		return -1;
	}

	if (ch->find_affect(AFFECT_BLOCK_CHAT))
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("채팅 금지 상태입니다."));
		return (iExtraLen);
	}

	char szNameTo[CHARACTER_NAME_MAX_LEN + 1];
	copy_packet_string(szNameTo, sizeof(szNameTo), pinfo->name_to, sizeof(pinfo->name_to));

	character * pkChr = character_manager::instance().find_pc(szNameTo);

	if (pkChr == ch)
		return (iExtraLen);

	ch->increase_pm_counter();
	ch->set_last_pm_pulse();

	desc * pkDesc = nullptr;

	uint8_t bOpponentEmpire = 0;

	if (metin2::config::test_server())
	{
		if (!pkChr)
			metin2::log::info( "Whisper to {}({}) from {}", "Null", szNameTo, ch->name());
		else
			metin2::log::info( "Whisper to {}({}) from {}", pkChr->name(), szNameTo, ch->name());
	}

	if (ch->is_block_mode(BLOCK_WHISPER))
	{
		if (ch->get_desc())
		{
			packet_gc_whisper pack;
			pack.header = HEADER_GC_WHISPER;
			pack.type = WHISPER_TYPE_SENDER_BLOCKED;
			pack.size = sizeof(packet_gc_whisper);
			strlcpy(pack.name_from, szNameTo, sizeof(pack.name_from));
			ch->get_desc()->packet(&pack, sizeof(pack));
		}
		return iExtraLen;
	}

	if (!pkChr)
	{
		cci * pkCCI = p2_p_manager::instance().find(szNameTo);

		if (pkCCI)
		{
			pkDesc = pkCCI->desc;
			pkDesc->set_relay(szNameTo);
			bOpponentEmpire = pkCCI->empire;

			if (metin2::config::test_server())
				metin2::log::info( "Whisper to {} from {} (Channel {:d} Mapindex {:d})", "Null", ch->name(), pkCCI->channel, pkCCI->map_index);
		}
	}
	else
	{
		pkDesc = pkChr->get_desc();
		bOpponentEmpire = pkChr->empire();
	}

	if (!pkDesc)
	{
		if (ch->get_desc())
		{
			packet_gc_whisper pack;

			pack.header = HEADER_GC_WHISPER;
			pack.type = WHISPER_TYPE_NOT_EXIST;
			pack.size = sizeof(packet_gc_whisper);
			strlcpy(pack.name_from, szNameTo, sizeof(pack.name_from));
			ch->get_desc()->packet(&pack, sizeof(packet_gc_whisper));
			metin2::log::info( "WHISPER: no player");
		}
	}
	else
	{
		if (ch->is_block_mode(BLOCK_WHISPER))
		{
			if (ch->get_desc())
			{
				packet_gc_whisper pack;
				pack.header = HEADER_GC_WHISPER;
				pack.type = WHISPER_TYPE_SENDER_BLOCKED;
				pack.size = sizeof(packet_gc_whisper);
				strlcpy(pack.name_from, szNameTo, sizeof(pack.name_from));
				ch->get_desc()->packet(&pack, sizeof(pack));
			}
		}
		else if (pkChr && pkChr->is_block_mode(BLOCK_WHISPER))
		{
			if (ch->get_desc())
			{
				packet_gc_whisper pack;
				pack.header = HEADER_GC_WHISPER;
				pack.type = WHISPER_TYPE_TARGET_BLOCKED;
				pack.size = sizeof(packet_gc_whisper);
				strlcpy(pack.name_from, szNameTo, sizeof(pack.name_from));
				ch->get_desc()->packet(&pack, sizeof(pack));
			}
		}
		else
		{
			uint8_t bType = WHISPER_TYPE_NORMAL;

			char buf[CHAT_MAX_LEN + 1];
			copy_packet_string(buf, sizeof(buf), data + sizeof(packet_cg_whisper), iExtraLen);
			const size_t buflen = strlen(buf);

			if (true == spam_block_check(ch, buf, buflen))
			{
				if (!pkChr)
				{
					cci * pkCCI = p2_p_manager::instance().find(szNameTo);

					if (pkCCI)
					{
						pkDesc->set_relay("");
					}
				}
				return iExtraLen;
			}

			banword_manager::instance().convert_string(buf, buflen);

			if (metin2::config::game().empire_whisper)
				if (!ch->is_equip_unique_group(UNIQUE_GROUP_RING_OF_LANGUAGE))
					if (!(pkChr && pkChr->is_equip_unique_group(UNIQUE_GROUP_RING_OF_LANGUAGE)))
						if (bOpponentEmpire != ch->empire() && ch->empire() && bOpponentEmpire // if the empires differ
								&& ch->gm_level() == GM_PLAYER && gm_get_level(szNameTo) == GM_PLAYER) // and both are normal players
							// we only know the name, so use gm_get_level
						{
							if (!pkChr)
							{
								// They are on another server, so only show the empire. The upper 4 bits of bType hold the empire number.
								bType = ch->empire() << 4;
							}
							else
							{
								convert_empire_text(ch->empire(), buf, buflen, 10 + 2 * pkChr->skill_power(SKILL_LANGUAGE1 + ch->empire() - 1)/*conversion probability*/);
							}
						}

			int processReturn = process_text_tag(ch, buf, buflen);
			if (0!=processReturn)
			{
				if (ch->get_desc())
				{
					item_table * pTable = item_manager::instance().table(ITEM_PRISM);

					if (pTable)
					{
						char buf[128];
						int len;
						if (3==processReturn) //trading
							len = snprintf(buf, sizeof(buf), lc_text("다른 거래중(창고,교환,상점)에는 개인상점을 사용할 수 없습니다."), pTable->locale_name);
						else
							len = snprintf(buf, sizeof(buf), lc_text("%s이 필요합니다."), pTable->locale_name);

						if (len < 0 || len >= (int) sizeof(buf))
							len = sizeof(buf) - 1;

						++len;  // includes the \0 character

						packet_gc_whisper pack;

						pack.header = HEADER_GC_WHISPER;
						pack.type = WHISPER_TYPE_ERROR;
						pack.size = sizeof(packet_gc_whisper) + len;
						strlcpy(pack.name_from, szNameTo, sizeof(pack.name_from));

						ch->get_desc()->buffered_packet(&pack, sizeof(pack));
						ch->get_desc()->packet(buf, len);

						metin2::log::info( "WHISPER: not enough {}: char: {}", pTable->locale_name, ch->name());
					}
				}

				// may be in relay state, so release the relay.
				pkDesc->set_relay("");
				return (iExtraLen);
			}

			if (ch->is_gm())
				bType = (bType & 0xF0) | WHISPER_TYPE_GM;

			if (buflen > 0)
			{
				packet_gc_whisper pack;

				pack.header = HEADER_GC_WHISPER;
				pack.size = sizeof(packet_gc_whisper) + buflen;
				pack.type = bType;
				strlcpy(pack.name_from, ch->name(), sizeof(pack.name_from));

				// The reason we write to the buffer instead of desc->BufferedPacket is 
				// that the packet may be encapsulated by P2P relay.
				temp_buffer tmpbuf;

				tmpbuf.write(&pack, sizeof(pack));
				tmpbuf.write(buf, buflen);

				pkDesc->packet(tmpbuf.read_peek(), tmpbuf.size());

			}
		}
	}
	if(pkDesc)
		pkDesc->set_relay("");

	return (iExtraLen);
}

struct raw_packet_to_character_func
{
	const void * buf;
	int	buf_len;

	raw_packet_to_character_func(const void * buf_value, int buf_len_value) : buf(buf_value), buf_len(buf_len_value)
	{
	}

	void operator () (character * c)
	{
		if (!c->get_desc())
			return;

		c->get_desc()->packet(buf, buf_len);
	}
};

struct f_empire_chat_packet
{
	packet_gc_chat& p;
	const char* orig_msg;
	int orig_len;
	char converted_msg[CHAT_MAX_LEN+1];

	uint8_t empire;
	int map_index;
	int namelen;

	f_empire_chat_packet(packet_gc_chat& p, const char* chat_msg, int len, uint8_t bEmpire, int iMapIndex, int iNameLen)
		: p(p), orig_msg(chat_msg), orig_len(len), empire(bEmpire), map_index(iMapIndex), namelen(iNameLen)
	{
		memset( converted_msg, 0, sizeof(converted_msg) );
	}

	void operator () (desc * d)
	{
		if (!d->get_character())
			return;

		if (d->get_character()->map_index() != map_index)
			return;

		d->buffered_packet(&p, sizeof(packet_gc_chat));

		if (d->empire() == empire ||
			empire == 0 ||
			d->get_character()->gm_level() > GM_PLAYER ||
			d->get_character()->is_equip_unique_group(UNIQUE_GROUP_RING_OF_LANGUAGE))
		{
			d->packet(orig_msg, orig_len);
		}
		else
		{
			// skill levels differ per person, so this must be done every time
			utf8_strlcpy(converted_msg, orig_msg, sizeof(converted_msg));
			size_t len = strlen(converted_msg);

			convert_empire_text(empire, converted_msg + namelen, len - namelen, 10 + 2 * d->get_character()->skill_power(SKILL_LANGUAGE1 + empire - 1));
			d->packet(converted_msg, orig_len);
		}
	}
};

int input_main::chat(character * ch, const char * data, size_t uiBytes)
{
	const packet_cg_chat* pinfo = reinterpret_cast<const packet_cg_chat*>(data);

	if (uiBytes < pinfo->size)
		return -1;

	const int iExtraLen = pinfo->size - sizeof(packet_cg_chat);

	if (iExtraLen < 0)
	{
		metin2::log::error("invalid packet length (len {:d} size {:d} buffer {:d})", iExtraLen, pinfo->size, uiBytes);
		ch->get_desc()->set_phase(PHASE_CLOSE);
		return -1;
	}

	char buf[CHAT_MAX_LEN - (CHARACTER_NAME_MAX_LEN + 3) + 1];
	copy_packet_string(buf, sizeof(buf), data + sizeof(packet_cg_chat), iExtraLen);
	const size_t buflen = strlen(buf);

	if (buflen > 1 && *buf == '/')
	{
		interpret_command(ch, buf + 1, buflen - 1);
		return iExtraLen;
	}

	if (ch->increase_chat_counter() >= 10)
	{
		if (ch->chat_counter() == 10)
		{
			metin2::log::info( "CHAT_HACK: {}", ch->name());
			ch->get_desc()->delayed_disconnect(5);
		}

		return iExtraLen;
	}

	// handle the chat-ban affect
	const affect* pAffect = ch->find_affect(AFFECT_BLOCK_CHAT);

	if (pAffect != nullptr)
	{
		send_block_chat_info(ch, pAffect->duration);
		return iExtraLen;
	}

	if (true == spam_block_check(ch, buf, buflen))
	{
		return iExtraLen;
	}

	char chatbuf[CHAT_MAX_LEN + 1];
	int len = snprintf(chatbuf, sizeof(chatbuf), "%s : %s", ch->name(), buf);

	if (CHAT_TYPE_SHOUT == pinfo->type)
	{
		log_manager::instance().shout_log(metin2::config::server().channel, ch->empire(), chatbuf); // unfiltered, for moderation
	}

	// filter the text and rebuild the line (filtering only buf after formatting had no effect)
	banword_manager::instance().convert_string(buf, buflen);
	len = snprintf(chatbuf, sizeof(chatbuf), "%s : %s", ch->name(), buf);

	if (len < 0 || len >= (int) sizeof(chatbuf))
		len = sizeof(chatbuf) - 1;

	int processReturn = process_text_tag(ch, chatbuf, len);
	if (0!=processReturn)
	{
		const item_table* pTable = item_manager::instance().table(ITEM_PRISM);

		if (nullptr != pTable)
		{
			if (3==processReturn) //trading
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("다른 거래중(창고,교환,상점)에는 개인상점을 사용할 수 없습니다."), pTable->locale_name);
			else
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s이 필요합니다."), pTable->locale_name);

		}

		return iExtraLen;
	}

	if (pinfo->type == CHAT_TYPE_SHOUT)
	{
		const int SHOUT_LIMIT_LEVEL = 15;

		if (ch->level() < SHOUT_LIMIT_LEVEL)
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("외치기는 레벨 %d 이상만 사용 가능 합니다."), SHOUT_LIMIT_LEVEL);
			return (iExtraLen);
		}

		if (thecore_heart->pulse - (int) ch->last_shout_pulse() < metin2::config::server().passes_per_sec * 15)
			return (iExtraLen);

		ch->set_last_shout_pulse(thecore_heart->pulse);

		packet_gg_shout p;

		p.header = HEADER_GG_SHOUT;
		p.empire = ch->empire();
		utf8_strlcpy(p.text, chatbuf, sizeof(p.text));

		p2_p_manager::instance().send(&p, sizeof(packet_gg_shout));

		send_shout(chatbuf, ch->empire());

		return (iExtraLen);
	}

	packet_gc_chat pack_chat;

	pack_chat.header = HEADER_GC_CHAT;
	pack_chat.size = sizeof(packet_gc_chat) + len;
	pack_chat.type = pinfo->type;
	pack_chat.id = ch->get_vid();

	switch (pinfo->type)
	{
		case CHAT_TYPE_TALKING:
			{
				const desc_manager::desc_set & c_ref_set = desc_manager::instance().client_set();

				std::for_each(c_ref_set.begin(), c_ref_set.end(), 
						f_empire_chat_packet(pack_chat,
							chatbuf,
							len, 
							(ch->gm_level() > GM_PLAYER ||
							 ch->is_equip_unique_group(UNIQUE_GROUP_RING_OF_LANGUAGE)) ? 0 : ch->empire(), 
							ch->map_index(), strlen(ch->name())));
			}
			break;

		case CHAT_TYPE_PARTY:
			{
				if (!ch->get_party())
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("파티 중이 아닙니다."));
				else
				{
					temp_buffer tbuf;

					tbuf.write(&pack_chat, sizeof(pack_chat));
					tbuf.write(chatbuf, len);

					raw_packet_to_character_func f(tbuf.read_peek(), tbuf.size());
					ch->get_party()->for_each_online_member(f);
				}
			}
			break;

		case CHAT_TYPE_GUILD:
			{
				if (!ch->get_guild())
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("길드에 가입하지 않았습니다."));
				else
					ch->get_guild()->chat(chatbuf);
			}
			break;

		default:
			metin2::log::error("Unknown chat type {:d}", pinfo->type);
			break;
	}

	return (iExtraLen);
}

void input_main::item_use(character * ch, const char * data)
{
	ch->use_item(((struct packet_cg_item_use *) data)->cell);
}

void input_main::item_to_item(character * ch, const char * pcData)
{
	packet_cg_item_use_to_item * p = (packet_cg_item_use_to_item *) pcData;
	if (ch)
		ch->use_item(p->cell, p->target_cell);
}

void input_main::item_drop(character * ch, const char * data)
{
	struct packet_cg_item_drop * pinfo = (struct packet_cg_item_drop *) data;

	//MONARCH_LIMIT
	//END_MONARCH_LIMIT
	if (!ch)
		return;

	// if elk (gold) is greater than 0, the gold is being dropped.
	if (pinfo->gold > 0)
		ch->drop_gold(pinfo->gold);
	else
		ch->drop_item(pinfo->cell);
}

void input_main::item_drop2(character * ch, const char * data)
{
	//MONARCH_LIMIT
	//END_MONARCH_LIMIT

	packet_cg_item_drop2 * pinfo = (packet_cg_item_drop2 *) data;

	// if elk (gold) is greater than 0, the gold is being dropped.

	if (!ch)
		return;
	if (pinfo->gold > 0)
		ch->drop_gold(pinfo->gold);
	else
		ch->drop_item(pinfo->cell, pinfo->count);
}

void input_main::item_move(character * ch, const char * data)
{
	struct packet_cg_item_move * pinfo = (struct packet_cg_item_move *) data;

	if (ch)
		ch->move_item(pinfo->cell, pinfo->cell_to, pinfo->count);
}

void input_main::item_pickup(character * ch, const char * data)
{
	struct packet_cg_item_pickup * pinfo = (struct packet_cg_item_pickup*) data;
	if (ch)
		ch->pickup_item(pinfo->vid);
}

void input_main::quickslot_add(character * ch, const char * data)
{
	struct packet_cg_quickslot_add * pinfo = (struct packet_cg_quickslot_add *) data;
	ch->set_quickslot(pinfo->pos, pinfo->slot);
}

void input_main::quickslot_delete(character * ch, const char * data)
{
	struct packet_cg_quickslot_del * pinfo = (struct packet_cg_quickslot_del *) data;
	ch->del_quickslot(pinfo->pos);
}

void input_main::quickslot_swap(character * ch, const char * data)
{
	struct packet_cg_quickslot_swap * pinfo = (struct packet_cg_quickslot_swap *) data;
	ch->swap_quickslot(pinfo->pos, pinfo->change_pos);
}

int input_main::messenger(character * ch, const char* c_pData, size_t uiBytes)
{
	packet_cg_messenger* p = (packet_cg_messenger*) c_pData;

	if (uiBytes < sizeof(packet_cg_messenger))
		return -1;

	c_pData += sizeof(packet_cg_messenger);
	uiBytes -= sizeof(packet_cg_messenger);

	switch (p->subheader)
	{
		case MESSENGER_SUBHEADER_CG_ADD_BY_VID:
			{
				if (uiBytes < sizeof(packet_cg_messenger_add_by_vid))
					return -1;

				packet_cg_messenger_add_by_vid * p2 = (packet_cg_messenger_add_by_vid *) c_pData;
				character * ch_companion = character_manager::instance().find(p2->vid);

				if (!ch_companion)
					return sizeof(packet_cg_messenger_add_by_vid);

				if (ch->is_observer_mode())
					return sizeof(packet_cg_messenger_add_by_vid);

				if (ch_companion->is_block_mode(BLOCK_MESSENGER_INVITE))
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("상대방이 메신져 추가 거부 상태입니다."));
					return sizeof(packet_cg_messenger_add_by_vid);
				}

				desc * d = ch_companion->get_desc();

				if (!d)
					return sizeof(packet_cg_messenger_add_by_vid);

				if (ch->gm_level() == GM_PLAYER && ch_companion->gm_level() != GM_PLAYER)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<메신져> 운영자는 메신져에 추가할 수 없습니다."));
					return sizeof(packet_cg_messenger_add_by_vid);
				}

				if (ch->get_desc() == d) // cannot add yourself.
					return sizeof(packet_cg_messenger_add_by_vid);

				messenger_manager::instance().request_to_add(ch, ch_companion);
			}
			return sizeof(packet_cg_messenger_add_by_vid);

		case MESSENGER_SUBHEADER_CG_ADD_BY_NAME:
			{
				if (uiBytes < CHARACTER_NAME_MAX_LEN)
					return -1;

				const std::string name(c_pData, strnlen(c_pData, CHARACTER_NAME_MAX_LEN));

				if (ch->gm_level() == GM_PLAYER && gm_get_level(name.c_str()) != GM_PLAYER)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<메신져> 운영자는 메신져에 추가할 수 없습니다."));
					return CHARACTER_NAME_MAX_LEN;
				}

				character * tch = character_manager::instance().find_pc(name.c_str());

				if (!tch)
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s 님은 접속되 있지 않습니다."), name.c_str());
				else
				{
					if (tch == ch) // cannot add yourself.
						return CHARACTER_NAME_MAX_LEN;

					if (tch->is_block_mode(BLOCK_MESSENGER_INVITE) == true)
					{
						ch->chat_packet(CHAT_TYPE_INFO, lc_text("상대방이 메신져 추가 거부 상태입니다."));
					}
					else
					{
						// changed since the messenger became per-character
						messenger_manager::instance().request_to_add(ch, tch);
					}
				}
			}
			return CHARACTER_NAME_MAX_LEN;

		case MESSENGER_SUBHEADER_CG_REMOVE:
			{
				if (uiBytes < CHARACTER_NAME_MAX_LEN)
					return -1;

				const std::string char_name(c_pData, strnlen(c_pData, CHARACTER_NAME_MAX_LEN));
				messenger_manager::instance().remove_from_list(ch->name(), char_name);
			}
			return CHARACTER_NAME_MAX_LEN;

		default:
			metin2::log::error("CInputMain::Messenger : Unknown subheader {:d} : {}", p->subheader, ch->name());
			break;
	}

	return 0;
}

int input_main::shop(character * ch, const char * data, size_t uiBytes)
{
	packet_cg_shop * p = (packet_cg_shop *) data;

	if (uiBytes < sizeof(packet_cg_shop))
		return -1;

	if (metin2::config::test_server())
		metin2::log::info( "CInputMain::Shop() ==> SubHeader {:d}", p->subheader);

	const char * c_pData = data + sizeof(packet_cg_shop);
	uiBytes -= sizeof(packet_cg_shop);

	switch (p->subheader)
	{
		case SHOP_SUBHEADER_CG_END:
			metin2::log::debug( "INPUT: {} SHOP: END", ch->name());
			shop_manager::instance().stop_shopping(ch);
			return 0;

		case SHOP_SUBHEADER_CG_BUY:
			{
				if (uiBytes < sizeof(uint8_t) + sizeof(uint8_t))
					return -1;

				uint8_t bPos = *(c_pData + 1);
				metin2::log::debug( "INPUT: {} SHOP: BUY {:d}", ch->name(), bPos);
				shop_manager::instance().buy(ch, bPos);
				return (sizeof(uint8_t) + sizeof(uint8_t));
			}

		case SHOP_SUBHEADER_CG_SELL:
			{
				if (uiBytes < sizeof(uint8_t))
					return -1;

				uint8_t pos = *c_pData;

				metin2::log::info( "INPUT: {} SHOP: SELL", ch->name());
				shop_manager::instance().sell(ch, pos);
				return sizeof(uint8_t);
			}

		case SHOP_SUBHEADER_CG_SELL2:
			{
				if (uiBytes < sizeof(uint8_t) + sizeof(uint8_t))
					return -1;

				uint8_t pos = *(c_pData++);
				uint8_t count = *(c_pData);

				metin2::log::info( "INPUT: {} SHOP: SELL2", ch->name());
				shop_manager::instance().sell(ch, pos, count);
				return sizeof(uint8_t) + sizeof(uint8_t);
			}

		default:
			metin2::log::error("CInputMain::Shop : Unknown subheader {:d} : {}", p->subheader, ch->name());
			break;
	}

	return 0;
}

void input_main::on_click(character * ch, const char * data)
{
	struct packet_cg_on_click *	pinfo = (struct packet_cg_on_click *) data;
	character *			victim;

	if ((victim = character_manager::instance().find(pinfo->vid)))
		victim->on_click(ch);
	else if (metin2::config::test_server())
	{
		metin2::log::error("CInputMain::OnClick {}.Click.NOT_EXIST_VID[{:d}]", ch->name(), pinfo->vid);
	}
}

void input_main::exchange(character * ch, const char * data)
{
	struct packet_cg_exchange * pinfo = (struct packet_cg_exchange *) data;
	character *	to_ch = nullptr;

	if (!ch->can_handle_item())
		return;

	int iPulse = thecore_pulse(); 

	if ((to_ch = character_manager::instance().find(pinfo->arg1)))
	{
		if (iPulse - to_ch->safebox_load_time() < passes_per_sec(portal_limit_time))
		{
			to_ch->chat_packet(CHAT_TYPE_INFO, lc_text("거래 후 %d초 이내에 창고를 열수 없습니다."), portal_limit_time);
			return;
		}

		if( true == to_ch->is_dead() )
		{
			return;
		}
	}

	metin2::log::info( "CInputMain()::Exchange()  SubHeader {:d} ", pinfo->sub_header);

	if (iPulse - ch->safebox_load_time() < passes_per_sec(portal_limit_time))
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("거래 후 %d초 이내에 창고를 열수 없습니다."), portal_limit_time);
		return;
	}

	switch (pinfo->sub_header)
	{
		case EXCHANGE_SUBHEADER_CG_START:	// arg1 == vid of target character
			if (!ch->get_exchange())
			{
				if ((to_ch = character_manager::instance().find(pinfo->arg1)))
				{
					//MONARCH_LIMIT
					if (iPulse - ch->safebox_load_time() < passes_per_sec(portal_limit_time))
					{
						ch->chat_packet(CHAT_TYPE_INFO, lc_text("창고를 연후 %d초 이내에는 거래를 할수 없습니다."), portal_limit_time);

						if (metin2::config::test_server())
							ch->chat_packet(CHAT_TYPE_INFO, "[TestOnly][Safebox]Pulse %d LoadTime %d PASS %d", iPulse, ch->safebox_load_time(), passes_per_sec(portal_limit_time));
						return; 
					}

					if (iPulse - to_ch->safebox_load_time() < passes_per_sec(portal_limit_time))
					{
						to_ch->chat_packet(CHAT_TYPE_INFO, lc_text("창고를 연후 %d초 이내에는 거래를 할수 없습니다."), portal_limit_time);

						if (metin2::config::test_server())
							to_ch->chat_packet(CHAT_TYPE_INFO, "[TestOnly][Safebox]Pulse %d LoadTime %d PASS %d", iPulse, to_ch->safebox_load_time(), passes_per_sec(portal_limit_time));
						return; 
					}

					if (ch->gold() >= GOLD_MAX)
					{	
						ch->chat_packet(CHAT_TYPE_INFO, lc_text("액수가 20억 냥을 초과하여 거래를 할수가 없습니다.."));

						metin2::log::error("[OVERFLOG_GOLD] START ({:d}) id {:d} name {} ", ch->gold(), ch->player_id(), ch->name());
						return;
					}

					if (to_ch->is_pc())
					{
						if (quest::quest_manager::instance().give_item_to_pc(ch->player_id(), to_ch))
						{
							metin2::log::info( "Exchange canceled by quest {} {}", ch->name(), to_ch->name());
							return;
						}
					}

					if (ch->my_shop() || ch->is_open_safebox() || ch->shop_owner() || ch->is_cube_open())
					{
						ch->chat_packet(CHAT_TYPE_INFO, lc_text("다른 거래중일경우 개인상점을 열수가 없습니다."));
						return;
					}

					ch->exchange_start(to_ch);
				}
			}
			break;

		case EXCHANGE_SUBHEADER_CG_ITEM_ADD:	// arg1 == position of item, arg2 == position in exchange window
			if (ch->get_exchange())
			{
				if (ch->get_exchange()->company()->accept_status() != true)
					ch->get_exchange()->add_item(pinfo->pos, pinfo->arg2);
			}
			break;

		case EXCHANGE_SUBHEADER_CG_ITEM_DEL:	// arg1 == position of item
			if (ch->get_exchange())
			{
				if (ch->get_exchange()->company()->accept_status() != true)
					ch->get_exchange()->remove_item(pinfo->arg1);
			}
			break;

		case EXCHANGE_SUBHEADER_CG_ELK_ADD:	// arg1 == amount of gold
			if (ch->get_exchange())
			{
				const int64_t nTotalGold = static_cast<int64_t>(ch->get_exchange()->company()->owner()->gold()) + static_cast<int64_t>(pinfo->arg1);

				if (GOLD_MAX <= nTotalGold)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("상대방의 총금액이 20억 냥을 초과하여 거래를 할수가 없습니다.."));

					metin2::log::error("[OVERFLOW_GOLD] ELK_ADD ({:d}) id {:d} name {} ",
							ch->get_exchange()->company()->owner()->gold(),
							ch->get_exchange()->company()->owner()->player_id(),
						   	ch->get_exchange()->company()->owner()->name());

					return;
				}

				if (ch->get_exchange()->company()->accept_status() != true)
					ch->get_exchange()->add_gold(pinfo->arg1);
			}
			break;

		case EXCHANGE_SUBHEADER_CG_ACCEPT:	// arg1 == not used
			if (ch->get_exchange())
			{
				metin2::log::info( "CInputMain()::Exchange() ==> ACCEPT "); 
				ch->get_exchange()->accept(true);
			}

			break;

		case EXCHANGE_SUBHEADER_CG_CANCEL:	// arg1 == not used
			if (ch->get_exchange())
				ch->get_exchange()->cancel();
			break;
	}
}

void input_main::position(character * ch, const char * data)
{
	struct packet_cg_position * pinfo = (struct packet_cg_position *) data;

	switch (pinfo->position)
	{
		case POSITION_GENERAL:
			ch->standup();
			break;

		case POSITION_SITTING_CHAIR:
			ch->sitdown(0);
			break;

		case POSITION_SITTING_GROUND:
			ch->sitdown(1);
			break;
	}
}

static const int combo_sequence_by_skill_level[3][8] = 
{
	// 0   1   2   3   4   5   6   7
	{ 14, 15, 16, 17,  0,  0,  0,  0 },
	{ 14, 15, 16, 18, 20,  0,  0,  0 },
	{ 14, 15, 16, 18, 19, 17,  0,  0 },
};

constexpr int COMBO_HACK_ALLOWABLE_MS = 100; // [2013 09 11 CYH]
uint32_t clac_valid_combo_interval( character * ch, uint8_t bArg )
{
	int nInterval = 300;
	float fAdjustNum = 1.5f; // to prevent normal users from being caught by the speed hack check. 2013.09.10 CYH

	if( !ch )
	{
		metin2::log::error( "ClacValidComboInterval() ch is NULL");
		return nInterval;
	}	

	if( bArg == 13 )
	{
		float normalAttackDuration = motion_manager::instance().normal_attack_duration(ch->race_num());
		nInterval = (int) (normalAttackDuration / (((float) ch->point(POINT_ATT_SPEED) / 100.f) * 900.f) + fAdjustNum );
	}
	else if( bArg == 14 )
	{		
		nInterval = (int)(ani_combo_speed(ch, 1 ) / ((ch->point(POINT_ATT_SPEED) / 100.f) + fAdjustNum) );
	}
	else if( bArg > 14 && bArg < 22 )
	{
		nInterval = (int)(ani_combo_speed(ch, bArg - 13 ) / ((ch->point(POINT_ATT_SPEED) / 100.f) + fAdjustNum) );
	}
	else
	{
		metin2::log::error( "ClacValidComboInterval() Invalid bArg({:d}) ch({})", bArg, ch->name() );		
	}	

	return nInterval;
}

bool check_combo_hack(character * ch, uint8_t bArg, uint32_t dwTime, bool CheckSpeedHack)
{
	//	cannot attack while dead or stunned, so skip.
	//	instead of this, adding it to CHRACTER::CanMove() 
	//	seems like the right thing, but
	//	other code already checks CanMove() independently of IsStun() and IsDead(),
	//	so to minimize the impact of the change
	//	this band-aid code is written here.
	if (ch->is_stun() || ch->is_dead())
		return false;
	int ComboInterval = dwTime - ch->last_combo_time();
	int HackScalar = 0; // base scalar unit 1

	// [2013 09 11 CYH] debugging log
		/*metin2::log::info( "COMBO_TEST_LOG: {} arg:{:d} interval:{:d} valid:{:d} atkspd:{:d} riding:{}",
						ch->GetName(),
						bArg,
						ComboInterval,
						ch->GetValidComboInterval(),
						ch->GetPoint(POINT_ATT_SPEED),
						ch->IsRiding() ? "yes" : "no");*/

	// bArg 14 ~ 21, up to 8 combos in total
	// 1. the first combo (14) can repeat after a certain time
	// 2. 15 ~ 21 cannot repeat
	// 3. they increase in order.
	if (bArg == 14)
	{
		if (CheckSpeedHack && ComboInterval > 0 && ComboInterval < ch->valid_combo_interval() - COMBO_HACK_ALLOWABLE_MS)
		{
			// FIXME the first combo can oddly arrive early, so divided by 300 -_-;
			// when attacking while being knocked down by many monsters
			// the first combo arrives at a very short interval.
			// this sometimes caused disconnects for combo hack, so the following code is disabled.

			//sys_log(0, "COMBO_HACK: 2 %s arg:%u interval:%d valid:%u atkspd:%u riding:%s",
			//		ch->GetName(),
			//		bArg,
			//		ComboInterval,
		}

		ch->set_combo_sequence(1);
		// 2013 09 11 CYH edited
		ch->set_valid_combo_interval( clac_valid_combo_interval(ch, bArg) );
		ch->set_last_combo_time(dwTime);
	}
	else if (bArg > 14 && bArg < 22)
	{
		int idx = std::min<int>(2, ch->combo_index());

		if (ch->combo_sequence() > 5) // currently there are no combos above 6.
		{
			HackScalar = 1;
			ch->set_valid_combo_interval(300);
			metin2::log::info( "COMBO_HACK: 5 {} combo_seq:{:d}", ch->name(), ch->combo_sequence());
		}
		// assassin dual-wield combo exception
		else if (bArg == 21 &&
				 idx == 2 &&
				 ch->combo_sequence() == 5 &&
				 ch->job() == JOB_ASSASSIN &&
				 ch->wear(WEAR_WEAPON) &&
				 ch->wear(WEAR_WEAPON)->sub_type() == WEAPON_DAGGER)
			ch->set_valid_combo_interval(300);
		else if (combo_sequence_by_skill_level[idx][ch->combo_sequence()] != bArg)
		{
			HackScalar = 1;
			ch->set_valid_combo_interval(300);

			metin2::log::info( "COMBO_HACK: 3 {} arg:{:d} valid:{:d} combo_idx:{:d} combo_seq:{:d}",
					ch->name(),
					bArg,
					combo_sequence_by_skill_level[idx][ch->combo_sequence()],
					idx,
					ch->combo_sequence());
		}
		else
		{
			if (CheckSpeedHack && ComboInterval < ch->valid_combo_interval() - COMBO_HACK_ALLOWABLE_MS)
			{
				HackScalar = 1 + (ch->valid_combo_interval() - ComboInterval) / 100;

				metin2::log::info( "COMBO_HACK: 2 {} arg:{:d} interval:{:d} valid:{:d} atkspd:{:d} riding:{}",
						ch->name(),
						bArg,
						ComboInterval,
						ch->valid_combo_interval(),
						ch->point(POINT_ATT_SPEED),
						ch->is_riding() ? "yes" : "no");
			}

			// when riding a horse, 15 ~ 16 repeat
			//if (ch->IsHorseRiding())
			if (ch->is_riding())
				ch->set_combo_sequence(ch->combo_sequence() == 1 ? 2 : 1);
			else
				ch->set_combo_sequence(ch->combo_sequence() + 1);

			// 2013 09 11 CYH edited
			ch->set_valid_combo_interval( clac_valid_combo_interval(ch, bArg) );
			ch->set_last_combo_time(dwTime);
		}
	}
	else if (bArg == 13) // basic attack (sent while polymorphed)
	{
		if (CheckSpeedHack && ComboInterval > 0 && ComboInterval < ch->valid_combo_interval() - COMBO_HACK_ALLOWABLE_MS)
		{
			// when attacking while being knocked down by many monsters
			// the first combo arrives at a very short interval.
			// this sometimes caused disconnects for combo hack, so the following code is disabled.

			//sys_log(0, "COMBO_HACK: 6 %s arg:%u interval:%d valid:%u atkspd:%u",
			//		ch->GetName(),
			//		bArg,
			//		ComboInterval,
		}

		if (ch->race_num() >= MAIN_RACE_MAX_NUM)
		{
			// POLYMORPH_BUG_FIX

			// DELETEME

			// 2013 09 11 CYH edited
			ch->set_valid_combo_interval( clac_valid_combo_interval(ch, bArg) );
			ch->set_last_combo_time(dwTime);
			// END_OF_POLYMORPH_BUG_FIX
		}
		else
		{
			// a nonsensical combo arrived - possibly a hacker?

			// because of the code above, attacking while polymorph is being removed
			// is sometimes detected as a hack.

			// in detail,
			// the server has processed poly 0, but
			// before the client receives that packet, it attacks a mob. <- i.e. attacks while still in mob form.
			//
			// then the client sends the server a command saying it attacked in mob form (arg == 13)
			//
			// and the server says "race is human but the attack type is a mob's!" and flags a hack.

			// really, the attack pattern should not be decided by the client and sent,
			// it should be decided by the server... why was it done like this...
			// by rtsummit
		}
	}
	else
	{
		// a nonsensical combo arrived - possibly a hacker?
		if (ch->get_desc()->delayed_disconnect(number(2, 9)))
		{
			log_manager::instance().hack_log("Hacker", ch);
			metin2::log::info( "HACKER: {} arg {:d}", ch->name(), bArg);
		}

		HackScalar = 10;
		ch->set_valid_combo_interval(300);
	}

	if (HackScalar)
	{
		// for 1.5 seconds after mounting or dismounting a horse, attacks are not treated as hacks but deal no damage
		if (get_dword_time() - ch->last_mount_time() > 1500)
			ch->increase_combo_hack_count(1 + HackScalar);

		ch->skip_combo_attack_by_time(ch->valid_combo_interval());
	}

	return HackScalar;
}

void input_main::move(character * ch, const char * data)
{
	if (!ch->can_move())
		return;

	struct packet_cg_move * pinfo = (struct packet_cg_move *) data;

	if (pinfo->func >= FUNC_MAX_NUM && !(pinfo->func & 0x80))
	{
		metin2::log::error("invalid move type: {}", ch->name());
		return;
	}

	//enum EMoveFuncType
	//	FUNC_WAIT,
	//	FUNC_MOVE,
	//	FUNC_ATTACK,
	//	FUNC_COMBO,
	//	FUNC_MOB_SKILL,
	//	_FUNC_SKILL,
	//	FUNC_MAX_NUM,

	// teleport hack check

//	if (!test_server)	//2012.05.15 Kim Yong-uk : on the test server, attacking while being knocked down by many monsters (while invincible) caused deaths from the combo hack check.
	{
		const float fDist = distance_sqrt((ch->x() - pinfo->x) / 100, (ch->y() - pinfo->y) / 100);

		if (((false == ch->is_riding() && fDist > 25) || fDist > 40) && OXEVENT_MAP_INDEX != ch->map_index())
		{

			metin2::log::info( "MOVE: {} trying to move too far (dist: {:.1f}m) Riding({:d})", ch->name(), fDist, ch->is_riding());

			ch->show(ch->map_index(), ch->x(), ch->y(), ch->z());
			ch->stop();
			return;
		}

		//
		// speed hack (SPEEDHACK) check
		//
		uint32_t dwCurTime = get_dword_time();
		// check starting 7 seconds after time sync. (was 5 seconds before 20090702)
		bool CheckSpeedHack = (false == ch->get_desc()->is_handshaking() && dwCurTime - ch->get_desc()->client_time() > 7000);

		if (CheckSpeedHack)
		{
			int iDelta = (int) (pinfo->time - ch->get_desc()->client_time());
			int iServerDelta = (int) (dwCurTime - ch->get_desc()->client_time());

			iDelta = (int) (dwCurTime - pinfo->time);

			// time is running slow. just log it for now. need to check whether many people actually do this. TODO
			if (iDelta >= 30000)
			{
				metin2::log::info( "SPEEDHACK: slow timer name {} delta {:d}", ch->name(), iDelta);
			}
			// running up to 20 msec fast per second is tolerated.
			else if (iDelta < -(iServerDelta / 50))
			{
				metin2::log::info( "SPEEDHACK: DETECTED! {} (delta {:d} {:d})", ch->name(), iDelta, iServerDelta);
			}
		}

		//
		// combo hack and speed hack check
		//
		if (pinfo->func == FUNC_COMBO && metin2::config::game().anti_cheat.check_multihack)
		{
			check_combo_hack(ch, pinfo->arg, pinfo->time, CheckSpeedHack); // combo check
		}
	}

	if (pinfo->func == FUNC_MOVE)
	{
		if (ch->limit_point(POINT_MOV_SPEED) == 0)
			return;

		ch->set_rotation(pinfo->rot * 5);	// duplicate code
		ch->reset_stop_time();				// ""

		ch->goto_(pinfo->x, pinfo->y);
	}
	else
	{
		if (pinfo->func == FUNC_ATTACK || pinfo->func == FUNC_COMBO)
			ch->on_move(true);
		else if (pinfo->func & FUNC_SKILL)
		{
			const int MASK_SKILL_MOTION = 0x7F;
			unsigned int motion = pinfo->func & MASK_SKILL_MOTION;

			if (!ch->is_usable_skill_motion(motion))
			{
				const char* name = ch->name();
				unsigned int job = ch->job();
				unsigned int group = ch->skill_group();

				char szBuf[256];
				snprintf(szBuf, sizeof(szBuf), "SKILL_HACK: name=%s, job=%d, group=%d, motion=%d", name, job, group, motion);
				log_manager::instance().hack_log(szBuf, ch->get_desc()->get_account_table().login, ch->name(), ch->get_desc()->host_name());
				metin2::log::info( "{}", szBuf);

				if (metin2::config::test_server())
				{
					ch->get_desc()->delayed_disconnect(number(2, 8));
					ch->chat_packet(CHAT_TYPE_INFO, szBuf);
				}
				else
				{
					ch->get_desc()->delayed_disconnect(number(150, 500));
				}
			}

			ch->on_move();
		}

		ch->set_rotation(pinfo->rot * 5);	// duplicate code
		ch->reset_stop_time();				// ""

		ch->move(pinfo->x, pinfo->y);
		ch->stop();
		ch->stop_stamina_consume();
	}

	packet_gc_move pack;

	pack.header      = HEADER_GC_MOVE;
	pack.func        = pinfo->func;
	pack.arg         = pinfo->arg;
	pack.rot         = pinfo->rot;
	pack.vid        = ch->get_vid();
	pack.x           = pinfo->x;
	pack.y           = pinfo->y;
	pack.time       = pinfo->time;
	pack.duration   = (pinfo->func == FUNC_MOVE) ? ch->current_move_duration() : 0;

	ch->packet_around(&pack, sizeof(packet_gc_move), ch);
	/*
	metin2::log::info( 
			"MOVE: {} Func:{:d} Arg:{:d} Pos:{:d}x{:d} Time:{:d} Dist:{:.1f}",
			ch->GetName(),
			pinfo->bFunc,
			pinfo->bArg,
			pinfo->lX / 100,
			pinfo->lY / 100,
			pinfo->dwTime,
			fDist);
	*/
}

void input_main::attack(character * ch, const uint8_t header, const char* data)
{
	if (nullptr == ch)
		return;

	struct type_identifier
	{
		uint8_t header;
		uint8_t type;
	};

	const struct type_identifier* const type = reinterpret_cast<const struct type_identifier*>(data);

	if (type->type > 0)
	{
		if (false == ch->can_use_skill(type->type))
		{
			return;
		}

		switch (type->type)
		{
			case SKILL_GEOMPUNG:
			case SKILL_SANGONG:
			case SKILL_YEONSA:
			case SKILL_KWANKYEOK:
			case SKILL_HWAJO:
			case SKILL_GIGUNG:
			case SKILL_PABEOB:
			case SKILL_MARYUNG:
			case SKILL_TUSOK:
			case SKILL_MAHWAN:
			case SKILL_BIPABU:
			case SKILL_NOEJEON:
			case SKILL_CHAIN:
			case SKILL_HORSE_WILDATTACK_RANGE:
				if (HEADER_CG_SHOOT != type->header)
				{
					if (metin2::config::test_server()) 
						ch->chat_packet(CHAT_TYPE_INFO, lc_text("Attack :name[%s] Vnum[%d] can't use skill by attack(warning)"), type->type);
					return;
				}
				break;
		}
	}

	switch (header)
	{
		case HEADER_CG_ATTACK:
			{
				if (nullptr == ch->get_desc())
					return;

				const packet_cg_attack* const packMelee = reinterpret_cast<const packet_cg_attack*>(data);

				ch->get_desc()->assemble_crc_magic_cube(packMelee->crc_magic_cube_proc_piece, packMelee->crc_magic_cube_file_piece);

				character *	victim = character_manager::instance().find(packMelee->vid);

				if (nullptr == victim || ch == victim)
					return;

				switch (victim->get_char_type())
				{
					case CHAR_TYPE_NPC:
					case CHAR_TYPE_WARP:
					case CHAR_TYPE_GOTO:
						return;
				}

				if (packMelee->type > 0)
				{
					if (false == ch->check_skill_hit_count(packMelee->type, victim->get_vid()))
					{
						return;
					}
				}

				ch->attack(victim, packMelee->type);
			}
			break;

		case HEADER_CG_SHOOT:
			{
				const packet_cg_shoot* const packShoot = reinterpret_cast<const packet_cg_shoot*>(data);

				ch->shoot(packShoot->type);
			}
			break;
	}
}

int input_main::sync_position(character * ch, const char * c_pcData, size_t uiBytes)
{
	const packet_cg_sync_position* pinfo = reinterpret_cast<const packet_cg_sync_position*>( c_pcData );

	if (uiBytes < pinfo->size)
		return -1;

	int iExtraLen = pinfo->size - sizeof(packet_cg_sync_position);

	if (iExtraLen < 0)
	{
		metin2::log::error("invalid packet length (len {:d} size {:d} buffer {:d})", iExtraLen, pinfo->size, uiBytes);
		ch->get_desc()->set_phase(PHASE_CLOSE);
		return -1;
	}

	if (0 != (iExtraLen % sizeof(packet_cg_sync_position_element)))
	{
		metin2::log::error("invalid packet length {:d} (name: {})", pinfo->size, ch->name());
		return iExtraLen;
	}

	int iCount = iExtraLen / sizeof(packet_cg_sync_position_element);

	if (iCount <= 0)
		return iExtraLen;

	static const int nCountLimit = 16;

	if( iCount > nCountLimit )
	{
		metin2::log::error( "Too many SyncPosition Count({:d}) from Name({})", iCount, ch->name() );
		iCount = nCountLimit;
	}

	std::vector<packet_cg_sync_position_element> synced;
	synced.reserve(iCount);

	const packet_cg_sync_position_element* e = 
		reinterpret_cast<const packet_cg_sync_position_element*>(c_pcData + sizeof(packet_cg_sync_position));

	timeval tvCurTime;
	gettimeofday(&tvCurTime, nullptr);

	for (int i = 0; i < iCount; ++i, ++e)
	{
		character * victim = character_manager::instance().find(e->vid);

		if (!victim)
			continue;

		switch (victim->get_char_type())
		{
			case CHAR_TYPE_NPC:
			case CHAR_TYPE_WARP:
			case CHAR_TYPE_GOTO:
				continue;
		}

		// ownership check
		if (!victim->set_sync_owner(ch))
			continue;

		const float fDistWithSyncOwner = distance_sqrt( (victim->x() - ch->x()) / 100, (victim->y() - ch->y()) / 100 );
		static const float fLimitDistWithSyncOwner = 2500.f + 1000.f;
		// if the distance to the victim is 2500 + a or more, treat it as a hack.
		//	distance reference: client functions __GetSkillTargetRange, __GetBowRange
		//	2500 : range of the longest-range skill in skill proto, or bow range
		//	a = POINT_BOW_DISTANCE value... not sure if it is actually used. not on any item, potion, skill or quest...
		//		still, just in case, set to 1000.f to also serve as a buffer...
		if (fDistWithSyncOwner > fLimitDistWithSyncOwner)
		{
			// tolerate up to g_iSyncHackLimitCount times.
			if (ch->sync_hack_count() < metin2::config::game().anti_cheat.sync_hack_limit)
			{
				ch->set_sync_hack_count(ch->sync_hack_count() + 1);
				continue;
			}
			else
			{
				log_manager::instance().hack_log( "SYNC_POSITION_HACK", ch );

				metin2::log::error( "Too far SyncPosition DistanceWithSyncOwner({:f})({}) from Name({}) CH({:d},{:d}) VICTIM({:d},{:d}) SYNC({:d},{:d})",
					fDistWithSyncOwner, victim->name(), ch->name(), ch->x(), ch->y(), victim->x(), victim->y(),
					e->x, e->y );

				ch->get_desc()->set_phase(PHASE_CLOSE);

				return -1;
			}
		}

		const float fDist = distance_sqrt( (victim->x() - e->x) / 100, (victim->y() - e->y) / 100 );
		static const int32_t g_lValidSyncInterval = 50 * 1000; // 100ms -> 50ms 2013 09 11 CYH
		const timeval &tvLastSyncTime = victim->last_sync_time();
		const int64_t elapsed_usec = (static_cast<int64_t>(tvCurTime.tv_sec) - tvLastSyncTime.tv_sec) * 1000000 + (tvCurTime.tv_usec - tvLastSyncTime.tv_usec);

		// to defend against hacks that abuse SyncPosition to send other users to strange places,
		// trying to SyncPosition the same user again within g_lValidSyncInterval ms is treated as a hack.
		if (elapsed_usec < g_lValidSyncInterval)
		{
			// tolerate up to g_iSyncHackLimitCount times.
			if (ch->sync_hack_count() < metin2::config::game().anti_cheat.sync_hack_limit)
			{
				ch->set_sync_hack_count(ch->sync_hack_count() + 1);
				continue;
			}
			else
			{
				log_manager::instance().hack_log( "SYNC_POSITION_HACK", ch );

				metin2::log::error( "Too often SyncPosition Interval({:d}ms)({}) from Name({}) VICTIM({:d},{:d}) SYNC({:d},{:d})",
					static_cast<long>(elapsed_usec / 1000), victim->name(), ch->name(), victim->x(), victim->y(),
					e->x, e->y );

				ch->get_desc()->set_phase(PHASE_CLOSE);

				return -1;
			}
		}
		else if( fDist > 25.0f )
		{
			log_manager::instance().hack_log( "SYNC_POSITION_HACK", ch );

			metin2::log::error( "Too far SyncPosition Distance({:f})({}) from Name({}) CH({:d},{:d}) VICTIM({:d},{:d}) SYNC({:d},{:d})",
				   	fDist, victim->name(), ch->name(), ch->x(), ch->y(), victim->x(), victim->y(),
				  e->x, e->y );

			ch->get_desc()->set_phase(PHASE_CLOSE);

			return -1;
		}
		else
		{
			victim->set_last_sync_time(tvCurTime);
			victim->sync(e->x, e->y);
			synced.push_back(*e);
		}
	}

	if (!synced.empty())
	{
		packet_gc_sync_position header;
		header.header = HEADER_GC_SYNC_POSITION;
		header.size = static_cast<uint16_t>(sizeof(header) + synced.size() * sizeof(packet_cg_sync_position_element));

		temp_buffer buf;
		buf.write(&header, sizeof(header));
		buf.write(synced.data(), synced.size() * sizeof(packet_cg_sync_position_element));
		ch->packet_around(buf.read_peek(), buf.size(), ch);
	}

	return iExtraLen;
}

void input_main::fly_target(character * ch, const char * pcData, uint8_t bHeader)
{
	packet_cg_fly_targeting * p = (packet_cg_fly_targeting *) pcData;
	ch->fly_target(p->target_vid, p->x, p->y, bHeader);
}

void input_main::use_skill(character * ch, const char * pcData)
{
	packet_cg_use_skill * p = (packet_cg_use_skill *) pcData;
	ch->use_skill(p->vnum, character_manager::instance().find(p->vid));
}

void input_main::script_button(character * ch, const void* c_pData)
{
	packet_cg_script_button * p = (packet_cg_script_button *) c_pData;
	metin2::log::info( "QUEST ScriptButton pid {:d} idx {:d}", ch->player_id(), p->idx);

	quest::pc* pc = quest::quest_manager::instance().pc_force(ch->player_id());
	if (pc && pc->is_confirm_wait())
	{
		quest::quest_manager::instance().confirm(ch->player_id(), quest::CONFIRM_TIMEOUT);
	}
	else if (p->idx & 0x80000000)
	{
		quest::quest_manager::instance().quest_info(ch->player_id(), p->idx & 0x7fffffff);
	}
	else
	{
		quest::quest_manager::instance().quest_button(ch->player_id(), p->idx);
	}
}

void input_main::script_answer(character * ch, const void* c_pData)
{
	packet_cg_script_answer * p = (packet_cg_script_answer *) c_pData;
	metin2::log::info( "QUEST ScriptAnswer pid {:d} answer {:d}", ch->player_id(), p->answer);

	if (p->answer > 250) // packet sent in response to the Next button
	{
		quest::quest_manager::instance().resume(ch->player_id());
	}
	else // packet sent after picking a choice button
	{
		quest::quest_manager::instance().select(ch->player_id(),  p->answer);
	}
}

// SCRIPT_SELECT_ITEM
void input_main::script_select_item(character * ch, const void* c_pData)
{
	packet_cg_script_select_item* p = (packet_cg_script_select_item*) c_pData;
	metin2::log::info( "QUEST ScriptSelectItem pid {:d} answer {:d}", ch->player_id(), p->selection);
	quest::quest_manager::instance().select_item(ch->player_id(), p->selection);
}
// END_OF_SCRIPT_SELECT_ITEM

void input_main::quest_input_string(character * ch, const void* c_pData)
{
	packet_cg_quest_input_string * p = (packet_cg_quest_input_string*) c_pData;

	const std::string msg(p->msg, strnlen(p->msg, sizeof(p->msg) - 1));
	metin2::log::info( "QUEST InputString pid {:d} msg {}", ch->player_id(), msg.c_str());

	quest::quest_manager::instance().input(ch->player_id(), msg.c_str());
}

void input_main::quest_confirm(character * ch, const void* c_pData)
{
	packet_cg_quest_confirm* p = (packet_cg_quest_confirm*) c_pData;
	character * ch_wait = character_manager::instance().find_by_pid(p->request_pid);
	if (p->answer)
		p->answer = quest::CONFIRM_YES;
	metin2::log::info( "QuestConfirm from {} pid {:d} name {} answer {:d}", ch->name(), p->request_pid, (ch_wait)?ch_wait->name():"", p->answer);
	if (ch_wait)
	{
		quest::quest_manager::instance().confirm(ch_wait->player_id(), (quest::quest_confirm_type) p->answer, ch->player_id());
	}
}

void input_main::target(character * ch, const char * pcData)
{
	packet_cg_target * p = (packet_cg_target *) pcData;

	building::object * pkObj = building::manager::instance().find_object_by_vid(p->vid);

	if (pkObj)
	{
		packet_gc_target pckTarget;
		pckTarget.header = HEADER_GC_TARGET;
		pckTarget.vid = p->vid;
		ch->get_desc()->packet(&pckTarget, sizeof(packet_gc_target));
	}
	else
		ch->set_target(character_manager::instance().find(p->vid));
}

void input_main::warp(character * ch, const char * pcData)
{
	ch->warp_end();
}

void input_main::safebox_checkin(character * ch, const char * c_pData)
{
	if (quest::quest_manager::instance().pc_force(ch->player_id())->is_running() == true)
		return;

	packet_cg_safebox_checkin * p = (packet_cg_safebox_checkin *) c_pData;

	if (!ch->can_handle_item())
		return;

	safebox * pkSafebox = ch->get_safebox();
	item * pkItem = ch->get_item(p->item_pos);

	if (!pkSafebox || !pkItem)
		return;

	if (pkItem->is_exchanging())
		return;

	if (pkItem->type() == ITEM_BELT && pkItem->is_equipped()) // Fix
		return;

	if (pkItem->cell() >= INVENTORY_MAX_NUM && is_set(pkItem->flag(), ITEM_FLAG_IRREMOVABLE))
	{
	    ch->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 창고로 옮길 수 없는 아이템 입니다."));
	    return;
	}

	if (!pkSafebox->is_empty(p->safe_pos, pkItem->size()))
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 옮길 수 없는 위치입니다."));
		return;
	}

	if (pkItem->vnum() == UNIQUE_ITEM_SAFEBOX_EXPAND)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 이 아이템은 넣을 수 없습니다."));
		return;
	}

	if( is_set(pkItem->anti_flag(), ITEM_ANTIFLAG_SAFEBOX) )
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 이 아이템은 넣을 수 없습니다."));
		return;
	}

	if (true == pkItem->is_locked())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 이 아이템은 넣을 수 없습니다."));
		return;
	}

	pkItem->remove_from_character();
	if (!pkItem->is_dragon_soul())
		ch->sync_quickslot(QUICKSLOT_TYPE_ITEM, p->item_pos.cell, 255);
	pkSafebox->add(p->safe_pos, pkItem);

	char szHint[128];
	snprintf(szHint, sizeof(szHint), "%s %u", pkItem->name(), pkItem->count());
	log_manager::instance().item_log(ch, pkItem, "SAFEBOX PUT", szHint);
}

void input_main::safebox_checkout(character * ch, const char * c_pData, bool bMall)
{
	packet_cg_safebox_checkout * p = (packet_cg_safebox_checkout *) c_pData;

	if (!ch->can_handle_item())
		return;

	safebox * pkSafebox;

	if (bMall)
		pkSafebox = ch->mall();
	else
		pkSafebox = ch->get_safebox();

	if (!pkSafebox)
		return;

	item * pkItem = pkSafebox->get(p->safe_pos);

	if (!pkItem)
		return;

	if (!ch->is_empty_item_grid(p->item_pos, pkItem->size()))
		return;

	// Same rule as CHARACTER::MoveItem: only some item types may go into the belt inventory.
	if (INVENTORY == p->item_pos.window_type && p->item_pos.is_belt_inventory_position() && !belt_inventory_helper::can_move_into_belt_inventory(pkItem))
		return;

	// special handling for dragon soul stones when moving from the item mall to the inventory
	// (items created in the mall get attributes as defined in item_proto,
	//  so without this handling dragon soul stones would get no attributes at all.)
	if (pkItem->is_dragon_soul())
	{
		if (bMall)
		{
			ds_manager::instance().dragon_soul_item_initialize(pkItem);
		}

		if (DRAGON_SOUL_INVENTORY != p->item_pos.window_type)
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 옮길 수 없는 위치입니다."));
			return;
		}

		item_pos DestPos = p->item_pos;
		if (!ds_manager::instance().is_valid_cell_for_this_item(pkItem, DestPos))
		{
			int iCell = ch->empty_dragon_soul_inventory(pkItem);
			if (iCell < 0)
			{
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 옮길 수 없는 위치입니다."));
				return ;
			}
			DestPos = item_pos (DRAGON_SOUL_INVENTORY, iCell);
		}

		pkSafebox->remove(p->safe_pos);
		pkItem->add_to_character(ch, DestPos);
		item_manager::instance().flush_delayed_save(pkItem);
	}
	else
	{
		if (DRAGON_SOUL_INVENTORY == p->item_pos.window_type)
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 옮길 수 없는 위치입니다."));
			return;
		}

		pkSafebox->remove(p->safe_pos);
		if (bMall)
		{
			if (nullptr == pkItem->proto())
			{
				metin2::log::error("pkItem->GetProto() == NULL (id : {:d})",pkItem->id());
				return ;
			}
			// if attributes should be attached with 100% probability but aren't, attach them anew. ...............
			if (100 == pkItem->proto()->alter_to_magic_item_pct && 0 == pkItem->attribute_count())
			{
				pkItem->alter_to_magic_item();
			}
		}
		pkItem->add_to_character(ch, p->item_pos);
		item_manager::instance().flush_delayed_save(pkItem);
	}

	uint32_t dwID = pkItem->id();
	db_clientdesc->db_packet_header(HEADER_GD_ITEM_FLUSH, 0, sizeof(uint32_t));
	db_clientdesc->packet(&dwID, sizeof(uint32_t));

	char szHint[128];
	snprintf(szHint, sizeof(szHint), "%s %u", pkItem->name(), pkItem->count());
	if (bMall)
		log_manager::instance().item_log(ch, pkItem, "MALL GET", szHint);
	else
		log_manager::instance().item_log(ch, pkItem, "SAFEBOX GET", szHint);
}

void input_main::safebox_item_move(character * ch, const char * data)
{
	struct packet_cg_item_move * pinfo = (struct packet_cg_item_move *) data;

	if (!ch->can_handle_item())
		return;

	if (!ch->get_safebox())
		return;

	ch->get_safebox()->move_item(pinfo->cell.cell, pinfo->cell_to.cell, pinfo->count);
}

// PARTY_JOIN_BUG_FIX
void input_main::party_invite(character * ch, const char * c_pData)
{
	if (ch->get_arena())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("대련장에서 사용하실 수 없습니다."));
		return;
	}

	packet_cg_party_invite * p = (packet_cg_party_invite*) c_pData;

	character * pInvitee = character_manager::instance().find(p->vid);

	if (!pInvitee || !ch->get_desc() || !pInvitee->get_desc())
	{
		metin2::log::error("PARTY Cannot find invited character");
		return;
	}

	ch->party_invite(pInvitee);
}

void input_main::party_invite_answer(character * ch, const char * c_pData)
{
	if (ch->get_arena())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("대련장에서 사용하실 수 없습니다."));
		return;
	}

	packet_cg_party_invite_answer * p = (packet_cg_party_invite_answer*) c_pData;

	character * pInviter = character_manager::instance().find(p->leader_vid);

	// pInviter had sent a party request to ch.

	if (!pInviter)
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티요청을 한 캐릭터를 찾을수 없습니다."));
	else if (!p->accept)
		pInviter->party_invite_deny(ch->player_id());
	else
		pInviter->party_invite_accept(ch);
}
// END_OF_PARTY_JOIN_BUG_FIX

void input_main::party_set_state(character * ch, const char* c_pData)
{
	if (!party_manager::instance().is_enable_pc_party())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 서버 문제로 파티 관련 처리를 할 수 없습니다."));
		return;
	}

	packet_cg_party_set_state* p = (packet_cg_party_set_state*) c_pData;

	if (!ch->get_party())
		return;

	if (ch->get_party()->leader_pid() != ch->player_id())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 리더만 변경할 수 있습니다."));
		return;
	}

	if (!ch->get_party()->is_member(p->pid))
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 상태를 변경하려는 사람이 파티원이 아닙니다."));
		return;
	}

	uint32_t pid = p->pid;
	metin2::log::info( "PARTY SetRole pid {:d} to role {:d} state {}", pid, p->role, p->flag ? "on" : "off");

	switch (p->role)
	{
		case PARTY_ROLE_NORMAL:
			break;

		case PARTY_ROLE_ATTACKER: 
		case PARTY_ROLE_TANKER: 
		case PARTY_ROLE_BUFFER:
		case PARTY_ROLE_SKILL_MASTER:
		case PARTY_ROLE_HASTE:
		case PARTY_ROLE_DEFENDER:
			if (ch->get_party()->set_role(pid, p->role, p->flag))
			{
				packet_party_state_change pack;
				pack.leader_pid = ch->player_id();
				pack.pid = p->pid;
				pack.role = p->role;
				pack.flag = p->flag;
				db_clientdesc->db_packet(HEADER_GD_PARTY_STATE_CHANGE, 0, &pack, sizeof(pack));
			}
			/* else
			   ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("<파티> 어태커 설정에 실패하였습니다.")); */
			break;

		default:
			metin2::log::error("wrong byRole in PartySetState Packet name {} state {:d}", ch->name(), p->role);
			break;
	}
}

void input_main::party_remove(character * ch, const char* c_pData)
{
	if (ch->get_arena())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("대련장에서 사용하실 수 없습니다."));
		return;
	}

	if (!party_manager::instance().is_enable_pc_party())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 서버 문제로 파티 관련 처리를 할 수 없습니다."));
		return;
	}

	if (ch->get_dungeon())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 던전 안에서는 파티에서 추방할 수 없습니다."));
		return;
	}

	packet_cg_party_remove* p = (packet_cg_party_remove*) c_pData;

	if (!ch->get_party())
		return;

	party * pParty = ch->get_party();
	if (pParty->leader_pid() == ch->player_id())
	{
		if (ch->get_dungeon())
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 던젼내에서는 파티원을 추방할 수 없습니다."));
		}
		else
		{
			// leader can remove any member
			if (p->pid == ch->player_id() || pParty->member_count() == 2)
			{
				// party disband
				party_manager::instance().delete_party(pParty);
			}
			else
			{
				character * B = character_manager::instance().find_by_pid(p->pid);
				if (B)
				{
					B->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티에서 추방당하셨습니다."));
				}
				pParty->quit(p->pid);
			}
		}
	}
	else
	{
		// otherwise, only remove itself
		if (p->pid == ch->player_id())
		{
			if (ch->get_dungeon())
			{
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 던젼내에서는 파티를 나갈 수 없습니다."));
			}
			else
			{
				if (pParty->member_count() == 2)
				{
					// party disband
					party_manager::instance().delete_party(pParty);
				}
				else
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티에서 나가셨습니다."));
					pParty->quit(ch->player_id());
				}
			}
		}
		else
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 다른 파티원을 탈퇴시킬 수 없습니다."));
		}
	}
}

void input_main::answer_make_guild(character * ch, const char* c_pData)
{
	packet_cg_answer_make_guild* p = (packet_cg_answer_make_guild*) c_pData;

	// Only an answer to the guild NPC's request counts, and only once.
	const uint32_t dwKey = ch->guild_creation_key();
	ch->set_guild_creation_key(0);

	if (dwKey == 0 || p->key != dwKey)
	{
		metin2::log::error("AnswerMakeGuild: {} answered without a matching request", ch->name());
		return;
	}

	// The quest asks for the same.
	if (ch->level() < 40 || ch->gold() < 200000)
		return;

	if (get_global_time() - ch->quest_flag("guild_building.new_disband_time") <
			guild_manager::instance().disband_delay())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 해산한 후 %d일 이내에는 길드를 만들 수 없습니다."), 
				quest::quest_manager::instance().event_flag("guild_disband_delay"));
		return;
	}

	if (get_global_time() - ch->quest_flag("guild_building.new_withdraw_time") <
			guild_manager::instance().withdraw_delay())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 탈퇴한 후 %d일 이내에는 길드를 만들 수 없습니다."), 
				quest::quest_manager::instance().event_flag("guild_withdraw_delay"));
		return;
	}

	if (ch->get_guild())
		return;

	guild_manager& gm = guild_manager::instance();

	guild_create_parameter cp;
	memset(&cp, 0, sizeof(cp));

	cp.master = ch;
	copy_packet_string(cp.name, sizeof(cp.name), p->guild_name, sizeof(p->guild_name));

	if (cp.name[0] == 0 || !check_name(cp.name))
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("적합하지 않은 길드 이름 입니다."));
		return;
	}

	uint32_t dwGuildID = gm.create_guild(cp);

	if (dwGuildID)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> [%s] 길드가 생성되었습니다."), cp.name);

		int GuildCreateFee;

		GuildCreateFee = 200000;

		ch->point_change(POINT_GOLD, -GuildCreateFee);
		db_manager::instance().send_money_log(MONEY_LOG_GUILD, ch->player_id(), -GuildCreateFee);

		char Log[128];
		snprintf(Log, sizeof(Log), "GUILD_NAME %s MASTER %s", cp.name, ch->name());
		log_manager::instance().char_log(ch, 0, "MAKE_GUILD", Log);

		ch->remove_specify_item(GUILD_CREATE_ITEM_VNUM, 1);
	}
	else
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드 생성에 실패하였습니다."));
}

void input_main::party_use_skill(character * ch, const char* c_pData)
{
	packet_cg_party_use_skill* p = (packet_cg_party_use_skill*) c_pData; 
	if (!ch->get_party())
		return;

	if (ch->player_id() != ch->get_party()->leader_pid())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티 기술은 파티장만 사용할 수 있습니다."));
		return;
	}

	switch (p->skill_index)
	{
		case PARTY_SKILL_HEAL:
			ch->get_party()->heal_party();
			break;
		case PARTY_SKILL_WARP:
			{
				character * pch = character_manager::instance().find(p->vid);
				if (pch)
					ch->get_party()->summon_to_leader(pch->player_id());
				else
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 소환하려는 대상을 찾을 수 없습니다."));
			}
			break;
	}
}

void input_main::party_parameter(character * ch, const char * c_pData)
{
	packet_cg_party_parameter * p = (packet_cg_party_parameter *) c_pData;

	if (ch->get_party() && ch->get_party()->leader_pid() == ch->player_id())
		ch->get_party()->set_parameter(p->distribute_mode);
}

size_t get_sub_packet_size(const guild_subheader_cg& header)
{
	switch (header)
	{
		case GUILD_SUBHEADER_CG_DEPOSIT_MONEY:				return sizeof(int);
		case GUILD_SUBHEADER_CG_WITHDRAW_MONEY:				return sizeof(int);
		case GUILD_SUBHEADER_CG_ADD_MEMBER:					return sizeof(uint32_t);
		case GUILD_SUBHEADER_CG_REMOVE_MEMBER:				return sizeof(uint32_t);
		case GUILD_SUBHEADER_CG_CHANGE_GRADE_NAME:			return 10;
		case GUILD_SUBHEADER_CG_CHANGE_GRADE_AUTHORITY:		return sizeof(uint8_t) + sizeof(uint8_t);
		case GUILD_SUBHEADER_CG_OFFER:						return sizeof(uint32_t);
		case GUILD_SUBHEADER_CG_CHARGE_GSP:					return sizeof(int);
		case GUILD_SUBHEADER_CG_POST_COMMENT:				return 1;
		case GUILD_SUBHEADER_CG_DELETE_COMMENT:				return sizeof(uint32_t);
		case GUILD_SUBHEADER_CG_REFRESH_COMMENT:			return 0;
		case GUILD_SUBHEADER_CG_CHANGE_MEMBER_GRADE:		return sizeof(uint32_t) + sizeof(uint8_t);
		case GUILD_SUBHEADER_CG_USE_SKILL:					return sizeof(packet_cg_guild_use_skill);
		case GUILD_SUBHEADER_CG_CHANGE_MEMBER_GENERAL:		return sizeof(uint32_t) + sizeof(uint8_t);
		case GUILD_SUBHEADER_CG_GUILD_INVITE_ANSWER:		return sizeof(uint32_t) + sizeof(uint8_t);
	}

	return 0;
}

int input_main::guild(character * ch, const char * data, size_t uiBytes)
{
	if (uiBytes < sizeof(packet_cg_guild))
		return -1;

	const packet_cg_guild* p = reinterpret_cast<const packet_cg_guild*>(data);
	const char* c_pData = data + sizeof(packet_cg_guild);

	uiBytes -= sizeof(packet_cg_guild);

	const guild_subheader_cg SubHeader = static_cast<guild_subheader_cg>(p->subheader);
	const size_t SubPacketLen = get_sub_packet_size(SubHeader);

	if (uiBytes < SubPacketLen)
	{
		return -1;
	}

	game::guilds::guild* pGuild = ch->get_guild();

	if (nullptr == pGuild)
	{
		if (SubHeader != GUILD_SUBHEADER_CG_GUILD_INVITE_ANSWER)
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드에 속해있지 않습니다."));
			return SubPacketLen;
		}
	}

	switch (SubHeader)
	{
		case GUILD_SUBHEADER_CG_DEPOSIT_MONEY:
			{
				// by mhh : guild funds cannot be deposited for the time being.
				return SubPacketLen;

				const int gold = std::min(*reinterpret_cast<const int*>(c_pData), deposit_limit());

				if (gold < 0)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 잘못된 금액입니다."));
					return SubPacketLen;
				}

				if (ch->gold() < gold)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 가지고 있는 돈이 부족합니다."));
					return SubPacketLen;
				}

				pGuild->request_deposit_money(ch, gold);
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_WITHDRAW_MONEY:
			{
				// by mhh : guild funds cannot be withdrawn for the time being.
				return SubPacketLen;

				const int gold = std::min(*reinterpret_cast<const int*>(c_pData), 500000);

				if (gold < 0)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 잘못된 금액입니다."));
					return SubPacketLen;
				}

			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_ADD_MEMBER:
			{
				const uint32_t vid = *reinterpret_cast<const uint32_t*>(c_pData);
				character * newmember = character_manager::instance().find(vid);

				// if (!newmember)
				if (!newmember || !newmember->is_pc()) // Fix
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 그러한 사람을 찾을 수 없습니다."));
					return SubPacketLen;
				}

				if (!ch->is_pc())
					return SubPacketLen;

				pGuild->invite(ch, newmember);
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_REMOVE_MEMBER:
			{
				if (pGuild->under_any_war() != 0)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드전 중에는 길드원을 탈퇴시킬 수 없습니다."));
					return SubPacketLen;
				}

				const uint32_t pid = *reinterpret_cast<const uint32_t*>(c_pData);
				const guild_member* m = pGuild->member(ch->player_id());

				if (nullptr == m)
					return -1;

				character * member = character_manager::instance().find_by_pid(pid);

				if (member)
				{
					if (member->get_guild() != pGuild)
					{
						ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 상대방이 같은 길드가 아닙니다."));
						return SubPacketLen;
					}

					if (!pGuild->has_grade_auth(m->grade, GUILD_AUTH_REMOVE_MEMBER))
					{
						ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드원을 강제 탈퇴 시킬 권한이 없습니다."));
						return SubPacketLen;
					}

					member->set_quest_flag("guild_building.new_withdraw_time", get_global_time());
					pGuild->request_remove_member(member->player_id());

				}
				else
				{
					if (!pGuild->has_grade_auth(m->grade, GUILD_AUTH_REMOVE_MEMBER))
					{
						ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드원을 강제 탈퇴 시킬 권한이 없습니다."));
						return SubPacketLen;
					}

					if (pGuild->request_remove_member(pid))
						ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드원을 강제 탈퇴 시켰습니다."));
					else
						ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 그러한 사람을 찾을 수 없습니다."));
				}
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_CHANGE_GRADE_NAME:
			{
				char gradename[GUILD_GRADE_NAME_MAX_LEN + 1];
				copy_packet_string(gradename, sizeof(gradename), c_pData + 1, SubPacketLen - 1);

				const guild_member * m = pGuild->member(ch->player_id());

				if (nullptr == m)
					return -1;

				if (m->grade != GUILD_LEADER_GRADE)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 직위 이름을 변경할 권한이 없습니다."));
				}
				else if (*c_pData == GUILD_LEADER_GRADE)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드장의 직위 이름은 변경할 수 없습니다."));
				}
				else if (!check_name(gradename))
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 적합하지 않은 직위 이름 입니다."));
				}
				else
				{
					pGuild->change_grade_name(*c_pData, gradename);
				}
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_CHANGE_GRADE_AUTHORITY:
			{
				const guild_member* m = pGuild->member(ch->player_id());

				if (nullptr == m)
					return -1;

				if (m->grade != GUILD_LEADER_GRADE)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 직위 권한을 변경할 권한이 없습니다."));
				}
				else if (*c_pData == GUILD_LEADER_GRADE)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드장의 권한은 변경할 수 없습니다."));
				}
				else
				{
					pGuild->change_grade_auth(*c_pData, *(c_pData + 1));
				}
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_OFFER:
			{
				uint32_t offer = *reinterpret_cast<const uint32_t*>(c_pData);

				if (pGuild->level() >= GUILD_MAX_LEVEL)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드가 이미 최고 레벨입니다."));
				}
				else
				{
					offer /= 100;
					offer *= 100;

					if (pGuild->offer_exp(ch, offer))
					{
						ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> %u의 경험치를 투자하였습니다."), offer);
					}
					else
					{
						ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 경험치 투자에 실패하였습니다."));
					}
				}
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_CHARGE_GSP:
			{
				const int offer = *reinterpret_cast<const int*>(c_pData);
				const int64_t gold = static_cast<int64_t>(offer) * 100;

				if (offer < 0 || gold > INT32_MAX || ch->gold() < gold)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 돈이 부족합니다."));
					return SubPacketLen;
				}

				if (!pGuild->charge_sp(ch, offer))
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 용신력 회복에 실패하였습니다."));
				}
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_POST_COMMENT:
			{
				const size_t length = *c_pData;

				if (length > GUILD_COMMENT_MAX_LEN)
				{
					// invalid length.. disconnect.
					metin2::log::error("POST_COMMENT: {} comment too long (length: {:d})", ch->name(), length);
					ch->get_desc()->set_phase(PHASE_CLOSE);
					return -1;
				}

				if (uiBytes < 1 + length)
					return -1;

				const guild_member* m = pGuild->member(ch->player_id());

				if (nullptr == m)
					return -1;

				if (length && !pGuild->has_grade_auth(m->grade, GUILD_AUTH_NOTICE) && *(c_pData + 1) == '!')
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 공지글을 작성할 권한이 없습니다."));
				}
				else
				{
					std::string str(c_pData + 1, length);
					pGuild->add_comment(ch, str);
				}

				return (1 + length);
			}

		case GUILD_SUBHEADER_CG_DELETE_COMMENT:
			{
				const uint32_t comment_id = *reinterpret_cast<const uint32_t*>(c_pData);

				pGuild->delete_comment(ch, comment_id);
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_REFRESH_COMMENT:
			pGuild->refresh_comment(ch);
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_CHANGE_MEMBER_GRADE:
			{
				const uint32_t pid = *reinterpret_cast<const uint32_t*>(c_pData);
				const uint8_t grade = *(c_pData + sizeof(uint32_t));
				const guild_member* m = pGuild->member(ch->player_id());

				if (nullptr == m)
					return -1;

				if (m->grade != GUILD_LEADER_GRADE)
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 직위를 변경할 권한이 없습니다."));
				else if (ch->player_id() == pid)
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드장의 직위는 변경할 수 없습니다."));
				else if (grade == 1)
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드장으로 직위를 변경할 수 없습니다."));
				else if (grade < 1 || grade > GUILD_GRADE_COUNT)
					metin2::log::error("CHANGE_MEMBER_GRADE: {} sent invalid grade {:d}", ch->name(), grade);
				else
					pGuild->change_member_grade(pid, grade);
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_USE_SKILL:
			{
				const packet_cg_guild_use_skill* p = reinterpret_cast<const packet_cg_guild_use_skill*>(c_pData);

				pGuild->use_skill(p->vnum, ch, p->pid);
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_CHANGE_MEMBER_GENERAL:
			{
				const uint32_t pid = *reinterpret_cast<const uint32_t*>(c_pData);
				const uint8_t is_general = *(c_pData + sizeof(uint32_t));
				const guild_member* m = pGuild->member(ch->player_id());

				if (nullptr == m)
					return -1;

				if (m->grade != GUILD_LEADER_GRADE)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 장군을 지정할 권한이 없습니다."));
				}
				else
				{
					if (!pGuild->change_member_general(pid, is_general))
					{
						ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 더이상 장수를 지정할 수 없습니다."));
					}
				}
			}
			return SubPacketLen;

		case GUILD_SUBHEADER_CG_GUILD_INVITE_ANSWER:
			{
				const uint32_t guild_id = *reinterpret_cast<const uint32_t*>(c_pData);
				const uint8_t accept = *(c_pData + sizeof(uint32_t));

				game::guilds::guild * g = guild_manager::instance().find_guild(guild_id);

				if (g)
				{
					if (accept)
						g->invite_accept(ch);
					else
						g->invite_deny(ch->player_id());
				}
			}
			return SubPacketLen;

	}

	return 0;
}

void input_main::fishing(character * ch, const char* c_pData)
{
	packet_cg_fishing* p = (packet_cg_fishing*)c_pData;
	ch->set_rotation(p->dir * 5);
	ch->fishing();
	return;
}

void input_main::item_give(character * ch, const char* c_pData)
{
	packet_cg_give_item* p = (packet_cg_give_item*) c_pData;
	character * to_ch = character_manager::instance().find(p->target_vid);

	if (to_ch)
		ch->give_item(to_ch, p->item_pos);
	else
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("아이템을 건네줄 수 없습니다."));
}

void input_main::hack(character * ch, const char * c_pData)
{
	packet_cg_hack * p = (packet_cg_hack *) c_pData;

	char buf[sizeof(p->buf)];
	copy_packet_string(buf, sizeof(buf), p->buf, sizeof(p->buf));

	metin2::log::error("HACK_DETECT: {} {}", ch->name(), buf);

	// the client currently never sends this packet, so always disconnect
	ch->get_desc()->set_phase(PHASE_CLOSE);
}

int input_main::my_shop(character * ch, const char * c_pData, size_t uiBytes)
{
	packet_cg_my_shop * p = (packet_cg_my_shop *) c_pData;
	int iExtraLen = p->count * sizeof(shop_item_table);

	if (uiBytes < sizeof(packet_cg_my_shop) + iExtraLen)
		return -1;

	if (ch->gold() >= GOLD_MAX)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("소유 돈이 20억냥을 넘어 거래를 핼수가 없습니다."));
		metin2::log::info( "MyShop ==> OverFlow Gold id {:d} name {} ", ch->player_id(), ch->name());
		return (iExtraLen);
	}

	if (ch->is_stun() || ch->is_dead())
		return (iExtraLen);

	if (ch->get_exchange() || ch->is_open_safebox() || ch->shop_owner() || ch->is_cube_open())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("다른 거래중일경우 개인상점을 열수가 없습니다."));
		return (iExtraLen);
	}

	char szSign[SHOP_SIGN_MAX_LEN + 1];
	copy_packet_string(szSign, sizeof(szSign), p->sign, sizeof(p->sign));

	metin2::log::info( "MyShop count {:d}", p->count);
	ch->open_my_shop(szSign, (shop_item_table *) (c_pData + sizeof(packet_cg_my_shop)), p->count);
	return (iExtraLen);
}

void input_main::refine(character * ch, const char* c_pData)
{
	const packet_cg_refine* p = reinterpret_cast<const packet_cg_refine*>(c_pData);

	if (ch->get_exchange() || ch->is_open_safebox() || ch->shop_owner() || ch->my_shop() || ch->is_cube_open())
	{
		ch->chat_packet(CHAT_TYPE_INFO,  lc_text("창고,거래창등이 열린 상태에서는 개량을 할수가 없습니다"));
		ch->clear_refine_mode();
		return;
	}

	if (p->type == 255)
	{
		// DoRefine Cancel
		ch->clear_refine_mode();
		return;
	}

	if (p->pos >= INVENTORY_MAX_NUM)
	{
		ch->clear_refine_mode();
		return;
	}

	item * item = ch->inventory_item(p->pos);

	if (!item)
	{
		ch->clear_refine_mode();
		return;
	}

	ch->set_refine_time();

	if (p->type == REFINE_TYPE_NORMAL)
	{
		metin2::log::info( "refine_type_noraml");
		ch->do_refine(item);
	}
	else if (p->type == REFINE_TYPE_SCROLL || p->type == REFINE_TYPE_HYUNIRON || p->type == REFINE_TYPE_MUSIN || p->type == REFINE_TYPE_BDRAGON)
	{
		metin2::log::info( "refine_type_scroll, ...");
		ch->do_refine_with_scroll(item);
	}
	else if (p->type == REFINE_TYPE_MONEY_ONLY)
	{
		game::items::item * const item = ch->inventory_item(p->pos);

		if (nullptr != item)
		{
			if (500 <= item->refine_set())
			{
				log_manager::instance().hack_log("DEVIL_TOWER_REFINE_HACK", ch);
			}
			else
			{
				if (ch->quest_flag("deviltower_zone.can_refine"))
				{
					ch->do_refine(item, true);
					ch->set_quest_flag("deviltower_zone.can_refine", 0);
				}
				else
				{
					ch->chat_packet(CHAT_TYPE_INFO, "사귀 타워 완료 보상은 한번까지 사용가능합니다.");
				}
			}
		}
	}

	ch->clear_refine_mode();
}

int input_main::analyze(desc * d, uint8_t bHeader, const char * c_pData)
{
	character * ch;

	if (!(ch = d->get_character()))
	{
		metin2::log::error("no character on desc");
		d->set_phase(PHASE_CLOSE);
		return (0);
	}

	if (ch && ch->is_pc())
	{
		if (get_global_time() < ch->analyze_protect)
		{
			ch->analyze_protect_count = ch->analyze_protect_count + 1;
			if (metin2::config::test_server())
			{
				ch->chat_packet(CHAT_TYPE_INFO, "<test server> analyze_protect_count. Count %d", ch->analyze_protect_count);
			}

			if (ch->analyze_protect_count >= 250)
			{
				ch->analyze_protect_count = 0;
				d->set_phase(PHASE_CLOSE);
				return (0);
			}
		}
		else
			ch->analyze_protect_count = 0;

		ch->analyze_protect = get_global_time() + 1;
	}

	int iExtraLen = 0;

	if (metin2::config::test_server() && bHeader != HEADER_CG_MOVE)
		metin2::log::info( "CInputMain::Analyze() ==> Header [{:d}] ", bHeader);

	switch (bHeader)
	{
		case HEADER_CG_PONG:
			pong(d); 
			break;

		case HEADER_CG_TIME_SYNC:
			handshake(d, c_pData);
			break;

		case HEADER_CG_CHAT:
			if (metin2::config::test_server())
			{
				char* pBuf = (char*)c_pData;
				metin2::log::info( "{}", pBuf + sizeof(packet_cg_chat));
			}

			if ((iExtraLen = chat(ch, c_pData, buffer_left_)) < 0)
				return -1;
			break;

		case HEADER_CG_WHISPER:
			if ((iExtraLen = whisper(ch, c_pData, buffer_left_)) < 0)
				return -1;
			break;

		case HEADER_CG_MOVE:
			move(ch, c_pData);

			if (check_client_version_enabled)
			{
				int version_value = atoi(client_version.c_str());
				int date	= atoi(d->client_version());

				if (version_value != date) // Fix
				{
					ch->chat_packet(CHAT_TYPE_NOTICE, lc_text("클라이언트 버전이 틀려 로그아웃 됩니다. 정상적으로 패치 후 접속하세요."));
					d->delayed_disconnect(10);
					log_manager::instance().hack_log("VERSION_CONFLICT", d->get_account_table().login, ch->name(), d->host_name());
				}
			}
			break;

		case HEADER_CG_CHARACTER_POSITION:
			position(ch, c_pData);
			break;

		case HEADER_CG_ITEM_USE:
			if (!ch->is_observer_mode())
				item_use(ch, c_pData);
			break;

		case HEADER_CG_ITEM_DROP:
			if (!ch->is_observer_mode())
			{
				item_drop(ch, c_pData);
			}
			break;

		case HEADER_CG_ITEM_DROP2:
			if (!ch->is_observer_mode())
				item_drop2(ch, c_pData);
			break;

		case HEADER_CG_ITEM_MOVE:
			if (!ch->is_observer_mode())
				item_move(ch, c_pData);
			break;

		case HEADER_CG_ITEM_PICKUP:
			if (!ch->is_observer_mode())
				item_pickup(ch, c_pData);
			break;

		case HEADER_CG_ITEM_USE_TO_ITEM:
			if (!ch->is_observer_mode())
				item_to_item(ch, c_pData);
			break;

		case HEADER_CG_ITEM_GIVE:
			if (!ch->is_observer_mode())
				item_give(ch, c_pData);
			break;

		case HEADER_CG_EXCHANGE:
			if (!ch->is_observer_mode())
				exchange(ch, c_pData);
			break;

		case HEADER_CG_ATTACK:
		case HEADER_CG_SHOOT:
			if (!ch->is_observer_mode())
			{
				attack(ch, bHeader, c_pData);
			}
			break;

		case HEADER_CG_USE_SKILL:
			if (!ch->is_observer_mode())
				use_skill(ch, c_pData);
			break;

		case HEADER_CG_QUICKSLOT_ADD:
			quickslot_add(ch, c_pData);
			break;

		case HEADER_CG_QUICKSLOT_DEL:
			quickslot_delete(ch, c_pData);
			break;

		case HEADER_CG_QUICKSLOT_SWAP:
			quickslot_swap(ch, c_pData);
			break;

		case HEADER_CG_SHOP:
			if ((iExtraLen = shop(ch, c_pData, buffer_left_)) < 0)
				return -1;
			break;

		case HEADER_CG_MESSENGER:
			if ((iExtraLen = messenger(ch, c_pData, buffer_left_))<0)
				return -1;
			break;

		case HEADER_CG_ON_CLICK:
			on_click(ch, c_pData);
			break;

		case HEADER_CG_SYNC_POSITION:
			if ((iExtraLen = sync_position(ch, c_pData, buffer_left_)) < 0)
				return -1;
			break;

		case HEADER_CG_ADD_FLY_TARGETING:
		case HEADER_CG_FLY_TARGETING:
			fly_target(ch, c_pData, bHeader);
			break;

		case HEADER_CG_SCRIPT_BUTTON:
			script_button(ch, c_pData);
			break;

			// SCRIPT_SELECT_ITEM
		case HEADER_CG_SCRIPT_SELECT_ITEM:
			script_select_item(ch, c_pData);
			break;
			// END_OF_SCRIPT_SELECT_ITEM

		case HEADER_CG_SCRIPT_ANSWER:
			script_answer(ch, c_pData);
			break;

		case HEADER_CG_QUEST_INPUT_STRING:
			quest_input_string(ch, c_pData);
			break;

		case HEADER_CG_QUEST_CONFIRM:
			quest_confirm(ch, c_pData);
			break;

		case HEADER_CG_TARGET:
			target(ch, c_pData);
			break;

		case HEADER_CG_WARP:
			warp(ch, c_pData);
			break;

		case HEADER_CG_SAFEBOX_CHECKIN:
			safebox_checkin(ch, c_pData);
			break;

		case HEADER_CG_SAFEBOX_CHECKOUT:
			safebox_checkout(ch, c_pData, false);
			break;

		case HEADER_CG_SAFEBOX_ITEM_MOVE:
			safebox_item_move(ch, c_pData);
			break;

		case HEADER_CG_MALL_CHECKOUT:
			safebox_checkout(ch, c_pData, true);
			break;

		case HEADER_CG_PARTY_INVITE:
			party_invite(ch, c_pData);
			break;

		case HEADER_CG_PARTY_REMOVE:
			party_remove(ch, c_pData);
			break;

		case HEADER_CG_PARTY_INVITE_ANSWER:
			party_invite_answer(ch, c_pData);
			break;

		case HEADER_CG_PARTY_SET_STATE:
			party_set_state(ch, c_pData);
			break;

		case HEADER_CG_PARTY_USE_SKILL:
			party_use_skill(ch, c_pData);
			break;

		case HEADER_CG_PARTY_PARAMETER:
			party_parameter(ch, c_pData);
			break;

		case HEADER_CG_ANSWER_MAKE_GUILD:
			answer_make_guild(ch, c_pData);
			break;

		case HEADER_CG_GUILD:
			if ((iExtraLen = guild(ch, c_pData, buffer_left_)) < 0)
				return -1;
			break;

		case HEADER_CG_FISHING:
			fishing(ch, c_pData);
			break;

		case HEADER_CG_HACK:
			hack(ch, c_pData);
			break;

		case HEADER_CG_MYSHOP:
			if ((iExtraLen = my_shop(ch, c_pData, buffer_left_)) < 0)
				return -1;
			break;

		case HEADER_CG_REFINE:
			refine(ch, c_pData);
			break;

		case HEADER_CG_CLIENT_VERSION:
			version(ch, c_pData);
			break;

		case HEADER_CG_DRAGON_SOUL_REFINE:
			{
				packet_cg_dragon_soul_refine* p = reinterpret_cast <packet_cg_dragon_soul_refine*>((void*)c_pData);
				switch(p->sub_type)
				{
				case DS_SUB_HEADER_CLOSE:
					ch->dragon_soul_refine_window_close();
					break;
				case DS_SUB_HEADER_DO_REFINE_GRADE:
					{
						ds_manager::instance().do_refine_grade(ch, p->item_grid);
					}
					break;
				case DS_SUB_HEADER_DO_REFINE_STEP:
					{
						ds_manager::instance().do_refine_step(ch, p->item_grid);
					}
					break;
				case DS_SUB_HEADER_DO_REFINE_STRENGTH:
					{
						ds_manager::instance().do_refine_strength(ch, p->item_grid);
					}
					break;
				}
			}

			break;
	}
	return (iExtraLen);
}
}
