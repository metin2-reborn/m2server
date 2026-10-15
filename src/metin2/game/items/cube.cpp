/*********************************************************************
 * date        : 2006.11.20
 * file        : cube.cpp
 * author      : mhh
 * description : 
 */


#include <metin2/game/items/cube.h>

#include <metin2/game/characters/character.h>
#include <metin2/game/app/config.h>
#include <metin2/game/items/item.h>
#include <metin2/game/items/item_manager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>

#include <metin2/proto/length.h>
#include <metin2/proto/tables.h>
#include <metin2/common/utils.h>
#include <metin2/log/log.h>
#include <metin2/core/utils.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>



#define RETURN_IF_CUBE_IS_NOT_OPENED(ch) if (!(ch)->is_cube_open()) return


/*--------------------------------------------------------*/
/*                   GLOBAL VARIABLES                     */
/*--------------------------------------------------------*/

namespace metin2::game::inline items {
static std::vector<std::unique_ptr<cube_data>>	cube_proto;
static bool is_initialized_cube_material_information = false;



/*--------------------------------------------------------*/
/*               Cube Material Information                */ 
/*--------------------------------------------------------*/
enum cube_result_category
{
	CUBE_CATEGORY_POTION,				// Herbs, extracts, etc.. (can't be classified as potions, so unused. Herbs and such are all just misc)
	CUBE_CATEGORY_WEAPON,				// Weapon
	CUBE_CATEGORY_ARMOR,				// Armor
};

typedef std::vector<cube_value>	cube_value_vector;

struct cube_material_info
{
	cube_material_info()
	{
		have_complicate_material = false;
	};

	cube_value			reward;							// What's the reward
	cube_value_vector	material;						// What are the materials
	uint32_t				gold;							// How much gold does it cost
	cube_value_vector	complicate_material;				// Complicated -_- materials

	// .. format agreed upon for showing materials on the client
	// 72723,1&72724,2&72730,1
	// 52001,1|52002,1|52003,1&72723,1&72724,5
	//	=> ( 52001,1 or 52002,1 or 52003,1 ) and 72723,1 and 72724,5
	std::string			info_text;		
	bool				have_complicate_material;		//
};

struct item_name_and_level
{
	item_name_and_level() { level = 0; }

	std::string		name;
	int				level;
};


// Please excuse the crappy data structures... made this in a soulless state thanks to someone
typedef std::vector<cube_material_info>								cube_result_list;
typedef std::unordered_map<uint32_t, cube_result_list>				cube_map_by_npc;				// What each NPC can make and what the materials are...
typedef std::unordered_map<uint32_t, std::string>					cube_result_info_text_by_npc;	// Info listing what each NPC can make, organized in the agreed format

cube_map_by_npc cube_info_map;
cube_result_info_text_by_npc cube_result_info_map_by_npc;				// The naming is f***ing stupid lol


/*--------------------------------------------------------*/
/*                  STATIC FUNCTIONS                      */ 
/*--------------------------------------------------------*/
 // Does it have the required item count?
static bool fn_check_item_count (item * *items, uint32_t item_vnum, int need_count)
{
	int	count = 0;

	// for all cube
	for (int i=0; i<CUBE_MAX_NUM; ++i)
	{
		if (nullptr==items[i])	continue;

		if (item_vnum==items[i]->vnum())
		{
			count += items[i]->count();
		}
	}

	return (count>=need_count);
}

// Removes the materials in the cube.
static void fn_remove_material (item * *items, uint32_t item_vnum, int need_count)
{
	int		count	= 0;
	item *	item	= nullptr;

	// for all cube
	for (int i=0; i<CUBE_MAX_NUM; ++i)
	{
		if (nullptr==items[i])	continue;

		item = items[i];
		if (item_vnum==item->vnum())
		{
			count += item->count();

			if (count>need_count)
			{
				item->set_count(count-need_count);
				return;
			}
			else
			{
				item->set_count(0);
				items[i] = nullptr;
			}
		}
	}
}


static cube_data* fn_find_cube (item * *items, uint16_t npc_vnum)
{
	uint32_t	i, end_index;

	if (0==npc_vnum)	return nullptr;

	// FOR ALL CUBE_PROTO
	end_index = cube_proto.size();
	for (i=0; i<end_index; ++i)
	{
		if ( cube_proto[i]->can_make_item(items, npc_vnum) )
			return cube_proto[i].get();
	}

	return nullptr;
}

static bool fn_check_valid_npc( uint16_t vnum )
{
	for ( auto iter = cube_proto.begin(); iter != cube_proto.end(); iter++ )
	{
		if ( std::find((*iter)->npc_vnum.begin(), (*iter)->npc_vnum.end(), vnum) != (*iter)->npc_vnum.end() )
			return true;
	}

	return false;
}

// Checks whether the cube data was initialized correctly.
static bool fn_check_cube_data (cube_data *cube_data)
{
	uint32_t	i = 0;
	uint32_t	end_index = 0;

	end_index = cube_data->npc_vnum.size();
	for (i=0; i<end_index; ++i)
	{
		if ( cube_data->npc_vnum[i] == 0 )	return false;
	}

	end_index = cube_data->item.size();
	for (i=0; i<end_index; ++i)
	{
		if ( cube_data->item[i].vnum == 0 )		return false;
		if ( cube_data->item[i].count == 0 )	return false;
	}

	end_index = cube_data->reward.size();
	for (i=0; i<end_index; ++i)
	{
		if ( cube_data->reward[i].vnum == 0 )	return false;
		if ( cube_data->reward[i].count == 0 )	return false;
	}
	return true;
}

cube_data::cube_data()
{
	this->percent = 0;
	this->gold = 0;
}

// Checks whether the required material quantities are satisfied.
bool cube_data::can_make_item (game::items::item * *items, uint16_t npc_vnum)
{
	// Checks whether the required materials and quantities are satisfied.
	uint32_t	i, end_index;
	uint32_t	need_vnum;
	int		need_count;
	int		found_npc = false;

	// check npc_vnum
	end_index = this->npc_vnum.size();
	for (i=0; i<end_index; ++i)
	{
		if (npc_vnum == this->npc_vnum[i])
			found_npc = true;
	}
	if (false==found_npc)	return false;

	end_index = this->item.size();
	for (i=0; i<end_index; ++i)
	{
		need_vnum	= this->item[i].vnum;
		need_count	= this->item[i].count;

		if ( false==fn_check_item_count(items, need_vnum, need_count) )
			return false;
	}

	return true;
}

// Determines the kind of item produced when the cube is run
cube_value* cube_data::reward_value ()
{
	int		end_index		= 0;
	uint32_t	reward_index	= 0;

	end_index = this->reward.size();
	reward_index = number(0, end_index);
	reward_index = number(0, end_index-1);

	return &this->reward[reward_index];
}

// Removes the materials in the cube
void cube_data::remove_material (character * ch)
{
	uint32_t	i, end_index;
	uint32_t	need_vnum;
	int		need_count;
	game::items::item *	*items = ch->cube_item();

	end_index = this->item.size();
	for (i=0; i<end_index; ++i)
	{
		need_vnum	= this->item[i].vnum;
		need_count	= this->item[i].count;

		fn_remove_material (items, need_vnum, need_count);
	}
}

void cube_clean_item (character * ch)
{
	item *	*cube_item;

	cube_item = ch->cube_item();

	for (int i=0; i<CUBE_MAX_NUM; ++i)
	{
		if (nullptr == cube_item[i])
			continue;

		cube_item[i] = nullptr;
	}
}

// Open cube window
void cube_open (character * ch)
{
	if (false == is_initialized_cube_material_information)
	{
		cube_information_initialize();
	}

	if (nullptr == ch)
		return;

	character *	npc;
	npc = ch->quest_npc();
	if (nullptr==npc)
	{
		if (metin2::config::test_server())
			metin2::log::debug( "cube_npc is NULL");
		return;
	}

	if ( fn_check_valid_npc(npc->race_num()) == false )
	{
		if ( metin2::config::test_server() == true )
		{
			metin2::log::debug( "cube not valid NPC");
		}
		return;
	}

	if (ch->is_cube_open())
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("이미 제조창이 열려있습니다."));
		return;
	}
	if ( ch->get_exchange() || ch->my_shop() || ch->shop_owner() || ch->is_open_safebox() || ch->is_cube_open() )
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("다른 거래중(창고,교환,상점)에는 사용할 수 없습니다."));
		return;
	}

	int32_t distance = distance_approx(ch->x() - npc->x(), ch->y() - npc->y());

	if (distance >= CUBE_MAX_DISTANCE)
	{
		metin2::log::debug( "CUBE: TOO_FAR: {} distance {:d}", ch->name(), distance);
		return;
	}


	cube_clean_item(ch);
	ch->set_cube_npc(npc);
	ch->chat_packet(CHAT_TYPE_COMMAND, "cube open %d", npc->race_num());
}

// Cube cancel
void cube_close (character * ch)
{
	RETURN_IF_CUBE_IS_NOT_OPENED(ch);
	cube_clean_item(ch);
	ch->set_cube_npc(nullptr);
	ch->chat_packet(CHAT_TYPE_COMMAND, "cube close");
	metin2::log::debug( "<CUBE> close ({})", ch->name());
}

void cube_init()
{
	char file_name[256+1];
	snprintf(file_name, sizeof(file_name), "%s/cube.txt", locale_service_get_base_path().c_str());

	metin2::log::info( "Cube_Init {}", file_name);

	cube_proto.clear();

	if (false == cube_load(file_name))
		metin2::log::error("Cube_Init failed");
}

bool cube_load (const char *file)
{
	FILE	*fp;
	char	one_line[256];
	int		value1, value2;
	const char	*delim = " \t\r\n";
	char	*v, *token_string;
	std::unique_ptr<cube_data>	cube_data;
	cube_value	cube_value = {0,0};

	if (nullptr == file || 0 == file[0])
		return false;

	if ((fp = fopen(file, "r")) == nullptr)
		return false;

	while (fgets(one_line, 256, fp))
	{
		value1 = value2 = 0;

		if (one_line[0] == '#')
			continue;

		token_string = strtok(one_line, delim);

		if (nullptr == token_string)
			continue;

		// set value1, value2
		if ((v = strtok(nullptr, delim)))
			str_to_number(value1, v);

		if ((v = strtok(nullptr, delim)))
			str_to_number(value2, v);

		TOKEN("section")
		{
			cube_data = std::make_unique<game::items::cube_data>();
		}
		else if (!cube_data)
		{
			metin2::log::error("cube: '{}' outside a section", token_string);
		}
		else TOKEN("npc")
		{
			cube_data->npc_vnum.push_back((uint16_t)value1);
		}
		else TOKEN("item")
		{
			cube_value.vnum		= value1;
			cube_value.count	= value2;

			cube_data->item.push_back(cube_value);
		}
		else TOKEN("reward")
		{
			cube_value.vnum		= value1;
			cube_value.count	= value2;

			cube_data->reward.push_back(cube_value);
		}
		else TOKEN("percent")
		{
			cube_data->percent = value1;
		}
		else TOKEN("gold")
		{
			// Gold required for crafting
			cube_data->gold = value1;
		}
		else TOKEN("end")
		{
			// TODO : check cube data
			if (false == fn_check_cube_data(cube_data.get()))
			{
				metin2::log::debug( "something wrong");
				cube_data.reset();
				continue;
			}
			cube_proto.push_back(std::move(cube_data));
		}
	}

	fclose(fp);
	return true;
}


// The cube holds raw pointers to inventory items. An item can be consumed while the cube is open
// (e.g. prisms spent by chat hyperlinks, expiring items), so drop every pointer that is no longer
// an item in the inventory before it is dereferenced. Only pointer values are compared here.
static void fn_validate_cube_items(character * ch)
{
	item ** cube_item = ch->cube_item();

	for (int i = 0; i < CUBE_MAX_NUM; ++i)
	{
		if (nullptr == cube_item[i])
			continue;

		bool bFound = false;

		for (int cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
		{
			if (ch->inventory_item(cell) == cube_item[i])
			{
				bFound = true;
				break;
			}
		}

		if (!bFound)
			cube_item[i] = nullptr;
	}
}

static bool fn_update_cube_status(character * ch)
{
	if (nullptr == ch)
		return false;

	if (!ch->is_cube_open())
		return false;

	fn_validate_cube_items(ch);

	character *	npc = ch->quest_npc();
	if (nullptr == npc)
		return false;

	cube_data* cube = fn_find_cube(ch->cube_item(), npc->race_num());

	if (nullptr == cube)
	{
		ch->chat_packet(CHAT_TYPE_COMMAND, "cube info 0 0 0");
		return false;
	}

	ch->chat_packet(CHAT_TYPE_COMMAND, "cube info %d %d %d", cube->gold, 0, 0);
	return true;
}

// return new item
bool cube_make (character * ch)
{
	// Finds combinations that require the given item. (referred to as cube data)
	// If there is cube data, check the item's materials.
	// Creates the new item.
	// Give the new item

	character *	npc;
	int			percent_number = 0;
	cube_data	*cube_proto;
	item *	*items;
	item *	new_item;

	if (!(ch)->is_cube_open())
	{
		(ch)->chat_packet(CHAT_TYPE_INFO, lc_text("제조창이 열려있지 않습니다"));
		return false;
	}

	npc = ch->quest_npc();
	if (nullptr == npc)
	{
		return false;
	}

	fn_validate_cube_items(ch);
	items = ch->cube_item();
	cube_proto = fn_find_cube(items, npc->race_num());

	if (nullptr == cube_proto)
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("제조 재료가 부족합니다"));
		return false;
	}

	if (static_cast<int64_t>(ch->gold()) < static_cast<int64_t>(cube_proto->gold))
	{
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("돈이 부족하거나 아이템이 제자리에 없습니다."));	// This text is already widely used, so no additional translation needed
		return false;
	}

	cube_value	*reward_value = cube_proto->reward_value();

	// Delete the used material items
	cube_proto->remove_material (ch);
	
	// Deduct the gold required for crafting
	if (0 < cube_proto->gold)
		ch->point_change(POINT_GOLD, -(cube_proto->gold), false);

	percent_number = number(1,100);
	if ( percent_number<=cube_proto->percent)
	{
		// Success
		ch->chat_packet(CHAT_TYPE_COMMAND, "cube success %d %d", reward_value->vnum, reward_value->count);
		new_item = ch->auto_give_item(reward_value->vnum, reward_value->count);

		log_manager::instance().cube_log(ch->player_id(), ch->x(), ch->y(),
				reward_value->vnum, new_item ? new_item->id() : 0, reward_value->count, 1);
		return true;
	}
	else
	{
		// Failure
		ch->chat_packet(CHAT_TYPE_INFO, lc_text("제조에 실패하였습니다."));	// 2012.11.12 newly added message (must be added to locale_string.txt)
		ch->chat_packet(CHAT_TYPE_COMMAND, "cube fail");
		log_manager::instance().cube_log(ch->player_id(), ch->x(), ch->y(),
				reward_value->vnum, 0, 0, 0);
		return false;
	}

	return false;
}


// Show the items in the cube
void cube_show_list (character * ch)
{
	item *	*cube_item;
	item *	item;

	RETURN_IF_CUBE_IS_NOT_OPENED(ch);

	fn_validate_cube_items(ch);
	cube_item = ch->cube_item();

	for (int i=0; i<CUBE_MAX_NUM; ++i)
	{
		item = cube_item[i];
		if (nullptr==item)	continue;

		ch->chat_packet(CHAT_TYPE_INFO, "cube[%d]: inventory[%d]: %s",
				i, item->cell(), item->name());
	}
}


// Register an inventory item into the cube
void cube_add_item (character * ch, int cube_index, int inven_index)
{
	// Does the item exist?
	// Find an empty slot in the cube
	// Cube setup
	// Send message
	item *	item;
	game::items::item *	*cube_item;

	RETURN_IF_CUBE_IS_NOT_OPENED(ch);

	if (inven_index<0 || INVENTORY_MAX_NUM<=inven_index)
		return;
	if (cube_index<0 || CUBE_MAX_NUM<=cube_index)
		return;

	item = ch->inventory_item(inven_index);

	if (nullptr==item)	return;

	cube_item = ch->cube_item();

	// If the item was already registered at another position, delete the old index
	for (int i=0; i<CUBE_MAX_NUM; ++i)
	{
		if (item==cube_item[i])
		{
			cube_item[i] = nullptr;
			break;
		}
	}

	cube_item[cube_index] = item;

	if (metin2::config::test_server())
		ch->chat_packet(CHAT_TYPE_INFO, "cube[%d]: inventory[%d]: %s added",
									cube_index, inven_index, item->name());

	// Wanted to tell the client what can be made from the items currently in the box
	// but just send how much gold is required instead
	fn_update_cube_status(ch);

	return;
}

// Remove an item from the cube
void cube_delete_item (character * ch, int cube_index)
{
	item *	item;
	game::items::item *	*cube_item;

	RETURN_IF_CUBE_IS_NOT_OPENED(ch);

	if (cube_index<0 || CUBE_MAX_NUM<=cube_index)	return;

	fn_validate_cube_items(ch);
	cube_item = ch->cube_item();

	if ( nullptr== cube_item[cube_index] )	return;

	item = cube_item[cube_index];
	cube_item[cube_index] = nullptr;

	if (metin2::config::test_server())
		ch->chat_packet(CHAT_TYPE_INFO, "cube[%d]: cube[%d]: %s deleted",
				cube_index, item->cell(), item->name());

	// Wanted to tell the client what can be made from the items currently in the box
	// but just send how much gold is required instead
	fn_update_cube_status(ch);

	return;
}

// Splits an item name into the plain name and refine level (Peerless Sword+5 -> Peerless Sword, 5)
item_name_and_level split_item_name_and_level_from_name(const std::string& name)
{
	int level = 0;
	item_name_and_level info;
	info.name = name;

	size_t pos = name.find("+");
	
	if (std::string::npos != pos)
	{
		const std::string levelStr = name.substr(pos + 1, name.size() - pos - 1);
		str_to_number(level, levelStr.c_str());

		info.name = name.substr(0, pos);
	}

	info.level = level;

	return info;
};

bool f_is_less_cube_value(const cube_value& a, const cube_value& b)
{
	return a.vnum < b.vnum;
}

void cube_make_cube_information_text()
{
	// Now convert the organized cube results and materials into info to send to the client.
	for (cube_map_by_npc::iterator iter = cube_info_map.begin(); cube_info_map.end() != iter; ++iter)
	{
		cube_result_list& resultList = iter->second;

		for (cube_result_list::iterator resultIter = resultList.begin(); resultList.end() != resultIter; ++resultIter)
		{
			cube_material_info& materialInfo = *resultIter;
			std::string& infoText = materialInfo.info_text;

			
			// This one is the bad guy
			if (0 < materialInfo.complicate_material.size())
			{
				std::sort(materialInfo.complicate_material.begin(), materialInfo.complicate_material.end(), f_is_less_cube_value);
				std::sort(materialInfo.material.begin(), materialInfo.material.end(), f_is_less_cube_value);

				//// Remove duplicate materials
				for (cube_value_vector::iterator iter = materialInfo.complicate_material.begin(); materialInfo.complicate_material.end() != iter; ++iter)
				{
					for (cube_value_vector::iterator targetIter = materialInfo.material.begin(); materialInfo.material.end() != targetIter; ++targetIter)
					{
						if (*targetIter == *iter)
						{
							targetIter = materialInfo.material.erase(targetIter);
						}
					}
				}

				// Build text following the agreed format like 72723,1 or 72725,1 or ...
				for (cube_value_vector::iterator iter = materialInfo.complicate_material.begin(); materialInfo.complicate_material.end() != iter; ++iter)
				{
					char tempBuffer[128];
					snprintf(tempBuffer, sizeof(tempBuffer), "%d,%d|", iter->vnum, iter->count);
					
					infoText += std::string(tempBuffer);
				}

				infoText.erase(infoText.size() - 1);

				if (0 < materialInfo.material.size())
					infoText.push_back('&');
			}

			// Also build the format for non-duplicate normal materials
			for (cube_value_vector::iterator iter = materialInfo.material.begin(); materialInfo.material.end() != iter; ++iter)
			{
				char tempBuffer[128];
				snprintf(tempBuffer, sizeof(tempBuffer), "%d,%d&", iter->vnum, iter->count);
				infoText += std::string(tempBuffer);
			}

			infoText.erase(infoText.size() - 1);

			// Add gold info if gold is needed to craft
			if (0 < materialInfo.gold)
			{
				char temp[128];
				snprintf(temp, sizeof(temp), "%d", materialInfo.gold);
				infoText += std::string("/") + temp;
			}

		} // for resultList
	} // for npc
}

bool cube_information_initialize()
{
	for (size_t i = 0; i < cube_proto.size(); ++i)
	{
		cube_data* cubeData = cube_proto[i].get();

		const std::vector<cube_value>& rewards = cubeData->reward;

		// Sorry for the hard coding
		if (1 != rewards.size())
		{
			metin2::log::error("[CubeInfo] WARNING! Does not support multiple rewards (count: {:d})", rewards.size());
			continue;
		}

		const cube_value& reward = rewards.at(0);
		const uint16_t& npcVNUM = cubeData->npc_vnum.at(0);
		bool bComplicate = false;
		
		cube_map_by_npc& cubeMap = cube_info_map;
		cube_result_list& resultList = cubeMap[npcVNUM];
		cube_material_info materialInfo;

		materialInfo.reward = reward;
		materialInfo.gold = cubeData->gold;
		materialInfo.material = cubeData->item;

		for (cube_result_list::iterator iter = resultList.begin(); resultList.end() != iter; ++iter)
		{
			cube_material_info& existInfo = *iter;

			// If a duplicate reward is already registered, distinguish whether it's made from an entirely different combination, 
			// or an almost identical combination that differs only in one part.
			// e.g. items that differ only in one part are grouped and shown as a single result, like below:
			// Dragon God Sword:
			//		Peerless Sword+5 ~ +9 x 1
			//		Red hilt fragment x1
			//		Green sword ornament fragment x1
			if (reward.vnum == existInfo.reward.vnum)
			{
				for (cube_value_vector::iterator existMaterialIter = existInfo.material.begin(); existInfo.material.end() != existMaterialIter; ++existMaterialIter)
				{
					item_table* existMaterialProto = item_manager::instance().table(existMaterialIter->vnum);
					if (nullptr == existMaterialProto)
					{
						metin2::log::error("There is no item({:d})", existMaterialIter->vnum);
						return false;
					}
					item_name_and_level existItemInfo = split_item_name_and_level_from_name(existMaterialProto->name);

					if (0 < existItemInfo.level)
					{
						// Between the materials of the cube result being added now and the materials of already registered cube results, 
						// search for overlapping parts
						for (cube_value_vector::iterator currentMaterialIter = materialInfo.material.begin(); materialInfo.material.end() != currentMaterialIter; ++currentMaterialIter)
						{
							item_table* currentMaterialProto = item_manager::instance().table(currentMaterialIter->vnum);
							item_name_and_level currentItemInfo = split_item_name_and_level_from_name(currentMaterialProto->name);

							if (currentItemInfo.name == existItemInfo.name)
							{
								bComplicate = true;
								existInfo.complicate_material.push_back(*currentMaterialIter);

								if (std::find(existInfo.complicate_material.begin(), existInfo.complicate_material.end(), *existMaterialIter) == existInfo.complicate_material.end())
									existInfo.complicate_material.push_back(*existMaterialIter);

								// TODO: may need to detect two or more duplicate items
								break;
							}
						} // for currentMaterialIter
					}	// if level
				}	// for existMaterialInfo
			}	// if (reward.vnum == existInfo.reward.vnum)

		}	// for resultList

		if (false == bComplicate)
			resultList.push_back(materialInfo);
	}

	cube_make_cube_information_text();

	is_initialized_cube_material_information = true;
	return true;
}

// Client to server: request info (list) of items the current NPC can make
void cube_request_result_list(character * ch)
{
	RETURN_IF_CUBE_IS_NOT_OPENED(ch);

	character *	npc = ch->quest_npc();
	if (nullptr == npc)
		return;

	uint32_t npcVNUM = npc->race_num();
	size_t resultCount = 0;

	std::string& resultText = cube_result_info_map_by_npc[npcVNUM];

	// If there's no organized list of what this NPC can make, build the cache
	if (resultText.length() == 0)
	{
		resultText.clear();

		const cube_result_list& resultList = cube_info_map[npcVNUM];
		for (cube_result_list::const_iterator iter = resultList.begin(); resultList.end() != iter; ++iter)
		{
			const cube_material_info& materialInfo = *iter;
			char temp[128];
			snprintf(temp, sizeof(temp), "%d,%d", materialInfo.reward.vnum, materialInfo.reward.count);

			resultText += std::string(temp) + "/";
		}

		resultCount = resultList.size();
		
		if (resultCount == 0) { // Fix
			return;
		}

		resultText.erase(resultText.size() - 1);

		// Logs an error if it exceeds the chat packet limit... ask the designers to adjust it, or switch to another method later...
		if (resultText.size() - 20 >= CHAT_MAX_LEN)
		{
			metin2::log::error("[CubeInfo] Too long cube result list text. (NPC: {:d}, length: {:d})", npcVNUM, resultText.size());
			resultText.clear();
			resultCount = 0;
		}

	}

	// Sends the list of items the current NPC can make in the format below.
	// (Server -> Client) /cube r_list npcVNUM resultCount vnum1,count1/vnum2,count2,/vnum3,count3/...
	// (Server -> Client) /cube r_list 20383 4 123,1/125,1/128,1/130,5
	
	ch->chat_packet(CHAT_TYPE_COMMAND, "cube r_list %d %d %s", npcVNUM, resultCount, resultText.c_str());
}

// 
void cube_request_material_info(character * ch, int request_start_index, int request_count)
{
	RETURN_IF_CUBE_IS_NOT_OPENED(ch);

	character *	npc = ch->quest_npc();
	if (nullptr == npc)
		return;

	uint32_t npcVNUM = npc->race_num();
	std::string materialInfoText = "";

	int index = 0;
	bool bCatchInfo = false;

	const cube_result_list& resultList = cube_info_map[npcVNUM];
	for (cube_result_list::const_iterator iter = resultList.begin(); resultList.end() != iter; ++iter)
	{
		const cube_material_info& materialInfo = *iter;

		if (index++ == request_start_index)
		{
			bCatchInfo = true;
		}
		
		if (bCatchInfo)
		{
			materialInfoText += materialInfo.info_text + "@";
		}

		if (index >= request_start_index + request_count)
			break;
	}

	// if (false == bCatchInfo)
	if (!bCatchInfo || materialInfoText.size() == 0) // Fix
	{
		metin2::log::error("[CubeInfo] Can't find matched material info (NPC: {:d}, index: {:d}, request count: {:d})", npcVNUM, request_start_index, request_count);
		return;
	}

	materialInfoText.erase(materialInfoText.size() - 1);

	// 
	// (Server -> Client) /cube m_info start_index count 125,1|126,2|127,2|123,5&555,5&555,4/120000
	if (materialInfoText.size() - 20 >= CHAT_MAX_LEN)
	{
		metin2::log::error("[CubeInfo] Too long material info. (NPC: {:d}, requestStart: {:d}, requestCount: {:d}, length: {:d})", npcVNUM, request_start_index, request_count, materialInfoText.size());
	}

	ch->chat_packet(CHAT_TYPE_COMMAND, "cube m_info %d %d %s", request_start_index, request_count, materialInfoText.c_str());

	
}
}
