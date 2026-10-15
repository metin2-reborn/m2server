#include "config.h"
#include "db.h"
#include "desc.h"
#include "desc_manager.h"
#include "input.h"
#include "packet.h"
#include "typedef.h"

#include <common/length.h>
#include <libsql/query.h>
#include <metin2/log/log.h>
#include <libthecore/utils.h>

#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>

namespace metin2::game {


bool FN_IS_VALID_LOGIN_STRING(const char *str)
{
	const char*	tmp;

	if (!str || !*str)
		return false;

	if (strlen(str) < 2)
		return false;

	for (tmp = str; *tmp; ++tmp)
	{
		// Allow only letters and digits
		if (isdigit((unsigned char) *tmp) || isalpha((unsigned char) *tmp))
			continue;

		// Canada allows some special characters




		return false;
	}

	return true;
}

bool Login_IsInChannelService(const char* c_login)
{
	if (c_login[0] == '[')
		return true;
	return false;
}

CInputAuth::CInputAuth()
{
}

void CInputAuth::Login(DESC * d, const char * c_pData)
{

	TPacketCGLogin3 * pinfo = (TPacketCGLogin3 *) c_pData;

	if (!metin2::config::is_auth())
	{
		metin2::log::error("CInputAuth class is not for game server. IP {} might be a hacker.", 
			d->GetHostName());
		d->DelayedDisconnect(5);
		return;
	}

	// One login attempt per connection (the client sends one): another would replace the
	// login key of a query still in flight, and allow password guessing at line rate.
	if (d->GetLoginKey() || d->GetAccountTable().id)
	{
		metin2::log::info( "InputAuth::Login : second login attempt from {}", d->GetHostName());
		d->SetPhase(PHASE_CLOSE);
		return;
	}

	// Copy for string integrity (the fields may lack their NUL)
	char szLogin[sizeof(pinfo->login)];
	CopyPacketString(szLogin, pinfo->login);

	char login[LOGIN_MAX_LEN + 1];
	trim_and_lower(szLogin, login, sizeof(login));

	char passwd[PASSWD_MAX_LEN + 1];
	CopyPacketString(passwd, pinfo->passwd);

	metin2::log::info( "InputAuth::Login : {}({:d}) desc {}",
			login, strlen(login), static_cast<const void *>(d));

	// check login string
	if (false == FN_IS_VALID_LOGIN_STRING(login))
	{
		metin2::log::info( "InputAuth::Login : IS_NOT_VALID_LOGIN_STRING({}) desc {}",
				login, static_cast<const void *>(d));
		LoginFailure(d, "NOID");
		return;
	}

	if (g_bNoMoreClient)
	{
		TPacketGCLoginFailure failurePacket;

		failurePacket.header = HEADER_GC_LOGIN_FAILURE;
		strlcpy(failurePacket.szStatus, "SHUTDOWN", sizeof(failurePacket.szStatus));

		d->Packet(&failurePacket, sizeof(failurePacket));
		return;
	}

	if (DESC_MANAGER::instance().FindByLoginName(login))
	{
		LoginFailure(d, "ALREADY");
		return;
	}

	uint32_t dwKey = DESC_MANAGER::instance().CreateLoginKey(d);
	metin2::log::info( "InputAuth::Login : key {:d} login {}", dwKey, login);

	// BRAZIL_AUTH

	TPacketCGLogin3 * p = new TPacketCGLogin3;
	std::memcpy(p, pinfo, sizeof(TPacketCGLogin3));

	// The query result reads these as strings.
	p->login[sizeof(p->login) - 1] = '\0';
	p->passwd[sizeof(p->passwd) - 1] = '\0';

	// CHANNEL_SERVICE_LOGIN
	if (Login_IsInChannelService(login))
	{
		metin2::log::info( "ChannelServiceLogin [{}]", login);

		DBManager::instance().ReturnQuery(QID_AUTH_LOGIN, dwKey, p, sql::format(
				"SELECT {},password,social_id,id,status,availDt - NOW() > 0,"
				"UNIX_TIMESTAMP(silver_expire),"
				"UNIX_TIMESTAMP(gold_expire),"
				"UNIX_TIMESTAMP(safebox_expire),"
				"UNIX_TIMESTAMP(autoloot_expire),"
				"UNIX_TIMESTAMP(fish_mind_expire),"
				"UNIX_TIMESTAMP(marriage_fast_expire),"
				"UNIX_TIMESTAMP(money_drop_rate_expire),"
				"UNIX_TIMESTAMP(create_time)"
				" FROM account WHERE login={}",
				passwd, login));
	}
	// END_OF_CHANNEL_SERVICE_LOGIN
	else
	{
		DBManager::instance().ReturnQuery(QID_AUTH_LOGIN, dwKey, p, sql::format(
				"SELECT PASSWORD({}),password,social_id,id,status,availDt - NOW() > 0,"
				"UNIX_TIMESTAMP(silver_expire),"
				"UNIX_TIMESTAMP(gold_expire),"
				"UNIX_TIMESTAMP(safebox_expire),"
				"UNIX_TIMESTAMP(autoloot_expire),"
				"UNIX_TIMESTAMP(fish_mind_expire),"
				"UNIX_TIMESTAMP(marriage_fast_expire),"
				"UNIX_TIMESTAMP(money_drop_rate_expire),"
				"UNIX_TIMESTAMP(create_time)"
				" FROM account WHERE login={}",
				passwd, login));
	}
}

int CInputAuth::Analyze(DESC * d, uint8_t bHeader, const char * c_pData)
{

	if (!metin2::config::is_auth())
	{
		metin2::log::error("CInputAuth class is not for game server. IP {} might be a hacker.", 
			d->GetHostName());
		d->DelayedDisconnect(5);
		return 0;
	}

	int iExtraLen = 0;

	if (metin2::config::test_server())
		metin2::log::info( " InputAuth Analyze Header[{:d}] ", bHeader);

	switch (bHeader)
	{
		case HEADER_CG_PONG:
			Pong(d);
			break;

		case HEADER_CG_LOGIN3:
			Login(d, c_pData);
			break;

		case HEADER_CG_HANDSHAKE:
			break;

		default:
			metin2::log::error("This phase does not handle this header {:d} (0x{:x})(phase: AUTH)", bHeader, bHeader);
			break;
	}

	return iExtraLen;
}
}
