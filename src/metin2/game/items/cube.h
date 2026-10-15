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
struct CUBE_VALUE
{
	uint32_t	vnum;
	int		count;

	bool operator == (const CUBE_VALUE& b) const
	{
		return (this->count == b.count) && (this->vnum == b.vnum);
	}
};

struct CUBE_DATA
{
	std::vector<uint16_t>		npc_vnum;
	std::vector<CUBE_VALUE>	item;
	std::vector<CUBE_VALUE>	reward;
	int						percent;
	unsigned int			gold;		// Cost required for crafting

	CUBE_DATA();

	bool		can_make_item (CItem * *items, uint16_t npc_vnum);
	CUBE_VALUE*	reward_value ();
	void		remove_material (CHARACTER * ch);
}; 

void Cube_init ();
bool Cube_load (const char *file);

bool Cube_make (CHARACTER * ch);

void Cube_clean_item (CHARACTER * ch);
void Cube_open (CHARACTER * ch);
void Cube_close (CHARACTER * ch);

void Cube_show_list (CHARACTER * ch);
void Cube_add_item (CHARACTER * ch, int cube_index, int inven_index);
void Cube_delete_item (CHARACTER * ch, int cube_index);

void Cube_request_result_list(CHARACTER * ch);
void Cube_request_material_info(CHARACTER * ch, int request_start_index, int request_count = 1);

// test print code

bool Cube_InformationInitialize();
}
