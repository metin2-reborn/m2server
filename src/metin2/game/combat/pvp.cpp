#include <metin2/game/combat/pvp.h>

#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/characters/char.h>
#include <metin2/game/characters/char_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/crc32.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <unordered_set>

namespace metin2::game::inline combat {

pvp::pvp(uint32_t dwPID1, uint32_t dwPID2)
{
	if (dwPID1 > dwPID2)
	{
		players_[0].pid = dwPID1;
		players_[1].pid = dwPID2;
		players_[0].agree = true;
	}
	else
	{
		players_[0].pid = dwPID2;
		players_[1].pid = dwPID1;
		players_[1].agree = true;
	}

	uint32_t adwID[2];
	adwID[0] = players_[0].pid;
	adwID[1] = players_[1].pid;
	crc_ = get_fast_hash((const char *) &adwID, 8);
	revenge_ = false;

	set_last_fight_time();
}

pvp::pvp(pvp & k)
{
	players_[0] = k.players_[0];
	players_[1] = k.players_[1];

	crc_ = k.crc_;
	revenge_ = k.revenge_;

	set_last_fight_time();
}

pvp::~pvp()
{
}

void pvp::packet(bool bDelete)
{
	if (!players_[0].vid || !players_[1].vid)
	{
		if (bDelete)
			metin2::log::error("null vid when removing {:d} {:d}", players_[0].vid, players_[0].vid);

		return;
	}

	packet_gcpvp pack;

	pack.header = HEADER_GC_PVP;

	if (bDelete)
	{
		pack.mode = PVP_MODE_NONE;
		pack.vid_src = players_[0].vid;
		pack.vid_dst = players_[1].vid;
	}
	else if (is_fight())
	{
		pack.mode = PVP_MODE_FIGHT;
		pack.vid_src = players_[0].vid;
		pack.vid_dst = players_[1].vid;
	}
	else
	{
		pack.mode = revenge_ ? PVP_MODE_REVENGE : PVP_MODE_AGREE;

		if (players_[0].agree)
		{
			pack.vid_src = players_[0].vid;
			pack.vid_dst = players_[1].vid;
		}
		else
		{
			pack.vid_src = players_[1].vid;
			pack.vid_dst = players_[0].vid;
		}
	}

	const desc_manager::desc_set & c_rSet = desc_manager::instance().client_set();
	desc_manager::desc_set::const_iterator it = c_rSet.begin();

	while (it != c_rSet.end())
	{
		desc * d = *it++;

		if (d->is_phase(PHASE_GAME) || d->is_phase(PHASE_DEAD))
			d->packet(&pack, sizeof(pack));
	}
}

bool pvp::agree(uint32_t dwPID)
{
	players_[players_[0].pid != dwPID ? 1 : 0].agree = true;

	if (is_fight())
	{
		packet();
		return true;
	}

	return false;
}

bool pvp::is_fight()
{
	return (players_[0].agree == players_[1].agree) && players_[0].agree;
}

void pvp::win(uint32_t dwPID)
{
	int iSlot = players_[0].pid != dwPID ? 1 : 0;

	revenge_ = true;

	players_[iSlot].agree = true; // Agree automatically
	players_[!iSlot].can_revenge = true;
	players_[!iSlot].agree = false;

	packet();
}

void pvp::set_vid(uint32_t dwPID, uint32_t dwVID)
{
	if (players_[0].pid == dwPID)
		players_[0].vid = dwVID;
	else
		players_[1].vid = dwVID;
}

void pvp::set_last_fight_time()
{
	last_fight_time_ = get_dword_time();
}

uint32_t pvp::last_fight_time()
{
	return last_fight_time_;
}

pvp_manager::pvp_manager()
{
}

pvp_manager::~pvp_manager()
{
}

void pvp_manager::insert(character * pkChr, character * pkVictim)
{
	if (pkChr->is_dead() || pkVictim->is_dead())
		return;

	pvp kPVP(pkChr->player_id(), pkVictim->player_id());

	pvp * pkPVP;

	if ((pkPVP = find(kPVP.crc_)))
	{
		// If revenge is possible, fight right away!
		if (pkPVP->agree(pkChr->player_id()))
		{
			pkVictim->chat_packet(CHAT_TYPE_INFO, lc_text("%s님과의 대결 시작!"), pkChr->name());
			pkChr->chat_packet(CHAT_TYPE_INFO, lc_text("%s님과의 대결 시작!"), pkVictim->name());
		}
		return;
	}

	auto pkNewPVP = std::make_unique<pvp>(kPVP);
	pkPVP = pkNewPVP.get();

	pkPVP->set_vid(pkChr->player_id(), pkChr->get_vid());
	pkPVP->set_vid(pkVictim->player_id(), pkVictim->get_vid());

	map_pk_pvp_.emplace(pkPVP->crc_, std::move(pkNewPVP));

	map_pk_pvp_set_by_id_[pkChr->player_id()].insert(pkPVP);
	map_pk_pvp_set_by_id_[pkVictim->player_id()].insert(pkPVP);

	pkPVP->packet();

	char msg[CHAT_MAX_LEN + 1];
	snprintf(msg, sizeof(msg), lc_text("%s님이 대결신청을 했습니다. 승낙하려면 대결동의를 하세요."), pkChr->name());

	pkVictim->chat_packet(CHAT_TYPE_INFO, msg);
	pkChr->chat_packet(CHAT_TYPE_INFO, lc_text("%s에게 대결신청을 했습니다."), pkVictim->name());

	// NOTIFY_PVP_MESSAGE
	desc * pkVictimDesc = pkVictim->get_desc();
	if (pkVictimDesc)
	{
		packet_gc_whisper pack;

		int len = std::min<int>(CHAT_MAX_LEN, strlen(msg) + 1);

		pack.header = HEADER_GC_WHISPER;
		pack.size = sizeof(packet_gc_whisper) + len;
		pack.type = WHISPER_TYPE_SYSTEM;
		strlcpy(pack.name_from, pkChr->name(), sizeof(pack.name_from));

		temp_buffer buf;

		buf.write(&pack, sizeof(packet_gc_whisper));
		buf.write(msg, len);

		pkVictimDesc->packet(buf.read_peek(), buf.size());
	}	
	// END_OF_NOTIFY_PVP_MESSAGE
}

void pvp_manager::connect_ex(character * pkChr, bool bDisconnect)
{
	pvp_set_map::iterator it = map_pk_pvp_set_by_id_.find(pkChr->player_id());

	if (it == map_pk_pvp_set_by_id_.end())
		return;

	uint32_t dwVID = bDisconnect ? 0 : pkChr->get_vid();

	std::unordered_set<pvp*>::iterator it2 = it->second.begin();

	while (it2 != it->second.end())
	{
		pvp * pkPVP = *it2++;
		pkPVP->set_vid(pkChr->player_id(), dwVID);
	}
}

void pvp_manager::connect(character * pkChr)
{
	connect_ex(pkChr, false);
}

void pvp_manager::disconnect(character * pkChr)
{
}

// Return value: 0 = PK, 1 = PVP
// If PVP is returned, experience or items are dropped; if PK, they are not.
bool pvp_manager::dead(character * pkChr, uint32_t dwKillerPID)
{
	pvp_set_map::iterator it = map_pk_pvp_set_by_id_.find(pkChr->player_id());

	if (it == map_pk_pvp_set_by_id_.end())
		return false;

	bool found = false;

	metin2::log::debug( "PVPManager::Dead {:d}", pkChr->player_id());
	std::unordered_set<pvp*>::iterator it2 = it->second.begin();

	while (it2 != it->second.end())
	{
		pvp * pkPVP = *it2++;

		uint32_t dwCompanionPID;

		if (pkPVP->players_[0].pid == pkChr->player_id())
			dwCompanionPID = pkPVP->players_[1].pid;
		else
			dwCompanionPID = pkPVP->players_[0].pid;

		if (dwCompanionPID == dwKillerPID)
		{
			if (pkPVP->is_fight())
			{
				pkPVP->set_last_fight_time();
				pkPVP->win(dwKillerPID);
				found = true;
				break;
			}
			else if (get_dword_time() - pkPVP->last_fight_time() <= 15000)
			{
				found = true;
				break;
			}
		}
	}

	return found;
}

bool pvp_manager::can_attack(character * pkChr, character * pkVictim)
{
	switch (pkVictim->get_char_type())
	{
		case CHAR_TYPE_NPC:
		case CHAR_TYPE_WARP:
		case CHAR_TYPE_GOTO:
			return false;
	}

	if (pkChr == pkVictim)  // I'm trying to hit myself -_-
		return false;

	if (pkVictim->is_npc() && pkChr->is_npc() && !pkChr->is_guard_npc())
		return false;

	if( true == pkChr->is_horse_riding() )
	{
		if( pkChr->horse_level() > 0 && 1 == pkChr->horse_grade() ) 
			return false;
	}
	else
	{
		switch( pkChr->get_mount_vnum() )
		{
			case 0:
			case 20030:
			case 20110:
			case 20111:
			case 20112:
			case 20113:
			case 20114:
			case 20115:
			case 20116:
			case 20117:
			case 20118:
				//New advanced mount
			case 20205:
			case 20206:
			case 20207:
			case 20208:
			case 20209:
			case 20210:
			case 20211:
			case 20212:
			case 20119:		// Ramadan black horse
			case 20219:		// Ramadan black horse clone (for Halloween)
			case 20220:		// Christmas mount
			case 20221:		// Armored white bear
			case 20222:		// Armored panda
			case 20120:
			case 20121:
			case 20122:
			case 20123:
			case 20124:
			case 20125:
			case 20214:		// Wild armored reindeer (male)	
			case 20215:		// Brave armored reindeer (male)	
			case 20217:		// Wild armored reindeer (female)	
			case 20218:		// Brave armored reindeer (female)	
			case 20224:
			case 20229:
			case 20225:
			case 20230:
				break;

			default:
				return false;
		}
	}

	if (pkVictim->is_npc() || pkChr->is_npc())
	{
		return true;
	}

	if (pkVictim->is_observer_mode() || pkChr->is_observer_mode())
		return false;

	{
		uint8_t bMapEmpire = sectree_manager::instance().empire_from_map_index(pkChr->map_index());

		if ( (pkChr->pk_mode() == PK_MODE_PROTECT && pkChr->empire() == bMapEmpire) ||
				(pkVictim->pk_mode() == PK_MODE_PROTECT && pkVictim->empire() == bMapEmpire) )
		{
			return false;
		}
	}

	if (pkChr->empire() != pkVictim->empire())
	{

		return true;
	}

	bool beKillerMode = false;

	if (pkVictim->get_party() && pkVictim->get_party() == pkChr->get_party())
	{
		return false;
		// Cannot attack same party on any pvp model
	}
	else
	{
		if (pkVictim->is_killer_mode())
		{
			return true;
		}

		if (pkChr->alignment() < 0 && pkVictim->alignment() >= 0)
		{
		    if (metin2::config::game().protect_normal_player)
		    {
			// Outlaws cannot attack good players in peace mode.
			if (PK_MODE_PEACE == pkVictim->pk_mode())
			    return false;
		    }
		}


		switch (pkChr->pk_mode())
		{
			case PK_MODE_PEACE:
			case PK_MODE_REVENGE:
				// Cannot attack same guild
				if (pkVictim->get_guild() && pkVictim->get_guild() == pkChr->get_guild())
					break;

				if (pkChr->pk_mode() == PK_MODE_REVENGE)
				{
					if (pkChr->alignment() < 0 && pkVictim->alignment() >= 0)
					{
						pkChr->set_killer_mode(true);
						return true;
					}
					else if (pkChr->alignment() >= 0 && pkVictim->alignment() < 0)
						return true;
				}
				break;

			case PK_MODE_GUILD:
				// Same implementation from PK_MODE_FREE except for attacking same guild
				if (!pkChr->get_guild() || (pkVictim->get_guild() != pkChr->get_guild()))
				{
					if (pkVictim->alignment() >= 0)
						pkChr->set_killer_mode(true);
					else if (pkChr->alignment() < 0 && pkVictim->alignment() < 0)
						pkChr->set_killer_mode(true);

					return true;
				}
				break;

			case PK_MODE_FREE:
				if (pkVictim->alignment() >= 0)
					pkChr->set_killer_mode(true);
				else if (pkChr->alignment() < 0 && pkVictim->alignment() < 0)
					pkChr->set_killer_mode(true);

				return true;
				break;
		}
	}

	pvp kPVP(pkChr->player_id(), pkVictim->player_id());
	pvp * pkPVP = find(kPVP.crc_);

	if (!pkPVP || !pkPVP->is_fight())
	{
		if (beKillerMode)
			pkChr->set_killer_mode(true);

		return (beKillerMode);
	}

	pkPVP->set_last_fight_time();
	return true;
}

pvp * pvp_manager::find(uint32_t dwCRC)
{
	auto it = map_pk_pvp_.find(dwCRC);

	if (it == map_pk_pvp_.end())
		return nullptr;

	return it->second.get();
}

void pvp_manager::delete_(pvp * pkPVP)
{
	auto it = map_pk_pvp_.find(pkPVP->crc_);

	if (it == map_pk_pvp_.end())
		return;

	map_pk_pvp_set_by_id_[pkPVP->players_[0].pid].erase(pkPVP);
	map_pk_pvp_set_by_id_[pkPVP->players_[1].pid].erase(pkPVP);
	map_pk_pvp_.erase(it);
}

void pvp_manager::send_list(desc * d)
{
	auto it = map_pk_pvp_.begin();

	uint32_t dwVID = d->get_character()->get_vid();

	packet_gcpvp pack;

	pack.header = HEADER_GC_PVP;

	while (it != map_pk_pvp_.end())
	{
		pvp * pkPVP = (it++)->second.get();

		if (!pkPVP->players_[0].vid || !pkPVP->players_[1].vid)
			continue;

		// Send only when both VIDs exist.
		if (pkPVP->is_fight())
		{
			pack.mode = PVP_MODE_FIGHT;
			pack.vid_src = pkPVP->players_[0].vid;
			pack.vid_dst = pkPVP->players_[1].vid;
		}
		else
		{
			pack.mode = pkPVP->revenge_ ? PVP_MODE_REVENGE : PVP_MODE_AGREE;

			if (pkPVP->players_[0].agree)
			{
				pack.vid_src = pkPVP->players_[0].vid;
				pack.vid_dst = pkPVP->players_[1].vid;
			}
			else
			{
				pack.vid_src = pkPVP->players_[1].vid;
				pack.vid_dst = pkPVP->players_[0].vid;
			}
		}

		d->packet(&pack, sizeof(pack));
		metin2::log::debug( "PVPManager::SendList {:d} {:d}", pack.vid_src, pack.vid_dst);

		if (pkPVP->players_[0].vid == dwVID)
		{
			character * ch = character_manager::instance().find(pkPVP->players_[1].vid);
			if (ch && ch->get_desc())
			{
				desc * d = ch->get_desc();
				d->packet(&pack, sizeof(pack));
			}
		}
		else if (pkPVP->players_[1].vid == dwVID)
		{
			character * ch = character_manager::instance().find(pkPVP->players_[0].vid);
			if (ch && ch->get_desc())
			{
				desc * d = ch->get_desc();
				d->packet(&pack, sizeof(pack));
			}
		}
	}
}

void pvp_manager::process()
{
	auto it = map_pk_pvp_.begin();

	while (it != map_pk_pvp_.end())
	{
		pvp * pvp = (it++)->second.get();

		if (get_dword_time() - pvp->last_fight_time() > 600000) // If there has been no fighting for 10 minutes or more
		{
			pvp->packet(true);
			delete_(pvp);
		}
	}
}
}
