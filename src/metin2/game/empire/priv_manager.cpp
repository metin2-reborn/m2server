#include <metin2/game/empire/priv_manager.h>

#include <metin2/game/characters/character.h>
#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/persistence/log.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/app/utils.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/log/log.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace metin2::game::inline empire {

static const char * get_empire_name(int priv)
{
	return lc_text(apsz_empire_names[priv]);
}

static const char * get_priv_name(int priv)
{
	return lc_text(apsz_priv_names[priv]);
}

priv_manager::priv_manager()
{
	memset(aak_priv_empire_data_, 0, sizeof(aak_priv_empire_data_));
}

void priv_manager::request_give_guild_priv(uint32_t guild_id, uint8_t type, int value, time_t duration_sec)
{
	if (MAX_PRIV_NUM <= type)
	{
		metin2::log::error("PRIV_MANAGER: RequestGiveGuildPriv: wrong guild priv type({:d})", type);
		return;
	}

	value = std::clamp(value, 0, 1000);
	duration_sec = std::clamp<int>(duration_sec, 0, 60*60*24*7);

	packet_give_guild_priv p;
	p.type = type;
	p.value = value;
	p.guild_id = guild_id;
	p.duration_sec = duration_sec;

	db_clientdesc->db_packet(HEADER_GD_REQUEST_GUILD_PRIV, 0, &p, sizeof(p));
}

void priv_manager::request_give_empire_priv(uint8_t empire, uint8_t type, int value, time_t duration_sec)
{
	if (MAX_PRIV_NUM <= type)
	{
		metin2::log::error("PRIV_MANAGER: RequestGiveEmpirePriv: wrong empire priv type({:d})", type);
		return;
	}

	value = std::clamp(value, 0, 1000);
	duration_sec = std::clamp<int>(duration_sec, 0, 60*60*24*7);

	packet_give_empire_priv p;
	p.type = type;
	p.value = value;
	p.empire = empire;
	p.duration_sec = duration_sec;

	db_clientdesc->db_packet(HEADER_GD_REQUEST_EMPIRE_PRIV, 0, &p, sizeof(p));
}

void priv_manager::request_give_character_priv(uint32_t pid, uint8_t type, int value)
{
	if (MAX_PRIV_NUM <= type)
	{
		metin2::log::error("PRIV_MANAGER: RequestGiveCharacterPriv: wrong char priv type({:d})", type);
		return;
	}

	value = std::clamp(value, 0, 1000);

	packet_give_character_priv p;
	p.type = type;
	p.value = value;
	p.pid = pid;

	db_clientdesc->db_packet(HEADER_GD_REQUEST_CHARACTER_PRIV, 0, &p, sizeof(p));
}

void priv_manager::give_guild_priv(uint32_t guild_id, uint8_t type, int value, uint8_t bLog, time_t end_time_sec)
{
	if (MAX_PRIV_NUM <= type)
	{
		metin2::log::error("PRIV_MANAGER: GiveGuildPriv: wrong guild priv type({:d})", type);
		return;
	}

	metin2::log::info("Set Guild Priv: guild_id({:d}) type({:d}) value({:d}) duration_sec({:d})", guild_id, type, value, end_time_sec - get_global_time());

	value = std::clamp(value, 0, 1000);
	end_time_sec = std::clamp<int>(end_time_sec, 0, get_global_time()+60*60*24*7);

	priv_guild_[type][guild_id].value = value;
	priv_guild_[type][guild_id].end_time_sec = end_time_sec;

	guild* g = guild_manager::instance().find_guild(guild_id);

	if (g)
	{
		if (value)
		{
			char buf[100];
			snprintf(buf, sizeof(buf), lc_text("%s 길드의 %s이 %d%% 증가했습니다!"), g->name(), get_priv_name(type), value);
			send_notice(buf);
		}
		else
		{
			char buf[100];
			snprintf(buf, sizeof(buf), lc_text("%s 길드의 %s이 정상으로 돌아왔습니다."), g->name(), get_priv_name(type));
			send_notice(buf);
		}

		if (bLog)
		{
			log_manager::instance().char_log(0, guild_id, type, value, "GUILD_PRIV", "", "");
		}
	}
}

void priv_manager::give_character_priv(uint32_t pid, uint8_t type, int value, uint8_t bLog)
{
	if (MAX_PRIV_NUM <= type)
	{
		metin2::log::error("PRIV_MANAGER: GiveCharacterPriv: wrong char priv type({:d})", type);
		return;
	}

	metin2::log::info("Set Character Priv {:d} {:d} {:d}", pid, type, value);

	value = std::clamp(value, 0, 1000);

	priv_char_[type][pid] = value;

	if (bLog)
		log_manager::instance().char_log(pid, 0, type, value, "CHARACTER_PRIV", "", "");
}

void priv_manager::give_empire_priv(uint8_t empire, uint8_t type, int value, uint8_t bLog, time_t end_time_sec)
{
	if (MAX_PRIV_NUM <= type)
	{
		metin2::log::error("PRIV_MANAGER: GiveEmpirePriv: wrong empire priv type({:d})", type);
		return;
	}

	metin2::log::info( "Set Empire Priv: empire({:d}) type({:d}) value({:d}) duration_sec({:d})", empire, type, value, end_time_sec-get_global_time());

	value = std::clamp(value, 0, 1000);
	end_time_sec = std::clamp<int>(end_time_sec, 0, get_global_time()+60*60*24*7);

	priv_empire_data& rkPrivEmpireData=aak_priv_empire_data_[type][empire];
	rkPrivEmpireData.value = value;
	rkPrivEmpireData.end_time_sec = end_time_sec;

	if (value)
	{
		char buf[100];
		snprintf(buf, sizeof(buf), lc_text("%s의 %s이 %d%% 증가했습니다!"), get_empire_name(empire), get_priv_name(type), value);

		if (empire)
			send_notice(buf);
		else
			send_log(buf);
	}
	else
	{
		char buf[100];
		snprintf(buf, sizeof(buf), lc_text("%s의 %s이 정상으로 돌아왔습니다."), get_empire_name(empire), get_priv_name(type));

		if (empire)
			send_notice(buf);
		else
			send_log(buf);
	}

	if (bLog)
	{
		log_manager::instance().char_log(0, empire, type, value, "EMPIRE_PRIV", "", "");
	}
}

int priv_manager::priv(character * ch, uint8_t type)
{
	// If the character's modifier is negative, always apply only the negative
	int val_ch = priv_by_character(ch->player_id(), type);

	if (val_ch < 0 && !ch->is_equip_unique_item(UNIQUE_ITEM_NO_BAD_LUCK_EFFECT))
		return val_ch;
	else
	{
		int val;

		// Takes the largest of personal, empire, guild and global values.
		val = std::max(val_ch, priv_by_empire(0, type));
		val = std::max(val, priv_by_empire(ch->empire(), type));

		if (ch->get_guild())
			val = std::max(val, priv_by_guild(ch->get_guild()->id(), type));

		return val;
	}
}

int priv_manager::priv_by_empire(uint8_t bEmpire, uint8_t type)
{
	priv_empire_data* pkPrivEmpireData = priv_by_empire_ex(bEmpire, type);

	if (pkPrivEmpireData)
		return pkPrivEmpireData->value;

	return 0;
}

priv_manager::priv_empire_data* priv_manager::priv_by_empire_ex(uint8_t bEmpire, uint8_t type)
{
	if (type >= MAX_PRIV_NUM)
		return nullptr;

	if (bEmpire >= EMPIRE_MAX_NUM)
		return nullptr;

	return &aak_priv_empire_data_[type][bEmpire];
}

int priv_manager::priv_by_guild(uint32_t guild_id, uint8_t type)
{
	if (type >= MAX_PRIV_NUM)
		return 0;

	auto itFind = priv_guild_[ type ].find( guild_id );

	if ( itFind == priv_guild_[ type ].end() )
		return 0;

	return itFind->second.value;
}

const priv_manager::priv_guild_data* priv_manager::priv_by_guild_ex( uint32_t dwGuildID, uint8_t byType ) const
{
	if ( byType >= MAX_PRIV_NUM )
		return nullptr;

	auto itFind = priv_guild_[ byType ].find( dwGuildID );

	if ( itFind == priv_guild_[ byType ].end() )
		return nullptr;

	return &itFind->second;
}

int priv_manager::priv_by_character(uint32_t pid, uint8_t type)
{
	if (type >= MAX_PRIV_NUM)
		return 0;

	auto it = priv_char_[type].find(pid);

	if (it != priv_char_[type].end())
		return it->second;

	return 0;
}
}
