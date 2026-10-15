#pragma once
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/common/singleton.h>

#include <cstdint>
#include <ctime>
#include <map>

/**
 * @version 05/06/08	Bang2ni - Added duration to guild privilege functions
 * 			          Changed RequestGiveGuildPriv, GiveGuildPriv prototypes
 * 			          Changed m_aPrivGuild type
 * 			          Added struct SPrivGuildData, member function GetPrivByGuildEx
 */

namespace metin2::game::inline empire {
class priv_manager : public singleton<priv_manager>
{
	public:
		priv_manager();

		void request_give_guild_priv(uint32_t guild_id, uint8_t type, int value, time_t duration_sec);
		void request_give_empire_priv(uint8_t empire, uint8_t type, int value, time_t duration_sec);
		void request_give_character_priv(uint32_t pid, uint8_t type, int value);

		void give_guild_priv(uint32_t guild_id, uint8_t type, int value, uint8_t log, time_t end_time_sec);
		void give_empire_priv(uint8_t empire, uint8_t type, int value, uint8_t log, time_t end_time_sec);
		void give_character_priv(uint32_t pid, uint8_t type, int value, uint8_t log);

		int priv(character * ch, uint8_t type);
		int priv_by_empire(uint8_t empire, uint8_t type);
		int priv_by_guild(uint32_t guild_id, uint8_t type);
		int priv_by_character(uint32_t pid, uint8_t type);

	public:
		struct priv_empire_data
		{
			int value;
			time_t end_time_sec;
		};

		priv_empire_data* priv_by_empire_ex(uint8_t empire, uint8_t type);

		/// Guild bonus data
		struct priv_guild_data
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
		const priv_guild_data*	priv_by_guild_ex( uint32_t guild_id, uint8_t type ) const;

	private:
		priv_empire_data aak_priv_empire_data_[MAX_PRIV_NUM][EMPIRE_MAX_NUM];
		std::map<uint32_t, priv_guild_data> priv_guild_[MAX_PRIV_NUM];
		std::map<uint32_t, int> priv_char_[MAX_PRIV_NUM];
};
}
