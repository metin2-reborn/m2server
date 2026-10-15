#pragma once

#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>

#include <cstdint>
#include <map>
#include <memory>
#include <unordered_set>

namespace metin2::game::inline characters {
    class character;
}

namespace metin2::game::inline combat {


// CPVP takes two uint32_t IDs, builds m_dwCRC from them and keeps it.
// CPVPManager looks entries up by the CRC built this way.
class pvp
{
	public:
		friend class pvp_manager;

		struct player {
			uint32_t	pid;
			uint32_t	vid;
			bool	agree;
			bool	can_revenge;

			player() : pid(0), vid(0), agree(false), can_revenge(false)
			{
			}
		};

		pvp(uint32_t dwID1, uint32_t dwID2);
		pvp(pvp & v);
		~pvp();

		void	win(uint32_t dwPID); // dwPID won!
		bool	is_fight();
		bool	agree(uint32_t dwPID);

		void	set_vid(uint32_t dwPID, uint32_t dwVID);
		void	packet(bool bDelete = false);

		void	set_last_fight_time();
		uint32_t	last_fight_time();

	protected:
		player	players_[2];
		uint32_t	crc_;
		bool	revenge_;

		uint32_t   last_fight_time_;
};

class pvp_manager : public singleton<pvp_manager>
{
	typedef std::map<uint32_t, std::unordered_set<pvp*> > pvp_set_map;

	public:
	pvp_manager();
	~pvp_manager() override;

	void			insert(character * pkChr, character * pkVictim);
	bool			can_attack(character * pkChr, character * pkVictim);
	bool			dead(character * pkChr, uint32_t dwKillerPID);	// Returns whether it was in PVP
	void			connect(character * pkChr);
	void			disconnect(character * pkChr);

	void			send_list(desc * d);
	void			delete_(pvp * pkPVP);

	void			process();

	public:
	pvp *			find(uint32_t dwCRC);
	protected:
	void			connect_ex(character * pkChr, bool bDisconnect);

	std::map<uint32_t, std::unique_ptr<pvp>>	map_pk_pvp_;
	pvp_set_map		map_pk_pvp_set_by_id_;
};
}
