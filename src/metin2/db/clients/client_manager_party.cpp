#include <metin2/db/clients/client_manager.h>
#include <metin2/db/clients/peer.h>

#include <metin2/log/log.h>
#include <metin2/proto/tables.h>

#include <utility>

namespace metin2::db::inline clients {

    void client_manager::query_party_create(peer *peer, packet_party_create *p) {
        party_map &pm = map_pk_channel_party_[peer->channel()];

        if (pm.find(p->leader_pid) == pm.end()) {
            pm.insert(make_pair(p->leader_pid, party_member()));
            forward_packet(HEADER_DG_PARTY_CREATE, p, sizeof(packet_party_create), peer->channel(), peer);
            metin2::log::info("Party: created with leader pid {:d}", p->leader_pid);
        } else {
            metin2::log::warn("Party: cannot create, party of leader pid {:d} already exists", p->leader_pid);
        }
    }

    void client_manager::query_party_delete(peer *peer, packet_party_delete *p) {
        party_map &pm = map_pk_channel_party_[peer->channel()];
        auto it = pm.find(p->leader_pid);

        if (it == pm.end()) {
            metin2::log::warn("Party: cannot delete, no party with leader pid {:d}", p->leader_pid);
            return;
        }

        pm.erase(it);
        forward_packet(HEADER_DG_PARTY_DELETE, p, sizeof(packet_party_delete), peer->channel(), peer);
        metin2::log::info("Party: deleted party of leader pid {:d}", p->leader_pid);
    }

    void client_manager::query_party_add(peer *peer, packet_party_add *p) {
        party_map &pm = map_pk_channel_party_[peer->channel()];
        auto it = pm.find(p->leader_pid);

        if (it == pm.end()) {
            metin2::log::warn("Party: cannot add member, no party with leader pid {:d}", p->leader_pid);
            return;
        }

        if (it->second.find(p->pid) == it->second.end()) {
            it->second.insert(std::make_pair(p->pid, party_info()));
            forward_packet(HEADER_DG_PARTY_ADD, p, sizeof(packet_party_add), peer->channel(), peer);
            metin2::log::info("Party: added pid {:d} to party of leader pid {:d}", p->pid, p->leader_pid);
        } else
            metin2::log::warn("Party: pid {:d} is already in party of leader pid {:d}", p->pid, p->leader_pid);
    }

    void client_manager::query_party_remove(peer *peer, packet_party_remove *p) {
        party_map &pm = map_pk_channel_party_[peer->channel()];
        auto it = pm.find(p->leader_pid);

        if (it == pm.end()) {
            metin2::log::warn("Party: cannot remove member, no party with leader pid {:d} (member pid {:d})", p->leader_pid, p->pid);
            return;
        }

        auto pit = it->second.find(p->pid);

        if (pit != it->second.end()) {
            it->second.erase(pit);
            forward_packet(HEADER_DG_PARTY_REMOVE, p, sizeof(packet_party_remove), peer->channel(), peer);
            metin2::log::info("Party: removed pid {:d} from party of leader pid {:d}", p->pid, p->leader_pid);
        } else
            metin2::log::warn("Party: cannot remove pid {:d}, not in party of leader pid {:d}", p->pid, p->leader_pid);
    }

    void client_manager::query_party_state_change(peer *peer, packet_party_state_change *p) {
        party_map &pm = map_pk_channel_party_[peer->channel()];
        auto it = pm.find(p->leader_pid);

        if (it == pm.end()) {
            metin2::log::warn("Party: cannot change role, no party with leader pid {:d} (member pid {:d})", p->leader_pid, p->pid);
            return;
        }

        auto pit = it->second.find(p->pid);

        if (pit == it->second.end()) {
            metin2::log::warn("Party: cannot change role of pid {:d}, not in party of leader pid {:d}", p->pid, p->leader_pid);
            return;
        }

        if (p->flag)
            pit->second.role = p->role;
        else
            pit->second.role = 0;

        forward_packet(HEADER_DG_PARTY_STATE_CHANGE, p, sizeof(packet_party_state_change), peer->channel(), peer);
        metin2::log::info("Party: pid {:d} in party of leader pid {:d} role {:d} flag {:d}", p->pid, p->leader_pid, p->role, p->flag);
    }

    void client_manager::query_party_set_member_level(peer *peer, packet_party_set_member_level *p) {
        party_map &pm = map_pk_channel_party_[peer->channel()];
        auto it = pm.find(p->leader_pid);

        if (it == pm.end()) {
            metin2::log::warn("Party: cannot set level, no party with leader pid {:d} (member pid {:d})", p->leader_pid, p->pid);
            return;
        }

        auto pit = it->second.find(p->pid);

        if (pit == it->second.end()) {
            metin2::log::warn("Party: cannot set level of pid {:d}, not in party of leader pid {:d}", p->pid, p->leader_pid);
            return;
        }

        pit->second.level = p->level;

        forward_packet(HEADER_DG_PARTY_SET_MEMBER_LEVEL, p, sizeof(packet_party_set_member_level), peer->channel());
        metin2::log::debug("Party: member pid {:d} is now level {:d}", p->pid, p->level);
    }
}
