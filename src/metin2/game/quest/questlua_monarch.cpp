#include <metin2/game/empire/castle.h>
#include <metin2/game/characters/char.h>
#include <metin2/game/characters/char_manager.h>
#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/config.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/app/event.h>
#include <metin2/game/guilds/guild.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/quest/lua_incl.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/empire/monarch.h>
#include <metin2/game/protocol/p2p.h>
#include <metin2/game/protocol/packet.h>
#include <metin2/game/quest/questlua.h>
#include <metin2/game/quest/questmanager.h>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/empire/start_position.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/log/log.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace metin2::game {


namespace quest
{
	EVENTINFO(monarch_powerup_event_info)
	{
		int EmpireIndex;

		monarch_powerup_event_info() 
		: EmpireIndex( 0 )
		{
		}
	};

	// NOTE: Copied from SPacketGGMonarchTransfer for event data 
	EVENTINFO(monarch_transfer2_event_info)
	{
		uint8_t	bHeader;
		uint32_t	dwTargetPID;
		int32_t	x;
		int32_t	y;

		monarch_transfer2_event_info() 
		: bHeader( 0 )
		, dwTargetPID( 0 )
		, x( 0 )
		, y( 0 )
		{
		}
	};

	EVENTFUNC(monarch_powerup_event)
	{
		monarch_powerup_event_info * info =  dynamic_cast<monarch_powerup_event_info*>(event->info);

		if ( info == nullptr )
		{
			quest::error( "monarch_powerup_event> <Factor> Null pointer" );
			return 0;
		}

		CMonarch::instance().PowerUp(info->EmpireIndex, false);
		return 0;
	}

	EVENTINFO(monarch_defenseup_event_info)
	{
		int EmpireIndex;

		monarch_defenseup_event_info() 
		: EmpireIndex( 0 )
		{
		}
	};

	EVENTFUNC(monarch_defenseup_event)
	{
		monarch_powerup_event_info * info =  dynamic_cast<monarch_powerup_event_info*>(event->info);

		if ( info == nullptr )
		{
			quest::error( "monarch_defenseup_event> <Factor> Null pointer" );
			return 0;
		}

		CMonarch::instance().DefenseUp(info->EmpireIndex, false);
		return 0;
	}

	//
	// "monarch_" lua functions
	//
	int takemonarchmoney(lua_State * L)
	{
		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr(); 

		int nMoney = (int)lua_tonumber(L,1);
		int nPID = ch->GetPlayerID();
		int nEmpire = ch->GetEmpire();
		nMoney = nMoney * 10000;

		metin2::log::info("[MONARCH] Take Money Empire({:d}) pid({:d}) Money({:d})", ch->GetEmpire(), ch->GetPlayerID(), nMoney);

		db_clientdesc->DBPacketHeader(HEADER_GD_TAKE_MONARCH_MONEY, ch->GetDesc()->GetHandle(), sizeof(int) * 3);
		db_clientdesc->Packet(&nEmpire, sizeof(int)); 
		db_clientdesc->Packet(&nPID, sizeof(int));
		db_clientdesc->Packet(&nMoney, sizeof(int));
		return 1;
	}

	int	 is_guild_master(lua_State * L)
	{
		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr(); 

		if (ch->GetGuild()	)
		{
			TGuildMember * pMember = ch->GetGuild()->GetMember(ch->GetPlayerID());

			if (pMember)
			{
				if (pMember->grade <= 4)
				{
					lua_pushnumber(L ,1);
					return 1;
				}
			}

		}
		lua_pushnumber(L ,0);

		return 1;
	}

	int monarch_bless(lua_State * L)
	{
		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr(); 

		if (false==ch->IsMonarch())
		{
			if (!ch->IsGM())
			{
				ch->ChatPacket(CHAT_TYPE_INFO ,LC_TEXT("군주의 자격을 가지고 있지 않습니다"));
				quest::error("No Monarch pid {:d} ", ch->GetPlayerID());
				return 0;
			}
		}

		int HealPrice = quest::CQuestManager::instance().GetEventFlag("MonarchHealGold");
		if (HealPrice == 0)
			HealPrice = 2000000;	// 2 million

		if (CMonarch::instance().HealMyEmpire(ch, HealPrice))
		{
			char szNotice[256];
			snprintf(szNotice, sizeof(szNotice),
					LC_TEXT("군주의 축복으로 이지역 %s 유저는 HP,SP가 모두 채워집니다."), EMPIRE_NAME(ch->GetEmpire()));
			SendNoticeMap(szNotice, ch->GetMapIndex(), false);

			ch->ChatPacket(CHAT_TYPE_INFO ,LC_TEXT("군주의 축복을 사용하였습니다."));
		}

		return 1;
	}

	// Monarch's Lion's Roar quest function 
	int monarch_powerup(lua_State * L)
	{
		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr(); 

		if (!ch)
			return 0;

		//Monarch check 
		if (false==ch->IsMonarch())
		{
			if (!ch->IsGM())
			{
				ch->ChatPacket(CHAT_TYPE_INFO ,LC_TEXT("군주의 자격을 가지고 있지 않습니다"));
				quest::error("No Monarch pid {:d} ", ch->GetPlayerID());
				return 0;
			}
		}

		//Check monarch treasury
		int	money_need = 5000000;	// 5 million
		if (!CMonarch::instance().IsMoneyOk(money_need, ch->GetEmpire()))
		{
			int NationMoney = CMonarch::instance().GetMoney(ch->GetEmpire());
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("국고에 돈이 부족합니다. 현재 : %u 필요금액 : %u"), NationMoney, money_need);
			return 0;
		}

		if (!CMonarch::instance().CheckPowerUpCT(ch->GetEmpire()))
		{
			int	next_sec = CMonarch::instance().GetPowerUpCT(ch->GetEmpire()) / metin2::config::server().passes_per_sec;
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("쿨타임 적용중  %d 후 사용가능"), next_sec);
			return 0;
		}

		//Apply monarch's Lion's Roar	
		CMonarch::instance().PowerUp(ch->GetEmpire(), true); 

		//Monarch's Lion's Roar duration 	
		int g_nMonarchPowerUpCT = 60 * 3;

		monarch_powerup_event_info* info = AllocEventInfo<monarch_powerup_event_info>();

		info->EmpireIndex = ch->GetEmpire();

		event_create(quest::monarch_powerup_event, info, PASSES_PER_SEC(g_nMonarchPowerUpCT));

		CMonarch::instance().SendtoDBDecMoney(5000000, ch->GetEmpire(), ch);

		char szNotice[256];
		snprintf(szNotice, sizeof(szNotice), LC_TEXT("군주의 사자후 영향으로 이지역 %s 유저는 3분간 10 %% 의 공격력이  증가됩니다"), EMPIRE_NAME(ch->GetEmpire()));

		SendNoticeMap(szNotice, ch->GetMapIndex(), false);

		return 1;		
	}
	// Monarch's Diamond Fist quest function 
	int monarch_defenseup(lua_State * L)
	{
		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr(); 

		if (!ch)
			return 0;

		
		//Monarch check	

		if (false==ch->IsMonarch())
		{
			if (!ch->IsGM())
			{
				ch->ChatPacket(CHAT_TYPE_INFO ,LC_TEXT("군주의 자격을 가지고 있지 않습니다"));
				quest::error("No Monarch pid {:d} ", ch->GetPlayerID());
				return 0;
			}
		}

		int	money_need = 5000000;	// 5 million
		if (!CMonarch::instance().IsMoneyOk(money_need, ch->GetEmpire()))
		{
			int NationMoney = CMonarch::instance().GetMoney(ch->GetEmpire());
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("국고에 돈이 부족합니다. 현재 : %u 필요금액 : %u"), NationMoney, money_need);
			return 0;
		}

		if (!CMonarch::instance().CheckDefenseUpCT(ch->GetEmpire()))
		{
			int	next_sec = CMonarch::instance().GetDefenseUpCT(ch->GetEmpire()) / metin2::config::server().passes_per_sec;
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("쿨타임 적용중  %d 후 사용가능"), next_sec);
			return 0;
		}	

		//Apply monarch's Diamond Fist	
		CMonarch::instance().DefenseUp(ch->GetEmpire(), true); 

		//Monarch's Diamond Fist duration
		int g_nMonarchDefenseUpCT = 60 * 3;

		monarch_defenseup_event_info* info = AllocEventInfo<monarch_defenseup_event_info>();

		info->EmpireIndex = ch->GetEmpire();

		event_create(quest::monarch_defenseup_event, info, PASSES_PER_SEC(g_nMonarchDefenseUpCT));

		CMonarch::instance().SendtoDBDecMoney(5000000, ch->GetEmpire(), ch);

		char szNotice[256];
		snprintf(szNotice, sizeof(szNotice), LC_TEXT("군주의 금강권 영향으로 이지역 %s 유저는 3분간 10 %% 의 방어력이  증가됩니다"), EMPIRE_NAME(ch->GetEmpire()));

		SendNoticeMap(szNotice, ch->GetMapIndex(), false);

		return 1;
	}

	int is_monarch(lua_State * L)
	{
		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr(); 
		if (!ch)
			return 0;
		lua_pushnumber(L, ch->IsMonarch());
		return 1;
	}

	int spawn_mob(lua_State * L)
	{
		if (!lua_isnumber(L, 1))
		{
			quest::error("invalid argument");
			return 0;
		}

		uint32_t mob_vnum = (uint32_t)lua_tonumber(L, 1);

		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr(); 

		if (!ch)
			return 0;

		const CMob * pkMob = nullptr;

		if (!(pkMob = CMobManager::instance().Get(mob_vnum)))
			if (pkMob == nullptr)
			{
				ch->ChatPacket(CHAT_TYPE_INFO, "No such mob by that vnum");
				return 0;
			}

		if (false == ch->IsMonarch())
		{
			if (!ch->IsGM())
			{
				ch->ChatPacket(CHAT_TYPE_INFO ,LC_TEXT("군주의 자격을 가지고 있지 않습니다"));
				quest::error("No Monarch pid {:d} ", ch->GetPlayerID());
				return 0;
			}
		}

		uint32_t dwQuestIdx = CQuestManager::instance().GetCurrentPC()->GetCurrentQuestIndex();

		bool ret = false;
		CHARACTER * mob = nullptr;

		{
			int32_t x = ch->GetX();
			int32_t y = ch->GetY();
			{
				mob = CHARACTER_MANAGER::instance().SpawnMob(mob_vnum, ch->GetMapIndex(), x, y, 0, pkMob->m_table.bType == CHAR_TYPE_STONE, -1);
			}

			if (mob)
			{

				mob->SetQuestBy(dwQuestIdx);

				if (!ret)
				{
					ret = true;
					lua_pushnumber(L, (uint32_t) mob->GetVID());
				}
			}
		}

		return 1;
	}

	int spawn_guard(lua_State * L)
	{
		// mob_level(0-2), region(0~3)
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
		{
			quest::error("invalid argument");
			return 0;
		}

		uint32_t	group_vnum		= (uint32_t)lua_tonumber(L,1);
		int		region_index	= (int)lua_tonumber(L, 2);
		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr(); 
		if (!ch)
			return 0;

		if (false==ch->IsMonarch())
		{
			if (!ch->IsGM())
			{
				ch->ChatPacket(CHAT_TYPE_INFO ,LC_TEXT("군주의 자격을 가지고 있지 않습니다"));
				quest::error("No Monarch pid {:d} ", ch->GetPlayerID());
				return 0;
			}
		}

		if (false==castle_is_my_castle(ch->GetEmpire(), ch->GetMapIndex()))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("성에서만 사용할 수 있는 기능입니다."));
			return 0;
		}

		CHARACTER *		guard_leader = nullptr;
		{
			int	money_need = castle_cost_of_hiring_guard(group_vnum);

			//Check monarch treasury
			if (!CMonarch::instance().IsMoneyOk(money_need, ch->GetEmpire()))
			{
				int NationMoney = CMonarch::instance().GetMoney(ch->GetEmpire());
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("국고에 돈이 부족합니다. 현재 : %u 필요금액 : %u"), NationMoney, money_need);
				return 0;
			}
			guard_leader = castle_spawn_guard(ch->GetEmpire(), group_vnum, region_index);

			if (guard_leader)
			{
				// Reduce treasury
				CMonarch::instance().SendtoDBDecMoney(money_need, ch->GetEmpire(), ch);
				castle_save();
			}
		}

		return 1;
	}

	int monarch_warp(lua_State * L)
	{
		if (!lua_isstring(L, 1))
		{
			quest::error("invalid argument");
			return 0;
		}

		std::string name = lua_tostring(L, 1);

		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr(); 
		if (!ch)
			return 0;

		if (!CMonarch::instance().IsMonarch(ch->GetPlayerID(), ch->GetEmpire()))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("군주만이 사용 가능한 기능입니다"));
			return 0;
		}

		//Check monarch cooldown
		if (!ch->IsMCOK(CHARACTER::MI_WARP))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%d 초간 쿨타임이 적용중입니다."), ch->GetMCLTime(CHARACTER::MI_WARP));	
			return 0;
		}

		//Monarch mob summon cost 
		const int WarpPrice = 10000;

		//Check monarch treasury 
		if (!CMonarch::instance().IsMoneyOk(WarpPrice, ch->GetEmpire()))
		{
			int NationMoney = CMonarch::instance().GetMoney(ch->GetEmpire());
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("국고에 돈이 부족합니다. 현재 : %u 필요금액 : %u"), NationMoney, WarpPrice);
			return 0;	
		}

		int x, y;

		CHARACTER * tch = CHARACTER_MANAGER::instance().FindPC(name.c_str());

		if (!tch)
		{
			CCI * pkCCI = P2P_MANAGER::instance().Find(name.c_str());

			if (pkCCI)
			{
				if (pkCCI->bEmpire != ch->GetEmpire())
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("타제국 유저에게는 이동할수 없습니다"));
					return 0;
				}
				if (pkCCI->bChannel != metin2::config::server().channel)
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("해당 유저는 %d 채널에 있습니다. (현재 채널 %d)"), pkCCI->bChannel, metin2::config::server().channel);
					return 0;
				}

				if (!IsMonarchWarpZone(pkCCI->lMapIndex))
				{
					ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT("해당 지역으로 이동할 수 없습니다."));	
					return 0;
				}

				PIXEL_POSITION pos;

				if (!SECTREE_MANAGER::instance().GetCenterPositionOfMap(pkCCI->lMapIndex, pos))
					ch->ChatPacket(CHAT_TYPE_INFO, "Cannot find map (index %d)", pkCCI->lMapIndex);
				else
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s 에게로 이동합니다"), name.c_str());
					ch->WarpSet(pos.x, pos.y);

					//Deduct monarch money	
					CMonarch::instance().SendtoDBDecMoney(WarpPrice, ch->GetEmpire(), ch);

					//Reset cooldown 
					ch->SetMC(CHARACTER::MI_WARP);
				}

			}
			else if (nullptr == CHARACTER_MANAGER::instance().FindPC(name.c_str()))
			{
				ch->ChatPacket(CHAT_TYPE_INFO, "There is no one by that name");
			}

			return 0;
		}
		else
		{
			if (tch->GetEmpire() != ch->GetEmpire())
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("타제국 유저에게는 이동할수 없습니다"));
				return 0;
			}

			if (!IsMonarchWarpZone(tch->GetMapIndex()))
			{
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT("해당 지역으로 이동할 수 없습니다."));
				return 0;
			}

			x = tch->GetX();
			y = tch->GetY();
		}

		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s 에게로 이동합니다"), name.c_str());
		ch->WarpSet(x,y);
		ch->Stop();

		//Deduct monarch money	
		CMonarch::instance().SendtoDBDecMoney(WarpPrice, ch->GetEmpire(), ch);

		//Reset cooldown 
		ch->SetMC(CHARACTER::MI_WARP); 

		return 0;
	}

	int empire_info(lua_State * L)
	{
		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr(); 

		if (nullptr == ch)
			return false;

		MonarchInfo * p = CMonarch::instance().GetMonarch();

		if (CMonarch::instance().IsMonarch(ch->GetPlayerID(), ch->GetEmpire()))	
		{
			ch->ChatPacket(CHAT_TYPE_INFO,LC_TEXT("나의 군주 정보"));

			for (int n = 1; n < 4; ++n)
			{
				if (n == ch->GetEmpire())
					ch->ChatPacket(CHAT_TYPE_INFO,LC_TEXT("[%s군주] : %s  보유금액 %lld "), EMPIRE_NAME(n), p->name[n], p->money[n]);
				else
					ch->ChatPacket(CHAT_TYPE_INFO,LC_TEXT("[%s군주] : %s  "), EMPIRE_NAME(n), p->name[n]);
			}
		}
		else
		{
			ch->ChatPacket(CHAT_TYPE_INFO,LC_TEXT("군주 정보"));

			for (int n = 1; n < 4; ++n)
				ch->ChatPacket(CHAT_TYPE_INFO,LC_TEXT("[%s군주] : %s  "), EMPIRE_NAME(n), p->name[n]);
		}

		return 0;
	}

	int monarch_transfer(lua_State * L)
	{
		if (!lua_isstring(L, 1))
		{
			quest::error("invalid argument");
			return 0;
		}

		std::string name = lua_tostring(L, 1);

		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr(); 

		if (!ch)
			return 0;

		if (!CMonarch::instance().IsMonarch(ch->GetPlayerID(), ch->GetEmpire()))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("군주만이 사용 가능한 기능입니다"));
			return 0;
		}

		// Check monarch cooldown
		if (!ch->IsMCOK(CHARACTER::MI_TRANSFER))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%d 초간 쿨타임이 적용중입니다."), ch->GetMCLTime(CHARACTER::MI_TRANSFER));	
			return 0;
		}

		// Monarch warp cost 
		const int WarpPrice = 10000;

		// Check monarch treasury 
		if (!CMonarch::instance().IsMoneyOk(WarpPrice, ch->GetEmpire()))
		{
			int NationMoney = CMonarch::instance().GetMoney(ch->GetEmpire());
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("국고에 돈이 부족합니다. 현재 : %u 필요금액 : %u"), NationMoney, WarpPrice);
			return 0;	
		}

		CHARACTER * tch = CHARACTER_MANAGER::instance().FindPC(name.c_str());

		if (!tch)
		{
			CCI * pkCCI = P2P_MANAGER::instance().Find(name.c_str());

			if (pkCCI)
			{
				if (pkCCI->bEmpire != ch->GetEmpire())
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("다른 제국 유저는 소환할 수 없습니다."));
					return 0;
				}

				if (pkCCI->bChannel != metin2::config::server().channel)
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s 님은 %d 채널에 접속 중 입니다. (현재 채널: %d)"), name.c_str(), pkCCI->bChannel, metin2::config::server().channel);
					return 0;
				}

				if (!IsMonarchWarpZone(pkCCI->lMapIndex))
				{
					ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT("해당 지역으로 이동할 수 없습니다."));	
					return 0;
				}
				if (!IsMonarchWarpZone(ch->GetMapIndex()))
				{
					ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT("해당 지역으로 소환할 수 없습니다."));
					return 0;
				}

				TPacketGGTransfer pgg;

				pgg.bHeader = HEADER_GG_TRANSFER;
				strlcpy(pgg.szName, name.c_str(), sizeof(pgg.szName));
				pgg.lX = ch->GetX();
				pgg.lY = ch->GetY();

				P2P_MANAGER::instance().Send(&pgg, sizeof(TPacketGGTransfer));
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s 님을 소환하였습니다."), name.c_str());

				// Deduct monarch money
				CMonarch::instance().SendtoDBDecMoney(WarpPrice, ch->GetEmpire(), ch);

				// Reset cooldown 
				ch->SetMC(CHARACTER::MI_TRANSFER);
			}
			else
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("입력하신 이름을 가진 사용자가 없습니다."));
			}

			return 0;
		}

		if (ch == tch)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("자신을 소환할 수 없습니다."));
			return 0;
		}

		if (tch->GetEmpire() != ch->GetEmpire())
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("다른 제국 유저는 소환할 수 없습니다."));
			return 0;
		}

		if (!IsMonarchWarpZone(tch->GetMapIndex()))
		{
			ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT("해당 지역으로 이동할 수 없습니다."));
			return 0;
		}
		if (!IsMonarchWarpZone(ch->GetMapIndex()))
		{
			ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT("해당 지역으로 소환할 수 없습니다."));
			return 0;
		}
		tch->WarpSet(ch->GetX(), ch->GetY(), ch->GetMapIndex());

		// Deduct monarch money	
		CMonarch::instance().SendtoDBDecMoney(WarpPrice, ch->GetEmpire(), ch);
		// Reset cooldown 
		ch->SetMC(CHARACTER::MI_TRANSFER);
		return 0;
	}

	int monarch_notice(lua_State * L)
	{
		if (!lua_isstring(L, 1))
			return 0;

		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();

		if (ch == nullptr)
			return 0;

		if (ch->IsMonarch() == false)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("군주만이 사용 가능한 기능입니다"));
			return 0;
		}

		std::string strNotice = lua_tostring(L, 1);

		if (strNotice.length() > 0)
			SendMonarchNotice(ch->GetEmpire(), strNotice.c_str());

		return 0;
	}

	int monarch_mob(lua_State * L)
	{
		if (!lua_isstring(L, 1))
			return 0;

		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();

		if (ch == nullptr)
			return 0;

		std::string vnum(lua_tostring(L, 1));
		do_monarch_mob(ch, vnum.c_str(), 0, 0);
		return 0;
	}

	EVENTFUNC(monarch_transfer2_event)
	{
		monarch_transfer2_event_info* info = dynamic_cast<monarch_transfer2_event_info*>(event->info);

		if ( info == nullptr )
		{
			quest::error( "monarch_transfer2_event> <Factor> Null pointer" );
			return 0;
		}

		CHARACTER * pTargetChar = CHARACTER_MANAGER::instance().FindByPID(info->dwTargetPID);

		if (pTargetChar != nullptr)
		{
			unsigned int qIndex = quest::CQuestManager::instance().GetQuestIndexByName("monarch_transfer");

			if (qIndex != 0)
			{
				pTargetChar->SetQuestFlag("monarch_transfer.x", info->x);
				pTargetChar->SetQuestFlag("monarch_transfer.y", info->y);
				quest::CQuestManager::instance().Letter(pTargetChar->GetPlayerID(), qIndex, 0);
			}
		}

		return 0;
	}

	int monarch_transfer2(lua_State* L)
	{
		if (lua_isstring(L, 1) == false) return 0;

		CHARACTER * ch = CQuestManager::instance().GetCurrentCharacterPtr();
		if (ch == nullptr) return false;

		if (CMonarch::instance().IsMonarch(ch->GetPlayerID(), ch->GetEmpire()) == false)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("군주만이 사용 가능한 기능입니다"));
			return 0;
		}

		if (ch->IsMCOK(CHARACTER::MI_TRANSFER) == false)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%d 초간 쿨타임이 적용중입니다."), ch->GetMCLTime(CHARACTER::MI_TRANSFER));
			return 0;
		}

		const int ciTransferCost = 10000;

		if (CMonarch::instance().IsMoneyOk(ciTransferCost, ch->GetEmpire()) == false)
		{
			int NationMoney = CMonarch::instance().GetMoney(ch->GetEmpire());
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("국고에 돈이 부족합니다. 현재 : %u 필요금액 : %u"), NationMoney, ciTransferCost);
			return 0;
		}

		std::string strTargetName = lua_tostring(L, 1);

		CHARACTER * pTargetChar = CHARACTER_MANAGER::instance().FindPC(strTargetName.c_str());

		if (pTargetChar == nullptr)
		{
			CCI* pCCI = P2P_MANAGER::instance().Find(strTargetName.c_str());

			if (pCCI != nullptr)
			{
				if (pCCI->bEmpire != ch->GetEmpire())
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("다른 제국 유저는 소환할 수 없습니다."));
					return 0;
				}

				if (pCCI->bChannel != metin2::config::server().channel)
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s 님은 %d 채널에 접속중입니다. (현재 채널: %d)"),
						   strTargetName.c_str(), pCCI->bChannel, metin2::config::server().channel);
					return 0;
				}

				if (!IsMonarchWarpZone(pCCI->lMapIndex))
				{
					ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT("해당 지역으로 이동할 수 없습니다."));
					return 0;
				}
				if (!IsMonarchWarpZone(ch->GetMapIndex()))
				{
					ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT("해당 지역으로 소환할 수 없습니다."));
					return 0;
				}

				TPacketMonarchGGTransfer packet;
				packet.bHeader = HEADER_GG_MONARCH_TRANSFER;
				packet.dwTargetPID = pCCI->dwPID;
				packet.x = ch->GetX();
				packet.y = ch->GetY();

				P2P_MANAGER::instance().Send(&packet, sizeof(TPacketMonarchGGTransfer));
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("소환 요청을 보냈습니다"));

				CMonarch::instance().SendtoDBDecMoney(ciTransferCost, ch->GetEmpire(), ch);
				ch->SetMC(CHARACTER::MI_TRANSFER);
			}
			else
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("입력하신 이름을 가진 사용자가 없습니다."));
				return 0;
			}
		}
		else
		{
			if (pTargetChar == ch)
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("자신을 소환할 수 없습니다."));
				return 0;
			}

			if (pTargetChar->GetEmpire() != ch->GetEmpire())
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("다른 제국 유저는 소환할 수 없습니다."));
				return 0;
			}

			if (DISTANCE_APPROX(pTargetChar->GetX() - ch->GetX(), pTargetChar->GetY() - ch->GetY()) <= 5000)
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s 님은 근처에 있습니다"), pTargetChar->GetName());
				return 0;
			}

			if (!IsMonarchWarpZone(pTargetChar->GetMapIndex()))
			{
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT("해당 지역으로 이동할 수 없습니다."));
				return 0;
			}
			if (!IsMonarchWarpZone(ch->GetMapIndex()))
			{
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT("해당 지역으로 소환할 수 없습니다."));
				return 0;
			}

			monarch_transfer2_event_info* info = AllocEventInfo<monarch_transfer2_event_info>();

			info->bHeader = HEADER_GG_MONARCH_TRANSFER;
			info->dwTargetPID = pTargetChar->GetPlayerID();
			info->x = ch->GetX();
			info->y = ch->GetY();

			event_create(monarch_transfer2_event, info, 1);

			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("소환 요청을 보냈습니다"));

			CMonarch::instance().SendtoDBDecMoney(ciTransferCost, ch->GetEmpire(), ch);
			ch->SetMC(CHARACTER::MI_TRANSFER);
			return 0;
		}

		return 0;
	}

	void RegisterMonarchFunctionTable()
	{
		luaL_reg Monarch_functions[] = 
		{
			{ "takemonarchmoney",		takemonarchmoney	},
			{ "isguildmaster",			is_guild_master		},
			{ "ismonarch",				is_monarch 			},
			{ "monarchbless",			monarch_bless		},
			{ "monarchpowerup",			monarch_powerup		},
			{ "monarchdefenseup",		monarch_defenseup	},
			{ "spawnmob",				spawn_mob			},
			{ "spawnguard",				spawn_guard			},
//			{ "frog_to_empire_money",	frog_to_empire_money		},
			{ "warp",					monarch_warp 		},
			{ "transfer",				monarch_transfer	},
			{ "transfer2",				monarch_transfer2	},
			{ "info",					empire_info 		},	
			{ "notice",					monarch_notice		},
			{ "monarch_mob",			monarch_mob			},

			{ nullptr,						nullptr				}
		};

		CQuestManager::instance().AddLuaFunctionTable("oh", Monarch_functions);
	}

}
}
