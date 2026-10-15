#include "log.h"
#include <metin2/log/log.h>

#include "char.h"
#include "config.h"
#include "desc.h"
#include "item.h"
#include "typedef.h"

#include <common/length.h>
#include <libsql/query.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>

namespace
{
	// Callers pass nullptr for an unknown text or IP; log those as an empty string.
	const char * or_empty(const char * text)
	{
		return text ? text : "";
	}
}

LogManager::LogManager() : m_bIsConnect(false)
{
}

LogManager::~LogManager()
{
}

bool LogManager::Connect(const char * host, const int port, const char * user, const char * pwd, const char * db)
{
	if (m_sql.Setup(host, user, pwd, db, g_stLocale.c_str(), false, port))
		m_bIsConnect = true;

	return m_bIsConnect;
}

void LogManager::Query(const char * c_pszFormat, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, c_pszFormat);
	vsnprintf(szQuery, sizeof(szQuery), c_pszFormat, args);
	va_end(args);

	Query(std::string(szQuery));
}

void LogManager::Query(const std::string & query)
{
	if (metin2::config::test_server())
		metin2::log::info( "LOG: {}", query.c_str());

	m_sql.AsyncQuery(query.c_str());
}

bool LogManager::IsConnected()
{
	return m_bIsConnect;
}

void LogManager::ItemLog(uint32_t dwPID, uint32_t x, uint32_t y, uint32_t dwItemID, const char * c_pszText, const char * c_pszHint, const char * c_pszIP, uint32_t dwVnum)
{
	Query(sql::format("INSERT DELAYED INTO log{} (type, time, who, x, y, what, how, hint, ip, vnum) VALUES('ITEM', NOW(), {}, {}, {}, {}, {}, {}, {}, {})",
			sql::raw(get_table_postfix()), dwPID, x, y, dwItemID, or_empty(c_pszText), c_pszHint, or_empty(c_pszIP), dwVnum));
}

void LogManager::ItemLog(CHARACTER * ch, CItem * item, const char * c_pszText, const char * c_pszHint)
{
	if (nullptr == ch || nullptr == item)
	{
		metin2::log::error("character or item nil (ch {} item {} text {})", static_cast<const void *>(ch), static_cast<const void *>(item), c_pszText);
		return;
	}

	ItemLog(ch->GetPlayerID(), ch->GetX(), ch->GetY(), item->GetID(),
			nullptr == c_pszText ? "" : c_pszText,
		   	c_pszHint, ch->GetDesc() ? ch->GetDesc()->GetHostName() : "",
		   	item->GetOriginalVnum());
}

void LogManager::ItemLog(CHARACTER * ch, int itemID, int itemVnum, const char * c_pszText, const char * c_pszHint)
{
	ItemLog(ch->GetPlayerID(), ch->GetX(), ch->GetY(), itemID, c_pszText, c_pszHint, ch->GetDesc() ? ch->GetDesc()->GetHostName() : "", itemVnum);
}

void LogManager::CharLog(uint32_t dwPID, uint32_t x, uint32_t y, uint32_t dwValue, const char * c_pszText, const char * c_pszHint, const char * c_pszIP)
{
	Query(sql::format("INSERT DELAYED INTO log{} (type, time, who, x, y, what, how, hint, ip) VALUES('CHARACTER', NOW(), {}, {}, {}, {}, {}, {}, {})",
			sql::raw(get_table_postfix()), dwPID, x, y, dwValue, or_empty(c_pszText), c_pszHint, or_empty(c_pszIP)));
}

void LogManager::CharLog(CHARACTER * ch, uint32_t dw, const char * c_pszText, const char * c_pszHint)
{
	if (ch)
		CharLog(ch->GetPlayerID(), ch->GetX(), ch->GetY(), dw, c_pszText, c_pszHint, ch->GetDesc() ? ch->GetDesc()->GetHostName() : "");
	else
		CharLog(0, 0, 0, dw, c_pszText, c_pszHint, "");
}

void LogManager::MoneyLog(uint8_t type, uint32_t vnum, int gold)
{
	if (type == MONEY_LOG_RESERVED || type >= MONEY_LOG_TYPE_MAX_NUM)
	{
		metin2::log::error("TYPE ERROR: type {:d} vnum {:d} gold {:d}", type, vnum, gold);
		return;
	}

	Query(sql::format("INSERT DELAYED INTO money_log{} VALUES (NOW(), {}, {}, {})", sql::raw(get_table_postfix()), type, vnum, gold));
}

void LogManager::HackLog(const char * c_pszHackName, const char * c_pszLogin, const char * c_pszName, const char * c_pszIP)
{
	Query(sql::format("INSERT INTO hack_log (time, login, name, ip, server, why) VALUES(NOW(), {}, {}, {}, {}, {})", c_pszLogin, c_pszName, c_pszIP, metin2::config::server().hostname, c_pszHackName));
}

void LogManager::HackLog(const char * c_pszHackName, CHARACTER * ch)
{
	if (ch->GetDesc())
	{
		HackLog(c_pszHackName, 
				ch->GetDesc()->GetAccountTable().login,
				ch->GetName(),
				ch->GetDesc()->GetHostName());
	}
}

void LogManager::GoldBarLog(uint32_t dwPID, uint32_t dwItemID, GOLDBAR_HOW eHow, const char* c_pszHint)
{
	const char * szHow;
	
	switch (eHow)
	{
		case PERSONAL_SHOP_BUY:
			szHow = "BUY";
			break;
			
		case PERSONAL_SHOP_SELL:
			szHow = "SELL";
			break;
			
		case SHOP_BUY:
			szHow = "SHOP_BUY";
			break;
			
		case SHOP_SELL:
			szHow = "SHOP_SELL";
			break;
			
		case EXCHANGE_TAKE:
			szHow = "EXCHANGE_TAKE";
			break;
			
		case EXCHANGE_GIVE:
			szHow = "EXCHANGE_GIVE";
			break;

		case QUEST:
			szHow = "QUEST";
			break;

		default:
			szHow = "";
			break;
	}
	
	Query(sql::format("INSERT DELAYED INTO goldlog{} (date, time, pid, what, how, hint) VALUES(CURDATE(), CURTIME(), {}, {}, {}, {})", 
			sql::raw(get_table_postfix()), dwPID, dwItemID, szHow, c_pszHint));
}

void LogManager::CubeLog(uint32_t dwPID, uint32_t x, uint32_t y, uint32_t item_vnum, uint32_t item_uid, int item_count, bool success)
{
	Query(sql::format("INSERT DELAYED INTO cube{} (pid, time, x, y, item_vnum, item_uid, item_count, success) "
			"VALUES({}, NOW(), {}, {}, {}, {}, {}, {})",
			sql::raw(get_table_postfix()), dwPID, x, y, item_vnum, item_uid, item_count, success?1:0));
}

void LogManager::SpeedHackLog(uint32_t pid, uint32_t x, uint32_t y, int hack_count)
{
	Query(sql::format("INSERT INTO speed_hack{} (pid, time, x, y, hack_count) "
			"VALUES({}, NOW(), {}, {}, {})",
			sql::raw(get_table_postfix()), pid, x, y, hack_count));
}

void LogManager::ChangeNameLog(uint32_t pid, const char *old_name, const char *new_name, const char *ip)
{
	Query(sql::format("INSERT DELAYED INTO change_name{} (pid, old_name, new_name, time, ip) "
			"VALUES({}, {}, {}, NOW(), {}) ",
			sql::raw(get_table_postfix()), pid, old_name, new_name, ip));
}

void LogManager::GMCommandLog(uint32_t dwPID, const char* szName, const char* szIP, uint8_t byChannel, const char* szCommand)
{
	Query(sql::format("INSERT DELAYED INTO command_log{} (userid, server, ip, port, username, command, date ) "
			"VALUES({}, 999, {}, {}, {}, {}, NOW()) ",
			sql::raw(get_table_postfix()), dwPID, szIP, byChannel, szName, szCommand));
}

void LogManager::RefineLog(uint32_t pid, const char* item_name, uint32_t item_id, int item_refine_level, int is_success, const char* how)
{
	Query(sql::format("INSERT INTO refinelog{} (pid, item_name, item_id, step, time, is_success, setType) VALUES({}, {}, {}, {}, NOW(), {}, {})",
			sql::raw(get_table_postfix()), pid, item_name, item_id, item_refine_level, is_success, how));
}


void LogManager::ShoutLog(uint8_t bChannel, uint8_t bEmpire, const char * pszText)
{
	Query(sql::format("INSERT INTO shout_log{} VALUES(NOW(), {}, {},{})", sql::raw(get_table_postfix()), bChannel, bEmpire, pszText));
}

void LogManager::LevelLog(CHARACTER * pChar, unsigned int level, unsigned int playhour)
{
	uint32_t aid = 0;

	if (nullptr != pChar->GetDesc())
	{
		aid = pChar->GetDesc()->GetAccountTable().id;
	}

	Query(sql::format("REPLACE INTO levellog{} (name, level, time, account_id, pid, playtime) VALUES({}, {}, NOW(), {}, {}, {})",
			sql::raw(get_table_postfix()), pChar->GetName(), level, aid, pChar->GetPlayerID(), playhour));
}

void LogManager::BootLog(const char * c_pszHostName, uint8_t bChannel)
{
	Query(sql::format("INSERT INTO bootlog (time, hostname, channel) VALUES(NOW(), {}, {})",
			c_pszHostName, bChannel));
}

void LogManager::FishLog(uint32_t dwPID, int prob_idx, int fish_id, int fish_level, uint32_t dwMiliseconds, uint32_t dwVnum, uint32_t dwValue)
{
	Query(sql::format("INSERT INTO fish_log{} VALUES(NOW(), {}, {}, {}, {}, {}, {}, {})",
			sql::raw(get_table_postfix()),
			dwPID,
			prob_idx,
			fish_id,
			fish_level,
			dwMiliseconds,
			dwVnum,
			dwValue));
}

void LogManager::QuestRewardLog(const char * c_pszQuestName, uint32_t dwPID, uint32_t dwLevel, int iValue1, int iValue2)
{
	Query(sql::format("INSERT INTO quest_reward_log{} VALUES({},{},{},2,{},{},NOW())", 
			sql::raw(get_table_postfix()), 
			c_pszQuestName,
			dwPID,
			dwLevel,
			iValue1, 
			iValue2));
}

void LogManager::DetailLoginLog(bool isLogin, CHARACTER * ch)
{
	if (nullptr == ch->GetDesc())
		return;

	if (true == isLogin)
	{
		Query(sql::format("INSERT INTO loginlog2(type, is_gm, login_time, channel, account_id, pid, ip, client_version) "
				"VALUES('INVALID', {}, NOW(), {}, {}, {}, inet_aton({}), {})",
				ch->IsGM() == true ? "Y" : "N",
				metin2::config::server().channel,
				ch->GetDesc()->GetAccountTable().id,
				ch->GetPlayerID(),
				ch->GetDesc()->GetHostName(),
				ch->GetDesc()->GetClientVersion()));
	}
	else
	{
		Query(sql::format("SET @i = (SELECT MAX(id) FROM loginlog2 WHERE account_id={} AND pid={})",
				ch->GetDesc()->GetAccountTable().id,
				ch->GetPlayerID()));

		Query("UPDATE loginlog2 SET type='VALID', logout_time=NOW(), playtime=TIMEDIFF(logout_time,login_time) WHERE id=@i");
	}
}

void LogManager::DragonSlayLog(uint32_t dwGuildID, uint32_t dwDragonVnum, uint32_t dwStartTime, uint32_t dwEndTime)
{
	Query(sql::format("INSERT INTO dragon_slay_log{} VALUES( {}, {}, FROM_UNIXTIME({}), FROM_UNIXTIME({}) )",
			sql::raw(get_table_postfix()),
			dwGuildID, dwDragonVnum, dwStartTime, dwEndTime));
}

		// Query( "INSERT INTO hackshield_log(time, account_id, login, pid, name, reason, ip) "
		// Query( "INSERT INTO hackshield_log(time, account_id, login, pid, name, reason, ip) "
				// "VALUES(NOW(), %u, '%s', %u, '%s', %u, inet_aton('%s'))",
				// ch->GetDesc()->GetAccountTable().id, ch->GetDesc()->GetAccountTable().login,
				// ch->GetPlayerID(), ch->GetName(),
				// ErrorCode,

