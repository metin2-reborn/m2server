#pragma once

#include <metin2/game/app/typedef.h>

#include <metin2/common/singleton.h>
#include <metin2/proto/tables.h>

#include <cstdint>
#include <iterator>

namespace metin2::game::inline empire {

class monarch : public singleton<monarch>
{
	public:
		monarch();
		~monarch() override;

		bool initialize();

		int heal_my_empire(character * ch, uint32_t price);
		void set_monarch_info(monarch_info * pInfo);

		bool is_monarch(uint32_t pid, uint8_t bEmpire);
		bool is_money_ok(int price, uint8_t bEmpire);
		bool sendto_db_add_money(int Money, uint8_t bEmpire, character * ch);
		bool sendto_db_dec_money(int Money, uint8_t bEmpire, character * ch);

		bool add_money(int Money, uint8_t bEmpire);
		bool dec_money(int Money, uint8_t bEmpire);
		int money(uint8_t bEmpire);

		monarch_info* get_monarch();

		uint32_t monarch_pid(uint8_t Empire);

		bool is_power_up(uint8_t Empire);
		bool is_defence_up(uint8_t Empire);

		int	power_up_ct(uint8_t Empire)
		{
			return Empire < std::size(power_up_ct_) ? power_up_ct_[Empire] : 0;
		}

		bool check_power_up_ct(uint8_t Empire);
		bool check_defense_up_ct(uint8_t Empire);

		int	defense_up_ct(uint8_t Empire)
		{
			return Empire < std::size(defense_up_ct_) ? defense_up_ct_[Empire] : 0;
		}

		void power_up(uint8_t Empire, bool On);
		void defense_up(uint8_t Empire, bool On);

	private:
		monarch_info monarch_info_;	

		int power_up_[4]; ///< Monarch's Lion's Roar
		int defense_up_[4]; ///< Monarch's Diamond Fist

		int power_up_ct_[4]; ///< Monarch's Lion's Roar
		int defense_up_ct_[4]; ///< Monarch's Diamond Fist
};

bool is_monarch_warp_zone (int map_idx);
}
