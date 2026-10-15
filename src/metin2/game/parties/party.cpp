#include <metin2/game/parties/party.h>

#include <metin2/game/characters/char.h>
#include <metin2/game/characters/char_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/world/dungeon.h>
#include <metin2/game/app/event.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/app/utils.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

namespace metin2::game::inline parties {

party_manager::party_manager()
{
	initialize();
}

party_manager::~party_manager()
{
}

void party_manager::initialize()
{
	enable_pc_party_ = false;
}

void party_manager::delete_all_party()
{
	pc_party_set::iterator it = set_pk_pc_party_.begin();

	while (it != set_pk_pc_party_.end())
	{
		delete_party(*it);
		it = set_pk_pc_party_.begin();
	}
}

bool party_manager::set_party(character * ch)	// Must only be used for PCs!!
{
	party_map::iterator it = map_pk_party_.find(ch->player_id());

	if (it == map_pk_party_.end())
		return false;

	party * pParty = it->second;
	pParty->link(ch);
	return true;
}

void party_manager::p2_p_login(uint32_t pid, const char* name)
{
	party_map::iterator it = map_pk_party_.find(pid);

	if (it == map_pk_party_.end())
		return;

	it->second->update_online_state(pid, name);
}
void party_manager::p2_p_logout(uint32_t pid)
{
	party_map::iterator it = map_pk_party_.find(pid);

	if (it == map_pk_party_.end())
		return;

	it->second->update_offline_state(pid);
}

void party_manager::p2_p_join_party(uint32_t leader, uint32_t pid, uint8_t role)
{
	party_map::iterator it = map_pk_party_.find(leader);

	if (it != map_pk_party_.end())
	{
		it->second->p2_p_join(pid);

		if (role >= PARTY_ROLE_MAX_NUM)
			role = PARTY_ROLE_NORMAL;

		it->second->set_role(pid, role, true);
	}
	else
	{
		metin2::log::error("No such party with leader [{:d}]", leader);
	}
}

void party_manager::p2_p_quit_party(uint32_t pid)
{
	party_map::iterator it = map_pk_party_.find(pid);

	if (it != map_pk_party_.end())
	{
		it->second->p2_p_quit(pid);
	}
	else
	{
		metin2::log::error("No such party with member [{:d}]", pid);
	}
}

party * party_manager::p2_p_create_party(uint32_t pid)
{
	party_map::iterator it = map_pk_party_.find(pid);
	if (it != map_pk_party_.end())
		return it->second;

	party * pParty = new party;

	set_pk_pc_party_.insert(pParty);

	set_party_member(pid, pParty);
	pParty->set_pc_party(true);
	pParty->p2_p_join(pid);

	return pParty;
}

void party_manager::p2_p_delete_party(uint32_t pid)
{
	party_map::iterator it = map_pk_party_.find(pid);

	if (it != map_pk_party_.end())
	{
		set_pk_pc_party_.erase(it->second);
		delete it->second;
	}
	else
		metin2::log::error("PARTY P2PDeleteParty Cannot find party [{:d}]", pid);
}

party * party_manager::create_party(character * pLeader)
{
	if (pLeader->get_party())
		return pLeader->get_party();

	party * pParty = new party;

	if (pLeader->is_pc())
	{
		packet_party_create p;
		p.leader_pid = pLeader->player_id();

		db_clientdesc->db_packet(HEADER_GD_PARTY_CREATE, 0, &p, sizeof(packet_party_create));

		metin2::log::info( "PARTY: Create {} pid {:d}", pLeader->name(), pLeader->player_id());
		pParty->set_pc_party(true);
		pParty->join(pLeader->player_id());

		set_pk_pc_party_.insert(pParty);
	}
	else
	{
		pParty->set_pc_party(false);
		pParty->join(pLeader->get_vid());
	}

	pParty->link(pLeader);
	return (pParty);
}

void party_manager::delete_party(party * pParty)
{
	packet_party_delete p;
	p.leader_pid = pParty->leader_pid();

	db_clientdesc->db_packet(HEADER_GD_PARTY_DELETE, 0, &p, sizeof(packet_party_delete));

	set_pk_pc_party_.erase(pParty);
	delete pParty;
}

void party_manager::set_party_member(uint32_t dwPID, party * pParty)
{
	party_map::iterator it = map_pk_party_.find(dwPID);

	if (pParty == nullptr)
	{
		if (it != map_pk_party_.end())
			map_pk_party_.erase(it);
	}
	else
	{
		if (it != map_pk_party_.end())
		{
			if (it->second != pParty)
			{
				it->second->quit(dwPID);
				it->second = pParty;
			}
		}
		else
			map_pk_party_.insert(party_map::value_type(dwPID, pParty));
	}
}

EVENTINFO(party_update_event_info)
{
	uint32_t pid;

	party_update_event_info()
	: pid( 0 )
	{
	}
};

/////////////////////////////////////////////////////////////////////////////
//
// CParty begin!
//
/////////////////////////////////////////////////////////////////////////////
EVENTFUNC(party_update_event)
{
	party_update_event_info* info = dynamic_cast<party_update_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "party_update_event> <Factor> Null pointer" );
		return 0;
	}

	uint32_t pid = info->pid;
	character * leader = character_manager::instance().find_by_pid(pid);

	if (leader && leader->get_desc())
	{
		party * pParty = leader->get_party();

		if (pParty)
			pParty->update();
	}

	return passes_per_sec(3);
}

party::party()
{
	initialize();
}

party::~party()
{
	destroy();
}

void party::initialize()
{
	metin2::log::debug( "Party::Initialize");

	exp_distribution_mode_ = PARTY_EXP_DISTRIBUTION_NON_PARITY;
	chr_exp_centralize_ = nullptr;

	leader_pid_ = 0;

	event_update_ = nullptr;

	memset(&an_role_count_, 0, sizeof(an_role_count_));
	memset(&an_max_role_, 0, sizeof(an_max_role_));
	an_max_role_[PARTY_ROLE_LEADER] = 1;
	an_max_role_[PARTY_ROLE_NORMAL] = 32;

	party_start_time_ = get_dword_time();
	long_time_exp_bonus_ = 0;

	party_heal_time_ = get_dword_time();
	party_heal_ready_ = false;
	can_use_party_heal_ = false;

	leadership_ = 0;
	exp_bonus_ = 0;

	next_owner_ = member_map_.begin();

	count_near_party_member_ = 0;

	chr_leader_ = nullptr;
	pc_party_ = false;
	dungeon_ = nullptr;
	dungeon_for_only_party_ = nullptr;
}


void party::destroy()
{
	metin2::log::debug( "Party::Destroy");

	// If the party was created by a PC, the PID must be removed from the party manager's map.
	if (pc_party_)
	{
		for (member_map::iterator it = member_map_.begin(); it != member_map_.end(); ++it)
			party_manager::instance().set_party_member(it->first, nullptr);
	}

	event_cancel(&event_update_); 

	remove_bonus();

	member_map::iterator it = member_map_.begin();

	uint32_t dwTime = get_dword_time();

	while (it != member_map_.end())
	{
		member & rMember = it->second;
		++it;

		if (rMember.character)
		{
			if (rMember.character->get_desc())
			{
				packet_gc_party_remove p;
				p.header = HEADER_GC_PARTY_REMOVE;
				p.pid = rMember.character->player_id();
				rMember.character->get_desc()->packet(&p, sizeof(p));
				rMember.character->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티가 해산 되었습니다."));
			}
			else
			{
				// For NPCs, start an event that makes it disappear after a while when not in combat.
				rMember.character->set_last_attacked(dwTime);
				rMember.character->start_destroy_when_idle_event();
			}

			rMember.character->set_party(nullptr);
		}
	}

	member_map_.clear();
	next_owner_ = member_map_.begin();
	
	if (dungeon_for_only_party_ != nullptr)
	{
		dungeon_for_only_party_->set_party_null();
		dungeon_for_only_party_ = nullptr;
	}
}

void party::chat_packet_to_all_member(uint8_t type, const char* format, ...)
{
	char chatbuf[CHAT_MAX_LEN + 1];
	va_list args;

	va_start(args, format);
	vsnprintf(chatbuf, sizeof(chatbuf), format, args);
	va_end(args);

	member_map::iterator it;

	for (it = member_map_.begin(); it != member_map_.end(); ++it)
	{
		member & rMember = it->second;

		if (rMember.character)
		{
			if (rMember.character->get_desc())
			{
				rMember.character->chat_packet(type, "%s", chatbuf);
			}
		}
	}
}

uint32_t party::leader_pid()
{
	return leader_pid_;
}

uint32_t party::member_count()
{
	return member_map_.size();
}

void party::p2_p_join(uint32_t dwPID)
{
	member_map::iterator it = member_map_.find(dwPID);

	if (it == member_map_.end())
	{
		member Member;

		Member.character	= nullptr;
		Member.near		= false;

		if (member_map_.empty())
		{
			Member.role = PARTY_ROLE_LEADER;
			leader_pid_ = dwPID;
		}
		else
			Member.role = PARTY_ROLE_NORMAL;

		if (pc_party_)
		{
			character * ch = character_manager::instance().find_by_pid(dwPID);

			if (ch)
			{
				metin2::log::info( "PARTY: Join {} pid {:d} leader {:d}", ch->name(), dwPID, leader_pid_);
				Member.name = ch->name();

				if (Member.role == PARTY_ROLE_LEADER)
					leadership_ = ch->leadership_skill_level();
			}
			else
			{
				cci * pcci = p2_p_manager::instance().find_by_pid(dwPID);

				if (!pcci);
				else if (pcci->channel == metin2::config::server().channel)
					Member.name = pcci->name;
				else
					metin2::log::error("member is not in same channel PID: {:d} channel {:d}, this channel {:d}", dwPID, pcci->channel, metin2::config::server().channel);
			}
		}

		metin2::log::debug( "PARTY[{:d}] MemberCountChange {:d} -> {:d}", leader_pid(), member_count(), member_count()+1);

		member_map_.insert(member_map::value_type(dwPID, Member));

		if (member_map_.size() == 1)
			next_owner_ = member_map_.begin();

		if (pc_party_)
		{
			party_manager::instance().set_party_member(dwPID, this);
			send_party_join_one_to_all(dwPID);

			character * ch = character_manager::instance().find_by_pid(dwPID);

			if (ch)
				send_parameter(ch);
		}
	}

	if (dungeon_)
	{
		dungeon_->quit_party(this);
	}
}

void party::join(uint32_t dwPID)
{
	p2_p_join(dwPID);

	if (pc_party_)
	{
		packet_party_add p;
		p.leader_pid = leader_pid();
		p.pid = dwPID;
		p.state = PARTY_ROLE_NORMAL; // #0000790: [M2EU] CZ crash increase: initialization matters! 
		db_clientdesc->db_packet(HEADER_GD_PARTY_ADD, 0, &p, sizeof(p));
	}
}

void party::p2_p_quit(uint32_t dwPID)
{
	member_map::iterator it = member_map_.find(dwPID);

	if (it == member_map_.end())
		return;

	if (pc_party_)
		send_party_remove_one_to_all(dwPID);

	if (it == next_owner_)
		increase_ownership();

	if (pc_party_)
		remove_bonus_for_one(dwPID);

	character * ch = it->second.character;
	uint8_t bRole = it->second.role;

	member_map_.erase(it);

	metin2::log::debug( "PARTY[{:d}] MemberCountChange {:d} -> {:d}", leader_pid(), member_count(), member_count() - 1);

	if (bRole < PARTY_ROLE_MAX_NUM)
	{
		--an_role_count_[bRole];
	}
	else
	{
		metin2::log::error("ROLE_COUNT_QUIT_ERROR: INDEX({:d}) > MAX({:d})", bRole, PARTY_ROLE_MAX_NUM);
	}

	if (ch)
	{
		ch->set_party(nullptr);
		compute_role_point(ch, bRole, false);
	}

	if (pc_party_)
		party_manager::instance().set_party_member(dwPID, nullptr);

	// If the leader leaves, the party must be disbanded.
	if (bRole == PARTY_ROLE_LEADER)
		party_manager::instance().delete_party(this);

	// Do not add code below this!!! After DeleteParty above, this no longer exists.
}

void party::quit(uint32_t dwPID)
{
	// Always PC
	p2_p_quit(dwPID);

	if (pc_party_ && dwPID != leader_pid())
	{
		packet_party_remove p;
		p.pid = dwPID;
		p.leader_pid = leader_pid();
		db_clientdesc->db_packet(HEADER_GD_PARTY_REMOVE, 0, &p, sizeof(p));
	}
}

void party::link(character * pkChr)
{
	member_map::iterator it;

	if (pkChr->is_pc())
		it = member_map_.find(pkChr->player_id());
	else
		it = member_map_.find(pkChr->get_vid());

	if (it == member_map_.end())
	{
		metin2::log::error("{} is not member of this party", pkChr->name());
		return;
	}

	// Create an update event for player parties
	if (pc_party_ && !event_update_)
	{
		party_update_event_info* info = alloc_event_info<party_update_event_info>();
		info->pid = leader_pid_;
		event_update_ = event_create(party_update_event, info, passes_per_sec(3));
	}

	if (it->second.role == PARTY_ROLE_LEADER)
		chr_leader_ = pkChr;

	metin2::log::debug( "PARTY[{:d}] {} linked to party", leader_pid(), pkChr->name());

	it->second.character = pkChr;
	pkChr->set_party(this);

	if (pkChr->is_pc())
	{
		if (it->second.name.empty())
		{
			it->second.name = pkChr->name();
		}

		send_party_join_one_to_all(pkChr->player_id());

		send_party_join_all_to_one(pkChr);
		send_party_link_one_to_all(pkChr);
		send_party_link_all_to_one(pkChr);
		send_party_info_all_to_one(pkChr);
		send_party_info_one_to_all(pkChr);

		send_parameter(pkChr);

		if (get_dungeon() && get_dungeon()->map_index() == pkChr->map_index())
		{
			pkChr->set_dungeon(get_dungeon());
		}

		request_set_member_level(pkChr->player_id(), pkChr->level());

	}
}

void party::request_set_member_level(uint32_t pid, uint8_t level)
{
	packet_party_set_member_level p;
	p.leader_pid = leader_pid();
	p.pid = pid;
	p.level = level;
	db_clientdesc->db_packet(HEADER_GD_PARTY_SET_MEMBER_LEVEL, 0, &p, sizeof(packet_party_set_member_level));
}

void party::p2_p_set_member_level(uint32_t pid, uint8_t level)
{
	if (!pc_party_)
		return;

	member_map::iterator it;

	metin2::log::info( "PARTY P2PSetMemberLevel leader {:d} pid {:d} level {:d}", leader_pid(), pid, level);

	it = member_map_.find(pid);
	if (it != member_map_.end())
	{
		it->second.level = level;
	}
}

namespace 
{
	struct f_exit_dungeon
	{
		void operator()(character * ch)
		{
			ch->exit_to_saved_location();
		}
	};
}

void party::unlink(character * pkChr)
{
	member_map::iterator it;

	if (pkChr->is_pc())
		it = member_map_.find(pkChr->player_id());
	else
		it = member_map_.find(pkChr->get_vid());

	if (it == member_map_.end())
	{
		metin2::log::error("{} is not member of this party", pkChr->name());
		return;
	}

	if (pkChr->is_pc())
	{
		send_party_unlink_one_to_all(pkChr);
		//SendPartyUnlinkAllToOne(pkChr); // Disconnecting, so no need to send Unlink packets.

		if (it->second.role == PARTY_ROLE_LEADER)
		{
			remove_bonus();

			if (it->second.character->get_dungeon())
			{
				// TODO: if in a dungeon, the rest leave too
				f_exit_dungeon f;
				for_each_near_member(f);
			}
		}
	}

	if (it->second.role == PARTY_ROLE_LEADER)
		chr_leader_ = nullptr;

	it->second.character = nullptr;
	pkChr->set_party(nullptr);
}

void party::send_party_remove_one_to_all(uint32_t pid)
{
	member_map::iterator it;

	packet_gc_party_remove p;
	p.header = HEADER_GC_PARTY_REMOVE;
	p.pid = pid;

	for (it = member_map_.begin(); it != member_map_.end(); ++it)
	{
		if (it->second.character && it->second.character->get_desc())
			it->second.character->get_desc()->packet(&p, sizeof(p));
	}
}

void party::send_party_join_one_to_all(uint32_t pid)
{
	const member& r = member_map_[pid];

	packet_gc_party_add p;

	p.header = HEADER_GC_PARTY_ADD;
	p.pid = pid;
	strlcpy(p.name, r.name.c_str(), sizeof(p.name));

	for (member_map::iterator it = member_map_.begin(); it != member_map_.end(); ++it)
	{
		if (it->second.character && it->second.character->get_desc())
			it->second.character->get_desc()->packet(&p, sizeof(p));
	}
}

void party::send_party_join_all_to_one(character * ch)
{
	if (!ch->get_desc())
		return;

	packet_gc_party_add p;

	p.header = HEADER_GC_PARTY_ADD;
	p.name[CHARACTER_NAME_MAX_LEN] = '\0';

	for (member_map::iterator it = member_map_.begin();it!= member_map_.end(); ++it)
	{
		p.pid = it->first;
		strlcpy(p.name, it->second.name.c_str(), sizeof(p.name));
		ch->get_desc()->packet(&p, sizeof(p));
	}
}

void party::send_party_unlink_one_to_all(character * ch)
{
	if (!ch->get_desc())
		return;

	member_map::iterator it;

	packet_gc_party_link p;
	p.header = HEADER_GC_PARTY_UNLINK;
	p.pid = ch->player_id();
	p.vid = (uint32_t)ch->get_vid();

	for (it = member_map_.begin();it!= member_map_.end(); ++it)
	{
		if (it->second.character && it->second.character->get_desc())
		{
			it->second.character->get_desc()->packet(&p, sizeof(p));
		}
	}
}

void party::send_party_link_one_to_all(character * ch)
{
	if (!ch->get_desc())
		return;

	member_map::iterator it;

	packet_gc_party_link p;
	p.header = HEADER_GC_PARTY_LINK;
	p.vid = ch->get_vid();
	p.pid = ch->player_id();

	for (it = member_map_.begin();it!= member_map_.end(); ++it)
	{
		if (it->second.character && it->second.character->get_desc())
		{
			it->second.character->get_desc()->packet(&p, sizeof(p));
		}
	}
}

void party::send_party_link_all_to_one(character * ch)
{
	if (!ch->get_desc())
		return;

	member_map::iterator it;

	packet_gc_party_link p;
	p.header = HEADER_GC_PARTY_LINK;

	for (it = member_map_.begin();it!= member_map_.end(); ++it)
	{
		if (it->second.character)
		{
			p.vid = it->second.character->get_vid();
			p.pid = it->second.character->player_id();
			ch->get_desc()->packet(&p, sizeof(p));
		}
	}
}

void party::send_party_info_one_to_all(uint32_t pid)
{
	member_map::iterator it = member_map_.find(pid);

	if (it == member_map_.end())
		return;

	if (it->second.character)
	{
		send_party_info_one_to_all(it->second.character);
		return;
	}

	// Data Building
	packet_gc_party_update p;
	memset(&p, 0, sizeof(p));
	p.header = HEADER_GC_PARTY_UPDATE;
	p.pid = pid;
	p.percent_hp = 255;
	p.role = it->second.role;

	for (it = member_map_.begin();it!= member_map_.end(); ++it)
	{
		if ((it->second.character) && (it->second.character->get_desc()))
		{
			it->second.character->get_desc()->packet(&p, sizeof(p));
		}
	}
}

void party::send_party_info_one_to_all(character * ch)
{
	if (!ch->get_desc())
		return;

	member_map::iterator it;

	// Data Building
	packet_gc_party_update p;
	ch->build_update_party_packet(p);

	for (it = member_map_.begin();it!= member_map_.end(); ++it)
	{
		if ((it->second.character) && (it->second.character->get_desc()))
		{
			metin2::log::debug( "PARTY send info {}[{:d}] to {}[{:d}]", ch->name(), (uint32_t)ch->get_vid(), it->second.character->name(), (uint32_t)it->second.character->get_vid());
			it->second.character->get_desc()->packet(&p, sizeof(p));
		}
	}
}

void party::send_party_info_all_to_one(character * ch)
{
	member_map::iterator it;

	packet_gc_party_update p;

	for (it = member_map_.begin(); it != member_map_.end(); ++it)
	{
		if (!it->second.character)
		{
			uint32_t pid = it->first;
			memset(&p, 0, sizeof(p));
			p.header = HEADER_GC_PARTY_UPDATE;
			p.pid = pid;
			p.percent_hp = 255;
			p.role = it->second.role;
			ch->get_desc()->packet(&p, sizeof(p));
			continue;
		}

		it->second.character->build_update_party_packet(p);
		metin2::log::debug( "PARTY send info {}[{:d}] to {}[{:d}]", it->second.character->name(), (uint32_t)it->second.character->get_vid(), ch->name(), (uint32_t)ch->get_vid());
		ch->get_desc()->packet(&p, sizeof(p));
	}
}

void party::send_message(character * ch, uint8_t bMsg, uint32_t dwArg1, uint32_t dwArg2)
{
	if (ch->get_party() != this)
	{
		metin2::log::error("{} is not member of this party {}", ch->name(), static_cast<const void *>(this));
		return;
	}

	switch (bMsg)
	{
		case PM_ATTACK:
			break;

		case PM_RETURN:
			{
				member_map::iterator it = member_map_.begin();

				while (it != member_map_.end())
				{
					member & rMember = it->second;
					++it;

					character * pkChr;

					if ((pkChr = rMember.character) && ch != pkChr)
					{
						uint32_t x = dwArg1 + number(-500, 500);
						uint32_t y = dwArg2 + number(-500, 500);

						pkChr->set_victim(nullptr);
						pkChr->set_rotation_to_xy(x, y);

						if (pkChr->goto_(x, y))
						{
							character * victim = pkChr->victim();
							metin2::log::info( "{} {} RETURN victim {}", pkChr->name(), static_cast<const void *>(pkChr), static_cast<const void *>(victim));
							pkChr->send_move_packet(FUNC_WAIT, 0, 0, 0, 0);
						}
					}
				}
			}
			break;

		case PM_ATTACKED_BY:	// Attacked, ask the leader for help
			{
				// When there is no leader
				character * pkChrVictim = ch->victim();

				if (!pkChrVictim)
					return;

				member_map::iterator it = member_map_.begin();

				while (it != member_map_.end())
				{
					member & rMember = it->second;
					++it;

					character * pkChr;

					if ((pkChr = rMember.character) && ch != pkChr)
					{
						if (pkChr->can_begin_fight())
							pkChr->begin_fight(pkChrVictim);
					}
				}
			}
			break;

		case PM_AGGRO_INCREASE:
			{
				character * victim = character_manager::instance().find(dwArg2);

				if (!victim)
					return;

				member_map::iterator it = member_map_.begin();

				while (it != member_map_.end())
				{
					member & rMember = it->second;
					++it;

					character * pkChr;

					if ((pkChr = rMember.character) && ch != pkChr)
					{
						pkChr->update_aggr_point(victim, DAMAGE_TYPE_SPECIAL, dwArg1);
					}
				}
			}
			break;
	}
}

character * party::leader_character()
{
	return member_map_[leader_pid()].character;
}

bool party::set_role(uint32_t dwPID, uint8_t bRole, bool bSet)
{
	member_map::iterator it = member_map_.find(dwPID);

	if (it == member_map_.end())
	{
		return false;
	}

	character * ch = it->second.character;

	if (bSet)
	{
		if (an_role_count_[bRole] >= an_max_role_[bRole])
			return false;

		if (it->second.role != PARTY_ROLE_NORMAL)
			return false;

		it->second.role = bRole;

		if (ch && leader())
			compute_role_point(ch, bRole, true);

		if (bRole < PARTY_ROLE_MAX_NUM)
		{
			++an_role_count_[bRole];
		}
		else
		{
			metin2::log::error("ROLE_COUNT_INC_ERROR: INDEX({:d}) > MAX({:d})", bRole, PARTY_ROLE_MAX_NUM);
		}
	}
	else
	{
		if (it->second.role == PARTY_ROLE_LEADER)
			return false;

		if (it->second.role == PARTY_ROLE_NORMAL)
			return false;

		it->second.role = PARTY_ROLE_NORMAL;

		if (ch && leader())
			compute_role_point(ch, PARTY_ROLE_NORMAL, false);

		if (bRole < PARTY_ROLE_MAX_NUM)
		{
			--an_role_count_[bRole];
		}
		else
		{
			metin2::log::error("ROLE_COUNT_DEC_ERROR: INDEX({:d}) > MAX({:d})", bRole, PARTY_ROLE_MAX_NUM);
		}
	}

	send_party_info_one_to_all(dwPID);
	return true;
}

uint8_t party::role(uint32_t pid)
{
	member_map::iterator it = member_map_.find(pid);

	if (it == member_map_.end())
		return PARTY_ROLE_NORMAL;
	else
		return it->second.role;
}

void party::remove_bonus()
{
	member_map::iterator it;

	for (it = member_map_.begin(); it != member_map_.end(); ++it)
	{
		character * ch;

		if ((ch = it->second.character))
		{
			compute_role_point(ch, it->second.role, false);
		}

		it->second.near = false;
	}
}

void party::remove_bonus_for_one(uint32_t pid)
{
	member_map::iterator it = member_map_.find(pid);

	if (it == member_map_.end())
		return;

	character * ch;

	if ((ch = it->second.character))
		compute_role_point(ch, it->second.role, false);
}

void party::heal_party()
{
	// XXX DELETEME until the client is done
	{
		return;
	}
	if (!party_heal_ready_)
		return;

	member_map::iterator it;
	character * l = leader_character();

	for (it = member_map_.begin(); it != member_map_.end(); ++it)
	{
		if (!it->second.character)
			continue;

		character * ch = it->second.character;

		if (distance_approx(l->x()-ch->x(), l->y()-ch->y()) < PARTY_DEFAULT_RANGE)
		{
			ch->point_change(POINT_HP, ch->max_hp()-ch->hp());
			ch->point_change(POINT_SP, ch->max_sp()-ch->sp());
		}
	}

	party_heal_ready_ = false;
	party_heal_time_ = get_dword_time();
}

void party::summon_to_leader(uint32_t pid)
{
	int xy[12][2] = 
	{
		{	250,	0		},
		{	216,	125		},
		{	125,	216		},
		{	0,		250		},
		{	-125,	216		},
		{	-216,	125		},
		{	-250,	0		},
		{	-216,	-125	},
		{	-125,	-216	},
		{	0,		-250	},
		{	125,	-216	},
		{	216,	-125	},
	};

	int n = 0;
	int x[12], y[12];

	sectree_manager & s = sectree_manager::instance();
	character * l = leader_character();

	if (member_map_.find(pid) == member_map_.end())
	{
		l->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 소환하려는 대상을 찾을 수 없습니다."));
		return;
	}

	character * ch = member_map_[pid].character;

	if (!ch)
	{
		l->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 소환하려는 대상을 찾을 수 없습니다."));
		return;
	}

	if (!ch->can_summon(leadership_))
	{
		l->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 대상을 소환할 수 없습니다."));
		return;
	}

	for (int i = 0; i < 12; ++i)
	{
		pixel_position p;

		if (s.movable_position(l->map_index(), l->x() + xy [i][0], l->y() + xy[i][1], p))
		{
			x[n] = p.x;
			y[n] = p.y;
			n++;
		}
	}

	if (n == 0)
		l->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티원을 현재 위치로 소환할 수 없습니다."));
	else
	{
		int i = number(0, n - 1);
		ch->show(l->map_index(), x[i], y[i]);
		ch->stop();
	}
}

void party::increase_ownership()
{
	if (member_map_.empty())
	{
		next_owner_ = member_map_.begin();
		return;
	}

	if (next_owner_ == member_map_.end())
		next_owner_ = member_map_.begin();
	else
	{
		next_owner_++;

		if (next_owner_ == member_map_.end())
			next_owner_ = member_map_.begin();
	}
}

character * party::next_ownership(character * ch, int32_t x, int32_t y)
{
	if (next_owner_ == member_map_.end())
		return ch;

	int size = member_map_.size();

	while (size-- > 0)
	{
		character * pkMember = next_owner_->second.character;

		if (pkMember && distance_approx(pkMember->x() - x, pkMember->y() - y) < 3000)
		{
			increase_ownership();
			return pkMember;
		}

		increase_ownership();
	}

	return ch;
}

void party::compute_role_point(character * ch, uint8_t bRole, bool bAdd)
{
	if (!bAdd)
	{
		ch->point_change(POINT_PARTY_ATTACKER_BONUS, -ch->point(POINT_PARTY_ATTACKER_BONUS));
		ch->point_change(POINT_PARTY_TANKER_BONUS, -ch->point(POINT_PARTY_TANKER_BONUS));
		ch->point_change(POINT_PARTY_BUFFER_BONUS, -ch->point(POINT_PARTY_BUFFER_BONUS));
		ch->point_change(POINT_PARTY_SKILL_MASTER_BONUS, -ch->point(POINT_PARTY_SKILL_MASTER_BONUS));
		ch->point_change(POINT_PARTY_DEFENDER_BONUS, -ch->point(POINT_PARTY_DEFENDER_BONUS));
		ch->point_change(POINT_PARTY_HASTE_BONUS, -ch->point(POINT_PARTY_HASTE_BONUS));
		ch->compute_battle_points();
		return;
	}

	//SKILL_POWER_BY_LEVEL
	float k = (float) ch->skill_power_by_level( std::min(SKILL_MAX_LEVEL, leadership_ ) )/ 100.0f;
	//END_SKILL_POWER_BY_LEVEL

	switch (bRole)
	{
		case PARTY_ROLE_ATTACKER:
			{
				int iBonus = (int) (10 + 60 * k);

				if (ch->point(POINT_PARTY_ATTACKER_BONUS) != iBonus)
				{
					ch->point_change(POINT_PARTY_ATTACKER_BONUS, iBonus - ch->point(POINT_PARTY_ATTACKER_BONUS));
					ch->compute_points();
				}
			}
			break;

		case PARTY_ROLE_TANKER:
			{
				int iBonus = (int) (50 + 1450 * k);

				if (ch->point(POINT_PARTY_TANKER_BONUS) != iBonus)
				{
					ch->point_change(POINT_PARTY_TANKER_BONUS, iBonus - ch->point(POINT_PARTY_TANKER_BONUS));
					ch->compute_points();
				}
			}
			break;

		case PARTY_ROLE_BUFFER:
			{
				int iBonus = (int) (5 + 45 * k);

				if (ch->point(POINT_PARTY_BUFFER_BONUS) != iBonus)
				{
					ch->point_change(POINT_PARTY_BUFFER_BONUS, iBonus - ch->point(POINT_PARTY_BUFFER_BONUS));
				}
			}
			break;

		case PARTY_ROLE_SKILL_MASTER:
			{
				int iBonus = (int) (25 + 600 * k);

				if (ch->point(POINT_PARTY_SKILL_MASTER_BONUS) != iBonus)
				{
					ch->point_change(POINT_PARTY_SKILL_MASTER_BONUS, iBonus - ch->point(POINT_PARTY_SKILL_MASTER_BONUS));
					ch->compute_points();
				}
			}
			break;
		case PARTY_ROLE_HASTE:
			{
				int iBonus = (int) (1+5*k);
				if (ch->point(POINT_PARTY_HASTE_BONUS) != iBonus)
				{
					ch->point_change(POINT_PARTY_HASTE_BONUS, iBonus - ch->point(POINT_PARTY_HASTE_BONUS));
					ch->compute_points();
				}
			}
			break;
		case PARTY_ROLE_DEFENDER:
			{
				int iBonus = (int) (5+30*k);
				if (ch->point(POINT_PARTY_DEFENDER_BONUS) != iBonus)
				{
					ch->point_change(POINT_PARTY_DEFENDER_BONUS, iBonus - ch->point(POINT_PARTY_DEFENDER_BONUS));
					ch->compute_points();
				}
			}
			break;
	}
}

void party::update()
{
	metin2::log::debug( "PARTY::Update");

	character * l = leader_character();

	if (!l)
		return;

	member_map::iterator it;

	int iNearMember = 0;
	bool bResendAll = false;

	for (it = member_map_.begin(); it != member_map_.end(); ++it)
	{
		character * ch = it->second.character;

		it->second.near = false;

		if (!ch)
			continue;

		if (l->get_dungeon())
			it->second.near = l->get_dungeon() == ch->get_dungeon();
		else
			it->second.near = (distance_approx(l->x()-ch->x(), l->y()-ch->y()) < PARTY_DEFAULT_RANGE);

		if (it->second.near)
		{
			++iNearMember;
		}
	}

	if (iNearMember <= 1 && !l->get_dungeon())
	{
		for (it = member_map_.begin(); it != member_map_.end(); ++it)
			it->second.near = false;

		iNearMember = 0;
	}

	if (iNearMember != count_near_party_member_)
	{
		count_near_party_member_ = iNearMember;
		bResendAll = true;
	}

	leadership_ = l->leadership_skill_level();
	int iNewExpBonus = compute_party_bonus_exp_percent();

	if (exp_bonus_ != iNewExpBonus)
	{
		bResendAll = true;
		exp_bonus_ = iNewExpBonus;
	}

	bool bLongTimeExpBonusChanged = false;

	// After enough time since the party was formed, an experience bonus is granted.
	if (!long_time_exp_bonus_ && (get_dword_time() - party_start_time_ > PARTY_ENOUGH_MINUTE_FOR_EXP_BONUS * 60 * 1000))
	{
		bLongTimeExpBonusChanged = true;
		long_time_exp_bonus_ = 5;
		bResendAll = true;
	}

	for (it = member_map_.begin(); it != member_map_.end(); ++it)
	{
		character * ch = it->second.character;

		if (!ch)
			continue;

		if (bLongTimeExpBonusChanged && ch->get_desc())
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("파티의 협동력이 높아져 지금부터 추가 경험치 보너스를 받습니다."));

		bool bNear = it->second.near;

		compute_role_point(ch, it->second.role, bNear);

		if (bNear)
		{
			if (!bResendAll)
				send_party_info_one_to_all(ch);
		}
	}

	// PARTY_ROLE_LIMIT_LEVEL_BUG_FIX
	an_max_role_[PARTY_ROLE_ATTACKER]	 = leadership_ >= 10 ? 1 : 0;
	an_max_role_[PARTY_ROLE_HASTE]	 = leadership_ >= 20 ? 1 : 0;
	an_max_role_[PARTY_ROLE_TANKER]	 = leadership_ >= 20 ? 1 : 0;
	an_max_role_[PARTY_ROLE_BUFFER]	 = leadership_ >= 25 ? 1 : 0;
	an_max_role_[PARTY_ROLE_SKILL_MASTER] = leadership_ >= 35 ? 1 : 0;
	an_max_role_[PARTY_ROLE_DEFENDER] 	 = leadership_ >= 40 ? 1 : 0;
	an_max_role_[PARTY_ROLE_ATTACKER]	+= leadership_ >= 40 ? 1 : 0;
	// END_OF_PARTY_ROLE_LIMIT_LEVEL_BUG_FIX

	// Party Heal Update
	if (!party_heal_ready_)
	{
		if (!can_use_party_heal_ && leadership_ >= 18)
			party_heal_time_ = get_dword_time();

		can_use_party_heal_ = leadership_ >= 18; // Leadership 18 or higher can use heal.

		// Leadership 40 or higher has a shorter party heal cooldown.
		uint32_t PartyHealCoolTime = (leadership_ >= 40) ? PARTY_HEAL_COOLTIME_SHORT * 60 * 1000 : PARTY_HEAL_COOLTIME_LONG * 60 * 1000;

		if (can_use_party_heal_)
		{
			if (get_dword_time() > party_heal_time_ + PartyHealCoolTime)
			{
				party_heal_ready_ = true;

				// send heal ready
			}
		}
	}

	if (bResendAll)
	{
		for (member_map::iterator it = member_map_.begin(); it != member_map_.end(); ++it)
			if (it->second.character)
				send_party_info_one_to_all(it->second.character);
	}
}

void party::update_online_state(uint32_t dwPID, const char* name)
{
	member& r = member_map_[dwPID];

	packet_gc_party_add p;

	p.header = HEADER_GC_PARTY_ADD;
	p.pid = dwPID;
	r.name = name;
	strlcpy(p.name, name, sizeof(p.name));

	for (member_map::iterator it = member_map_.begin(); it != member_map_.end(); ++it)
	{
		if (it->second.character && it->second.character->get_desc())
			it->second.character->get_desc()->packet(&p, sizeof(p));
	}
}
void party::update_offline_state(uint32_t dwPID)
{

	packet_gc_party_add p;
	p.header = HEADER_GC_PARTY_ADD;
	p.pid = dwPID;
	memset(p.name, 0, CHARACTER_NAME_MAX_LEN+1);

	for (member_map::iterator it = member_map_.begin(); it != member_map_.end(); ++it)
	{
		if (it->second.character && it->second.character->get_desc())
			it->second.character->get_desc()->packet(&p, sizeof(p));
	}
}

int party::flag(const std::string& name)
{
	flag_map::iterator it = map_i_flag_.find(name);

	if (it != map_i_flag_.end())
	{
		return it->second;
	}

	return 0;
}

void party::set_flag(const std::string& name, int value)
{
	flag_map::iterator it = map_i_flag_.find(name);

	if (it == map_i_flag_.end())
	{
		map_i_flag_.insert(make_pair(name, value));
	}
	else if (it->second != value)
	{
		it->second = value;
	}
}

void party::set_dungeon(dungeon * pDungeon)
{
	dungeon_ = pDungeon;
	map_i_flag_.clear();
}

dungeon * party::get_dungeon()
{
	return dungeon_;
}

void party::set_dungeon_for_only_party(dungeon * pDungeon)
{
	dungeon_for_only_party_ = pDungeon;
}

dungeon * party::dungeon_for_only_party()
{
	return dungeon_for_only_party_;
}


bool party::is_position_near_leader(character * ch)
{
	if (!chr_leader_)
		return false;

	if (distance_approx(ch->x() - chr_leader_->x(), ch->y() - chr_leader_->y()) >= PARTY_DEFAULT_RANGE)
		return false;

	return true;
}


int party::exp_bonus_percent()
{
	if (near_member_count() <= 1)
		return 0;

	return exp_bonus_ + long_time_exp_bonus_;
}

uint8_t party::count_member_by_vnum(uint32_t dwVnum)
{
	if (pc_party_)
		return 0;

	character * tch;
	uint8_t bCount = 0;

	member_map::iterator it;

	for (it = member_map_.begin(); it != member_map_.end(); ++it)
	{
		if (!(tch = it->second.character))
			continue;

		if (tch->is_pc())
			continue;

		if (tch->get_mob_table().vnum == dwVnum)
			++bCount;
	}

	return bCount;
}

void party::send_parameter(character * ch)
{
	packet_gc_party_parameter p;

	p.header = HEADER_GC_PARTY_PARAMETER;
	p.distribute_mode = exp_distribution_mode_;

	desc * d = ch->get_desc();

	if (d)
	{
		d->packet(&p, sizeof(packet_gc_party_parameter));
	}
}

void party::send_parameter_to_all()
{
	if (!pc_party_)
		return;

	member_map::iterator it;

	for (it = member_map_.begin(); it != member_map_.end(); ++it)
		if (it->second.character)
			send_parameter(it->second.character);
}

void party::set_parameter(int iMode)
{
	if (iMode >= PARTY_EXP_DISTRIBUTION_MAX_NUM)
	{
		metin2::log::error("Invalid exp distribution mode {:d}", iMode);
		return;
	}

	exp_distribution_mode_ = iMode;
	send_parameter_to_all();
}

int party::exp_distribution_mode()
{
	return exp_distribution_mode_;
}

character * party::exp_centralize_character()
{
	return chr_exp_centralize_;
}

uint8_t party::member_max_level()
{
	uint8_t bMax = 0;

	auto it = member_map_.begin();
	while (it!=member_map_.end())
	{
		if (!it->second.level)
		{
			++it;
			continue;
		}

		if (!bMax)
			bMax = it->second.level;
		else if (it->second.level)
			bMax = std::max<int>(bMax, it->second.level);
		++it;
	}
	return bMax;
}

int party::compute_party_bonus_exp_percent()
{
	if (near_member_count() <= 1)
		return 0;

	character * leader = leader_character();

	int iBonusPartyExpFromItem = 0;

	// UPGRADE_PARTY_BONUS
	int iMemberCount=std::min<int>(8, near_member_count());

	if (leader && (leader->is_equip_unique_item(UNIQUE_ITEM_PARTY_BONUS_EXP) || leader->is_equip_unique_item(UNIQUE_ITEM_PARTY_BONUS_EXP_MALL)
		|| leader->is_equip_unique_item(UNIQUE_ITEM_PARTY_BONUS_EXP_GIFT) || leader->is_equip_unique_group(10010)))
	{
		// Need to verify the Chinese-side Yukdo application.
		iBonusPartyExpFromItem = 30;
	}

	return iBonusPartyExpFromItem + chn_ai_party_bonus_exp_percent_by_member_count[iMemberCount];
	// END_OF_UPGRADE_PARTY_BONUS
}
}
