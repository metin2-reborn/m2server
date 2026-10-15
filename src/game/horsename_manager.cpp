
#include "horsename_manager.h"

#include "affect.h"
#include "char.h"
#include "char_manager.h"
#include "desc_client.h"
#include "typedef.h"
#include "utils.h"

#include <common/tables.h>
#include <metin2/log/log.h>

#include <cstdint>
#include <cstring>
#include <map>
#include <string>

namespace metin2::game {

CHorseNameManager::CHorseNameManager()
{
	m_mapHorseNames.clear();
}

const char* CHorseNameManager::GetHorseName(uint32_t dwPlayerID)
{
	std::map<uint32_t, std::string>::iterator iter;

	iter = m_mapHorseNames.find(dwPlayerID);

	if ( iter != m_mapHorseNames.end() )
	{
		return iter->second.c_str();
	}
	else
	{
		return nullptr;
	}
}

void CHorseNameManager::UpdateHorseName(uint32_t dwPlayerID, const char* szHorseName, bool broadcast)
{
	if ( szHorseName == nullptr )
	{
		metin2::log::error("HORSE_NAME: NULL NAME ({:d})", dwPlayerID);
		szHorseName = "";
	}

	metin2::log::info( "HORSENAME: update {:d} {}", dwPlayerID, szHorseName);

	m_mapHorseNames[dwPlayerID] = szHorseName;

	if ( broadcast == true )
	{
		BroadcastHorseName(dwPlayerID, szHorseName);
	}
}

void CHorseNameManager::BroadcastHorseName(uint32_t dwPlayerID, const char* szHorseName)
{
	TPacketUpdateHorseName packet;
	packet.dwPlayerID = dwPlayerID;
	strlcpy(packet.szHorseName, szHorseName, sizeof(packet.szHorseName));

	db_clientdesc->DBPacket(HEADER_GD_UPDATE_HORSE_NAME, 0, &packet, sizeof(TPacketUpdateHorseName));
}

void CHorseNameManager::Validate(CHARACTER * pChar)
{
	CAffect *pkAff = pChar->FindAffect(AFFECT_HORSE_NAME);

	if ( pkAff != nullptr )
	{
		if ( pChar->GetQuestFlag("horse_name.valid_till") < get_global_time() )
		{
			pChar->HorseSummon(false, true);
			pChar->RemoveAffect(pkAff);
			UpdateHorseName(pChar->GetPlayerID(), "", true);
			pChar->HorseSummon(true, true);
		}
		else
		{
			++(pkAff->lDuration);
		}
	}
}
}
