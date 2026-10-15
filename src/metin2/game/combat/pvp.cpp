#include <metin2/game/combat/pvp.h>

#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
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

pvp::pvp(uint32_t pid1, uint32_t pid2)
{
	if (pid1 > pid2)
	{
		players_[0].pid = pid1;
		players_[1].pid = pid2;
		players_[0].agree = true;
	}
	else
	{
		players_[0].pid = pid2;
		players_[1].pid = pid1;
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

void pvp::packet(bool removing)
{
	if (!players_[0].vid || !players_[1].vid)
	{
		if (removing)
			metin2::log::error("null vid when removing {:d} {:d}", players_[0].vid, players_[0].vid);

		return;
	}

	packet_gcpvp pack;

	pack.header = HEADER_GC_PVP;

	if (removing)
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

bool pvp::agree(uint32_t pid_value)
{
	players_[players_[0].pid != pid_value ? 1 : 0].agree = true;

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

void pvp::win(uint32_t pid_value)
{
	int iSlot = players_[0].pid != pid_value ? 1 : 0;

	revenge_ = true;

	players_[iSlot].agree = true;
	players_[!iSlot].can_revenge = true;
	players_[!iSlot].agree = false;

	packet();
}

void pvp::set_vid(uint32_t pid_value, uint32_t vid_value)
{
	if (players_[0].pid == pid_value)
		players_[0].vid = vid_value;
	else
		players_[1].vid = vid_value;
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

void pvp_manager::insert(character * chr, character * victim)
{
	if (chr->is_dead() || victim->is_dead())
		return;

	pvp kPVP(chr->player_id(), victim->player_id());

	pvp * pkPVP;

	if ((pkPVP = find(kPVP.crc_)))
	{
		if (pkPVP->agree(chr->player_id()))
		{
			victim->chat_packet(CHAT_TYPE_INFO, lc_text("%s님과의 대결 시작!"), chr->name());
			chr->chat_packet(CHAT_TYPE_INFO, lc_text("%s님과의 대결 시작!"), victim->name());
		}
		return;
	}

	auto pkNewPVP = std::make_unique<pvp>(kPVP);
	pkPVP = pkNewPVP.get();

	pkPVP->set_vid(chr->player_id(), chr->get_vid());
	pkPVP->set_vid(victim->player_id(), victim->get_vid());

	map_pk_pvp_.emplace(pkPVP->crc_, std::move(pkNewPVP));

	map_pk_pvp_set_by_id_[chr->player_id()].insert(pkPVP);
	map_pk_pvp_set_by_id_[victim->player_id()].insert(pkPVP);

	pkPVP->packet();

	char msg[CHAT_MAX_LEN + 1];
	snprintf(msg, sizeof(msg), lc_text("%s님이 대결신청을 했습니다. 승낙하려면 대결동의를 하세요."), chr->name());

	victim->chat_packet(CHAT_TYPE_INFO, msg);
	chr->chat_packet(CHAT_TYPE_INFO, lc_text("%s에게 대결신청을 했습니다."), victim->name());

	desc * pkVictimDesc = victim->get_desc();
	if (pkVictimDesc)
	{
		packet_gc_whisper pack;

		int len = std::min<int>(CHAT_MAX_LEN, strlen(msg) + 1);

		pack.header = HEADER_GC_WHISPER;
		pack.size = sizeof(packet_gc_whisper) + len;
		pack.type = WHISPER_TYPE_SYSTEM;
		strlcpy(pack.name_from, chr->name(), sizeof(pack.name_from));

		temp_buffer buf;

		buf.write(&pack, sizeof(packet_gc_whisper));
		buf.write(msg, len);

		pkVictimDesc->packet(buf.read_peek(), buf.size());
	}	
}

void pvp_manager::connect_ex(character * chr, bool disconnect)
{
	pvp_set_map::iterator it = map_pk_pvp_set_by_id_.find(chr->player_id());

	if (it == map_pk_pvp_set_by_id_.end())
		return;

	uint32_t dwVID = disconnect ? 0 : chr->get_vid();

	std::unordered_set<pvp*>::iterator it2 = it->second.begin();

	while (it2 != it->second.end())
	{
		pvp * pkPVP = *it2++;
		pkPVP->set_vid(chr->player_id(), dwVID);
	}
}

void pvp_manager::connect(character * chr)
{
	connect_ex(chr, false);
}

void pvp_manager::disconnect(character * chr)
{
}

bool pvp_manager::dead(character * chr, uint32_t killer_pid)
{
	pvp_set_map::iterator it = map_pk_pvp_set_by_id_.find(chr->player_id());

	if (it == map_pk_pvp_set_by_id_.end())
		return false;

	bool found = false;

	metin2::log::debug( "PVPManager::Dead {:d}", chr->player_id());
	std::unordered_set<pvp*>::iterator it2 = it->second.begin();

	while (it2 != it->second.end())
	{
		pvp * pkPVP = *it2++;

		uint32_t dwCompanionPID;

		if (pkPVP->players_[0].pid == chr->player_id())
			dwCompanionPID = pkPVP->players_[1].pid;
		else
			dwCompanionPID = pkPVP->players_[0].pid;

		if (dwCompanionPID == killer_pid)
		{
			if (pkPVP->is_fight())
			{
				pkPVP->set_last_fight_time();
				pkPVP->win(killer_pid);
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

bool pvp_manager::can_attack(character * chr, character * victim)
{
	switch (victim->get_char_type())
	{
		case CHAR_TYPE_NPC:
		case CHAR_TYPE_WARP:
		case CHAR_TYPE_GOTO:
			return false;
	}

	if (chr == victim)
		return false;

	if (victim->is_npc() && chr->is_npc() && !chr->is_guard_npc())
		return false;

	if( true == chr->is_horse_riding() )
	{
		if( chr->horse_level() > 0 && 1 == chr->horse_grade() ) 
			return false;
	}
	else
	{
		switch( chr->get_mount_vnum() )
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
			case 20205:
			case 20206:
			case 20207:
			case 20208:
			case 20209:
			case 20210:
			case 20211:
			case 20212:
			case 20119:
			case 20219:
			case 20220:
			case 20221:
			case 20222:
			case 20120:
			case 20121:
			case 20122:
			case 20123:
			case 20124:
			case 20125:
			case 20214:
			case 20215:
			case 20217:
			case 20218:
			case 20224:
			case 20229:
			case 20225:
			case 20230:
				break;

			default:
				return false;
		}
	}

	if (victim->is_npc() || chr->is_npc())
	{
		return true;
	}

	if (victim->is_observer_mode() || chr->is_observer_mode())
		return false;

	{
		uint8_t bMapEmpire = sectree_manager::instance().empire_from_map_index(chr->map_index());

		if ( (chr->pk_mode() == PK_MODE_PROTECT && chr->empire() == bMapEmpire) ||
				(victim->pk_mode() == PK_MODE_PROTECT && victim->empire() == bMapEmpire) )
		{
			return false;
		}
	}

	if (chr->empire() != victim->empire())
	{

		return true;
	}

	bool beKillerMode = false;

	if (victim->get_party() && victim->get_party() == chr->get_party())
	{
		return false;
	}
	else
	{
		if (victim->is_killer_mode())
		{
			return true;
		}

		if (chr->alignment() < 0 && victim->alignment() >= 0)
		{
		    if (metin2::config::game().protect_normal_player)
		    {
			if (PK_MODE_PEACE == victim->pk_mode())
			    return false;
		    }
		}


		switch (chr->pk_mode())
		{
			case PK_MODE_PEACE:
			case PK_MODE_REVENGE:
				if (victim->get_guild() && victim->get_guild() == chr->get_guild())
					break;

				if (chr->pk_mode() == PK_MODE_REVENGE)
				{
					if (chr->alignment() < 0 && victim->alignment() >= 0)
					{
						chr->set_killer_mode(true);
						return true;
					}
					else if (chr->alignment() >= 0 && victim->alignment() < 0)
						return true;
				}
				break;

			case PK_MODE_GUILD:
				if (!chr->get_guild() || (victim->get_guild() != chr->get_guild()))
				{
					if (victim->alignment() >= 0)
						chr->set_killer_mode(true);
					else if (chr->alignment() < 0 && victim->alignment() < 0)
						chr->set_killer_mode(true);

					return true;
				}
				break;

			case PK_MODE_FREE:
				if (victim->alignment() >= 0)
					chr->set_killer_mode(true);
				else if (chr->alignment() < 0 && victim->alignment() < 0)
					chr->set_killer_mode(true);

				return true;
				break;
		}
	}

	pvp kPVP(chr->player_id(), victim->player_id());
	pvp * pkPVP = find(kPVP.crc_);

	if (!pkPVP || !pkPVP->is_fight())
	{
		if (beKillerMode)
			chr->set_killer_mode(true);

		return (beKillerMode);
	}

	pkPVP->set_last_fight_time();
	return true;
}

pvp * pvp_manager::find(uint32_t crc)
{
	auto it = map_pk_pvp_.find(crc);

	if (it == map_pk_pvp_.end())
		return nullptr;

	return it->second.get();
}

void pvp_manager::remove(pvp * pvp_value)
{
	auto it = map_pk_pvp_.find(pvp_value->crc_);

	if (it == map_pk_pvp_.end())
		return;

	map_pk_pvp_set_by_id_[pvp_value->players_[0].pid].erase(pvp_value);
	map_pk_pvp_set_by_id_[pvp_value->players_[1].pid].erase(pvp_value);
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

		if (get_dword_time() - pvp->last_fight_time() > 600000)
		{
			pvp->packet(true);
			remove(pvp);
		}
	}
}
}
