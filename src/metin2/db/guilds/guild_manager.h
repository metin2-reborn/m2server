// vim:ts=8 sw=4
#pragma once

#include <metin2/db/clients/peer.h>

#include <metin2/proto/length.h>
#include <metin2/common/singleton.h>
#include <metin2/proto/tables.h>
#include <metin2/poly/expression.h>
#include <metin2/sql/async_sql.h>

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

	guild_declare_info(uint8_t type_value, uint32_t guild_id1, uint32_t guild_id2)
		: type(type_value)
	{
		guild_id[0] = guild_id1;
		guild_id[1] = guild_id2;
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

	guild_wait_start_info(uint8_t type_value,
			uint32_t g1,
			uint32_t g2,
			int32_t war_price_value,
			int32_t initial_score_value,
			guild_war_reservation * reserve_value)
		: type(type_value), war_price(war_price_value), initial_score(initial_score_value), reserve(reserve_value)
	{
		gid[0] = g1;
		gid[1] = g2;
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

	guild_war_pq_element(uint8_t type_value, uint32_t gid1, uint32_t gid2) : end(false), type(type_value)
	{
		type = type_value;
		gid[0] = gid1;
		gid[1] = gid2;
		score[0] = score[1] = 0;
		bet_score[0] = bet_score[1] = 0;
	}
};

struct guild_skill_used
{
    uint32_t gid;
    uint32_t skill_vnum;

    // GUILD_SKILL_COOLTIME_BUG_FIX
    guild_skill_used(uint32_t gid_value, uint32_t skill_vnum_value) : gid(gid_value), skill_vnum(skill_vnum_value)
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
	guild_war_reservation(const guild_war_reserve& table);

	void initialize();

	guild_war_reserve & data_ref()
	{
	    return data_;
	}

	void	on_setup(peer * peer);
	bool	bet(const char * login_value, uint32_t gold_value, uint32_t guild_value);
	void	draw();
	void	end(int score_from, int score_to);

	int	last_notice_min() { return last_notice_min_; }
	void	set_last_notice_min(int min) { last_notice_min_ = min; }

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

	void	load(uint32_t guild_id);

	guild & touch_guild(uint32_t gid);

	void	update();

	void	on_setup(peer * peer);
	void	start_war(uint8_t type, uint32_t gid1, uint32_t gid2, guild_war_reservation * reserve_value = nullptr);

	void	update_score(uint32_t gain_gid, uint32_t opp_gid, int score_delta, int bet_score_delta);

	void	add_declare(uint8_t type, uint32_t guild_from, uint32_t guild_to);
	void	remove_declare(uint32_t guild_from, uint32_t guild_to);

	bool	take_bet_price(uint32_t guild_to, uint32_t guild_from, int32_t war_price);

	void	recv_war_end(uint32_t gid1, uint32_t gid2);
	void	recv_war_over(uint32_t guild_winner, uint32_t guild_loser, bool draw_value, int32_t war_price);

	void	change_ladder_point(uint32_t gid, int change);

	void	use_skill(uint32_t gid, uint32_t skill_vnum, uint32_t cooltime);

	void	deposit_money(uint32_t guild, int32_t gold_value);
	void	withdraw_money(peer* peer, uint32_t guild_value, int32_t gold_value);
	void	withdraw_money_reply(uint32_t guild, uint8_t give_success, int32_t gold_value);

	void	money_change(uint32_t guild_value, uint32_t gold);

	void	query_ranking();
	void	result_ranking(MYSQL_RES * res);
	int	ranking(uint32_t gid);

	//
	// Reserve War
	//
	void	boot_reserve_war();
	bool	reserve_war(packet_guild_war * p);
	void	process_reserve_war();
	bool	bet(uint32_t id, const char * login, uint32_t gold, uint32_t guild);

	void	cancel_war(uint32_t gid1, uint32_t gid2);

	bool	change_master(uint32_t gid, uint32_t from, uint32_t to);

    private:
	void parse_result(ql_result * res);

	void remove_war(uint32_t gid1, uint32_t gid2);	// erase war from m_WarMap and set end on priority queue

	void war_end(uint32_t gid1, uint32_t gid2, bool force_draw = false);

	int ladder_point(uint32_t gid);

	void guild_war_win(uint32_t gid);
	void guild_war_draw(uint32_t gid);
	void guild_war_lose(uint32_t gid);

	void process_draw(uint32_t guild_id1, uint32_t guild_id2);
	void process_win_lose(uint32_t guild_winner, uint32_t guild_loser);

	bool is_half_win_ladder_point(uint32_t guild_winner, uint32_t guild_loser);

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
