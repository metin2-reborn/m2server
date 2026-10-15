#include <metin2/game/characters/affect.h>
#include <metin2/game/events/arena.h>
#include <metin2/game/protocol/buffer_manager.h>
#include <metin2/game/guilds/building.h>
#include <metin2/game/characters/char.h>
#include <metin2/game/characters/char_manager.h>
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
#include <metin2/game/guilds/MarkImage.h>
#include <metin2/game/guilds/MarkManager.h>
#include <metin2/game/social/marriage.h>
#include <metin2/game/social/messenger_manager.h>
#include <metin2/game/events/OXEvent.h>
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

static void _send_bonus_info(CHARACTER * ch)
{
	int	item_drop_bonus = 0;
	int gold_drop_bonus = 0;
	int gold10_drop_bonus	= 0;
	int exp_bonus		= 0;

	item_drop_bonus		= CPrivManager::instance().GetPriv(ch, PRIV_ITEM_DROP);
	gold_drop_bonus		= CPrivManager::instance().GetPriv(ch, PRIV_GOLD_DROP);
	gold10_drop_bonus	= CPrivManager::instance().GetPriv(ch, PRIV_GOLD10_DROP);
	exp_bonus			= CPrivManager::instance().GetPriv(ch, PRIV_EXP_PCT);

	if (item_drop_bonus)
	{
		ch->ChatPacket(CHAT_TYPE_NOTICE, 
				LC_TEXT("아이템 드롭률  %d%% 추가 이벤트 중입니다."), item_drop_bonus);
	}
	if (gold_drop_bonus)
	{
		ch->ChatPacket(CHAT_TYPE_NOTICE, 
				LC_TEXT("골드 드롭률 %d%% 추가 이벤트 중입니다."), gold_drop_bonus);
	}
	if (gold10_drop_bonus)
	{
		ch->ChatPacket(CHAT_TYPE_NOTICE, 
				LC_TEXT("대박골드 드롭률 %d%% 추가 이벤트 중입니다."), gold10_drop_bonus);
	}
	if (exp_bonus)
	{
		ch->ChatPacket(CHAT_TYPE_NOTICE, 
				LC_TEXT("경험치 %d%% 추가 획득 이벤트 중입니다."), exp_bonus);
	}
}

static bool FN_is_battle_zone(CHARACTER * ch)
{
	switch (ch->GetMapIndex())
	{
		case 1:         // Shinsoo village 1
		case 2:         // Shinsoo village 2
		case 21:        // Chunjo village 1
		case 23:        // Chunjo village 2
		case 41:        // Jinno village 1
		case 43:        // Jinno village 2
		case 113:       // OX map
			return false;
	}

	return true;
}

void CInputLogin::Login(DESC * d, const char * data)
{
	TPacketCGLogin * pinfo = (TPacketCGLogin *) data;

	if (d->GetAccountTable().id)
	{
		metin2::log::error("already logged in (login {})", d->GetAccountTable().login);
		return;
	}

	char szLogin[sizeof(pinfo->login)];
	CopyPacketString(szLogin, pinfo->login);

	char login[LOGIN_MAX_LEN + 1];
	trim_and_lower(szLogin, login, sizeof(login));

	metin2::log::info( "InputLogin::Login : {}", login);

	TPacketGCLoginFailure failurePacket;

	if (!metin2::config::test_server())
	{
		failurePacket.header = HEADER_GC_LOGIN_FAILURE;
		strlcpy(failurePacket.szStatus, "VERSION", sizeof(failurePacket.szStatus));
		d->Packet(&failurePacket, sizeof(TPacketGCLoginFailure));
		return;
	}

	if (g_bNoMoreClient)
	{
		failurePacket.header = HEADER_GC_LOGIN_FAILURE;
		strlcpy(failurePacket.szStatus, "SHUTDOWN", sizeof(failurePacket.szStatus));
		d->Packet(&failurePacket, sizeof(TPacketGCLoginFailure));
		return;
	}

	if (metin2::config::server().user_limit > 0)
	{
		int iTotal;
		int * paiEmpireUserCount;
		int iLocal;

		DESC_MANAGER::instance().GetUserCount(iTotal, &paiEmpireUserCount, iLocal);

		if (metin2::config::server().user_limit <= iTotal)
		{
			failurePacket.header = HEADER_GC_LOGIN_FAILURE;
			strlcpy(failurePacket.szStatus, "FULL", sizeof(failurePacket.szStatus));
			d->Packet(&failurePacket, sizeof(TPacketGCLoginFailure));
			return;
		}
	}

	TLoginPacket login_packet;

	strlcpy(login_packet.login, login, sizeof(login_packet.login));
	char szPasswd[sizeof(pinfo->passwd)];
	CopyPacketString(szPasswd, pinfo->passwd);
	strlcpy(login_packet.passwd, szPasswd, sizeof(login_packet.passwd));

	db_clientdesc->DBPacket(HEADER_GD_LOGIN, d->GetHandle(), &login_packet, sizeof(TLoginPacket)); 
}

void CInputLogin::LoginByKey(DESC * d, const char * data)
{
	TPacketCGLogin2 * pinfo = (TPacketCGLogin2 *) data;

	if (d->GetAccountTable().id)
	{
		metin2::log::error("already logged in (login {})", d->GetAccountTable().login);
		return;
	}

	char szLogin[sizeof(pinfo->login)];
	CopyPacketString(szLogin, pinfo->login);

	char login[LOGIN_MAX_LEN + 1];
	trim_and_lower(szLogin, login, sizeof(login));

	if (g_bNoMoreClient)
	{
		TPacketGCLoginFailure failurePacket;

		failurePacket.header = HEADER_GC_LOGIN_FAILURE;
		strlcpy(failurePacket.szStatus, "SHUTDOWN", sizeof(failurePacket.szStatus));
		d->Packet(&failurePacket, sizeof(TPacketGCLoginFailure));
		return;
	}

	if (metin2::config::server().user_limit > 0)
	{
		int iTotal;
		int * paiEmpireUserCount;
		int iLocal;

		DESC_MANAGER::instance().GetUserCount(iTotal, &paiEmpireUserCount, iLocal);

		if (metin2::config::server().user_limit <= iTotal)
		{
			TPacketGCLoginFailure failurePacket;

			failurePacket.header = HEADER_GC_LOGIN_FAILURE;
			strlcpy(failurePacket.szStatus, "FULL", sizeof(failurePacket.szStatus));

			d->Packet(&failurePacket, sizeof(TPacketGCLoginFailure));
			return;
		}
	}

	metin2::log::info( "LOGIN_BY_KEY: {} key {:d}", login, pinfo->dwLoginKey);

	d->SetLoginKey(pinfo->dwLoginKey);

	TPacketGDLoginByKey ptod;

	strlcpy(ptod.szLogin, login, sizeof(ptod.szLogin));
	ptod.dwLoginKey = pinfo->dwLoginKey;
	std::memcpy(ptod.adwClientKey, pinfo->adwClientKey, sizeof(uint32_t) * 4);
	strlcpy(ptod.szIP, d->GetHostName(), sizeof(ptod.szIP));

	db_clientdesc->DBPacket(HEADER_GD_LOGIN_BY_KEY, d->GetHandle(), &ptod, sizeof(TPacketGDLoginByKey));
}

void CInputLogin::ChangeName(DESC * d, const char * data)
{
	TPacketCGChangeName * p = (TPacketCGChangeName *) data;
	const TAccountTable & c_r = d->GetAccountTable();

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

	if (!c_r.players[p->index].bChangeName)
		return;

	char szName[sizeof(p->name)];
	CopyPacketString(szName, p->name);

	if (!check_name(szName))
	{
		TPacketGCCreateFailure pack;
		pack.header = HEADER_GC_CHARACTER_CREATE_FAILURE;
		pack.bType = 0;
		d->Packet(&pack, sizeof(pack));
		return;
	}

	TPacketGDChangeName pdb;

	pdb.pid = c_r.players[p->index].dwID;
	strlcpy(pdb.name, szName, sizeof(pdb.name));
	db_clientdesc->DBPacket(HEADER_GD_CHANGE_NAME, d->GetHandle(), &pdb, sizeof(TPacketGDChangeName));
}

void CInputLogin::CharacterSelect(DESC * d, const char * data)
{
	struct TPacketCGPlayerSelect * pinfo = (struct TPacketCGPlayerSelect *) data;
	const TAccountTable & c_r = d->GetAccountTable();

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

	if (c_r.players[pinfo->index].bChangeName)
	{
		metin2::log::error("name must be changed idx {:d}, login {}, name {}", 
				pinfo->index, c_r.login, c_r.players[pinfo->index].szName);
		return;
	}

	TPlayerLoadPacket player_load_packet;

	player_load_packet.account_id	= c_r.id;
	player_load_packet.player_id	= c_r.players[pinfo->index].dwID;
	player_load_packet.account_index	= pinfo->index;

	db_clientdesc->DBPacket(HEADER_GD_PLAYER_LOAD, d->GetHandle(), &player_load_packet, sizeof(TPlayerLoadPacket));
}

bool RaceToJob(unsigned race, unsigned* ret_job)
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

// New character support
bool NewPlayerTable2(TPlayerTable * table, const char * name, uint8_t race, uint8_t shape, uint8_t bEmpire)
{
	if (race >= MAIN_RACE_MAX_NUM)
	{
		metin2::log::error("NewPlayerTable2.OUT_OF_RACE_RANGE({:d} >= max({:d}))", race, MAIN_RACE_MAX_NUM);
		return false;
	}

	unsigned job;

	if (!RaceToJob(race, &job))
	{	
		metin2::log::error("NewPlayerTable2.RACE_TO_JOB_ERROR({:d})", race);
		return false;
	}

	metin2::log::info( "NewPlayerTable2(name={}, race={:d}, job={:d})", name, race, job); 

	memset(table, 0, sizeof(TPlayerTable));

	strlcpy(table->name, name, sizeof(table->name));

	table->level		= 1;
	table->job			= race;	// Store race instead of job
	table->voice		= 0;
	table->part_base	= shape;

	table->st		= JobInitialPoints[job].st;
	table->dx		= JobInitialPoints[job].dx;
	table->ht		= JobInitialPoints[job].ht;
	table->iq		= JobInitialPoints[job].iq;

	table->hp		= JobInitialPoints[job].max_hp + table->ht * JobInitialPoints[job].hp_per_ht;
	table->sp		= JobInitialPoints[job].max_sp + table->iq * JobInitialPoints[job].sp_per_iq;
	table->stamina	= JobInitialPoints[job].max_stamina;

	table->x		= CREATE_START_X(bEmpire) + number(-300, 300);
	table->y		= CREATE_START_Y(bEmpire) + number(-300, 300);
	table->z		= 0;
	table->dir		= 0;
	table->playtime = 0;
	table->gold 	= 0;

	table->skill_group = 0;

	return true;
}

void CInputLogin::CharacterCreate(DESC * d, const char * data)
{
	struct TPacketCGPlayerCreate * pinfo = (struct TPacketCGPlayerCreate *) data;
	TPlayerCreatePacket player_create_packet;

	char szName[sizeof(pinfo->name)];
	CopyPacketString(szName, pinfo->name);

	metin2::log::info( "PlayerCreate: name {} pos {:d} job {:d} shape {:d}",
			szName, 
			pinfo->index, 
			pinfo->job, 
			pinfo->shape);

	const TAccountTable & c_rAccountTable = d->GetAccountTable();

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

	TPacketGCLoginFailure packFailure;
	memset(&packFailure, 0, sizeof(packFailure));
	packFailure.header = HEADER_GC_CHARACTER_CREATE_FAILURE;

	if (true == metin2::config::game().block_char_creation)
	{
		d->Packet(&packFailure, sizeof(packFailure));
		return;
	}

	// Creation fails if the name is not allowed or the default outfit is invalid
	if (!check_name(szName) || pinfo->shape > 1)
	{

		d->Packet(&packFailure, sizeof(packFailure));
		return;
	}

	if (0 == strcmp(c_rAccountTable.login, szName))
	{
		TPacketGCCreateFailure pack;
		pack.header = HEADER_GC_CHARACTER_CREATE_FAILURE;
		pack.bType = 1;

		d->Packet(&pack, sizeof(pack));
		return;
	}

	memset(&player_create_packet, 0, sizeof(TPlayerCreatePacket));

	// job is 16 bits on the wire; NewPlayerTable2 takes a byte, so range check before narrowing.
	if (pinfo->job >= MAIN_RACE_MAX_NUM ||
		!NewPlayerTable2(&player_create_packet.player_table, szName, (uint8_t) pinfo->job, pinfo->shape, d->GetEmpire()))
	{
		metin2::log::error("player_prototype error: job {:d} ", pinfo->job);
		d->Packet(&packFailure, sizeof(packFailure));
		return;
	}

	trim_and_lower(c_rAccountTable.login, player_create_packet.login, sizeof(player_create_packet.login));
	strlcpy(player_create_packet.passwd, c_rAccountTable.passwd, sizeof(player_create_packet.passwd));

	player_create_packet.account_id	= c_rAccountTable.id;
	player_create_packet.account_index	= pinfo->index;

	metin2::log::info( "PlayerCreate: name {} account_id {:d}, TPlayerCreatePacketSize({:d}), Packet->Gold {:d}",
			szName, 
			pinfo->index, 
			sizeof(TPlayerCreatePacket),
			player_create_packet.player_table.gold);

	db_clientdesc->DBPacket(HEADER_GD_PLAYER_CREATE, d->GetHandle(), &player_create_packet, sizeof(TPlayerCreatePacket));
}

void CInputLogin::CharacterDelete(DESC * d, const char * data)
{
	struct TPacketCGPlayerDelete * pinfo = (struct TPacketCGPlayerDelete *) data;
	const TAccountTable & c_rAccountTable = d->GetAccountTable();

	if (!c_rAccountTable.id)
	{
		metin2::log::error("PlayerDelete: no login data");
		return;
	}

	char szPrivateCode[sizeof(pinfo->private_code)];
	CopyPacketString(szPrivateCode, pinfo->private_code);

	metin2::log::info( "PlayerDelete: login: {} index: {:d}, social_id {}", c_rAccountTable.login, pinfo->index, szPrivateCode);

	if (pinfo->index >= PLAYER_PER_ACCOUNT)
	{
		metin2::log::error("PlayerDelete: index overflow {:d}, login: {}", pinfo->index, c_rAccountTable.login);
		return;
	}

	if (!c_rAccountTable.players[pinfo->index].dwID)
	{
		metin2::log::error("PlayerDelete: Wrong Social ID index {:d}, login: {}", pinfo->index, c_rAccountTable.login);
		d->Packet(encode_byte(HEADER_GC_CHARACTER_DELETE_WRONG_SOCIAL_ID), 1);
		return;
	}

	TPlayerDeletePacket	player_delete_packet;

	trim_and_lower(c_rAccountTable.login, player_delete_packet.login, sizeof(player_delete_packet.login));
	player_delete_packet.player_id	= c_rAccountTable.players[pinfo->index].dwID;
	player_delete_packet.account_index	= pinfo->index;
	strlcpy(player_delete_packet.private_code, szPrivateCode, sizeof(player_delete_packet.private_code));

	db_clientdesc->DBPacket(HEADER_GD_PLAYER_DELETE, d->GetHandle(), &player_delete_packet, sizeof(TPlayerDeletePacket));
}

void CInputLogin::Entergame(DESC * d, const char * data)
{
	CHARACTER * ch;

	if (!(ch = d->GetCharacter()))
	{
		d->SetPhase(PHASE_CLOSE);
		return;
	}

	PIXEL_POSITION pos = ch->GetXYZ();

	if (!SECTREE_MANAGER::instance().GetMovablePosition(ch->GetMapIndex(), pos.x, pos.y, pos))
	{
		PIXEL_POSITION pos2;
		SECTREE_MANAGER::instance().GetRecallPositionByEmpire(ch->GetMapIndex(), ch->GetEmpire(), pos2);

		metin2::log::error("!GetMovablePosition (name {} {:d}x{:d} map {:d} changed to {:d}x{:d})", 
				ch->GetName(),
				pos.x, pos.y,
				ch->GetMapIndex(),
				pos2.x, pos2.y);
		pos = pos2;
	}

	CGuildManager::instance().LoginMember(ch);

	// Add the character to the map 
	ch->Show(ch->GetMapIndex(), pos.x, pos.y, pos.z);

	SECTREE_MANAGER::instance().SendNPCPosition(ch);
	ch->ReviveInvisible(5);

	d->SetPhase(PHASE_GAME);

	if(ch->GetItemAward_cmd())																		//When entering the game phase
		quest::CQuestManager::instance().ItemInformer(ch->GetPlayerID(),ch->GetItemAward_vnum());	//call questmanager

	metin2::log::info( "ENTERGAME: {} {:d}x{:d}x{:d} {} map_index {:d}", 
			ch->GetName(), ch->GetX(), ch->GetY(), ch->GetZ(), d->GetHostName(), ch->GetMapIndex());

	if (ch->GetHorseLevel() > 0)
	{
		ch->EnterHorse();
	}

	// Start recording play time
	ch->ResetPlayTime();

	// Add auto-save event
	ch->StartSaveEvent();
	ch->StartRecoveryEvent();
	ch->StartCheckSpeedHackEvent();

	CPVPManager::instance().Connect(ch);
	CPVPManager::instance().SendList(d);

	MessengerManager::instance().Login(ch->GetName());

	CPartyManager::instance().SetParty(ch);
	CGuildManager::instance().SendGuildWar(ch);

	building::CManager::instance().SendLandList(d, ch->GetMapIndex());

	marriage::CManager::instance().Login(ch);

	TPacketGCTime p;
	p.bHeader = HEADER_GC_TIME;
	p.time = get_global_time();
	d->Packet(&p, sizeof(p));

	TPacketGCChannel p2;
	p2.header = HEADER_GC_CHANNEL;
	p2.channel = metin2::config::server().channel;
	d->Packet(&p2, sizeof(p2));

	ch->SendGreetMessage();

	_send_bonus_info(ch);

	for (int i = 0; i <= PREMIUM_MAX_NUM; ++i)
	{
		int remain = ch->GetPremiumRemainSeconds(i);

		if (remain <= 0)
			continue;

		ch->AddAffect(AFFECT_PREMIUM_START + i, POINT_NONE, 0, 0, remain, 0, true);
		metin2::log::info( "PREMIUM: {} type {:d} {:d}min", ch->GetName(), i, remain);
	}

	if (g_bCheckClientVersion)
	{
		int version = atoi(g_stClientVersion.c_str());
		int date = atoi(d->GetClientVersion());

		metin2::log::info( "VERSION CHECK {:d} {:d} {} {}", version, date, g_stClientVersion.c_str(), d->GetClientVersion());

		if (!d->GetClientVersion())
		{
			d->DelayedDisconnect(10);
		}
		else
		{
			if (version != date) // Fix
			{
				ch->ChatPacket(CHAT_TYPE_NOTICE, LC_TEXT("클라이언트 버전이 틀려 로그아웃 됩니다. 정상적으로 패치 후 접속하세요."));
				d->DelayedDisconnect(10);
				LogManager::instance().HackLog("VERSION_CONFLICT", ch);

				metin2::log::info( "VERSION : WRONG VERSION USER : account:{} name:{} hostName:{} server_version:{} client_version:{}",
						d->GetAccountTable().login,
						ch->GetName(),
						d->GetHostName(),
						g_stClientVersion.c_str(),
						d->GetClientVersion());
			}
		}
	}
	else
	{
		metin2::log::info( "VERSION : NO CHECK");
	}

	if (ch->IsGM() == true)
		ch->ChatPacket(CHAT_TYPE_COMMAND, "ConsoleEnable");

	if (ch->GetMapIndex() >= 10000)
	{
		if (CWarMapManager::instance().IsWarMap(ch->GetMapIndex()))
			ch->SetWarMap(CWarMapManager::instance().Find(ch->GetMapIndex()));
		else if (marriage::WeddingManager::instance().IsWeddingMap(ch->GetMapIndex()))
			ch->SetWeddingMap(marriage::WeddingManager::instance().Find(ch->GetMapIndex()));
		else {
			ch->SetDungeon(CDungeonManager::instance().FindByMapIndex(ch->GetMapIndex()));
		}
	}
	else if (CArenaManager::instance().IsArenaMap(ch->GetMapIndex()) == true)
	{
		int memberFlag = CArenaManager::instance().IsMember(ch->GetMapIndex(), ch->GetPlayerID());
		if (memberFlag == MEMBER_OBSERVER)
		{
			ch->SetObserverMode(true);
			ch->SetArenaObserverMode(true);
			if (CArenaManager::instance().RegisterObserverPtr(ch, ch->GetMapIndex(), ch->GetX()/100, ch->GetY()/100))
			{
				metin2::log::info( "ARENA : Observer add failed");
			}

			if (ch->IsHorseRiding() == true)
			{
				ch->StopRiding();
				ch->HorseSummon(false);
			}
		}
		else if (memberFlag == MEMBER_DUELIST)
		{
			TPacketGCDuelStart duelStart;
			duelStart.header = HEADER_GC_DUEL_START;
			duelStart.wSize = sizeof(TPacketGCDuelStart);

			ch->GetDesc()->Packet(&duelStart, sizeof(TPacketGCDuelStart));

			if (ch->IsHorseRiding() == true)
			{
				ch->StopRiding();
				ch->HorseSummon(false);
			}

			CParty * pParty = ch->GetParty();
			if (pParty != nullptr)
			{
				if (pParty->GetMemberCount() == 2)
				{
					CPartyManager::instance().DeleteParty(pParty);
				}
				else
				{
					pParty->Quit(ch->GetPlayerID());
				}
			}
		}
		else if (memberFlag == MEMBER_NO)		
		{
			if (ch->GetGMLevel() == GM_PLAYER)
				ch->WarpSet(EMPIRE_START_X(ch->GetEmpire()), EMPIRE_START_Y(ch->GetEmpire()));
		}
		else
		{
			// wtf
		}
	}
	else if (ch->GetMapIndex() == 113)
	{
		// ox event map
		if (COXEventManager::instance().Enter(ch) == false)
		{
			// Not allowed into the ox map. If it's a player, send them to the village
			if (ch->GetGMLevel() == GM_PLAYER)
				ch->WarpSet(EMPIRE_START_X(ch->GetEmpire()), EMPIRE_START_Y(ch->GetEmpire()));
		}
	}
	else
	{
		if (CWarMapManager::instance().IsWarMap(ch->GetMapIndex()) ||
				marriage::WeddingManager::instance().IsWeddingMap(ch->GetMapIndex()))
		{
			if (!metin2::config::test_server())
				ch->WarpSet(EMPIRE_START_X(ch->GetEmpire()), EMPIRE_START_Y(ch->GetEmpire()));
		}
	}

	if (ch->GetHorseLevel() > 0)
	{
		uint32_t pid = ch->GetPlayerID();

		if (pid != 0 && CHorseNameManager::instance().GetHorseName(pid) == nullptr)
			db_clientdesc->DBPacket(HEADER_GD_REQ_HORSE_NAME, 0, &pid, sizeof(uint32_t));
	}

	// Show guidance when entering a neutral map
	if (metin2::config::game().notice_battle_zone)
	{
		if (FN_is_battle_zone(ch))
		{
			ch->ChatPacket(CHAT_TYPE_NOTICE, LC_TEXT("이 맵에선 강제적인 대전이 있을수 도 있습니다."));
			ch->ChatPacket(CHAT_TYPE_NOTICE, LC_TEXT("이 조항에 동의하지 않을시"));
			ch->ChatPacket(CHAT_TYPE_NOTICE, LC_TEXT("본인의 주성 및 부성으로 돌아가시기 바랍니다."));
		}
	}
}

void CInputLogin::Empire(DESC * d, const char * c_pData)
{
	const TPacketCGEmpire* p = reinterpret_cast<const TPacketCGEmpire*>(c_pData);

	// Empire 0 is "none": only 1 .. EMPIRE_MAX_NUM - 1 can be chosen.
	if (p->bEmpire == 0 || EMPIRE_MAX_NUM <= p->bEmpire)
	{
		d->SetPhase(PHASE_CLOSE);
		return;
	}

	const TAccountTable& r = d->GetAccountTable();

	if (!r.id)
	{
		metin2::log::error("EmpireSelect: no login data");
		return;
	}

	if (r.bEmpire != 0)
	{
		for (int i = 0; i < PLAYER_PER_ACCOUNT; ++i)
		{
			if (0 != r.players[i].dwID)
			{
				metin2::log::error("EmpireSelectFailed {:d}", r.players[i].dwID);
				return;
			}
		}
	}

	TEmpireSelectPacket pd;

	pd.dwAccountID = r.id;
	pd.bEmpire = p->bEmpire;

	db_clientdesc->DBPacket(HEADER_GD_EMPIRE_SELECT, d->GetHandle(), &pd, sizeof(pd));
}

// Whether the player behind a mark connection is the master of guild dwGuildID.
static bool IsMarkOwnerGuildMaster(DESC * d, uint32_t dwGuildID)
{
	DESC * pkOwner = DESC_MANAGER::instance().FindByHandle(d->GetMarkOwnerHandle());
	CHARACTER * ch = pkOwner ? pkOwner->GetCharacter() : nullptr;

	return ch && ch->GetGuild() && ch->GetGuild()->GetID() == dwGuildID && ch->GetGuild()->GetMasterPID() == ch->GetPlayerID();
}

int CInputLogin::GuildSymbolUpload(DESC * d, const char* c_pData, size_t uiBytes)
{
	if (uiBytes < sizeof(TPacketCGGuildSymbolUpload))
		return -1;

	metin2::log::info( "GuildSymbolUpload uiBytes {:d}", uiBytes);

	TPacketCGGuildSymbolUpload* p = (TPacketCGGuildSymbolUpload*) c_pData;

	if (uiBytes < p->size)
		return -1;

	int iSymbolSize = p->size - sizeof(TPacketCGGuildSymbolUpload);

	if (iSymbolSize <= 0 || iSymbolSize > 64 * 1024)
	{
		// Guild symbols larger than 64k cannot be uploaded
		// Disconnect and ignore
		d->SetPhase(PHASE_CLOSE);
		return 0;
	}

	if (!IsMarkOwnerGuildMaster(d, p->guild_id))
	{
		metin2::log::error("GuildSymbolUpload: guild {:d} not uploaded by its master, host {}", p->guild_id, d->GetHostName());
		d->SetPhase(PHASE_CLOSE);
		return 0;
	}

	// For a guild that owns no land.
	if (!metin2::config::test_server())
		if (!building::CManager::instance().FindLandByGuild(p->guild_id))
		{
			d->SetPhase(PHASE_CLOSE);
			return 0;
		}

	metin2::log::info( "GuildSymbolUpload Do Upload {:02X}{:02X}{:02X}{:02X} {:d}", c_pData[7], c_pData[8], c_pData[9], c_pData[10], sizeof(*p));

	CGuildMarkManager::instance().UploadSymbol(p->guild_id, iSymbolSize, (const uint8_t*)(c_pData + sizeof(*p)));
	CGuildMarkManager::instance().SaveSymbol(GUILD_SYMBOL_FILENAME);
	return iSymbolSize;
}

void CInputLogin::GuildSymbolCRC(DESC * d, const char* c_pData)
{
	const TPacketCGSymbolCRC & CGPacket = *((TPacketCGSymbolCRC *) c_pData);

	metin2::log::info( "GuildSymbolCRC {:d} {:d} {:d}", CGPacket.guild_id, CGPacket.crc, CGPacket.size);

	const CGuildMarkManager::TGuildSymbol * pkGS = CGuildMarkManager::instance().GetGuildSymbol(CGPacket.guild_id);

	if (!pkGS || pkGS->raw.empty())
		return;

	metin2::log::info( "  Server {:d} {:d}", pkGS->crc, pkGS->raw.size());

	if (pkGS->raw.size() != CGPacket.size || pkGS->crc != CGPacket.crc)
	{
		TPacketGCGuildSymbolData GCPacket;

		GCPacket.header = HEADER_GC_SYMBOL_DATA;
		GCPacket.size = sizeof(GCPacket) + pkGS->raw.size();
		GCPacket.guild_id = CGPacket.guild_id;

		d->BufferedPacket(&GCPacket, sizeof(GCPacket));
		d->Packet(&pkGS->raw[0], pkGS->raw.size());

		if (pkGS->raw.size() >= 4)
			metin2::log::info( "SendGuildSymbolHead {:02X}{:02X}{:02X}{:02X} Size {:d}",
					pkGS->raw[0], pkGS->raw[1], pkGS->raw[2], pkGS->raw[3], pkGS->raw.size());
	}
}

void CInputLogin::GuildMarkUpload(DESC * d, const char* c_pData)
{
	TPacketCGMarkUpload * p = (TPacketCGMarkUpload *) c_pData;
	CGuildManager& rkGuildMgr = CGuildManager::instance();
	CGuild * pkGuild;

	if (!(pkGuild = rkGuildMgr.FindGuild(p->gid)))
	{
		metin2::log::error("MARK_SERVER: GuildMarkUpload: no guild. gid {:d}", p->gid);
		return;
	}

	if (!IsMarkOwnerGuildMaster(d, p->gid))
	{
		metin2::log::error("MARK_SERVER: GuildMarkUpload: gid {:d} not uploaded by its master, host {}", p->gid, d->GetHostName());
		d->SetPhase(PHASE_CLOSE);
		return;
	}

	if (pkGuild->GetLevel() < metin2::config::game().mark.min_level)
	{
		metin2::log::info( "MARK_SERVER: GuildMarkUpload: level < {:d} ({:d})", metin2::config::game().mark.min_level, pkGuild->GetLevel());
		return;
	}

	CGuildMarkManager & rkMarkMgr = CGuildMarkManager::instance();

	metin2::log::info( "MARK_SERVER: GuildMarkUpload: gid {:d}", p->gid);

	bool isEmpty = true;

	for (uint32_t iPixel = 0; iPixel < SGuildMark::SIZE; ++iPixel)
		if (*((uint32_t *) p->image + iPixel) != 0x00000000)
			isEmpty = false;

	if (isEmpty)
		rkMarkMgr.DeleteMark(p->gid);
	else
		rkMarkMgr.SaveMark(p->gid, p->image);
}

void CInputLogin::GuildMarkIDXList(DESC * d, const char* c_pData)
{
	CGuildMarkManager & rkMarkMgr = CGuildMarkManager::instance();

	uint32_t bufSize = sizeof(uint16_t) * 2 * rkMarkMgr.GetMarkCount();
	char * buf = nullptr;

	if (bufSize > 0)
	{
		buf = (char *) malloc(bufSize);
		rkMarkMgr.CopyMarkIdx(buf);
	}

	TPacketGCMarkIDXList p;
	p.header = HEADER_GC_MARK_IDXLIST;
	p.bufSize = sizeof(p) + bufSize;
	p.count = rkMarkMgr.GetMarkCount();

	if (buf)
	{
		d->BufferedPacket(&p, sizeof(p));
		d->LargePacket(buf, bufSize);
		free(buf);
	}
	else
		d->Packet(&p, sizeof(p));

	metin2::log::info( "MARK_SERVER: GuildMarkIDXList {:d} bytes sent.", p.bufSize);
}

void CInputLogin::GuildMarkCRCList(DESC * d, const char* c_pData)
{
	TPacketCGMarkCRCList * pCG = (TPacketCGMarkCRCList *) c_pData;

	std::map<uint8_t, const SGuildMarkBlock *> mapDiffBlocks;
	CGuildMarkManager::instance().GetDiffBlocks(pCG->imgIdx, pCG->crclist, mapDiffBlocks);

	uint32_t blockCount = 0;
	TEMP_BUFFER buf(1024 * 1024); // 1M buffer

	for (auto it = mapDiffBlocks.begin(); it != mapDiffBlocks.end(); ++it)
	{
		uint8_t posBlock = it->first;
		const SGuildMarkBlock & rkBlock = *it->second;

		buf.write(&posBlock, sizeof(uint8_t));
		const uint32_t dwCompSize = static_cast<uint32_t>(rkBlock.m_sizeCompBuf);
		buf.write(&dwCompSize, sizeof(uint32_t));
		buf.write(rkBlock.m_abCompBuf, rkBlock.m_sizeCompBuf);

		++blockCount;
	}

	TPacketGCMarkBlock pGC;

	pGC.header = HEADER_GC_MARK_BLOCK;
	pGC.imgIdx = pCG->imgIdx;
	pGC.bufSize = buf.size() + sizeof(TPacketGCMarkBlock);
	pGC.count = blockCount;

	metin2::log::info( "MARK_SERVER: Sending blocks. (imgIdx {:d} diff {:d} size {:d})", pCG->imgIdx, mapDiffBlocks.size(), pGC.bufSize);

	if (buf.size() > 0)
	{
		d->BufferedPacket(&pGC, sizeof(TPacketGCMarkBlock));
		d->LargePacket(buf.read_peek(), buf.size());
	}
	else
		d->Packet(&pGC, sizeof(TPacketGCMarkBlock));
}

int CInputLogin::Analyze(DESC * d, uint8_t bHeader, const char * c_pData)
{
	int iExtraLen = 0;

	switch (bHeader)
	{
		case HEADER_CG_PONG:
		case HEADER_CG_TIME_SYNC:
			break;

		// Guild mark packets only come over a mark connection (HEADER_CG_MARK_LOGIN), which
		// is unencrypted and must not reach anything else.
		case HEADER_CG_MARK_CRCLIST:
		case HEADER_CG_MARK_IDXLIST:
		case HEADER_CG_MARK_UPLOAD:
		case HEADER_CG_GUILD_SYMBOL_UPLOAD:
		case HEADER_CG_SYMBOL_CRC:
			if (!d->IsMarkLogin())
			{
				metin2::log::error("guild mark packet {:d} without mark login, host {}", bHeader, d->GetHostName());
				d->SetPhase(PHASE_CLOSE);
				return 0;
			}
			break;

		default:
			if (d->IsMarkLogin())
			{
				metin2::log::error("mark connection sent packet {:d}, host {}", bHeader, d->GetHostName());
				d->SetPhase(PHASE_CLOSE);
				return 0;
			}
			break;
	}

	switch (bHeader)
	{
		// Character slots are handled while choosing a character, by an account, with no
		// character loaded yet.
		case HEADER_CG_CHARACTER_SELECT:
		case HEADER_CG_CHARACTER_CREATE:
		case HEADER_CG_CHARACTER_DELETE:
		case HEADER_CG_CHANGE_NAME:
		case HEADER_CG_EMPIRE:
			if (!d->IsPhase(PHASE_SELECT) || d->GetCharacter())
			{
				metin2::log::error("packet {:d} outside of character select, host {}", bHeader, d->GetHostName());
				return 0;
			}
			break;

		case HEADER_CG_LOGIN:
		case HEADER_CG_LOGIN2:
			if (!d->IsPhase(PHASE_LOGIN))
			{
				metin2::log::error("login packet {:d} outside of the login phase, host {}", bHeader, d->GetHostName());
				return 0;
			}
			break;
	}

	switch (bHeader)
	{
		case HEADER_CG_PONG:
			Pong(d);
			break;

		case HEADER_CG_TIME_SYNC:
			Handshake(d, c_pData);
			break;

		case HEADER_CG_LOGIN:
			Login(d, c_pData);
			break;

		case HEADER_CG_LOGIN2:
			LoginByKey(d, c_pData);
			break;

		case HEADER_CG_CHARACTER_SELECT:
			CharacterSelect(d, c_pData);
			break;

		case HEADER_CG_CHARACTER_CREATE:
			CharacterCreate(d, c_pData);
			break;

		case HEADER_CG_CHARACTER_DELETE:
			CharacterDelete(d, c_pData);
			break;

		case HEADER_CG_ENTERGAME:
			Entergame(d, c_pData);
			break;

		case HEADER_CG_EMPIRE:
			Empire(d, c_pData);
			break;

		case HEADER_CG_MOVE:
			break;

			///////////////////////////////////////
			// Guild Mark
			/////////////////////////////////////
		case HEADER_CG_MARK_CRCLIST:
			GuildMarkCRCList(d, c_pData);
			break;

		case HEADER_CG_MARK_IDXLIST:
			GuildMarkIDXList(d, c_pData);
			break;

		case HEADER_CG_MARK_UPLOAD:
			GuildMarkUpload(d, c_pData);
			break;

			//////////////////////////////////////
			// Guild Symbol
			/////////////////////////////////////
		case HEADER_CG_GUILD_SYMBOL_UPLOAD:
			if ((iExtraLen = GuildSymbolUpload(d, c_pData, m_iBufferLeft)) < 0)
				return -1;
			break;

		case HEADER_CG_SYMBOL_CRC:
			GuildSymbolCRC(d, c_pData);
			break;
			/////////////////////////////////////

		case HEADER_CG_HACK:
			break;

		case HEADER_CG_CHANGE_NAME:
			ChangeName(d, c_pData);
			break;

		case HEADER_CG_CLIENT_VERSION:
			Version(d->GetCharacter(), c_pData);
			break;

		case HEADER_CG_CLIENT_VERSION2:
			Version(d->GetCharacter(), c_pData);
			break;

		default:
			metin2::log::error("login phase does not handle this packet! header {:d}", bHeader);
			return (0);
	}

	return (iExtraLen);
}
}
