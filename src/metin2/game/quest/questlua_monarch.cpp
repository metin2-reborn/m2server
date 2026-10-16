#include <metin2/config/config.h>
#include <metin2/game/empire/castle.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
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
#include <string>

namespace metin2::game {


namespace quest
{
	EVENTINFO(monarch_powerup_event_info)
	{
		int empire_index;

		monarch_powerup_event_info() 
		: empire_index( 0 )
		{
		}
	};

	EVENTINFO(monarch_transfer2_event_info)
	{
		uint8_t	header;
		uint32_t	target_pid;
		int32_t	x;
		int32_t	y;

		monarch_transfer2_event_info() 
		: header( 0 )
		, target_pid( 0 )
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
			quest::error("Monarch power-up event has no info");
			return 0;
		}

		monarch::instance().power_up(info->empire_index, false);
		return 0;
	}

	EVENTINFO(monarch_defenseup_event_info)
	{
		int empire_index;

		monarch_defenseup_event_info() 
		: empire_index( 0 )
		{
		}
	};

	EVENTFUNC(monarch_defenseup_event)
	{
		monarch_powerup_event_info * info =  dynamic_cast<monarch_powerup_event_info*>(event->info);

		if ( info == nullptr )
		{
			quest::error("Monarch defense-up event has no info");
			return 0;
		}

		monarch::instance().defense_up(info->empire_index, false);
		return 0;
	}

	auto takemonarchmoney(lua_State * lua) -> int
	{
		character * ch = quest_manager::instance().current_character_ptr(); 

		int nMoney = (int)lua_tonumber(lua,1);
		int nPID = ch->player_id();
		int nEmpire = ch->empire();
		nMoney = nMoney * 10000;

		metin2::log::info("Monarch: pid {1:d} takes {2:d} gold from empire {0:d}", ch->empire(), ch->player_id(), nMoney);

		db_clientdesc->db_packet_header(HEADER_GD_TAKE_MONARCH_MONEY, ch->get_desc()->handle(), sizeof(int) * 3);
		db_clientdesc->packet(&nEmpire, sizeof(int)); 
		db_clientdesc->packet(&nPID, sizeof(int));
		db_clientdesc->packet(&nMoney, sizeof(int));
		return 1;
	}

	auto	 is_guild_master(lua_State * lua) -> int
	{
		character * ch = quest_manager::instance().current_character_ptr(); 

		if (ch->get_guild()	)
		{
			guild_member * pMember = ch->get_guild()->member(ch->player_id());

			if (pMember)
			{
				if (pMember->grade <= 4)
				{
					lua_pushnumber(lua ,1);
					return 1;
				}
			}

		}
		lua_pushnumber(lua ,0);

		return 1;
	}

	auto monarch_bless(lua_State * lua) -> int
	{
		character * ch = quest_manager::instance().current_character_ptr(); 

		if (false==ch->is_monarch())
		{
			if (!ch->is_gm())
			{
				ch->chat_packet(CHAT_TYPE_INFO ,lc_text("군주의 자격을 가지고 있지 않습니다"));
				quest::error("Player (pid {:d}) is not a monarch", ch->player_id());
				return 0;
			}
		}

		int HealPrice = quest::quest_manager::instance().event_flag("MonarchHealGold");
		if (HealPrice == 0)
			HealPrice = 2000000;

		if (monarch::instance().heal_my_empire(ch, HealPrice))
		{
			char szNotice[256];
			snprintf(szNotice, sizeof(szNotice),
					lc_text("군주의 축복으로 이지역 %s 유저는 HP,SP가 모두 채워집니다."), empire_name(ch->empire()));
			send_notice_map(szNotice, ch->map_index(), false);

			ch->chat_packet(CHAT_TYPE_INFO ,lc_text("군주의 축복을 사용하였습니다."));
		}

		return 1;
	}

	auto monarch_powerup(lua_State * lua) -> int
	{
		character * ch = quest_manager::instance().current_character_ptr(); 

		if (!ch)
			return 0;

		if (false==ch->is_monarch())
		{
			if (!ch->is_gm())
			{
				ch->chat_packet(CHAT_TYPE_INFO ,lc_text("군주의 자격을 가지고 있지 않습니다"));
				quest::error("Player (pid {:d}) is not a monarch", ch->player_id());
				return 0;
			}
		}

		int	money_need = 5000000;
		if (!monarch::instance().is_money_ok(money_need, ch->empire()))
		{
			int NationMoney = monarch::instance().money(ch->empire());
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("국고에 돈이 부족합니다. 현재 : %u 필요금액 : %u"), NationMoney, money_need);
			return 0;
		}

		if (!monarch::instance().check_power_up_ct(ch->empire()))
		{
			int	next_sec = monarch::instance().power_up_ct(ch->empire()) / metin2::config::server().passes_per_sec;
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("쿨타임 적용중  %d 후 사용가능"), next_sec);
			return 0;
		}

		monarch::instance().power_up(ch->empire(), true); 

		int g_nMonarchPowerUpCT = 60 * 3;

		monarch_powerup_event_info* info = alloc_event_info<monarch_powerup_event_info>();

		info->empire_index = ch->empire();

		event_create(quest::monarch_powerup_event, info, passes_per_sec(g_nMonarchPowerUpCT));

		monarch::instance().sendto_db_dec_money(5000000, ch->empire(), ch);

		char szNotice[256];
		snprintf(szNotice, sizeof(szNotice), lc_text("군주의 사자후 영향으로 이지역 %s 유저는 3분간 10 %% 의 공격력이  증가됩니다"), empire_name(ch->empire()));

		send_notice_map(szNotice, ch->map_index(), false);

		return 1;		
	}
	auto monarch_defenseup(lua_State * lua) -> int
	{
		character * ch = quest_manager::instance().current_character_ptr(); 

		if (!ch)
			return 0;

		

		if (false==ch->is_monarch())
		{
			if (!ch->is_gm())
			{
				ch->chat_packet(CHAT_TYPE_INFO ,lc_text("군주의 자격을 가지고 있지 않습니다"));
				quest::error("Player (pid {:d}) is not a monarch", ch->player_id());
				return 0;
			}
		}

		int	money_need = 5000000;
		if (!monarch::instance().is_money_ok(money_need, ch->empire()))
		{
			int NationMoney = monarch::instance().money(ch->empire());
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("국고에 돈이 부족합니다. 현재 : %u 필요금액 : %u"), NationMoney, money_need);
			return 0;
		}

		if (!monarch::instance().check_defense_up_ct(ch->empire()))
		{
			int	next_sec = monarch::instance().defense_up_ct(ch->empire()) / metin2::config::server().passes_per_sec;
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("쿨타임 적용중  %d 후 사용가능"), next_sec);
			return 0;
		}	

		monarch::instance().defense_up(ch->empire(), true); 

		int g_nMonarchDefenseUpCT = 60 * 3;

		monarch_defenseup_event_info* info = alloc_event_info<monarch_defenseup_event_info>();

		info->empire_index = ch->empire();

		event_create(quest::monarch_defenseup_event, info, passes_per_sec(g_nMonarchDefenseUpCT));

		monarch::instance().sendto_db_dec_money(5000000, ch->empire(), ch);

		char szNotice[256];
		snprintf(szNotice, sizeof(szNotice), lc_text("군주의 금강권 영향으로 이지역 %s 유저는 3분간 10 %% 의 방어력이  증가됩니다"), empire_name(ch->empire()));

		send_notice_map(szNotice, ch->map_index(), false);

		return 1;
	}

	auto is_monarch(lua_State * lua) -> int
	{
		character * ch = quest_manager::instance().current_character_ptr(); 
		if (!ch)
			return 0;
		lua_pushnumber(lua, ch->is_monarch());
		return 1;
	}

	auto spawn_mob(lua_State * lua) -> int
	{
		if (!lua_isnumber(lua, 1))
		{
			quest::error("Invalid arguments");
			return 0;
		}

		uint32_t mob_vnum = (uint32_t)lua_tonumber(lua, 1);

		character * ch = quest_manager::instance().current_character_ptr(); 

		if (!ch)
			return 0;

		const mob * pkMob = nullptr;

		if (!(pkMob = mob_manager::instance().get(mob_vnum)))
			if (pkMob == nullptr)
			{
				ch->chat_packet(CHAT_TYPE_INFO, "No such mob by that vnum");
				return 0;
			}

		if (false == ch->is_monarch())
		{
			if (!ch->is_gm())
			{
				ch->chat_packet(CHAT_TYPE_INFO ,lc_text("군주의 자격을 가지고 있지 않습니다"));
				quest::error("Player (pid {:d}) is not a monarch", ch->player_id());
				return 0;
			}
		}

		uint32_t dwQuestIdx = quest_manager::instance().current_pc()->current_quest_index();

		bool ret = false;
		character * mob = nullptr;

		{
			int32_t x = ch->x();
			int32_t y = ch->y();
			{
				mob = character_manager::instance().spawn_mob(mob_vnum, ch->map_index(), x, y, 0, pkMob->table.type == CHAR_TYPE_STONE, -1);
			}

			if (mob)
			{

				mob->set_quest_by(dwQuestIdx);

				if (!ret)
				{
					ret = true;
					lua_pushnumber(lua, (uint32_t) mob->get_vid());
				}
			}
		}

		return 1;
	}

	auto spawn_guard(lua_State * lua) -> int
	{
		if (!lua_isnumber(lua, 1) || !lua_isnumber(lua, 2))
		{
			quest::error("Invalid arguments");
			return 0;
		}

		uint32_t	group_vnum		= (uint32_t)lua_tonumber(lua,1);
		int		region_index	= (int)lua_tonumber(lua, 2);
		character * ch = quest_manager::instance().current_character_ptr(); 
		if (!ch)
			return 0;

		if (false==ch->is_monarch())
		{
			if (!ch->is_gm())
			{
				ch->chat_packet(CHAT_TYPE_INFO ,lc_text("군주의 자격을 가지고 있지 않습니다"));
				quest::error("Player (pid {:d}) is not a monarch", ch->player_id());
				return 0;
			}
		}

		if (false==castle_is_my_castle(ch->empire(), ch->map_index()))
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("성에서만 사용할 수 있는 기능입니다."));
			return 0;
		}

		character *		guard_leader = nullptr;
		{
			int	money_need = castle_cost_of_hiring_guard(group_vnum);

			if (!monarch::instance().is_money_ok(money_need, ch->empire()))
			{
				int NationMoney = monarch::instance().money(ch->empire());
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("국고에 돈이 부족합니다. 현재 : %u 필요금액 : %u"), NationMoney, money_need);
				return 0;
			}
			guard_leader = castle_spawn_guard(ch->empire(), group_vnum, region_index);

			if (guard_leader)
			{
				monarch::instance().sendto_db_dec_money(money_need, ch->empire(), ch);
				castle_save();
			}
		}

		return 1;
	}

	auto monarch_warp(lua_State * lua) -> int
	{
		if (!lua_isstring(lua, 1))
		{
			quest::error("Invalid arguments");
			return 0;
		}

		std::string name = lua_tostring(lua, 1);

		character * ch = quest_manager::instance().current_character_ptr(); 
		if (!ch)
			return 0;

		if (!monarch::instance().is_monarch(ch->player_id(), ch->empire()))
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("군주만이 사용 가능한 기능입니다"));
			return 0;
		}

		if (!ch->is_mcok(character::MI_WARP))
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("%d 초간 쿨타임이 적용중입니다."), ch->mcl_time(character::MI_WARP));	
			return 0;
		}

		const int WarpPrice = 10000;

		if (!monarch::instance().is_money_ok(WarpPrice, ch->empire()))
		{
			int NationMoney = monarch::instance().money(ch->empire());
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("국고에 돈이 부족합니다. 현재 : %u 필요금액 : %u"), NationMoney, WarpPrice);
			return 0;	
		}

		int x, y;

		character * tch = character_manager::instance().find_pc(name.c_str());

		if (!tch)
		{
			cci * pkCCI = p2_p_manager::instance().find(name.c_str());

			if (pkCCI)
			{
				if (pkCCI->empire != ch->empire())
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("타제국 유저에게는 이동할수 없습니다"));
					return 0;
				}
				if (pkCCI->channel != metin2::config::server().channel)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("해당 유저는 %d 채널에 있습니다. (현재 채널 %d)"), pkCCI->channel, metin2::config::server().channel);
					return 0;
				}

				if (!is_monarch_warp_zone(pkCCI->map_index))
				{
					ch->chat_packet (CHAT_TYPE_INFO, lc_text("해당 지역으로 이동할 수 없습니다."));	
					return 0;
				}

				pixel_position pos;

				if (!sectree_manager::instance().center_position_of_map(pkCCI->map_index, pos))
					ch->chat_packet(CHAT_TYPE_INFO, "Cannot find map (index %d)", pkCCI->map_index);
				else
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s 에게로 이동합니다"), name.c_str());
					ch->warp_set(pos.x, pos.y);

					monarch::instance().sendto_db_dec_money(WarpPrice, ch->empire(), ch);

					ch->set_mc(character::MI_WARP);
				}

			}
			else if (nullptr == character_manager::instance().find_pc(name.c_str()))
			{
				ch->chat_packet(CHAT_TYPE_INFO, "There is no one by that name");
			}

			return 0;
		}
		else
		{
			if (tch->empire() != ch->empire())
			{
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("타제국 유저에게는 이동할수 없습니다"));
				return 0;
			}

			if (!is_monarch_warp_zone(tch->map_index()))
			{
				ch->chat_packet (CHAT_TYPE_INFO, lc_text("해당 지역으로 이동할 수 없습니다."));
				return 0;
			}

			x = tch->x();
			y = tch->y();
		}

		ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s 에게로 이동합니다"), name.c_str());
		ch->warp_set(x,y);
		ch->stop();

		monarch::instance().sendto_db_dec_money(WarpPrice, ch->empire(), ch);

		ch->set_mc(character::MI_WARP); 

		return 0;
	}

	auto empire_info(lua_State * lua) -> int
	{
		character * ch = quest_manager::instance().current_character_ptr(); 

		if (nullptr == ch)
			return false;

		monarch_info * p = monarch::instance().get_monarch();

		if (monarch::instance().is_monarch(ch->player_id(), ch->empire()))	
		{
			ch->chat_packet(CHAT_TYPE_INFO,lc_text("나의 군주 정보"));

			for (int n = 1; n < 4; ++n)
			{
				if (n == ch->empire())
					ch->chat_packet(CHAT_TYPE_INFO,lc_text("[%s군주] : %s  보유금액 %lld "), empire_name(n), p->name[n], p->money[n]);
				else
					ch->chat_packet(CHAT_TYPE_INFO,lc_text("[%s군주] : %s  "), empire_name(n), p->name[n]);
			}
		}
		else
		{
			ch->chat_packet(CHAT_TYPE_INFO,lc_text("군주 정보"));

			for (int n = 1; n < 4; ++n)
				ch->chat_packet(CHAT_TYPE_INFO,lc_text("[%s군주] : %s  "), empire_name(n), p->name[n]);
		}

		return 0;
	}

	auto monarch_transfer(lua_State * lua) -> int
	{
		if (!lua_isstring(lua, 1))
		{
			quest::error("Invalid arguments");
			return 0;
		}

		std::string name = lua_tostring(lua, 1);

		character * ch = quest_manager::instance().current_character_ptr(); 

		if (!ch)
			return 0;

		if (!monarch::instance().is_monarch(ch->player_id(), ch->empire()))
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("군주만이 사용 가능한 기능입니다"));
			return 0;
		}

		if (!ch->is_mcok(character::MI_TRANSFER))
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("%d 초간 쿨타임이 적용중입니다."), ch->mcl_time(character::MI_TRANSFER));	
			return 0;
		}

		const int WarpPrice = 10000;

		if (!monarch::instance().is_money_ok(WarpPrice, ch->empire()))
		{
			int NationMoney = monarch::instance().money(ch->empire());
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("국고에 돈이 부족합니다. 현재 : %u 필요금액 : %u"), NationMoney, WarpPrice);
			return 0;	
		}

		character * tch = character_manager::instance().find_pc(name.c_str());

		if (!tch)
		{
			cci * pkCCI = p2_p_manager::instance().find(name.c_str());

			if (pkCCI)
			{
				if (pkCCI->empire != ch->empire())
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("다른 제국 유저는 소환할 수 없습니다."));
					return 0;
				}

				if (pkCCI->channel != metin2::config::server().channel)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s 님은 %d 채널에 접속 중 입니다. (현재 채널: %d)"), name.c_str(), pkCCI->channel, metin2::config::server().channel);
					return 0;
				}

				if (!is_monarch_warp_zone(pkCCI->map_index))
				{
					ch->chat_packet (CHAT_TYPE_INFO, lc_text("해당 지역으로 이동할 수 없습니다."));	
					return 0;
				}
				if (!is_monarch_warp_zone(ch->map_index()))
				{
					ch->chat_packet (CHAT_TYPE_INFO, lc_text("해당 지역으로 소환할 수 없습니다."));
					return 0;
				}

				packet_gg_transfer pgg;

				pgg.header = HEADER_GG_TRANSFER;
				strlcpy(pgg.name, name.c_str(), sizeof(pgg.name));
				pgg.x = ch->x();
				pgg.y = ch->y();

				p2_p_manager::instance().send(&pgg, sizeof(packet_gg_transfer));
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s 님을 소환하였습니다."), name.c_str());

				monarch::instance().sendto_db_dec_money(WarpPrice, ch->empire(), ch);

				ch->set_mc(character::MI_TRANSFER);
			}
			else
			{
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("입력하신 이름을 가진 사용자가 없습니다."));
			}

			return 0;
		}

		if (ch == tch)
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("자신을 소환할 수 없습니다."));
			return 0;
		}

		if (tch->empire() != ch->empire())
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("다른 제국 유저는 소환할 수 없습니다."));
			return 0;
		}

		if (!is_monarch_warp_zone(tch->map_index()))
		{
			ch->chat_packet (CHAT_TYPE_INFO, lc_text("해당 지역으로 이동할 수 없습니다."));
			return 0;
		}
		if (!is_monarch_warp_zone(ch->map_index()))
		{
			ch->chat_packet (CHAT_TYPE_INFO, lc_text("해당 지역으로 소환할 수 없습니다."));
			return 0;
		}
		tch->warp_set(ch->x(), ch->y(), ch->map_index());

		monarch::instance().sendto_db_dec_money(WarpPrice, ch->empire(), ch);
		ch->set_mc(character::MI_TRANSFER);
		return 0;
	}

	auto monarch_notice(lua_State * lua) -> int
	{
		if (!lua_isstring(lua, 1))
			return 0;

		character * ch = quest_manager::instance().current_character_ptr();

		if (ch == nullptr)
			return 0;

		if (ch->is_monarch() == false)
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("군주만이 사용 가능한 기능입니다"));
			return 0;
		}

		std::string strNotice = lua_tostring(lua, 1);

		if (strNotice.length() > 0)
			send_monarch_notice(ch->empire(), strNotice.c_str());

		return 0;
	}

	auto monarch_mob(lua_State * lua) -> int
	{
		if (!lua_isstring(lua, 1))
			return 0;

		character * ch = quest_manager::instance().current_character_ptr();

		if (ch == nullptr)
			return 0;

		std::string vnum(lua_tostring(lua, 1));
		do_monarch_mob(ch, vnum.c_str(), 0, 0);
		return 0;
	}

	EVENTFUNC(monarch_transfer2_event)
	{
		monarch_transfer2_event_info* info = dynamic_cast<monarch_transfer2_event_info*>(event->info);

		if ( info == nullptr )
		{
			quest::error("Monarch transfer event has no info");
			return 0;
		}

		character * pTargetChar = character_manager::instance().find_by_pid(info->target_pid);

		if (pTargetChar != nullptr)
		{
			unsigned int qIndex = quest::quest_manager::instance().quest_index_by_name("monarch_transfer");

			if (qIndex != 0)
			{
				pTargetChar->set_quest_flag("monarch_transfer.x", info->x);
				pTargetChar->set_quest_flag("monarch_transfer.y", info->y);
				quest::quest_manager::instance().letter(pTargetChar->player_id(), qIndex, 0);
			}
		}

		return 0;
	}

	auto monarch_transfer2(lua_State* lua) -> int
	{
		if (lua_isstring(lua, 1) == false) return 0;

		character * ch = quest_manager::instance().current_character_ptr();
		if (ch == nullptr) return false;

		if (monarch::instance().is_monarch(ch->player_id(), ch->empire()) == false)
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("군주만이 사용 가능한 기능입니다"));
			return 0;
		}

		if (ch->is_mcok(character::MI_TRANSFER) == false)
		{
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("%d 초간 쿨타임이 적용중입니다."), ch->mcl_time(character::MI_TRANSFER));
			return 0;
		}

		const int ciTransferCost = 10000;

		if (monarch::instance().is_money_ok(ciTransferCost, ch->empire()) == false)
		{
			int NationMoney = monarch::instance().money(ch->empire());
			ch->chat_packet(CHAT_TYPE_INFO, lc_text("국고에 돈이 부족합니다. 현재 : %u 필요금액 : %u"), NationMoney, ciTransferCost);
			return 0;
		}

		std::string strTargetName = lua_tostring(lua, 1);

		character * pTargetChar = character_manager::instance().find_pc(strTargetName.c_str());

		if (pTargetChar == nullptr)
		{
			cci* pCCI = p2_p_manager::instance().find(strTargetName.c_str());

			if (pCCI != nullptr)
			{
				if (pCCI->empire != ch->empire())
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("다른 제국 유저는 소환할 수 없습니다."));
					return 0;
				}

				if (pCCI->channel != metin2::config::server().channel)
				{
					ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s 님은 %d 채널에 접속중입니다. (현재 채널: %d)"),
						   strTargetName.c_str(), pCCI->channel, metin2::config::server().channel);
					return 0;
				}

				if (!is_monarch_warp_zone(pCCI->map_index))
				{
					ch->chat_packet (CHAT_TYPE_INFO, lc_text("해당 지역으로 이동할 수 없습니다."));
					return 0;
				}
				if (!is_monarch_warp_zone(ch->map_index()))
				{
					ch->chat_packet (CHAT_TYPE_INFO, lc_text("해당 지역으로 소환할 수 없습니다."));
					return 0;
				}

				packet_monarch_gg_transfer packet;
				packet.header = HEADER_GG_MONARCH_TRANSFER;
				packet.target_pid = pCCI->pid;
				packet.x = ch->x();
				packet.y = ch->y();

				p2_p_manager::instance().send(&packet, sizeof(packet_monarch_gg_transfer));
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("소환 요청을 보냈습니다"));

				monarch::instance().sendto_db_dec_money(ciTransferCost, ch->empire(), ch);
				ch->set_mc(character::MI_TRANSFER);
			}
			else
			{
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("입력하신 이름을 가진 사용자가 없습니다."));
				return 0;
			}
		}
		else
		{
			if (pTargetChar == ch)
			{
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("자신을 소환할 수 없습니다."));
				return 0;
			}

			if (pTargetChar->empire() != ch->empire())
			{
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("다른 제국 유저는 소환할 수 없습니다."));
				return 0;
			}

			if (distance_approx(pTargetChar->x() - ch->x(), pTargetChar->y() - ch->y()) <= 5000)
			{
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("%s 님은 근처에 있습니다"), pTargetChar->name());
				return 0;
			}

			if (!is_monarch_warp_zone(pTargetChar->map_index()))
			{
				ch->chat_packet (CHAT_TYPE_INFO, lc_text("해당 지역으로 이동할 수 없습니다."));
				return 0;
			}
			if (!is_monarch_warp_zone(ch->map_index()))
			{
				ch->chat_packet (CHAT_TYPE_INFO, lc_text("해당 지역으로 소환할 수 없습니다."));
				return 0;
			}

			monarch_transfer2_event_info* info = alloc_event_info<monarch_transfer2_event_info>();

			info->header = HEADER_GG_MONARCH_TRANSFER;
			info->target_pid = pTargetChar->player_id();
			info->x = ch->x();
			info->y = ch->y();

			event_create(monarch_transfer2_event, info, 1);

			ch->chat_packet(CHAT_TYPE_INFO, lc_text("소환 요청을 보냈습니다"));

			monarch::instance().sendto_db_dec_money(ciTransferCost, ch->empire(), ch);
			ch->set_mc(character::MI_TRANSFER);
			return 0;
		}

		return 0;
	}

	void register_monarch_function_table()
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
			{ "warp",					monarch_warp 		},
			{ "transfer",				monarch_transfer	},
			{ "transfer2",				monarch_transfer2	},
			{ "info",					empire_info 		},	
			{ "notice",					monarch_notice		},
			{ "monarch_mob",			monarch_mob			},

			{ nullptr,						nullptr				}
		};

		quest_manager::instance().add_lua_function_table("oh", Monarch_functions);
	}

}
}
