#include <metin2/game/text/banword.h>
#include <metin2/game/guilds/building.h>
#include <metin2/game/empire/castle.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/config.h>
#include <metin2/game/app/constants.h>
#include <metin2/game/items/cube.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/items/dragon_soul.h>
#include <metin2/game/app/event.h>
#include <metin2/game/admin/gm.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/characters/horsename_manager.h>
#include <metin2/game/protocol/input.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/world/map_location.h>
#include <metin2/game/social/marriage.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/empire/monarch.h>
#include <metin2/game/characters/motion.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/parties/party.h>
#include <metin2/game/empire/priv_manager.h>
#include <metin2/game/protocol/protocol.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/quest/questpc.h>
#include <metin2/game/items/refine.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/trade/shop_manager.h>
#include <metin2/game/combat/skill.h>
#include <metin2/game/empire/start_position.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/items/unique_item.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/social/wedding.h>

#include <metin2/proto/building.h>
#include <metin2/proto/length.h>
#include <metin2/common/startup.h>
#include <metin2/proto/tables.h>
#include <metin2/core/core_signal.h>
#include <metin2/log/log.h>
#include <metin2/core/main.h>
#include <metin2/core/utils.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>

namespace metin2::game::inline protocol {


bool get_server_location(account_table & tab, uint8_t empire_value)
{
	bool bFound = false;

	for (int i = 0; i < PLAYER_PER_ACCOUNT; ++i)
	{
		if (0 == tab.players[i].id)
			continue;

		bFound = true;
		int32_t lIndex = 0;

		if (!map_locations::instance().get(tab.players[i].x,
					tab.players[i].y,
					lIndex,
					tab.players[i].addr,
					tab.players[i].port))
		{
			metin2::log::error("location error name {} mapindex {:d} {:d} x {:d} empire {:d}",
					tab.players[i].name, lIndex, tab.players[i].x, tab.players[i].y, tab.empire);

			tab.players[i].x = empire_start_x(tab.empire);
			tab.players[i].y = empire_start_y(tab.empire);

			lIndex = 0;

			if (!map_locations::instance().get(tab.players[i].x, tab.players[i].y, lIndex, tab.players[i].addr, tab.players[i].port))
			{
				metin2::log::error("cannot find server for mapindex {:d} {:d} x {:d} (name {})", 
						lIndex,
						tab.players[i].x,
						tab.players[i].y,
						tab.players[i].name);

				if (!proxy_ip.empty())
					tab.players[i].addr=inet_addr(proxy_ip.c_str());

				continue;
			}
		}

		struct in_addr in;

		if (!proxy_ip.empty())
			tab.players[i].addr=inet_addr(proxy_ip.c_str());

		in.s_addr = tab.players[i].addr;
		metin2::log::info( "success to {}:{:d}", inet_ntoa(in), tab.players[i].port);
	}

	return bFound;
}


void input_db::login_success(uint32_t handle, const char *data)
{
	metin2::log::info( "LoginSuccess");

	account_table * pTab = (account_table *) data;

	desc * d = desc_manager::instance().find_by_handle(handle);

	if (!d)
	{
		metin2::log::info( "CInputDB::LoginSuccess - cannot find handle [{}]", pTab->login);

		logout_packet pack;

		strlcpy(pack.login, pTab->login, sizeof(pack.login));
		db_clientdesc->db_packet(HEADER_GD_LOGOUT, handle, &pack, sizeof(pack));
		return;
	}

	if (strcmp(pTab->status, "OK")) // if not OK
	{
		metin2::log::info( "CInputDB::LoginSuccess - status[{}] is not OK [{}]", pTab->status, pTab->login);

		logout_packet pack;

		strlcpy(pack.login, pTab->login, sizeof(pack.login));
		db_clientdesc->db_packet(HEADER_GD_LOGOUT, handle, &pack, sizeof(pack));

		login_failure(d, pTab->status);
		return;
	}

	for (int i = 0; i != PLAYER_PER_ACCOUNT; ++i)
	{
		simple_player& player = pTab->players[i];
		metin2::log::info( "\tplayer({}).job({:d})", player.name, player.job);
	}

	bool bFound = get_server_location(*pTab, pTab->empire);

	d->bind_account_table(pTab);

	if (!bFound) // if there's no character, send to a random empire.. -_-
	{
		packet_gc_empire pe;
		pe.header = HEADER_GC_EMPIRE;
		pe.empire = number(1, 3);
		d->packet(&pe, sizeof(pe));
	}
	else
	{
		packet_gc_empire pe;
		pe.header = HEADER_GC_EMPIRE;
		pe.empire = d->empire();
		d->packet(&pe, sizeof(pe));
	}

	d->set_phase(PHASE_SELECT);
	d->send_login_success_packet();

	metin2::log::info( "InputDB::login_success: {}", pTab->login);
}

void input_db::player_create_failure(desc * d, uint8_t type_value)
{
	if (!d)
		return;

	packet_gc_create_failure pack;

	pack.header	= HEADER_GC_CHARACTER_CREATE_FAILURE;
	pack.type	= type_value;

	d->packet(&pack, sizeof(pack));
}

void input_db::player_create_success(desc * d, const char * data)
{
	if (!d)
		return;

	packet_dg_create_success * pPacketDB = (packet_dg_create_success *) data;

	if (pPacketDB->account_character_index >= PLAYER_PER_ACCOUNT)
	{
		d->packet(encode_byte(HEADER_GC_CHARACTER_CREATE_FAILURE), 1);
		return;
	}

	int32_t lIndex = 0;

	if (!game::world::map_locations::instance().get(pPacketDB->player.x,
				pPacketDB->player.y,
				lIndex,
				pPacketDB->player.addr,
				pPacketDB->player.port))
	{
		metin2::log::error("InputDB::PlayerCreateSuccess: cannot find server for mapindex {:d} {:d} x {:d} (name {})", 
				lIndex,
				pPacketDB->player.x,
				pPacketDB->player.y,
				pPacketDB->player.name);
	}

	account_table & r_Tab = d->get_account_table();
	r_Tab.players[pPacketDB->account_character_index] = pPacketDB->player;

	packet_gc_player_create_success pack;

	pack.header = HEADER_GC_CHARACTER_CREATE_SUCCESS;
	pack.account_character_index = pPacketDB->account_character_index;
	pack.player = pPacketDB->player;

	if (!proxy_ip.empty())
		pack.player.addr=inet_addr(proxy_ip.c_str());

	d->packet(&pack, sizeof(packet_gc_player_create_success));

	log_manager::instance().char_log(pack.player.id, 0, 0, 0, "CREATE PLAYER", "", d->host_name());
}

void input_db::player_delete_success(desc * d, const char * data)
{
	if (!d)
		return;

	uint8_t account_index;
	account_index = decode_byte(data);
	d->buffered_packet(encode_byte(HEADER_GC_CHARACTER_DELETE_SUCCESS),	1);
	d->packet(encode_byte(account_index),			1);

	d->get_account_table().players[account_index].id = 0;
}

void input_db::player_delete_fail(desc * d)
{
	if (!d)
		return;

	d->packet(encode_byte(HEADER_GC_CHARACTER_DELETE_WRONG_SOCIAL_ID),	1);

}

void input_db::change_name(desc * d, const char * data)
{
	if (!d)
		return;

	packet_dg_change_name * p = (packet_dg_change_name *) data;

	account_table & r = d->get_account_table();

	if (!r.id)
		return;

	for (size_t i = 0; i < PLAYER_PER_ACCOUNT; ++i)
		if (r.players[i].id == p->pid)
		{
			strlcpy(r.players[i].name, p->name, sizeof(r.players[i].name));
			r.players[i].change_name = 0;

			packet_gc_change_name pgc;

			pgc.header = HEADER_GC_CHANGE_NAME;
			pgc.pid = p->pid;
			strlcpy(pgc.name, p->name, sizeof(pgc.name));

			d->packet(&pgc, sizeof(packet_gc_change_name));
			break;
		}
}

void input_db::player_load(desc * d, const char * data)
{
	player_table * pTab = (player_table *) data;

	if (!d)
		return;

	int32_t lMapIndex = pTab->map_index;
	pixel_position pos;

	if (lMapIndex == 0)
	{
		lMapIndex = sectree_manager::instance().map_index(pTab->x, pTab->y);

		if (lMapIndex == 0) // coordinates not found.
		{
			lMapIndex = empire_start_map(d->get_account_table().empire);
			pos.x = empire_start_x(d->get_account_table().empire);
			pos.y = empire_start_y(d->get_account_table().empire);
		}
		else
		{
			pos.x = pTab->x;
			pos.y = pTab->y;
		}
	}
	pTab->map_index = lMapIndex;

	// if they were on a private map and the private map is gone, they must return to the exit.
	// ----
	// but if they must go to the exit... why look up the public map position matching the private map instead of the exit...
	// I don't know the history... so hardcoding again.
	// if it's the Demon's Cave, go to the exit...
	// by rtsummit
	if (!sectree_manager::instance().valid_location(pTab->map_index, pTab->x, pTab->y, lMapIndex, pos, d->empire()))
	{
		metin2::log::error("InputDB::PlayerLoad : cannot find valid location {:d} x {:d} (name: {})", pTab->x, pTab->y, pTab->name);
		d->set_phase(PHASE_CLOSE);
		return;
	}

	pTab->x = pos.x;
	pTab->y = pos.y;
	pTab->map_index = lMapIndex;

	if (d->get_character() || d->is_phase(PHASE_GAME))
	{
		character * p = d->get_character();
		metin2::log::error("login state already has main state (character {} {})", p->name(), static_cast<const void *>(p));
		return;
	}

	if (nullptr != character_manager::instance().find_pc(pTab->name))
	{
		metin2::log::error("InputDB: PlayerLoad : {} already exist in game", pTab->name);
		return;
	}

	character * ch = character_manager::instance().create_character(pTab->name, pTab->id);

	ch->bind_desc(d);
	ch->set_player_proto(pTab);
	ch->set_empire(d->empire());

	d->bind_character(ch);
	
	{
		// P2P Login
		packet_gg_login p;

		p.header = HEADER_GG_LOGIN;
		strlcpy(p.name, ch->name(), sizeof(p.name));
		p.pid = ch->player_id();
		p.empire = ch->empire();
		p.map_index = sectree_manager::instance().map_index(ch->x(), ch->y());
		p.channel = metin2::config::server().channel;

		p2_p_manager::instance().send(&p, sizeof(packet_gg_login));

		char buf[51];
		snprintf(buf, sizeof(buf), "%s %d %d %d %d",
				ch->get_desc()->host_name(), ch->gold(), metin2::config::server().channel, ch->map_index(), ch->alignment());
		log_manager::instance().char_log(ch, 0, "LOGIN", buf);

	}

	d->set_phase(PHASE_LOADING);
	ch->main_character_packet();

	int32_t lPublicMapIndex = lMapIndex >= 10000 ? lMapIndex / 10000 : lMapIndex;

	//if (!map_allow_find(lMapIndex >= 10000 ? lMapIndex / 10000 : lMapIndex) || !CheckEmpire(ch, lMapIndex))
	if (!map_allow_find(lPublicMapIndex))
	{
		metin2::log::error("InputDB::PlayerLoad : entering {:d} map is not allowed here (name: {}, empire {:d})", 
				lMapIndex, pTab->name, d->empire());

		ch->set_warp_location(empire_start_map(d->empire()),
				empire_start_x(d->empire()) / 100,
				empire_start_y(d->empire()) / 100);

		d->set_phase(PHASE_CLOSE);
		return;
	}

	quest::quest_manager::instance().broadcast_event_flag_on_login(ch);

	for (int i = 0; i < QUICKSLOT_MAX_NUM; ++i)
		ch->set_quickslot(i, pTab->quickslot[i]);

	ch->points_packet();
	ch->skill_level_packet();

	metin2::log::info( "InputDB: player_load {} {:d}x{:d}x{:d} LEVEL {:d} MOV_SPEED {:d} JOB {:d} ATG {:d} DFG {:d} GMLv {:d}",
			pTab->name, 
			ch->x(), ch->y(), ch->z(),
			ch->level(),
			ch->point(POINT_MOV_SPEED),
			ch->job(),
			ch->point(POINT_ATT_GRADE),
			ch->point(POINT_DEF_GRADE),
			ch->gm_level());

	ch->query_safebox_size();

}

void input_db::boot(const char* data)
{
	signal_timer_disable();

	// packet size check
	uint32_t dwPacketSize = decode_4bytes(data);
	data += 4;

	// packet version check
	uint8_t bVersion = decode_byte(data);
	data += 1;

	metin2::log::info( "BOOT: PACKET: {:d}", dwPacketSize);
	metin2::log::info( "BOOT: VERSION: {:d}", bVersion);
	if (bVersion != 6)
	{
		metin2::log::error("boot version error");
		thecore_shutdown();
	}

	metin2::log::info( "sizeof(TMobTable) = {:d}", sizeof(mob_table));
	metin2::log::info( "sizeof(TItemTable) = {:d}", sizeof(item_table));
	metin2::log::info( "sizeof(TShopTable) = {:d}", sizeof(shop_table));
	metin2::log::info( "sizeof(TSkillTable) = {:d}", sizeof(skill_table));
	metin2::log::info( "sizeof(TRefineTable) = {:d}", sizeof(refine_table));
	metin2::log::info( "sizeof(TItemAttrTable) = {:d}", sizeof(item_attr_table));
	metin2::log::info( "sizeof(TItemRareTable) = {:d}", sizeof(item_attr_table));
	metin2::log::info( "sizeof(TBanwordTable) = {:d}", sizeof(banword_table));
	metin2::log::info( "sizeof(TLand) = {:d}", sizeof(building::land_table));
	metin2::log::info( "sizeof(TObjectProto) = {:d}", sizeof(building::object_proto));
	metin2::log::info( "sizeof(TObject) = {:d}", sizeof(building::object_table));
	//ADMIN_MANAGER
	metin2::log::info( "sizeof(TAdminManager) = {:d}", sizeof (t_admin_info) );
	//END_ADMIN_MANAGER

	uint16_t size;

	/*
	 * MOB
	 */

	if (decode_2bytes(data)!=sizeof(mob_table))
	{
		metin2::log::error("mob table size error");
		thecore_shutdown();
		return;
	}
	data += 2;

	size = decode_2bytes(data);
	data += 2;
	metin2::log::info( "BOOT: MOB: {:d}", size);

	if (size)
	{
		mob_manager::instance().initialize((mob_table *) data, size);
		data += size * sizeof(mob_table);
	}

	/*
	 * ITEM
	 */

	if (decode_2bytes(data) != sizeof(item_table))
	{
		metin2::log::error("item table size error");
		thecore_shutdown();
		return;
	}
	data += 2;

	size = decode_2bytes(data);
	data += 2;
	metin2::log::info( "BOOT: ITEM: {:d}", size);


	if (size)
	{
		item_manager::instance().initialize((item_table *) data, size);
		data += size * sizeof(item_table);
	}

	/*
	 * SHOP
	 */

	if (decode_2bytes(data) != sizeof(shop_table))
	{
		metin2::log::error("shop table size error");
		thecore_shutdown();
		return;
	}
	data += 2;

	size = decode_2bytes(data);
	data += 2;
	metin2::log::info( "BOOT: SHOP: {:d}", size);


	if (size)
	{
		if (!shop_manager::instance().initialize((shop_table *) data, size))
		{
			metin2::log::error("shop table Initialize error");
			thecore_shutdown();
			return;
		}
		data += size * sizeof(shop_table);
	}

	/*
	 * SKILL
	 */

	if (decode_2bytes(data) != sizeof(skill_table))
	{
		metin2::log::error("skill table size error");
		thecore_shutdown();
		return;
	}
	data += 2;

	size = decode_2bytes(data);
	data += 2;
	metin2::log::info( "BOOT: SKILL: {:d}", size);

	if (size)
	{
		if (!skill_manager::instance().initialize((skill_table *) data, size))
		{
			metin2::log::error("cannot initialize skill table");
			thecore_shutdown();
			return;
		}

		data += size * sizeof(skill_table);
	}
	/*
	 * REFINE RECIPE
	 */
	if (decode_2bytes(data) != sizeof(refine_table))
	{
		metin2::log::error("refine table size error");
		thecore_shutdown();
		return;
	}
	data += 2;

	size = decode_2bytes(data);
	data += 2;
	metin2::log::info( "BOOT: REFINE: {:d}", size);

	if (size)
	{
		refine_manager::instance().initialize((refine_table*) data, size);
		data += size * sizeof(refine_table);
	}

	/*
	 * ITEM ATTR
	 */
	if (decode_2bytes(data) != sizeof(item_attr_table))
	{
		metin2::log::error("item attr table size error");
		thecore_shutdown();
		return;
	}
	data += 2;

	size = decode_2bytes(data);
	data += 2;
	metin2::log::info( "BOOT: ITEM_ATTR: {:d}", size);

	if (size)
	{
		item_attr_table * p = (item_attr_table *) data;

		for (int i = 0; i < size; ++i, ++p)
		{
			if (p->apply_index >= MAX_APPLY_NUM)
				continue;

			map_item_attr[p->apply_index] = *p;
			metin2::log::info( "ITEM_ATTR[{:d}]: {} {:d}", p->apply_index, p->apply, p->prob);
		}
	}

	data += size * sizeof(item_attr_table);


	/*
     * ITEM RARE
     */
	if (decode_2bytes(data) != sizeof(item_attr_table))
	{
		metin2::log::error("item rare table size error");
		thecore_shutdown();
		return;
	}
	data += 2;

	size = decode_2bytes(data);
	data += 2;
	metin2::log::info( "BOOT: ITEM_RARE: {:d}", size);

	if (size)
	{
		item_attr_table * p = (item_attr_table *) data;

		for (int i = 0; i < size; ++i, ++p)
		{
			if (p->apply_index >= MAX_APPLY_NUM)
				continue;

			map_item_rare[p->apply_index] = *p;
			metin2::log::info( "ITEM_RARE[{:d}]: {} {:d}", p->apply_index, p->apply, p->prob);
		}
	}

	data += size * sizeof(item_attr_table);


	/*
	 * BANWORDS
	 */

	if (decode_2bytes(data) != sizeof(banword_table))
	{
		metin2::log::error("ban word table size error");
		thecore_shutdown();
		return;
	}
	data += 2;

	size = decode_2bytes(data);
	data += 2;

	banword_manager::instance().initialize((banword_table *) data, size);
	data += size * sizeof(banword_table);

	{
		using namespace building;

		/*
		 * LANDS
		 */

		if (decode_2bytes(data) != sizeof(land_table))
		{
			metin2::log::error("land table size error");
			thecore_shutdown();
			return;
		}
		data += 2;

		size = decode_2bytes(data);
		data += 2;

		land_table * kLand = (land_table *) data;
		data += size * sizeof(land_table);

		for (uint16_t i = 0; i < size; ++i, ++kLand)
			manager::instance().load_land(kLand);

		/*
		 * OBJECT PROTO
		 */

		if (decode_2bytes(data) != sizeof(object_proto))
		{
			metin2::log::error("object proto table size error");
			thecore_shutdown();
			return;
		}
		data += 2;

		size = decode_2bytes(data);
		data += 2;

		manager::instance().load_object_proto((object_proto *) data, size);
		data += size * sizeof(object_proto);

		/*
		 * OBJECT 
		 */
		if (decode_2bytes(data) != sizeof(object_table))
		{
			metin2::log::error("object table size error");
			thecore_shutdown();
			return;
		}
		data += 2;

		size = decode_2bytes(data);
		data += 2;

		object_table * kObj = (object_table *) data;
		data += size * sizeof(object_table);

		for (uint16_t i = 0; i < size; ++i, ++kObj)
			manager::instance().load_object(kObj, true);
	}
	int32_t server_time;
    memcpy(&server_time, data, sizeof(server_time));
    set_global_time(server_time);
	data += sizeof(int32_t);

	if (decode_2bytes(data) != sizeof(item_id_range_table) )
	{
		metin2::log::error("ITEM ID RANGE size error");
		thecore_shutdown();
		return;
	}
	data += 2;

	size = decode_2bytes(data);
	data += 2;

	item_id_range_table* range = (item_id_range_table*) data;
	data += size * sizeof(item_id_range_table);

	item_id_range_table* rangespare = (item_id_range_table*) data;
	data += size * sizeof(item_id_range_table);

	//ADMIN_MANAGER
	//register admin
	int ChunkSize = decode_2bytes(data );
	data += 2;
	int HostSize = decode_2bytes(data );
	data += 2;
	metin2::log::info( "GM Value Count {:d} {:d}", HostSize, ChunkSize  );
	for (int n = 0; n < HostSize; ++n )
	{
		gm_new_host_inert(data );
		metin2::log::info( "GM HOST : IP[{}] ", data );		
		data += ChunkSize;
	}
	
	
	data += 2;
	int adminsize = decode_2bytes(data );
	data += 2;

	for (int n = 0; n < adminsize; ++n )
	{
		t_admin_info& rAdminInfo = *(t_admin_info*)data;

		gm_new_insert(rAdminInfo );
		
		data += sizeof(rAdminInfo );
	}
	
	//END_ADMIN_MANAGER
		
	//MONARCH
	data += 2;
	data += 2;

	monarch_info& p = *(monarch_info *) data;
	data += sizeof(monarch_info);

	monarch::instance().set_monarch_info(&p);

	for (int n = 1; n < 4; ++n)
	{
		if (*p.name[n])
			metin2::log::info( "[MONARCH] Empire {:d} Pid {:d} Money {:d} {}", n, p.pid[n], p.money[n], p.name[n]);
	}
	
	int CandidacySize = decode_2bytes(data);
	data += 2;

	int CandidacyCount = decode_2bytes(data);
	data += 2;

	if (metin2::config::test_server())
		metin2::log::info( "[MONARCH] Size {:d} Count {:d}", CandidacySize, CandidacyCount);

	data += CandidacySize * CandidacyCount;

	
	//END_MONARCH

	uint16_t endCheck=decode_2bytes(data);
	if (endCheck != 0xffff)
	{
		metin2::log::error("boot packet end check error [{:x}]!=0xffff", endCheck);
		thecore_shutdown();
		return;
	}
	else
		metin2::log::info( "boot packet end check ok [{:x}]==0xffff", endCheck );
	data +=2; 

	if (!item_manager::instance().set_max_item_id(*range))
	{
		metin2::log::error("not enough item id contact your administrator!");
		thecore_shutdown();
		return;
	}

	if (!item_manager::instance().set_max_spare_item_id(*rangespare))
	{
		metin2::log::error("not enough item id for spare contact your administrator!");
		thecore_shutdown();
		return;
	}



	// LOCALE_SERVICE
	const int FILE_NAME_LEN = 256;
	char szCommonDropItemFileName[FILE_NAME_LEN];
	char szETCDropItemFileName[FILE_NAME_LEN];
	char szMOBDropItemFileName[FILE_NAME_LEN];
	char szDropItemGroupFileName[FILE_NAME_LEN];
	char szSpecialItemGroupFileName[FILE_NAME_LEN];
	char szMapIndexFileName[FILE_NAME_LEN];
	char szItemVnumMaskTableFileName[FILE_NAME_LEN];
	char szDragonSoulTableFileName[FILE_NAME_LEN];

	snprintf(szCommonDropItemFileName, sizeof(szCommonDropItemFileName),
			"%s/common_drop_item.txt", locale_service_get_base_path().c_str());
	snprintf(szETCDropItemFileName, sizeof(szETCDropItemFileName),
			"%s/etc_drop_item.txt", locale_service_get_base_path().c_str());
	snprintf(szMOBDropItemFileName, sizeof(szMOBDropItemFileName),
			"%s/mob_drop_item.txt", locale_service_get_base_path().c_str());
	snprintf(szSpecialItemGroupFileName, sizeof(szSpecialItemGroupFileName),
			"%s/special_item_group.txt", locale_service_get_base_path().c_str());
	snprintf(szDropItemGroupFileName, sizeof(szDropItemGroupFileName),
			"%s/drop_item_group.txt", locale_service_get_base_path().c_str());
	snprintf(szMapIndexFileName, sizeof(szMapIndexFileName),
			"%s/index", locale_service_get_map_path().c_str());
	snprintf(szItemVnumMaskTableFileName, sizeof(szItemVnumMaskTableFileName),
			"%s/ori_to_new_table.txt", locale_service_get_base_path().c_str());
	snprintf(szDragonSoulTableFileName, sizeof(szDragonSoulTableFileName),
			"%s/dragon_soul_table.txt", locale_service_get_base_path().c_str());

	metin2::log::info( "Initializing Informations of Cube System");
	if (!cube_information_initialize())
	{
		metin2::log::error("cannot init cube infomation.");
		thecore_shutdown();
		return;
	}

	metin2::log::info( "LoadLocaleFile: CommonDropItem: {}", szCommonDropItemFileName);
	if (!item_manager::instance().read_common_drop_item_file(szCommonDropItemFileName))
	{
		metin2::log::error("cannot load CommonDropItem: {}", szCommonDropItemFileName);
		thecore_shutdown();
		return;
	}

	metin2::log::info( "LoadLocaleFile: ETCDropItem: {}", szETCDropItemFileName);
	if (!item_manager::instance().read_etc_drop_item_file(szETCDropItemFileName))
	{
		metin2::log::error("cannot load ETCDropItem: {}", szETCDropItemFileName);
		thecore_shutdown();
		return;
	}

	metin2::log::info( "LoadLocaleFile: DropItemGroup: {}", szDropItemGroupFileName);
	if (!item_manager::instance().read_drop_item_group(szDropItemGroupFileName))
	{
		metin2::log::error("cannot load DropItemGroup: {}", szDropItemGroupFileName);
		thecore_shutdown();
		return;
	}

	metin2::log::info( "LoadLocaleFile: SpecialItemGroup: {}", szSpecialItemGroupFileName);
	if (!item_manager::instance().read_special_drop_item_file(szSpecialItemGroupFileName))
	{
		metin2::log::error("cannot load SpecialItemGroup: {}", szSpecialItemGroupFileName);
		thecore_shutdown();
		return;
	}

	metin2::log::info( "LoadLocaleFile: ItemVnumMaskTable : {}", szItemVnumMaskTableFileName);
	if (!item_manager::instance().read_item_vnum_mask_table(szItemVnumMaskTableFileName))
	{
		metin2::log::info( "Could not open MaskItemTable");
	}

	metin2::log::info( "LoadLocaleFile: MOBDropItemFile: {}", szMOBDropItemFileName);
	if (!item_manager::instance().read_monster_drop_item_group(szMOBDropItemFileName))
	{
		metin2::log::error("cannot load MOBDropItemFile: {}", szMOBDropItemFileName);
		thecore_shutdown();
		return;
	}

	metin2::log::info( "LoadLocaleFile: MapIndex: {}", szMapIndexFileName);
	if (!sectree_manager::instance().build(szMapIndexFileName, locale_service_get_map_path().c_str()))
	{
		metin2::log::error("cannot load MapIndex: {}", szMapIndexFileName);
		thecore_shutdown();
		return;
	}

	metin2::log::info( "LoadLocaleFile: DragonSoulTable: {}", szDragonSoulTableFileName);
	if (!ds_manager::instance().read_dragon_soul_table_file(szDragonSoulTableFileName))
	{
		metin2::log::error("cannot load DragonSoulTable: {}", szDragonSoulTableFileName);
	}

	// END_OF_LOCALE_SERVICE


	building::manager::instance().finalize_boot();

	motion_manager::instance().build();

	signal_timer_enable(30);

	if (metin2::config::test_server())
	{
		mob_manager::instance().dump_regen_count("mob_count");
	}


	// castle_boot
	castle_boot();

    if (!mark_server_ready()) { thecore_shutdown(); return; }
    metin2::log::info( "BOOT: COMPLETE");
}

EVENTINFO(quest_login_event_info)
{
	uint32_t pid;

	quest_login_event_info() 
	: pid( 0 )
	{
	}
};

EVENTFUNC(quest_login_event)
{
	quest_login_event_info* info = dynamic_cast<quest_login_event_info*>( event->info );

	if ( info == nullptr )
	{
		metin2::log::error( "quest_login_event> <Factor> Null pointer" );
		return 0;
	}

	uint32_t dwPID = info->pid;

	character * ch = character_manager::instance().find_by_pid(dwPID);

	if (!ch)
		return 0;

	desc * d = ch->get_desc();

	if (!d)
		return 0;

	if (d->is_phase(PHASE_HANDSHAKE) ||
		d->is_phase(PHASE_LOGIN) ||
		d->is_phase(PHASE_SELECT) ||
		d->is_phase(PHASE_DEAD) ||
		d->is_phase(PHASE_LOADING))
	{
		return passes_per_sec(1);
	}
	else if (d->is_phase(PHASE_CLOSE))
	{
		return 0;
	}
	else if (d->is_phase(PHASE_GAME))
	{
		metin2::log::info( "QUEST_LOAD: Login pc {:d} by event", ch->player_id());
		quest::quest_manager::instance().login(ch->player_id());
		return 0;
	}
	else
	{
		metin2::log::error("input_db.cpp:quest_login_event INVALID PHASE pid {:d}", ch->player_id());
		return 0;
	}
}

void input_db::quest_load(desc * d, const char * data)
{
	if (nullptr == d)
		return;

	character * ch = d->get_character();

	if (nullptr == ch)
		return;

	const uint32_t dwCount = decode_4bytes(data);

	const quest_table* pQuestTable = reinterpret_cast<const quest_table*>(data+4);

	if (nullptr != pQuestTable)
	{
		if (dwCount != 0)
		{
			if (ch->player_id() != pQuestTable[0].pid)
			{
				metin2::log::error("PID differs {:d} {:d}", ch->player_id(), pQuestTable[0].pid);
				return;
			}
		}

		metin2::log::info( "QUEST_LOAD: count {:d}", dwCount);

		quest::pc * pkPC = quest::quest_manager::instance().pc_force(ch->player_id());

		if (!pkPC)
		{
			metin2::log::error("null quest::PC with id {:d}", pQuestTable[0].pid);
			return;
		}

		if (pkPC->is_loaded())
			return;

		for (unsigned int i = 0; i < dwCount; ++i)
		{
			std::string st(pQuestTable[i].name);

			st += ".";
			st += pQuestTable[i].state;

			metin2::log::info(  "            {} {:d}", st.c_str(), pQuestTable[i].value);
			pkPC->set_flag(st.c_str(), pQuestTable[i].value, false);
		}

		pkPC->set_loaded();
		pkPC->build();

		if (ch->get_desc()->is_phase(PHASE_GAME))
		{
			metin2::log::info( "QUEST_LOAD: Login pc {:d}", pQuestTable[0].pid);
			quest::quest_manager::instance().login(pQuestTable[0].pid);
		}
		else
		{
			quest_login_event_info* info = alloc_event_info<quest_login_event_info>();
			info->pid = ch->player_id();

			event_create(quest_login_event, info, passes_per_sec(1));
		}
	}	
}

void input_db::safebox_load(desc * d, const char * data)
{
	if (!d)
		return;

	safebox_table * p = (safebox_table *) data;

	if (d->get_account_table().id != p->id)
	{
		metin2::log::error("SafeboxLoad: safebox has different id {:d} != {:d}", d->get_account_table().id, p->id);
		return;
	}

	if (!d->get_character())
		return;

	uint8_t bSize = 1;

	character * ch = d->get_character();

	//PREVENT_TRADE_WINDOW
	if (ch->shop_owner() || ch->get_exchange() || ch->my_shop() || ch->is_cube_open() )
	{
		d->get_character()->chat_packet(CHAT_TYPE_INFO, lc_text("다른거래창이 열린상태에서는 창고를 열수가 없습니다." ) );
		d->get_character()->cancel_safebox_load();
		return;
	}
	//END_PREVENT_TRADE_WINDOW

	// ADD_PREMIUM
	if (d->get_character()->premium_remain_seconds(PREMIUM_SAFEBOX) > 0 ||
			d->get_character()->is_equip_unique_group(UNIQUE_GROUP_LARGE_SAFEBOX))
		bSize = 3;
	// END_OF_ADD_PREMIUM

	//if (d->GetCharacter()->IsEquipUniqueItem(UNIQUE_ITEM_SAFEBOX_EXPAND))
	//bSize = 3; // safebox expansion ticket

	d->get_character()->load_safebox(bSize * SAFEBOX_PAGE_SIZE, p->gold, p->item_count, (player_item *) (data + sizeof(safebox_table)));
}

void input_db::safebox_change_size(desc * d, const char * data)
{
	if (!d)
		return;

	uint8_t bSize = *(uint8_t *) data;

	if (!d->get_character())
		return;

	d->get_character()->change_safebox_size(bSize);
}

//
// @version	05/06/20 Bang2ni - cancel ReqSafeboxLoad
//
void input_db::safebox_wrong_password(desc * d)
{
	if (!d)
		return;

	if (!d->get_character())
		return;

	packet_cg_safebox_wrong_password p;
	p.header = HEADER_GC_SAFEBOX_WRONG_PASSWORD;
	d->packet(&p, sizeof(p));

	d->get_character()->cancel_safebox_load();
}

void input_db::safebox_change_password_answer(desc * d, const char* data)
{
	if (!d)
		return;

	if (!d->get_character())
		return;

	safebox_change_password_packet_answer* p = (safebox_change_password_packet_answer*) data;
	if (p->flag)
	{
		d->get_character()->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 창고 비밀번호가 변경되었습니다."));
	}
	else
	{
		d->get_character()->chat_packet(CHAT_TYPE_INFO, lc_text("<창고> 기존 비밀번호가 틀렸습니다."));
	}
}

void input_db::mall_load(desc * d, const char * data)
{
	if (!d)
		return;

	safebox_table * p = (safebox_table *) data;

	if (d->get_account_table().id != p->id)
	{
		metin2::log::error("safebox has different id {:d} != {:d}", d->get_account_table().id, p->id);
		return;
	}

	if (!d->get_character())
		return;

	d->get_character()->load_mall(p->item_count, (player_item *) (data + sizeof(safebox_table)));
}

void input_db::login_already(desc * d, const char * data)
{
	if (!d)
		return;

	// INTERNATIONAL_VERSION disconnect if already logged in
	{ 
		packet_dg_login_already * p = (packet_dg_login_already *) data;

		desc * d2 = desc_manager::instance().find_by_login_name(p->login);

		if (d2)
			d2->disconnect_of_same_login();
		else
		{
			packet_gg_disconnect pgg;

			pgg.header = HEADER_GG_DISCONNECT;
			strlcpy(pgg.login, p->login, sizeof(pgg.login));

			p2_p_manager::instance().send(&pgg, sizeof(packet_gg_disconnect));
		}
	}
	// END_OF_INTERNATIONAL_VERSION

	login_failure(d, "ALREADY");
}

void input_db::empire_select(desc * d, const char * data)
{
	metin2::log::info( "EmpireSelect {}", static_cast<const void *>(d));

	if (!d)
		return;

	account_table & rTable = d->get_account_table();
	rTable.empire = *(uint8_t *) data;

	packet_gc_empire pe;
	pe.header = HEADER_GC_EMPIRE;
	pe.empire = rTable.empire;
	d->packet(&pe, sizeof(pe));

	for (int i = 0; i < PLAYER_PER_ACCOUNT; ++i) 
		if (rTable.players[i].id)
		{
			rTable.players[i].x = empire_start_x(rTable.empire);
			rTable.players[i].y = empire_start_y(rTable.empire);
		}

	get_server_location(d->get_account_table(), rTable.empire);

	d->send_login_success_packet();
}

void input_db::map_locations(const char * data)
{
	uint8_t bCount = *(uint8_t *) (data++);

	metin2::log::info( "InputDB::MapLocations {:d}", bCount);

	map_location * pLoc = (map_location *) data;

	while (bCount--)
	{
		for (int i = 0; i < MAX_MAPS_PER_SERVER; ++i)
		{
			if (0 == pLoc->maps[i])
				break;

			game::world::map_locations::instance().insert(pLoc->maps[i], pLoc->host, pLoc->port);
		}

		pLoc++;
	}
}

void input_db::p2_p(const char * data)
{

	packet_dg_p2_p * p = (packet_dg_p2_p *) data;

	p2_p_manager& mgr = p2_p_manager::instance();

	if (false == desc_manager::instance().is_p2_p_desc_exist(p->host, p->port))
	{
	    client_desc * pkDesc = nullptr;
		metin2::log::info( "InputDB:P2P {}:{:d}", p->host, p->port);
	    pkDesc = desc_manager::instance().create_connection_desc(p->host, p->port, PHASE_P2P, false);
		mgr.register_connector(pkDesc);
		pkDesc->set_p2_p(p->host, p->port);
	}
}

void input_db::guild_load(const char * data)
{
	guild_manager::instance().load_guild(*(uint32_t *) data);
}

void input_db::guild_skill_update(const char* data)
{
	packet_guild_skill_update * p = (packet_guild_skill_update *) data;

	guild * g = guild_manager::instance().touch_guild(p->guild_id);

	if (g)
	{
		g->update_skill(p->skill_point, p->skill_levels);
		g->guild_point_change(POINT_SP, p->amount, p->save?true:false);
	}
}

void input_db::guild_war(const char* data)
{
	packet_guild_war * p = (packet_guild_war*) data;

	metin2::log::info( "InputDB::GuildWar {:d} {:d} state {:d}", p->guild_from, p->guild_to, p->war);

	switch (p->war)
	{
		case GUILD_WAR_SEND_DECLARE:
		case GUILD_WAR_RECV_DECLARE:
			guild_manager::instance().declare_war(p->guild_from, p->guild_to, p->type);
			break;

		case GUILD_WAR_REFUSE:
			guild_manager::instance().refuse_war(p->guild_from, p->guild_to);
			break;

		case GUILD_WAR_WAIT_START:
			guild_manager::instance().wait_start_war(p->guild_from, p->guild_to);
			break;

		case GUILD_WAR_CANCEL:
			guild_manager::instance().cancel_war(p->guild_from, p->guild_to);
			break;

		case GUILD_WAR_ON_WAR:
			guild_manager::instance().start_war(p->guild_from, p->guild_to);
			break;

		case GUILD_WAR_END:
			guild_manager::instance().end_war(p->guild_from, p->guild_to);
			break;

		case GUILD_WAR_OVER:
			guild_manager::instance().war_over(p->guild_from, p->guild_to, p->type);
			break;

		case GUILD_WAR_RESERVE:
			guild_manager::instance().reserve_war(p->guild_from, p->guild_to, p->type);
			break;

		default:
			metin2::log::error("Unknown guild war state");
			break;
	}
}

void input_db::guild_war_score(const char* data)
{
	packet_guild_war_score* p = (packet_guild_war_score*) data;
	guild * g = guild_manager::instance().touch_guild(p->guild_gain_point);
	g->set_war_score_against_to(p->guild_opponent, p->score);
}

void input_db::guild_skill_recharge()
{
	guild_manager::instance().skill_recharge();
}

void input_db::guild_exp_update(const char* data)
{
	packet_guild_skill_update * p = (packet_guild_skill_update *) data;
	metin2::log::debug( "GuildExpUpdate {:d}", p->amount);

	guild * g = guild_manager::instance().touch_guild(p->guild_id);

	if (g)
		g->guild_point_change(POINT_EXP, p->amount);
}

void input_db::guild_add_member(const char* data)
{
	packet_dg_guild_member * p = (packet_dg_guild_member *) data;
	guild * g = guild_manager::instance().touch_guild(p->guild);

	if (g)
		g->add_member(p);
}

void input_db::guild_remove_member(const char* data)
{
	packet_guild* p=(packet_guild*)data;
	guild* g = guild_manager::instance().touch_guild(p->guild);

	if (g)
		g->remove_member(p->info);
}

void input_db::guild_change_grade(const char* data)
{
	packet_guild* p=(packet_guild*)data;
	guild* g = guild_manager::instance().touch_guild(p->guild);

	if (g)
		g->p2_p_change_grade((uint8_t)p->info);
}

void input_db::guild_change_member_data(const char* data)
{
	metin2::log::info( "Recv GuildChangeMemberData");
	packet_guild_change_member_data * p = (packet_guild_change_member_data *) data;
	guild * g = guild_manager::instance().touch_guild(p->guild_id);

	if (g)
		g->change_member_data(p->pid, p->offer, p->level, p->grade);
}

void input_db::guild_disband(const char* data)
{
	packet_guild * p = (packet_guild*) data;
	guild_manager::instance().disband_guild(p->guild);
}

void input_db::guild_ladder(const char* data)
{
	packet_guild_ladder* p = (packet_guild_ladder*) data;
	metin2::log::info( "Recv GuildLadder {:d} {:d} / w {:d} d {:d} l {:d}", p->guild, p->ladder_point, p->win, p->draw, p->loss);
	guild * g = guild_manager::instance().touch_guild(p->guild);

	g->set_ladder_point(p->ladder_point);
	g->set_war_data(p->win, p->draw, p->loss);
}

void input_db::item_load(desc * d, const char * data)
{
	character * ch;

	if (!d || !(ch = d->get_character()))
		return;

	if (ch->is_item_loaded())
		return;

	uint32_t dwCount = decode_4bytes(data);
	data += sizeof(uint32_t);

	metin2::log::info( "ITEM_LOAD: COUNT {} {:d}", ch->name(), dwCount);

	std::vector<item *> v;

	player_item * p = (player_item *) data;

	for (uint32_t i = 0; i < dwCount; ++i, ++p)
	{
		item * item = item_manager::instance().create_item(p->vnum, p->count, p->id);

		if (!item)
		{
			metin2::log::error("cannot create item by vnum {:d} (name {} id {:d})", p->vnum, ch->name(), p->id);
			continue;
		}

		item->set_skip_save(true);
		item->set_sockets(p->sockets);
		item->set_attributes(p->attr);

		if ((p->window == INVENTORY && ch->inventory_item(p->pos)) ||
				(p->window == EQUIPMENT && ch->wear(p->pos)))
		{
			metin2::log::info( "ITEM_RESTORE: {} {}", ch->name(), item->name());
			v.push_back(item);
		}
		else
		{
			switch (p->window)
			{
				case INVENTORY:
				case DRAGON_SOUL_INVENTORY:
					item->add_to_character(ch, item_pos(p->window, p->pos));
					break;

				case EQUIPMENT:
					if (item->check_item_use_level(ch->level()) == true )
					{
						if (item->equip_to(ch, p->pos) == false )
						{
							v.push_back(item);
						}
					}
					else
					{
						v.push_back(item);
					}
					break;
			}
		}

		if (false == item->on_after_created_item())
			metin2::log::error("Failed to call ITEM::OnAfterCreatedItem (vnum: {:d}, id: {:d})", item->vnum(), item->id());

		item->set_skip_save(false);
	}

	auto it = v.begin();

	while (it != v.end())
	{
		game::items::item * item = *(it++);

		int pos = ch->empty_inventory(item->size());

		if (pos < 0)
		{
			pixel_position coord;
			coord.x = ch->x();
			coord.y = ch->y();

			item->add_to_ground(ch->map_index(), coord);
			item->set_ownership(ch, 180);
			item->start_destroy_event();
		}
		else
			item->add_to_character(ch, item_pos(INVENTORY, pos));
	}

	ch->check_maximum_points();
	ch->points_packet();

	ch->set_item_loaded();
}

void input_db::affect_load(desc * d, const char * data)
{
	if (!d)
		return;

	if (!d->get_character())
		return;

	character * ch = d->get_character();

	uint32_t dwPID = decode_4bytes(data);
	data += sizeof(uint32_t);

	uint32_t dwCount = decode_4bytes(data);
	data += sizeof(uint32_t);

	if (ch->player_id() != dwPID)
		return;

	ch->load_affect(dwCount, (packet_affect_element *) data);
	
}
	


void input_db::party_create(const char* data)
{
	packet_party_create* p = (packet_party_create*) data;
	party_manager::instance().p2_p_create_party(p->leader_pid);
}

void input_db::party_delete(const char* data)
{
	packet_party_delete* p = (packet_party_delete*) data;
	party_manager::instance().p2_p_delete_party(p->leader_pid);
}

void input_db::party_add(const char* data)
{
	packet_party_add* p = (packet_party_add*) data;
	party_manager::instance().p2_p_join_party(p->leader_pid, p->pid, p->state);
}

void input_db::party_remove(const char* data)
{
	packet_party_remove* p = (packet_party_remove*) data;
	party_manager::instance().p2_p_quit_party(p->pid);
}

void input_db::party_state_change(const char* data)
{
	packet_party_state_change * p = (packet_party_state_change *) data;
	party * pParty = party_manager::instance().p2_p_create_party(p->leader_pid);

	if (!pParty)
		return;

	pParty->set_role(p->pid, p->role, p->flag);
}

void input_db::party_set_member_level(const char* data)
{
	packet_party_set_member_level* p = (packet_party_set_member_level*) data;
	party * pParty = party_manager::instance().p2_p_create_party(p->leader_pid);

	if (!pParty)
		return;

	pParty->p2_p_set_member_level(p->pid, p->level);
}

void input_db::time(const char * data)
{
	int32_t server_time;
    memcpy(&server_time, data, sizeof(server_time));
    set_global_time(server_time);
}

void input_db::reload_proto(const char * data)
{
	uint16_t wSize;

	/*
	 * Skill
	 */
	wSize = decode_2bytes(data);
	data += sizeof(uint16_t);
	if (wSize) skill_manager::instance().initialize((skill_table *) data, wSize);
	data += sizeof(skill_table) * wSize;

	/*
	 * Banwords
	 */

	wSize = decode_2bytes(data);
	data += sizeof(uint16_t);
	banword_manager::instance().initialize((banword_table *) data, wSize);
	data += sizeof(banword_table) * wSize;

	/*
	 * ITEM
	 */
	wSize = decode_2bytes(data);
	data += 2;
	metin2::log::info( "RELOAD: ITEM: {:d}", wSize);

	if (wSize)
	{
		item_manager::instance().initialize((item_table *) data, wSize);
		data += wSize * sizeof(item_table);
	}

	/*
	 * MONSTER
	 */
	wSize = decode_2bytes(data);
	data += 2;
	metin2::log::info( "RELOAD: MOB: {:d}", wSize);

	if (wSize)
	{
		mob_manager::instance().initialize((mob_table *) data, wSize);
		data += wSize * sizeof(mob_table);
	}

	motion_manager::instance().build();

	character_manager::instance().for_each_pc(std::mem_fn(&character::compute_points));
}

void input_db::guild_skill_usable_change(const char* data)
{
	packet_guild_skill_usable_change* p = (packet_guild_skill_usable_change*) data;

	guild* g = guild_manager::instance().touch_guild(p->guild);

	g->skill_usable_change(p->skill_vnum, p->usable?true:false);
}

void input_db::auth_login(desc * d, const char * data)
{
	if (!d)
		return;

	uint8_t bResult = *(uint8_t *) data;

	packet_gc_auth_success ptoc;

	ptoc.header = HEADER_GC_AUTH_SUCCESS;

	if (bResult)
	{
		ptoc.login_key = d->get_login_key();
	}
	else
	{
		ptoc.login_key = 0;
	}

	ptoc.result = bResult;

	d->packet(&ptoc, sizeof(packet_gc_auth_success));
	metin2::log::info( "AuthLogin result {:d} key {:d}", bResult, d->get_login_key());
}
void input_db::change_empire_priv(const char* data)
{
	packet_dg_change_empire_priv* p = (packet_dg_change_empire_priv*) data;

	// ADD_EMPIRE_PRIV_TIME
	priv_manager::instance().give_empire_priv(p->empire, p->type, p->value, p->log, p->end_time_sec);
	// END_OF_ADD_EMPIRE_PRIV_TIME
}

/**
 * @version 05/06/08	Bang2ni - added duration
 */
void input_db::change_guild_priv(const char* data)
{
	packet_dg_change_guild_priv* p = (packet_dg_change_guild_priv*) data;

	// ADD_GUILD_PRIV_TIME
	priv_manager::instance().give_guild_priv(p->guild_id, p->type, p->value, p->log, p->end_time_sec);
	// END_OF_ADD_GUILD_PRIV_TIME
}

void input_db::change_character_priv(const char* data)
{
	packet_dg_change_character_priv* p = (packet_dg_change_character_priv*) data;
	priv_manager::instance().give_character_priv(p->pid, p->type, p->value, p->log);
}

void input_db::money_log(const char* data)
{
	packet_money_log * p = (packet_money_log *) data;

	if (p->type == 4) // QUEST_MONEY_LOG_SKIP
		return;

	if (metin2::config::is_auth() ==true )
		return;

	log_manager::instance().money_log(p->type, p->vnum, p->gold);
}

void input_db::guild_money_change(const char* data)
{
	packet_dg_guild_money_change* p = (packet_dg_guild_money_change*) data;

	guild* g = guild_manager::instance().touch_guild(p->guild);
	if (g)
	{
		g->recv_money_change(p->total_gold);
	}
}

void input_db::guild_withdraw_money(const char* data)
{
	packet_dg_guild_money_withdraw* p = (packet_dg_guild_money_withdraw*) data;

	guild* g = guild_manager::instance().touch_guild(p->guild);
	if (g)
	{
		g->recv_withdraw_money_give(p->change_gold);
	}
}

void input_db::set_event_flag(const char* data)
{
	packet_set_event_flag* p = (packet_set_event_flag*) data;
	quest::quest_manager::instance().set_event_flag(p->flag_name, p->value);
}

void input_db::create_object(const char * data)
{
	using namespace building;
	manager::instance().load_object((object_table *) data);
}

void input_db::delete_object(const char * data)
{
	using namespace building;
	manager::instance().delete_object(*(uint32_t *) data);
}

void input_db::update_land(const char * data)
{
	using namespace building;
	manager::instance().update_land((land_table *) data);
}

void input_db::notice(const char * data)
{

	char szBuf[256+1];
	strlcpy(szBuf, data, sizeof(szBuf));

	metin2::log::info( "InputDB:: Notice: {}", szBuf);

	send_notice(szBuf);
}

void input_db::guild_war_reserve_add(guild_war_reserve * p)
{
	guild_manager::instance().reserve_war_add(p);
}

void input_db::guild_war_reserve_delete(uint32_t id)
{
	guild_manager::instance().reserve_war_delete(id);
}

void input_db::guild_war_bet(packet_gd_guild_war_bet * p)
{
	guild_manager::instance().reserve_war_bet(p);
}

void input_db::marriage_add(packet_marriage_add * p)
{
	metin2::log::info( "MarriageAdd {:d} {:d} {:d} {} {}", p->pi_d1, p->pi_d2, (uint32_t)p->t_marry_time, p->name1, p->name2);
	marriage::manager::instance().add(p->pi_d1, p->pi_d2, p->t_marry_time, p->name1, p->name2);
}

void input_db::marriage_update(packet_marriage_update * p)
{
	metin2::log::info( "MarriageUpdate {:d} {:d} {:d} {:d}", p->pi_d1, p->pi_d2, p->love_point, p->married);
	marriage::manager::instance().update(p->pi_d1, p->pi_d2, p->love_point, p->married);
}

void input_db::marriage_remove(packet_marriage_remove * p)
{
	metin2::log::info( "MarriageRemove {:d} {:d}", p->pi_d1, p->pi_d2);
	marriage::manager::instance().remove(p->pi_d1, p->pi_d2);
}

void input_db::wedding_request(packet_wedding_request* p)
{
	marriage::wedding_manager::instance().request(p->pi_d1, p->pi_d2);
}

void input_db::wedding_ready(packet_wedding_ready* p)
{
	metin2::log::info( "WeddingReady {:d} {:d} {:d}", p->pi_d1, p->pi_d2, p->map_index);
	marriage::manager::instance().wedding_ready(p->pi_d1, p->pi_d2, p->map_index);
}

void input_db::wedding_start(packet_wedding_start* p)
{
	metin2::log::info( "WeddingStart {:d} {:d}", p->pi_d1, p->pi_d2);
	marriage::manager::instance().wedding_start(p->pi_d1, p->pi_d2);
}

void input_db::wedding_end(packet_wedding_end* p)
{
	metin2::log::info( "WeddingEnd {:d} {:d}", p->pi_d1, p->pi_d2);
	marriage::manager::instance().wedding_end(p->pi_d1, p->pi_d2);
}

// MYSHOP_PRICE_LIST
void input_db::myshop_pricelist_res(desc * d, const packet_myshop_pricelist_header* p )
{
	character * ch;

	if (!d || !(ch = d->get_character()) )
		return;

	metin2::log::info( "RecvMyshopPricelistRes name[{}]", ch->name());
	ch->use_silk_botary_real(p );

}
// END_OF_MYSHOP_PRICE_LIST


//RELOAD_ADMIN
void input_db::reload_admin(const char * data )
{
	gm_new_clear();
	int ChunkSize = decode_2bytes(data );
	data += 2;
	int HostSize = decode_2bytes(data );
	data += 2;
	
	for (int n = 0; n < HostSize; ++n )
	{
		gm_new_host_inert(data );
		data += ChunkSize;
	}
	
	
	data += 2;
	int size = 	decode_2bytes(data );
	data += 2;
	
	for (int n = 0; n < size; ++n )
	{
		t_admin_info& rAdminInfo = *(t_admin_info*)data;

		gm_new_insert(rAdminInfo );

		data += sizeof (t_admin_info );
	
		character * pChar = character_manager::instance().find_pc(rAdminInfo.name );
		if (pChar )
		{
			pChar->set_gm_level();
		}
	}
	
}
//END_RELOAD_ADMIN

////////////////////////////////////////////////////////////////////
// Analyze
// @version	05/06/10 Bang2ni - added handling of the item price list packet (HEADER_DG_MYSHOP_PRICELIST_RES).
////////////////////////////////////////////////////////////////////
int input_db::analyze(desc * d, uint8_t header, const char * data)
{
	switch (header)
	{
	case HEADER_DG_BOOT:
		boot(data);
		break;

	case HEADER_DG_LOGIN_SUCCESS:
		login_success(handle_, data);
		break;

	case HEADER_DG_LOGIN_NOT_EXIST:
		login_failure(desc_manager::instance().find_by_handle(handle_), "NOID");
		break;

	case HEADER_DG_LOGIN_WRONG_PASSWD:
		login_failure(desc_manager::instance().find_by_handle(handle_), "WRONGPWD");
		break;

	case HEADER_DG_LOGIN_ALREADY:
		login_already(desc_manager::instance().find_by_handle(handle_), data);
		break;

	case HEADER_DG_PLAYER_LOAD_SUCCESS:
		player_load(desc_manager::instance().find_by_handle(handle_), data);
		break;

	case HEADER_DG_PLAYER_CREATE_SUCCESS:
		player_create_success(desc_manager::instance().find_by_handle(handle_), data);
		break;

	case HEADER_DG_PLAYER_CREATE_FAILED:
		player_create_failure(desc_manager::instance().find_by_handle(handle_), 0);
		break;

	case HEADER_DG_PLAYER_CREATE_ALREADY:
		player_create_failure(desc_manager::instance().find_by_handle(handle_), 1);
		break;

	case HEADER_DG_PLAYER_DELETE_SUCCESS:
		player_delete_success(desc_manager::instance().find_by_handle(handle_), data);
		break;

	case HEADER_DG_PLAYER_LOAD_FAILED:
		break;

	case HEADER_DG_PLAYER_DELETE_FAILED:
		player_delete_fail(desc_manager::instance().find_by_handle(handle_));
		break;

	case HEADER_DG_ITEM_LOAD:
		item_load(desc_manager::instance().find_by_handle(handle_), data);
		break;

	case HEADER_DG_QUEST_LOAD:
		quest_load(desc_manager::instance().find_by_handle(handle_), data);
		break;

	case HEADER_DG_AFFECT_LOAD:
		affect_load(desc_manager::instance().find_by_handle(handle_), data);
		break;

	case HEADER_DG_SAFEBOX_LOAD:
		safebox_load(desc_manager::instance().find_by_handle(handle_), data);
		break;

	case HEADER_DG_SAFEBOX_CHANGE_SIZE:
		safebox_change_size(desc_manager::instance().find_by_handle(handle_), data);
		break;

	case HEADER_DG_SAFEBOX_WRONG_PASSWORD:
		safebox_wrong_password(desc_manager::instance().find_by_handle(handle_));
		break;

	case HEADER_DG_SAFEBOX_CHANGE_PASSWORD_ANSWER:
		safebox_change_password_answer(desc_manager::instance().find_by_handle(handle_), data);
		break;

	case HEADER_DG_MALL_LOAD:
		mall_load(desc_manager::instance().find_by_handle(handle_), data);
		break;

	case HEADER_DG_EMPIRE_SELECT:
		empire_select(desc_manager::instance().find_by_handle(handle_), data);
		break;

	case HEADER_DG_MAP_LOCATIONS:
		map_locations(data);
		break;

	case HEADER_DG_P2P:
		p2_p(data);
		break;

	case HEADER_DG_GUILD_SKILL_UPDATE:
		guild_skill_update(data);
		break;

	case HEADER_DG_GUILD_LOAD:
		guild_load(data);
		break;

	case HEADER_DG_GUILD_SKILL_RECHARGE:
		guild_skill_recharge();
		break;

	case HEADER_DG_GUILD_EXP_UPDATE:
		guild_exp_update(data);
		break;

	case HEADER_DG_PARTY_CREATE:
		party_create(data);
		break;

	case HEADER_DG_PARTY_DELETE:
		party_delete(data);
		break;

	case HEADER_DG_PARTY_ADD:
		party_add(data);
		break;

	case HEADER_DG_PARTY_REMOVE:
		party_remove(data);
		break;

	case HEADER_DG_PARTY_STATE_CHANGE:
		party_state_change(data);
		break;

	case HEADER_DG_PARTY_SET_MEMBER_LEVEL:
		party_set_member_level(data);    
		break;

	case HEADER_DG_TIME:
		time(data);
		break;

	case HEADER_DG_GUILD_ADD_MEMBER:
		guild_add_member(data);
		break;

	case HEADER_DG_GUILD_REMOVE_MEMBER:
		guild_remove_member(data);
		break;

	case HEADER_DG_GUILD_CHANGE_GRADE:
		guild_change_grade(data);
		break;

	case HEADER_DG_GUILD_CHANGE_MEMBER_DATA:
		guild_change_member_data(data);
		break;

	case HEADER_DG_GUILD_DISBAND:
		guild_disband(data);
		break;

	case HEADER_DG_RELOAD_PROTO:
		reload_proto(data);
		break;

	case HEADER_DG_GUILD_WAR:
		guild_war(data);
		break;

	case HEADER_DG_GUILD_WAR_SCORE:
		guild_war_score(data);
		break;

	case HEADER_DG_GUILD_LADDER:
		guild_ladder(data);
		break;

	case HEADER_DG_GUILD_SKILL_USABLE_CHANGE:
		guild_skill_usable_change(data);
		break;

	case HEADER_DG_CHANGE_NAME:
		change_name(desc_manager::instance().find_by_handle(handle_), data);
		break;

	case HEADER_DG_AUTH_LOGIN:
		auth_login(desc_manager::instance().find_by_handle(handle_), data);
		break;

	case HEADER_DG_CHANGE_EMPIRE_PRIV:
		change_empire_priv(data);
		break;

	case HEADER_DG_CHANGE_GUILD_PRIV:
		change_guild_priv(data);
		break;

	case HEADER_DG_CHANGE_CHARACTER_PRIV:
		change_character_priv(data);
		break;

	case HEADER_DG_MONEY_LOG:
		money_log(data);
		break;

	case HEADER_DG_GUILD_WITHDRAW_MONEY_GIVE:
		guild_withdraw_money(data);
		break;

	case HEADER_DG_GUILD_MONEY_CHANGE:
		guild_money_change(data);
		break;

	case HEADER_DG_SET_EVENT_FLAG:
		set_event_flag(data);
		break;

	case HEADER_DG_CREATE_OBJECT:
		create_object(data);
		break;

	case HEADER_DG_DELETE_OBJECT:
		delete_object(data);
		break;

	case HEADER_DG_UPDATE_LAND:
		update_land(data);
		break;

	case HEADER_DG_NOTICE:
		notice(data);
		break;

	case HEADER_DG_GUILD_WAR_RESERVE_ADD:
		guild_war_reserve_add((guild_war_reserve *) data);
		break;

	case HEADER_DG_GUILD_WAR_RESERVE_DEL:
		guild_war_reserve_delete(*(uint32_t *) data);
		break;

	case HEADER_DG_GUILD_WAR_BET:
		guild_war_bet((packet_gd_guild_war_bet *) data);
		break;

	case HEADER_DG_MARRIAGE_ADD:
		marriage_add((packet_marriage_add*) data);
		break;

	case HEADER_DG_MARRIAGE_UPDATE:
		marriage_update((packet_marriage_update*) data);
		break;

	case HEADER_DG_MARRIAGE_REMOVE:
		marriage_remove((packet_marriage_remove*) data);
		break;

	case HEADER_DG_WEDDING_REQUEST:
		wedding_request((packet_wedding_request*) data);
		break;

	case HEADER_DG_WEDDING_READY:
		wedding_ready((packet_wedding_ready*) data);
		break;

	case HEADER_DG_WEDDING_START:
		wedding_start((packet_wedding_start*) data);
		break;

	case HEADER_DG_WEDDING_END:
		wedding_end((packet_wedding_end*) data);
		break;

		// MYSHOP_PRICE_LIST
	case HEADER_DG_MYSHOP_PRICELIST_RES:
		myshop_pricelist_res(desc_manager::instance().find_by_handle(handle_), (packet_myshop_pricelist_header*) data );
		break;
		// END_OF_MYSHOP_PRICE_LIST
		//
	// RELOAD_ADMIN
	case HEADER_DG_RELOAD_ADMIN:
		reload_admin(data );		
		break;
	//END_RELOAD_ADMIN

	case HEADER_DG_ADD_MONARCH_MONEY:
		add_monarch_money(desc_manager::instance().find_by_handle(handle_), data ); 
		break;

	case HEADER_DG_DEC_MONARCH_MONEY:
		dec_monarch_money(desc_manager::instance().find_by_handle(handle_), data );
		break;

	case HEADER_DG_TAKE_MONARCH_MONEY:
		take_monarch_money(desc_manager::instance().find_by_handle(handle_), data );
		break;

	case HEADER_DG_CHANGE_MONARCH_LORD_ACK :
		change_monarch_lord((packet_change_monarch_lord_ack*)data);
		break;

	case HEADER_DG_UPDATE_MONARCH_INFO :
		update_monarch_info((monarch_info*)data);
		break;


	case HEADER_DG_ACK_CHANGE_GUILD_MASTER :
		this->guild_change_master((packet_change_guild_master*) data);
		break;	
	case HEADER_DG_ACK_SPARE_ITEM_ID_RANGE :
		item_manager::instance().set_max_spare_item_id(*((item_id_range_table*)data) );
		break;

	case HEADER_DG_UPDATE_HORSE_NAME :
	case HEADER_DG_ACK_HORSE_NAME :
		horse_name_manager::instance().update_horse_name(
				((packet_update_horse_name*)data)->player_id, 
				((packet_update_horse_name*)data)->horse_name);
		break;

	case HEADER_DG_NEED_LOGIN_LOG:
		detail_log( (packet_need_login_log_info*) data );
		break;
	// Germany gift feature test
	case HEADER_DG_ITEMAWARD_INFORMER:
		item_award_informer((packet_item_award_infromer*) data);
		break;
	case HEADER_DG_RESPOND_CHANNELSTATUS:
		respond_channel_status(desc_manager::instance().find_by_handle(handle_), data);
		break;
	default:
		return (-1);
	}

	return 0;
}

bool input_db::process(desc * d, const void * orig, int bytes, int & r_i_bytes_proceed)
{
	const char *	c_pData = (const char *) orig;
	uint8_t		bHeader, bLastHeader = 0;
	int			iSize;
	int			iLastPacketLen = 0;

	for (buffer_left_ = bytes; buffer_left_ > 0;)
	{
		if (buffer_left_ < 9)
			return true;

		bHeader		= *((uint8_t *) (c_pData));	// 1
		handle_	= *((uint32_t *) (c_pData + 1));	// 4
		iSize		= *((uint32_t *) (c_pData + 5));	// 4

		metin2::log::debug( "DBCLIENT: header {:d} handle {:d} size {:d} bytes {:d}", bHeader, handle_, iSize, bytes); 

		if (buffer_left_ - 9 < iSize)
			return true;

		const char * pRealData = (c_pData + 9);

		if (analyze(d, bHeader, pRealData) < 0)
		{
			metin2::log::error("in InputDB: UNKNOWN HEADER: {:d}, LAST HEADER: {:d}({:d}), REMAIN BYTES: {:d}, host: {}",
					bHeader, bLastHeader, iLastPacketLen, buffer_left_, d->host_name());

		}

		c_pData		+= 9 + iSize;
		buffer_left_	-= 9 + iSize;
		r_i_bytes_proceed	+= 9 + iSize;

		iLastPacketLen	= 9 + iSize;
		bLastHeader	= bHeader;
	}

	return true;
}

void input_db::add_monarch_money(desc * d, const char * data )
{
	int Empire = *(int *) data;
	data += sizeof(int);

	int Money = *(int *) data;
	data += sizeof(int);
	
	monarch::instance().add_money(Money, Empire);

	uint32_t pid = monarch::instance().monarch_pid(Empire);	

	character * ch = character_manager::instance().find_by_pid(pid);

	if (ch)
	{
		if (number(1, 100) > 95) 
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("현재 %s 국고에는 %u 의 돈이 있습니다"), empire_name(Empire), monarch::instance().money(Empire));
	}
}
	
void input_db::dec_monarch_money(desc * d, const char * data)
{
	int Empire = *(int *) data;
	data += sizeof(int);
	
	int Money = *(int *) data;
	data += sizeof(int);

	monarch::instance().dec_money(Money, Empire);
	
	uint32_t pid = monarch::instance().monarch_pid(Empire);	

	character * ch = character_manager::instance().find_by_pid(pid);

	if (ch)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("현재 %s 국고에는 %d 의 돈이 있습니다"), empire_name(Empire), monarch::instance().money(Empire));
	}
}

void input_db::take_monarch_money(desc * d, const char * data)
{
	int Empire = *(int *) data;
	data += sizeof(int);
	
	int Money = *(int *) data;
	data += sizeof(int);

	if (!monarch::instance().dec_money(Money, Empire))
	{
		if (!d)
			return;

		if (!d->get_character())
			return;

		character * ch = d->get_character();
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("국고에 돈이 부족하거나 돈을 가져올수 없는 상황입니다"));
	}
}

void input_db::change_monarch_lord(packet_change_monarch_lord_ack* info)
{
	char notice[256];
	snprintf(notice, sizeof(notice), lc_text("%s의 군주가 %s 님으로 교체되었습니다."), empire_name(info->empire), info->name);
	send_notice(notice);
}

void input_db::update_monarch_info(monarch_info* info)
{
	monarch::instance().set_monarch_info(info);
	metin2::log::info( "MONARCH INFO UPDATED");
}

void input_db::guild_change_master(packet_change_guild_master* p)
{
	guild_manager::instance().change_master(p->guild_id);
}

void input_db::detail_log(const packet_need_login_log_info* info)
{
	character * pChar = character_manager::instance().find_by_pid( info->player_id );

	if (nullptr != pChar)
	{
		log_manager::instance().detail_login_log(true, pChar);

	}
}

void input_db::item_award_informer(packet_item_award_infromer *data)
{	
	desc * d = desc_manager::instance().find_by_login_name(data->login);	//login info
	
	if(d == nullptr)
		return;
	else
	{
		if (d->get_character())
		{
			character * ch = d->get_character();	
			ch->set_item_award_vnum(data->vnum);	// store temporarily in ch, handled in QuestLoad
			ch->set_item_award_cmd(data->command);		

			if(d->is_phase(PHASE_GAME))			//when in game phase
			{
				quest::quest_manager::instance().item_informer(ch->player_id(),ch->item_award_vnum());	//call questmanager
			}
		}
	}
}

void input_db::respond_channel_status(desc * desc, const char* pc_data) 
{
	if (!desc) {
		return;
	}
	const int nSize = decode_4bytes(pc_data);
	pc_data += sizeof(nSize);

	uint8_t bHeader = HEADER_GC_RESPOND_CHANNELSTATUS;
	desc->buffered_packet(&bHeader, sizeof(uint8_t));
	desc->buffered_packet(&nSize, sizeof(nSize));
	if (0 < nSize) {
		desc->buffered_packet(pc_data, sizeof(channel_status)*nSize);
	}
	uint8_t bSuccess = 1;
	desc->packet(&bSuccess, sizeof(bSuccess));
	desc->set_channel_status_requested(false);
}
}
