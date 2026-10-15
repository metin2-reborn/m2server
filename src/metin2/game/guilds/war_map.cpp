#include <metin2/game/guilds/war_map.h>

#include <metin2/game/characters/affect.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/app/event.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/items/item.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <utility>

namespace metin2::game::inline guilds {

EVENTINFO(war_map_event_info)
{
	int step;
	game::guilds::war_map * war_map;

	war_map_event_info() 
	: step( 0 )
	, war_map( nullptr )
	{
	}
};

EVENTFUNC(war_begin_event)
{
	war_map_event_info* info = dynamic_cast<war_map_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "war_begin_event> <Factor> Null pointer" );
		return 0;
	}

	war_map * pMap = info->war_map;
	pMap->check_war_end();
	return passes_per_sec(10);
}

EVENTFUNC(war_end_event)
{
	war_map_event_info* info = dynamic_cast<war_map_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "war_end_event> <Factor> Null pointer" );
		return 0;
	}

	war_map * pMap = info->war_map;

	if (info->step == 0)
	{
		++info->step;
		pMap->exit_all();
		return passes_per_sec(5);
	}
	else
	{
		pMap->set_end_event(nullptr);
		war_map_manager::instance().destroy_war_map(pMap);
		return 0;
	}
}

EVENTFUNC(war_timeout_event)
{
	war_map_event_info* info = dynamic_cast<war_map_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "war_timeout_event> <Factor> Null pointer" );
		return 0;
	}

	war_map * pMap = info->war_map;
	pMap->timeout();
	return 0;
}

void war_map::team_data::initialize()
{
	id = 0;
	guild = nullptr;
	member_count = 0;
	use_potion_price = 0;
	score = 0;
	chr_flag = nullptr;
	chr_flag_base = nullptr;

	set_pid_joiner.clear();
}

war_map::war_map(int32_t lMapIndex, const guild_war_info & r_info, war_map_info * pkWarMapInfo, uint32_t dwGuildID1, uint32_t dwGuildID2)
{
	map_info_ = *pkWarMapInfo;
	map_info_.map_index = lMapIndex;

	memcpy(&war_info_, &r_info, sizeof(guild_war_info));

	team_data_[0].initialize();
	team_data_[0].id = dwGuildID1;
	team_data_[0].guild = guild_manager::instance().touch_guild(dwGuildID1);

	team_data_[1].initialize();
	team_data_[1].id = dwGuildID2;
	team_data_[1].guild = guild_manager::instance().touch_guild(dwGuildID2);
	observer_count_ = 0;

	war_map_event_info* info = alloc_event_info<war_map_event_info>();
	info->war_map = this;

	set_begin_event(event_create(war_begin_event, info, passes_per_sec(60)));
	end_event_ = nullptr;
	timeout_event_ = nullptr;
	reset_flag_event_ = nullptr;
	timeout_ = false;
	start_time_ = get_dword_time();
	ended_ = false;

	if (type() == WAR_MAP_TYPE_FLAG)
	{
		add_flag_base(0);
		add_flag_base(1);
		add_flag(0);
		add_flag(1);
	}
}

war_map::~war_map()
{
	event_cancel(&begin_event_);
	event_cancel(&end_event_);
	event_cancel(&timeout_event_);
	event_cancel(&reset_flag_event_);

	metin2::log::info( "WarMap::~WarMap : map index {:d}", map_index());

	auto it = set_pk_chr_.begin();

	while (it != set_pk_chr_.end())
	{
		character * ch = *(it++);

		if (ch->get_desc())
		{
			metin2::log::info( "WarMap::~WarMap : disconnecting {}", ch->name());
			desc_manager::instance().destroy_desc(ch->get_desc());
		}
	}

	set_pk_chr_.clear();
}

void war_map::set_begin_event(event_ptr pkEv)
{
	if (begin_event_ != nullptr) {
		event_cancel(&begin_event_);
	}
	if (pkEv != nullptr) {
		begin_event_ = pkEv;
	}
}

void war_map::set_end_event(event_ptr pkEv)
{
	if (end_event_ != nullptr) {
		event_cancel(&end_event_);
	}
	if (pkEv != nullptr) {
		end_event_ = pkEv;
	}
}

void war_map::set_timeout_event(event_ptr pkEv)
{
	if (timeout_event_ != nullptr) {
		event_cancel(&timeout_event_);
	}
	if (pkEv != nullptr) {
		timeout_event_ = pkEv;
	}
}

void war_map::set_reset_flag_event(event_ptr pkEv)
{
	if (reset_flag_event_ != nullptr) {
		event_cancel(&reset_flag_event_);
	}
	if (pkEv != nullptr) {
		reset_flag_event_ = pkEv;
	}
}

bool war_map::team_index(uint32_t dwGuildID, uint8_t & bIdx)
{
	if (team_data_[0].id == dwGuildID)
	{
		bIdx = 0;
		return true;
	}
	else if (team_data_[1].id == dwGuildID)
	{
		bIdx = 1;
		return true;
	}

	return false;
}

guild * war_map::get_guild(uint8_t bIdx)
{
	return team_data_[bIdx].guild;
}

int32_t war_map::map_index()
{
	return map_info_.map_index;
}

uint8_t war_map::type()
{
	return map_info_.type;
}

uint32_t war_map::guild_opponent(character * ch)
{
	if (ch->get_guild())
	{
		uint32_t gid = ch->get_guild()->id();
		uint8_t idx;

		if (team_index(gid, idx))
			return team_data_[!idx].id;
	}
	return 0;
}

uint32_t war_map::winner_guild()
{
	uint32_t win_gid = 0;

	if (team_data_[1].score > team_data_[0].score)
	{
		win_gid = team_data_[1].id;
	}
	else if (team_data_[0].score > team_data_[1].score)
	{
		win_gid = team_data_[0].id;
	}

	return (win_gid);
}

void war_map::use_potion(character * ch, item * item)
{
	if (end_event_)
		return;

	if (ch->is_observer_mode())
		return;

	if (!ch->get_guild())
		return;

	if (!item->proto())
		return;

	int iPrice = item->proto()->gold;

	uint32_t gid = ch->get_guild()->id();

	if (gid == team_data_[0].id)
		team_data_[0].use_potion_price += iPrice;
	else if (gid == team_data_[1].id)
		team_data_[1].use_potion_price += iPrice;
}

int war_map::team_data::accumulated_joiner_count()
{
	return set_pid_joiner.size();
}

int war_map::team_data::cur_jointer_count()
{
	return member_count;
}

void war_map::team_data::append_member(character * ch)
{
	set_pid_joiner.insert(ch->player_id());
	++member_count;
}

void war_map::team_data::remove_member(character * ch)
{
	// set_pidJoiner counts cumulative members, so don't remove from it
	--member_count;
}


struct f_send_user_count
{
	char buf1[30];
	char buf2[128];

	f_send_user_count(uint32_t g1, int g1_count, uint32_t g2, int g2_count, int observer)
	{
		snprintf(buf1, sizeof(buf1), "ObserverCount %d", observer);
		snprintf(buf2, sizeof(buf2), "WarUC %u %d %u %d %d", g1, g1_count, g2, g2_count, observer);
	}

	void operator() (character * ch)
	{
		ch->chat_packet(CHAT_TYPE_COMMAND, buf1);
		ch->chat_packet(CHAT_TYPE_COMMAND, buf2);
	}
};

void war_map::update_user_count()
{
	f_send_user_count f(
			team_data_[0].id, 
			team_data_[0].accumulated_joiner_count(), 
			team_data_[1].id, 
			team_data_[1].accumulated_joiner_count(), 
			observer_count_);

	std::for_each(set_pk_chr_.begin(), set_pk_chr_.end(), f);
}

void war_map::inc_member(character * ch)
{
	if (!ch->is_pc())
		return;

	metin2::log::info( "WarMap::IncMember");
	uint32_t gid = 0;

	if (ch->get_guild())
		gid = ch->get_guild()->id();

	bool isWarMember = ch->quest_flag("war.is_war_member") > 0 ? true : false;

	if (isWarMember && gid != team_data_[0].id && gid != team_data_[1].id)
	{
		ch->set_quest_flag("war.is_war_member", 0);
		isWarMember = false;
	}

	if (isWarMember)
	{
		if (gid == team_data_[0].id)
		{
			team_data_[0].append_member(ch);

		}
		else if (gid == team_data_[1].id)
		{
			team_data_[1].append_member(ch);

		}

		event_cancel(&timeout_event_);

		metin2::log::info( "WarMap +m {:d}(cur:{:d}, acc:{:d}) vs {:d}(cur:{:d}, acc:{:d})",
				team_data_[0].id, team_data_[0].cur_jointer_count(), team_data_[0].accumulated_joiner_count(),
				team_data_[1].id, team_data_[1].cur_jointer_count(), team_data_[1].accumulated_joiner_count());
	}
	else
	{
		++observer_count_; 
		metin2::log::info( "WarMap +o {:d}", observer_count_);
		ch->set_observer_mode(true);
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("관전 모드로 길드전에 참가하셨습니다."));
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("자신을 선택하시면 밖으로 나갈 수 있는 <관람 종료> 버튼이 나옵니다."));
	}

	update_user_count();

	set_pk_chr_.insert(ch);

	desc * d = ch->get_desc();

	send_war_packet(d);
	send_score_packet(0, d);
	send_score_packet(1, d);
}

void war_map::dec_member(character * ch)
{
	if (!ch->is_pc())
		return;

	metin2::log::info( "WarMap::DecMember");
	uint32_t gid = 0;

	if (ch->get_guild())
		gid = ch->get_guild()->id();

	if (!ch->is_observer_mode())
	{
		if (gid == team_data_[0].id)
			team_data_[0].remove_member(ch);
		else if (gid == team_data_[1].id)
			team_data_[1].remove_member(ch);

		if (map_info_.type == WAR_MAP_TYPE_FLAG)
		{
			affect * pkAff = ch->find_affect(AFFECT_WAR_FLAG);

			if (pkAff)
			{
				uint8_t idx;

				if (team_index(pkAff->apply_value, idx))
					add_flag(idx, ch->x(), ch->y());

				ch->remove_affect(AFFECT_WAR_FLAG);
			}
		}

		metin2::log::info( "WarMap -m {:d}(cur:{:d}, acc:{:d}) vs {:d}(cur:{:d}, acc:{:d})",
				team_data_[0].id, team_data_[0].cur_jointer_count(), team_data_[0].accumulated_joiner_count(),
				team_data_[1].id, team_data_[1].cur_jointer_count(), team_data_[1].accumulated_joiner_count());

		check_war_end();
		ch->set_quest_flag("war.is_war_member", 0);
	}
	else
	{
		--observer_count_;

		metin2::log::info( "WarMap -o {:d}", observer_count_);
		ch->set_observer_mode(false);
	}

	update_user_count();

	set_pk_chr_.erase(ch);
}

struct f_exit_guild_war
{
	void operator() (character * ch)
	{
		if (ch->is_pc())
		{
			ch->exit_to_saved_location();
		}
	}
};

void war_map::exit_all()
{
	f_exit_guild_war f;
	std::for_each(set_pk_chr_.begin(), set_pk_chr_.end(), f);
}

void war_map::check_war_end()
{
	if (ended_)
		return;

	if (team_data_[0].member_count == 0 || team_data_[1].member_count == 0)
	{
		if (timeout_)
			return;

		if (timeout_event_)
			return;

		notice(lc_text("길드전에 참가한 상대방 길드원이 아무도 없습니다."));
		notice(lc_text("1분 이내에 아무도 접속하지 않으면 길드전이 자동 종료됩니다."));

		metin2::log::info( "CheckWarEnd: Timeout begin {:d} vs {:d}", team_data_[0].id, team_data_[1].id);

		war_map_event_info* info = alloc_event_info<war_map_event_info>();
		info->war_map = this;

		set_timeout_event(event_create(war_timeout_event, info, passes_per_sec(60)));
	}
	else
		check_score();
}

int war_map::reward_gold(uint8_t bWinnerIdx)
{
	int iRewardGold = war_info_.war_price;
	iRewardGold += (team_data_[bWinnerIdx].use_potion_price * war_info_.winner_potion_reward_pct_to_winner) / 100;
	iRewardGold += (team_data_[bWinnerIdx ? 0 : 1].use_potion_price * war_info_.loser_potion_reward_pct_to_winner) / 100;
	return iRewardGold;
}

void war_map::draw()
{
	guild_manager::instance().request_war_over(team_data_[0].id, team_data_[1].id, 0, 0);
}

void war_map::timeout()
{
	set_timeout_event(nullptr);

	if (timeout_)
		return;

	if (ended_)
		return;

	uint32_t dwWinner = 0;
	uint32_t dwLoser = 0;
	int iRewardGold = 0;

	if (get_dword_time() - start_time_ < 60000 * 5)
	{
		notice(lc_text("길드전이 일찍 종료되어 무승부로 판정 되었습니다. (5분이 지나지 않음)"));
		dwWinner = 0;
		dwLoser = 0;
	}
	else
	{
		int iWinnerIdx = -1;

		if (team_data_[0].member_count == 0)
			iWinnerIdx = 1;
		else if (team_data_[1].member_count == 0)
			iWinnerIdx = 0;

		if (iWinnerIdx == -1)
		{
			dwWinner = winner_guild();

			if (dwWinner == team_data_[0].id)
			{
				iRewardGold = reward_gold(0);
				dwLoser = team_data_[1].id;
			}
			else if (dwWinner == team_data_[1].id)
			{
				iRewardGold = reward_gold(1);
				dwLoser = team_data_[0].id;
			}

			metin2::log::error("WarMap: member count is not zero, guild1 {:d} {:d} guild2 {:d} {:d}, winner {:d}",
					team_data_[0].id, team_data_[0].member_count,
					team_data_[1].id, team_data_[1].member_count,
					dwWinner);
		}
		else
		{
			dwWinner = team_data_[iWinnerIdx].id;
			dwLoser = team_data_[iWinnerIdx == 0 ? 1 : 0].id;

			iRewardGold = reward_gold(iWinnerIdx);
		}
	}

	metin2::log::info( "WarMap: Timeout {:d} {:d} winner {:d} loser {:d} reward {:d} map {:d}",
			team_data_[0].id, team_data_[1].id, dwWinner, dwLoser, iRewardGold, map_info_.map_index);

	if (dwWinner)
		guild_manager::instance().request_war_over(dwWinner, dwLoser, dwWinner, iRewardGold);
	else
		guild_manager::instance().request_war_over(team_data_[0].id, team_data_[1].id, dwWinner, iRewardGold);

	timeout_ = true;
}

namespace
{
	struct f_packet
	{
		f_packet(const void * p, int size_value) : pv_data(p), size(size_value)
		{
		}

		void operator () (character * ch)
		{
			ch->get_desc()->packet(pv_data, size);
		}

		const void * pv_data;
		int size;
	};

	struct f_notice
	{
		f_notice(const char * psz_value) : psz(psz_value)
		{
		}

		void operator() (character * ch)
		{
			ch->chat_packet(CHAT_TYPE_NOTICE, "%s", psz);
		}

		const char * psz;
	};
};

void war_map::notice(const char * psz)
{
	f_notice f(psz);
	std::for_each(set_pk_chr_.begin(), set_pk_chr_.end(), f);
}

void war_map::packet(const void * p, int size)
{
	f_packet f(p, size);
	std::for_each(set_pk_chr_.begin(), set_pk_chr_.end(), f);
}

void war_map::send_war_packet(desc * d)
{
	packet_gc_guild pack;
	packet_gc_guild_war pack2;

	pack.header		= HEADER_GC_GUILD;
	pack.subheader	= GUILD_SUBHEADER_GC_WAR;
	pack.size		= sizeof(pack) + sizeof(pack2);

	pack2.guild_self	= team_data_[0].id;
	pack2.guild_opp	= team_data_[1].id;
	pack2.type		= guild_manager::instance().touch_guild(team_data_[0].id)->get_guild_war_type(team_data_[1].id);
	pack2.war_state	= guild_manager::instance().touch_guild(team_data_[0].id)->get_guild_war_state(team_data_[1].id);

	d->buffered_packet(&pack, sizeof(pack));
	d->packet(&pack2, sizeof(pack2));
}

void war_map::send_score_packet(uint8_t bIdx, desc * d)
{
	packet_gc_guild p;

	p.header = HEADER_GC_GUILD;
	p.subheader = GUILD_SUBHEADER_GC_WAR_SCORE;
	p.size = sizeof(p) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(int32_t);

	temp_buffer buf;
	buf.write(&p, sizeof(p));
	buf.write(&team_data_[bIdx].id, sizeof(uint32_t));
	buf.write(&team_data_[bIdx ? 0 : 1].id, sizeof(uint32_t));
	buf.write(&team_data_[bIdx].score, sizeof(int32_t));

	if (d)
		d->packet(buf.read_peek(), buf.size());
	else
		packet(buf.read_peek(), buf.size());
}

void war_map::update_score(uint32_t g1, int score1, uint32_t g2, int score2)
{
	uint8_t idx;

	if (team_index(g1, idx))
	{
		if (team_data_[idx].score != score1)
		{
			team_data_[idx].score = score1;
			send_score_packet(idx);
		}
	}

	if (team_index(g2, idx))
	{
		if (team_data_[idx].score != score2)
		{
			team_data_[idx].score = score2;
			send_score_packet(idx);
		}
	}

	check_score();
}

bool war_map::check_score()
{
	if (ended_)
		return true;

	// Check only after 30 seconds.
	if (get_dword_time() - start_time_ < 30000)
		return false;

	// Don't check if the scores are equal.
	if (team_data_[0].score == team_data_[1].score)
		return false;

	int iEndScore = war_info_.end_score;

	if (metin2::config::test_server()) iEndScore /= 10;

	uint32_t dwWinner;
	uint32_t dwLoser;

	if (team_data_[0].score >= iEndScore)
	{
		dwWinner = team_data_[0].id;
		dwLoser = team_data_[1].id;
	}
	else if (team_data_[1].score >= iEndScore)
	{
		dwWinner = team_data_[1].id;
		dwLoser = team_data_[0].id;
	}
	else
		return false;

	int iRewardGold = 0;

	if (dwWinner == team_data_[0].id)
		iRewardGold = reward_gold(0);
	else if (dwWinner == team_data_[1].id)
		iRewardGold = reward_gold(1);

	metin2::log::info( "WarMap::CheckScore end score {:d} guild1 {:d} score guild2 {:d} {:d} score {:d} winner {:d} reward {:d}", 
			iEndScore,
			team_data_[0].id,
			team_data_[0].score,
			team_data_[1].id,
			team_data_[1].score,
			dwWinner,
			iRewardGold);

	guild_manager::instance().request_war_over(dwWinner, dwLoser, dwWinner, iRewardGold);
	return true;
}

bool war_map::set_ended()
{
	metin2::log::info( "WarMap::SetEnded {:d}", map_info_.map_index);

	if (end_event_)
		return false;

	if (team_data_[0].chr_flag)
	{
		character_manager::instance().destroy_character(team_data_[0].chr_flag);
		team_data_[0].chr_flag = nullptr;
	}

	if (team_data_[0].chr_flag_base)
	{
		character_manager::instance().destroy_character(team_data_[0].chr_flag_base);
		team_data_[0].chr_flag_base = nullptr;
	}

	if (team_data_[1].chr_flag)
	{
		character_manager::instance().destroy_character(team_data_[1].chr_flag);
		team_data_[1].chr_flag = nullptr;
	}

	if (team_data_[1].chr_flag_base)
	{
		character_manager::instance().destroy_character(team_data_[1].chr_flag_base);
		team_data_[1].chr_flag_base = nullptr;
	}

	event_cancel(&reset_flag_event_);
	ended_ = true;

	war_map_event_info* info = alloc_event_info<war_map_event_info>();

	info->war_map = this;
	info->step = 0;
	set_end_event(event_create(war_end_event, info, passes_per_sec(10)));
	return true;
}

void war_map::on_kill(character * killer, character * ch)
{
	if (ended_)
		return;

	uint32_t dwKillerGuild = 0;
	uint32_t dwDeadGuild = 0;

	if (killer->get_guild())
		dwKillerGuild = killer->get_guild()->id();

	if (ch->get_guild())
		dwDeadGuild = ch->get_guild()->id();

	uint8_t idx;

	metin2::log::info( "WarMap::OnKill {:d} {:d}", dwKillerGuild, dwDeadGuild);

	if (!team_index(dwKillerGuild, idx))
		return;

	if (!team_index(dwDeadGuild, idx))
		return;

	switch (map_info_.type)
	{
		case WAR_MAP_TYPE_NORMAL:
			send_guild_war_score(dwKillerGuild, dwDeadGuild, 1, ch->level());
			break;

		case WAR_MAP_TYPE_FLAG:
			{
				affect * pkAff = ch->find_affect(AFFECT_WAR_FLAG);

				if (pkAff)
				{
					if (team_index(pkAff->apply_value, idx))
						add_flag(idx, ch->x(), ch->y());

					ch->remove_affect(AFFECT_WAR_FLAG);
				}
			}
			break;

		default:
			metin2::log::error("unknown war map type {:d} index {:d}", map_info_.type, map_info_.map_index);
			break;
	}
}

void war_map::add_flag_base(uint8_t bIdx, uint32_t x, uint32_t y)
{
	if (ended_)
		return;

	assert(bIdx < 2);

	team_data & r = team_data_[bIdx];

	if (r.chr_flag_base)
		return;

	if (x == 0)
	{
		x = map_info_.pos_start[bIdx].x;
		y = map_info_.pos_start[bIdx].y;
	}

	r.chr_flag_base = character_manager::instance().spawn_mob(warmap::WAR_FLAG_BASE_VNUM, map_info_.map_index, x, y, 0);
	metin2::log::info( "WarMap::AddFlagBase {:d} {} id {:d}", bIdx, static_cast<const void *>(r.chr_flag_base), r.id);

	r.chr_flag_base->set_point(POINT_STAT, r.id);
	r.chr_flag_base->set_war_map(this);
}

void war_map::add_flag(uint8_t bIdx, uint32_t x, uint32_t y)
{
	if (ended_)
		return;

	assert(bIdx < 2);

	team_data & r = team_data_[bIdx];

	if (r.chr_flag)
		return;

	if (x == 0)
	{
		x = map_info_.pos_start[bIdx].x;
		y = map_info_.pos_start[bIdx].y;
	}

	r.chr_flag = character_manager::instance().spawn_mob(bIdx == 0 ? warmap::WAR_FLAG_VNUM0 : warmap::WAR_FLAG_VNUM1, map_info_.map_index, x, y, 0);
	metin2::log::info( "WarMap::AddFlag {:d} {} id {:d}", bIdx, static_cast<const void *>(r.chr_flag), r.id);

	r.chr_flag->set_point(POINT_STAT, r.id);
	r.chr_flag->set_war_map(this);
}

void war_map::remove_flag(uint8_t bIdx)
{
	assert(bIdx < 2);

	team_data & r = team_data_[bIdx];

	if (!r.chr_flag)
		return;

	metin2::log::info( "WarMap::RemoveFlag {:d} {}", bIdx, static_cast<const void *>(r.chr_flag));

	r.chr_flag->dead(nullptr, true);
	r.chr_flag = nullptr;
}

bool war_map::is_flag_on_base(uint8_t bIdx)
{
	assert(bIdx < 2);

	team_data & r = team_data_[bIdx];

	if (!r.chr_flag)
		return false;

	const pixel_position & pos = r.chr_flag->xyz();

	if (pos.x == map_info_.pos_start[bIdx].x && pos.y == map_info_.pos_start[bIdx].y)
		return true;

	return false;
}

EVENTFUNC(war_reset_flag_event)
{
	war_map_event_info* info = dynamic_cast<war_map_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "war_reset_flag_event> <Factor> Null pointer" );
		return 0;
	}

	war_map * pMap = info->war_map;

	pMap->add_flag(0);
	pMap->add_flag(1);

	pMap->set_reset_flag_event(nullptr);
	return 0;
}

struct f_remove_flag_affect
{
	void operator() (character * ch)
	{
		if (ch->find_affect(AFFECT_WAR_FLAG))
			ch->remove_affect(AFFECT_WAR_FLAG);
	}
};

void war_map::reset_flag()
{
	if (map_info_.type != WAR_MAP_TYPE_FLAG)
		return;

	if (reset_flag_event_)
		return;

	if (ended_)
		return;

	f_remove_flag_affect f;
	std::for_each(set_pk_chr_.begin(), set_pk_chr_.end(), f);

	remove_flag(0);
	remove_flag(1);

	war_map_event_info* info = alloc_event_info<war_map_event_info>();

	info->war_map = this;
	info->step = 0;
	set_reset_flag_event(event_create(war_reset_flag_event, info, passes_per_sec(10)));
}

/////////////////////////////////////////////////////////////////////////////////
// WarMapManager
/////////////////////////////////////////////////////////////////////////////////
war_map_manager::war_map_manager()
{
}

war_map_manager::~war_map_manager()
{
}

bool war_map_manager::load_war_map_info(const char * c_pszFileName)
{
	auto k = std::make_unique<war_map_info>();
	k->type = WAR_MAP_TYPE_NORMAL;

	k->map_index = 110;
	k->pos_start[0].x = 48 * 100 + 32000;
	k->pos_start[0].y = 52 * 100 + 0;
	k->pos_start[1].x = 183 * 100 + 32000;
	k->pos_start[1].y = 206 * 100 + 0;
	k->pos_start[2].x = 141 * 100 + 32000;
	k->pos_start[2].y = 117 * 100 + 0;

	map_k_war_map_info_.emplace(k->map_index, std::move(k));

	k = std::make_unique<war_map_info>();
	k->type = WAR_MAP_TYPE_FLAG;

	k->map_index = 111;
	k->pos_start[0].x = 68 * 100 + 57600;
	k->pos_start[0].y = 69 * 100 + 0;
	k->pos_start[1].x = 171 * 100 + 57600;
	k->pos_start[1].y = 182 * 100 + 0; 
	k->pos_start[2].x = 122 * 100 + 57600;
	k->pos_start[2].y = 131 * 100 + 0;

	map_k_war_map_info_.emplace(k->map_index, std::move(k));
	return true;
}

bool war_map_manager::is_war_map(int32_t lMapIndex)
{
	return get_war_map_info(lMapIndex) ? true : false;
}

bool war_map_manager::start_position(int32_t lMapIndex, uint8_t bIdx, pixel_position & pos)
{
	assert(bIdx < 3);

	war_map_info * pi = get_war_map_info(lMapIndex);

	if (!pi)
	{
		metin2::log::info( "GetStartPosition FAILED [{:d}] WarMapInfoSize({:d})", lMapIndex, map_k_war_map_info_.size());

		decltype(map_k_war_map_info_.begin()) it;
		for (it	= map_k_war_map_info_.begin(); it != map_k_war_map_info_.end(); ++it)
		{
			pixel_position& cur=it->second->pos_start[bIdx];
			metin2::log::info( "WarMap[{:d}]=Pos({:d}, {:d})", it->first, cur.x, cur.y);
		}
		return false;
	}

	pos = pi->pos_start[bIdx];
	return true;
}

int32_t war_map_manager::create_war_map(const guild_war_info& guildWarInfo, uint32_t dwGuildID1, uint32_t dwGuildID2)
{
	war_map_info * pkInfo = get_war_map_info(guildWarInfo.map_index);
	if (!pkInfo)
	{
		metin2::log::error("GuildWar.CreateWarMap.NOT_FOUND_MAPINFO[{:d}]", guildWarInfo.map_index);
		return 0;
	}

	uint32_t lMapIndex = sectree_manager::instance().create_private_map(guildWarInfo.map_index);

	if (lMapIndex)
	{
		war_map_.insert(std::make_pair(lMapIndex, new war_map(lMapIndex, guildWarInfo, pkInfo, dwGuildID1, dwGuildID2)));
	}

	return lMapIndex;
}

war_map_info * war_map_manager::get_war_map_info(int32_t lMapIndex)
{
	if (lMapIndex >= 10000)
		lMapIndex /= 10000;

	auto it = map_k_war_map_info_.find(lMapIndex);

	if (map_k_war_map_info_.end() == it)
		return nullptr;

	return it->second.get();
}

void war_map_manager::destroy_war_map(war_map* pMap)
{
	int32_t mapIdx = pMap->map_index();

	metin2::log::info( "WarMap::DestroyWarMap : {:d}", mapIdx);

	war_map_.erase(pMap->map_index());
	delete pMap;

	sectree_manager::instance().destroy_private_map(mapIdx);
}

war_map * war_map_manager::find(int32_t lMapIndex)
{
	auto it = war_map_.find(lMapIndex);

	if (it == war_map_.end())
		return nullptr;

	return it->second;
}

void war_map_manager::on_shutdown()
{
	auto it = war_map_.begin();

	while (it != war_map_.end())
		(it++)->second->draw();
}
}
