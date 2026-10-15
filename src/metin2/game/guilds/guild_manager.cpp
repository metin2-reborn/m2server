#include <metin2/config/config.h>
#include <metin2/game/guilds/guild_manager.h>

#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/config.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/guilds/mark_manager.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/guilds/war_map.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>
#include <metin2/log/log.h>

#include <mysql.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>

namespace metin2::game::inline guilds {

namespace
{

	struct f_guild_name_sender
	{
		f_guild_name_sender(uint32_t id, const char* guild_name) : id(id), name(guild_name)
		{
			p.header = HEADER_GC_GUILD;
			p.subheader = GUILD_SUBHEADER_GC_GUILD_NAME;
			p.size = sizeof(p) + GUILD_NAME_MAX_LEN + sizeof(uint32_t);
		}

		void operator()(character * ch)
		{
			desc * d = ch->get_desc();

			if (d)
			{
				d->buffered_packet(&p, sizeof(p));
				d->buffered_packet(&id, sizeof(id));
				d->packet(name, GUILD_NAME_MAX_LEN);
			}
		}

		uint32_t		id;
		const char *	name;
		packet_gc_guild	p;
	};
}

guild_manager::guild_manager()
{
}

guild_manager::~guild_manager()
{
	guild_.clear();
}

auto guild_manager::disband_delay() -> int
{
	return quest::quest_manager::instance().event_flag("guild_disband_delay") * (metin2::config::test_server() ? 60 : 86400);
}

auto guild_manager::withdraw_delay() -> int
{
	return quest::quest_manager::instance().event_flag("guild_withdraw_delay") * (metin2::config::test_server() ? 60 : 86400);
}

auto guild_manager::create_guild(guild_create_parameter& gcp) -> uint32_t
{
	if (!gcp.master)
		return 0;

	if (!check_name(gcp.name))
	{
		gcp.master->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드 이름이 적합하지 않습니다."));
		return 0;
	}

	std::unique_ptr<sql_msg> pmsg(db_manager::instance().direct_query(sql::format("SELECT COUNT(*) FROM guild{} WHERE name = {}",
				sql::raw(get_table_postfix()), gcp.name)));

	if (pmsg->get()->num_rows > 0)
	{
		MYSQL_ROW row = mysql_fetch_row(pmsg->get()->res);

		if (!(row[0] && row[0][0] == '0'))
		{
			gcp.master->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 이미 같은 이름의 길드가 있습니다."));
			return 0;
		}
	}
	else
	{
		gcp.master->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드를 생성할 수 없습니다."));
		return 0;
	}

	auto pg = std::make_unique<guild>(gcp);
	const uint32_t dwGuildID = pg->id();
	guild_.insert(std::make_pair(dwGuildID, std::move(pg)));
	return dwGuildID;
}

void guild_manager::unlink(uint32_t pid)
{
	map_pk_guild_by_pid_.erase(pid);
}

auto guild_manager::linked_guild(uint32_t pid) -> guild *
{
	guild_map::iterator it = map_pk_guild_by_pid_.find(pid);

	if (it == map_pk_guild_by_pid_.end())
		return nullptr;

	return it->second; 
}

void guild_manager::link(uint32_t pid, guild* guild)
{
	if (map_pk_guild_by_pid_.find(pid) == map_pk_guild_by_pid_.end())
		map_pk_guild_by_pid_.insert(std::make_pair(pid,guild));
}

void guild_manager::p2_p_logout_member(uint32_t pid)
{
	guild_map::iterator it = map_pk_guild_by_pid_.find(pid);

	if (it != map_pk_guild_by_pid_.end())
	{
		it->second->p2_p_logout_member(pid);
	}
}

void guild_manager::p2_p_login_member(uint32_t pid)
{
	guild_map::iterator it = map_pk_guild_by_pid_.find(pid);

	if (it != map_pk_guild_by_pid_.end())
	{
		it->second->p2_p_login_member(pid);
	}
}

void guild_manager::login_member(character * ch)
{
	guild_map::iterator it = map_pk_guild_by_pid_.find(ch->player_id());

	if (it != map_pk_guild_by_pid_.end())
	{
		it->second->login_member(ch);
	}
}


auto guild_manager::touch_guild(uint32_t guild_id) -> guild*
{
	auto it = guild_.find(guild_id);

	if (it == guild_.end())
	{
		guild_.insert(std::make_pair(guild_id, std::make_unique<guild>(guild_id)));
		it = guild_.find(guild_id);

		character_manager::instance().for_each_pc(f_guild_name_sender(guild_id, it->second->name()));
	}

	return it->second.get();
}

auto guild_manager::find_guild(uint32_t guild_id) -> guild*
{
	auto it = guild_.find(guild_id);
	if (it == guild_.end())
	{
		return nullptr;
	}
	return it->second.get();
}

auto	guild_manager::find_guild_by_name(const std::string guild_name) -> guild*
{
	decltype(guild_.begin()) it;
	for (it = guild_.begin(); it!=guild_.end(); ++it)
	{
		if (it->second->name()==guild_name)
			return it->second.get();
	}
	return nullptr;
}

void guild_manager::initialize()
{
	metin2::log::info( "Initializing Guild");

	if (metin2::config::is_auth())
	{
		metin2::log::info( "	No need for auth server");
		return;
	}

	std::unique_ptr<sql_msg> pmsg(db_manager::instance().direct_query("SELECT id FROM guild%s", get_table_postfix()));

	for (uint i = 0; i < pmsg->get()->num_rows; i++)
	{
		MYSQL_ROW row = mysql_fetch_row(pmsg->get()->res);
		uint32_t guild_id = strtoul(row[0], (char**) nullptr, 10);
		load_guild(guild_id);
	}

	guild_mark_manager & rkMarkMgr = guild_mark_manager::instance();

	rkMarkMgr.set_mark_path_prefix("mark");
	mkdir("mark", S_IRWXU);

	rkMarkMgr.load_mark_index();
	rkMarkMgr.load_mark_images();
	rkMarkMgr.load_symbol(GUILD_SYMBOL_FILENAME);
}

void guild_manager::load_guild(uint32_t guild_id)
{
	auto it = guild_.find(guild_id);

	if (it == guild_.end())
	{
		guild_.insert(std::make_pair(guild_id, std::make_unique<guild>(guild_id)));
	}
}

void guild_manager::disband_guild(uint32_t guild_id)
{
	auto it = guild_.find(guild_id);

	if (it == guild_.end())
		return;

	it->second->disband();

	guild_.erase(it);

	guild_mark_manager::instance().delete_mark(guild_id);
}

void guild_manager::skill_recharge()
{
	for (auto it = guild_.begin(); it!=guild_.end();++it)
	{
		it->second->skill_recharge();
	}
}

auto guild_manager::rank(guild* g) -> int
{
	int point = g->ladder_point();
	int rank = 1;

	for (auto it = guild_.begin(); it != guild_.end();++it)
	{
		guild * pg = it->second.get();

		if (pg->ladder_point() > point)
			rank++;
	}

	return rank;
}

struct f_guild_compare
{
	auto operator () (guild* g1, guild* g2) const -> bool
	{
		if (g1->ladder_point() < g2->ladder_point())
			return true;
		if (g1->ladder_point() > g2->ladder_point())
			return false;
		if (g1->guild_war_win_count() < g2->guild_war_win_count())
			return true;
		if (g1->guild_war_win_count() > g2->guild_war_win_count())
			return false;
		if (g1->guild_war_loss_count() < g2->guild_war_loss_count())
			return true;
		if (g1->guild_war_loss_count() > g2->guild_war_loss_count())
			return false;
		int c = strcmp(g1->name(), g2->name());
		if (c>0) 
			return true;
		return false;
	}
};

void guild_manager::high_rank_string(uint32_t my_guild, char * buffer, size_t buflen)
{
	std::vector<guild*> v;

	for (auto it = guild_.begin(); it != guild_.end(); ++it)
	{
		if (it->second)
			v.push_back(it->second.get());
	}

	std::sort(v.begin(), v.end(), f_guild_compare());
	int n = v.size();
	int len = 0, len2;
	*buffer = '\0';

	for (int i = 0; i < 8; ++i)
	{
		if (n - i - 1 < 0)
			break;

		guild * g = v[n - i - 1];

		if (!g)
			continue;

		if (g->ladder_point() <= 0)
			break;

		if (my_guild == g->id())
		{
			len2 = snprintf(buffer + len, buflen - len, "[COLOR r;255|g;255|b;0]");

			if (len2 < 0 || len2 >= (int) buflen - len)
				len += (buflen - len) - 1;
			else
				len += len2;
		}

		if (i)
		{
			len2 = snprintf(buffer + len, buflen - len, "[ENTER]");

			if (len2 < 0 || len2 >= (int) buflen - len)
				len += (buflen - len) - 1;
			else
				len += len2;
		}

		len2 = snprintf(buffer + len, buflen - len, "%3d | %-12s | %-8d | %4d | %4d | %4d", 
				rank(g),
				g->name(),
				g->ladder_point(),
				g->guild_war_win_count(),
				g->guild_war_draw_count(),
				g->guild_war_loss_count());

		if (len2 < 0 || len2 >= (int) buflen - len)
			len += (buflen - len) - 1;
		else
			len += len2;

		if (g->id() == my_guild)
		{
			len2 = snprintf(buffer + len, buflen - len, "[/COLOR]");

			if (len2 < 0 || len2 >= (int) buflen - len)
				len += (buflen - len) - 1;
			else
				len += len2;
		}
	}
}

void guild_manager::around_rank_string(uint32_t my_guild, char * buffer, size_t buflen)
{
	std::vector<guild*> v;

	for (auto it = guild_.begin(); it != guild_.end(); ++it)
	{
		if (it->second)
			v.push_back(it->second.get());
	}

	std::sort(v.begin(), v.end(), f_guild_compare());
	int n = v.size();
	int idx;

	for (idx = 0; idx < (int) v.size(); ++idx)
		if (v[idx]->id() == my_guild)
			break;

	int len = 0, len2;
	int count = 0;
	*buffer = '\0';

	for (int i = -3; i <= 3; ++i)
	{
		if (idx - i < 0)
			continue;

		if (idx - i >= n)
			continue;

		guild * g = v[idx - i];

		if (!g)
			continue;

		if (my_guild == g->id())
		{
			len2 = snprintf(buffer + len, buflen - len, "[COLOR r;255|g;255|b;0]");

			if (len2 < 0 || len2 >= (int) buflen - len)
				len += (buflen - len) - 1;
			else
				len += len2;
		}

		if (count)
		{
			len2 = snprintf(buffer + len, buflen - len, "[ENTER]");

			if (len2 < 0 || len2 >= (int) buflen - len)
				len += (buflen - len) - 1;
			else
				len += len2;
		}

		len2 = snprintf(buffer + len, buflen - len, "%3d | %-12s | %-8d | %4d | %4d | %4d", 
				rank(g),
				g->name(),
				g->ladder_point(),
				g->guild_war_win_count(),
				g->guild_war_draw_count(),
				g->guild_war_loss_count());

		if (len2 < 0 || len2 >= (int) buflen - len)
			len += (buflen - len) - 1;
		else
			len += len2;

		++count;

		if (g->id() == my_guild)
		{
			len2 = snprintf(buffer + len, buflen - len, "[/COLOR]");

			if (len2 < 0 || len2 >= (int) buflen - len)
				len += (buflen - len) - 1;
			else
				len += len2;
		}
	}
}

void guild_manager::request_cancel_war(uint32_t guild_id1, uint32_t guild_id2)
{
	metin2::log::info( "RequestCancelWar {:d} {:d}", guild_id1, guild_id2);

	packet_guild_war p;
	p.war = GUILD_WAR_CANCEL;
	p.guild_from = guild_id1;
	p.guild_to = guild_id2;
	db_clientdesc->db_packet(HEADER_GD_GUILD_WAR, 0, &p, sizeof(p));
}

void guild_manager::request_end_war(uint32_t guild_id1, uint32_t guild_id2)
{
	metin2::log::info( "RequestEndWar {:d} {:d}", guild_id1, guild_id2);

	packet_guild_war p;
	p.war = GUILD_WAR_END;
	p.guild_from = guild_id1;
	p.guild_to = guild_id2;
	db_clientdesc->db_packet(HEADER_GD_GUILD_WAR, 0, &p, sizeof(p));
}

void guild_manager::request_war_over(uint32_t guild1, uint32_t guild2, uint32_t guild_winner, int32_t reward)
{
	guild * g1 = touch_guild(guild1);
	guild * g2 = touch_guild(guild2);

	if (g1->get_guild_war_state(g2->id()) != GUILD_WAR_ON_WAR)
	{
		metin2::log::info( "RequestWarOver : both guild was not in war {:d} {:d}", guild1, guild2);
		request_end_war(guild1, guild2);
		return;
	}

	packet_guild_war p;

	p.war = GUILD_WAR_OVER;
	p.war_price = 0;
	p.type = guild_winner == 0 ? 1 : 0;

	if (guild_winner == 0)
	{
		p.guild_from = guild1;
		p.guild_to = guild2;
	}
	else
	{
		p.guild_from = guild_winner;
		p.guild_to = guild_winner == guild1 ? guild2 : guild1;
	}

	db_clientdesc->db_packet(HEADER_GD_GUILD_WAR, 0, &p, sizeof(p));
	metin2::log::info( "RequestWarOver : winner {:d} loser {:d} draw {:d} betprice {:d}", p.guild_from, p.guild_to, p.type, p.war_price);
}

void guild_manager::declare_war(uint32_t guild_id1, uint32_t guild_id2, uint8_t type)
{
	if (guild_id1 == guild_id2)
		return;

	guild * g1 = find_guild(guild_id1);
	guild * g2 = find_guild(guild_id2);

	if (!g1 || !g2)
		return;

	if (g1->declare_war(guild_id2, type, GUILD_WAR_SEND_DECLARE) &&
		g2->declare_war(guild_id1, type, GUILD_WAR_RECV_DECLARE))
	{
		char buf[256];
		snprintf(buf, sizeof(buf), lc_text("%s 길드가 %s 길드에 선전포고를 하였습니다!"), touch_guild(guild_id1)->name(), touch_guild(guild_id2)->name());
		send_notice(buf);
	}
}

void guild_manager::refuse_war(uint32_t guild_id1, uint32_t guild_id2)
{
	guild * g1 = find_guild(guild_id1);
	guild * g2 = find_guild(guild_id2);

	if (g1 && g2)
	{
		if (g2->master_character())
			g2->master_character()->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> %s 길드가 길드전을 거부하였습니다."), g1->name());
	}

	if ( g1 != nullptr )
		g1->refuse_war(guild_id2);

	if ( g2 != nullptr && g1 != nullptr )
		g2->refuse_war(g1->id());
}

void guild_manager::wait_start_war(uint32_t guild_id1, uint32_t guild_id2)
{
	guild * g1 = find_guild(guild_id1);
	guild * g2 = find_guild(guild_id2);

	if (!g1 || !g2)
	{
		metin2::log::info( "GuildWar: CGuildManager::WaitStartWar({:d},{:d}) - something wrong in arg. there is no guild like that.", guild_id1, guild_id2);
		return;
	}

	if (g1->wait_start_war(guild_id2) || g2->wait_start_war(guild_id1) )
	{
		char buf[256];
		snprintf(buf, sizeof(buf), lc_text("%s 길드와 %s 길드가 잠시 후 전쟁을 시작합니다!"), g1->name(), g2->name());
		send_notice(buf);
	}
}

struct f_send_war_list
{
	f_send_war_list(uint8_t subheader, uint32_t guild_id1, uint32_t guild_id2)
	{
		gid1 = guild_id1;
		gid2 = guild_id2;

		p.header	= HEADER_GC_GUILD;
		p.size		= sizeof(p) + sizeof(uint32_t) * 2;
		p.subheader	= subheader;
	}

	void operator() (character * ch)
	{
		desc * d = ch->get_desc();

		if (d)
		{
			d->buffered_packet(&p, sizeof(p));
			d->buffered_packet(&gid1, sizeof(uint32_t));
			d->packet(&gid2, sizeof(uint32_t));
		}
	}

	uint32_t gid1, gid2;
	packet_gc_guild p;
};

void guild_manager::start_war(uint32_t guild_id1, uint32_t guild_id2)
{
	guild * g1 = find_guild(guild_id1);
	guild * g2 = find_guild(guild_id2);

	if (!g1 || !g2)
		return;

	if (!g1->check_start_war(guild_id2) || !g2->check_start_war(guild_id1))
		return;

	g1->start_war(guild_id2);
	g2->start_war(guild_id1);

	char buf[256];
	snprintf(buf, sizeof(buf), lc_text("%s 길드와 %s 길드가 전쟁을 시작하였습니다!"), g1->name(), g2->name());
	send_notice(buf);

	if (guild_id1 > guild_id2)
		std::swap(guild_id1, guild_id2);

	character_manager::instance().for_each_pc(f_send_war_list(GUILD_SUBHEADER_GC_GUILD_WAR_LIST, guild_id1, guild_id2));
	guild_war_.insert(std::make_pair(guild_id1, guild_id2));
}

void send_guild_war_over_notice(guild* g1, guild* g2, bool draw)
{
	if (g1 && g2)
	{
		char buf[256];

		if (draw)
		{
			snprintf(buf, sizeof(buf), lc_text("%s 길드와 %s 길드 사이의 전쟁이 무승부로 끝났습니다."), g1->name(), g2->name());
		}
		else
		{
			if ( g1->war_score_against_to( g2->id() ) > g2->war_score_against_to( g1->id() ) )
			{
				snprintf(buf, sizeof(buf), lc_text("%s 길드가 %s 길드와의 전쟁에서 승리 했습니다."), g1->name(), g2->name());
			}
			else
			{
				snprintf(buf, sizeof(buf), lc_text("%s 길드가 %s 길드와의 전쟁에서 승리 했습니다."), g2->name(), g1->name());
			}
		}

		send_notice(buf);
	}
}

auto guild_manager::end_war(uint32_t guild_id1, uint32_t guild_id2) -> bool
{
	if (guild_id1 > guild_id2)
		std::swap(guild_id1, guild_id2);

	guild * g1 = find_guild(guild_id1);
	guild * g2 = find_guild(guild_id2);

	std::pair<uint32_t, uint32_t> k = std::make_pair(guild_id1, guild_id2);

	guild_war_container::iterator it = guild_war_.find(k);

	if (guild_war_.end() == it)
	{
		metin2::log::info( "EndWar({:d},{:d}) - EndWar request but guild is not in list", guild_id1, guild_id2);
		return false;
	}

	if ( g1 && g2 )
	{
	    if (g1->get_guild_war_type(guild_id2) == GUILD_WAR_TYPE_FIELD)
		{
			send_guild_war_over_notice(g1, g2, g1->war_score_against_to(guild_id2) == g2->war_score_against_to(guild_id1));
		}
	}
	else
	{
	    return false;
	}

	if (g1)
		g1->end_war(guild_id2);

	if (g2)
		g2->end_war(guild_id1);

	guild_war_end_time_[k] = get_global_time();
	character_manager::instance().for_each_pc(f_send_war_list(GUILD_SUBHEADER_GC_GUILD_WAR_END_LIST, guild_id1, guild_id2));
	guild_war_.erase(it);

	return true;
}

void guild_manager::war_over(uint32_t guild_id1, uint32_t guild_id2, bool draw)
{
	guild * g1 = find_guild(guild_id1);
	guild * g2 = find_guild(guild_id2);

	if (guild_id1 > guild_id2)
		std::swap(guild_id1, guild_id2);

	std::pair<uint32_t, uint32_t> k = std::make_pair(guild_id1, guild_id2);

	guild_war_container::iterator it = guild_war_.find(k);

	if (guild_war_.end() == it)
		return;

	send_guild_war_over_notice(g1, g2, draw);

	end_war(guild_id1, guild_id2);
}

void guild_manager::cancel_war(uint32_t guild_id1, uint32_t guild_id2)
{
	if (!end_war(guild_id1, guild_id2))
		return;

	guild * g1 = find_guild(guild_id1);
	guild * g2 = find_guild(guild_id2);

	if (g1)
	{
		character * master1 = g1->master_character();

		if (master1)
			master1->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드전이 취소 되었습니다."));
	}

	if (g2)
	{
		character * master2 = g2->master_character();

		if (master2)
			master2->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드전이 취소 되었습니다."));
	}

	if (g1 && g2)
	{
		char buf[256+1];
		snprintf(buf, sizeof(buf), lc_text("%s 길드와 %s 길드 사이의 전쟁이 취소되었습니다."), g1->name(), g2->name());
		send_notice(buf);
	}
}

void guild_manager::reserve_war(uint32_t guild1, uint32_t guild2, uint8_t type)
{
	metin2::log::info( "GuildManager::ReserveWar {:d} {:d}", guild1, guild2);

	guild * g1 = find_guild(guild1);
	guild * g2 = find_guild(guild2);

	if (!g1 || !g2)
		return;

	g1->reserve_war(guild2, type);
	g2->reserve_war(guild1, type);
}

void guild_manager::show_guild_war_list(character * ch)
{
	for (auto it = guild_war_.begin(); it != guild_war_.end(); ++it)
	{
		guild * A = touch_guild(it->first);
		guild * B = touch_guild(it->second);

		if (A && B)
		{
			ch->chat_packet(CHAT_TYPE_NOTICE, "%s[%d] vs %s[%d] time %u sec.",
					A->name(), A->id(),
					B->name(), B->id(),
					get_global_time() - A->war_start_time(B->id()));
		}
	}
}

void guild_manager::send_guild_war(character * ch)
{
	if (!ch->get_desc())
		return;

	temp_buffer buf;
	packet_gc_guild p;
	p.header= HEADER_GC_GUILD;
	p.subheader = GUILD_SUBHEADER_GC_GUILD_WAR_LIST;
	p.size = sizeof(p) + (sizeof(uint32_t) * 2) * guild_war_.size();
	buf.write(&p, sizeof(p));

	decltype(guild_war_.begin()) it;

	for (it = guild_war_.begin(); it != guild_war_.end(); ++it)
	{
		buf.write(&it->first, sizeof(uint32_t));
		buf.write(&it->second, sizeof(uint32_t));
	}

	ch->get_desc()->packet(buf.read_peek(), buf.size());
}

void send_guild_war_score(uint32_t guild, uint32_t guild_opp, int delta, int bet_score_delta)
{
	packet_guild_war_score p;

	p.guild_gain_point = guild;
	p.guild_opponent = guild_opp;
	p.score = delta;
	p.bet_score = bet_score_delta;

	db_clientdesc->db_packet(HEADER_GD_GUILD_WAR_SCORE, 0, &p, sizeof(packet_guild_war_score));
	metin2::log::info( "SendGuildWarScore {:d} {:d} {:d}", guild, guild_opp, delta);
}

void guild_manager::kill(character * killer, character * victim)
{
	if (!killer->is_pc())
		return;

	if (!victim->is_pc())
		return;

	if (killer->get_war_map())
	{
		killer->get_war_map()->on_kill(killer, victim);
		return;
	}

	guild * gAttack = killer->get_guild();
	guild * gDefend = victim->get_guild();

	if (!gAttack || !gDefend)
		return;

	if (gAttack->get_guild_war_type(gDefend->id()) != GUILD_WAR_TYPE_FIELD)
		return;

	if (!gAttack->under_war(gDefend->id()))
		return;

	send_guild_war_score(gAttack->id(), gDefend->id(), victim->level());
}

void guild_manager::stop_all_guild_war()
{
	for (auto it = guild_war_.begin(); it != guild_war_.end(); ++it)
	{
		guild * g = guild_manager::instance().touch_guild(it->first);
		guild * pg = guild_manager::instance().touch_guild(it->second);
		g->end_war(it->second);
		pg->end_war(it->first);
	}

	guild_war_.clear();
}

void guild_manager::reserve_war_add(guild_war_reserve * p)
{
	auto it = map_k_reserve_war_.find(p->id);

	guild_war_reserve_for_game * pkReserve;

	if (it != map_k_reserve_war_.end())
		pkReserve = it->second.get();
	else
	{
		auto pkNewReserve = std::make_unique<guild_war_reserve_for_game>();
		pkReserve = pkNewReserve.get();

		map_k_reserve_war_.insert(std::make_pair(p->id, std::move(pkNewReserve)));
		vec_k_reserve_war_.push_back(pkReserve);
	}

	std::memcpy(&pkReserve->data, p, sizeof(guild_war_reserve));

	metin2::log::info( "ReserveWarAdd {:d} gid1 {:d} power {:d} gid2 {:d} power {:d} handicap {:d}",
			pkReserve->data.id, p->guild_from, p->power_from, p->guild_to, p->power_to, p->handicap);
}

void guild_manager::reserve_war_bet(packet_gd_guild_war_bet * p)
{
	auto it = map_k_reserve_war_.find(p->war_id);

	if (it == map_k_reserve_war_.end())
		return;

	it->second->bet.insert(std::make_pair(p->login, std::make_pair(p->guild, p->gold)));
}

auto guild_manager::is_bet(uint32_t id, const char * login) -> bool
{
	auto it = map_k_reserve_war_.find(id);

	if (it == map_k_reserve_war_.end())
		return true;

	return it->second->bet.end() != it->second->bet.find(login);
}

void guild_manager::reserve_war_delete(uint32_t id)
{
	metin2::log::info( "ReserveWarDelete {:d}", id);
	auto it = map_k_reserve_war_.find(id);

	if (it == map_k_reserve_war_.end())
		return;

	auto it_vec = vec_k_reserve_war_.begin();

	while (it_vec != vec_k_reserve_war_.end())
	{
		if (*it_vec == it->second.get())
		{
			it_vec = vec_k_reserve_war_.erase(it_vec);
			break;
		}
		else
			++it_vec;
	}

	map_k_reserve_war_.erase(it);
}

auto guild_manager::reserve_war_ref() -> std::vector<guild_war_reserve_for_game *> &
{
	return vec_k_reserve_war_;
}


void guild_manager::change_master(uint32_t gid)
{
	auto iter = guild_.find(gid);

	if ( iter == guild_.end() )
		return;

	iter->second->load(gid);

	db_manager::instance().func_query([guild = iter->second.get()](sql_msg *msg) { guild->send_guild_data_update_to_all_member(msg); },
			"SELECT 1");

}
}
