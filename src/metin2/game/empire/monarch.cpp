#include <metin2/game/empire/monarch.h>

#include <metin2/game/characters/character.h>
#include <metin2/game/app/config.h>
#include <metin2/game/protocol/desc_client.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/world/sectree_manager.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/log/log.h>
#include <metin2/core/main.h>

#include <cstdint>
#include <cstring>
#include <iterator>

namespace metin2::game::inline empire {


monarch::monarch()
{
	memset( &monarch_info_, 0, sizeof(monarch_info_) );

	initialize();
}

monarch::~monarch()
{
}

bool monarch::initialize()
{
	memset(power_up_, 0, sizeof(power_up_));
	memset(defense_up_, 0, sizeof(defense_up_));
	memset(power_up_ct_, 0, sizeof(power_up_ct_));
	memset(defense_up_ct_, 0, sizeof(defense_up_ct_));

	return 0;
}

struct f_heal_my_empire 
{
	uint8_t empire;
	void operator () (entity * ent)
	{
		if (ent->is_type(ENTITY_CHARACTER))
		{
			character * ch = (character *) ent;

			if (ch->is_pc() && empire == ch->empire())
			{
				ch->chat_packet(CHAT_TYPE_INFO, lc_text("군주가 내린 축복으로 모든 에너지가 가득 채워집니다"));
				ch->point_change(POINT_HP, ch->max_hp() - ch->hp());
				ch->point_change(POINT_SP, ch->max_sp() - ch->sp());
				ch->effect_packet(SE_SPUP_BLUE);
				ch->effect_packet(SE_HPUP_RED);
			}
		}
	}
};

int monarch::heal_my_empire(character * ch ,uint32_t price)
{
	uint8_t Empire = ch->empire();
	uint32_t pid = ch->player_id();

	metin2::log::info( "HealMyEmpire[{:d}] pid:{:d} price {:d}", pid, Empire, price);

	if (is_monarch(pid, Empire) == 0)
	{
		if (!ch->is_gm())
		{
			ch->chat_packet(CHAT_TYPE_INFO ,lc_text("군주의 자격을 가지고 있지 않습니다"));
			metin2::log::error("No Monarch pid {:d} ", pid);
			return 0;
		}
	}

	if (!ch->is_mcok(character::MI_HEAL))
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("%d 초 후에 군주의 축복을 사용할 수 있습니다"), ch->mcl_time(character::MI_HEAL));

		return 0;
	}

	if (!is_money_ok(price, Empire)) 
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("국고에 돈이 부족합니다. 현재 : %u 필요금액 : %u"), money(Empire), price);
		return 0;
	}

	int iMapIndex = ch->map_index();

	f_heal_my_empire f;
	f.empire = Empire;
	sectree_manager::instance().for_each(iMapIndex, f);	

	// Send money deduction to DB 
	sendto_db_dec_money(price, Empire, ch);

	// Set cooldown	
	ch->set_mc(character::MI_HEAL);

	if (metin2::config::test_server())
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("[TEST_ONLY]현재 국고 : %d "), money(Empire) - price);
	return 1;
}

void monarch::set_monarch_info(monarch_info * pInfo)
{
	memcpy(&monarch_info_, pInfo, sizeof(monarch_info));
}

bool monarch::is_monarch(uint32_t pid, uint8_t bEmpire)
{
	if (bEmpire >= std::size(monarch_info_.pid))
		return false;

	return (monarch_info_.pid[bEmpire] == pid);
}

bool monarch::is_money_ok(int price, uint8_t bEmpire)
{
	metin2::log::debug( "GetMoney({:d}), price = ({:d},{:d})", bEmpire, money(bEmpire), price);
	return (money(bEmpire) >= price);
}

bool monarch::sendto_db_add_money(int Money, uint8_t bEmpire, character * ch)
{
	if (money(bEmpire) + Money > 2000000000)
		return false;

	if (money(bEmpire) + Money < 0)
		return false;

	int nEmpire = bEmpire;	
	db_clientdesc->db_packet_header(HEADER_GD_ADD_MONARCH_MONEY, ch->get_desc()->handle(), sizeof(int) + sizeof(int));
	db_clientdesc->packet(&nEmpire, sizeof(int));	
	db_clientdesc->packet(&Money, sizeof(int));	
	return true;
}

bool monarch::sendto_db_dec_money(int Money, uint8_t bEmpire, character * ch)
{
	if (bEmpire >= std::size(monarch_info_.money))
		return false;

	if (money(bEmpire) - Money < 0)
		return false;

	// The actual deduction is processed after the round trip to the server
	int nEmpire = bEmpire;	

	db_clientdesc->db_packet_header(HEADER_GD_DEC_MONARCH_MONEY, ch->get_desc()->handle(), sizeof(int) + sizeof(int));
	db_clientdesc->packet(&nEmpire, sizeof(int));	
	db_clientdesc->packet(&Money, sizeof(int));	
	return true;
}

bool monarch::add_money(int Money, uint8_t bEmpire)
{
	if (bEmpire >= std::size(monarch_info_.money))
		return false;

	// Cannot deposit 2 billion or more
	if (money(bEmpire) + Money > 2000000000)
		return false;

	monarch_info_.money[bEmpire] += Money;
	return true;
}

bool monarch::dec_money(int Money, uint8_t bEmpire)
{
	if (bEmpire >= std::size(monarch_info_.money))
		return false;

	if (money(bEmpire) - Money < 0)
		return false;

	monarch_info_.money[bEmpire] -= Money;
	return true;
}

int monarch::money(uint8_t bEmpire)
{
	if (bEmpire >= std::size(monarch_info_.money))
		return 0;

	return monarch_info_.money[bEmpire];
}

monarch_info* monarch::get_monarch()
{
	return &monarch_info_;
}

uint32_t monarch::monarch_pid(uint8_t Empire)
{
	return Empire < std::size(monarch_info_.pid) ? monarch_info_.pid[Empire] : 0;
}

bool monarch::is_power_up(uint8_t Empire)
{
	return Empire < std::size(power_up_) ? power_up_[Empire] : false;
}

bool monarch::is_defence_up(uint8_t Empire)
{
	return Empire < std::size(defense_up_) ? defense_up_[Empire] : false;
}

bool monarch::check_power_up_ct(uint8_t Empire)
{
	if (Empire >= std::size(power_up_ct_))
		return false;

	if (power_up_ct_[Empire] > thecore_pulse())
	{
		if (metin2::config::test_server())
			metin2::log::info( "[TEST_ONLY] : CheckPowerUpCT CT{:d} Now{:d} 60sec {:d}", power_up_ct_[Empire], thecore_pulse(), passes_per_sec(60 * 10));
		return false;
	}

	return true;
}

bool monarch::check_defense_up_ct(uint8_t Empire)
{
	if (Empire >= std::size(defense_up_ct_))
		return false;

	if (defense_up_ct_[Empire] > thecore_pulse())
	{
		if (metin2::config::test_server())
			metin2::log::info( "[TEST_ONLY] : CheckPowerUpCT CT{:d} Now{:d} 60sec {:d}", power_up_ct_[Empire], thecore_pulse(), passes_per_sec(60 * 10));
		return false;
	}

	return true;
}

void monarch::power_up(uint8_t Empire, bool On)
{
	if (Empire >= std::size(power_up_ct_))
		return;

	power_up_[Empire] = On;

	// Monarch Lion's Roar cooldown 
	power_up_ct_[Empire] = thecore_pulse() + passes_per_sec(60 * 10);
}

void monarch::defense_up(uint8_t Empire, bool On)
{
	if (Empire >= std::size(defense_up_ct_))
		return;

	defense_up_[Empire] = On;

	// Monarch Diamond Fist cooldown 
	defense_up_ct_[Empire] = thecore_pulse() + passes_per_sec(60 * 10);
}

bool is_monarch_warp_zone (int map_idx)
{
	// Hungry Ghost Cave, Cave of a Thousand.
	if (map_idx >= 10000)
		map_idx /= 10000;

	switch (map_idx)
	{
	case 301:
	case 302:
	case 303:
	case 304:
		//Dungeon
	case 351: // Red Dragon Fortress
	case 352: // White Dragon Fortress
		return false;
	}

	return (map_idx != 208 && map_idx != 216 && map_idx != 217);
}
}
