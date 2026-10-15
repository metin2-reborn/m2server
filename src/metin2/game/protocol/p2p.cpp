#include <metin2/game/protocol/p2p.h>

#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/social/marriage.h>
#include <metin2/game/social/messenger_manager.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/log/log.h>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>

namespace metin2::game::inline protocol {

p2_p_manager::p2_p_manager()
{
	handle_count_ = 0;

	memset(empire_user_count_, 0, sizeof(empire_user_count_));
}

p2_p_manager::~p2_p_manager()
{
}

void p2_p_manager::boot(desc * d)
{
	character_manager::name_map & map = character_manager::instance().pc_map();
	character_manager::name_map::iterator it = map.begin();

	packet_gg_login p;

	while (it != map.end())
	{
		character * ch = it->second;
		it++;

		p.header = HEADER_GG_LOGIN;
		strlcpy(p.name, ch->name(), sizeof(p.name));
		p.pid = ch->player_id();
		p.empire = ch->empire();
		p.map_index = sectree_manager::instance().map_index(ch->x(), ch->y());
		p.channel = metin2::config::server().channel;

		d->packet(&p, sizeof(p));
	}
}

void p2_p_manager::flush_output()
{
	std::unordered_set<desc *>::iterator it = set_pk_peers_.begin();

	while (it != set_pk_peers_.end())
	{
		desc * pkDesc = *it++;
		pkDesc->flush_output();
	}
}

void p2_p_manager::register_acceptor(desc * d)
{
	metin2::log::info( "P2P Acceptor opened (host {})", d->host_name());
	set_pk_peers_.insert(d);
	boot(d);
}

void p2_p_manager::unregister_acceptor(desc * d)
{
	metin2::log::info( "P2P Acceptor closed (host {})", d->host_name());
	erase_user_by_desc(d);
	set_pk_peers_.erase(d);
}

void p2_p_manager::register_connector(desc * d)
{
	metin2::log::info( "P2P Connector opened (host {})", d->host_name());
	set_pk_peers_.insert(d);
	boot(d);

	packet_gg_setup p;
	p.header = HEADER_GG_SETUP;
	p.port = metin2::config::server().p2p_port;
	p.channel = metin2::config::server().channel;
	d->packet(&p, sizeof(p));
}

void p2_p_manager::unregister_connector(desc * d)
{
	std::unordered_set<desc *>::iterator it = set_pk_peers_.find(d);

	if (it != set_pk_peers_.end())
	{
		metin2::log::info( "P2P Connector closed (host {})", d->host_name());
		erase_user_by_desc(d);
		set_pk_peers_.erase(it);
	}
}

void p2_p_manager::erase_user_by_desc(desc * d)
{
	cci_map::iterator it = map_pk_cci_.begin();

	while (it != map_pk_cci_.end())
	{
		cci * pkCCI = it->second.get();
		it++;

		if (pkCCI->desc == d)
			logout(pkCCI);
	}
}

void p2_p_manager::send(const void * c_pvData, int iSize, desc * except)
{
	std::unordered_set<desc *>::iterator it = set_pk_peers_.begin();

	while (it != set_pk_peers_.end())
	{
		desc * pkDesc = *it++;

		if (except == pkDesc)
			continue;

		pkDesc->packet(c_pvData, iSize);
	}
}

void p2_p_manager::login(desc * d, const packet_gg_login * p)
{
	cci* pkCCI = find(p->name);

	bool UpdateP2P = false;

	if (nullptr == pkCCI)
	{
		UpdateP2P = true;
		auto pkNewCCI = std::make_unique<cci>();
		pkCCI = pkNewCCI.get();

		strlcpy(pkCCI->name, p->name, sizeof(pkCCI->name));

		pkCCI->pid = p->pid;
		pkCCI->empire = p->empire;

		if (p->channel == metin2::config::server().channel)
		{
			if (pkCCI->empire < EMPIRE_MAX_NUM)
			{
				++empire_user_count_[pkCCI->empire];
			}
			else
			{
				metin2::log::error("LOGIN_EMPIRE_ERROR: {:d} >= MAX({:d})", pkCCI->empire, EMPIRE_MAX_NUM);
			}
		}

		map_dw_pid_pk_cci_.insert(std::make_pair(pkCCI->pid, pkCCI));
		map_pk_cci_.emplace(pkCCI->name, std::move(pkNewCCI));
	}

	pkCCI->map_index = p->map_index;
	pkCCI->desc = d;
	pkCCI->channel = p->channel;
	metin2::log::info( "P2P: Login {}", pkCCI->name);

	guild_manager::instance().p2_p_login_member(pkCCI->pid);
	party_manager::instance().p2_p_login(pkCCI->pid, pkCCI->name);

	// The messenger only needs updating when the CCI is created.
	if (UpdateP2P) {
		std::string name(pkCCI->name);
	    messenger_manager::instance().p2_p_login(name);
	}
}

void p2_p_manager::logout(cci * pkCCI)
{
	if (pkCCI->channel == metin2::config::server().channel)
	{
		if (pkCCI->empire < EMPIRE_MAX_NUM)
		{
			--empire_user_count_[pkCCI->empire];
			if (empire_user_count_[pkCCI->empire] < 0)
			{
				metin2::log::error("m_aiEmpireUserCount[{:d}] < 0", pkCCI->empire);
			}
		}
		else
		{
			metin2::log::error("LOGOUT_EMPIRE_ERROR: {:d} >= MAX({:d})", pkCCI->empire, EMPIRE_MAX_NUM);
		}
	}

	std::string name(pkCCI->name);

	guild_manager::instance().p2_p_logout_member(pkCCI->pid);
	party_manager::instance().p2_p_logout(pkCCI->pid);
	messenger_manager::instance().p2_p_logout(name);
	marriage::manager::instance().logout(pkCCI->pid);

	map_dw_pid_pk_cci_.erase(pkCCI->pid);
	map_pk_cci_.erase(name);
}

void p2_p_manager::logout(const char * c_pszName)
{
	cci * pkCCI = find(c_pszName);

	if (!pkCCI)
		return;

	metin2::log::info( "P2P: Logout {}", c_pszName);
	logout(pkCCI);
}

cci * p2_p_manager::find_by_pid(uint32_t pid)
{
	pidcci_map::iterator it = map_dw_pid_pk_cci_.find(pid);
	if (it == map_dw_pid_pk_cci_.end())
		return nullptr;
	return it->second;
}

cci * p2_p_manager::find(const char * c_pszName)
{
	cci_map::const_iterator it;

	it = map_pk_cci_.find(c_pszName);

	if (it == map_pk_cci_.end())
		return nullptr;

	return it->second.get();
}

int p2_p_manager::count()
{
	return empire_user_count_[1] + empire_user_count_[2] + empire_user_count_[3];
}

int p2_p_manager::empire_user_count(int idx)
{
	assert(idx < EMPIRE_MAX_NUM);
	return empire_user_count_[idx];
}


int p2_p_manager::desc_count()
{
	return set_pk_peers_.size();
}

void p2_p_manager::p2_p_host_names(std::string& hostNames)
{
	std::unordered_set<desc *>::iterator it = set_pk_peers_.begin();

	std::ostringstream oss(std::ostringstream::out);

	while (it != set_pk_peers_.end())
	{
		desc * pkDesc = *it++;

		oss << pkDesc->p2_p_host() << " " << pkDesc->p2_p_port() << "\n";

	}
	hostNames += oss.str();
}
}
