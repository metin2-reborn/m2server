#pragma once

#include "typedef.h"

#include <cstdint>
#include <map>
#include <vector>

namespace metin2::game {

class CHARACTER;

class CBuffOnAttributes
{
public:
	CBuffOnAttributes(CHARACTER * pOwner, uint8_t m_point_type, std::vector <uint8_t>* vec_buff_targets);
	~CBuffOnAttributes();

	// If equipped and in m_p_vec_buff_wear_targets, remove the effect applied by that item.
	void RemoveBuffFromItem(CItem * pItem);
	// If in m_p_vec_buff_wear_targets, add the effect for that item's attributes.
	void AddBuffFromItem(CItem * pItem);
	// Change m_bBuffValue and also the buff value.
	void ChangeBuffValue(uint8_t bNewValue);
	// CHRACTRE::ComputePoints resets and recalculates attribute values, so 
	// forcibly give the buff attribute values to the owner.
	void GiveAllAttributes();

	// Sum the attributes of all items in m_p_vec_buff_wear_targets by type,
	// and give (m_bBuffValue)% of those attributes as a buff.
	bool On(uint8_t bValue);
	// Remove the buff, then reset
	void Off();

	void Initialize();
private:
	CHARACTER * m_pBuffOwner;
	uint8_t m_bPointType;
	uint8_t m_bBuffValue;
	std::vector <uint8_t>* m_p_vec_buff_wear_targets;
	
	// Map of apply_type, apply_value pairs
	typedef std::map <uint8_t, int> TMapAttr;
	// Holds the attributes of all items in m_p_vec_buff_wear_targets summed by type.
	TMapAttr m_map_additional_attrs;

};
}
