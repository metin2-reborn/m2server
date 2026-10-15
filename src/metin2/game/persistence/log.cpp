#include <metin2/game/persistence/log.h>
#include <metin2/log/log.h>

#include <metin2/game/characters/character.h>
#include <metin2/game/app/config.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/items/item.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/text/locale_service.h>

#include <metin2/proto/length.h>
#include <metin2/sql/query.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>

namespace metin2::game::inline persistence {

namespace
{
	// Callers pass nullptr for an unknown text or IP; log those as an empty string.
	const char * or_empty(const char * text)
	{
		return text ? text : "";
	}
}

log_manager::log_manager() : is_connect_(false)
{
}

log_manager::~log_manager()
{
}

bool log_manager::connect(const char * host, const int port, const char * user, const char * pwd, const char * db)
{
	if (sql_.setup(host, user, pwd, db, locale.c_str(), false, port))
		is_connect_ = true;

	return is_connect_;
}

void log_manager::query(const char * c_pszFormat, ...)
{
	char szQuery[4096];
	va_list args;

	va_start(args, c_pszFormat);
	vsnprintf(szQuery, sizeof(szQuery), c_pszFormat, args);
	va_end(args);

	query(std::string(szQuery));
}

void log_manager::query(const std::string & query)
{
	if (metin2::config::test_server())
		metin2::log::info( "LOG: {}", query.c_str());

	sql_.async_query(query.c_str());
}

bool log_manager::is_connected()
{
	return is_connect_;
}

void log_manager::item_log(uint32_t dwPID, uint32_t x, uint32_t y, uint32_t dwItemID, const char * c_pszText, const char * c_pszHint, const char * c_pszIP, uint32_t dwVnum)
{
	query(sql::format("INSERT DELAYED INTO log{} (type, time, who, x, y, what, how, hint, ip, vnum) VALUES('ITEM', NOW(), {}, {}, {}, {}, {}, {}, {}, {})",
			sql::raw(get_table_postfix()), dwPID, x, y, dwItemID, or_empty(c_pszText), c_pszHint, or_empty(c_pszIP), dwVnum));
}

void log_manager::item_log(character * ch, item * item, const char * c_pszText, const char * c_pszHint)
{
	if (nullptr == ch || nullptr == item)
	{
		metin2::log::error("character or item nil (ch {} item {} text {})", static_cast<const void *>(ch), static_cast<const void *>(item), c_pszText);
		return;
	}

	item_log(ch->player_id(), ch->x(), ch->y(), item->id(),
			nullptr == c_pszText ? "" : c_pszText,
		   	c_pszHint, ch->get_desc() ? ch->get_desc()->host_name() : "",
		   	item->original_vnum());
}

void log_manager::item_log(character * ch, int itemID, int itemVnum, const char * c_pszText, const char * c_pszHint)
{
	item_log(ch->player_id(), ch->x(), ch->y(), itemID, c_pszText, c_pszHint, ch->get_desc() ? ch->get_desc()->host_name() : "", itemVnum);
}

void log_manager::char_log(uint32_t dwPID, uint32_t x, uint32_t y, uint32_t dwValue, const char * c_pszText, const char * c_pszHint, const char * c_pszIP)
{
	query(sql::format("INSERT DELAYED INTO log{} (type, time, who, x, y, what, how, hint, ip) VALUES('CHARACTER', NOW(), {}, {}, {}, {}, {}, {}, {})",
			sql::raw(get_table_postfix()), dwPID, x, y, dwValue, or_empty(c_pszText), c_pszHint, or_empty(c_pszIP)));
}

void log_manager::char_log(character * ch, uint32_t dw, const char * c_pszText, const char * c_pszHint)
{
	if (ch)
		char_log(ch->player_id(), ch->x(), ch->y(), dw, c_pszText, c_pszHint, ch->get_desc() ? ch->get_desc()->host_name() : "");
	else
		char_log(0, 0, 0, dw, c_pszText, c_pszHint, "");
}

void log_manager::money_log(uint8_t type, uint32_t vnum, int gold)
{
	if (type == MONEY_LOG_RESERVED || type >= MONEY_LOG_TYPE_MAX_NUM)
	{
		metin2::log::error("TYPE ERROR: type {:d} vnum {:d} gold {:d}", type, vnum, gold);
		return;
	}

	query(sql::format("INSERT DELAYED INTO money_log{} VALUES (NOW(), {}, {}, {})", sql::raw(get_table_postfix()), type, vnum, gold));
}

void log_manager::hack_log(const char * c_pszHackName, const char * c_pszLogin, const char * c_pszName, const char * c_pszIP)
{
	query(sql::format("INSERT INTO hack_log (time, login, name, ip, server, why) VALUES(NOW(), {}, {}, {}, {}, {})", c_pszLogin, c_pszName, c_pszIP, metin2::config::server().hostname, c_pszHackName));
}

void log_manager::hack_log(const char * c_pszHackName, character * ch)
{
	if (ch->get_desc())
	{
		hack_log(c_pszHackName, 
				ch->get_desc()->get_account_table().login,
				ch->name(),
				ch->get_desc()->host_name());
	}
}

void log_manager::gold_bar_log(uint32_t dwPID, uint32_t dwItemID, goldbar_how eHow, const char* c_pszHint)
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
	
	query(sql::format("INSERT DELAYED INTO goldlog{} (date, time, pid, what, how, hint) VALUES(CURDATE(), CURTIME(), {}, {}, {}, {})", 
			sql::raw(get_table_postfix()), dwPID, dwItemID, szHow, c_pszHint));
}

void log_manager::cube_log(uint32_t dwPID, uint32_t x, uint32_t y, uint32_t item_vnum, uint32_t item_uid, int item_count, bool success)
{
	query(sql::format("INSERT DELAYED INTO cube{} (pid, time, x, y, item_vnum, item_uid, item_count, success) "
			"VALUES({}, NOW(), {}, {}, {}, {}, {}, {})",
			sql::raw(get_table_postfix()), dwPID, x, y, item_vnum, item_uid, item_count, success?1:0));
}

void log_manager::speed_hack_log(uint32_t pid, uint32_t x, uint32_t y, int hack_count)
{
	query(sql::format("INSERT INTO speed_hack{} (pid, time, x, y, hack_count) "
			"VALUES({}, NOW(), {}, {}, {})",
			sql::raw(get_table_postfix()), pid, x, y, hack_count));
}

void log_manager::change_name_log(uint32_t pid, const char *old_name, const char *new_name, const char *ip)
{
	query(sql::format("INSERT DELAYED INTO change_name{} (pid, old_name, new_name, time, ip) "
			"VALUES({}, {}, {}, NOW(), {}) ",
			sql::raw(get_table_postfix()), pid, old_name, new_name, ip));
}

void log_manager::gm_command_log(uint32_t dwPID, const char* szName, const char* szIP, uint8_t byChannel, const char* szCommand)
{
	query(sql::format("INSERT DELAYED INTO command_log{} (userid, server, ip, port, username, command, date ) "
			"VALUES({}, 999, {}, {}, {}, {}, NOW()) ",
			sql::raw(get_table_postfix()), dwPID, szIP, byChannel, szName, szCommand));
}

void log_manager::refine_log(uint32_t pid, const char* item_name, uint32_t item_id, int item_refine_level, int is_success, const char* how)
{
	query(sql::format("INSERT INTO refinelog{} (pid, item_name, item_id, step, time, is_success, setType) VALUES({}, {}, {}, {}, NOW(), {}, {})",
			sql::raw(get_table_postfix()), pid, item_name, item_id, item_refine_level, is_success, how));
}


void log_manager::shout_log(uint8_t bChannel, uint8_t bEmpire, const char * pszText)
{
	query(sql::format("INSERT INTO shout_log{} VALUES(NOW(), {}, {},{})", sql::raw(get_table_postfix()), bChannel, bEmpire, pszText));
}

void log_manager::level_log(character * pChar, unsigned int level, unsigned int playhour)
{
	uint32_t aid = 0;

	if (nullptr != pChar->get_desc())
	{
		aid = pChar->get_desc()->get_account_table().id;
	}

	query(sql::format("REPLACE INTO levellog{} (name, level, time, account_id, pid, playtime) VALUES({}, {}, NOW(), {}, {}, {})",
			sql::raw(get_table_postfix()), pChar->name(), level, aid, pChar->player_id(), playhour));
}

void log_manager::boot_log(const char * c_pszHostName, uint8_t bChannel)
{
	query(sql::format("INSERT INTO bootlog (time, hostname, channel) VALUES(NOW(), {}, {})",
			c_pszHostName, bChannel));
}

void log_manager::fish_log(uint32_t dwPID, int prob_idx, int fish_id, int fish_level, uint32_t dwMiliseconds, uint32_t dwVnum, uint32_t dwValue)
{
	query(sql::format("INSERT INTO fish_log{} VALUES(NOW(), {}, {}, {}, {}, {}, {}, {})",
			sql::raw(get_table_postfix()),
			dwPID,
			prob_idx,
			fish_id,
			fish_level,
			dwMiliseconds,
			dwVnum,
			dwValue));
}

void log_manager::quest_reward_log(const char * c_pszQuestName, uint32_t dwPID, uint32_t dwLevel, int iValue1, int iValue2)
{
	query(sql::format("INSERT INTO quest_reward_log{} VALUES({},{},{},2,{},{},NOW())", 
			sql::raw(get_table_postfix()), 
			c_pszQuestName,
			dwPID,
			dwLevel,
			iValue1, 
			iValue2));
}

void log_manager::detail_login_log(bool isLogin, character * ch)
{
	if (nullptr == ch->get_desc())
		return;

	if (true == isLogin)
	{
		query(sql::format("INSERT INTO loginlog2(type, is_gm, login_time, channel, account_id, pid, ip, client_version) "
				"VALUES('INVALID', {}, NOW(), {}, {}, {}, inet_aton({}), {})",
				ch->is_gm() == true ? "Y" : "N",
				metin2::config::server().channel,
				ch->get_desc()->get_account_table().id,
				ch->player_id(),
				ch->get_desc()->host_name(),
				ch->get_desc()->client_version()));
	}
	else
	{
		query(sql::format("SET @i = (SELECT MAX(id) FROM loginlog2 WHERE account_id={} AND pid={})",
				ch->get_desc()->get_account_table().id,
				ch->player_id()));

		query("UPDATE loginlog2 SET type='VALID', logout_time=NOW(), playtime=TIMEDIFF(logout_time,login_time) WHERE id=@i");
	}
}

void log_manager::dragon_slay_log(uint32_t dwGuildID, uint32_t dwDragonVnum, uint32_t dwStartTime, uint32_t dwEndTime)
{
	query(sql::format("INSERT INTO dragon_slay_log{} VALUES( {}, {}, FROM_UNIXTIME({}), FROM_UNIXTIME({}) )",
			sql::raw(get_table_postfix()),
			dwGuildID, dwDragonVnum, dwStartTime, dwEndTime));
}

		// Query( "INSERT INTO hackshield_log(time, account_id, login, pid, name, reason, ip) "
		// Query( "INSERT INTO hackshield_log(time, account_id, login, pid, name, reason, ip) "
				// "VALUES(NOW(), %u, '%s', %u, '%s', %u, inet_aton('%s'))",
				// ch->GetDesc()->GetAccountTable().id, ch->GetDesc()->GetAccountTable().login,
				// ch->GetPlayerID(), ch->GetName(),
				// ErrorCode,
}
