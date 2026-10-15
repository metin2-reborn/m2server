#pragma once
#include "typedef.h"

#include <common/length.h>
#include <common/singleton.h>

#include <cstdint>
#include <ctime>
#include <map>

/**
 * @version 05/06/08	Bang2ni - Added duration to guild privilege functions
 * 			          Changed RequestGiveGuildPriv, GiveGuildPriv prototypes
 * 			          Changed m_aPrivGuild type
 * 			          Added struct SPrivGuildData, member function GetPrivByGuildEx
 */

namespace metin2::game {
class CPrivManager : public singleton<CPrivManager>
{
	public:
		CPrivManager();

		void RequestGiveGuildPriv(uint32_t guild_id, uint8_t type, int value, time_t dur_time_sec);
		void RequestGiveEmpirePriv(uint8_t empire, uint8_t type, int value, time_t dur_time_sec);
		void RequestGiveCharacterPriv(uint32_t pid, uint8_t type, int value);

		void GiveGuildPriv(uint32_t guild_id, uint8_t type, int value, uint8_t bLog, time_t end_time_sec);
		void GiveEmpirePriv(uint8_t empire, uint8_t type, int value, uint8_t bLog, time_t end_time_sec);
		void GiveCharacterPriv(uint32_t pid, uint8_t type, int value, uint8_t bLog);

		int GetPriv(CHARACTER * ch, uint8_t type);
		int GetPrivByEmpire(uint8_t bEmpire, uint8_t type);
		int GetPrivByGuild(uint32_t guild_id, uint8_t type);
		int GetPrivByCharacter(uint32_t pid, uint8_t type);

	public:
		struct SPrivEmpireData
		{
			int m_value;
			time_t m_end_time_sec;
		};

		SPrivEmpireData* GetPrivByEmpireEx(uint8_t bEmpire, uint8_t type);

		/// Guild bonus data
		struct SPrivGuildData
		{
			int		value;		///< Bonus value
			time_t	end_time_sec;	///< Duration
		};

		/// Get guild bonus data.
		/**
		 * @param [in]	dwGuildID ID of the guild to look up
		 * @param [in]	byType bonus type
		 * @return	pointer to the target guild's bonus data, or NULL if there is no bonus data for the given bonus type and guild ID
		 */
		const SPrivGuildData*	GetPrivByGuildEx( uint32_t dwGuildID, uint8_t byType ) const;

	private:
		SPrivEmpireData m_aakPrivEmpireData[MAX_PRIV_NUM][EMPIRE_MAX_NUM];
		std::map<uint32_t, SPrivGuildData> m_aPrivGuild[MAX_PRIV_NUM];
		std::map<uint32_t, int> m_aPrivChar[MAX_PRIV_NUM];
};
}
