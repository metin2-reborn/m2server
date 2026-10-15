#pragma once

#include <metin2/common/singleton.h>
#include <metin2/proto/tables.h>

#include <cstdint>
#include <list>

namespace metin2::db::inline items {

class item_id_range_manager : public singleton<item_id_range_manager>
{
	private :
		const static uint32_t max_item_id_ = 4290000000UL;
		const static uint32_t minimum_range_ = 10000000UL;
		static constexpr uint32_t minimum_remain_count_ = 10000UL;

		std::list<item_id_range_table> data_;

	public :
		item_id_range_manager();

		void build();
		auto build_range(uint32_t min_value, uint32_t max_value, item_id_range_table& range) -> bool;
		void update_range(uint32_t min, uint32_t max);

		auto range() -> item_id_range_table;
};
}
