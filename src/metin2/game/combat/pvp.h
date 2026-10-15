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

		pvp(uint32_t pid1, uint32_t pid2);
		pvp(pvp & k);
		~pvp();

		void	win(uint32_t pid_value);
		bool	is_fight();
		bool	agree(uint32_t pid_value);

		void	set_vid(uint32_t pid_value, uint32_t vid_value);
		void	packet(bool removing = false);

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

	void			insert(character * chr, character * victim);
	bool			can_attack(character * chr, character * victim);
	bool			dead(character * chr, uint32_t killer_pid);
	void			connect(character * chr);
	void			disconnect(character * chr);

	void			send_list(desc * d);
	void			remove(pvp * pvp_value);

	void			process();

	public:
	pvp *			find(uint32_t crc);
	protected:
	void			connect_ex(character * chr, bool disconnect);

	std::map<uint32_t, std::unique_ptr<pvp>>	map_pk_pvp_;
	pvp_set_map		map_pk_pvp_set_by_id_;
};
}
