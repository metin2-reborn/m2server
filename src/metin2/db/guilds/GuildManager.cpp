#include <metin2/db/guilds/GuildManager.h>

#include <metin2/db/clients/ClientManager.h>
#include <metin2/config/config.h>
#include <metin2/db/storage/DBManager.h>
#include <metin2/db/app/Main.h>
#include <metin2/db/clients/Peer.h>
#include <metin2/db/clients/QID.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/common/utils.h>
#include <metin2/sql/AsyncSQL.h>
#include <metin2/sql/query.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <mysql.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <utility>

namespace metin2::db::inline guilds {

const int GUILD_RANK_MAX_NUM = 20;

uint32_t get_guild_war_wait_start_duration()
{
	return 5;
}

uint32_t get_guild_war_reserve_seconds()
{
	return 10;
}

namespace 
{
	struct f_send_peer_war
	{
		f_send_peer_war(uint8_t bType, uint8_t bWar, uint32_t GID1, uint32_t GID2)
		{
			if (number(0, 1))
				std::swap(GID1, GID2);

			memset(&p, 0, sizeof(packet_guild_war));

			p.war = bWar;
			p.type = bType;
			p.guild_from = GID1;
			p.guild_to = GID2;
		}

		void operator() (peer* peer)
		{
			if (peer->channel() == 0)
				return;

			peer->encode_header(HEADER_DG_GUILD_WAR, 0, sizeof(packet_guild_war));
			peer->encode(&p, sizeof(packet_guild_war));
		}

		packet_guild_war p;
	};

	struct f_send_guild_war_score
	{
		f_send_guild_war_score(uint32_t guild_gain, uint32_t dwOppGID, int iScore, int iBetScore)
		{
			pck.guild_gain_point = guild_gain;
			pck.guild_opponent = dwOppGID;
			pck.score = iScore;
			pck.bet_score = iBetScore;
		}

		void operator() (peer* peer)
		{
			if (peer->channel() == 0)
				return;

			peer->encode_header(HEADER_DG_GUILD_WAR_SCORE, 0, sizeof(pck));
			peer->encode(&pck, sizeof(pck));
		}

		packet_guild_war_score pck;
	};
}

guild_manager::guild_manager()
{
}

guild_manager::~guild_manager()
{
	while (!pq_on_war_.empty())
	{
		if (!pq_on_war_.top().second->end)
			delete pq_on_war_.top().second;

		pq_on_war_.pop();
	}
}

guild & guild_manager::touch_guild(uint32_t GID)
{
	auto it = map_k_guild_.find(GID);

	if (it != map_k_guild_.end())
		return it->second;

	guild info;
	map_k_guild_.insert(std::map<uint32_t, guild>::value_type(GID, info));
	return map_k_guild_[GID];
}

void guild_manager::parse_result(ql_result * pRes)
{
	MYSQL_ROW row;

	while ((row = mysql_fetch_row(pRes->sql_result)))
	{
		uint32_t GID = strtoul(row[0], nullptr, 10);

		guild & r_info = touch_guild(GID);

		r_info.name = row[1];
		str_to_number(r_info.ladder_point, row[2]);
		str_to_number(r_info.win, row[3]);
		str_to_number(r_info.draw, row[4]);
		str_to_number(r_info.loss, row[5]);
		str_to_number(r_info.gold, row[6]);
		str_to_number(r_info.level, row[7]);

		metin2::log::info( 
				"GuildWar: {:<24} ladder {:<5d} win {:<3d} draw {:<3d} loss {:<3d}", 
				r_info.name.c_str(),
				r_info.ladder_point,
				r_info.win,
				r_info.draw,
				r_info.loss);
	}
}

void guild_manager::initialize()
{
	char szQuery[1024];
	snprintf(szQuery, sizeof(szQuery), "SELECT id, name, ladder_point, win, draw, loss, gold, level FROM guild%s", get_table_postfix());
	std::unique_ptr<ql_msg> pmsg(db_manager::instance().direct_query(szQuery));

	if (pmsg->get()->num_rows)
		parse_result(pmsg->get());

	std::string power = metin2::config::db().guild_war_power;
	const char * str = power.c_str();

	if (!poly_power_.analyze(str))
		metin2::log::error("cannot set power poly: {}", str);
	else
		metin2::log::info( "POWER_POLY: {}", str);

	std::string handicap = metin2::config::db().guild_war_handicap;
	str = handicap.c_str();

	if (!poly_handicap_.analyze(str))
		metin2::log::error("cannot set handicap poly: {}", str);
	else
		metin2::log::info( "HANDICAP_POLY: {}", str);

	query_ranking();
}

void guild_manager::load(uint32_t dwGuildID)
{
	char szQuery[1024];

	snprintf(szQuery, sizeof(szQuery), "SELECT id, name, ladder_point, win, draw, loss, gold, level FROM guild%s WHERE id=%u", get_table_postfix(), dwGuildID);
	std::unique_ptr<ql_msg> pmsg(db_manager::instance().direct_query(szQuery));

	if (pmsg->get()->num_rows)
		parse_result(pmsg->get());
}

void guild_manager::query_ranking()
{
	char szQuery[256];
	snprintf(szQuery, sizeof(szQuery), "SELECT id,name,ladder_point FROM guild%s ORDER BY ladder_point DESC LIMIT 20", get_table_postfix());

	db_manager::instance().return_query(szQuery, QID_GUILD_RANKING, 0, nullptr);
}

int guild_manager::ranking(uint32_t dwGID)
{
	auto it = map_k_ladder_point_ranking_by_gid_.find(dwGID);

	if (it == map_k_ladder_point_ranking_by_gid_.end())
		return GUILD_RANK_MAX_NUM;

	return std::clamp(it->second, 0, GUILD_RANK_MAX_NUM);
}

void guild_manager::result_ranking(MYSQL_RES * pRes)
{
	if (!pRes)
		return;

	int iLastLadderPoint = -1;
	int iRank = 0;

	map_k_ladder_point_ranking_by_gid_.clear();

	MYSQL_ROW row;

	while ((row = mysql_fetch_row(pRes)))
	{
		uint32_t	dwGID = 0; str_to_number(dwGID, row[0]);
		int	iLadderPoint = 0; str_to_number(iLadderPoint, row[2]);

		if (iLadderPoint != iLastLadderPoint)
			++iRank;

		metin2::log::info( "GUILD_RANK: {:<24} {:2d} {:d}", row[1], iRank, iLadderPoint);

		map_k_ladder_point_ranking_by_gid_.insert(std::make_pair(dwGID, iRank));
	}
}

void guild_manager::update()
{
	process_reserve_war(); // handle scheduled wars

	time_t now = client_manager::instance().current_time();

	if (!pq_on_war_.empty())
	{
		// UNKNOWN_GUILD_MANAGE_UPDATE_LOG
		/*
		   metin2::log::info( "GuildManager::Update size {:d} now {:d} top {:d}, {}({:d}) vs {}({:d})", 
		   m_WarMap.size(),
		   now,
		   m_pqOnWar.top().first,
		   m_map_kGuild[m_pqOnWar.top().second->GID[0]].szName,
		   m_pqOnWar.top().second->GID[0],
		   m_map_kGuild[m_pqOnWar.top().second->GID[1]].szName,
		   m_pqOnWar.top().second->GID[1]);
		   */
		// END_OF_UNKNOWN_GUILD_MANAGE_UPDATE_LOG

		while (!pq_on_war_.empty() && (pq_on_war_.top().first <= now || (pq_on_war_.top().second && pq_on_war_.top().second->end)))
		{
			guild_war_pq_element * e = pq_on_war_.top().second;

			pq_on_war_.pop();

			if (e)
			{
				if (!e->end)
					war_end(e->gid[0], e->gid[1], false); 

				delete e;
			}
		}
	}

	// GUILD_SKILL_COOLTIME_BUG_FIX
	while (!pq_skill_.empty() && pq_skill_.top().first <= now)
	{
		const guild_skill_used& s = pq_skill_.top().second;
		client_manager::instance().send_guild_skill_usable(s.gid, s.skill_vnum, true);
		pq_skill_.pop();
	}
	// END_OF_GUILD_SKILL_COOLTIME_BUG_FIX

	while (!pq_wait_start_.empty() && pq_wait_start_.top().first <= now)
	{
		const guild_wait_start_info & ws = pq_wait_start_.top().second;
		pq_wait_start_.pop();

		start_war(ws.type, ws.gid[0], ws.gid[1], ws.reserve); // insert new element to m_WarMap and m_pqOnWar

		if (ws.initial_score)
		{
			update_score(ws.gid[0], ws.gid[1], ws.initial_score, 0);
			update_score(ws.gid[1], ws.gid[0], ws.initial_score, 0);
		}

		packet_guild_war p;

		p.type		= ws.type;
		p.war		= GUILD_WAR_ON_WAR;
		p.guild_from	= ws.gid[0];
		p.guild_to	= ws.gid[1];

		client_manager::instance().forward_packet(HEADER_DG_GUILD_WAR, &p, sizeof(p));
		metin2::log::info( "GuildWar: GUILD sending start of wait start war {:d} {:d}", ws.gid[0], ws.gid[1]);
	}
}

void guild_manager::on_setup(peer* peer)
{
	for (const auto & [g1, wars] : war_map_)
		for (const auto & [g2, war] : wars)
		{
			guild_war_pq_element* p = war.element;

			if (!p || p->end)
				continue;

			f_send_peer_war(p->type, GUILD_WAR_ON_WAR, g1, g2) (peer);
			f_send_guild_war_score(p->gid[0], p->gid[1], p->score[0], p->bet_score[0]) (peer);
			f_send_guild_war_score(p->gid[1], p->gid[0], p->score[1], p->bet_score[1]) (peer);
		}

	for (const auto & declare : declare_map_)
	{
		f_send_peer_war(declare.type, GUILD_WAR_SEND_DECLARE, declare.guild_id[0], declare.guild_id[1]) (peer);
	}

	for (const auto & [id, reserve] : map_k_war_reserve_)
	{
		reserve->on_setup(peer);
	}
}

void guild_manager::guild_war_win(uint32_t GID)
{
	auto it = map_k_guild_.find(GID);

	if (it == map_k_guild_.end())
		return;

	++it->second.win;

	char buf[1024];
	snprintf(buf, sizeof(buf), "UPDATE guild%s SET win=%d WHERE id=%u", get_table_postfix(), it->second.win, GID);
	db_manager::instance().async_query(buf);
}

void guild_manager::guild_war_lose(uint32_t GID)
{
	auto it = map_k_guild_.find(GID);

	if (it == map_k_guild_.end())
		return;

	++it->second.loss;

	char buf[1024];
	snprintf(buf, sizeof(buf), "UPDATE guild%s SET loss=%d WHERE id=%u", get_table_postfix(), it->second.loss, GID);
	db_manager::instance().async_query(buf);
}

void guild_manager::guild_war_draw(uint32_t GID)
{
	auto it = map_k_guild_.find(GID);

	if (it == map_k_guild_.end())
		return;

	++it->second.draw;

	char buf[1024];
	snprintf(buf, sizeof(buf), "UPDATE guild%s SET draw=%d WHERE id=%u", get_table_postfix(), it->second.draw, GID);
	db_manager::instance().async_query(buf);
}

bool guild_manager::is_half_win_ladder_point(uint32_t dwGuildWinner, uint32_t dwGuildLoser)
{
	uint32_t GID1 = dwGuildWinner;
	uint32_t GID2 = dwGuildLoser;

	if (GID1 > GID2)
		std::swap(GID1, GID2);

	auto it = guild_war_end_time_[GID1].find(GID2);

	if (it != guild_war_end_time_[GID1].end() && 
			it->second + GUILD_WAR_LADDER_HALF_PENALTY_TIME > client_manager::instance().current_time())
		return true;

	return false;
}

void guild_manager::process_draw(uint32_t dwGuildID1, uint32_t dwGuildID2)
{
	metin2::log::info( "GuildWar: \tThe war between {:d} and {:d} is ended in draw", dwGuildID1, dwGuildID2);

	guild_war_draw(dwGuildID1);
	guild_war_draw(dwGuildID2);
	change_ladder_point(dwGuildID1, 0);
	change_ladder_point(dwGuildID2, 0);

	query_ranking();
}

void guild_manager::process_win_lose(uint32_t dwGuildWinner, uint32_t dwGuildLoser)
{
	guild_war_win(dwGuildWinner);
	guild_war_lose(dwGuildLoser);
	metin2::log::info( "GuildWar: \tWinner : {:d} Loser : {:d}", dwGuildWinner, dwGuildLoser);

	int iPoint = ladder_point(dwGuildLoser);
	int gain = (int)(iPoint * 0.05);
	int loss = (int)(iPoint * 0.07);

	if (is_half_win_ladder_point(dwGuildWinner, dwGuildLoser))
		gain /= 2;

	metin2::log::info( "GuildWar: \tgain : {:d} loss : {:d}", gain, loss);

	change_ladder_point(dwGuildWinner, gain);
	change_ladder_point(dwGuildLoser, -loss);

	query_ranking();
}

void guild_manager::remove_war(uint32_t GID1, uint32_t GID2)
{
	metin2::log::info( "GuildWar: RemoveWar({:d}, {:d})", GID1, GID2);

	if (GID1 > GID2)
		std::swap(GID2, GID1);

	auto it = war_map_[GID1].find(GID2);

	if (it == war_map_[GID1].end())
	{
		if (war_map_[GID1].empty())
			war_map_.erase(GID1);

		return;
	}

	if (it->second.element)
		it->second.element->end = true;

	guild_war_end_time_[GID1][GID2] = client_manager::instance().current_time();

	war_map_[GID1].erase(it);

	if (war_map_[GID1].empty())
		war_map_.erase(GID1);
}

//
// guild war abnormal end and field war end
//
void guild_manager::war_end(uint32_t GID1, uint32_t GID2, bool bForceDraw)
{
	if (GID1 > GID2)
		std::swap(GID2, GID1);

	metin2::log::info( "GuildWar: WarEnd {:d} {:d}", GID1, GID2);

	auto itWarMap = war_map_[GID1].find(GID2);

	if (itWarMap == war_map_[GID1].end())
	{
		metin2::log::error("GuildWar: war not exist or already ended. [1]");
		return;
	}

	guild_war_info gwi = itWarMap->second;
	guild_war_pq_element * pData = gwi.element;

	if (!pData || pData->end)
	{
		metin2::log::error("GuildWar: war not exist or already ended. [2]");
		return;
	}

	uint32_t win_guild = pData->gid[0];
	uint32_t lose_guild = pData->gid[1];

	bool bDraw = false;

	if (!bForceDraw) // check scores unless it's a forced draw.
	{
		if (pData->score[0] > pData->score[1])
		{
			win_guild = pData->gid[0];
			lose_guild = pData->gid[1];
		}
		else if (pData->score[1] > pData->score[0])
		{
			win_guild = pData->gid[1];
			lose_guild = pData->gid[0];
		}
		else
			bDraw = true;
	}
	else // a forced draw is always a draw
		bDraw = true;

	if (bDraw)
		process_draw(win_guild, lose_guild);
	else
		process_win_lose(win_guild, lose_guild);

	// the DB server sometimes ends it on its own, so a packet must be sent separately.
	client_manager::instance().for_each_peer(f_send_peer_war(0, GUILD_WAR_END, GID1, GID2));

	remove_war(GID1, GID2);
}

//
// guild war normal end
// 
void guild_manager::recv_war_over(uint32_t dwGuildWinner, uint32_t dwGuildLoser, bool bDraw, int32_t lWarPrice)
{
	metin2::log::info( "GuildWar: RecvWarOver : winner {:d} vs {:d} draw? {:d} war_price {:d}", dwGuildWinner, dwGuildLoser, bDraw ? 1 : 0, lWarPrice);

	uint32_t GID1 = dwGuildWinner;
	uint32_t GID2 = dwGuildLoser;

	if (GID1 > GID2)
		std::swap(GID1, GID2);

	auto it = war_map_[GID1].find(GID2);

	if (it == war_map_[GID1].end())
		return;

	guild_war_info & gw = it->second;

	// Award
	if (bDraw)
	{
		// give bet money / 2 to both guild
		deposit_money(dwGuildWinner, lWarPrice / 2);
		deposit_money(dwGuildLoser, lWarPrice / 2);
		process_draw(dwGuildWinner, dwGuildLoser);
	}
	else
	{
		// give bet money to winner guild
		deposit_money(dwGuildWinner, lWarPrice);
		process_win_lose(dwGuildWinner, dwGuildLoser);
	}

	if (gw.reserve)
	{
		if (bDraw || !gw.element)
			gw.reserve->draw();
		else if (gw.element->type == GUILD_WAR_TYPE_BATTLE)
			gw.reserve->end(gw.element->bet_score[0], gw.element->bet_score[1]);
	}

	remove_war(GID1, GID2);
}

void guild_manager::recv_war_end(uint32_t GID1, uint32_t GID2)
{
	metin2::log::info( "GuildWar: RecvWarEnded : {:d} vs {:d}", GID1, GID2);
	war_end(GID1, GID2, true); // must always end abnormally.
}

void guild_manager::start_war(uint8_t bType, uint32_t GID1, uint32_t GID2, guild_war_reservation * pkReserve)
{
	metin2::log::info( "GuildWar: StartWar({:d},{:d},{:d})", bType, GID1, GID2);

	if (GID1 > GID2)
		std::swap(GID1, GID2);

	guild_war_info & gw = war_map_[GID1][GID2]; // map insert

	if (bType == GUILD_WAR_TYPE_FIELD)
		gw.t_end_time = client_manager::instance().current_time() + GUILD_WAR_DURATION;
	else
		gw.t_end_time = client_manager::instance().current_time() + 172800;

	gw.element = new guild_war_pq_element(bType, GID1, GID2);
	gw.reserve = pkReserve;

	pq_on_war_.push(std::make_pair(gw.t_end_time, gw.element));
}

void guild_manager::update_score(uint32_t dwGainGID, uint32_t dwOppGID, int iScoreDelta, int iBetScoreDelta)
{
	uint32_t GID1 = dwGainGID;
	uint32_t GID2 = dwOppGID;

	if (GID1 > GID2)
		std::swap(GID1, GID2);

	auto it = war_map_[GID1].find(GID2);

	if (it != war_map_[GID1].end())
	{
		guild_war_pq_element * p = it->second.element;

		if (!p || p->end)
		{
			metin2::log::error("GuildWar: war not exist or already ended.");
			return;
		}

		int iNewScore = 0;
		int iNewBetScore = 0;

		if (p->gid[0] == dwGainGID)
		{
			p->score[0] += iScoreDelta;
			p->bet_score[0] += iBetScoreDelta;

			iNewScore = p->score[0];
			iNewBetScore = p->bet_score[0];
		}
		else
		{
			p->score[1] += iScoreDelta;
			p->bet_score[1] += iBetScoreDelta;

			iNewScore = p->score[1];
			iNewBetScore = p->bet_score[1];
		}

		metin2::log::info( "GuildWar: SendGuildWarScore guild {:d} wartype {:d} score_delta {:d} betscore_delta {:d} result {:d}, {:d}",
				dwGainGID, p->type, iScoreDelta, iBetScoreDelta, iNewScore, iNewBetScore);

		client_manager::instance().for_each_peer(f_send_guild_war_score(dwGainGID, dwOppGID, iNewScore, iNewBetScore));
	}
}

void guild_manager::add_declare(uint8_t bType, uint32_t guild_from, uint32_t guild_to)
{
	guild_declare_info di(bType, guild_from, guild_to);

	if (declare_map_.find(di) == declare_map_.end())
		declare_map_.insert(di);

	metin2::log::info( "GuildWar: AddDeclare(Type:{:d},from:{:d},to:{:d})", bType, guild_from, guild_to);
}

void guild_manager::remove_declare(uint32_t guild_from, uint32_t guild_to)
{
	__typeof(declare_map_.begin()) it = declare_map_.find(guild_declare_info(0, guild_from, guild_to));

	if (it != declare_map_.end())
		declare_map_.erase(it);

	it = declare_map_.find(guild_declare_info(0,guild_to, guild_from));

	if (it != declare_map_.end())
		declare_map_.erase(it);

	metin2::log::info( "GuildWar: RemoveDeclare(from:{:d},to:{:d})", guild_from, guild_to);
}

bool guild_manager::take_bet_price(uint32_t dwGuildTo, uint32_t dwGuildFrom, int32_t lWarPrice)
{
	auto it_from = map_k_guild_.find(dwGuildFrom);
	auto it_to = map_k_guild_.find(dwGuildTo);

	if (it_from == map_k_guild_.end() || it_to == map_k_guild_.end())
	{
		metin2::log::info( "TakeBetPrice: guild not exist {:d} {:d}",
				dwGuildFrom, dwGuildTo);
		return false;
	}

	if (it_from->second.gold < lWarPrice || it_to->second.gold < lWarPrice)
	{
		metin2::log::info( "TakeBetPrice: not enough gold {:d} {:d} to {:d} {:d}", 
				dwGuildFrom, it_from->second.gold, dwGuildTo, it_to->second.gold);
		return false;
	}

	it_from->second.gold -= lWarPrice;
	it_to->second.gold -= lWarPrice;

	money_change(dwGuildFrom, it_from->second.gold);
	money_change(dwGuildTo, it_to->second.gold);
	return true;
}

int guild_manager::ladder_point(uint32_t GID)
{
	auto it = map_k_guild_.find(GID);

	if (it == map_k_guild_.end())
		return 0;

	return it->second.ladder_point;
}

void guild_manager::change_ladder_point(uint32_t GID, int change)
{
	auto it = map_k_guild_.find(GID);

	if (it == map_k_guild_.end())
		return;

	guild & r = it->second;

	r.ladder_point += change;

	if (r.ladder_point < 0)
		r.ladder_point = 0;

	char buf[1024];
	snprintf(buf, sizeof(buf), "UPDATE guild%s SET ladder_point=%d WHERE id=%u", get_table_postfix(), r.ladder_point, GID);
	db_manager::instance().async_query(buf);

	metin2::log::info( "GuildManager::ChangeLadderPoint {:d} {:d}", GID, r.ladder_point);
	metin2::log::info( "{}", buf);

	// send packet
	packet_guild_ladder p;

	p.guild = GID;
	p.ladder_point = r.ladder_point;
	p.win = r.win;
	p.draw = r.draw;
	p.loss = r.loss;

	client_manager::instance().forward_packet(HEADER_DG_GUILD_LADDER, &p, sizeof(packet_guild_ladder));
}

void guild_manager::use_skill(uint32_t GID, uint32_t dwSkillVnum, uint32_t dwCooltime)
{
	// GUILD_SKILL_COOLTIME_BUG_FIX
	metin2::log::info( "UseSkill(gid={:d}, skill={:d}) CoolTime({:d}:{:d})", GID, dwSkillVnum, dwCooltime, client_manager::instance().current_time() + dwCooltime);
	pq_skill_.push(std::make_pair(client_manager::instance().current_time() + dwCooltime, guild_skill_used(GID, dwSkillVnum)));
	// END_OF_GUILD_SKILL_COOLTIME_BUG_FIX
}

void guild_manager::money_change(uint32_t dwGuild, uint32_t dwGold)
{
	metin2::log::info( "GuildManager::MoneyChange {:d} {:d}", dwGuild, dwGold);

	packet_dg_guild_money_change p;
	p.guild = dwGuild;
	p.total_gold = dwGold;
	client_manager::instance().forward_packet(HEADER_DG_GUILD_MONEY_CHANGE, &p, sizeof(p));

	char buf[1024];
	snprintf(buf, sizeof(buf), "UPDATE guild%s SET gold=%u WHERE id = %u", get_table_postfix(), dwGold, dwGuild);
	db_manager::instance().async_query(buf);
}

void guild_manager::deposit_money(uint32_t dwGuild, int32_t iGold)
{
	if (iGold <= 0)
		return;

	auto it = map_k_guild_.find(dwGuild);

	if (it == map_k_guild_.end())
	{
		metin2::log::error("No guild by id {:d}", dwGuild);
		return;
	}

	it->second.gold += iGold;
	metin2::log::info( "GUILD: {:d} Deposit {:d} Total {:d}", dwGuild, iGold, it->second.gold);

	money_change(dwGuild, it->second.gold);
}

void guild_manager::withdraw_money(peer* peer, uint32_t dwGuild, int32_t iGold)
{
	auto it = map_k_guild_.find(dwGuild);

	if (it == map_k_guild_.end())
	{
		metin2::log::error("No guild by id {:d}", dwGuild);
		return;
	}

	// has money, so withdraw and raise it
	if (it->second.gold >= iGold)
	{
		it->second.gold -= iGold;
		metin2::log::info( "GUILD: {:d} Withdraw {:d} Total {:d}", dwGuild, iGold, it->second.gold);

		packet_dg_guild_money_withdraw p;
		p.guild = dwGuild;
		p.change_gold = iGold;

		peer->encode_header(HEADER_DG_GUILD_WITHDRAW_MONEY_GIVE, 0, sizeof(packet_dg_guild_money_withdraw));
		peer->encode(&p, sizeof(packet_dg_guild_money_withdraw));
	}
}

void guild_manager::withdraw_money_reply(uint32_t dwGuild, uint8_t bGiveSuccess, int32_t iGold)
{
	auto it = map_k_guild_.find(dwGuild);

	if (it == map_k_guild_.end())
		return;

	metin2::log::info( "GuildManager::WithdrawMoneyReply : guild {:d} success {:d} gold {:d}", dwGuild, bGiveSuccess, iGold);

	if (!bGiveSuccess)
		it->second.gold += iGold;
	else
		money_change(dwGuild, it->second.gold);
}

//
// scheduled guild war (spectators can bet)
//
const int score_by_level[GUILD_MAX_LEVEL+1] =
{
	500,	// level 0 = 500 probably error
	500,	// 1
	1000,
	2000,
	3000,
	4000,
	6000,
	8000,
	10000,
	12000,
	15000,	// 10
	18000,
	21000,
	24000,
	28000,
	32000,
	36000,
	40000,
	45000,
	50000,
	55000,
};

const int score_by_ranking[GUILD_RANK_MAX_NUM+1] =
{
	0,
	55000,	// 1st place
	50000,
	45000,
	40000,
	36000,
	32000,
	28000,
	24000,
	21000,
	18000,	// 10th place
	15000,
	12000,
	10000,
	8000,
	6000,
	4000,
	3000,
	2000,
	1000,
	500		// 20th place
};

void guild_manager::boot_reserve_war()
{
	const char * c_apszQuery[2] = 
	{
		"SELECT id, guild1, guild2, UNIX_TIMESTAMP(time), type, warprice, initscore, bet_from, bet_to, power1, power2, handicap FROM guild_war_reservation WHERE started=1 AND winner=-1",
		"SELECT id, guild1, guild2, UNIX_TIMESTAMP(time), type, warprice, initscore, bet_from, bet_to, power1, power2, handicap FROM guild_war_reservation WHERE started=0"
	};

	for (int i = 0; i < 2; ++i)
	{
		std::unique_ptr<ql_msg> pmsg(db_manager::instance().direct_query(c_apszQuery[i]));

		if (pmsg->get()->num_rows == 0)
			continue;

		MYSQL_ROW row;

		while ((row = mysql_fetch_row(pmsg->get()->sql_result)))
		{
			int col = 0;

			guild_war_reserve t;

			str_to_number(t.id, row[col++]);
			str_to_number(t.guild_from, row[col++]);
			str_to_number(t.guild_to, row[col++]);
			str_to_number(t.time, row[col++]);
			str_to_number(t.type, row[col++]);
			str_to_number(t.war_price, row[col++]);
			str_to_number(t.initial_score, row[col++]);
			str_to_number(t.bet_from, row[col++]);
			str_to_number(t.bet_to, row[col++]);
			str_to_number(t.power_from, row[col++]);
			str_to_number(t.power_to, row[col++]);
			str_to_number(t.handicap, row[col++]);
			t.started = 0;

			auto pkReserve = std::make_unique<guild_war_reservation>(t);

			char buf[512];
			snprintf(buf, sizeof(buf), "GuildWar: BootReserveWar : step %d id %u GID1 %u GID2 %u", i, t.id, t.guild_from, t.guild_to);
			// if i == 0 the DB crashed during the guild war, so treat it as a draw.
			// also, scheduled guild wars with 5 minutes or less left are treated as a draw. (bets are refunded)
			if (i == 0 || (int) t.time - client_manager::instance().current_time() < 0)
			{
				if (i == 0)
					metin2::log::info( "{} : DB was shutdowned while war is being.", buf);
				else
					metin2::log::info( "{} : left time lower than 5 minutes, will be canceled", buf);

				pkReserve->draw();
			}
			else
			{
				metin2::log::info( "{} : OK", buf);
				map_k_war_reserve_.insert(std::make_pair(t.id, std::move(pkReserve)));
			}
		}
	}
}

int get_average_guild_member_level(uint32_t dwGID)
{
	char szQuery[QUERY_MAX_LEN];

	snprintf(szQuery, sizeof(szQuery), 
			"SELECT AVG(level) FROM guild_member%s, player%s AS p WHERE guild_id=%u AND guild_member%s.pid=p.id", 
			get_table_postfix(), get_table_postfix(), dwGID, get_table_postfix());

	std::unique_ptr<ql_msg> msg(db_manager::instance().direct_query(szQuery));

	MYSQL_ROW row;
	row = mysql_fetch_row(msg->get()->sql_result);

	int nAverageLevel = 0; str_to_number(nAverageLevel, row[0]);
	return nAverageLevel;
}

int get_guild_member_count(uint32_t dwGID)
{
	char szQuery[QUERY_MAX_LEN];

	snprintf(szQuery, sizeof(szQuery), "SELECT COUNT(*) FROM guild_member%s WHERE guild_id=%u", get_table_postfix(), dwGID);

	std::unique_ptr<ql_msg> msg(db_manager::instance().direct_query(szQuery));

	MYSQL_ROW row;
	row = mysql_fetch_row(msg->get()->sql_result);

	uint32_t dwCount = 0; str_to_number(dwCount, row[0]);
	return dwCount;
}

bool guild_manager::reserve_war(packet_guild_war * p)
{
	uint32_t GID1 = p->guild_from;
	uint32_t GID2 = p->guild_to;

	if (GID1 > GID2)
		std::swap(GID1, GID2);

	if (p->war_price > 0)
		if (!take_bet_price(GID1, GID2, p->war_price))
			return false;

	guild_war_reserve t;
	memset(&t, 0, sizeof(guild_war_reserve));

	t.guild_from = GID1;
	t.guild_to = GID2;
	t.time = client_manager::instance().current_time() + get_guild_war_reserve_seconds();
	t.type = p->type;
	t.war_price = p->war_price;
	t.initial_score = p->initial_score;

	int lvp, rkp, alv, mc;

	// power calculation
	guild & k1 = touch_guild(GID1);

	lvp = score_by_level[std::min(GUILD_MAX_LEVEL, k1.level)];
	rkp = score_by_ranking[ranking(GID1)];
	alv = get_average_guild_member_level(GID1);
	mc = get_guild_member_count(GID1);

	poly_power_.set_var("lvp", lvp);
	poly_power_.set_var("rkp", rkp);
	poly_power_.set_var("alv", alv);
	poly_power_.set_var("mc", mc);

	t.power_from = (int32_t) poly_power_.eval();
	metin2::log::info( "GuildWar: {:d} lvp {:d} rkp {:d} alv {:d} mc {:d} power {:d}", GID1, lvp, rkp, alv, mc, t.power_from);

	// power calculation
	guild & k2 = touch_guild(GID2);

	lvp = score_by_level[std::min(GUILD_MAX_LEVEL, k2.level)];
	rkp = score_by_ranking[ranking(GID2)];
	alv = get_average_guild_member_level(GID2);
	mc = get_guild_member_count(GID2);

	poly_power_.set_var("lvp", lvp);
	poly_power_.set_var("rkp", rkp);
	poly_power_.set_var("alv", alv);
	poly_power_.set_var("mc", mc);

	t.power_to = (int32_t) poly_power_.eval();
	metin2::log::info( "GuildWar: {:d} lvp {:d} rkp {:d} alv {:d} mc {:d} power {:d}", GID2, lvp, rkp, alv, mc, t.power_to);

	// handicap calculation
	if (t.power_to > t.power_from)
	{
		poly_handicap_.set_var("pA", t.power_to);
		poly_handicap_.set_var("pB", t.power_from);
	}
	else
	{
		poly_handicap_.set_var("pA", t.power_from);
		poly_handicap_.set_var("pB", t.power_to);
	}

	t.handicap = (int32_t) poly_handicap_.eval();
	metin2::log::info( "GuildWar: handicap {:d}", t.handicap);

	// query
	char szQuery[512];

	snprintf(szQuery, sizeof(szQuery),
			"INSERT INTO guild_war_reservation (guild1, guild2, time, type, warprice, initscore, power1, power2, handicap) "
			"VALUES(%u, %u, DATE_ADD(NOW(), INTERVAL 180 SECOND), %u, %d, %d, %d, %d, %d)",
			GID1, GID2, p->type, p->war_price, p->initial_score, t.power_from, t.power_to, t.handicap);

	std::unique_ptr<ql_msg> pmsg(db_manager::instance().direct_query(szQuery));

	if (pmsg->get()->affected_rows == 0 || pmsg->get()->insert_id == 0 || pmsg->get()->affected_rows == (uint32_t)-1)
	{
		metin2::log::error("GuildWar: Cannot insert row");
		return false;
	}

	t.id = pmsg->get()->insert_id;

	map_k_war_reserve_.insert(std::make_pair(t.id, std::make_unique<guild_war_reservation>(t)));

	client_manager::instance().forward_packet(HEADER_DG_GUILD_WAR_RESERVE_ADD, &t, sizeof(guild_war_reserve));
	return true;
}

void guild_manager::process_reserve_war()
{
	uint32_t dwCurTime = client_manager::instance().current_time();

	auto it = map_k_war_reserve_.begin();

	while (it != map_k_war_reserve_.end())
	{
		auto it2 = it++;

		guild_war_reservation * pk = it2->second.get();
		guild_war_reserve & r = pk->data_ref();

		if (!r.started && r.time - 1800 <= dwCurTime) // announce starting 30 minutes before.
		{
			int iMin = (int) ceil((int)(r.time - dwCurTime) / 60.0);

			metin2::log::info( "GuildWar: started GID1 {:d} GID2 {:d} {:d} time {:d} min {:d}", r.guild_from, r.guild_to, r.started, dwCurTime - r.time, iMin);

			if (iMin <= 0)
			{
				char szQuery[128];
				snprintf(szQuery, sizeof(szQuery), "UPDATE guild_war_reservation SET started=1 WHERE id=%u", r.id);
				db_manager::instance().async_query(szQuery);

				client_manager::instance().forward_packet(HEADER_DG_GUILD_WAR_RESERVE_DEL, &r.id, sizeof(uint32_t));

				r.started = true;

				guild_wait_start_info info(r.type, r.guild_from, r.guild_to, r.war_price, r.initial_score, pk);
				pq_wait_start_.push(std::make_pair(dwCurTime + get_guild_war_wait_start_duration(), info));

				packet_guild_war pck;

				pck.type = r.type;
				pck.war = GUILD_WAR_WAIT_START;
				pck.guild_from = r.guild_from;
				pck.guild_to = r.guild_to;
				pck.war_price = r.war_price;
				pck.initial_score = r.initial_score;

				client_manager::instance().forward_packet(HEADER_DG_GUILD_WAR, &pck, sizeof(packet_guild_war));
			}
			else
			{
				if (iMin != pk->last_notice_min())
				{
					pk->set_last_notice_min(iMin);
				}
			}
		}
	}
}

bool guild_manager::bet(uint32_t dwID, const char * c_pszLogin, uint32_t dwGold, uint32_t dwGuild)
{
	auto it = map_k_war_reserve_.find(dwID);

	if (it == map_k_war_reserve_.end())
	{
		metin2::log::info( "WAR_RESERVE: Bet: cannot find reserve war by id {:d}", dwID);
		const std::string query = sql::format("INSERT INTO item_award (login, vnum, socket0, given_time) VALUES({}, {}, {}, NOW())",
				c_pszLogin, ITEM_ELK_VNUM, dwGold);
		db_manager::instance().async_query(query.c_str());
		return false;
	}

	if (!it->second->bet(c_pszLogin, dwGold, dwGuild))
	{
		metin2::log::info( "WAR_RESERVE: Bet: cannot bet id {:d}, login {}, gold {:d}, guild {:d}", dwID, c_pszLogin, dwGold, dwGuild);
		const std::string query = sql::format("INSERT INTO item_award (login, vnum, socket0, given_time) VALUES({}, {}, {}, NOW())", 
				c_pszLogin, ITEM_ELK_VNUM, dwGold);
		db_manager::instance().async_query(query.c_str());
		return false;
	}

	return true;
}

void guild_manager::cancel_war(uint32_t GID1, uint32_t GID2)
{
	remove_declare(GID1, GID2);
	remove_war(GID1, GID2);
}

bool guild_manager::change_master(uint32_t dwGID, uint32_t dwFrom, uint32_t dwTo)
{
	auto iter = map_k_guild_.find(dwGID);

	if (iter == map_k_guild_.end())
		return false;

	char szQuery[1024];

	snprintf(szQuery, sizeof(szQuery), "UPDATE guild%s SET master=%u WHERE id=%u", get_table_postfix(), dwTo, dwGID);
	std::unique_ptr<ql_msg>(db_manager::instance().direct_query(szQuery));

	snprintf(szQuery, sizeof(szQuery), "UPDATE guild_member%s SET grade=1 WHERE pid=%u", get_table_postfix(), dwTo);
	std::unique_ptr<ql_msg>(db_manager::instance().direct_query(szQuery));

	snprintf(szQuery, sizeof(szQuery), "UPDATE guild_member%s SET grade=15 WHERE pid=%u", get_table_postfix(), dwFrom);
	std::unique_ptr<ql_msg>(db_manager::instance().direct_query(szQuery));

	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Guild War Reserve Class
//////////////////////////////////////////////////////////////////////////////////////////
guild_war_reservation::guild_war_reservation(const guild_war_reserve & rTable)
{
	std::memcpy(&data_, &rTable, sizeof(guild_war_reserve));
	last_notice_min_ = -1;

	initialize();
}

void guild_war_reservation::initialize()
{
	char szQuery[256];
	snprintf(szQuery, sizeof(szQuery), "SELECT login, guild, gold FROM guild_war_bet WHERE war_id=%u", data_.id);

	std::unique_ptr<ql_msg> msgbet(db_manager::instance().direct_query(szQuery));

	if (msgbet->get()->num_rows)
	{
		MYSQL_RES * res = msgbet->get()->sql_result;
		MYSQL_ROW row;

		char szLogin[LOGIN_MAX_LEN+1];
		uint32_t dwGuild;
		uint32_t dwGold;

		while ((row = mysql_fetch_row(res)))
		{
			dwGuild = dwGold = 0;
			strlcpy(szLogin, row[0], sizeof(szLogin));
			str_to_number(dwGuild, row[1]);
			str_to_number(dwGold, row[2]);

			bet_.insert(std::make_pair(szLogin, std::make_pair(dwGuild, dwGold)));
		}
	}
}

void guild_war_reservation::on_setup(peer * peer)
{
	if (data_.started) // don't send ones that already started.
		return;

	f_send_peer_war(data_.type, GUILD_WAR_RESERVE, data_.guild_from, data_.guild_to) (peer);

	peer->encode_header(HEADER_DG_GUILD_WAR_RESERVE_ADD, 0, sizeof(guild_war_reserve));
	peer->encode(&data_, sizeof(guild_war_reserve));

	packet_gd_guild_war_bet pckBet;
	pckBet.war_id = data_.id;

	auto it = bet_.begin();

	while (it != bet_.end())
	{
		strlcpy(pckBet.login, it->first.c_str(), sizeof(pckBet.login));
		pckBet.guild = it->second.first;
		pckBet.gold = it->second.second;

		peer->encode_header(HEADER_DG_GUILD_WAR_BET, 0, sizeof(packet_gd_guild_war_bet));
		peer->encode(&pckBet, sizeof(packet_gd_guild_war_bet));

		++it;
	}
}

bool guild_war_reservation::bet(const char * pszLogin, uint32_t dwGold, uint32_t dwGuild)
{
	if (data_.guild_from != dwGuild && data_.guild_to != dwGuild)
	{
		metin2::log::info( "GuildWarReserve::Bet: invalid guild id");
		return false;
	}

	if (data_.started)
	{
		metin2::log::info( "GuildWarReserve::Bet: war is already started");
		return false;
	}

	if (bet_.find(pszLogin) != bet_.end())
	{
		metin2::log::info( "GuildWarReserve::Bet: failed. already bet");
		return false;
	}

	std::string query = sql::format(
			"INSERT INTO guild_war_bet (war_id, login, gold, guild) VALUES({}, {}, {}, {})",
			data_.id, pszLogin, dwGold, dwGuild);

	std::unique_ptr<ql_msg> pmsg(db_manager::instance().direct_query(query.c_str()));

	if (pmsg->get()->affected_rows == 0 || pmsg->get()->affected_rows == (uint32_t)-1)
	{
		metin2::log::info( "GuildWarReserve::Bet: failed. cannot insert row to guild_war_bet table");
		return false;
	}

	if (data_.guild_from == dwGuild)
		data_.bet_from += dwGold;
	else
		data_.bet_to += dwGold;

	client_manager::instance().forward_packet(HEADER_DG_GUILD_WAR_RESERVE_ADD, &data_, sizeof(guild_war_reserve));

	query = sql::format("UPDATE guild_war_reservation SET bet_from={},bet_to={} WHERE id={}", 
			data_.bet_from, data_.bet_to, data_.id);

	db_manager::instance().async_query(query.c_str());

	metin2::log::info( "GuildWarReserve::Bet: success. {} {:d} war_id {:d} bet {:d} : {:d}", pszLogin, dwGuild, data_.id, data_.bet_from, data_.bet_to);
	bet_.insert(std::make_pair(pszLogin, std::make_pair(dwGuild, dwGold)));

	packet_gd_guild_war_bet pckBet;
	pckBet.war_id = data_.id;
	strlcpy(pckBet.login, pszLogin, sizeof(pckBet.login));
	pckBet.guild = dwGuild;
	pckBet.gold = dwGold;

	client_manager::instance().forward_packet(HEADER_DG_GUILD_WAR_BET, &pckBet, sizeof(packet_gd_guild_war_bet));
	return true;
}

//
// draw handling: normally there should be a winner, but in certain situations such as server problems
//              a draw must be possible.
//
void guild_war_reservation::draw() 
{
	std::string query = sql::format("UPDATE guild_war_reservation SET started=1,winner=0 WHERE id={}", data_.id);
	db_manager::instance().async_query(query.c_str());

	if (bet_.empty())
		return;

	metin2::log::info( "WAR_REWARD: Draw. war_id {:d}", data_.id);

	auto it = bet_.begin();

	while (1)
	{
		int iRow = 0;

		query = "INSERT INTO item_award (login, vnum, socket0, given_time) VALUES";

		while (it != bet_.end())
		{
			if (iRow == 0)
				query += sql::format("({}, {}, {}, NOW())", 
						it->first, ITEM_ELK_VNUM, it->second.second);
			else
				query += sql::format(",({}, {}, {}, NOW())", 
						it->first, ITEM_ELK_VNUM, it->second.second);

			it++;

			if (query.size() > 384)
				break;

			++iRow;
		}

		if (iRow > 0)
		{
			metin2::log::info( "WAR_REWARD: QUERY: {}", query.c_str());
			db_manager::instance().async_query(query.c_str());
		}

		if (it == bet_.end())
			break;
	}
}

void guild_war_reservation::end(int iScoreFrom, int iScoreTo)
{
	uint32_t dwWinner;

	metin2::log::info( "WAR_REWARD: End: From {:d} {:d} To {:d} {:d}", data_.guild_from, iScoreFrom, data_.guild_to, iScoreTo);

	if (data_.power_from > data_.power_to)
	{
		if (data_.handicap > iScoreFrom - iScoreTo)
		{
			metin2::log::info( "WAR_REWARD: End: failed to overcome handicap, From is strong but To won");
			dwWinner = data_.guild_to;
		}
		else
		{
			metin2::log::info( "WAR_REWARD: End: success to overcome handicap, From win!");
			dwWinner = data_.guild_from;
		}
	}
	else
	{
		if (data_.handicap > iScoreTo - iScoreFrom) 
		{
			metin2::log::info( "WAR_REWARD: End: failed to overcome handicap, To is strong but From won");
			dwWinner = data_.guild_from;
		}
		else
		{
			metin2::log::info( "WAR_REWARD: End: success to overcome handicap, To win!");
			dwWinner = data_.guild_to;
		}
	}

	std::string query = sql::format("UPDATE guild_war_reservation SET started=1,winner={},result1={},result2={} WHERE id={}", 
			dwWinner, iScoreFrom, iScoreTo, data_.id);
	db_manager::instance().async_query(query.c_str());

	if (bet_.empty())
		return;

	uint32_t dwTotalBet = data_.bet_from + data_.bet_to;
	uint32_t dwWinnerBet = 0;

	if (dwWinner == data_.guild_from)
		dwWinnerBet = data_.bet_from;
	else if (dwWinner == data_.guild_to)
		dwWinnerBet = data_.bet_to;
	else
	{
		metin2::log::error("WAR_REWARD: fatal error, winner does not exist!");
		return;
	}

	if (dwWinnerBet == 0)
	{
		metin2::log::error("WAR_REWARD: total bet money on winner is zero");
		return;
	}

	metin2::log::info( "WAR_REWARD: End: Total bet: {:d}, Winner bet: {:d}", dwTotalBet, dwWinnerBet);

	auto it = bet_.begin();

	while (1)
	{
		int iRow = 0;

		query = "INSERT INTO item_award (login, vnum, socket0, given_time) VALUES";

		while (it != bet_.end())
		{
			if (it->second.first != dwWinner)
			{
				++it;
				continue;
			}

			double ratio = (double) it->second.second / dwWinnerBet;

			// distribute after a 10% tax deduction
			metin2::log::info( "WAR_REWARD: {} {:d} ratio {:f}", it->first.c_str(), it->second.second, ratio);

			uint32_t dwGold = (uint32_t) (dwTotalBet * ratio * 0.9);

			if (iRow == 0)
				query += sql::format("({}, {}, {}, NOW())",
						it->first, ITEM_ELK_VNUM, dwGold);
			else
				query += sql::format(",({}, {}, {}, NOW())",
						it->first, ITEM_ELK_VNUM, dwGold);

			++it;

			if (query.size() > 384)
				break;

			++iRow;
		}

		if (iRow > 0)
		{
			metin2::log::info( "WAR_REWARD: query: {}", query.c_str());
			db_manager::instance().async_query(query.c_str());
		}

		if (it == bet_.end())
			break;
	}
}
}
