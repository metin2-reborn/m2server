#include <metin2/game/characters/affect.h>
#include <metin2/game/empire/castle.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/config.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/empire/empire_text_convert.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/protocol/input.h>
#include <metin2/game/social/messenger_manager.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/events/xmas_event.h>

#include <metin2/proto/length.h>
#include <metin2/log/log.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace metin2::game::inline protocol {


////////////////////////////////////////////////////////////////////////////////
// Input Processor
input_p2_p::input_p2_p()
{
	bind_packet_info(&packet_info_gg_);
}

void input_p2_p::login(desc * d, const char * c_pData)
{
	p2_p_manager::instance().login(d, (packet_gg_login *) c_pData);
}

void input_p2_p::logout(desc * d, const char * c_pData)
{
	packet_gg_logout * p = (packet_gg_logout *) c_pData;
	p2_p_manager::instance().logout(p->name);
}

int input_p2_p::relay(desc * d, const char * c_pData, size_t uiBytes)
{
	packet_gg_relay * p = (packet_gg_relay *) c_pData;

	if (uiBytes < sizeof(packet_gg_relay) + p->size)
		return -1;

	if (p->size < 0)
	{
		metin2::log::error("invalid packet length {:d}", p->size);
		d->set_phase(PHASE_CLOSE);
		return -1;
	}

	metin2::log::info( "InputP2P::Relay : {} size {:d}", p->name, p->size);

	character * pkChr = character_manager::instance().find_pc(p->name);

	const uint8_t* c_pbData = (const uint8_t *) (c_pData + sizeof(packet_gg_relay));

	if (!pkChr)
		return p->size;

	if (*c_pbData == HEADER_GC_WHISPER)
	{
		if (pkChr->is_block_mode(BLOCK_WHISPER))
		{
			// Whisper rejected because whispers are blocked.
			return p->size;
		}

		char buf[1024];
		memcpy(buf, c_pbData, std::min<int>(p->size, sizeof(buf)));

		packet_gc_whisper* p2 = (packet_gc_whisper*) buf;
		// bType upper 4 bits: Empire number
		// bType lower 4 bits: EWhisperType
		uint8_t bToEmpire = (p2->type >> 4);
		p2->type = p2->type & 0x0F;
		if(p2->type == 0x0F) {
			// System message whispers use all bits of bType, including the upper ones.
			p2->type = WHISPER_TYPE_SYSTEM;
		} else {
			if (!pkChr->is_equip_unique_group(UNIQUE_GROUP_RING_OF_LANGUAGE))
				if (bToEmpire >= 1 && bToEmpire <= 3 && pkChr->empire() != bToEmpire)
				{
					convert_empire_text(bToEmpire,
							buf + sizeof(packet_gc_whisper), 
							p2->size - sizeof(packet_gc_whisper),
							10+2*pkChr->skill_power(SKILL_LANGUAGE1 + bToEmpire - 1));
				}
		}

		pkChr->get_desc()->packet(buf, p->size);
	}
	else
		pkChr->get_desc()->packet(c_pbData, p->size);

	return (p->size);
}

int input_p2_p::notice(desc * d, const char * c_pData, size_t uiBytes)
{
	packet_gg_notice * p = (packet_gg_notice *) c_pData;

	if (uiBytes < sizeof(packet_gg_notice) + p->size)
		return -1;

	if (p->size < 0)
	{
		metin2::log::error("invalid packet length {:d}", p->size);
		d->set_phase(PHASE_CLOSE);
		return -1;
	}

	char szBuf[256+1];
	strlcpy(szBuf, c_pData + sizeof(packet_gg_notice), std::min<int>(p->size + 1, sizeof(szBuf)));
	send_notice(szBuf);
	return (p->size);
}

int input_p2_p::monarch_notice(desc * d, const char * c_pData, size_t uiBytes)
{
	packet_gg_monarch_notice * p = (packet_gg_monarch_notice *) c_pData;

	if (uiBytes < p->size + sizeof(packet_gg_monarch_notice))
		return -1;

	if (p->size < 0)
	{
		metin2::log::error("invalid packet length {:d}", p->size);
		d->set_phase(PHASE_CLOSE);
		return -1;
	}

	char szBuf[256+1];
	strlcpy(szBuf, c_pData + sizeof(packet_gg_monarch_notice), std::min<int>(p->size + 1, sizeof(szBuf)));
	send_monarch_notice(p->empire, szBuf);
	return (p->size);
}

int input_p2_p::monarch_transfer(desc * d, const char* c_pData)
{
	packet_monarch_gg_transfer* p = (packet_monarch_gg_transfer*) c_pData;
	character * pTargetChar = character_manager::instance().find_by_pid(p->target_pid);

	if (pTargetChar != nullptr)
	{
		unsigned int qIndex = quest::quest_manager::instance().quest_index_by_name("monarch_transfer");

		if (qIndex != 0)
		{
			pTargetChar->set_quest_flag("monarch_transfer.x", p->x);
			pTargetChar->set_quest_flag("monarch_transfer.y", p->y);
			quest::quest_manager::instance().letter(pTargetChar->player_id(), qIndex, 0);
		}
	}

	return 0;
}

int input_p2_p::guild(desc * d, const char* c_pData, size_t uiBytes)
{
	packet_gg_guild * p = (packet_gg_guild *) c_pData;
	uiBytes -= sizeof(packet_gg_guild);
	c_pData += sizeof(packet_gg_guild);

	game::guilds::guild * g = guild_manager::instance().find_guild(p->guild);

	switch (p->sub_header)
	{
		case GUILD_SUBHEADER_GG_CHAT:
			{
				if (uiBytes < sizeof(packet_gg_guild_chat))
					return -1;

				packet_gg_guild_chat * p = (packet_gg_guild_chat *) c_pData;

				if (g)
					g->p2_p_chat(p->text);

				return sizeof(packet_gg_guild_chat);
			}
			
		case GUILD_SUBHEADER_GG_SET_MEMBER_COUNT_BONUS:
			{
				if (uiBytes < sizeof(int))
					return -1;

				int iBonus = *((int *) c_pData);
				game::guilds::guild* pGuild = guild_manager::instance().find_guild(p->guild);
				if (pGuild)
				{
					pGuild->set_member_count_bonus(iBonus);
				}
				return sizeof(int);
			}
		default:
			metin2::log::error("UNKNOWN GUILD SUB PACKET");
			break;
	}
	return 0;
}


struct func_shout
{
	const char * str;
	uint8_t empire;

	func_shout(const char * str_value, uint8_t bEmpire) : str(str_value), empire(bEmpire)
	{
	}   

	void operator () (desc * d)
	{
		if (!d->get_character() || (d->get_character()->gm_level() == GM_PLAYER && d->empire() != empire))
			return;

		d->get_character()->chat_packet(CHAT_TYPE_SHOUT, "%s", str);
	}
};

void send_shout(const char * szText, uint8_t bEmpire)
{
	const desc_manager::desc_set & c_ref_set = desc_manager::instance().client_set();
	std::for_each(c_ref_set.begin(), c_ref_set.end(), func_shout(szText, bEmpire));
}

void input_p2_p::shout(const char * c_pData)
{
	packet_gg_shout * p = (packet_gg_shout *) c_pData;
	send_shout(p->text, p->empire);
}

void input_p2_p::disconnect(const char * c_pData)
{
	packet_gg_disconnect * p = (packet_gg_disconnect *) c_pData;

	desc * d = desc_manager::instance().find_by_login_name(p->login);

	if (!d)
		return;

	if (!d->get_character())
	{
		d->set_phase(PHASE_CLOSE);
	}
	else
		d->disconnect_of_same_login();
}

void input_p2_p::setup(desc * d, const char * c_pData)
{
	packet_gg_setup * p = (packet_gg_setup *) c_pData;
	metin2::log::info( "P2P: Setup {}:{:d}", d->host_name(), p->port);
	d->set_p2_p(d->host_name(), p->port);
}

void input_p2_p::messenger_add(const char * c_pData)
{
	packet_gg_messenger * p = (packet_gg_messenger *) c_pData;
	metin2::log::info( "P2P: Messenger Add {} {}", p->account, p->companion);
	messenger_manager::instance().apply_add_to_list(p->account, p->companion);
}

void input_p2_p::messenger_remove(const char * c_pData)
{
	packet_gg_messenger * p = (packet_gg_messenger *) c_pData;
	metin2::log::info( "P2P: Messenger Remove {} {}", p->account, p->companion);
	messenger_manager::instance().apply_remove_from_list(p->account, p->companion);
}

void input_p2_p::find_position(desc * d, const char* c_pData)
{
	packet_gg_find_position* p = (packet_gg_find_position*) c_pData;
	character * ch = character_manager::instance().find_by_pid(p->target_pid);
	if (ch && ch->map_index() < 10000)
	{
		packet_gg_warp_character pw;
		pw.header = HEADER_GG_WARP_CHARACTER;
		pw.pid = p->from_pid;
		pw.x = ch->x();
		pw.y = ch->y();
		d->packet(&pw, sizeof(pw));
	}
}

void input_p2_p::warp_character(const char* c_pData)
{
	packet_gg_warp_character* p = (packet_gg_warp_character*) c_pData;
	character * ch = character_manager::instance().find_by_pid(p->pid);
	if (ch)
	{
		ch->warp_set(p->x, p->y);
	}
}

void input_p2_p::guild_war_zone_map_index(const char* c_pData)
{
	packet_gg_guild_war_map_index * p = (packet_gg_guild_war_map_index*) c_pData;
	guild_manager & gm = guild_manager::instance();

	metin2::log::info( "P2P: GuildWarZoneMapIndex g1({:d}) vs g2({:d}), mapIndex({:d})", p->guild_i_d1, p->guild_i_d2, p->map_index);

	game::guilds::guild * g1 = gm.find_guild(p->guild_i_d1);
	game::guilds::guild * g2 = gm.find_guild(p->guild_i_d2);

	if (g1 && g2)
	{
		g1->set_guild_war_map_index(p->guild_i_d2, p->map_index);
		g2->set_guild_war_map_index(p->guild_i_d1, p->map_index);
	}
}

void input_p2_p::transfer(const char * c_pData)
{
	packet_gg_transfer * p = (packet_gg_transfer *) c_pData;

	character * ch = character_manager::instance().find_pc(p->name);

	if (ch)
		ch->warp_set(p->x, p->y);
}

void input_p2_p::xmas_warp_santa(const char * c_pData)
{
	packet_gg_xmas_warp_santa * p =(packet_gg_xmas_warp_santa *) c_pData;

	if (p->channel == metin2::config::server().channel && map_allow_find(p->map_index))
	{
		int	iNextSpawnDelay = 60;

		iNextSpawnDelay = 50 * 60;

		xmas::spawn_santa(p->map_index, iNextSpawnDelay); // A new Santa appears after 50 minutes (20 minutes in Korea)

		packet_gg_xmas_warp_santa_reply pack_reply;
		pack_reply.header = HEADER_GG_XMAS_WARP_SANTA_REPLY;
		pack_reply.channel = metin2::config::server().channel;
		p2_p_manager::instance().send(&pack_reply, sizeof(pack_reply));
	}
}

void input_p2_p::xmas_warp_santa_reply(const char* c_pData)
{
	packet_gg_xmas_warp_santa_reply* p = (packet_gg_xmas_warp_santa_reply*) c_pData;

	if (p->channel == metin2::config::server().channel)
	{
		character_vector_interactor i;

		if (character_manager::instance().characters_by_race_num(xmas::MOB_SANTA_VNUM, i))
		{
			character_vector_interactor::iterator it = i.begin();

			while (it != i.end()) {
				character_manager::instance().destroy_character(*it++);
			}
		}
	}
}

// BLOCK_CHAT
void input_p2_p::block_chat(const char * c_pData)
{
	packet_gg_block_chat * p = (packet_gg_block_chat *) c_pData;

	character * ch = character_manager::instance().find_pc(p->name);

	if (ch)
	{
		metin2::log::info( "BLOCK CHAT apply name {} dur {:d}", p->name, p->block_duration);
		ch->add_affect(AFFECT_BLOCK_CHAT, POINT_NONE, 0, AFF_NONE, p->block_duration, 0, true);
	}
	else
	{
		metin2::log::info( "BLOCK CHAT fail name {} dur {:d}", p->name, p->block_duration);
	}
}   
// END_OF_BLOCK_CHAT
//

void input_p2_p::iam_awake(desc * d, const char * c_pData)
{
	std::string hostNames;
	p2_p_manager::instance().p2_p_host_names(hostNames);
	metin2::log::info( "P2P Awakeness check from {}. My P2P connection number is {:d}. and details...\n{}", d->host_name(), p2_p_manager::instance().desc_count(), hostNames.c_str());
}

int input_p2_p::analyze(desc * d, uint8_t bHeader, const char * c_pData)
{
	if (metin2::config::test_server())
		metin2::log::info( "CInputP2P::Anlayze[Header {:d}]", bHeader);

	int iExtraLen = 0;

	switch (bHeader)
	{
		case HEADER_GG_SETUP:
			setup(d, c_pData);
			break;

		case HEADER_GG_LOGIN:
			login(d, c_pData);
			break;

		case HEADER_GG_LOGOUT:
			logout(d, c_pData);
			break;

		case HEADER_GG_RELAY:
			if ((iExtraLen = relay(d, c_pData, buffer_left_)) < 0)
				return -1;
			break;

		case HEADER_GG_NOTICE:
			if ((iExtraLen = notice(d, c_pData, buffer_left_)) < 0)
				return -1;
			break;

		case HEADER_GG_SHUTDOWN:
			metin2::log::error("Accept shutdown p2p command from {}.", d->host_name());
			shutdown(10);
			break;

		case HEADER_GG_GUILD:
			if ((iExtraLen = guild(d, c_pData, buffer_left_)) < 0)
				return -1;
			break;

		case HEADER_GG_SHOUT:
			shout(c_pData);
			break;

		case HEADER_GG_DISCONNECT:
			disconnect(c_pData);
			break;

		case HEADER_GG_MESSENGER_ADD:
			messenger_add(c_pData);
			break;

		case HEADER_GG_MESSENGER_REMOVE:
			messenger_remove(c_pData);
			break;

		case HEADER_GG_FIND_POSITION:
			find_position(d, c_pData);
			break;

		case HEADER_GG_WARP_CHARACTER:
			warp_character(c_pData);
			break;

		case HEADER_GG_GUILD_WAR_ZONE_MAP_INDEX:
			guild_war_zone_map_index(c_pData);
			break;

		case HEADER_GG_TRANSFER:
			transfer(c_pData);
			break;

		case HEADER_GG_XMAS_WARP_SANTA:
			xmas_warp_santa(c_pData);
			break;

		case HEADER_GG_XMAS_WARP_SANTA_REPLY:
			xmas_warp_santa_reply(c_pData);
			break;

		case HEADER_GG_CHECK_CLIENT_VERSION:
			check_client_version();
			break;

		case HEADER_GG_BLOCK_CHAT:
			block_chat(c_pData);
			break;

		case HEADER_GG_SIEGE:
			{
				packet_gg_siege* pSiege = (packet_gg_siege*)c_pData;
				castle_siege(pSiege->empire, pSiege->tower_count);
			}
			break;

		case HEADER_GG_MONARCH_NOTICE:
			if ((iExtraLen = monarch_notice(d, c_pData, buffer_left_)) < 0)
				return -1;
			break;

		case HEADER_GG_MONARCH_TRANSFER :
			monarch_transfer(d, c_pData);
			break;

		case HEADER_GG_CHECK_AWAKENESS:
			iam_awake(d, c_pData);
			break;
	}

	return (iExtraLen);
}
}
