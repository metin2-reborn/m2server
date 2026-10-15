#pragma once

#include "char.h"
#include "event.h"

#include <common/singleton.h>
#include <libthecore/log.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>

enum // unit : minute
{
	PARTY_ENOUGH_MINUTE_FOR_EXP_BONUS = 60, // Bonus experience starts 60 minutes after the party is formed
	PARTY_HEAL_COOLTIME_LONG = 60,
	PARTY_HEAL_COOLTIME_SHORT = 30,
	PARTY_MAX_MEMBER = 8,
	PARTY_DEFAULT_RANGE = 5000,
};

enum EPartyRole
{   
	PARTY_ROLE_NORMAL,  
	PARTY_ROLE_LEADER,
	PARTY_ROLE_ATTACKER,
	PARTY_ROLE_TANKER,
	PARTY_ROLE_BUFFER,  
	PARTY_ROLE_SKILL_MASTER,
	PARTY_ROLE_HASTE,   
	PARTY_ROLE_DEFENDER,
	PARTY_ROLE_MAX_NUM, 
};  

enum EPartyExpDistributionModes
{
	PARTY_EXP_DISTRIBUTION_NON_PARITY,
	PARTY_EXP_DISTRIBUTION_PARITY,
	PARTY_EXP_DISTRIBUTION_MAX_NUM
};

class CParty;
class CDungeon;

class CPartyManager : public singleton<CPartyManager>
{
	public:
		typedef std::map<uint32_t, CParty *> TPartyMap;
		typedef std::set<CParty *> TPCPartySet;

	public:
		CPartyManager();
		~CPartyManager() override;

		void		Initialize();

		void		EnablePCParty() { m_bEnablePCParty = true; sys_log(0,"PARTY Enable"); }
		void		DisablePCParty() { m_bEnablePCParty = false; sys_log(0,"PARTY Disable"); }
		bool		IsEnablePCParty() { return m_bEnablePCParty; }

		CParty *		CreateParty(CHARACTER * pkLeader);
		void		DeleteParty(CParty * pParty);
		void		DeleteAllParty();
		bool		SetParty(CHARACTER * pkChr);

		void		SetPartyMember(uint32_t dwPID, CParty * pParty);

		void		P2PLogin(uint32_t pid, const char* name);
		void		P2PLogout(uint32_t pid);

		CParty *		P2PCreateParty(uint32_t pid);
		void		P2PDeleteParty(uint32_t pid);
		void		P2PJoinParty(uint32_t leader, uint32_t pid, uint8_t role = 0);
		void		P2PQuitParty(uint32_t pid);

	private:
		TPartyMap	m_map_pkParty;		// Container for looking up which party a PID is in

		TPCPartySet	m_set_pkPCParty;	// The full set of everyone's parties

		bool		m_bEnablePCParty;	// If the DB is not up, people's party state cannot be changed
};

enum EPartyMessages
{
	PM_ATTACK,		// Attack him
	PM_RETURN,		// Return back to position
	PM_ATTACKED_BY,	// I was attacked by someone
	PM_AGGRO_INCREASE,	// My aggro is increased
};

class CParty
{
	public:
		struct TMember {
			CHARACTER *	pCharacter;
			bool	bNear;
			uint8_t	bRole;
			uint8_t	bLevel;
			std::string strName;
		};

		typedef std::map<uint32_t, TMember> TMemberMap;

		typedef std::map<std::string, int> TFlagMap;

	public:
		CParty();
		virtual ~CParty();

		void		P2PJoin(uint32_t dwPID);
		void		P2PQuit(uint32_t dwPID);
		virtual void	Join(uint32_t dwPID);
		void		Quit(uint32_t dwPID);
		void		Link(CHARACTER * pkChr);
		void		Unlink(CHARACTER * pkChr);

		void		ChatPacketToAllMember(uint8_t type, const char* format, ...);	

		void		UpdateOnlineState(uint32_t dwPID, const char* name);
		void		UpdateOfflineState(uint32_t dwPID);

		uint32_t		GetLeaderPID();
		CHARACTER *	GetLeaderCharacter();
		CHARACTER *	GetLeader() { return m_pkChrLeader; }

		uint32_t		GetMemberCount();
		uint32_t		GetNearMemberCount()	{ return m_iCountNearPartyMember; }

		bool		IsMember(uint32_t pid) { return m_memberMap.find(pid) != m_memberMap.end(); }

		bool		IsPositionNearLeader(CHARACTER * ch);

		void		SendMessage(CHARACTER * ch, uint8_t bMsg, uint32_t dwArg1, uint32_t dwArg2);

		void		SendPartyJoinOneToAll(uint32_t dwPID);
		void		SendPartyJoinAllToOne(CHARACTER * ch);
		void		SendPartyRemoveOneToAll(uint32_t dwPID);

		void		SendPartyInfoOneToAll(uint32_t pid);
		void		SendPartyInfoOneToAll(CHARACTER * ch);
		void		SendPartyInfoAllToOne(CHARACTER * ch);

		void		SendPartyLinkOneToAll(CHARACTER * ch);
		void		SendPartyLinkAllToOne(CHARACTER * ch);
		void		SendPartyUnlinkOneToAll(CHARACTER * ch);

		int		GetPartyBonusExpPercent()	{ return m_iExpBonus; }

		int	ComputePartyBonusExpPercent();

		template <class Func> void ForEachMember(Func & f);
		template <class Func> void ForEachMemberPtr(Func & f);
		template <class Func> void ForEachOnlineMember(Func & f);
		template <class Func> void ForEachNearMember(Func & f);
		template <class Func> void ForEachOnMapMember (Func & f, int32_t lMapIndex);
		template <class Func> bool ForEachOnMapMemberBool (Func & f, int32_t lMapIndex);

		void		Update();

		int		GetExpBonusPercent();

		bool		SetRole(uint32_t pid, uint8_t bRole, bool on);
		uint8_t		GetRole(uint32_t pid);

		uint8_t		GetMemberMaxLevel();

		void		ComputeRolePoint(CHARACTER * ch, uint8_t bRole, bool bAdd);

		void		HealParty();
		void		SummonToLeader(uint32_t pid);

		void		SetPCParty(bool b) { m_bPCParty = b; }

		CHARACTER *	GetNextOwnership(CHARACTER * ch, int32_t x, int32_t y);

		void		SetFlag(const std::string& name, int value);
		int		GetFlag(const std::string& name);

		void		SetDungeon(CDungeon * pDungeon);
		CDungeon *	GetDungeon();

		uint8_t		CountMemberByVnum(uint32_t dwVnum);

		void		SetParameter(int iMode);
		int		GetExpDistributionMode();

		CHARACTER *	GetExpCentralizeCharacter();

		void		RequestSetMemberLevel(uint32_t pid, uint8_t level);
		void		P2PSetMemberLevel(uint32_t pid, uint8_t level);

	protected:
		void		IncreaseOwnership();

		virtual void	Initialize();
		void		Destroy();

		void		RemoveBonus();
		void		RemoveBonusForOne(uint32_t pid);

		void		SendParameter(CHARACTER * ch);
		void		SendParameterToAll();

		TMemberMap	m_memberMap;
		uint32_t		m_dwLeaderPID;
		CHARACTER *	m_pkChrLeader;

		LPEVENT		m_eventUpdate;

		TMemberMap::iterator m_itNextOwner;

	private:
		int		m_iExpDistributionMode;
		CHARACTER *	m_pkChrExpCentralize;

		uint32_t		m_dwPartyStartTime;

		uint32_t		m_dwPartyHealTime;
		bool		m_bPartyHealReady;
		bool		m_bCanUsePartyHeal;

		int		m_anRoleCount[PARTY_ROLE_MAX_NUM];
		int		m_anMaxRole[PARTY_ROLE_MAX_NUM];

		int		m_iLongTimeExpBonus;

		// used in Update
		int		m_iLeadership;
		int		m_iExpBonus;

		// changed only in Update
		int		m_iCountNearPartyMember;

		bool		m_bPCParty;

		TFlagMap	m_map_iFlag;

		CDungeon *	m_pkDungeon;
		// Dungeon member variable for the Hungry Ghost (Agwi) cave.
		// I really didn't want to go this far, but party management in dungeons is a total mess,
		// so this is a temporary workaround until that is fixed.
		CDungeon *	m_pkDungeon_for_Only_party;
	public:
		void SetDungeon_for_Only_party(CDungeon * pDungeon);
		CDungeon * GetDungeon_for_Only_party();
};

template <class Func> void CParty::ForEachMember(Func & f)
{
	TMemberMap::iterator it;

	for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
		f(it->first);
}

template <class Func> void CParty::ForEachMemberPtr(Func & f)
{
	TMemberMap::iterator it;

	for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
		f(it->second.pCharacter);
}

template <class Func> void CParty::ForEachOnlineMember(Func & f)
{
	TMemberMap::iterator it;

	for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
		if (it->second.pCharacter)
			f(it->second.pCharacter);
}

template <class Func> void CParty::ForEachNearMember(Func & f)
{
	TMemberMap::iterator it;

	for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
		if (it->second.pCharacter && it->second.bNear)
			f(it->second.pCharacter);
}

template <class Func> void CParty::ForEachOnMapMember (Func & f, int32_t lMapIndex)
{
	TMemberMap::iterator it;

	for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
	{
		CHARACTER * ch = it->second.pCharacter;
		if (ch)
		{
			if (ch->GetMapIndex () == lMapIndex)
				f(ch);
		}
	}
}

template <class Func> bool CParty::ForEachOnMapMemberBool(Func & f, int32_t lMapIndex)
{
	TMemberMap::iterator it;

	for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
	{
		CHARACTER * ch = it->second.pCharacter;
		if (ch)
		{
			if (ch->GetMapIndex () == lMapIndex)
			{
				if(f(ch) == false)
				{
					return false;
			
				}
			}
		}
	}
	return true;
}
