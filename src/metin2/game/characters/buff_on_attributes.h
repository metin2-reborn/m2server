#pragma once

#include <metin2/game/app/typedef.h>

#include <cstdint>
#include <map>
#include <vector>

namespace metin2::game::inline characters {

class character;

class buff_on_attributes
{
public:
	buff_on_attributes(character * owner, uint8_t point_type, std::vector <uint8_t>* p_vec_buff_wear_targets);
	~buff_on_attributes();

	// If equipped and in m_p_vec_buff_wear_targets, remove the effect applied by that item.
	void remove_buff_from_item(item * item_value);
	// If in m_p_vec_buff_wear_targets, add the effect for that item's attributes.
	void add_buff_from_item(item * item_value);
	// Change m_bBuffValue and also the buff value.
	void change_buff_value(uint8_t new_value);
	// CHRACTRE::ComputePoints resets and recalculates attribute values, so 
	// forcibly give the buff attribute values to the owner.
	void give_all_attributes();

	// Sum the attributes of all items in m_p_vec_buff_wear_targets by type,
	// and give (m_bBuffValue)% of those attributes as a buff.
	bool on(uint8_t value);
	// Remove the buff, then reset
	void off();

	void initialize();
private:
	character * buff_owner_;
	uint8_t point_type_;
	uint8_t buff_value_;
	std::vector <uint8_t>* p_vec_buff_wear_targets_;
	
	// Map of apply_type, apply_value pairs
	typedef std::map <uint8_t, int> map_attr;
	// Holds the attributes of all items in m_p_vec_buff_wear_targets summed by type.
	map_attr map_additional_attrs_;

};
}
