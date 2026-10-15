#pragma once

#include "typedef.h"

#include <common/singleton.h>

#include <cstdint>
#include <map>
#include <memory>
#include <unordered_set>

class CHARACTER;

// CPVP takes two uint32_t IDs, builds m_dwCRC from them and keeps it.
// CPVPManager looks entries up by the CRC built this way.
class CPVP
{
	public:
		friend class CPVPManager;

		struct TPlayer {
			uint32_t	dwPID;
			uint32_t	dwVID;
			bool	bAgree;
			bool	bCanRevenge;

			TPlayer() : dwPID(0), dwVID(0), bAgree(false), bCanRevenge(false)
			{
			}
		};

		CPVP(uint32_t dwID1, uint32_t dwID2);
		CPVP(CPVP & v);
		~CPVP();

		void	Win(uint32_t dwPID); // dwPID won!
		bool	IsFight();
		bool	Agree(uint32_t dwPID);

		void	SetVID(uint32_t dwPID, uint32_t dwVID);
		void	Packet(bool bDelete = false);

		void	SetLastFightTime();
		uint32_t	GetLastFightTime();

	protected:
		TPlayer	m_players[2];
		uint32_t	m_dwCRC;
		bool	m_bRevenge;

		uint32_t   m_dwLastFightTime;
};

class CPVPManager : public singleton<CPVPManager>
{
	typedef std::map<uint32_t, std::unordered_set<CPVP*> > CPVPSetMap;

	public:
	CPVPManager();
	~CPVPManager() override;

	void			Insert(CHARACTER * pkChr, CHARACTER * pkVictim);
	bool			CanAttack(CHARACTER * pkChr, CHARACTER * pkVictim);
	bool			Dead(CHARACTER * pkChr, uint32_t dwKillerPID);	// Returns whether it was in PVP
	void			Connect(CHARACTER * pkChr);
	void			Disconnect(CHARACTER * pkChr);

	void			SendList(DESC * d);
	void			Delete(CPVP * pkPVP);

	void			Process();

	public:
	CPVP *			Find(uint32_t dwCRC);
	protected:
	void			ConnectEx(CHARACTER * pkChr, bool bDisconnect);

	std::map<uint32_t, std::unique_ptr<CPVP>>	m_map_pkPVP;
	CPVPSetMap		m_map_pkPVPSetByID;
};
