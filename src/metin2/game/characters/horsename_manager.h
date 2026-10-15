#pragma once

#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>

#include <cstdint>
#include <map>
#include <string>

namespace metin2::game::inline characters {

class horse_name_manager : public singleton<horse_name_manager>
{
	private :
		std::map<uint32_t, std::string> horse_names_;

		void broadcast_horse_name(uint32_t dwPlayerID, const char* szHorseName);

	public :
		horse_name_manager();

		const char* horse_name(uint32_t dwPlayerID);

		void update_horse_name(uint32_t dwPlayerID, const char* szHorseName, bool broadcast=false);

		void validate(character * pChar);
};
}
