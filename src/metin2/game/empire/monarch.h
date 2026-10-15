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
		void set_monarch_info(monarch_info * info);

		bool is_monarch(uint32_t pid, uint8_t empire);
		bool is_money_ok(int price, uint8_t empire);
		bool sendto_db_add_money(int money_value, uint8_t empire, character * ch);
		bool sendto_db_dec_money(int money_value, uint8_t empire, character * ch);

		bool add_money(int money_value, uint8_t empire);
		bool dec_money(int money_value, uint8_t empire);
		int money(uint8_t empire);

		monarch_info* get_monarch();

		uint32_t monarch_pid(uint8_t empire);

		bool is_power_up(uint8_t empire);
		bool is_defence_up(uint8_t empire);

		int	power_up_ct(uint8_t empire)
		{
			return empire < std::size(power_up_ct_) ? power_up_ct_[empire] : 0;
		}

		bool check_power_up_ct(uint8_t empire);
		bool check_defense_up_ct(uint8_t empire);

		int	defense_up_ct(uint8_t empire)
		{
			return empire < std::size(defense_up_ct_) ? defense_up_ct_[empire] : 0;
		}

		void power_up(uint8_t empire, bool on);
		void defense_up(uint8_t empire, bool on);

	private:
		monarch_info monarch_info_;	

		int power_up_[4];
		int defense_up_[4];

		int power_up_ct_[4];
		int defense_up_ct_[4];
};

bool is_monarch_warp_zone (int map_idx);
}
