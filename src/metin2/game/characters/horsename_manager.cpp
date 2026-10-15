
#include <metin2/game/characters/horsename_manager.h>

#include <metin2/game/characters/affect.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>

#include <metin2/proto/tables.h>
#include <metin2/log/log.h>

#include <cstdint>
#include <map>
#include <string>

namespace metin2::game::inline characters {

horse_name_manager::horse_name_manager()
{
	horse_names_.clear();
}

auto horse_name_manager::horse_name(uint32_t player_id) -> const char*
{
	std::map<uint32_t, std::string>::iterator iter;

	iter = horse_names_.find(player_id);

	if ( iter != horse_names_.end() )
	{
		return iter->second.c_str();
	}
	else
	{
		return nullptr;
	}
}

void horse_name_manager::update_horse_name(uint32_t player_id, const char* horse_name, bool broadcast)
{
	if ( horse_name == nullptr )
	{
		metin2::log::error("HORSE_NAME: NULL NAME ({:d})", player_id);
		horse_name = "";
	}

	metin2::log::info( "HORSENAME: update {:d} {}", player_id, horse_name);

	horse_names_[player_id] = horse_name;

	if ( broadcast == true )
	{
		broadcast_horse_name(player_id, horse_name);
	}
}

void horse_name_manager::broadcast_horse_name(uint32_t player_id_value, const char* horse_name_value)
{
	packet_update_horse_name packet;
	packet.player_id = player_id_value;
	strlcpy(packet.horse_name, horse_name_value, sizeof(packet.horse_name));

	db_clientdesc->db_packet(HEADER_GD_UPDATE_HORSE_NAME, 0, &packet, sizeof(packet_update_horse_name));
}

void horse_name_manager::validate(character * ch)
{
	affect *pkAff = ch->find_affect(AFFECT_HORSE_NAME);

	if ( pkAff != nullptr )
	{
		if ( ch->quest_flag("horse_name.valid_till") < get_global_time() )
		{
			ch->horse_summon(false, true);
			ch->remove_affect(pkAff);
			update_horse_name(ch->player_id(), "", true);
			ch->horse_summon(true, true);
		}
		else
		{
			++(pkAff->duration);
		}
	}
}
}
