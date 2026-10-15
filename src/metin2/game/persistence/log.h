#pragma once

#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>
#include <metin2/sql/AsyncSQL.h>

#include <cstdint>
#include <string>

namespace metin2::game::inline persistence {

enum goldbar_how
{
	PERSONAL_SHOP_BUY	= 1 ,
	PERSONAL_SHOP_SELL	= 2 ,
	SHOP_BUY			= 3 ,
	SHOP_SELL			= 4 ,
	EXCHANGE_TAKE		= 5 ,
	EXCHANGE_GIVE		= 6 ,
	QUEST				= 7 ,
};

class log_manager : public singleton<log_manager>
{
	public:
		log_manager();
		~log_manager() override;

		bool		is_connected();

		bool		connect(const char * host, const int port, const char * user, const char * pwd, const char * db);

		void		item_log(uint32_t dwPID, uint32_t x, uint32_t y, uint32_t dwItemID, const char * c_pszText, const char * c_pszHint, const char * c_pszIP, uint32_t dwVnum);
		void		item_log(character * ch, item * item, const char * c_pszText, const char * c_pszHint);
		void		item_log(character * ch, int itemID, int itemVnum, const char * c_pszText, const char * c_pszHint);

		void		char_log(uint32_t dwPID, uint32_t x, uint32_t y, uint32_t dw, const char * c_pszText, const char * c_pszHint, const char * c_pszIP);
		void		char_log(character * ch, uint32_t dw, const char * c_pszText, const char * c_pszHint);

		void		money_log(uint8_t type, uint32_t vnum, int gold);
		void		hack_log(const char * c_pszHackName, const char * c_pszLogin, const char * c_pszName, const char * c_pszIP);
		void		hack_log(const char * c_pszHackName, character * ch);
		void		gold_bar_log(uint32_t dwPID, uint32_t dwItemID, goldbar_how eHow, const char * c_pszHint);
		void		cube_log(uint32_t dwPID, uint32_t x, uint32_t y, uint32_t item_vnum, uint32_t item_uid, int item_count, bool success);
		void		gm_command_log(uint32_t dwPID, const char * szName, const char * szIP, uint8_t byChannel, const char * szCommand);
		void		speed_hack_log(uint32_t pid, uint32_t x, uint32_t y, int hack_count);
		void		change_name_log(uint32_t pid, const char * old_name, const char * new_name, const char * ip);
		void		refine_log(uint32_t pid, const char * item_name, uint32_t item_id, int item_refine_level, int is_success, const char * how);
		void		shout_log(uint8_t bChannel, uint8_t bEmpire, const char * pszText);
		void		level_log(character * pChar, unsigned int level, unsigned int playhour);
		void		boot_log(const char * c_pszHostName, uint8_t bChannel);
		void		fish_log(uint32_t dwPID, int prob_idx, int fish_id, int fish_level, uint32_t dwMiliseconds, uint32_t dwVnum = false, uint32_t dwValue = 0);
		void		quest_reward_log(const char * c_pszQuestName, uint32_t dwPID, uint32_t dwLevel, int iValue1, int iValue2);
		void		detail_login_log(bool isLogin, character * ch);
		void		dragon_slay_log(uint32_t dwGuildID, uint32_t dwDragonVnum, uint32_t dwStartTime, uint32_t dwEndTime);

	private:
		void		query(const std::string & query);
		void		query(const char * c_pszFormat, ...) __attribute__((format(printf, 2, 3)));

		async_sql	sql_;
		bool		is_connect_;
};
}
