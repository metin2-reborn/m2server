#include <metin2/game/characters/buff_on_attributes.h>

#include <metin2/game/characters/char.h>
#include <metin2/game/items/item.h>
#include <metin2/game/app/typedef.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/log/log.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace metin2::game::inline characters {

buff_on_attributes::buff_on_attributes(character * pOwner, uint8_t point_type, std::vector <uint8_t>* p_vec_buff_wear_targets)
:	buff_owner_(pOwner), point_type_(point_type), p_vec_buff_wear_targets_(p_vec_buff_wear_targets)
{
	initialize();
}

buff_on_attributes::~buff_on_attributes()
{
	off();
}

void buff_on_attributes::initialize()
{
	buff_value_ = 0;
	map_additional_attrs_.clear();
}

void buff_on_attributes::remove_buff_from_item(item * pItem)
{
	if (0 == buff_value_)
		return ;
	if (nullptr != pItem)
	{
		if (pItem->cell() < INVENTORY_MAX_NUM)
			return;
		std::vector <uint8_t>::iterator it = find (p_vec_buff_wear_targets_->begin(), p_vec_buff_wear_targets_->end(), pItem->cell() - INVENTORY_MAX_NUM);
		if (p_vec_buff_wear_targets_->end() == it)
			return;

		int m = pItem->attribute_count();
		for (int j = 0; j < m; j++)
		{
			player_item_attribute attr = pItem->get_attribute(j);
			map_attr::iterator it = map_additional_attrs_.find(attr.type);
			// Remove the value for this attribute type from m_map_additional_attrs,
			// and reduce the buff effect by (m_bBuffValue)% of the changed value
			if (it != map_additional_attrs_.end())
			{
				int& sum_of_attr_value = it->second;
				int old_value = sum_of_attr_value * buff_value_ / 100;
				int new_value = (sum_of_attr_value - attr.s_value) * buff_value_ / 100;
				buff_owner_->apply_point(attr.type, new_value - old_value);
				sum_of_attr_value -= attr.s_value;
			}
			else
			{
				metin2::log::error("Buff ERROR(type {:d}). This item({:d}) attr_type({:d}) was not in buff pool", point_type_, pItem->vnum(), attr.type);
				return;
			}
		}
	}
}

void buff_on_attributes::add_buff_from_item(item * pItem)
{
	if (0 == buff_value_)
		return ;
	if (nullptr != pItem)
	{
		if (pItem->cell() < INVENTORY_MAX_NUM)
			return;
		std::vector <uint8_t>::iterator it = find (p_vec_buff_wear_targets_->begin(), p_vec_buff_wear_targets_->end(), pItem->cell() - INVENTORY_MAX_NUM);
		if (p_vec_buff_wear_targets_->end() == it)
			return;

		int m = pItem->attribute_count();
		for (int j = 0; j < m; j++)
		{
			player_item_attribute attr = pItem->get_attribute(j);
			map_attr::iterator it = map_additional_attrs_.find(attr.type);
			
			// If m_map_additional_attrs has no value for this attribute type, add it.
			// Add a buff effect of (m_bBuffValue)% of the added value
			if (it == map_additional_attrs_.end())
			{
				buff_owner_->apply_point(attr.type, attr.s_value * buff_value_ / 100);
				map_additional_attrs_.insert(map_attr::value_type(attr.type, attr.s_value));
			}
			// If m_map_additional_attrs has a value for this attribute type, increase it,
			// and add a buff effect of (m_bBuffValue)% of the changed value
			else
			{
				int& sum_of_attr_value = it->second;
				int old_value = sum_of_attr_value * buff_value_ / 100;
				int new_value = (sum_of_attr_value + attr.s_value) * buff_value_ / 100;
				buff_owner_->apply_point(attr.type, new_value - old_value);
				sum_of_attr_value += attr.s_value;
			}
		}
	}
}

void buff_on_attributes::change_buff_value(uint8_t bNewValue)
{
	if (0 == buff_value_)
		on(bNewValue);
	else if (0 == bNewValue)
		off();
	else
	{
		// Previously (m_bBuffValue)% of the m_map_additional_attrs values was applied as the buff,
		// so change it to (bNewValue)%.
		for (map_attr::iterator it = map_additional_attrs_.begin(); it != map_additional_attrs_.end(); it++)
		{
			int& sum_of_attr_value = it->second;
			
			buff_owner_->apply_point(it->first, -sum_of_attr_value * buff_value_ / 100);
		}
		buff_value_ = bNewValue;
	}
}

void buff_on_attributes::give_all_attributes()
{
	if (0 == buff_value_)
		return;
	for (map_attr::iterator it = map_additional_attrs_.begin(); it != map_additional_attrs_.end(); it++)
	{
		uint8_t apply_type = it->first;
		int apply_value = it->second * buff_value_ / 100;
		
		buff_owner_->apply_point(apply_type, apply_value);
	}
}

bool buff_on_attributes::on(uint8_t bValue)
{
	if (0 != buff_value_ || 0 == bValue)
		return false;
	
	int n = p_vec_buff_wear_targets_->size();
	map_additional_attrs_.clear();
	for (int i = 0; i < n; i++)
	{
		item * pItem = buff_owner_->wear(p_vec_buff_wear_targets_->at(i));
		if (nullptr != pItem)
		{
			int m = pItem->attribute_count();
			for (int j = 0; j < m; j++)
			{
				player_item_attribute attr = pItem->get_attribute(j);
				map_attr::iterator it = map_additional_attrs_.find(attr.type);
				if (it != map_additional_attrs_.end())
				{
					it->second += attr.s_value;
				}
				else
				{
					map_additional_attrs_.insert(map_attr::value_type(attr.type, attr.s_value));
				}
			}
		}
	}

	for (map_attr::iterator it = map_additional_attrs_.begin(); it != map_additional_attrs_.end(); it++)
	{
		buff_owner_->apply_point(it->first, it->second * bValue / 100);
	}
	
	buff_value_ = bValue;

	return true;
}

void buff_on_attributes::off()
{
	if (0 == buff_value_)
		return ;

	for (map_attr::iterator it = map_additional_attrs_.begin(); it != map_additional_attrs_.end(); it++)
	{
		buff_owner_->apply_point(it->first, -it->second * buff_value_ / 100);
	}
	initialize();
}
}
