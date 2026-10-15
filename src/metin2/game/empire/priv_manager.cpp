#include <metin2/game/empire/priv_manager.h>

#include <metin2/game/characters/char.h>
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

static const char * GetEmpireName(int priv)
{
	return LC_TEXT(c_apszEmpireNames[priv]);
}

static const char * GetPrivName(int priv)
{
	return LC_TEXT(c_apszPrivNames[priv]);
}

CPrivManager::CPrivManager()
{
	memset(m_aakPrivEmpireData, 0, sizeof(m_aakPrivEmpireData));
}

void CPrivManager::RequestGiveGuildPriv(uint32_t guild_id, uint8_t type, int value, time_t duration_sec)
{
	if (MAX_PRIV_NUM <= type)
	{
		metin2::log::error("PRIV_MANAGER: RequestGiveGuildPriv: wrong guild priv type({:d})", type);
		return;
	}

	value = std::clamp(value, 0, 1000);
	duration_sec = std::clamp<int>(duration_sec, 0, 60*60*24*7);

	TPacketGiveGuildPriv p;
	p.type = type;
	p.value = value;
	p.guild_id = guild_id;
	p.duration_sec = duration_sec;

	db_clientdesc->DBPacket(HEADER_GD_REQUEST_GUILD_PRIV, 0, &p, sizeof(p));
}

void CPrivManager::RequestGiveEmpirePriv(uint8_t empire, uint8_t type, int value, time_t duration_sec)
{
	if (MAX_PRIV_NUM <= type)
	{
		metin2::log::error("PRIV_MANAGER: RequestGiveEmpirePriv: wrong empire priv type({:d})", type);
		return;
	}

	value = std::clamp(value, 0, 1000);
	duration_sec = std::clamp<int>(duration_sec, 0, 60*60*24*7);

	TPacketGiveEmpirePriv p;
	p.type = type;
	p.value = value;
	p.empire = empire;
	p.duration_sec = duration_sec;

	db_clientdesc->DBPacket(HEADER_GD_REQUEST_EMPIRE_PRIV, 0, &p, sizeof(p));
}

void CPrivManager::RequestGiveCharacterPriv(uint32_t pid, uint8_t type, int value)
{
	if (MAX_PRIV_NUM <= type)
	{
		metin2::log::error("PRIV_MANAGER: RequestGiveCharacterPriv: wrong char priv type({:d})", type);
		return;
	}

	value = std::clamp(value, 0, 1000);

	TPacketGiveCharacterPriv p;
	p.type = type;
	p.value = value;
	p.pid = pid;

	db_clientdesc->DBPacket(HEADER_GD_REQUEST_CHARACTER_PRIV, 0, &p, sizeof(p));
}

void CPrivManager::GiveGuildPriv(uint32_t guild_id, uint8_t type, int value, uint8_t bLog, time_t end_time_sec)
{
	if (MAX_PRIV_NUM <= type)
	{
		metin2::log::error("PRIV_MANAGER: GiveGuildPriv: wrong guild priv type({:d})", type);
		return;
	}

	metin2::log::info("Set Guild Priv: guild_id({:d}) type({:d}) value({:d}) duration_sec({:d})", guild_id, type, value, end_time_sec - get_global_time());

	value = std::clamp(value, 0, 1000);
	end_time_sec = std::clamp<int>(end_time_sec, 0, get_global_time()+60*60*24*7);

	m_aPrivGuild[type][guild_id].value = value;
	m_aPrivGuild[type][guild_id].end_time_sec = end_time_sec;

	CGuild* g = CGuildManager::instance().FindGuild(guild_id);

	if (g)
	{
		if (value)
		{
			char buf[100];
			snprintf(buf, sizeof(buf), LC_TEXT("%s 길드의 %s이 %d%% 증가했습니다!"), g->GetName(), GetPrivName(type), value);
			SendNotice(buf);
		}
		else
		{
			char buf[100];
			snprintf(buf, sizeof(buf), LC_TEXT("%s 길드의 %s이 정상으로 돌아왔습니다."), g->GetName(), GetPrivName(type));
			SendNotice(buf);
		}

		if (bLog)
		{
			LogManager::instance().CharLog(0, guild_id, type, value, "GUILD_PRIV", "", "");
		}
	}
}

void CPrivManager::GiveCharacterPriv(uint32_t pid, uint8_t type, int value, uint8_t bLog)
{
	if (MAX_PRIV_NUM <= type)
	{
		metin2::log::error("PRIV_MANAGER: GiveCharacterPriv: wrong char priv type({:d})", type);
		return;
	}

	metin2::log::info("Set Character Priv {:d} {:d} {:d}", pid, type, value);

	value = std::clamp(value, 0, 1000);

	m_aPrivChar[type][pid] = value;

	if (bLog)
		LogManager::instance().CharLog(pid, 0, type, value, "CHARACTER_PRIV", "", "");
}

void CPrivManager::GiveEmpirePriv(uint8_t empire, uint8_t type, int value, uint8_t bLog, time_t end_time_sec)
{
	if (MAX_PRIV_NUM <= type)
	{
		metin2::log::error("PRIV_MANAGER: GiveEmpirePriv: wrong empire priv type({:d})", type);
		return;
	}

	metin2::log::info( "Set Empire Priv: empire({:d}) type({:d}) value({:d}) duration_sec({:d})", empire, type, value, end_time_sec-get_global_time());

	value = std::clamp(value, 0, 1000);
	end_time_sec = std::clamp<int>(end_time_sec, 0, get_global_time()+60*60*24*7);

	SPrivEmpireData& rkPrivEmpireData=m_aakPrivEmpireData[type][empire];
	rkPrivEmpireData.m_value = value;
	rkPrivEmpireData.m_end_time_sec = end_time_sec;

	if (value)
	{
		char buf[100];
		snprintf(buf, sizeof(buf), LC_TEXT("%s의 %s이 %d%% 증가했습니다!"), GetEmpireName(empire), GetPrivName(type), value);

		if (empire)
			SendNotice(buf);
		else
			SendLog(buf);
	}
	else
	{
		char buf[100];
		snprintf(buf, sizeof(buf), LC_TEXT("%s의 %s이 정상으로 돌아왔습니다."), GetEmpireName(empire), GetPrivName(type));

		if (empire)
			SendNotice(buf);
		else
			SendLog(buf);
	}

	if (bLog)
	{
		LogManager::instance().CharLog(0, empire, type, value, "EMPIRE_PRIV", "", "");
	}
}

int CPrivManager::GetPriv(CHARACTER * ch, uint8_t type)
{
	// If the character's modifier is negative, always apply only the negative
	int val_ch = GetPrivByCharacter(ch->GetPlayerID(), type);

	if (val_ch < 0 && !ch->IsEquipUniqueItem(UNIQUE_ITEM_NO_BAD_LUCK_EFFECT))
		return val_ch;
	else
	{
		int val;

		// Takes the largest of personal, empire, guild and global values.
		val = std::max(val_ch, GetPrivByEmpire(0, type));
		val = std::max(val, GetPrivByEmpire(ch->GetEmpire(), type));

		if (ch->GetGuild())
			val = std::max(val, GetPrivByGuild(ch->GetGuild()->GetID(), type));

		return val;
	}
}

int CPrivManager::GetPrivByEmpire(uint8_t bEmpire, uint8_t type)
{
	SPrivEmpireData* pkPrivEmpireData = GetPrivByEmpireEx(bEmpire, type);

	if (pkPrivEmpireData)
		return pkPrivEmpireData->m_value;

	return 0;
}

CPrivManager::SPrivEmpireData* CPrivManager::GetPrivByEmpireEx(uint8_t bEmpire, uint8_t type)
{
	if (type >= MAX_PRIV_NUM)
		return nullptr;

	if (bEmpire >= EMPIRE_MAX_NUM)
		return nullptr;

	return &m_aakPrivEmpireData[type][bEmpire];
}

int CPrivManager::GetPrivByGuild(uint32_t guild_id, uint8_t type)
{
	if (type >= MAX_PRIV_NUM)
		return 0;

	auto itFind = m_aPrivGuild[ type ].find( guild_id );

	if ( itFind == m_aPrivGuild[ type ].end() )
		return 0;

	return itFind->second.value;
}

const CPrivManager::SPrivGuildData* CPrivManager::GetPrivByGuildEx( uint32_t dwGuildID, uint8_t byType ) const
{
	if ( byType >= MAX_PRIV_NUM )
		return nullptr;

	auto itFind = m_aPrivGuild[ byType ].find( dwGuildID );

	if ( itFind == m_aPrivGuild[ byType ].end() )
		return nullptr;

	return &itFind->second;
}

int CPrivManager::GetPrivByCharacter(uint32_t pid, uint8_t type)
{
	if (type >= MAX_PRIV_NUM)
		return 0;

	auto it = m_aPrivChar[type].find(pid);

	if (it != m_aPrivChar[type].end())
		return it->second;

	return 0;
}
}
