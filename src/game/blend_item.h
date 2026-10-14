/*********************************************************************
 * date        : 2007.02.24
 * file        : blend_item.h
 * author      : mhh
 * description : 
 */

#ifndef _blend_item_h_
#define _blend_item_h_

#include "typedef.h"

#include <cstdint>

inline constexpr int MAX_BLEND_ITEM_VALUE = 5;
bool	Blend_Item_init();
bool	Blend_Item_load(char *file);
bool	Blend_Item_set_value(CItem * item);
bool	Blend_Item_find(uint32_t item_vnum);

#endif	/* _blend_item_h_ */

