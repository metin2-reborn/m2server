#pragma once

#include "constants.h"
#include "event.h"
#include "guild.h"
#include "typedef.h"

#include <common/singleton.h>

#include <cstdint>
#include <map>
#include <set>

enum EWarMapTypes
{
	WAR_MAP_TYPE_NORMAL,
	WAR_MAP_TYPE_FLAG,
};

struct TWarMapInfo {
	uint8_t		bType;
	int32_t		lMapIndex;
	PIXEL_POSITION	posStart[3];
};

namespace warmap
{
	enum
	{
		WAR_FLAG_VNUM_START = 20035,
		WAR_FLAG_VNUM_END = 20037,

		WAR_FLAG_VNUM0 = 20035,
		WAR_FLAG_VNUM1 = 20036,

		WAR_FLAG_BASE_VNUM = 20038
	};

	inline bool IsWarFlag(uint32_t dwVnum)
	{
		if (dwVnum >= WAR_FLAG_VNUM_START && dwVnum <= WAR_FLAG_VNUM_END)
			return true;

		return false;
	}

	inline bool IsWarFlagBase(uint32_t dwVnum)
	{
		return dwVnum == WAR_FLAG_BASE_VNUM ? true : false;
	}
};

class CWarMap
{
	public:
		friend class CGuild;

		CWarMap(int32_t lMapIndex, const TGuildWarInfo & r_info, TWarMapInfo * pkWarMapInfo, uint32_t dwGuildID1, uint32_t dwGuildID2);
		~CWarMap();

		bool	GetTeamIndex(uint32_t dwGuild, uint8_t & bIdx);

		void	IncMember(CHARACTER * ch);
		void	DecMember(CHARACTER * ch);

		CGuild * GetGuild(uint8_t bIdx);

		uint8_t	GetType();
		int32_t	GetMapIndex();
		uint32_t	GetGuildOpponent(CHARACTER * ch);

		uint32_t	GetWinnerGuild();
		void	UsePotion(CHARACTER * ch, CItem * item);

		void	Draw();	// Force a draw
		void	Timeout();
		void	CheckWarEnd();
		bool	SetEnded();
		void	ExitAll();

		void	SetBeginEvent(LPEVENT pkEv);
		void	SetTimeoutEvent(LPEVENT pkEv);
		void	SetEndEvent(LPEVENT pkEv);
		void	SetResetFlagEvent(LPEVENT pkEv);

		void	UpdateScore(uint32_t g1, int score1, uint32_t g2, int score2);
		bool	CheckScore();

		int	GetRewardGold(uint8_t bWinnerIdx);

		void	Packet(const void * pv, int size);
		void	Notice(const char * psz);
		void	SendWarPacket(DESC * d);
		void	SendScorePacket(uint8_t bIdx, DESC * d = nullptr);

		void	OnKill(CHARACTER * killer, CHARACTER * ch);

		void	AddFlag(uint8_t bIdx, uint32_t x=0, uint32_t y=0);
		void	AddFlagBase(uint8_t bIdx, uint32_t x=0, uint32_t y=0);
		void	RemoveFlag(uint8_t bIdx);
		bool	IsFlagOnBase(uint8_t bIdx);
		void	ResetFlag();

	private:
		void	UpdateUserCount();

	private:
		TWarMapInfo	m_kMapInfo;
		bool		m_bEnded;

		LPEVENT m_pkBeginEvent;
		LPEVENT m_pkTimeoutEvent;
		LPEVENT m_pkEndEvent;
		LPEVENT	m_pkResetFlagEvent;

		struct TeamData {
			uint32_t	dwID;
			CGuild * pkGuild;
			int		iMemberCount;
			int		iUsePotionPrice;
			int		iScore;
			CHARACTER * pkChrFlag;
			CHARACTER * pkChrFlagBase;

			std::set<uint32_t> set_pidJoiner;

			void Initialize();

			int GetAccumulatedJoinerCount(); // Cumulative participant count
			int GetCurJointerCount(); // Current participant count

			void AppendMember(CHARACTER * ch);
			void RemoveMember(CHARACTER * ch);
		};

		TeamData	m_TeamData[2];
		int		m_iObserverCount;
		uint32_t		m_dwStartTime;
		uint8_t		m_bTimeout;

		TGuildWarInfo	m_WarInfo;

		CHARACTER_SET m_set_pkChr;
};

class CWarMapManager : public singleton<CWarMapManager>
{
	public:
		CWarMapManager();
		~CWarMapManager() override;

		bool		LoadWarMapInfo(const char * c_pszFileName);
		bool		IsWarMap(int32_t lMapIndex);
		TWarMapInfo *	GetWarMapInfo(int32_t lMapIndex);
		bool		GetStartPosition(int32_t lMapIndex, uint8_t bIdx, PIXEL_POSITION & pos);

		template <typename Func> Func for_each(Func f);
		int32_t		CreateWarMap(const TGuildWarInfo & r_WarInfo, uint32_t dwGuildID1, uint32_t dwGuildID2);
		void		DestroyWarMap(CWarMap* pMap);
		CWarMap *	Find(int32_t lMapIndex);

		void		OnShutdown();

	private:
		std::map<int32_t, TWarMapInfo *> m_map_kWarMapInfo;
		std::map<int32_t, CWarMap *> m_mapWarMap;
};

template <typename Func> Func CWarMapManager::for_each(Func f)
{
	for (auto it = m_mapWarMap.begin(); it != m_mapWarMap.end(); ++it)
		f(it->second);

	return f;
}
