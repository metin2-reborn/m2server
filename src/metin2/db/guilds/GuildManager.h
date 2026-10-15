// vim:ts=8 sw=4
#pragma once

#include <metin2/db/clients/Peer.h>

#include <metin2/proto/length.h>
#include <metin2/common/singleton.h>
#include <metin2/proto/tables.h>
#include <metin2/poly/Poly.h>
#include <metin2/sql/AsyncSQL.h>

#include <mysql.h>

#include <cstdint>
#include <ctime>
#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace metin2::db::inline guilds {

enum
{
    GUILD_WARP_WAR_CHANNEL = 99
};

class guild_war_reservation;

struct guild_declare_info
{
	uint8_t type;
	uint32_t guild_id[2];

	guild_declare_info(uint8_t _bType, uint32_t _dwGuildID1, uint32_t _dwGuildID2)
		: type(_bType)
	{
		guild_id[0] = _dwGuildID1;
		guild_id[1] = _dwGuildID2;
	}

	bool operator < (const guild_declare_info& r) const
	{
		return guild_id[0] < r.guild_id[0] || (guild_id[0] == r.guild_id[0] && guild_id[1] < r.guild_id[1]);
	}
};

struct guild_wait_start_info
{
	uint8_t			type;
	uint32_t			gid[2];
	int32_t			war_price;
	int32_t			initial_score;
	guild_war_reservation *	reserve;

	guild_wait_start_info(uint8_t _bType,
			uint32_t _g1,
			uint32_t _g2,
			int32_t _lWarPrice,
			int32_t _lInitialScore,
			guild_war_reservation * _pkReserve)
		: type(_bType), war_price(_lWarPrice), initial_score(_lInitialScore), reserve(_pkReserve)
	{
		gid[0] = _g1;
		gid[1] = _g2;
	}

	bool operator < (const guild_wait_start_info& r) const
	{
		return gid[0] < r.gid[0] || (gid[0] == r.gid[0] && gid[1] < r.gid[1]);
	}
};

struct guild_war_pq_element
{
	bool	end;
	uint8_t	type;
	uint32_t	gid[2];
	uint32_t	score[2];
	uint32_t	bet_score[2];

	guild_war_pq_element(uint8_t _bType, uint32_t GID1, uint32_t GID2) : end(false), type(_bType)
	{
		type = _bType;
		gid[0] = GID1;
		gid[1] = GID2;
		score[0] = score[1] = 0;
		bet_score[0] = bet_score[1] = 0;
	}
};

struct guild_skill_used
{
    uint32_t gid;
    uint32_t skill_vnum;

    // GUILD_SKILL_COOLTIME_BUG_FIX
    guild_skill_used(uint32_t _GID, uint32_t _dwSkillVnum) : gid(_GID), skill_vnum(_dwSkillVnum)
    {
    }
    // END_OF_GUILD_SKILL_COOLTIME_BUG_FIX
};

inline bool operator < (const guild_skill_used& a, const guild_skill_used& b)
{
    return a.gid < b.gid || (a.gid == b.gid && a.skill_vnum < b.skill_vnum);
}

struct guild {
	guild() : ladder_point(0), win(0), draw(0), loss(0), gold(0), level(0)
	{
	}

	std::string name;
	int	ladder_point;
	int	win;
	int	draw;
	int	loss;
	int	gold;
	int	level;
};

struct guild_war_info {
    time_t		t_end_time;
    guild_war_pq_element * element;
    guild_war_reservation * reserve;

    guild_war_info() : element(nullptr)
    {
    }
};

class guild_war_reservation
{
    public:
	guild_war_reservation(const guild_war_reserve& rTable);

	void initialize();

	guild_war_reserve & data_ref()
	{
	    return data_;
	}

	void	on_setup(peer * peer);
	bool	bet(const char * pszLogin, uint32_t dwGold, uint32_t dwGuild);
	void	draw();
	void	end(int iScoreFrom, int iScoreTo);

	int	last_notice_min() { return last_notice_min_; }
	void	set_last_notice_min(int iMin) { last_notice_min_ = iMin; }

    private:
	guild_war_reservation();  // Intentionally not implemented to prevent use of the default constructor

	guild_war_reserve				data_;
	// <login, <guild, gold>>
	std::map<std::string, std::pair<uint32_t, uint32_t> > bet_;
	int						last_notice_min_;
};

class guild_manager : public singleton<guild_manager>
{
    public:
	guild_manager();
	~guild_manager() override;

	void	initialize();

	void	load(uint32_t dwGuildID);

	guild & touch_guild(uint32_t GID);

	void	update();

	void	on_setup(peer * peer);
	void	start_war(uint8_t bType, uint32_t GID1, uint32_t GID2, guild_war_reservation * pkReserve = nullptr);

	void	update_score(uint32_t guild_gain_point, uint32_t guild_opponent, int iScore, int iBetScore);

	void	add_declare(uint8_t bType, uint32_t guild_from, uint32_t guild_to);
	void	remove_declare(uint32_t guild_from, uint32_t guild_to);

	bool	take_bet_price(uint32_t dwGuildTo, uint32_t dwGuildFrom, int32_t lWarPrice);

	void	recv_war_end(uint32_t GID1, uint32_t GID2);
	void	recv_war_over(uint32_t dwGuildWinner, uint32_t dwGuildLoser, bool bDraw, int32_t lWarPrice);

	void	change_ladder_point(uint32_t GID, int change);

	void	use_skill(uint32_t dwGuild, uint32_t dwSkillVnum, uint32_t dwCooltime);

	void	deposit_money(uint32_t dwGuild, int32_t lGold);
	void	withdraw_money(peer* peer, uint32_t dwGuild, int32_t lGold);
	void	withdraw_money_reply(uint32_t dwGuild, uint8_t bGiveSuccess, int32_t lGold);

	void	money_change(uint32_t dwGuild, uint32_t dwGold);

	void	query_ranking();
	void	result_ranking(MYSQL_RES * pRes);
	int	ranking(uint32_t dwGID);

	//
	// Reserve War
	//
	void	boot_reserve_war();
	bool	reserve_war(packet_guild_war * p);
	void	process_reserve_war();
	bool	bet(uint32_t dwID, const char * c_pszLogin, uint32_t dwGold, uint32_t dwGuild);

	void	cancel_war(uint32_t GID1, uint32_t GID2);

	bool	change_master(uint32_t dwGID, uint32_t dwFrom, uint32_t dwTo);

    private:
	void parse_result(ql_result * pRes);

	void remove_war(uint32_t GID1, uint32_t GID2);	// erase war from m_WarMap and set end on priority queue

	void war_end(uint32_t GID1, uint32_t GID2, bool bDraw = false);

	int ladder_point(uint32_t GID);

	void guild_war_win(uint32_t GID);
	void guild_war_draw(uint32_t GID);
	void guild_war_lose(uint32_t GID);

	void process_draw(uint32_t dwGuildID1, uint32_t dwGuildID2);
	void process_win_lose(uint32_t dwGuildWinner, uint32_t dwGuildLoser);

	bool is_half_win_ladder_point(uint32_t dwGuildWinner, uint32_t dwGuildLoser);

	std::map<uint32_t, guild>					map_k_guild_;
	std::map<uint32_t, std::map<uint32_t, time_t> >		guild_war_end_time_;

	std::set<guild_declare_info>				declare_map_; // Stores the declaration-of-war state
	std::map<uint32_t, std::map<uint32_t, guild_war_info> >	war_map_;

	typedef std::pair<time_t, guild_war_pq_element *>	st_pair_guild_war;
	typedef std::pair<time_t, guild_skill_used>	st_pair_skill_used;
	typedef std::pair<time_t, guild_wait_start_info>	st_pair_wait_start;

	std::priority_queue<st_pair_guild_war, std::vector<st_pair_guild_war>, std::greater<st_pair_guild_war> >
	    pq_on_war_;
	std::priority_queue<st_pair_wait_start, std::vector<st_pair_wait_start>, std::greater<st_pair_wait_start> >
	    pq_wait_start_;
	std::priority_queue<st_pair_skill_used, std::vector<st_pair_skill_used>, std::greater<st_pair_skill_used> >
	    pq_skill_;

	std::map<uint32_t, std::unique_ptr<guild_war_reservation>>	map_k_war_reserve_;
	expression							poly_power_;
	expression							poly_handicap_;

	// GID Ranking
	std::map<uint32_t, int>					map_k_ladder_point_ranking_by_gid_;
};
}
