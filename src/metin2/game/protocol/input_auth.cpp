#include <metin2/config/config.h>
#include <metin2/game/app/config.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/protocol/input.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/sql/query.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>

namespace metin2::game::inline protocol {


auto fn_is_valid_login_string(const char *str) -> bool
{
	const char*	tmp;

	if (!str || !*str)
		return false;

	if (strlen(str) < 2)
		return false;

	for (tmp = str; *tmp; ++tmp)
	{
		if (isdigit((unsigned char) *tmp) || isalpha((unsigned char) *tmp))
			continue;





		return false;
	}

	return true;
}

auto login_is_in_channel_service(const char* login) -> bool
{
	if (login[0] == '[')
		return true;
	return false;
}

input_auth::input_auth()
{
}

void input_auth::login(desc * d, const char * data)
{

	packet_cg_login3 * pinfo = (packet_cg_login3 *) data;

	if (!metin2::config::is_auth())
	{
		metin2::log::error("CInputAuth class is not for game server. IP {} might be a hacker.", 
			d->host_name());
		d->delayed_disconnect(5);
		return;
	}

	// One login per connection: another would replace the key of a query in flight and allow
	// password guessing at line rate.
	if (d->get_login_key() || d->get_account_table().id)
	{
		metin2::log::info( "InputAuth::Login : second login attempt from {}", d->host_name());
		d->set_phase(PHASE_CLOSE);
		return;
	}

	char szLogin[sizeof(pinfo->login)];
	copy_packet_string(szLogin, pinfo->login);

	char login[LOGIN_MAX_LEN + 1];
	trim_and_lower(szLogin, login, sizeof(login));

	char passwd[PASSWD_MAX_LEN + 1];
	copy_packet_string(passwd, pinfo->passwd);

	metin2::log::info( "InputAuth::Login : {}({:d}) desc {}",
			login, strlen(login), static_cast<const void *>(d));

	if (false == fn_is_valid_login_string(login))
	{
		metin2::log::info( "InputAuth::Login : IS_NOT_VALID_LOGIN_STRING({}) desc {}",
				login, static_cast<const void *>(d));
		login_failure(d, "NOID");
		return;
	}

	if (no_more_client)
	{
		packet_gc_login_failure failurePacket;

		failurePacket.header = HEADER_GC_LOGIN_FAILURE;
		strlcpy(failurePacket.status, "SHUTDOWN", sizeof(failurePacket.status));

		d->packet(&failurePacket, sizeof(failurePacket));
		return;
	}

	if (desc_manager::instance().find_by_login_name(login))
	{
		login_failure(d, "ALREADY");
		return;
	}

	uint32_t dwKey = desc_manager::instance().create_login_key(d);
	metin2::log::info( "InputAuth::Login : key {:d} login {}", dwKey, login);


	packet_cg_login3 * p = new packet_cg_login3;
	std::memcpy(p, pinfo, sizeof(packet_cg_login3));

	p->login[sizeof(p->login) - 1] = '\0';
	p->passwd[sizeof(p->passwd) - 1] = '\0';

	if (login_is_in_channel_service(login))
	{
		metin2::log::info( "ChannelServiceLogin [{}]", login);

		db_manager::instance().return_query(QID_AUTH_LOGIN, dwKey, p, sql::format(
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
	else
	{
		db_manager::instance().return_query(QID_AUTH_LOGIN, dwKey, p, sql::format(
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

auto input_auth::analyze(desc * d, uint8_t header, const char * data) -> int
{

	if (!metin2::config::is_auth())
	{
		metin2::log::error("CInputAuth class is not for game server. IP {} might be a hacker.", 
			d->host_name());
		d->delayed_disconnect(5);
		return 0;
	}

	int iExtraLen = 0;

	if (metin2::config::test_server())
		metin2::log::info( " InputAuth Analyze Header[{:d}] ", header);

	switch (header)
	{
		case HEADER_CG_PONG:
			pong(d);
			break;

		case HEADER_CG_LOGIN3:
			login(d, data);
			break;

		case HEADER_CG_HANDSHAKE:
			break;

		default:
			metin2::log::error("This phase does not handle this header {:d} (0x{:x})(phase: AUTH)", header, header);
			break;
	}

	return iExtraLen;
}
}
