#include <metin2/config/config.h>
#include <metin2/game/characters/affect.h>
#include <metin2/game/events/arena.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/guilds/building.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/world/dungeon.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/characters/horsename_manager.h>
#include <metin2/game/protocol/input.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/guilds/mark_image.h>
#include <metin2/game/guilds/mark_manager.h>
#include <metin2/game/social/marriage.h>
#include <metin2/game/social/messenger_manager.h>
#include <metin2/game/events/ox_event.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/empire/priv_manager.h>
#include <metin2/game/protocol/protocol.h>
#include <metin2/game/combat/pvp.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/empire/start_position.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/guilds/war_map.h>
#include <metin2/game/social/wedding.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>

namespace metin2::game::inline protocol {

static void send_bonus_info(character * ch)
{
	int	item_drop_bonus = 0;
	int gold_drop_bonus = 0;
	int gold10_drop_bonus	= 0;
	int exp_bonus		= 0;

	item_drop_bonus		= priv_manager::instance().priv(ch, PRIV_ITEM_DROP);
	gold_drop_bonus		= priv_manager::instance().priv(ch, PRIV_GOLD_DROP);
	gold10_drop_bonus	= priv_manager::instance().priv(ch, PRIV_GOLD10_DROP);
	exp_bonus			= priv_manager::instance().priv(ch, PRIV_EXP_PCT);

	if (item_drop_bonus)
	{
		ch->chat_packet(CHAT_TYPE_NOTICE, 
				lc_text("아이템 드롭률  %d%% 추가 이벤트 중입니다."), item_drop_bonus);
	}
	if (gold_drop_bonus)
	{
		ch->chat_packet(CHAT_TYPE_NOTICE, 
				lc_text("골드 드롭률 %d%% 추가 이벤트 중입니다."), gold_drop_bonus);
	}
	if (gold10_drop_bonus)
	{
		ch->chat_packet(CHAT_TYPE_NOTICE, 
				lc_text("대박골드 드롭률 %d%% 추가 이벤트 중입니다."), gold10_drop_bonus);
	}
	if (exp_bonus)
	{
		ch->chat_packet(CHAT_TYPE_NOTICE, 
				lc_text("경험치 %d%% 추가 획득 이벤트 중입니다."), exp_bonus);
	}
}

static auto fn_is_battle_zone(character * ch) -> bool
{
	switch (ch->map_index())
	{
		case 1:
		case 2:
		case 21:
		case 23:
		case 41:
		case 43:
		case 113:
			return false;
	}

	return true;
}

void input_login::login(desc * d, const char * data)
{
	packet_cg_login * pinfo = (packet_cg_login *) data;

	if (d->get_account_table().id)
	{
		metin2::log::error("already logged in (login {})", d->get_account_table().login);
		return;
	}

	char szLogin[sizeof(pinfo->login)];
	copy_packet_string(szLogin, pinfo->login);

	char login[LOGIN_MAX_LEN + 1];
	trim_and_lower(szLogin, login, sizeof(login));

	metin2::log::info( "InputLogin::Login : {}", login);

	packet_gc_login_failure failurePacket;

	if (!metin2::config::test_server())
	{
		failurePacket.header = HEADER_GC_LOGIN_FAILURE;
		strlcpy(failurePacket.status, "VERSION", sizeof(failurePacket.status));
		d->packet(&failurePacket, sizeof(packet_gc_login_failure));
		return;
	}

	if (no_more_client)
	{
		failurePacket.header = HEADER_GC_LOGIN_FAILURE;
		strlcpy(failurePacket.status, "SHUTDOWN", sizeof(failurePacket.status));
		d->packet(&failurePacket, sizeof(packet_gc_login_failure));
		return;
	}

	if (metin2::config::server().user_limit > 0)
	{
		int iTotal;
		int * paiEmpireUserCount;
		int iLocal;

		desc_manager::instance().user_count(iTotal, &paiEmpireUserCount, iLocal);

		if (metin2::config::server().user_limit <= iTotal)
		{
			failurePacket.header = HEADER_GC_LOGIN_FAILURE;
			strlcpy(failurePacket.status, "FULL", sizeof(failurePacket.status));
			d->packet(&failurePacket, sizeof(packet_gc_login_failure));
			return;
		}
	}

	login_packet login_packet;

	strlcpy(login_packet.login, login, sizeof(login_packet.login));
	char szPasswd[sizeof(pinfo->passwd)];
	copy_packet_string(szPasswd, pinfo->passwd);
	strlcpy(login_packet.passwd, szPasswd, sizeof(login_packet.passwd));

	db_clientdesc->db_packet(HEADER_GD_LOGIN, d->handle(), &login_packet, sizeof(proto::login_packet)); 
}

void input_login::login_by_key(desc * d, const char * data)
{
	packet_cg_login2 * pinfo = (packet_cg_login2 *) data;

	if (d->get_account_table().id)
	{
		metin2::log::error("already logged in (login {})", d->get_account_table().login);
		return;
	}

	char szLogin[sizeof(pinfo->login)];
	copy_packet_string(szLogin, pinfo->login);

	char login[LOGIN_MAX_LEN + 1];
	trim_and_lower(szLogin, login, sizeof(login));

	if (no_more_client)
	{
		packet_gc_login_failure failurePacket;

		failurePacket.header = HEADER_GC_LOGIN_FAILURE;
		strlcpy(failurePacket.status, "SHUTDOWN", sizeof(failurePacket.status));
		d->packet(&failurePacket, sizeof(packet_gc_login_failure));
		return;
	}

	if (metin2::config::server().user_limit > 0)
	{
		int iTotal;
		int * paiEmpireUserCount;
		int iLocal;

		desc_manager::instance().user_count(iTotal, &paiEmpireUserCount, iLocal);

		if (metin2::config::server().user_limit <= iTotal)
		{
			packet_gc_login_failure failurePacket;

			failurePacket.header = HEADER_GC_LOGIN_FAILURE;
			strlcpy(failurePacket.status, "FULL", sizeof(failurePacket.status));

			d->packet(&failurePacket, sizeof(packet_gc_login_failure));
			return;
		}
	}

	metin2::log::info( "LOGIN_BY_KEY: {} key {:d}", login, pinfo->login_key);

	d->set_login_key(pinfo->login_key);

	packet_gd_login_by_key ptod;

	strlcpy(ptod.login, login, sizeof(ptod.login));
	ptod.login_key = pinfo->login_key;
	std::memcpy(ptod.client_key, pinfo->client_key, sizeof(uint32_t) * 4);
	strlcpy(ptod.ip, d->host_name(), sizeof(ptod.ip));

	db_clientdesc->db_packet(HEADER_GD_LOGIN_BY_KEY, d->handle(), &ptod, sizeof(packet_gd_login_by_key));
}

void input_login::change_name(desc * d, const char * data)
{
	packet_cg_change_name * p = (packet_cg_change_name *) data;
	const account_table & c_r = d->get_account_table();

	if (!c_r.id)
	{
		metin2::log::error("no account table");
		return;
	}

	if (p->index >= PLAYER_PER_ACCOUNT)
	{
		metin2::log::error("index overflow {:d}, login: {}", p->index, c_r.login);
		return;
	}

	if (!c_r.players[p->index].change_name)
		return;

	char szName[sizeof(p->name)];
	copy_packet_string(szName, p->name);

	if (!check_name(szName))
	{
		packet_gc_create_failure pack;
		pack.header = HEADER_GC_CHARACTER_CREATE_FAILURE;
		pack.type = 0;
		d->packet(&pack, sizeof(pack));
		return;
	}

	packet_gd_change_name pdb;

	pdb.pid = c_r.players[p->index].id;
	strlcpy(pdb.name, szName, sizeof(pdb.name));
	db_clientdesc->db_packet(HEADER_GD_CHANGE_NAME, d->handle(), &pdb, sizeof(packet_gd_change_name));
}

void input_login::character_select(desc * d, const char * data)
{
	struct packet_cg_player_select * pinfo = (struct packet_cg_player_select *) data;
	const account_table & c_r = d->get_account_table();

	metin2::log::info( "player_select: login: {} index: {:d}", c_r.login, pinfo->index);

	if (!c_r.id)
	{
		metin2::log::error("no account table");
		return;
	}

	if (pinfo->index >= PLAYER_PER_ACCOUNT)
	{
		metin2::log::error("index overflow {:d}, login: {}", pinfo->index, c_r.login);
		return;
	}

	if (c_r.players[pinfo->index].change_name)
	{
		metin2::log::error("name must be changed idx {:d}, login {}, name {}", 
				pinfo->index, c_r.login, c_r.players[pinfo->index].name);
		return;
	}

	player_load_packet player_load_packet;

	player_load_packet.account_id	= c_r.id;
	player_load_packet.player_id	= c_r.players[pinfo->index].id;
	player_load_packet.account_index	= pinfo->index;

	db_clientdesc->db_packet(HEADER_GD_PLAYER_LOAD, d->handle(), &player_load_packet, sizeof(proto::player_load_packet));
}

auto race_to_job(unsigned race, unsigned* ret_job) -> bool
{
	*ret_job = 0;

	if (race >= MAIN_RACE_MAX_NUM)
		return false;

	switch (race)
	{
		case MAIN_RACE_WARRIOR_M:
			*ret_job = JOB_WARRIOR;
			break;

		case MAIN_RACE_WARRIOR_W:
			*ret_job = JOB_WARRIOR;
			break;

		case MAIN_RACE_ASSASSIN_M:
			*ret_job = JOB_ASSASSIN;
			break;

		case MAIN_RACE_ASSASSIN_W:
			*ret_job = JOB_ASSASSIN;
			break;

		case MAIN_RACE_SURA_M:
			*ret_job = JOB_SURA;
			break;

		case MAIN_RACE_SURA_W:
			*ret_job = JOB_SURA;
			break;

		case MAIN_RACE_SHAMAN_M:
			*ret_job = JOB_SHAMAN;
			break;

		case MAIN_RACE_SHAMAN_W:
			*ret_job = JOB_SHAMAN;
			break;

		default:
			return false;
			break;
	}
	return true;
}

auto new_player_table2(player_table * table, const char * name, uint8_t race, uint8_t shape, uint8_t empire) -> bool
{
	if (race >= MAIN_RACE_MAX_NUM)
	{
		metin2::log::error("NewPlayerTable2.OUT_OF_RACE_RANGE({:d} >= max({:d}))", race, MAIN_RACE_MAX_NUM);
		return false;
	}

	unsigned job;

	if (!race_to_job(race, &job))
	{	
		metin2::log::error("NewPlayerTable2.RACE_TO_JOB_ERROR({:d})", race);
		return false;
	}

	metin2::log::info( "NewPlayerTable2(name={}, race={:d}, job={:d})", name, race, job); 

	memset(table, 0, sizeof(player_table));

	strlcpy(table->name, name, sizeof(table->name));

	table->level		= 1;
	table->job			= race;
	table->voice		= 0;
	table->part_base	= shape;

	table->st		= job_initial_points[job].st;
	table->dx		= job_initial_points[job].dx;
	table->ht		= job_initial_points[job].ht;
	table->iq		= job_initial_points[job].iq;

	table->hp		= job_initial_points[job].max_hp + table->ht * job_initial_points[job].hp_per_ht;
	table->sp		= job_initial_points[job].max_sp + table->iq * job_initial_points[job].sp_per_iq;
	table->stamina	= job_initial_points[job].max_stamina;

	table->x		= create_start_x(empire) + number(-300, 300);
	table->y		= create_start_y(empire) + number(-300, 300);
	table->z		= 0;
	table->dir		= 0;
	table->playtime = 0;
	table->gold 	= 0;

	table->skill_group = 0;

	return true;
}

void input_login::character_create(desc * d, const char * data)
{
	struct packet_cg_player_create * pinfo = (struct packet_cg_player_create *) data;
	player_create_packet player_create_packet;

	char szName[sizeof(pinfo->name)];
	copy_packet_string(szName, pinfo->name);

	metin2::log::info( "PlayerCreate: name {} pos {:d} job {:d} shape {:d}",
			szName, 
			pinfo->index, 
			pinfo->job, 
			pinfo->shape);

	const account_table & c_rAccountTable = d->get_account_table();

	if (!c_rAccountTable.id)
	{
		metin2::log::error("PlayerCreate: no login data");
		return;
	}

	if (pinfo->index >= PLAYER_PER_ACCOUNT)
	{
		metin2::log::error("PlayerCreate: index overflow {:d}, login: {}", pinfo->index, c_rAccountTable.login);
		return;
	}

	packet_gc_login_failure packFailure;
	memset(&packFailure, 0, sizeof(packFailure));
	packFailure.header = HEADER_GC_CHARACTER_CREATE_FAILURE;

	if (true == metin2::config::game().block_char_creation)
	{
		d->packet(&packFailure, sizeof(packFailure));
		return;
	}

	if (!check_name(szName) || pinfo->shape > 1)
	{

		d->packet(&packFailure, sizeof(packFailure));
		return;
	}

	if (0 == strcmp(c_rAccountTable.login, szName))
	{
		packet_gc_create_failure pack;
		pack.header = HEADER_GC_CHARACTER_CREATE_FAILURE;
		pack.type = 1;

		d->packet(&pack, sizeof(pack));
		return;
	}

	memset(&player_create_packet, 0, sizeof(proto::player_create_packet));

	// job is 16 bits on the wire but stored in a byte.
	if (pinfo->job >= MAIN_RACE_MAX_NUM ||
		!new_player_table2(&player_create_packet.player_table, szName, (uint8_t) pinfo->job, pinfo->shape, d->empire()))
	{
		metin2::log::error("player_prototype error: job {:d} ", pinfo->job);
		d->packet(&packFailure, sizeof(packFailure));
		return;
	}

	trim_and_lower(c_rAccountTable.login, player_create_packet.login, sizeof(player_create_packet.login));
	strlcpy(player_create_packet.passwd, c_rAccountTable.passwd, sizeof(player_create_packet.passwd));

	player_create_packet.account_id	= c_rAccountTable.id;
	player_create_packet.account_index	= pinfo->index;

	metin2::log::info( "PlayerCreate: name {} account_id {:d}, TPlayerCreatePacketSize({:d}), Packet->Gold {:d}",
			szName, 
			pinfo->index, 
			sizeof(proto::player_create_packet),
			player_create_packet.player_table.gold);

	db_clientdesc->db_packet(HEADER_GD_PLAYER_CREATE, d->handle(), &player_create_packet, sizeof(proto::player_create_packet));
}

void input_login::character_delete(desc * d, const char * data)
{
	struct packet_cg_player_delete * pinfo = (struct packet_cg_player_delete *) data;
	const account_table & c_rAccountTable = d->get_account_table();

	if (!c_rAccountTable.id)
	{
		metin2::log::error("PlayerDelete: no login data");
		return;
	}

	char szPrivateCode[sizeof(pinfo->private_code)];
	copy_packet_string(szPrivateCode, pinfo->private_code);

	metin2::log::info( "PlayerDelete: login: {} index: {:d}, social_id {}", c_rAccountTable.login, pinfo->index, szPrivateCode);

	if (pinfo->index >= PLAYER_PER_ACCOUNT)
	{
		metin2::log::error("PlayerDelete: index overflow {:d}, login: {}", pinfo->index, c_rAccountTable.login);
		return;
	}

	if (!c_rAccountTable.players[pinfo->index].id)
	{
		metin2::log::error("PlayerDelete: Wrong Social ID index {:d}, login: {}", pinfo->index, c_rAccountTable.login);
		d->packet(encode_byte(HEADER_GC_CHARACTER_DELETE_WRONG_SOCIAL_ID), 1);
		return;
	}

	player_delete_packet	player_delete_packet;

	trim_and_lower(c_rAccountTable.login, player_delete_packet.login, sizeof(player_delete_packet.login));
	player_delete_packet.player_id	= c_rAccountTable.players[pinfo->index].id;
	player_delete_packet.account_index	= pinfo->index;
	strlcpy(player_delete_packet.private_code, szPrivateCode, sizeof(player_delete_packet.private_code));

	db_clientdesc->db_packet(HEADER_GD_PLAYER_DELETE, d->handle(), &player_delete_packet, sizeof(proto::player_delete_packet));
}

void input_login::entergame(desc * d, const char * data)
{
	character * ch;

	if (!(ch = d->get_character()))
	{
		d->set_phase(PHASE_CLOSE);
		return;
	}

	pixel_position pos = ch->xyz();

	if (!sectree_manager::instance().movable_position(ch->map_index(), pos.x, pos.y, pos))
	{
		pixel_position pos2;
		sectree_manager::instance().recall_position_by_empire(ch->map_index(), ch->empire(), pos2);

		metin2::log::error("!GetMovablePosition (name {} {:d}x{:d} map {:d} changed to {:d}x{:d})", 
				ch->name(),
				pos.x, pos.y,
				ch->map_index(),
				pos2.x, pos2.y);
		pos = pos2;
	}

	guild_manager::instance().login_member(ch);

	ch->show(ch->map_index(), pos.x, pos.y, pos.z);

	sectree_manager::instance().send_npc_position(ch);
	ch->revive_invisible(5);

	d->set_phase(PHASE_GAME);

	if(ch->item_award_cmd())
		quest::quest_manager::instance().item_informer(ch->player_id(),ch->item_award_vnum());

	metin2::log::info( "ENTERGAME: {} {:d}x{:d}x{:d} {} map_index {:d}", 
			ch->name(), ch->x(), ch->y(), ch->z(), d->host_name(), ch->map_index());

	if (ch->horse_level() > 0)
	{
		ch->enter_horse();
	}

	ch->reset_play_time();

	ch->start_save_event();
	ch->start_recovery_event();
	ch->start_check_speed_hack_event();

	pvp_manager::instance().connect(ch);
	pvp_manager::instance().send_list(d);

	messenger_manager::instance().login(ch->name());

	party_manager::instance().set_party(ch);
	guild_manager::instance().send_guild_war(ch);

	building::manager::instance().send_land_list(d, ch->map_index());

	marriage::manager::instance().login(ch);

	packet_gc_time p;
	p.header = HEADER_GC_TIME;
	p.time = get_global_time();
	d->packet(&p, sizeof(p));

	packet_gc_channel p2;
	p2.header = HEADER_GC_CHANNEL;
	p2.channel = metin2::config::server().channel;
	d->packet(&p2, sizeof(p2));

	ch->send_greet_message();

	send_bonus_info(ch);

	for (int i = 0; i <= PREMIUM_MAX_NUM; ++i)
	{
		int remain = ch->premium_remain_seconds(i);

		if (remain <= 0)
			continue;

		ch->add_affect(AFFECT_PREMIUM_START + i, POINT_NONE, 0, 0, remain, 0, true);
		metin2::log::info( "PREMIUM: {} type {:d} {:d}min", ch->name(), i, remain);
	}

	if (check_client_version_enabled)
	{
		int version = atoi(client_version.c_str());
		int date = atoi(d->client_version());

		metin2::log::info( "VERSION CHECK {:d} {:d} {} {}", version, date, client_version.c_str(), d->client_version());

		if (!d->client_version())
		{
			d->delayed_disconnect(10);
		}
		else
		{
			if (version != date)
			{
				ch->chat_packet(CHAT_TYPE_NOTICE, lc_text("클라이언트 버전이 틀려 로그아웃 됩니다. 정상적으로 패치 후 접속하세요."));
				d->delayed_disconnect(10);
				log_manager::instance().hack_log("VERSION_CONFLICT", ch);

				metin2::log::info( "VERSION : WRONG VERSION USER : account:{} name:{} hostName:{} server_version:{} client_version:{}",
						d->get_account_table().login,
						ch->name(),
						d->host_name(),
						client_version.c_str(),
						d->client_version());
			}
		}
	}
	else
	{
		metin2::log::info( "VERSION : NO CHECK");
	}

	if (ch->is_gm() == true)
		ch->chat_packet(CHAT_TYPE_COMMAND, "ConsoleEnable");

	if (ch->map_index() >= 10000)
	{
		if (war_map_manager::instance().is_war_map(ch->map_index()))
			ch->set_war_map(war_map_manager::instance().find(ch->map_index()));
		else if (marriage::wedding_manager::instance().is_wedding_map(ch->map_index()))
			ch->set_wedding_map(marriage::wedding_manager::instance().find(ch->map_index()));
		else {
			ch->set_dungeon(dungeon_manager::instance().find_by_map_index(ch->map_index()));
		}
	}
	else if (arena_manager::instance().is_arena_map(ch->map_index()) == true)
	{
		int memberFlag = arena_manager::instance().is_member(ch->map_index(), ch->player_id());
		if (memberFlag == MEMBER_OBSERVER)
		{
			ch->set_observer_mode(true);
			ch->set_arena_observer_mode(true);
			if (arena_manager::instance().register_observer_ptr(ch, ch->map_index(), ch->x()/100, ch->y()/100))
			{
				metin2::log::info( "ARENA : Observer add failed");
			}

			if (ch->is_horse_riding() == true)
			{
				ch->stop_riding();
				ch->horse_summon(false);
			}
		}
		else if (memberFlag == MEMBER_DUELIST)
		{
			packet_gc_duel_start duelStart;
			duelStart.header = HEADER_GC_DUEL_START;
			duelStart.size = sizeof(packet_gc_duel_start);

			ch->get_desc()->packet(&duelStart, sizeof(packet_gc_duel_start));

			if (ch->is_horse_riding() == true)
			{
				ch->stop_riding();
				ch->horse_summon(false);
			}

			party * pParty = ch->get_party();
			if (pParty != nullptr)
			{
				if (pParty->member_count() == 2)
				{
					party_manager::instance().delete_party(pParty);
				}
				else
				{
					pParty->quit(ch->player_id());
				}
			}
		}
		else if (memberFlag == MEMBER_NO)		
		{
			if (ch->gm_level() == GM_PLAYER)
				ch->warp_set(empire_start_x(ch->empire()), empire_start_y(ch->empire()));
		}
		else
		{
		}
	}
	else if (ch->map_index() == 113)
	{
		if (ox_event_manager::instance().enter(ch) == false)
		{
			if (ch->gm_level() == GM_PLAYER)
				ch->warp_set(empire_start_x(ch->empire()), empire_start_y(ch->empire()));
		}
	}
	else
	{
		if (war_map_manager::instance().is_war_map(ch->map_index()) ||
				marriage::wedding_manager::instance().is_wedding_map(ch->map_index()))
		{
			if (!metin2::config::test_server())
				ch->warp_set(empire_start_x(ch->empire()), empire_start_y(ch->empire()));
		}
	}

	if (ch->horse_level() > 0)
	{
		uint32_t pid = ch->player_id();

		if (pid != 0 && horse_name_manager::instance().horse_name(pid) == nullptr)
			db_clientdesc->db_packet(HEADER_GD_REQ_HORSE_NAME, 0, &pid, sizeof(uint32_t));
	}

	if (metin2::config::game().notice_battle_zone)
	{
		if (fn_is_battle_zone(ch))
		{
			ch->chat_packet(CHAT_TYPE_NOTICE, lc_text("이 맵에선 강제적인 대전이 있을수 도 있습니다."));
			ch->chat_packet(CHAT_TYPE_NOTICE, lc_text("이 조항에 동의하지 않을시"));
			ch->chat_packet(CHAT_TYPE_NOTICE, lc_text("본인의 주성 및 부성으로 돌아가시기 바랍니다."));
		}
	}
}

void input_login::empire(desc * d, const char * data)
{
	const packet_cg_empire* p = reinterpret_cast<const packet_cg_empire*>(data);

	if (p->empire == 0 || EMPIRE_MAX_NUM <= p->empire)
	{
		d->set_phase(PHASE_CLOSE);
		return;
	}

	const account_table& r = d->get_account_table();

	if (!r.id)
	{
		metin2::log::error("EmpireSelect: no login data");
		return;
	}

	if (r.empire != 0)
	{
		for (int i = 0; i < PLAYER_PER_ACCOUNT; ++i)
		{
			if (0 != r.players[i].id)
			{
				metin2::log::error("EmpireSelectFailed {:d}", r.players[i].id);
				return;
			}
		}
	}

	empire_select_packet pd;

	pd.account_id = r.id;
	pd.empire = p->empire;

	db_clientdesc->db_packet(HEADER_GD_EMPIRE_SELECT, d->handle(), &pd, sizeof(pd));
}

static auto is_mark_owner_guild_master(desc * d, uint32_t guild_id) -> bool
{
	desc * pkOwner = desc_manager::instance().find_by_handle(d->mark_owner_handle());
	character * ch = pkOwner ? pkOwner->get_character() : nullptr;

	return ch && ch->get_guild() && ch->get_guild()->id() == guild_id && ch->get_guild()->master_pid() == ch->player_id();
}

auto input_login::guild_symbol_upload(desc * d, const char* data, size_t bytes) -> int
{
	if (bytes < sizeof(packet_cg_guild_symbol_upload))
		return -1;

	metin2::log::info( "GuildSymbolUpload uiBytes {:d}", bytes);

	packet_cg_guild_symbol_upload* p = (packet_cg_guild_symbol_upload*) data;

	if (bytes < p->size)
		return -1;

	int iSymbolSize = p->size - sizeof(packet_cg_guild_symbol_upload);

	if (iSymbolSize <= 0 || iSymbolSize > 64 * 1024)
	{
		d->set_phase(PHASE_CLOSE);
		return 0;
	}

	if (!is_mark_owner_guild_master(d, p->guild_id))
	{
		metin2::log::error("GuildSymbolUpload: guild {:d} not uploaded by its master, host {}", p->guild_id, d->host_name());
		d->set_phase(PHASE_CLOSE);
		return 0;
	}

	if (!metin2::config::test_server())
		if (!building::manager::instance().find_land_by_guild(p->guild_id))
		{
			d->set_phase(PHASE_CLOSE);
			return 0;
		}

	metin2::log::info( "GuildSymbolUpload Do Upload {:02X}{:02X}{:02X}{:02X} {:d}", data[7], data[8], data[9], data[10], sizeof(*p));

	guild_mark_manager::instance().upload_symbol(p->guild_id, iSymbolSize, (const uint8_t*)(data + sizeof(*p)));
	guild_mark_manager::instance().save_symbol(GUILD_SYMBOL_FILENAME);
	return iSymbolSize;
}

void input_login::guild_symbol_crc(desc * d, const char* data)
{
	const packet_cg_symbol_crc & CGPacket = *((packet_cg_symbol_crc *) data);

	metin2::log::info( "GuildSymbolCRC {:d} {:d} {:d}", CGPacket.guild_id, CGPacket.crc, CGPacket.size);

	const guild_mark_manager::guild_symbol * pkGS = guild_mark_manager::instance().get_guild_symbol(CGPacket.guild_id);

	if (!pkGS || pkGS->raw.empty())
		return;

	metin2::log::info( "  Server {:d} {:d}", pkGS->crc, pkGS->raw.size());

	if (pkGS->raw.size() != CGPacket.size || pkGS->crc != CGPacket.crc)
	{
		packet_gc_guild_symbol_data GCPacket;

		GCPacket.header = HEADER_GC_SYMBOL_DATA;
		GCPacket.size = sizeof(GCPacket) + pkGS->raw.size();
		GCPacket.guild_id = CGPacket.guild_id;

		d->buffered_packet(&GCPacket, sizeof(GCPacket));
		d->packet(&pkGS->raw[0], pkGS->raw.size());

		if (pkGS->raw.size() >= 4)
			metin2::log::info( "SendGuildSymbolHead {:02X}{:02X}{:02X}{:02X} Size {:d}",
					pkGS->raw[0], pkGS->raw[1], pkGS->raw[2], pkGS->raw[3], pkGS->raw.size());
	}
}

void input_login::guild_mark_upload(desc * d, const char* data)
{
	packet_cg_mark_upload * p = (packet_cg_mark_upload *) data;
	guild_manager& rkGuildMgr = guild_manager::instance();
	guild * pkGuild;

	if (!(pkGuild = rkGuildMgr.find_guild(p->gid)))
	{
		metin2::log::error("MARK_SERVER: GuildMarkUpload: no guild. gid {:d}", p->gid);
		return;
	}

	if (!is_mark_owner_guild_master(d, p->gid))
	{
		metin2::log::error("MARK_SERVER: GuildMarkUpload: gid {:d} not uploaded by its master, host {}", p->gid, d->host_name());
		d->set_phase(PHASE_CLOSE);
		return;
	}

	if (pkGuild->level() < metin2::config::game().mark.min_level)
	{
		metin2::log::info( "MARK_SERVER: GuildMarkUpload: level < {:d} ({:d})", metin2::config::game().mark.min_level, pkGuild->level());
		return;
	}

	guild_mark_manager & rkMarkMgr = guild_mark_manager::instance();

	metin2::log::info( "MARK_SERVER: GuildMarkUpload: gid {:d}", p->gid);

	bool isEmpty = true;

	for (uint32_t iPixel = 0; iPixel < guild_mark::SIZE; ++iPixel)
		if (*((uint32_t *) p->image + iPixel) != 0x00000000)
			isEmpty = false;

	if (isEmpty)
		rkMarkMgr.delete_mark(p->gid);
	else
		rkMarkMgr.save_mark(p->gid, p->image);
}

void input_login::guild_mark_idx_list(desc * d, const char* data)
{
	guild_mark_manager & rkMarkMgr = guild_mark_manager::instance();

	uint32_t bufSize = sizeof(uint16_t) * 2 * rkMarkMgr.mark_count();
	char * buf = nullptr;

	if (bufSize > 0)
	{
		buf = (char *) malloc(bufSize);
		rkMarkMgr.copy_mark_idx(buf);
	}

	packet_gc_mark_idx_list p;
	p.header = HEADER_GC_MARK_IDXLIST;
	p.buf_size = sizeof(p) + bufSize;
	p.count = rkMarkMgr.mark_count();

	if (buf)
	{
		d->buffered_packet(&p, sizeof(p));
		d->large_packet(buf, bufSize);
		free(buf);
	}
	else
		d->packet(&p, sizeof(p));

	metin2::log::info( "MARK_SERVER: GuildMarkIDXList {:d} bytes sent.", p.buf_size);
}

void input_login::guild_mark_crc_list(desc * d, const char* data)
{
	packet_cg_mark_crc_list * pCG = (packet_cg_mark_crc_list *) data;

	std::map<uint8_t, const guild_mark_block *> mapDiffBlocks;
	guild_mark_manager::instance().diff_blocks(pCG->img_idx, pCG->crclist, mapDiffBlocks);

	uint32_t blockCount = 0;
	temp_buffer buf(1024 * 1024);

	for (auto it = mapDiffBlocks.begin(); it != mapDiffBlocks.end(); ++it)
	{
		uint8_t posBlock = it->first;
		const guild_mark_block & rkBlock = *it->second;

		buf.write(&posBlock, sizeof(uint8_t));
		const uint32_t dwCompSize = static_cast<uint32_t>(rkBlock.size_comp_buf);
		buf.write(&dwCompSize, sizeof(uint32_t));
		buf.write(rkBlock.comp_buf, rkBlock.size_comp_buf);

		++blockCount;
	}

	packet_gc_mark_block pGC;

	pGC.header = HEADER_GC_MARK_BLOCK;
	pGC.img_idx = pCG->img_idx;
	pGC.buf_size = buf.size() + sizeof(packet_gc_mark_block);
	pGC.count = blockCount;

	metin2::log::info( "MARK_SERVER: Sending blocks. (imgIdx {:d} diff {:d} size {:d})", pCG->img_idx, mapDiffBlocks.size(), pGC.buf_size);

	if (buf.size() > 0)
	{
		d->buffered_packet(&pGC, sizeof(packet_gc_mark_block));
		d->large_packet(buf.read_peek(), buf.size());
	}
	else
		d->packet(&pGC, sizeof(packet_gc_mark_block));
}

auto input_login::analyze(desc * d, uint8_t header, const char * data) -> int
{
	int iExtraLen = 0;

	switch (header)
	{
		case HEADER_CG_PONG:
		case HEADER_CG_TIME_SYNC:
			break;

		// Mark packets come only over the unencrypted mark connection and must not reach anything else.
		case HEADER_CG_MARK_CRCLIST:
		case HEADER_CG_MARK_IDXLIST:
		case HEADER_CG_MARK_UPLOAD:
		case HEADER_CG_GUILD_SYMBOL_UPLOAD:
		case HEADER_CG_SYMBOL_CRC:
			if (!d->is_mark_login())
			{
				metin2::log::error("guild mark packet {:d} without mark login, host {}", header, d->host_name());
				d->set_phase(PHASE_CLOSE);
				return 0;
			}
			break;

		default:
			if (d->is_mark_login())
			{
				metin2::log::error("mark connection sent packet {:d}, host {}", header, d->host_name());
				d->set_phase(PHASE_CLOSE);
				return 0;
			}
			break;
	}

	switch (header)
	{
		case HEADER_CG_CHARACTER_SELECT:
		case HEADER_CG_CHARACTER_CREATE:
		case HEADER_CG_CHARACTER_DELETE:
		case HEADER_CG_CHANGE_NAME:
		case HEADER_CG_EMPIRE:
			if (!d->is_phase(PHASE_SELECT) || d->get_character())
			{
				metin2::log::error("packet {:d} outside of character select, host {}", header, d->host_name());
				return 0;
			}
			break;

		case HEADER_CG_LOGIN:
		case HEADER_CG_LOGIN2:
			if (!d->is_phase(PHASE_LOGIN))
			{
				metin2::log::error("login packet {:d} outside of the login phase, host {}", header, d->host_name());
				return 0;
			}
			break;
	}

	switch (header)
	{
		case HEADER_CG_PONG:
			pong(d);
			break;

		case HEADER_CG_TIME_SYNC:
			handshake(d, data);
			break;

		case HEADER_CG_LOGIN:
			login(d, data);
			break;

		case HEADER_CG_LOGIN2:
			login_by_key(d, data);
			break;

		case HEADER_CG_CHARACTER_SELECT:
			character_select(d, data);
			break;

		case HEADER_CG_CHARACTER_CREATE:
			character_create(d, data);
			break;

		case HEADER_CG_CHARACTER_DELETE:
			character_delete(d, data);
			break;

		case HEADER_CG_ENTERGAME:
			entergame(d, data);
			break;

		case HEADER_CG_EMPIRE:
			empire(d, data);
			break;

		case HEADER_CG_MOVE:
			break;

		case HEADER_CG_MARK_CRCLIST:
			guild_mark_crc_list(d, data);
			break;

		case HEADER_CG_MARK_IDXLIST:
			guild_mark_idx_list(d, data);
			break;

		case HEADER_CG_MARK_UPLOAD:
			guild_mark_upload(d, data);
			break;

		case HEADER_CG_GUILD_SYMBOL_UPLOAD:
			if ((iExtraLen = guild_symbol_upload(d, data, buffer_left_)) < 0)
				return -1;
			break;

		case HEADER_CG_SYMBOL_CRC:
			guild_symbol_crc(d, data);
			break;

		case HEADER_CG_HACK:
			break;

		case HEADER_CG_CHANGE_NAME:
			change_name(d, data);
			break;

		case HEADER_CG_CLIENT_VERSION:
			version(d->get_character(), data);
			break;

		case HEADER_CG_CLIENT_VERSION2:
			version(d->get_character(), data);
			break;

		default:
			metin2::log::error("login phase does not handle this packet! header {:d}", header);
			return (0);
	}

	return (iExtraLen);
}
}
