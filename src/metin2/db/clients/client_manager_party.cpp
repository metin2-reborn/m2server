#include <metin2/db/clients/client_manager.h>
#include <metin2/db/clients/peer.h>

#include <metin2/proto/tables.h>
#include <metin2/log/log.h>

#include <utility>

namespace metin2::db::inline clients {

void client_manager::query_party_create(peer* peer, packet_party_create* p)
{
	party_map & pm = map_pk_channel_party_[peer->channel()];

	if (pm.find(p->leader_pid) == pm.end())
	{
		pm.insert(make_pair(p->leader_pid, party_member()));
		forward_packet(HEADER_DG_PARTY_CREATE, p, sizeof(packet_party_create), peer->channel(), peer);
		metin2::log::info( "PARTY Create [{:d}]", p->leader_pid);
	}
	else
	{
		metin2::log::error("PARTY Create - Already exists [{:d}]", p->leader_pid);
	}
}

void client_manager::query_party_delete(peer* peer, packet_party_delete* p)
{
	party_map& pm = map_pk_channel_party_[peer->channel()];
	auto it = pm.find(p->leader_pid);

	if (it == pm.end())
	{
		metin2::log::error("PARTY Delete - Non exists [{:d}]", p->leader_pid);
		return;
	}

	pm.erase(it);
	forward_packet(HEADER_DG_PARTY_DELETE, p, sizeof(packet_party_delete), peer->channel(), peer);
	metin2::log::info( "PARTY Delete [{:d}]", p->leader_pid);
}

void client_manager::query_party_add(peer* peer, packet_party_add* p)
{
	party_map & pm = map_pk_channel_party_[peer->channel()];
	auto it = pm.find(p->leader_pid);

	if (it == pm.end())
	{
		metin2::log::error("PARTY Add - Non exists [{:d}]", p->leader_pid);
		return;
	}

	if (it->second.find(p->pid) == it->second.end())
	{
		it->second.insert(std::make_pair(p->pid, party_info()));
		forward_packet(HEADER_DG_PARTY_ADD, p, sizeof(packet_party_add), peer->channel(), peer);
		metin2::log::info( "PARTY Add [{:d}] to [{:d}]", p->pid, p->leader_pid);
	}
	else
		metin2::log::error("PARTY Add - Already [{:d}] in party [{:d}]", p->pid, p->leader_pid);
}

void client_manager::query_party_remove(peer* peer, packet_party_remove* p)
{
	party_map & pm = map_pk_channel_party_[peer->channel()];
	auto it = pm.find(p->leader_pid);

	if (it == pm.end())
	{
		metin2::log::error("PARTY Remove - Non exists [{:d}] cannot remove [{:d}]",p->leader_pid, p->pid);
		return;
	}

	auto pit = it->second.find(p->pid);

	if (pit != it->second.end())
	{
		it->second.erase(pit);
		forward_packet(HEADER_DG_PARTY_REMOVE, p, sizeof(packet_party_remove), peer->channel(), peer);
		metin2::log::info( "PARTY Remove [{:d}] to [{:d}]", p->pid, p->leader_pid);
	}
	else
		metin2::log::error("PARTY Remove - Cannot find [{:d}] in party [{:d}]", p->pid, p->leader_pid);
}

void client_manager::query_party_state_change(peer* peer, packet_party_state_change* p)
{
	party_map & pm = map_pk_channel_party_[peer->channel()];
	auto it = pm.find(p->leader_pid);

	if (it == pm.end())
	{
		metin2::log::error("PARTY StateChange - Non exists [{:d}] cannot state change [{:d}]",p->leader_pid, p->pid);
		return;
	}

	auto pit = it->second.find(p->pid);

	if (pit == it->second.end())
	{
		metin2::log::error("PARTY StateChange - Cannot find [{:d}] in party [{:d}]", p->pid, p->leader_pid);
		return;
	}

	if (p->flag)
		pit->second.role = p->role;
	else 
		pit->second.role = 0;

	forward_packet(HEADER_DG_PARTY_STATE_CHANGE, p, sizeof(packet_party_state_change), peer->channel(), peer);
	metin2::log::info( "PARTY StateChange [{:d}] at [{:d}] from {:d} {:d}",p->pid, p->leader_pid, p->role, p->flag);
}

void client_manager::query_party_set_member_level(peer* peer, packet_party_set_member_level* p)
{
	party_map & pm = map_pk_channel_party_[peer->channel()];
	auto it = pm.find(p->leader_pid);

	if (it == pm.end())
	{
		metin2::log::error("PARTY SetMemberLevel - Non exists [{:d}] cannot level change [{:d}]",p->leader_pid, p->pid);
		return;
	}

	auto pit = it->second.find(p->pid);

	if (pit == it->second.end())
	{
		metin2::log::error("PARTY SetMemberLevel - Cannot find [{:d}] in party [{:d}]", p->pid, p->leader_pid);
		return;
	}

	pit->second.level = p->level;

	forward_packet(HEADER_DG_PARTY_SET_MEMBER_LEVEL, p, sizeof(packet_party_set_member_level), peer->channel());
	metin2::log::info( "PARTY SetMemberLevel pid [{:d}] level {:d}",p->pid, p->level);
}
}
