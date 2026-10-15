
#include <metin2/game/characters/horsename_manager.h>

#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/char.h>
#include <metin2/game/characters/char_manager.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>

#include <metin2/proto/tables.h>
#include <metin2/log/log.h>

#include <cstdint>
#include <cstring>
#include <map>
#include <string>

namespace metin2::game::inline characters {

horse_name_manager::horse_name_manager()
{
	horse_names_.clear();
}

const char* horse_name_manager::horse_name(uint32_t dwPlayerID)
{
	std::map<uint32_t, std::string>::iterator iter;

	iter = horse_names_.find(dwPlayerID);

	if ( iter != horse_names_.end() )
	{
		return iter->second.c_str();
	}
	else
	{
		return nullptr;
	}
}

void horse_name_manager::update_horse_name(uint32_t dwPlayerID, const char* szHorseName, bool broadcast)
{
	if ( szHorseName == nullptr )
	{
		metin2::log::error("HORSE_NAME: NULL NAME ({:d})", dwPlayerID);
		szHorseName = "";
	}

	metin2::log::info( "HORSENAME: update {:d} {}", dwPlayerID, szHorseName);

	horse_names_[dwPlayerID] = szHorseName;

	if ( broadcast == true )
	{
		broadcast_horse_name(dwPlayerID, szHorseName);
	}
}

void horse_name_manager::broadcast_horse_name(uint32_t dwPlayerID, const char* szHorseName)
{
	packet_update_horse_name packet;
	packet.player_id = dwPlayerID;
	strlcpy(packet.horse_name, szHorseName, sizeof(packet.horse_name));

	db_clientdesc->db_packet(HEADER_GD_UPDATE_HORSE_NAME, 0, &packet, sizeof(packet_update_horse_name));
}

void horse_name_manager::validate(character * pChar)
{
	affect *pkAff = pChar->find_affect(AFFECT_HORSE_NAME);

	if ( pkAff != nullptr )
	{
		if ( pChar->quest_flag("horse_name.valid_till") < get_global_time() )
		{
			pChar->horse_summon(false, true);
			pChar->remove_affect(pkAff);
			update_horse_name(pChar->player_id(), "", true);
			pChar->horse_summon(true, true);
		}
		else
		{
			++(pkAff->duration);
		}
	}
}
}
