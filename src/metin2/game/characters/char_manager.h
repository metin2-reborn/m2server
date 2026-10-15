#pragma once

#include <metin2/game/app/typedef.h>
#include <metin2/game/world/vid.h>

#include <metin2/proto/length.h>
#include <metin2/common/singleton.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <unordered_map>

namespace metin2::game::inline world {
    class CDungeon;
}

namespace metin2::game::inline characters {

class CHARACTER;
class CharacterVectorInteractor;

class CHARACTER_MANAGER : public singleton<CHARACTER_MANAGER>
{
	public:
		typedef std::unordered_map<std::string, CHARACTER *> NAME_MAP;

		CHARACTER_MANAGER();
		~CHARACTER_MANAGER() override;

		void                    Destroy();

		void			GracefulShutdown();	// Used for a normal shutdown. Saves all PCs and destroys them.

		uint32_t			AllocVID();

		CHARACTER *             CreateCharacter(const char * name, uint32_t dwPID = 0);
		void DestroyCharacter(CHARACTER * ch);

		void			Update(int iPulse);

		CHARACTER *		SpawnMob(uint32_t dwVnum, int32_t lMapIndex, int32_t x, int32_t y, int32_t z, bool bSpawnMotion = false, int iRot = -1, bool bShow = true);
		CHARACTER *		SpawnMobRange(uint32_t dwVnum, int32_t lMapIndex, int sx, int sy, int ex, int ey, bool bIsException=false, bool bSpawnMotion = false , bool bAggressive = false);
		CHARACTER *		SpawnGroup(uint32_t dwVnum, int32_t lMapIndex, int sx, int sy, int ex, int ey, REGEN * pkRegen = nullptr, bool bAggressive_ = false, CDungeon * pDungeon = nullptr);
		bool			SpawnGroupGroup(uint32_t dwVnum, int32_t lMapIndex, int sx, int sy, int ex, int ey, REGEN * pkRegen = nullptr, bool bAggressive_ = false, CDungeon * pDungeon = nullptr);
		bool			SpawnMoveGroup(uint32_t dwVnum, int32_t lMapIndex, int sx, int sy, int ex, int ey, int tx, int ty, REGEN * pkRegen = nullptr, bool bAggressive_ = false);
		CHARACTER *		SpawnMobRandomPosition(uint32_t dwVnum, int32_t lMapIndex);

		void			SelectStone(CHARACTER * pkChrStone);

		NAME_MAP &		GetPCMap() { return m_map_pkPCChr; }

		CHARACTER *		Find(uint32_t dwVID);
		CHARACTER *		Find(const VID & vid);
		CHARACTER *		FindPC(const char * name);
		CHARACTER *		FindByPID(uint32_t dwPID);

		bool			AddToStateList(CHARACTER * ch);
		void			RemoveFromStateList(CHARACTER * ch);

		// DelayedSave: if a routine does many things that require saving,
		// there are too many queries, so just mark it "to be saved" and save it
		// a moment later (e.g. 1 frame).
		void                    DelayedSave(CHARACTER * ch);
		bool                    FlushDelayedSave(CHARACTER * ch); // If in the delayed list, remove it and save. Used when handling disconnects.
		void			ProcessDelayedSave();

		template<class Func>	Func for_each_pc(Func f);

		void			RegisterForMonsterLog(CHARACTER * ch);
		void			UnregisterForMonsterLog(CHARACTER * ch);
		void			PacketMonsterLog(CHARACTER * ch, const void* buf, int size);

		void			KillLog(uint32_t dwVnum);

		void			RegisterRaceNum(uint32_t dwVnum);
		void			RegisterRaceNumMap(CHARACTER * ch);
		void			UnregisterRaceNumMap(CHARACTER * ch);
		bool			GetCharactersByRaceNum(uint32_t dwRaceNum, CharacterVectorInteractor & i);

		CHARACTER *		FindSpecifyPC(unsigned int uiJobFlag, int32_t lMapIndex, CHARACTER * except=nullptr, int iMinLevel = 1, int iMaxLevel = PLAYER_MAX_LEVEL_CONST);

		void			SetMobItemRate(int value)	{ m_iMobItemRate = value; }
		void			SetMobDamageRate(int value)	{ m_iMobDamageRate = value; }
		void			SetMobGoldAmountRate(int value)	{ m_iMobGoldAmountRate = value; }
		void			SetMobGoldDropRate(int value)	{ m_iMobGoldDropRate = value; }
		void			SetMobExpRate(int value)	{ m_iMobExpRate = value; }

		void			SetMobItemRatePremium(int value)	{ m_iMobItemRatePremium = value; }
		void			SetMobGoldAmountRatePremium(int value)	{ m_iMobGoldAmountRatePremium = value; }
		void			SetMobGoldDropRatePremium(int value)	{ m_iMobGoldDropRatePremium = value; }
		void			SetMobExpRatePremium(int value)		{ m_iMobExpRatePremium = value; }

		int			GetMobItemRate(CHARACTER * ch);
		int			GetMobDamageRate(CHARACTER * ch);
		int			GetMobGoldAmountRate(CHARACTER * ch);
		int			GetMobGoldDropRate(CHARACTER * ch);
		int			GetMobExpRate(CHARACTER * ch);

		void		SendScriptToMap(int32_t lMapIndex, const std::string & s);

		bool			BeginPendingDestroy();
		void			FlushPendingDestroy();

	private:
		int					m_iMobItemRate;
		int					m_iMobDamageRate;
		int					m_iMobGoldAmountRate;
		int					m_iMobGoldDropRate;
		int					m_iMobExpRate;

		int					m_iMobItemRatePremium;
		int					m_iMobGoldAmountRatePremium;
		int					m_iMobGoldDropRatePremium;
		int					m_iMobExpRatePremium;

		int					m_iVIDCount;

		std::unordered_map<uint32_t, CHARACTER *> m_map_pkChrByVID;
		std::unordered_map<uint32_t, CHARACTER *> m_map_pkChrByPID;
		NAME_MAP			m_map_pkPCChr;

		CHARACTER_SET		m_set_pkChrState;	// Those with a running FSM
		CHARACTER_SET		m_set_pkChrForDelayedSave;
		CHARACTER_SET		m_set_pkChrMonsterLog;

		CHARACTER *			m_pkChrSelectedStone;

		std::map<uint32_t, uint32_t> m_map_dwMobKillCount;

		std::set<uint32_t>		m_set_dwRegisteredRaceNum;
		std::map<uint32_t, CHARACTER_SET> m_map_pkChrByRaceNum;

		bool				m_bUsePendingDestroy;
		CHARACTER_SET		m_set_pkChrPendingDestroy;

};

	template<class Func>	
Func CHARACTER_MANAGER::for_each_pc(Func f)
{
	std::unordered_map<uint32_t, CHARACTER *>::iterator it;

	for (it = m_map_pkChrByPID.begin(); it != m_map_pkChrByPID.end(); ++it)
		f(it->second);

	return f;
}

class CharacterVectorInteractor : public CHARACTER_VECTOR
{
	public:
		CharacterVectorInteractor() : m_bMyBegin(false) { }

		CharacterVectorInteractor(const CHARACTER_SET & r);
		virtual ~CharacterVectorInteractor();

	private:
		bool m_bMyBegin;
};
}
