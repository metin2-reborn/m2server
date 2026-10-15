#include "config.h"
#include "db.h"
#include "desc.h"
#include "desc_manager.h"
#include "input.h"
#include "packet.h"
#include "typedef.h"

#include <common/length.h>
#include <common/utils.h>
#include <libnet/io.h>
#include <libsql/query.h>
#include <libthecore/log.h>
#include <libthecore/utils.h>

#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>


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
		if (isdigit(*tmp) || isalpha(*tmp))
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

	if (!g_bAuthServer)
	{
		sys_err ("CInputAuth class is not for game server. IP %s might be a hacker.", 
			d->GetHostName());
		d->DelayedDisconnect(5);
		return;
	}

	// Copy for string integrity
	char login[LOGIN_MAX_LEN + 1];
	trim_and_lower(pinfo->login, login, sizeof(login));

	char passwd[PASSWD_MAX_LEN + 1];
	strlcpy(passwd, pinfo->passwd, sizeof(passwd));

	sys_log(0, "InputAuth::Login : %s(%zu) desc %p",
			login, strlen(login), d);

	// check login string
	if (false == FN_IS_VALID_LOGIN_STRING(login))
	{
		sys_log(0, "InputAuth::Login : IS_NOT_VALID_LOGIN_STRING(%s) desc %p",
				login, d);
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
	sys_log(0, "InputAuth::Login : key %u login %s", dwKey, login);

	// BRAZIL_AUTH

	TPacketCGLogin3 * p = new TPacketCGLogin3;
	std::memcpy(p, pinfo, sizeof(TPacketCGLogin3));

	// CHANNEL_SERVICE_LOGIN
	if (Login_IsInChannelService(login))
	{
		sys_log(0, "ChannelServiceLogin [%s]", login);

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

void CInputAuth::LoginOpenID(DESC * d, const char * c_pData)
{

	//OpenID test code.
	TPacketCGLogin5 *tempInfo1 = (TPacketCGLogin5 *)c_pData;

	//Send an auth key verification request to the Japanese web server.
	char* authKey = tempInfo1->authKey;
	char returnID[LOGIN_MAX_LEN + 1] = {0};

	int test_url_get_protocol = auth_OpenID(authKey, d->GetHostName(), returnID);

	//Authentication failed. Error handling
	if (0!=test_url_get_protocol)
	{
		LoginFailure(d, "OpenID Fail");
		return;
	}

	TPacketCGLogin3 tempInfo2;
	strncpy(tempInfo2.login, returnID, LOGIN_MAX_LEN);
	strncpy(tempInfo2.passwd, "0000", PASSWD_MAX_LEN);
	for(int i=0;i<4;i++)
		tempInfo2.adwClientKey[i] = tempInfo1->adwClientKey[i];
	TPacketCGLogin3 * pinfo = &tempInfo2;

	if (!g_bAuthServer)
	{
		sys_err ("CInputAuth class is not for game server. IP %s might be a hacker.", 
			d->GetHostName());
		d->DelayedDisconnect(5);
		return;
	}

	// Copy for string integrity
	char login[LOGIN_MAX_LEN + 1];
	trim_and_lower(pinfo->login, login, sizeof(login));

	char passwd[PASSWD_MAX_LEN + 1];
	strlcpy(passwd, pinfo->passwd, sizeof(passwd));

	sys_log(0, "InputAuth::Login : %s(%zu) desc %p",
			login, strlen(login), d);

	// check login string
	if (false == FN_IS_VALID_LOGIN_STRING(login))
	{
		sys_log(0, "InputAuth::Login : IS_NOT_VALID_LOGIN_STRING(%s) desc %p",
				login, d);
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
	sys_log(0, "InputAuth::Login : key %u login %s", dwKey, login);

	// BRAZIL_AUTH

	TPacketCGLogin3 * p = new TPacketCGLogin3;
	std::memcpy(p, pinfo, sizeof(TPacketCGLogin3));

	// CHANNEL_SERVICE_LOGIN
	if (Login_IsInChannelService(login))
	{
		sys_log(0, "ChannelServiceLogin [%s]", login);

		DBManager::instance().ReturnQuery(QID_AUTH_LOGIN_OPENID, dwKey, p, sql::format(
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
		DBManager::instance().ReturnQuery(QID_AUTH_LOGIN_OPENID, dwKey, p, sql::format(
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


//OpenID
int CInputAuth::auth_OpenID(const char *authKey, const char *ipAddr, char *rID)
{
	//return value
	//0 : normal execution
	//1 : cannot connect to openid auth server 
	//2 : socket_write failed
	//3 : openid auth server not reply
	//4 : Reply Error
	//5 : Incorrect auth key.


    constexpr uint16_t port = 80;
    constexpr std::chrono::seconds timeout(3);

    auto socket = net::connect(openid_host, port, timeout);
    if (!socket)
    {
		sys_err("[auth_OpenID] : could not connect to OpenID server(%s)", openid_host);
		return 1;
    }

    // send request
    {
		char request[512];
		int len = snprintf(request, sizeof(request),
						//"GET /kyw/gameauth.php?auth_key=%s&ip=%s HTTP/1.1\r\n"
						"GET %s?auth_key=%s&ip=%s HTTP/1.1\r\n"
						"Host: %s\r\n"
						"Connection: Close\r\n\r\n",
						openid_uri, authKey, ipAddr, openid_host);

		if (len < 0 || len >= (int) sizeof(request) || !net::write(*socket, std::span((const uint8_t *) request, len), timeout))
		{
			sys_err("[auth_OpenID] : could not send auth-request (%s)", authKey);
			return 2;
		}
    }

    // read reply
	char reply[1024] = {0};
	const size_t len = net::read_some(*socket, std::span((uint8_t *) reply, sizeof(reply) - 1), timeout);

	if (len == 0)
	{
	    sys_err("[auth_OpenID] : could not recv auth-reply (%s)", authKey);
	    return 3;
	}

	//Parse the result
	char buffer[1024];
	strlcpy(buffer, reply, sizeof(buffer));

	const char *delim = "\r\n";
	char *v = strtok(buffer, delim);
	char *result = nullptr;

	while (v)
	{
		result = v;
		v = strtok(nullptr, delim);
	}


	char *id = strtok(result, "%");
	char *success = strtok(nullptr, "%");

	if (!*id || !*success)
	{
	    sys_err("[auth_OpenID] : OpenID AuthServer Reply Error (%s)", reply);
		return 4;
	}

	if (0 != strcmp("OK", success))		//Error handling
	{
		int returnNumber = 0;
		str_to_number(returnNumber, id);
		switch (returnNumber)
		{
		case 1:
			sys_err("[auth_OpenID] : AuthKey incorrect");
			break;
		case 2:
			sys_err("[auth_OpenID] : ip incorrect");
			break;
		case 3:
			sys_err("[auth_OpenID] : used AuthKey");
			break;
		case 4:
			sys_err("[auth_OpenID] : AuthKey not delivered");
			break;
		case 5:
			sys_err("[auth_OpenID] : ip not delivered");
			break;
		case 6:
			sys_err("[auth_OpenID] : AuthKey time over");
			break;
		default:
			break;

		return 5;
		}
	}

	strlcpy(rID, id, LOGIN_MAX_LEN + 1);	// rID is the caller's returnID buffer

	return 0;
}


int CInputAuth::Analyze(DESC * d, uint8_t bHeader, const char * c_pData)
{

	if (!g_bAuthServer)
	{
		sys_err ("CInputAuth class is not for game server. IP %s might be a hacker.", 
			d->GetHostName());
		d->DelayedDisconnect(5);
		return 0;
	}

	int iExtraLen = 0;

	if (test_server)
		sys_log(0, " InputAuth Analyze Header[%d] ", bHeader);

	switch (bHeader)
	{
		case HEADER_CG_PONG:
			Pong(d);
			break;

		case HEADER_CG_LOGIN3:
			Login(d, c_pData);
			break;

		//2012.07.19 OpenID : Kim Yong-uk
		case HEADER_CG_LOGIN5_OPENID:
			if (openid_server)
				LoginOpenID(d, c_pData);
			else
				sys_err("HEADER_CG_LOGIN5_OPENID : wrong client access");
			break;

		case HEADER_CG_HANDSHAKE:
			break;

		default:
			sys_err("This phase does not handle this header %d (0x%x)(phase: AUTH)", bHeader, bHeader);
			break;
	}

	return iExtraLen;
}

