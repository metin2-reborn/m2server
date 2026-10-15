#pragma once

#include "typedef.h"

#include <common/singleton.h>

#include <cstdint>
#include <map>
#include <string>

namespace metin2::game {

class CHorseNameManager : public singleton<CHorseNameManager>
{
	private :
		std::map<uint32_t, std::string> m_mapHorseNames;

		void BroadcastHorseName(uint32_t dwPlayerID, const char* szHorseName);

	public :
		CHorseNameManager();

		const char* GetHorseName(uint32_t dwPlayerID);

		void UpdateHorseName(uint32_t dwPlayerID, const char* szHorseName, bool broadcast=false);

		void Validate(CHARACTER * pChar);
};
}
