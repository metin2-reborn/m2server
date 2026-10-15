/*********************************************************************
 * date        : 2006.11.20
 * file        : cube.h
 * author      : mhh
 * description : Cube system
 */

#pragma once

#include <metin2/game/app/typedef.h>

#include <cstdint>
#include <vector>

namespace metin2::game::inline items {

inline constexpr int CUBE_MAX_NUM = 24; // OLD:INVENTORY_MAX_NUM
inline constexpr int CUBE_MAX_DISTANCE = 1000;
struct cube_value
{
	uint32_t	vnum;
	int		count;

	bool operator == (const cube_value& b) const
	{
		return (this->count == b.count) && (this->vnum == b.vnum);
	}
};

struct cube_data
{
	std::vector<uint16_t>		npc_vnum;
	std::vector<cube_value>	item;
	std::vector<cube_value>	reward;
	int						percent;
	unsigned int			gold;		// Cost required for crafting

	cube_data();

	bool		can_make_item (game::items::item * *items, uint16_t npc_vnum);
	cube_value*	reward_value ();
	void		remove_material (character * ch);
}; 

void cube_init ();
bool cube_load (const char *file);

bool cube_make (character * ch);

void cube_clean_item (character * ch);
void cube_open (character * ch);
void cube_close (character * ch);

void cube_show_list (character * ch);
void cube_add_item (character * ch, int cube_index, int inven_index);
void cube_delete_item (character * ch, int cube_index);

void cube_request_result_list(character * ch);
void cube_request_material_info(character * ch, int request_start_index, int request_count = 1);

// test print code

bool cube_information_initialize();
}
