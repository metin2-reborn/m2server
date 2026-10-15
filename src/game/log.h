#pragma once

#include "typedef.h"

#include <common/singleton.h>
#include <libsql/AsyncSQL.h>

#include <cstdint>

enum GOLDBAR_HOW
{
	PERSONAL_SHOP_BUY	= 1 ,
	PERSONAL_SHOP_SELL	= 2 ,
	SHOP_BUY			= 3 ,
	SHOP_SELL			= 4 ,
	EXCHANGE_TAKE		= 5 ,
	EXCHANGE_GIVE		= 6 ,
	QUEST				= 7 ,
};

class LogManager : public singleton<LogManager>
{
	public:
		LogManager();
		~LogManager() override;

		bool		IsConnected();

		bool		Connect(const char * host, const int port, const char * user, const char * pwd, const char * db);

		void		ItemLog(uint32_t dwPID, uint32_t x, uint32_t y, uint32_t dwItemID, const char * c_pszText, const char * c_pszHint, const char * c_pszIP, uint32_t dwVnum);
		void		ItemLog(CHARACTER * ch, CItem * item, const char * c_pszText, const char * c_pszHint);
		void		ItemLog(CHARACTER * ch, int itemID, int itemVnum, const char * c_pszText, const char * c_pszHint);

		void		CharLog(uint32_t dwPID, uint32_t x, uint32_t y, uint32_t dw, const char * c_pszText, const char * c_pszHint, const char * c_pszIP);
		void		CharLog(CHARACTER * ch, uint32_t dw, const char * c_pszText, const char * c_pszHint);

		void		MoneyLog(uint8_t type, uint32_t vnum, int gold);
		void		HackLog(const char * c_pszHackName, const char * c_pszLogin, const char * c_pszName, const char * c_pszIP);
		void		HackLog(const char * c_pszHackName, CHARACTER * ch);
		void		GoldBarLog(uint32_t dwPID, uint32_t dwItemID, GOLDBAR_HOW eHow, const char * c_pszHint);
		void		CubeLog(uint32_t dwPID, uint32_t x, uint32_t y, uint32_t item_vnum, uint32_t item_uid, int item_count, bool success);
		void		GMCommandLog(uint32_t dwPID, const char * szName, const char * szIP, uint8_t byChannel, const char * szCommand);
		void		SpeedHackLog(uint32_t pid, uint32_t x, uint32_t y, int hack_count);
		void		ChangeNameLog(uint32_t pid, const char * old_name, const char * new_name, const char * ip);
		void		RefineLog(uint32_t pid, const char * item_name, uint32_t item_id, int item_refine_level, int is_success, const char * how);
		void		ShoutLog(uint8_t bChannel, uint8_t bEmpire, const char * pszText);
		void		LevelLog(CHARACTER * pChar, unsigned int level, unsigned int playhour);
		void		BootLog(const char * c_pszHostName, uint8_t bChannel);
		void		FishLog(uint32_t dwPID, int prob_idx, int fish_id, int fish_level, uint32_t dwMiliseconds, uint32_t dwVnum = false, uint32_t dwValue = 0);
		void		QuestRewardLog(const char * c_pszQuestName, uint32_t dwPID, uint32_t dwLevel, int iValue1, int iValue2);
		void		DetailLoginLog(bool isLogin, CHARACTER * ch);
		void		DragonSlayLog(uint32_t dwGuildID, uint32_t dwDragonVnum, uint32_t dwStartTime, uint32_t dwEndTime);

	private:
		void		Query(const char * c_pszFormat, ...);

		CAsyncSQL	m_sql;
		bool		m_bIsConnect;
};
