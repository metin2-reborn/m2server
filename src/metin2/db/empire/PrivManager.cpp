#include <metin2/db/empire/PrivManager.h>

#include <metin2/db/clients/ClientManager.h>
#include <metin2/db/clients/Peer.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/log/log.h>

#include <cstdint>
#include <ctime>
#include <utility>

namespace metin2::db::inline empire {

const int CHARACTER_GOOD_PRIV_DURATION = 2*60*60;
const int CHARACTER_BAD_PRIV_DURATION = 60*60;

priv_manager::priv_manager()
{
	for (int type = 0; type < MAX_PRIV_NUM; ++type)
	{
		for (int empire = 0; empire < EMPIRE_MAX_NUM; ++empire)
			aa_priv_empire_[type][empire] = nullptr;
	}
}

priv_manager::~priv_manager()
{
}

//
// @version 05/06/07	Bang2ni - handling for guilds with duplicate bonuses applied
//
void priv_manager::update()
{
	time_t now = client_manager::instance().current_time();

	while (!pq_priv_guild_.empty() && pq_priv_guild_.top().first <= now)
	{
		priv_guild_data* p = pq_priv_guild_.top().second;
		pq_priv_guild_.pop();

		if (p->value != 0 && !p->removed)
		{

			__typeof(priv_guild_[p->type].begin()) it = priv_guild_[p->type].find(p->guild_id);

			// ADD_GUILD_PRIV_TIME
			// If a guild had a bonus set more than once, the map value was updated (modified), so
			// only actually delete it and cast to the game servers when the TPrivGuildData pointer matches.
			if (it != priv_guild_[p->type].end() && it->second == p) {
				priv_guild_[p->type].erase(it);
				send_change_guild_priv(p->guild_id, p->type, 0, 0);
				// END_OF_ADD_GUILD_PRIV_TIME
			}
		}

		delete p;
	}

	while (!pq_priv_empire_.empty() && pq_priv_empire_.top().first <= now)
	{
		priv_empire_data* p = (pq_priv_empire_.top().second);
		pq_priv_empire_.pop();

		if (p->value != 0 && !p->removed)
		{
			send_change_empire_priv(p->empire, p->type, 0, 0);
			aa_priv_empire_[p->type][p->empire] = nullptr;
		}

		delete p;
	}

	while (!pq_priv_char_.empty() && pq_priv_char_.top().first <= now)
	{
		priv_char_data* p = (pq_priv_char_.top().second);
		pq_priv_char_.pop();

		if (!p->removed)
		{
			// TODO send packet
			send_change_char_priv(p->pid, p->type, 0);
			__typeof(priv_char_[p->type].begin()) it = priv_char_[p->type].find(p->pid);
			if (it != priv_char_[p->type].end())
				priv_char_[p->type].erase(it);
		}
		delete p;
	}
}

void priv_manager::add_char_priv(uint32_t pid, uint8_t type, int value)
{
	if (MAX_PRIV_NUM <= type)
	{
		metin2::log::error("PRIV_MANAGER: AddCharPriv: wrong char priv type({:d}) recved", type);
		return;
	}

	__typeof(priv_char_[type].begin()) it = priv_char_[type].find(pid);

	if (it != priv_char_[type].end())
		return;

	if (!value)
		return;

	time_t now = client_manager::instance().current_time();
	priv_char_data* p = new priv_char_data(type, value, pid);

	int iDuration = CHARACTER_BAD_PRIV_DURATION;

	if (value > 0)
		iDuration = CHARACTER_GOOD_PRIV_DURATION;

	pq_priv_char_.push(std::make_pair(now+iDuration, p));
	priv_char_[type].insert(std::make_pair(pid, p));

	// TODO send packet
	metin2::log::info( "AddCharPriv {:d} {:d} {:d}", pid, type, value);
	send_change_char_priv(pid, type, value);
}

//
// @version 05/06/07	Bang2ni - set bonus on a guild that already has one
//
void priv_manager::add_guild_priv(uint32_t guild_id, uint8_t type, int value, time_t duration_sec)
{
	if (MAX_PRIV_NUM <= type)
	{
		metin2::log::error("PRIV_MANAGER: AddGuildPriv: wrong guild priv type({:d}) recved", type);
		return;
	}

	__typeof(priv_guild_[type].begin()) it = priv_guild_[type].find(guild_id);

	time_t now = client_manager::instance().current_time();
	time_t end = now + duration_sec;
	priv_guild_data * p = new priv_guild_data(type, value, guild_id, end);
	pq_priv_guild_.push(std::make_pair(end, p));

	// ADD_GUILD_PRIV_TIME
	// If a bonus is already set, update the map value.
	// The previous value's pointer is freed when it is removed from the priority queue.
	if (it != priv_guild_[type].end())
		it->second = p;
	else
		priv_guild_[type].insert(std::make_pair(guild_id, p));

	send_change_guild_priv(guild_id, type, value, end);
	// END_OF_ADD_GUILD_PRIV_TIME

	metin2::log::info( "Guild Priv guild({:d}) type({:d}) value({:d}) duration_sec({:d})", guild_id, type, value, duration_sec);
}

void priv_manager::add_empire_priv(uint8_t empire, uint8_t type, int value, time_t duration_sec)
{
	if (MAX_PRIV_NUM <= type)
	{
		metin2::log::error("PRIV_MANAGER: AddEmpirePriv: wrong empire priv type({:d}) recved", type);
		return;
	}

	if (duration_sec < 0)
		duration_sec = 0;

	time_t now = client_manager::instance().current_time();
	time_t end = now+duration_sec;

	// Invalidate the previous setting
	// pointer in the priority_queue == m_aaPrivEmpire[type][empire]
	{
		if (aa_priv_empire_[type][empire])
			aa_priv_empire_[type][empire]->removed = true;
	}

	priv_empire_data * p = new priv_empire_data(type, value, empire, end);
	pq_priv_empire_.push(std::make_pair(end, p));
	aa_priv_empire_[type][empire] = p;

	// ADD_EMPIRE_PRIV_TIME
	send_change_empire_priv(empire, type, value, end);
	// END_OF_ADD_EMPIRE_PRIV_TIME

	metin2::log::info( "Empire Priv empire({:d}) type({:d}) value({:d}) duration_sec({:d})", empire, type, value, duration_sec);
}

/**
 * @version 05/06/08	Bang2ni - added duration
 */
struct f_send_change_guild_priv
{
	f_send_change_guild_priv(uint32_t guild_id, uint8_t type, int value, time_t end_time_sec)
	{
		p.guild_id = guild_id;
		p.type = type;
		p.value = value;
		p.log = 1;
		// ADD_GUILD_PRIV_TIME
		p.end_time_sec = end_time_sec;
		// END_OF_ADD_GUILD_PRIV_TIME
	}

	void operator() (peer* peer)
	{
		peer->encode_header(HEADER_DG_CHANGE_GUILD_PRIV, 0, sizeof(packet_dg_change_guild_priv));
		peer->encode(&p, sizeof(packet_dg_change_guild_priv));
		p.log = 0;
	}

	packet_dg_change_guild_priv p;
};

struct f_send_change_empire_priv
{
	f_send_change_empire_priv(uint8_t empire, uint8_t type, int value, time_t end_time_sec)
	{
		p.empire = empire;
		p.type = type;
		p.value = value;
		p.log = 1;
		// ADD_EMPIRE_PRIV_TIME
		p.end_time_sec = end_time_sec;
		// END_OF_ADD_EMPIRE_PRIV_TIME
	}

	void operator ()(peer* peer)
	{
		peer->encode_header(HEADER_DG_CHANGE_EMPIRE_PRIV, 0, sizeof(packet_dg_change_empire_priv));
		peer->encode(&p, sizeof(packet_dg_change_empire_priv));
		p.log = 0;
	}

	packet_dg_change_empire_priv p;
};

struct f_send_change_char_priv
{
	f_send_change_char_priv(uint32_t pid, uint8_t type, int value)
	{
		p.pid = pid;
		p.type = type;
		p.value = value;
		p.log = 1;
	}
	void operator()(peer* peer)
	{
		peer->encode_header(HEADER_DG_CHANGE_CHARACTER_PRIV, 0, sizeof(packet_dg_change_character_priv));
		peer->encode(&p, sizeof(packet_dg_change_character_priv));
		p.log = 0;
	}
	packet_dg_change_character_priv p;
};

// ADD_GUILD_PRIV_TIME
void priv_manager::send_change_guild_priv(uint32_t guild_id, uint8_t type, int value, time_t end_time_sec)
{
	client_manager::instance().for_each_peer(f_send_change_guild_priv(guild_id, type, value, end_time_sec));
}
// END_OF_ADD_GUILD_PRIV_TIME

// ADD_EMPIRE_PRIV_TIME
void priv_manager::send_change_empire_priv(uint8_t empire, uint8_t type, int value, time_t end_time_sec)
{
	client_manager::instance().for_each_peer(f_send_change_empire_priv(empire, type, value, end_time_sec));
}
// END_OF_ADD_EMPIRE_PRIV_TIME

void priv_manager::send_change_char_priv(uint32_t pid, uint8_t type, int value)
{
	client_manager::instance().for_each_peer(f_send_change_char_priv(pid, type, value));
}

void priv_manager::send_priv_on_setup(peer* peer)
{
	for (int i = 1; i < MAX_PRIV_NUM; ++i)
	{
		for (int e = 0; e < EMPIRE_MAX_NUM; ++e)
		{
			// ADD_EMPIRE_PRIV_TIME
			priv_empire_data* pPrivEmpireData = aa_priv_empire_[i][e];
			if (pPrivEmpireData)
			{
				f_send_change_empire_priv(e, i, pPrivEmpireData->value, pPrivEmpireData->end_time_sec)(peer);
			}
			// END_OF_ADD_EMPIRE_PRIV_TIME
		}

		for (__typeof(priv_guild_[i].begin()) it = priv_guild_[i].begin(); it != priv_guild_[i].end();++it)
		{
			// ADD_GUILD_PRIV_TIME
			f_send_change_guild_priv(it->first, i, it->second->value, it->second->end_time_sec)(peer);
			// END_OF_ADD_GUILD_PRIV_TIME
		}
		for (__typeof(priv_char_[i].begin()) it = priv_char_[i].begin(); it != priv_char_[i].end(); ++it)
		{
			f_send_change_char_priv(it->first, i, it->second->value)(peer);
		}
	}
}
}
