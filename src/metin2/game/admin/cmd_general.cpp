#include <metin2/config/config.h>
#include <metin2/game/characters/affect.h>
#include <metin2/game/events/arena.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/items/cube.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/world/dungeon.h>
#include <metin2/game/app/event.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/persistence/log.h>
#include <metin2/game/social/messenger_manager.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/empire/monarch.h>
#include <metin2/game/characters/motion.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/combat/pvp.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/empire/start_position.h>
#include <metin2/game/empire/threeway_war.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/guilds/war_map.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/common/utils.h>
#include <metin2/log/log.h>
#include <metin2/core/main.h>
#include <metin2/core/utils.h>

#include <openssl/evp.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

namespace metin2::game::inline admin {

ACMD(do_user_horse_ride)
{
	if (ch->is_observer_mode())
		return;

	if (ch->is_dead() || ch->is_stun())
		return;

	if (ch->is_horse_riding() == false)
	{
		if (ch->get_mount_vnum())
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("이미 탈것을 이용중입니다."));
			return;
		}

		if (ch->horse() == nullptr)
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("말을 먼저 소환해주세요."));
			return;
		}

		ch->start_riding();
	}
	else
	{
		ch->stop_riding();
	}
}

ACMD(do_user_horse_back)
{
	if (ch->horse() != nullptr)
	{
		ch->horse_summon(false);
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("말을 돌려보냈습니다."));
	}
	else if (ch->is_horse_riding() == true)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("말에서 먼저 내려야 합니다."));
	}
	else
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("말을 먼저 소환해주세요."));
	}
}

ACMD(do_user_horse_feed)
{
	if (ch->my_shop())
		return;

	if (ch->horse() == nullptr)
	{
		if (ch->is_horse_riding() == false)
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("말을 먼저 소환해주세요."));
		else
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("말을 탄 상태에서는 먹이를 줄 수 없습니다."));
		return;
	}

	uint32_t dwFood = ch->horse_grade() + 50054 - 1;

	if (ch->count_specify_item(dwFood) > 0)
	{
		ch->remove_specify_item(dwFood, 1);
		ch->feed_horse();
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("말에게 %s%s 주었습니다."), 
				item_manager::instance().table(dwFood)->locale_name,
				"");
	}
	else
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s 아이템이 필요합니다"), item_manager::instance().table(dwFood)->locale_name);
	}
}

constexpr int MAX_REASON_LEN = 128;
EVENTINFO(timed_event_info)
{
	dynamic_character_ptr ch;
	int		subcmd;
	int         	left_second;
	char		reason[MAX_REASON_LEN];

	timed_event_info() 
	: ch()
	, subcmd( 0 )
	, left_second( 0 )
	{
		::memset( reason, 0, MAX_REASON_LEN );
	}
};

struct send_disconnect_func
{
	void operator () (desc * d)
	{
		if (d->get_character())
		{
			if (d->get_character()->gm_level() == GM_PLAYER)
				d->get_character()->chat_packet(CHAT_TYPE_COMMAND, "quit Shutdown(SendDisconnectFunc)");
		}
	}
};

struct disconnect_func
{
	void operator () (desc * d)
	{
		if (d->type() == DESC_TYPE_CONNECTOR)
			return;

		if (d->is_phase(PHASE_P2P))
			return;

		if (d->get_character())
			d->get_character()->disconnect("server shutdown");

		d->set_phase(PHASE_CLOSE);
	}
};

EVENTINFO(shutdown_event_data)
{
	int seconds;

	shutdown_event_data()
	: seconds( 0 )
	{
	}
};

EVENTFUNC(shutdown_event)
{
	shutdown_event_data* info = dynamic_cast<shutdown_event_data*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "Shutdown event has no info" );
		return 0;
	}

	int * pSec = & (info->seconds);

	if (*pSec < 0)
	{
		metin2::log::debug( "Shutdown countdown at {:d}", *pSec);

		if (--*pSec == -10)
		{
			const desc_manager::desc_set & c_set_desc = desc_manager::instance().client_set();
			std::for_each(c_set_desc.begin(), c_set_desc.end(), disconnect_func());
			return metin2::config::server().passes_per_sec;
		}
		else if (*pSec < -10)
			return 0;

		return metin2::config::server().passes_per_sec;
	}
	else if (*pSec == 0)
	{
		const desc_manager::desc_set & c_set_desc = desc_manager::instance().client_set();
		std::for_each(c_set_desc.begin(), c_set_desc.end(), send_disconnect_func());
		no_more_client = true;
		--*pSec;
		return metin2::config::server().passes_per_sec;
	}
	else
	{
		char buf[64];
		snprintf(buf, sizeof(buf), lc_text("셧다운이 %d초 남았습니다."), *pSec);
		send_notice(buf);

		--*pSec;
		return metin2::config::server().passes_per_sec;
	}
}

void shutdown(int sec)
{
	if (no_more_client)
	{
		thecore_shutdown();
		return;
	}

	war_map_manager::instance().on_shutdown();

	char buf[64];
	snprintf(buf, sizeof(buf), lc_text("%d초 후 게임이 셧다운 됩니다."), sec);

	send_notice(buf);

	shutdown_event_data* info = alloc_event_info<shutdown_event_data>();
	info->seconds = sec;

	event_create(shutdown_event, info, 1);
}

ACMD(do_shutdown)
{
	if (nullptr == ch)
	{
		metin2::log::info("Shutdown command issued without a character");
	}
	packet_gg_shutdown p;
	p.header = HEADER_GG_SHUTDOWN;
	p2_p_manager::instance().send(&p, sizeof(packet_gg_shutdown));

	shutdown(10);
}

EVENTFUNC(timed_event)
{
	timed_event_info * info = dynamic_cast<timed_event_info *>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "Timed event has no info" );
		return 0;
	}

	character *	ch = info->ch;
	if (ch == nullptr) {
		return 0;
	}
	desc * d = ch->get_desc();

	if (info->left_second <= 0)
	{
		ch->timed_event = nullptr;

		switch (info->subcmd)
		{
			case SCMD_LOGOUT:
			case SCMD_QUIT:
			case SCMD_PHASE_SELECT:
				{
					packet_need_login_log_info acc_info;
					acc_info.player_id = ch->get_desc()->get_account_table().id;

					db_clientdesc->db_packet( HEADER_GD_VALID_LOGOUT, 0, &acc_info, sizeof(acc_info) );

					log_manager::instance().detail_login_log( false, ch );
				}
				break;
		}

		switch (info->subcmd)
		{
			case SCMD_LOGOUT:
				if (d)
					d->set_phase(PHASE_CLOSE);
				break;

			case SCMD_QUIT:
				ch->chat_packet(CHAT_TYPE_COMMAND, "quit");
				break;

			case SCMD_PHASE_SELECT:
				{
					ch->disconnect("returned to character select");

					if (d)
					{
						d->set_phase(PHASE_SELECT);
					}
				}
				break;
		}

		return 0;
	}
	else
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("%d초 남았습니다."), info->left_second);
		--info->left_second;
	}

	return passes_per_sec(1);
}

ACMD(do_cmd)
{

	if (ch->timed_event)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("취소 되었습니다."));
		event_cancel(&ch->timed_event);
		return;
	}

	switch (subcmd)
	{
		case SCMD_LOGOUT:
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("로그인 화면으로 돌아 갑니다. 잠시만 기다리세요."));
			break;

		case SCMD_QUIT:
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("게임을 종료 합니다. 잠시만 기다리세요."));
			break;

		case SCMD_PHASE_SELECT:
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("캐릭터를 전환 합니다. 잠시만 기다리세요."));
			break;
	}

	int nExitLimitTime = 10;

	if (ch->is_hack(false, true, nExitLimitTime) &&
		false == three_way_war::instance().is_sung_zi_map_index(ch->map_index()) &&
	   	(!ch->get_war_map() || ch->get_war_map()->type() == GUILD_WAR_TYPE_FLAG))
	{
		return;
	}

	switch (subcmd)
	{
		case SCMD_LOGOUT:
		case SCMD_QUIT:
		case SCMD_PHASE_SELECT:
			{
				timed_event_info* info = alloc_event_info<timed_event_info>();

				{
					if (ch->is_position(POS_FIGHTING))
						info->left_second = 10;
					else
						info->left_second = 3;
				}

				info->ch		= ch;
				info->subcmd		= subcmd;
				strlcpy(info->reason, argument, sizeof(info->reason));

				ch->timed_event	= event_create(timed_event, info, 1);
			}
			break;
	}
}

ACMD(do_mount)
{
}

ACMD(do_console)
{
	ch->chat_packet(CHAT_TYPE_COMMAND, "ConsoleEnable");
}

ACMD(do_restart)
{
	if (false == ch->is_dead())
	{
		ch->chat_packet(CHAT_TYPE_COMMAND, "CloseRestartWindow");
		ch->start_recovery_event();
		return;
	}

	if (nullptr == ch->dead_event)
		return;

	int iTimeToDead = (event_time(ch->dead_event) / metin2::config::server().passes_per_sec);

	if (subcmd != SCMD_RESTART_TOWN && (!ch->get_war_map() || ch->get_war_map()->type() == GUILD_WAR_TYPE_FLAG))
	{
		if (!metin2::config::test_server())
		{
			if (ch->is_hack())
			{
				if (false == three_way_war::instance().is_sung_zi_map_index(ch->map_index()))
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("아직 재시작 할 수 없습니다. (%d초 남음)"), iTimeToDead - (180 - portal_limit_time));
					return;
				}
			}

			if (iTimeToDead > 170)
			{
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("아직 재시작 할 수 없습니다. (%d초 남음)"), iTimeToDead - 170);
				return;
			}
		}
	}

	if (subcmd == SCMD_RESTART_TOWN)
	{
		if (ch->is_hack())
		{
			if ((!ch->get_war_map() || ch->get_war_map()->type() == GUILD_WAR_TYPE_FLAG) ||
			   	false == three_way_war::instance().is_sung_zi_map_index(ch->map_index()))
			{
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("아직 재시작 할 수 없습니다. (%d초 남음)"), iTimeToDead - (180 - portal_limit_time));
				return;
			}
		}

		if (iTimeToDead > 173)
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("아직 마을에서 재시작 할 수 없습니다. (%d 초 남음)"), iTimeToDead - 173);
			return;
		}
	}

	ch->chat_packet(CHAT_TYPE_COMMAND, "CloseRestartWindow");

	ch->get_desc()->set_phase(PHASE_GAME);
	ch->set_position(POS_STANDING);
	ch->start_recovery_event();

	if (1 == quest::quest_manager::instance().event_flag("threeway_war"))
	{
		if (subcmd == SCMD_RESTART_TOWN || subcmd == SCMD_RESTART_HERE)
		{
			if (true == three_way_war::instance().is_three_way_war_map_index(ch->map_index()) &&
					false == three_way_war::instance().is_sung_zi_map_index(ch->map_index()))
			{
				ch->warp_set(empire_start_x(ch->empire()), empire_start_y(ch->empire()));

				ch->revive_invisible(5);
				ch->point_change(POINT_HP, ch->max_hp() - ch->hp());
				ch->point_change(POINT_SP, ch->max_sp() - ch->sp());

				return;
			}

			if (true == three_way_war::instance().is_sung_zi_map_index(ch->map_index()))
			{
				if (three_way_war::instance().revive_token_for_player(ch->player_id()) <= 0)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("성지에서 부활 기회를 모두 잃었습니다! 마을로 이동합니다!"));
					ch->warp_set(empire_start_x(ch->empire()), empire_start_y(ch->empire()));
				}
				else
				{
					ch->show(ch->map_index(), get_sungzi_start_x(ch->empire()), get_sungzi_start_y(ch->empire()));
				}

				ch->point_change(POINT_HP, ch->max_hp() - ch->hp());
				ch->point_change(POINT_SP, ch->max_sp() - ch->sp());
				ch->revive_invisible(5);

				return;
			}
		}
	}

	if (ch->get_dungeon())
		ch->get_dungeon()->use_revive(ch);

	if (ch->get_war_map() && !ch->is_observer_mode())
	{
		war_map * pMap = ch->get_war_map();
		uint32_t dwGuildOpponent = pMap ? pMap->guild_opponent(ch) : 0;

		if (dwGuildOpponent)
		{
			switch (subcmd)
			{
				case SCMD_RESTART_TOWN:
					metin2::log::debug( "Player {} restarts in town", ch->name());
					pixel_position pos;

					if (war_map_manager::instance().start_position(ch->map_index(), ch->get_guild()->id() < dwGuildOpponent ? 0 : 1, pos))
						ch->show(ch->map_index(), pos.x, pos.y);
					else
						ch->exit_to_saved_location();

					ch->point_change(POINT_HP, ch->max_hp() - ch->hp());
					ch->point_change(POINT_SP, ch->max_sp() - ch->sp());
					ch->revive_invisible(5);
					break;

				case SCMD_RESTART_HERE:
					metin2::log::debug( "Player {} restarts in place", ch->name());
					ch->restart_at_same_pos();
					ch->point_change(POINT_HP, ch->max_hp() - ch->hp());
					ch->point_change(POINT_SP, ch->max_sp() - ch->sp());
					ch->revive_invisible(5);
					break;
			}

			return;
		}
	}

	switch (subcmd)
	{
		case SCMD_RESTART_TOWN:
			metin2::log::debug( "Player {} restarts in town", ch->name());
			pixel_position pos;

			if (sectree_manager::instance().recall_position_by_empire(ch->map_index(), ch->empire(), pos))
				ch->warp_set(pos.x, pos.y);
			else
				ch->warp_set(empire_start_x(ch->empire()), empire_start_y(ch->empire()));

			ch->point_change(POINT_HP, 50 - ch->hp());
			ch->death_penalty(1);
			break;

		case SCMD_RESTART_HERE:
			metin2::log::debug( "Player {} restarts in place", ch->name());
			ch->restart_at_same_pos();
			ch->point_change(POINT_HP, 50 - ch->hp());
			ch->death_penalty(0);
			ch->revive_invisible(5);
			break;
	}
}

constexpr int MAX_STAT = 90;
ACMD(do_stat_reset)
{
	ch->point_change(POINT_STAT_RESET_COUNT, 12 - ch->point(POINT_STAT_RESET_COUNT));
}

ACMD(do_stat_minus)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	if (ch->is_polymorphed())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("둔갑 중에는 능력을 올릴 수 없습니다."));
		return;
	}

	if (ch->point(POINT_STAT_RESET_COUNT) <= 0)
		return;

	if (!strcmp(arg1, "st"))
	{
		if (ch->real_point(POINT_ST) <= job_initial_points[ch->job()].st)
			return;

		ch->set_real_point(POINT_ST, ch->real_point(POINT_ST) - 1);
		ch->set_point(POINT_ST, ch->point(POINT_ST) - 1);
		ch->compute_points();
		ch->point_change(POINT_ST, 0);
	}
	else if (!strcmp(arg1, "dx"))
	{
		if (ch->real_point(POINT_DX) <= job_initial_points[ch->job()].dx)
			return;

		ch->set_real_point(POINT_DX, ch->real_point(POINT_DX) - 1);
		ch->set_point(POINT_DX, ch->point(POINT_DX) - 1);
		ch->compute_points();
		ch->point_change(POINT_DX, 0);
	}
	else if (!strcmp(arg1, "ht"))
	{
		if (ch->real_point(POINT_HT) <= job_initial_points[ch->job()].ht)
			return;

		ch->set_real_point(POINT_HT, ch->real_point(POINT_HT) - 1);
		ch->set_point(POINT_HT, ch->point(POINT_HT) - 1);
		ch->compute_points();
		ch->point_change(POINT_HT, 0);
		ch->point_change(POINT_MAX_HP, 0);
	}
	else if (!strcmp(arg1, "iq"))
	{
		if (ch->real_point(POINT_IQ) <= job_initial_points[ch->job()].iq)
			return;

		ch->set_real_point(POINT_IQ, ch->real_point(POINT_IQ) - 1);
		ch->set_point(POINT_IQ, ch->point(POINT_IQ) - 1);
		ch->compute_points();
		ch->point_change(POINT_IQ, 0);
		ch->point_change(POINT_MAX_SP, 0);
	}
	else
		return;

	ch->point_change(POINT_STAT, +1);
	ch->point_change(POINT_STAT_RESET_COUNT, -1);
	ch->compute_points();
}

ACMD(do_stat)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	if (ch->is_polymorphed())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("둔갑 중에는 능력을 올릴 수 없습니다."));
		return;
	}

	if (ch->point(POINT_STAT) <= 0)
		return;

	uint8_t idx = 0;

	if (!strcmp(arg1, "st"))
		idx = POINT_ST;
	else if (!strcmp(arg1, "dx"))
		idx = POINT_DX;
	else if (!strcmp(arg1, "ht"))
		idx = POINT_HT;
	else if (!strcmp(arg1, "iq"))
		idx = POINT_IQ;
	else
		return;

	if (ch->real_point(idx) >= MAX_STAT)
		return;

	ch->set_real_point(idx, ch->real_point(idx) + 1);
	ch->set_point(idx, ch->point(idx) + 1);
	ch->compute_points();
	ch->point_change(idx, 0);

	if (idx == POINT_IQ)
	{
		ch->point_change(POINT_MAX_HP, 0);
	}
	else if (idx == POINT_HT)
	{
		ch->point_change(POINT_MAX_SP, 0);
	}

	ch->point_change(POINT_STAT, -1);
	ch->compute_points();
}

ACMD(do_pvp)
{
	if (ch->get_arena() != nullptr || arena_manager::instance().is_arena_map(ch->map_index()) == true)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("대련장에서 사용하실 수 없습니다."));
		return;
	}

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	uint32_t vid = 0;
	str_to_number(vid, arg1);
	character * pkVictim = character_manager::instance().find(vid);

	if (!pkVictim)
		return;

	if (pkVictim->is_npc())
		return;

	if (pkVictim->get_arena() != nullptr)
	{
		pkVictim->chat_packet(CHAT_TYPE_INFO, lc_text("상대방이 대련중입니다."));
		return;
	}

	pvp_manager::instance().insert(ch, pkVictim);
}

ACMD(do_guildskillup)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	if (!ch->get_guild())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드에 속해있지 않습니다."));
		return;
	}

	guild* g = ch->get_guild();
	guild_member* gm = g->member(ch->player_id());
	if (gm->grade == GUILD_LEADER_GRADE)
	{
		uint32_t vnum = 0;
		str_to_number(vnum, arg1);
		g->skill_level_up(vnum);
	}
	else
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드 스킬 레벨을 변경할 권한이 없습니다."));
	}
}

ACMD(do_skillup)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	uint32_t vnum = 0;
	str_to_number(vnum, arg1);

	if (true == ch->can_use_skill(vnum))
	{
		ch->skill_level_up(vnum);
	}
	else
	{
		switch(vnum)
		{
			case SKILL_HORSE_WILDATTACK:
			case SKILL_HORSE_CHARGE:
			case SKILL_HORSE_ESCAPE:
			case SKILL_HORSE_WILDATTACK_RANGE:

			case SKILL_7_A_ANTI_TANHWAN:
			case SKILL_7_B_ANTI_AMSEOP:
			case SKILL_7_C_ANTI_SWAERYUNG:
			case SKILL_7_D_ANTI_YONGBI:

			case SKILL_8_A_ANTI_GIGONGCHAM:
			case SKILL_8_B_ANTI_YEONSA:
			case SKILL_8_C_ANTI_MAHWAN:
			case SKILL_8_D_ANTI_BYEURAK:

			case SKILL_ADD_HP:
			case SKILL_RESIST_PENETRATE:
				ch->skill_level_up(vnum);
				break;
		}
	}
}

ACMD(do_safebox_close)
{
	ch->close_safebox();
}

ACMD(do_safebox_password)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	ch->req_safebox_load(arg1);
}

ACMD(do_safebox_change_password)
{
	char arg1[256];
	char arg2[256];

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || strlen(arg1)>6)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 잘못된 암호를 입력하셨습니다."));
		return;
	}

	if (!*arg2 || strlen(arg2)>6)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 잘못된 암호를 입력하셨습니다."));
		return;
	}

	safebox_change_password_packet p;

	p.id = ch->get_desc()->get_account_table().id;
	strlcpy(p.old_password, arg1, sizeof(p.old_password));
	strlcpy(p.new_password, arg2, sizeof(p.new_password));

	db_clientdesc->db_packet(HEADER_GD_SAFEBOX_CHANGE_PASSWORD, ch->get_desc()->handle(), &p, sizeof(p));
}

ACMD(do_mall_password)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1 || strlen(arg1) > 6)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 잘못된 암호를 입력하셨습니다."));
		return;
	}

	int iPulse = thecore_pulse();

	if (ch->mall())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 창고가 이미 열려있습니다."));
		return;
	}

	if (iPulse - ch->mall_load_time() < metin2::config::server().passes_per_sec * 10)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 창고를 닫은지 10초 안에는 열 수 없습니다."));
		return;
	}

	ch->set_mall_load_time(iPulse);

	safebox_load_packet p;
	p.id = ch->get_desc()->get_account_table().id;
	strlcpy(p.login, ch->get_desc()->get_account_table().login, sizeof(p.login));
	strlcpy(p.password, arg1, sizeof(p.password));

	db_clientdesc->db_packet(HEADER_GD_MALL_LOAD, ch->get_desc()->handle(), &p, sizeof(p));
}

ACMD(do_mall_close)
{
	if (ch->mall())
	{
		ch->set_mall_load_time(thecore_pulse());
		ch->close_mall();
		ch->save();
	}
}

ACMD(do_ungroup)
{
	if (!ch->get_party())
		return;

	if (!party_manager::instance().is_enable_pc_party())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 서버 문제로 파티 관련 처리를 할 수 없습니다."));
		return;
	}

	if (ch->get_dungeon())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 던전 안에서는 파티에서 나갈 수 없습니다."));
		return;
	}

	party * pParty = ch->get_party();

	if (pParty->member_count() == 2)
	{
		party_manager::instance().delete_party(pParty);
	}
	else
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<파티> 파티에서 나가셨습니다."));
		pParty->quit(ch->player_id());
	}
}

ACMD(do_close_shop)
{
	if (ch->my_shop())
	{
		ch->close_my_shop();
		return;
	}
}

ACMD(do_set_walk_mode)
{
	ch->set_now_walking(true);
	ch->set_walking(true);
}

ACMD(do_set_run_mode)
{
	ch->set_now_walking(false);
	ch->set_walking(false);
}

ACMD(do_war)
{
	guild * g = ch->get_guild();

	if (!g)
		return;

	if (g->under_any_war())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 이미 다른 전쟁에 참전 중 입니다."));
		return;
	}

	char arg1[256], arg2[256];
	int type = GUILD_WAR_TYPE_FIELD;
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1)
		return;

	if (*arg2)
	{
		str_to_number(type, arg2);

		if (type >= GUILD_WAR_TYPE_MAX_NUM)
			type = GUILD_WAR_TYPE_FIELD;

		if(type < 0)
			return;
	}

	uint32_t gm_pid = g->master_pid();

	if (gm_pid != ch->player_id())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드전에 대한 권한이 없습니다."));
		return;
	}

	guild * opp_g = guild_manager::instance().find_guild_by_name(arg1);

	if (!opp_g)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 그런 길드가 없습니다."));
		return;
	}

	switch (g->get_guild_war_state(opp_g->id()))
	{
		case GUILD_WAR_NONE:
			{
				if (opp_g->under_any_war())
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 상대방 길드가 이미 전쟁 중 입니다."));
					return;
				}

				int iWarPrice = kor_a_guild_war_info[type].war_price;

				if (g->guild_money() < iWarPrice)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 전비가 부족하여 길드전을 할 수 없습니다."));
					return;
				}

				if (opp_g->guild_money() < iWarPrice)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 상대방 길드의 전비가 부족하여 길드전을 할 수 없습니다."));
					return;
				}
			}
			break;

		case GUILD_WAR_SEND_DECLARE:
			{
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("이미 선전포고 중인 길드입니다."));
				return;
			}
			break;

		case GUILD_WAR_RECV_DECLARE:
			{
				if (opp_g->under_any_war())
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 상대방 길드가 이미 전쟁 중 입니다."));
					g->request_refuse_war(opp_g->id());
					return;
				}
			}
			break;

		case GUILD_WAR_RESERVE:
			{
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 이미 전쟁이 예약된 길드 입니다."));
				return;
			}
			break;

		case GUILD_WAR_END:
			return;

		default:
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 이미 전쟁 중인 길드입니다."));
			g->request_refuse_war(opp_g->id());
			return;
	}

	if (!g->can_start_war(type))
	{
		if (g->ladder_point() == 0)
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 레더 점수가 모자라서 길드전을 할 수 없습니다."));
			metin2::log::debug( "Guild war refused: guild {} has no ladder points", g->name());
		}
		else if (g->member_count() < GUILD_WAR_MIN_MEMBER_COUNT)
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드전을 하기 위해선 최소한 %d명이 있어야 합니다."), GUILD_WAR_MIN_MEMBER_COUNT);
			metin2::log::debug( "Guild war refused: fewer than {:d} members", GUILD_WAR_MIN_MEMBER_COUNT);
		}
		else
		{
			metin2::log::debug( "Guild war refused: guild {} cannot start war type {:d}", g->name(), type);
		}
		return;
	}

	if (!opp_g->can_start_war(GUILD_WAR_TYPE_FIELD))
	{
		if (opp_g->ladder_point() == 0)
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 상대방 길드의 레더 점수가 모자라서 길드전을 할 수 없습니다."));
		else if (opp_g->member_count() < GUILD_WAR_MIN_MEMBER_COUNT)
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 상대방 길드의 길드원 수가 부족하여 길드전을 할 수 없습니다."));
		return;
	}

	do
	{
		if (g->master_character() != nullptr)
			break;

		cci *pCCI = p2_p_manager::instance().find_by_pid(g->master_pid());

		if (pCCI != nullptr)
			break;

		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 상대방 길드의 길드장이 접속중이 아닙니다."));
		g->request_refuse_war(opp_g->id());
		return;

	} while (false);

	do
	{
		if (opp_g->master_character() != nullptr)
			break;

		cci *pCCI = p2_p_manager::instance().find_by_pid(opp_g->master_pid());

		if (pCCI != nullptr)
			break;

		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 상대방 길드의 길드장이 접속중이 아닙니다."));
		g->request_refuse_war(opp_g->id());
		return;

	} while (false);

	g->request_declare_war(opp_g->id(), type);
}

ACMD(do_nowar)
{
	guild* g = ch->get_guild();
	if (!g)
		return;

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	uint32_t gm_pid = g->master_pid();

	if (gm_pid != ch->player_id())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 길드전에 대한 권한이 없습니다."));
		return;
	}

	guild* opp_g = guild_manager::instance().find_guild_by_name(arg1);

	if (!opp_g)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("<길드> 그런 길드가 없습니다."));
		return;
	}

	g->request_refuse_war(opp_g->id());
}

ACMD(do_detaillog)
{
	ch->detail_log();
}

ACMD(do_monsterlog)
{
	ch->toggle_monster_log();
}

ACMD(do_pkmode)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	uint8_t mode = 0;
	str_to_number(mode, arg1);

	if (mode == PK_MODE_PROTECT)
		return;

	if (ch->level() < metin2::config::game().pk_protect_level && mode != 0)
		return;

	ch->set_pk_mode(mode);
}

ACMD(do_messenger_auth)
{
	if (ch->get_arena())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("대련장에서 사용하실 수 없습니다."));
		return;
	}

	char arg1[256], arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || !*arg2)
		return;

	char answer = std::tolower(static_cast<unsigned char>(*arg1));

	if (!messenger_manager::instance().auth_to_add(ch->name(), arg2, answer == 'y' ? false : true))
		return;

	if (answer != 'y')
	{
		character * tch = character_manager::instance().find_pc(arg2);

		if (tch)
			tch->chat_packet(CHAT_TYPE_INFO, lc_text("%s 님으로 부터 친구 등록을 거부 당했습니다."), ch->name());
	}

}

ACMD(do_setblockmode)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (*arg1)
	{
		uint8_t flag = 0;
		str_to_number(flag, arg1);
		ch->set_block_mode(flag);
	}
}

ACMD(do_unmount)
{
	if (true == ch->un_equip_special_ride_unique_item())
	{
		ch->remove_affect(AFFECT_MOUNT);
		ch->remove_affect(AFFECT_MOUNT_BONUS);

		if (ch->is_horse_riding())
		{
			ch->stop_riding(); 
		}
	}
	else
	{
		ch->chat_packet( CHAT_TYPE_INFO, lc_text("인벤토리가 꽉 차서 내릴 수 없습니다."));
	}

}

ACMD(do_observer_exit)
{
	if (ch->is_observer_mode())
	{
		if (ch->get_war_map())
			ch->set_war_map(nullptr);

		if (ch->get_arena() != nullptr || ch->arena_observer_mode() == true)
		{
			ch->set_arena_observer_mode(false);

			if (ch->get_arena() != nullptr)
				ch->get_arena()->remove_observer(ch->player_id());

			ch->set_arena(nullptr);
			ch->warp_set(arena_return_point_x(ch->empire()), arena_return_point_y(ch->empire()));
		}
		else
		{
			ch->exit_to_saved_location();
		}
		ch->set_observer_mode(false);
	}
}

ACMD(do_view_equip)
{
	if (ch->gm_level() <= GM_PLAYER)
		return;

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (*arg1)
	{
		uint32_t vid = 0;
		str_to_number(vid, arg1);
		character * tch = character_manager::instance().find(vid);

		if (!tch)
			return;

		if (!tch->is_pc())
			return;
		tch->send_equipment(ch);
	}
}

ACMD(do_party_request)
{
	if (ch->get_arena())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("대련장에서 사용하실 수 없습니다."));
		return;
	}

	if (ch->get_party())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("이미 파티에 속해 있으므로 가입신청을 할 수 없습니다."));
		return;
	}

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	uint32_t vid = 0;
	str_to_number(vid, arg1);
	character * tch = character_manager::instance().find(vid);

	if (tch)
		if (!ch->request_to_party(tch))
			ch->chat_packet(CHAT_TYPE_COMMAND, "PartyRequestDenied");
}

ACMD(do_party_request_accept)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	uint32_t vid = 0;
	str_to_number(vid, arg1);
	character * tch = character_manager::instance().find(vid);

	if (tch)
		ch->accept_to_party(tch);
}

ACMD(do_party_request_deny)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	uint32_t vid = 0;
	str_to_number(vid, arg1);
	character * tch = character_manager::instance().find(vid);

	if (tch)
		ch->deny_to_party(tch);
}

ACMD(do_monarch_warpto)
{

	if (!monarch::instance().is_monarch(ch->player_id(), ch->empire()))
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("군주만이 사용 가능한 기능입니다"));
		return;
	}

	if (!ch->is_mcok(character::MI_WARP))
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("%d 초간 쿨타임이 적용중입니다."), ch->mcl_time(character::MI_WARP));
		return;
	}

	const int WarpPrice = 10000;

	if (!monarch::instance().is_money_ok(WarpPrice, ch->empire()))
	{
		int NationMoney = monarch::instance().money(ch->empire());
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("국고에 돈이 부족합니다. 현재 : %u 필요금액 : %u"), NationMoney, WarpPrice);
		return;	
	}

	int x = 0, y = 0;
	char arg1[256];

	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("사용법: warpto <character name>"));
		return;
	}

	character * tch = character_manager::instance().find_pc(arg1);

	if (!tch)
	{
		cci * pkCCI = p2_p_manager::instance().find(arg1);

		if (pkCCI)
		{
			if (pkCCI->empire != ch->empire())
			{
				ch->chat_packet (CHAT_TYPE_INFO, lc_text("타제국 유저에게는 이동할수 없습니다"));
				return;
			}

			if (pkCCI->channel != metin2::config::server().channel)
			{
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("해당 유저는 %d 채널에 있습니다. (현재 채널 %d)"), pkCCI->channel, metin2::config::server().channel);
				return;
			}
			if (!is_monarch_warp_zone(pkCCI->map_index))
			{
				ch->chat_packet (CHAT_TYPE_INFO, lc_text("해당 지역으로 이동할 수 없습니다."));
				return;
			}

			pixel_position pos;

			if (!sectree_manager::instance().center_position_of_map(pkCCI->map_index, pos))
				ch->chat_packet(CHAT_TYPE_INFO, "Cannot find map (index %d)", pkCCI->map_index);
			else
			{
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s 에게로 이동합니다"), arg1);
				ch->warp_set(pos.x, pos.y);

				monarch::instance().sendto_db_dec_money(WarpPrice, ch->empire(), ch);

				ch->set_mc(character::MI_WARP);
			}
		}
		else if (nullptr == character_manager::instance().find_pc(arg1))
		{
			ch->chat_packet(CHAT_TYPE_INFO, "There is no one by that name");
		}

		return;
	}
	else
	{
		if (tch->empire() != ch->empire())
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("타제국 유저에게는 이동할수 없습니다"));
			return;
		}
		if (!is_monarch_warp_zone(tch->map_index()))
		{
			ch->chat_packet (CHAT_TYPE_INFO, lc_text("해당 지역으로 이동할 수 없습니다."));
			return;
		}
		x = tch->x();
		y = tch->y();
	}

	ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s 에게로 이동합니다"), arg1);
	ch->warp_set(x, y);
	ch->stop();

	monarch::instance().sendto_db_dec_money(WarpPrice, ch->empire(), ch);

	ch->set_mc(character::MI_WARP);
}

ACMD(do_monarch_transfer)
{

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("사용법: transfer <name>"));
		return;
	}

	if (!monarch::instance().is_monarch(ch->player_id(), ch->empire()))
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("군주만이 사용 가능한 기능입니다"));
		return;
	}

	if (!ch->is_mcok(character::MI_TRANSFER))
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("%d 초간 쿨타임이 적용중입니다."), ch->mcl_time(character::MI_TRANSFER));	
		return;
	}

	const int WarpPrice = 10000;

	if (!monarch::instance().is_money_ok(WarpPrice, ch->empire()))
	{
		int NationMoney = monarch::instance().money(ch->empire());
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("국고에 돈이 부족합니다. 현재 : %u 필요금액 : %u"), NationMoney, WarpPrice);
		return;	
	}

	character * tch = character_manager::instance().find_pc(arg1);

	if (!tch)
	{
		cci * pkCCI = p2_p_manager::instance().find(arg1);

		if (pkCCI)
		{
			if (pkCCI->empire != ch->empire())
			{
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("다른 제국 유저는 소환할 수 없습니다."));
				return;
			}
			if (pkCCI->channel != metin2::config::server().channel)
			{
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s 님은 %d 채널에 접속 중 입니다. (현재 채널: %d)"), arg1, pkCCI->channel, metin2::config::server().channel);
				return;
			}
			if (!is_monarch_warp_zone(pkCCI->map_index))
			{
				ch->chat_packet (CHAT_TYPE_INFO, lc_text("해당 지역으로 이동할 수 없습니다."));
				return;
			}
			if (!is_monarch_warp_zone(ch->map_index()))
			{
				ch->chat_packet (CHAT_TYPE_INFO, lc_text("해당 지역으로 소환할 수 없습니다."));
				return;
			}

			packet_gg_transfer pgg;

			pgg.header = HEADER_GG_TRANSFER;
			strlcpy(pgg.name, arg1, sizeof(pgg.name));
			pgg.x = ch->x();
			pgg.y = ch->y();

			p2_p_manager::instance().send(&pgg, sizeof(packet_gg_transfer));
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s 님을 소환하였습니다."), arg1);

			monarch::instance().sendto_db_dec_money(WarpPrice, ch->empire(), ch);
			ch->set_mc(character::MI_TRANSFER);
		}
		else
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("입력하신 이름을 가진 사용자가 없습니다."));
		}

		return;
	}

	if (ch == tch)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("자신을 소환할 수 없습니다."));
		return;
	}

	if (tch->empire() != ch->empire())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("다른 제국 유저는 소환할 수 없습니다."));
		return;
	}
	if (!is_monarch_warp_zone(tch->map_index()))
	{
		ch->chat_packet (CHAT_TYPE_INFO, lc_text("해당 지역으로 이동할 수 없습니다."));
		return;
	}
	if (!is_monarch_warp_zone(ch->map_index()))
	{
		ch->chat_packet (CHAT_TYPE_INFO, lc_text("해당 지역으로 소환할 수 없습니다."));
		return;
	}

	tch->warp_set(ch->x(), ch->y(), ch->map_index());

	monarch::instance().sendto_db_dec_money(WarpPrice, ch->empire(), ch);
	ch->set_mc(character::MI_TRANSFER);
}

ACMD(do_monarch_info)
{
	if (monarch::instance().is_monarch(ch->player_id(), ch->empire()))	
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("나의 군주 정보"));
		monarch_info * p = monarch::instance().get_monarch();
		for (int n = 1; n < 4; ++n)
		{
			if (n == ch->empire())
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("[%s군주] : %s  보유금액 %lld "), empire_name(n), p->name[n], p->money[n]);
			else
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("[%s군주] : %s  "), empire_name(n), p->name[n]);

		}
	}
	else
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("군주 정보"));
		monarch_info * p = monarch::instance().get_monarch();
		for (int n = 1; n < 4; ++n)
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("[%s군주] : %s  "), empire_name(n), p->name[n]);

		}
	}

}	

ACMD(do_elect)
{
	db_clientdesc->db_packet_header(HEADER_GD_COME_TO_VOTE, ch->get_desc()->handle(), 0);
}

struct goto_info
{
	std::string 	st_name;

	uint8_t 	empire;
	int 	index;
	uint32_t 	x, y;

	goto_info()
	{
		st_name 	= "";
		empire 		= 0;
		index 	= 0;

		x = 0;
		y = 0;
	}

};

ACMD(do_monarch_tax)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
		ch->chat_packet(CHAT_TYPE_INFO, "Usage: monarch_tax <1-50>");
		return;
	}

	if (!ch->is_monarch())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("군주만이 사용할수 있는 기능입니다"));
		return;
	}

	int tax = 0;
	str_to_number(tax,  arg1);

	if (tax < 1 || tax > 50)
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("1-50 사이의 수치를 선택해주세요"));

	quest::quest_manager::instance().set_event_flag("trade_tax", tax); 

	ch->chat_packet(CHAT_TYPE_INFO, lc_text("세금이 %d %로 설정되었습니다"));

	char szMsg[1024];	

	snprintf(szMsg, sizeof(szMsg), "군주의 명으로 세금이 %d %% 로 변경되었습니다", tax);
	broadcast_notice(szMsg);

	snprintf(szMsg, sizeof(szMsg), "앞으로는 거래 금액의 %d %% 가 국고로 들어가게됩니다.", tax);
	broadcast_notice(szMsg);

	ch->set_mc(character::MI_TAX); 
}

static const uint32_t cs_dw_monarch_mob_vnums[] =
{
	191,
	192,
	193,
	194,
	391,
	392,
	393,
	394,
	491,
	492,
	493,
	494,
	591,
	691,
	791,
	1304,
	1901,
	2091,
	2191,
	2206,
	0,
};

ACMD(do_monarch_mob)
{
	char arg1[256];
	character *	tch;

	one_argument(argument, arg1, sizeof(arg1));

	if (!ch->is_monarch())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("군주만이 사용할수 있는 기능입니다"));
		return;
	}

	if (!*arg1)
	{
		ch->chat_packet(CHAT_TYPE_INFO, "Usage: mmob <mob name>");
		return;
	}

	const int SummonPrice = 5000000;

	if (!ch->is_mcok(character::MI_SUMMON))
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("%d 초간 쿨타임이 적용중입니다."), ch->mcl_time(character::MI_SUMMON));	
		return;
	}

	if (!monarch::instance().is_money_ok(SummonPrice, ch->empire()))
	{
		int NationMoney = monarch::instance().money(ch->empire());
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("국고에 돈이 부족합니다. 현재 : %u 필요금액 : %u"), NationMoney, SummonPrice);
		return;	
	}

	const mob * pkMob;
	uint32_t vnum = 0;

	if (isdigit(*arg1))
	{
		str_to_number(vnum, arg1);

		if ((pkMob = mob_manager::instance().get(vnum)) == nullptr)
			vnum = 0;
	}
	else
	{
		pkMob = mob_manager::instance().get(arg1, true);

		if (pkMob)
			vnum = pkMob->table.vnum;
	}

	uint32_t count;

	for (count = 0; cs_dw_monarch_mob_vnums[count] != 0; ++count)
		if (cs_dw_monarch_mob_vnums[count] == vnum)
			break;

	if (0 == cs_dw_monarch_mob_vnums[count])
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("소환할수 없는 몬스터 입니다. 소환가능한 몬스터는 홈페이지를 참조하세요"));
		return;
	}

	tch = character_manager::instance().spawn_mob_range(vnum, 
			ch->map_index(),
			ch->x() - number(200, 750), 
			ch->y() - number(200, 750), 
			ch->x() + number(200, 750), 
			ch->y() + number(200, 750), 
			true,
			pkMob->table.type == CHAR_TYPE_STONE,
			true);

	if (tch)
	{
		monarch::instance().sendto_db_dec_money(SummonPrice, ch->empire(), ch);

		ch->set_mc(character::MI_SUMMON); 
	}
}

static auto fn_point_string(int apply_number) -> const char*
{
	switch (apply_number)
	{
		case POINT_MAX_HP:	return lc_text("최대 생명력 +%d");
		case POINT_MAX_SP:	return lc_text("최대 정신력 +%d");
		case POINT_HT:		return lc_text("체력 +%d");
		case POINT_IQ:		return lc_text("지능 +%d");
		case POINT_ST:		return lc_text("근력 +%d");
		case POINT_DX:		return lc_text("민첩 +%d");
		case POINT_ATT_SPEED:	return lc_text("공격속도 +%d");
		case POINT_MOV_SPEED:	return lc_text("이동속도 %d");
		case POINT_CASTING_SPEED:	return lc_text("쿨타임 -%d");
		case POINT_HP_REGEN:	return lc_text("생명력 회복 +%d");
		case POINT_SP_REGEN:	return lc_text("정신력 회복 +%d");
		case POINT_POISON_PCT:	return lc_text("독공격 %d");
		case POINT_STUN_PCT:	return lc_text("스턴 +%d");
		case POINT_SLOW_PCT:	return lc_text("슬로우 +%d");
		case POINT_CRITICAL_PCT:	return lc_text("%d%% 확률로 치명타 공격");
		case POINT_RESIST_CRITICAL:	return lc_text("상대의 치명타 확률 %d%% 감소");
		case POINT_PENETRATE_PCT:	return lc_text("%d%% 확률로 관통 공격");
		case POINT_RESIST_PENETRATE: return lc_text("상대의 관통 공격 확률 %d%% 감소");
		case POINT_ATTBONUS_HUMAN:	return lc_text("인간류 몬스터 타격치 +%d%%");
		case POINT_ATTBONUS_ANIMAL:	return lc_text("동물류 몬스터 타격치 +%d%%");
		case POINT_ATTBONUS_ORC:	return lc_text("웅귀족 타격치 +%d%%");
		case POINT_ATTBONUS_MILGYO:	return lc_text("밀교류 타격치 +%d%%");
		case POINT_ATTBONUS_UNDEAD:	return lc_text("시체류 타격치 +%d%%");
		case POINT_ATTBONUS_DEVIL:	return lc_text("악마류 타격치 +%d%%");
		case POINT_STEAL_HP:		return lc_text("타격치 %d%% 를 생명력으로 흡수");
		case POINT_STEAL_SP:		return lc_text("타력치 %d%% 를 정신력으로 흡수");
		case POINT_MANA_BURN_PCT:	return lc_text("%d%% 확률로 타격시 상대 전신력 소모");
		case POINT_DAMAGE_SP_RECOVER:	return lc_text("%d%% 확률로 피해시 정신력 회복");
		case POINT_BLOCK:			return lc_text("물리타격시 블럭 확률 %d%%");
		case POINT_DODGE:			return lc_text("활 공격 회피 확률 %d%%");
		case POINT_RESIST_SWORD:	return lc_text("한손검 방어 %d%%");
		case POINT_RESIST_TWOHAND:	return lc_text("양손검 방어 %d%%");
		case POINT_RESIST_DAGGER:	return lc_text("두손검 방어 %d%%");
		case POINT_RESIST_BELL:		return lc_text("방울 방어 %d%%");
		case POINT_RESIST_FAN:		return lc_text("부채 방어 %d%%");
		case POINT_RESIST_BOW:		return lc_text("활공격 저항 %d%%");
		case POINT_RESIST_FIRE:		return lc_text("화염 저항 %d%%");
		case POINT_RESIST_ELEC:		return lc_text("전기 저항 %d%%");
		case POINT_RESIST_MAGIC:	return lc_text("마법 저항 %d%%");
		case POINT_RESIST_WIND:		return lc_text("바람 저항 %d%%");
		case POINT_RESIST_ICE:		return lc_text("냉기 저항 %d%%");
		case POINT_RESIST_EARTH:	return lc_text("대지 저항 %d%%");
		case POINT_RESIST_DARK:		return lc_text("어둠 저항 %d%%");
		case POINT_REFLECT_MELEE:	return lc_text("직접 타격치 반사 확률 : %d%%");
		case POINT_REFLECT_CURSE:	return lc_text("저주 되돌리기 확률 %d%%");
		case POINT_POISON_REDUCE:	return lc_text("독 저항 %d%%");
		case POINT_KILL_SP_RECOVER:	return lc_text("%d%% 확률로 적퇴치시 정신력 회복");
		case POINT_EXP_DOUBLE_BONUS:	return lc_text("%d%% 확률로 적퇴치시 경험치 추가 상승");
		case POINT_GOLD_DOUBLE_BONUS:	return lc_text("%d%% 확률로 적퇴치시 돈 2배 드롭");
		case POINT_ITEM_DROP_BONUS:	return lc_text("%d%% 확률로 적퇴치시 아이템 2배 드롭");
		case POINT_POTION_BONUS:	return lc_text("물약 사용시 %d%% 성능 증가");
		case POINT_KILL_HP_RECOVERY:	return lc_text("%d%% 확률로 적퇴치시 생명력 회복");
		case POINT_ATT_GRADE_BONUS:	return lc_text("공격력 +%d");
		case POINT_DEF_GRADE_BONUS:	return lc_text("방어력 +%d");
		case POINT_MAGIC_ATT_GRADE:	return lc_text("마법 공격력 +%d");
		case POINT_MAGIC_DEF_GRADE:	return lc_text("마법 방어력 +%d");
		case POINT_MAX_STAMINA:	return lc_text("최대 지구력 +%d");
		case POINT_ATTBONUS_WARRIOR:	return lc_text("무사에게 강함 +%d%%");
		case POINT_ATTBONUS_ASSASSIN:	return lc_text("자객에게 강함 +%d%%");
		case POINT_ATTBONUS_SURA:		return lc_text("수라에게 강함 +%d%%");
		case POINT_ATTBONUS_SHAMAN:		return lc_text("무당에게 강함 +%d%%");
		case POINT_ATTBONUS_MONSTER:	return lc_text("몬스터에게 강함 +%d%%");
		case POINT_MALL_ATTBONUS:		return lc_text("공격력 +%d%%");
		case POINT_MALL_DEFBONUS:		return lc_text("방어력 +%d%%");
		case POINT_MALL_EXPBONUS:		return lc_text("경험치 %d%%");
		case POINT_MALL_ITEMBONUS:		return lc_text("아이템 드롭율 %.1f배");
		case POINT_MALL_GOLDBONUS:		return lc_text("돈 드롭율 %.1f배");
		case POINT_MAX_HP_PCT:			return lc_text("최대 생명력 +%d%%");
		case POINT_MAX_SP_PCT:			return lc_text("최대 정신력 +%d%%");
		case POINT_SKILL_DAMAGE_BONUS:	return lc_text("스킬 데미지 %d%%");
		case POINT_NORMAL_HIT_DAMAGE_BONUS:	return lc_text("평타 데미지 %d%%");
		case POINT_SKILL_DEFEND_BONUS:		return lc_text("스킬 데미지 저항 %d%%");
		case POINT_NORMAL_HIT_DEFEND_BONUS:	return lc_text("평타 데미지 저항 %d%%");
		case POINT_RESIST_WARRIOR:	return lc_text("무사공격에 %d%% 저항");
		case POINT_RESIST_ASSASSIN:	return lc_text("자객공격에 %d%% 저항");
		case POINT_RESIST_SURA:		return lc_text("수라공격에 %d%% 저항");
		case POINT_RESIST_SHAMAN:	return lc_text("무당공격에 %d%% 저항");
		default:					return nullptr;
	}
}

static auto fn_hair_affect_string(character * ch, char *buf, size_t bufsiz) -> bool
{
	if (nullptr == ch || nullptr == buf)
		return false;

	affect* aff = nullptr;
	time_t expire = 0;
	struct tm ltm;
	int	year, mon, day;
	int	offset = 0;

	aff = ch->find_affect(AFFECT_HAIR);

	if (nullptr == aff)
		return false;

	expire = ch->quest_flag("hair.limit_time");

	if (expire < get_global_time())
		return false;

	offset = snprintf(buf, bufsiz, fn_point_string(aff->apply_on), aff->apply_value);

	if (offset < 0 || offset >= (int) bufsiz)
		offset = bufsiz - 1;

	localtime_r(&expire, &ltm);

	year	= ltm.tm_year + 1900;
	mon		= ltm.tm_mon + 1;
	day		= ltm.tm_mday;

	snprintf(buf + offset, bufsiz - offset, lc_text(" (만료일 : %d년 %d월 %d일)"), year, mon, day);

	return true;
}

ACMD(do_costume)
{
	char buf[512];
	const size_t bufferSize = sizeof(buf);

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	item* pBody = ch->wear(WEAR_COSTUME_BODY);
	item* pHair = ch->wear(WEAR_COSTUME_HAIR);

	ch->chat_packet(CHAT_TYPE_INFO, "COSTUME status:");

	if (pHair)
	{
		const char* itemName = pHair->name();
		ch->chat_packet(CHAT_TYPE_INFO, "  HAIR : %s", itemName);

		for (int i = 0; i < pHair->attribute_count(); ++i)
		{
			const player_item_attribute& attr = pHair->get_attribute(i);
			if (0 < attr.type)
			{
				snprintf(buf, bufferSize, fn_point_string(attr.type), attr.s_value);
				ch->chat_packet(CHAT_TYPE_INFO, "     %s", buf);
			}
		}

		if (pHair->is_equipped() && arg1[0] == 'h')
			ch->unequip_item(pHair);
	}

	if (pBody)
	{
		const char* itemName = pBody->name();
		ch->chat_packet(CHAT_TYPE_INFO, "  BODY : %s", itemName);

		if (pBody->is_equipped() && arg1[0] == 'b')
			ch->unequip_item(pBody);
	}
}

ACMD(do_hair)
{
	char buf[256];

	if (false == fn_hair_affect_string(ch, buf, sizeof(buf)))
		return;

	ch->chat_packet(CHAT_TYPE_INFO, buf);
}

ACMD(do_inventory)
{
	int	index = 0;
	int	count		= 1;

	char arg1[256];
	char arg2[256];

	item *	item;

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1)
	{
		ch->chat_packet(CHAT_TYPE_INFO, "Usage: inventory <start_index> <count>");
		return;
	}

	if (!*arg2)
	{
		index = 0;
		str_to_number(count, arg1);
	}
	else
	{
		str_to_number(index, arg1); index = std::min(index, INVENTORY_MAX_NUM);
		str_to_number(count, arg2); count = std::min(count, INVENTORY_MAX_NUM);
	}

	for (int i = 0; i < count; ++i)
	{
		if (index >= INVENTORY_MAX_NUM)
			break;

		item = ch->inventory_item(index);

		ch->chat_packet(CHAT_TYPE_INFO, "inventory [%d] = %s",
						index, item ? item->name() : "<NONE>");
		++index;
	}
}

ACMD(do_gift)
{
	ch->chat_packet(CHAT_TYPE_COMMAND, "gift");
}

ACMD(do_cube)
{
	if (!ch->can_do_cube())
		return;

	metin2::log::debug( "Cube command from player {}: {}", ch->name(), argument);
	int cube_index = 0, inven_index = 0;
	const char *line;

	char arg1[256], arg2[256], arg3[256];

	line = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	one_argument(line, arg3, sizeof(arg3));

	if (0 == arg1[0])
	{
		ch->chat_packet(CHAT_TYPE_INFO, "Usage: cube open");
		ch->chat_packet(CHAT_TYPE_INFO, "       cube close");
		ch->chat_packet(CHAT_TYPE_INFO, "       cube add <inveltory_index>");
		ch->chat_packet(CHAT_TYPE_INFO, "       cube delete <cube_index>");
		ch->chat_packet(CHAT_TYPE_INFO, "       cube list");
		ch->chat_packet(CHAT_TYPE_INFO, "       cube cancel");
		ch->chat_packet(CHAT_TYPE_INFO, "       cube make [all]");
		return;
	}

	const std::string& strArg1 = std::string(arg1);

	// /cube r_info          -> /cube r_list npc_vnum count vnum,count/vnum,count/...
	// /cube r_info i [n]    -> /cube m_info start count materials (see cube.cpp)
	if (strArg1 == "r_info")
	{
		if (0 == arg2[0])
			cube_request_result_list(ch);
		else
		{
			if (isdigit(*arg2))
			{
				int listIndex = 0, requestCount = 1;
				str_to_number(listIndex, arg2);

				if (0 != arg3[0] && isdigit(*arg3))
					str_to_number(requestCount, arg3);

				cube_request_material_info(ch, listIndex, requestCount);
			}
		}

		return;
	}

	switch (std::tolower(static_cast<unsigned char>(arg1[0])))
	{
		case 'o':
			cube_open(ch);
			break;

		case 'c':
			cube_close(ch);
			break;

		case 'l':
			cube_show_list(ch);
			break;

		case 'a':
			{
				if (0 == arg2[0] || !isdigit(*arg2) ||
					0 == arg3[0] || !isdigit(*arg3))
					return;

				str_to_number(cube_index, arg2);
				str_to_number(inven_index, arg3);
				cube_add_item (ch, cube_index, inven_index);
			}
			break;

		case 'd':
			{
				if (0 == arg2[0] || !isdigit(*arg2))
					return;

				str_to_number(cube_index, arg2);
				cube_delete_item (ch, cube_index);
			}
			break;

		case 'm':
			if (0 != arg2[0])
			{
				while (true == cube_make(ch))
					metin2::log::debug( "Cube: item made");
			}
			else
				cube_make(ch);
			break;

		default:
			return;
	}
}

ACMD(do_in_game_mall)
{
	const char country_code[] = "en";

	char buf[512+1];
	char sas[33];
	const char sas_key[] = "GF9001";

	snprintf(buf, sizeof(buf), "%u%u%s", ch->player_id(), ch->aid(), sas_key);

	static const char hex[] = "0123456789abcdef";
	unsigned char digest[EVP_MAX_MD_SIZE];
	EVP_Digest(buf, strlen(buf), digest, nullptr, EVP_md5(), nullptr);
	int i;
	for (i = 0; i < 16; ++i) {
		sas[i+i] = hex[digest[i] >> 4];
		sas[i+i+1] = hex[digest[i] & 0x0f];
	}
	sas[i+i] = '\0';

	snprintf(buf, sizeof(buf), "mall http://%s/ishop?pid=%u&c=%s&sid=%d&sas=%s",
			metin2::config::game().mall_url.c_str(), ch->player_id(), country_code, metin2::config::game().server_id, sas);

	ch->chat_packet(CHAT_TYPE_COMMAND, buf);
}

ACMD(do_dice) 
{
	char arg1[256], arg2[256];
	int start = 1, end = 100;

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (*arg1 && *arg2)
	{
		start = atoi(arg1);
		end = atoi(arg2);
	}
	else if (*arg1 && !*arg2)
	{
		start = 1;
		end = atoi(arg1);
	}

	end = std::max(start, end);
	start = std::min(start, end);

	int n = number(start, end);

	if (ch->get_party())
		ch->get_party()->chat_packet_to_all_member(CHAT_TYPE_INFO, lc_text("%s님이 주사위를 굴려 %d가 나왔습니다. (%d-%d)"), ch->name(), n, start, end);
	else
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("당신이 주사위를 굴려 %d가 나왔습니다. (%d-%d)"), n, start, end);
}

ACMD(do_click_mall)
{
	ch->chat_packet(CHAT_TYPE_COMMAND, "ShowMeMallPassword");
}

ACMD(do_ride)
{
    if (ch->is_dead() || ch->is_stun())
	return;

    {
	if (ch->is_horse_riding())
	{
	    metin2::log::debug( "Player {} stops riding", ch->name());
	    ch->stop_riding(); 
	    return;
	}

	if (ch->get_mount_vnum())
	{
	    metin2::log::debug( "Player {} unmounts", ch->name());
	    do_unmount(ch, nullptr, 0, 0);
	    return;
	}
    }

    {
	if (ch->horse() != nullptr)
	{
	    metin2::log::debug( "Player {} starts riding", ch->name());
	    ch->start_riding();
	    return;
	}

	for (uint8_t i=0; i<INVENTORY_MAX_NUM; ++i)
	{
		item * item = ch->inventory_item(i);
		if (nullptr == item)
			continue;

		if (item->is_ride_item())
		{
			if (nullptr==ch->wear(WEAR_UNIQUE1) || nullptr==ch->wear(WEAR_UNIQUE2))
			{
				metin2::log::debug( "Player {} rides via unique item {}", ch->name(), item->vnum());
				ch->use_item(item_pos (INVENTORY, i));
				return;
			}
		}

	    switch (item->vnum())
	    {
		case 71114:
		case 71116:
		case 71118:
		case 71120:
		    metin2::log::debug( "Player {} rides via quest item {}", ch->name(), item->vnum());
		    ch->use_item(item_pos (INVENTORY, i));
		    return;
	    }

		if( (item->vnum() > 52000) && (item->vnum() < 52091) )	{
			metin2::log::debug( "Player {} rides via quest item {}", ch->name(), item->vnum());
			ch->use_item(item_pos (INVENTORY, i));
		    return;
		}
	}
    }

    ch->chat_packet(CHAT_TYPE_INFO, lc_text("말을 먼저 소환해주세요."));
}
}
