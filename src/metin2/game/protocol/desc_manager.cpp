#include <metin2/game/protocol/desc_manager.h>

#include <metin2/game/characters/char.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/crc32.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/desc_p2p.h>
#include <metin2/game/protocol/ip_ban.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>

#include <metin2/proto/length.h>
#include <metin2/net/connection.h>
#include <metin2/net/io.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <utility>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace metin2::game::inline protocol {

desc_manager::desc_manager() : destroyed_(false)
{
	initialize();
	//NOTE : what on earth is calling Initialize at the end of Destroy..-_-; really 

}

desc_manager::~desc_manager()
{
	destroy();
}

void desc_manager::initialize()
{
	sockets_connected_ = 0;
	handle_count_ = 0;
	local_user_count_ = 0;
	memset(empire_user_count_, 0, sizeof(empire_user_count_));
}

void desc_manager::destroy()
{
	if (destroyed_) {
		return;
	}
	destroyed_ = true;

	desc_set::iterator i = set_pk_desc_.begin();

	while (i != set_pk_desc_.end())
	{
		desc * d = *i;
		desc_set::iterator ci = i;
		++i;

		if (d->type() == DESC_TYPE_CONNECTOR)
			continue;

		if (d->is_phase(PHASE_P2P))
			continue;

		destroy_desc(d, false);
		set_pk_desc_.erase(ci);
	}

	i = set_pk_desc_.begin();

	while (i != set_pk_desc_.end())
	{
		desc * d = *i;
		desc_set::iterator ci = i;
		++i;

		destroy_desc(d, false);
		set_pk_desc_.erase(ci);
	}

	set_pk_client_desc_.clear();

	map_login_name_.clear();
	map_handle_.clear();

	initialize();
}

uint32_t desc_manager::create_handshake()
{
	char crc_buf[8];
	crc_t crc;
	desc_handshake_map::iterator it;

RETRY:
	do
	{
		uint32_t val = thecore_random() % (1024 * 1024);

		*(uint32_t *) (crc_buf    ) = val;
		*((uint32_t *) crc_buf + 1) = get_global_time();

		crc = get_cr_c32(crc_buf, 8);
		it = map_handshake_.find(crc);
	}
	while (it != map_handshake_.end());

	if (crc == 0)
		goto RETRY;

	return (crc);
}

desc * desc_manager::accept_desc(net::tcp::socket socket)
{
	auto connection = net::connection::create(std::move(socket), MAX_CLIENT_INPUT, MAX_CLIENT_OUTPUT);
	const char * host = connection->remote_address().c_str();

	if (metin2::config::is_auth())
	{
		in_addr addr{};

		if (inet_pton(AF_INET, host, &addr) == 1 && is_ban_ip(addr))
		{
			metin2::log::info( "connection from {} was banned.", host);
			connection->close();
			return nullptr;
		}
	}

	if (sockets_connected_ >= MAX_ALLOW_USER)
	{
		metin2::log::error("max connection reached. MAX_ALLOW_USER = {:d}", MAX_ALLOW_USER);
		connection->close();
		return nullptr;
	}

	desc * newd = new desc;
	crc_t handshake = create_handshake();

	if (!newd->setup(std::move(connection), ++handle_count_, handshake))
	{
		newd->destroy();
		delete newd;
		return nullptr;
	}

	map_handshake_.insert(desc_handshake_map::value_type(handshake, newd));
	map_handle_.insert(desc_handle_map::value_type(newd->handle(), newd));

	set_pk_desc_.insert(newd);
	++sockets_connected_;
	return (newd);
}

desc * desc_manager::accept_p2_p_desc(net::tcp::socket socket)
{
	desc_p2_p * pkDesc = new desc_p2_p;

	if (!pkDesc->setup(net::connection::create(std::move(socket), MAX_SERVER_INPUT, MAX_SERVER_OUTPUT)))
	{     
		metin2::log::error("DESC_MANAGER::AcceptP2PDesc : Setup failed");
		pkDesc->destroy();
		delete pkDesc;
		return nullptr;
	}

	set_pk_desc_.insert(pkDesc);
	++sockets_connected_;

	metin2::log::info( "DESC_MANAGER::AcceptP2PDesc  {}", pkDesc->host_name());
	p2_p_manager::instance().register_acceptor(pkDesc);
	return (pkDesc);
}

void desc_manager::connect_account(const std::string& login, desc * d)
{
metin2::log::debug( "BBBB ConnectAccount({})", login.c_str());
	map_login_name_.insert(desc_loginname_map::value_type(login,d));
}

void desc_manager::disconnect_account(const std::string& login)
{
metin2::log::debug( "BBBB DisConnectAccount({})", login.c_str());
	map_login_name_.erase(login);
}

void desc_manager::destroy_desc(desc * d, bool bEraseFromSet)
{
	if (bEraseFromSet)
		set_pk_desc_.erase(d);

	if (d->handshake())
		map_handshake_.erase(d->handshake());

	if (d->handle() != 0)
	{
		map_handle_.erase(d->handle());
		map_handle_random_key_.erase(d->handle());
	}
	else
		set_pk_client_desc_.erase((client_desc *) d);

	// Explicit call to the virtual function Destroy()
	d->destroy();

	delete d;
	--sockets_connected_;
}

void desc_manager::destroy_closed()
{
	desc_set::iterator i = set_pk_desc_.begin();

	while (i != set_pk_desc_.end())
	{
		desc * d = *i;
		desc_set::iterator ci = i;
		++i;

		if (d->is_phase(PHASE_CLOSE))
		{
			if (d->type() == DESC_TYPE_CONNECTOR)
			{
				client_desc * client_desc = (game::protocol::client_desc *)d;

				if (client_desc->is_retry_when_closed())
				{
					client_desc->reset();
					continue;
				}
			}

			destroy_desc(d, false);
			set_pk_desc_.erase(ci);
		}
	}
}

desc * desc_manager::find_by_login_name(const std::string& login)
{
	desc_loginname_map::iterator it = map_login_name_.find(login);

	if (map_login_name_.end() == it)
		return nullptr;

	return (it->second); 
}

desc * desc_manager::find_by_handle(uint32_t handle)
{
	desc_handle_map::iterator it = map_handle_.find(handle);

	if (map_handle_.end() == it)
		return nullptr;

	return (it->second); 
}

const desc_manager::desc_set & desc_manager::client_set()
{
	return set_pk_desc_;
}

struct name_with_desc_func
{
	const char * name;

	name_with_desc_func(const char * name_value) : name(name_value)
	{
	}

	bool operator () (desc * d)
	{
		if (d->get_character() && !strcmp(d->get_character()->name(), name))
			return true;

		return false;
	}
};

desc * desc_manager::find_by_character_name(const char *name)
{
	desc_set::iterator it = std::find_if (set_pk_desc_.begin(), set_pk_desc_.end(), name_with_desc_func(name));
	return (it == set_pk_desc_.end()) ? nullptr : (*it);
}

client_desc * desc_manager::create_connection_desc(const char * host, uint16_t port, int iPhaseWhenSucceed, bool bRetryWhenClosed)
{
	client_desc * newd;

	newd = new client_desc;

	newd->setup(host, port);
	newd->connect(iPhaseWhenSucceed);
	newd->set_retry_when_closed(bRetryWhenClosed);

	set_pk_desc_.insert(newd);
	set_pk_client_desc_.insert(newd);

	++sockets_connected_;
	return (newd);
}

struct func_try_connect
{
	void operator () (desc * d)
	{
		((client_desc *)d)->connect();
	}
};

void desc_manager::try_connect()
{
	func_try_connect f;
	std::for_each(set_pk_client_desc_.begin(), set_pk_client_desc_.end(), f);
}

bool desc_manager::is_p2_p_desc_exist(const char * szHost, uint16_t wPort)
{
	client_desc_set::iterator it = set_pk_client_desc_.begin();
	
	while (it != set_pk_client_desc_.end())
	{
		client_desc * d = *(it++);

		if (!strcmp(d->p2_p_host(), szHost) && d->p2_p_port() == wPort)
			return true;
	}

	return false;
}

class func_who
{
	public:
		int total_count;
		int empire_user_count[EMPIRE_MAX_NUM];

		func_who()
		{
			total_count = 0;
			memset(empire_user_count, 0, sizeof(empire_user_count));
		}

		void operator() (desc * d)
		{
			if (d->get_character())
			{
				++total_count;
				++empire_user_count[d->empire()];
			}
		}
};

void desc_manager::update_local_user_count()
{
	const desc_set & c_ref_set = client_set();
	func_who f;
	f = std::for_each(c_ref_set.begin(), c_ref_set.end(), f);

	local_user_count_ = f.total_count;
	std::memcpy(empire_user_count_, f.empire_user_count, sizeof(empire_user_count_));

	empire_user_count_[1] += p2_p_manager::instance().empire_user_count(1);
	empire_user_count_[2] += p2_p_manager::instance().empire_user_count(2);
	empire_user_count_[3] += p2_p_manager::instance().empire_user_count(3);
}

void desc_manager::user_count(int & iTotal, int ** paiEmpireUserCount, int & iLocalCount)
{
	*paiEmpireUserCount = &empire_user_count_[0];
	
	int iCount = p2_p_manager::instance().count();
	if (iCount < 0)
	{
		metin2::log::error("P2P_MANAGER::instance().GetCount() == -1");
	}
	iTotal = local_user_count_ + iCount;
	iLocalCount = local_user_count_;
}


uint32_t desc_manager::make_random_key(uint32_t dwHandle)
{ 
	uint32_t random_key = thecore_random();
	map_handle_random_key_[dwHandle] = random_key;
	return random_key;
}

bool desc_manager::check_random_key(uint32_t dwHandle, uint32_t dwKey)
{
	auto it = map_handle_random_key_.find(dwHandle);
	return it != map_handle_random_key_.end() && it->second == dwKey && find_by_handle(dwHandle) != nullptr;
}

desc * desc_manager::find_by_login_key(uint32_t dwKey)
{
	std::map<uint32_t, login_key *>::iterator it = map_pk_login_key_.find(dwKey);

	if (it == map_pk_login_key_.end())
		return nullptr;

	return it->second->desc;
}


uint32_t desc_manager::create_login_key(desc * d)
{
	uint32_t dwKey = 0;

	do
	{
		dwKey = number(1, INT_MAX);

		if (map_pk_login_key_.find(dwKey) != map_pk_login_key_.end())
			continue;

		login_key * pkKey = new login_key(dwKey, d);
		d->set_login_key(pkKey);
		map_pk_login_key_.insert(std::make_pair(dwKey, pkKey));
		break;
	} while (1);

	return dwKey;
}

void desc_manager::process_expired_login_key()
{
	uint32_t dwCurrentTime = get_dword_time();

	std::map<uint32_t, login_key *>::iterator it, it2;

	it = map_pk_login_key_.begin();

	while (it != map_pk_login_key_.end())
	{
		it2 = it++;

		if (it2->second->expire_time == 0)
			continue;

		if (dwCurrentTime - it2->second->expire_time > 60000)
		{
			delete it2->second;
			map_pk_login_key_.erase(it2);
		}
	}
}
}
