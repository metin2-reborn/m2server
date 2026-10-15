#include <metin2/db/items/item_id_range_manager.h>

#include <metin2/db/clients/client_manager.h>
#include <metin2/db/storage/db_manager.h>
#include <metin2/db/app/main.h>
#include <metin2/db/clients/peer.h>

#include <metin2/proto/tables.h>
#include <metin2/common/utils.h>
#include <metin2/sql/async_sql.h>
#include <metin2/log/log.h>

#include <mysql.h>

#include <cstdint>
#include <cstdio>
#include <memory>

namespace metin2::db::inline items {

item_id_range_manager::item_id_range_manager()
{
	data_.clear();
}

void item_id_range_manager::build()
{
	uint32_t dwMin = 0;
	uint32_t dwMax = 0;
	item_id_range_table range;

	for (int i = 0; ; ++i)
	{
		dwMin = cs_dw_minimum_range_ * (i + 1) + 1;
		dwMax = cs_dw_minimum_range_ * (i + 2);

		if (dwMax == cs_dw_max_item_id_)
			break;

		if (client_manager::instance().item_range().min <= dwMin &&
			client_manager::instance().item_range().max >= dwMax)
		{
			continue;
		}

		if (build_range(dwMin, dwMax, range) == true)
		{
			data_.push_back(range);
		}
	}
}

struct f_check_collision
{
	bool has_collision;
	item_id_range_table range;

	f_check_collision(item_id_range_table data)
	{
		has_collision = false;
		range = data;
	}

	void operator() (peer* peer)
	{
		if (has_collision == false)
		{
			has_collision = peer->check_item_id_range_collision(range);
		}
	}
};

item_id_range_table item_id_range_manager::range()
{
	item_id_range_table ret;
	ret.min = 0;
	ret.max = 0;
	ret.usable_item_id_min = 0;

	if (data_.size() > 0)
	{
		while (data_.size() > 0)
		{
			ret = data_.front();
			data_.pop_front();

			f_check_collision f(ret);
			client_manager::instance().for_each_peer(f);

			if (f.has_collision == false) return ret;
		}
	}

	for (int i = 0; i < 10; ++i)
		metin2::log::error("ItemIDRange: NO MORE ITEM ID RANGE");

	return ret;
}

bool item_id_range_manager::build_range(uint32_t dwMin, uint32_t dwMax, item_id_range_table& range)
{
	char szQuery[1024];
	uint32_t dwItemMaxID = 0;
	std::unique_ptr<ql_msg> pMsg;
	MYSQL_ROW row;

	snprintf(szQuery, sizeof(szQuery), "SELECT MAX(id) FROM item%s WHERE id >= %u and id <= %u", get_table_postfix(), dwMin, dwMax);

	pMsg.reset(db_manager::instance().direct_query(szQuery));

	if (pMsg != nullptr)
	{
		if (pMsg->get()->num_rows > 0)
		{
			row = mysql_fetch_row(pMsg->get()->sql_result);
			str_to_number(dwItemMaxID, row[0]);
		}
	}

	if (dwItemMaxID == 0)
		dwItemMaxID = dwMin;
	else
		dwItemMaxID++;
	
	if ((dwMax < dwItemMaxID) || (dwMax - dwItemMaxID < cs_dw_minimum_remain_count_))
	{
		metin2::log::info( "ItemIDRange: Build {:d} ~ {:d} start: {:d}\tNOT USE remain count is below {:d}",
			   	dwMin, dwMax, dwItemMaxID, cs_dw_minimum_remain_count_);
	}
	else
	{
		range.min = dwMin;
		range.max = dwMax;
		range.usable_item_id_min = dwItemMaxID;

		snprintf(szQuery, sizeof(szQuery), "SELECT COUNT(*) FROM item%s WHERE id >= %u AND id <= %u", 
				get_table_postfix(), range.usable_item_id_min, range.max);

		pMsg.reset(db_manager::instance().direct_query(szQuery));

		if (pMsg != nullptr)
		{
			if (pMsg->get()->num_rows > 0)
			{
				uint32_t count = 0;
				row = mysql_fetch_row(pMsg->get()->sql_result);
				str_to_number(count, row[0]);

				if (count > 0)
				{
					metin2::log::error("ItemIDRange: Build: {:d} ~ {:d}\thave a item", range.usable_item_id_min, range.max);
					return false;
				}
				else
				{
					metin2::log::info( "ItemIDRange: Build: {:d} ~ {:d} start:{:d}", range.min, range.max, range.usable_item_id_min);
					return true;
				}
			}
		}
	}

	return false;
}

void item_id_range_manager::update_range(uint32_t dwMin, uint32_t dwMax)
{
	item_id_range_table range;

	if (build_range(dwMin, dwMax, range) == true)
	{
		data_.push_back(range);
	}
}
}
