/*********************************************************************
 * date        : 2006.11.20
 * file        : cube.cpp
 * author      : mhh
 * description : 
 */


#include "cube.h"

#include "char.h"
#include "config.h"
#include "dev_log.h"
#include "item.h"
#include "item_manager.h"
#include "locale.hpp"
#include "locale_service.h"
#include "log.h"
#include "typedef.h"
#include "utils.h"

#include <common/length.h>
#include <common/tables.h>
#include <common/utils.h>
#include <libthecore/log.h>
#include <libthecore/utils.h>

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



#define RETURN_IF_CUBE_IS_NOT_OPENED(ch) if (!(ch)->IsCubeOpen()) return


/*--------------------------------------------------------*/
/*                   GLOBAL VARIABLES                     */
/*--------------------------------------------------------*/
static std::vector<std::unique_ptr<CUBE_DATA>>	s_cube_proto;
static bool s_isInitializedCubeMaterialInformation = false;



/*--------------------------------------------------------*/
/*               Cube Material Information                */ 
/*--------------------------------------------------------*/
enum ECubeResultCategory
{
	CUBE_CATEGORY_POTION,				// Herbs, extracts, etc.. (can't be classified as potions, so unused. Herbs and such are all just misc)
	CUBE_CATEGORY_WEAPON,				// Weapon
	CUBE_CATEGORY_ARMOR,				// Armor
};

typedef std::vector<CUBE_VALUE>	TCubeValueVector;

struct SCubeMaterialInfo
{
	SCubeMaterialInfo()
	{
		bHaveComplicateMaterial = false;
	};

	CUBE_VALUE			reward;							// What's the reward
	TCubeValueVector	material;						// What are the materials
	uint32_t				gold;							// How much gold does it cost
	TCubeValueVector	complicateMaterial;				// Complicated -_- materials

	// .. format agreed upon for showing materials on the client
	// 72723,1&72724,2&72730,1
	// 52001,1|52002,1|52003,1&72723,1&72724,5
	//	=> ( 52001,1 or 52002,1 or 52003,1 ) and 72723,1 and 72724,5
	std::string			infoText;		
	bool				bHaveComplicateMaterial;		//
};

struct SItemNameAndLevel
{
	SItemNameAndLevel() { level = 0; }

	std::string		name;
	int				level;
};


// Please excuse the crappy data structures... made this in a soulless state thanks to someone
typedef std::vector<SCubeMaterialInfo>								TCubeResultList;
typedef std::unordered_map<uint32_t, TCubeResultList>				TCubeMapByNPC;				// What each NPC can make and what the materials are...
typedef std::unordered_map<uint32_t, std::string>					TCubeResultInfoTextByNPC;	// Info listing what each NPC can make, organized in the agreed format

TCubeMapByNPC cube_info_map;
TCubeResultInfoTextByNPC cube_result_info_map_by_npc;				// The naming is f***ing stupid lol


/*--------------------------------------------------------*/
/*                  STATIC FUNCTIONS                      */ 
/*--------------------------------------------------------*/
 // Does it have the required item count?
static bool FN_check_item_count (CItem * *items, uint32_t item_vnum, int need_count)
{
	int	count = 0;

	// for all cube
	for (int i=0; i<CUBE_MAX_NUM; ++i)
	{
		if (nullptr==items[i])	continue;

		if (item_vnum==items[i]->GetVnum())
		{
			count += items[i]->GetCount();
		}
	}

	return (count>=need_count);
}

// Removes the materials in the cube.
static void FN_remove_material (CItem * *items, uint32_t item_vnum, int need_count)
{
	int		count	= 0;
	CItem *	item	= nullptr;

	// for all cube
	for (int i=0; i<CUBE_MAX_NUM; ++i)
	{
		if (nullptr==items[i])	continue;

		item = items[i];
		if (item_vnum==item->GetVnum())
		{
			count += item->GetCount();

			if (count>need_count)
			{
				item->SetCount(count-need_count);
				return;
			}
			else
			{
				item->SetCount(0);
				items[i] = nullptr;
			}
		}
	}
}


static CUBE_DATA* FN_find_cube (CItem * *items, uint16_t npc_vnum)
{
	uint32_t	i, end_index;

	if (0==npc_vnum)	return nullptr;

	// FOR ALL CUBE_PROTO
	end_index = s_cube_proto.size();
	for (i=0; i<end_index; ++i)
	{
		if ( s_cube_proto[i]->can_make_item(items, npc_vnum) )
			return s_cube_proto[i].get();
	}

	return nullptr;
}

static bool FN_check_valid_npc( uint16_t vnum )
{
	for ( auto iter = s_cube_proto.begin(); iter != s_cube_proto.end(); iter++ )
	{
		if ( std::find((*iter)->npc_vnum.begin(), (*iter)->npc_vnum.end(), vnum) != (*iter)->npc_vnum.end() )
			return true;
	}

	return false;
}

// Checks whether the cube data was initialized correctly.
static bool FN_check_cube_data (CUBE_DATA *cube_data)
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

CUBE_DATA::CUBE_DATA()
{
	this->percent = 0;
	this->gold = 0;
}

// Checks whether the required material quantities are satisfied.
bool CUBE_DATA::can_make_item (CItem * *items, uint16_t npc_vnum)
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

		if ( false==FN_check_item_count(items, need_vnum, need_count) )
			return false;
	}

	return true;
}

// Determines the kind of item produced when the cube is run
CUBE_VALUE* CUBE_DATA::reward_value ()
{
	int		end_index		= 0;
	uint32_t	reward_index	= 0;

	end_index = this->reward.size();
	reward_index = number(0, end_index);
	reward_index = number(0, end_index-1);

	return &this->reward[reward_index];
}

// Removes the materials in the cube
void CUBE_DATA::remove_material (CHARACTER * ch)
{
	uint32_t	i, end_index;
	uint32_t	need_vnum;
	int		need_count;
	CItem *	*items = ch->GetCubeItem();

	end_index = this->item.size();
	for (i=0; i<end_index; ++i)
	{
		need_vnum	= this->item[i].vnum;
		need_count	= this->item[i].count;

		FN_remove_material (items, need_vnum, need_count);
	}
}

void Cube_clean_item (CHARACTER * ch)
{
	CItem *	*cube_item;

	cube_item = ch->GetCubeItem();

	for (int i=0; i<CUBE_MAX_NUM; ++i)
	{
		if (nullptr == cube_item[i])
			continue;

		cube_item[i] = nullptr;
	}
}

// Open cube window
void Cube_open (CHARACTER * ch)
{
	if (false == s_isInitializedCubeMaterialInformation)
	{
		Cube_InformationInitialize();
	}

	if (nullptr == ch)
		return;

	CHARACTER *	npc;
	npc = ch->GetQuestNPC();
	if (nullptr==npc)
	{
		if (test_server)
			dev_log(LOG_DEB0, "cube_npc is NULL");
		return;
	}

	if ( FN_check_valid_npc(npc->GetRaceNum()) == false )
	{
		if ( test_server == true )
		{
			dev_log(LOG_DEB0, "cube not valid NPC");
		}
		return;
	}

	if (ch->IsCubeOpen())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("이미 제조창이 열려있습니다."));
		return;
	}
	if ( ch->GetExchange() || ch->GetMyShop() || ch->GetShopOwner() || ch->IsOpenSafebox() || ch->IsCubeOpen() )
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("다른 거래중(창고,교환,상점)에는 사용할 수 없습니다."));
		return;
	}

	int32_t distance = DISTANCE_APPROX(ch->GetX() - npc->GetX(), ch->GetY() - npc->GetY());

	if (distance >= CUBE_MAX_DISTANCE)
	{
		sys_log(1, "CUBE: TOO_FAR: %s distance %d", ch->GetName(), distance);
		return;
	}


	Cube_clean_item(ch);
	ch->SetCubeNpc(npc);
	ch->ChatPacket(CHAT_TYPE_COMMAND, "cube open %d", npc->GetRaceNum());
}

// Cube cancel
void Cube_close (CHARACTER * ch)
{
	RETURN_IF_CUBE_IS_NOT_OPENED(ch);
	Cube_clean_item(ch);
	ch->SetCubeNpc(nullptr);
	ch->ChatPacket(CHAT_TYPE_COMMAND, "cube close");
	dev_log(LOG_DEB0, "<CUBE> close (%s)", ch->GetName());
}

void Cube_init()
{
	char file_name[256+1];
	snprintf(file_name, sizeof(file_name), "%s/cube.txt", LocaleService_GetBasePath().c_str());

	sys_log(0, "Cube_Init %s", file_name);

	s_cube_proto.clear();

	if (false == Cube_load(file_name))
		sys_err("Cube_Init failed");
}

bool Cube_load (const char *file)
{
	FILE	*fp;
	char	one_line[256];
	int		value1, value2;
	const char	*delim = " \t\r\n";
	char	*v, *token_string;
	std::unique_ptr<CUBE_DATA>	cube_data;
	CUBE_VALUE	cube_value = {0,0};

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
			cube_data = std::make_unique<CUBE_DATA>();
		}
		else if (!cube_data)
		{
			sys_err("cube: '%s' outside a section", token_string);
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
			if (false == FN_check_cube_data(cube_data.get()))
			{
				dev_log(LOG_DEB0, "something wrong");
				cube_data.reset();
				continue;
			}
			s_cube_proto.push_back(std::move(cube_data));
		}
	}

	fclose(fp);
	return true;
}


static bool FN_update_cube_status(CHARACTER * ch)
{
	if (nullptr == ch)
		return false;

	if (!ch->IsCubeOpen())
		return false;

	CHARACTER *	npc = ch->GetQuestNPC();
	if (nullptr == npc)
		return false;

	CUBE_DATA* cube = FN_find_cube(ch->GetCubeItem(), npc->GetRaceNum());

	if (nullptr == cube)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "cube info 0 0 0");
		return false;
	}

	ch->ChatPacket(CHAT_TYPE_COMMAND, "cube info %d %d %d", cube->gold, 0, 0);
	return true;
}

// return new item
bool Cube_make (CHARACTER * ch)
{
	// Finds combinations that require the given item. (referred to as cube data)
	// If there is cube data, check the item's materials.
	// Creates the new item.
	// Give the new item

	CHARACTER *	npc;
	int			percent_number = 0;
	CUBE_DATA	*cube_proto;
	CItem *	*items;
	CItem *	new_item;

	if (!(ch)->IsCubeOpen())
	{
		(ch)->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("제조창이 열려있지 않습니다"));
		return false;
	}

	npc = ch->GetQuestNPC();
	if (nullptr == npc)
	{
		return false;
	}

	items = ch->GetCubeItem();
	cube_proto = FN_find_cube(items, npc->GetRaceNum());

	if (nullptr == cube_proto)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("제조 재료가 부족합니다"));
		return false;
	}

	if (static_cast<int64_t>(ch->GetGold()) < static_cast<int64_t>(cube_proto->gold))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("돈이 부족하거나 아이템이 제자리에 없습니다."));	// This text is already widely used, so no additional translation needed
		return false;
	}

	CUBE_VALUE	*reward_value = cube_proto->reward_value();

	// Delete the used material items
	cube_proto->remove_material (ch);
	
	// Deduct the gold required for crafting
	if (0 < cube_proto->gold)
		ch->PointChange(POINT_GOLD, -(cube_proto->gold), false);

	percent_number = number(1,100);
	if ( percent_number<=cube_proto->percent)
	{
		// Success
		ch->ChatPacket(CHAT_TYPE_COMMAND, "cube success %d %d", reward_value->vnum, reward_value->count);
		new_item = ch->AutoGiveItem(reward_value->vnum, reward_value->count);

		LogManager::instance().CubeLog(ch->GetPlayerID(), ch->GetX(), ch->GetY(),
				reward_value->vnum, new_item->GetID(), reward_value->count, 1);
		return true;
	}
	else
	{
		// Failure
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("제조에 실패하였습니다."));	// 2012.11.12 newly added message (must be added to locale_string.txt)
		ch->ChatPacket(CHAT_TYPE_COMMAND, "cube fail");
		LogManager::instance().CubeLog(ch->GetPlayerID(), ch->GetX(), ch->GetY(),
				reward_value->vnum, 0, 0, 0);
		return false;
	}

	return false;
}


// Show the items in the cube
void Cube_show_list (CHARACTER * ch)
{
	CItem *	*cube_item;
	CItem *	item;

	RETURN_IF_CUBE_IS_NOT_OPENED(ch);

	cube_item = ch->GetCubeItem();

	for (int i=0; i<CUBE_MAX_NUM; ++i)
	{
		item = cube_item[i];
		if (nullptr==item)	continue;

		ch->ChatPacket(CHAT_TYPE_INFO, "cube[%d]: inventory[%d]: %s",
				i, item->GetCell(), item->GetName());
	}
}


// Register an inventory item into the cube
void Cube_add_item (CHARACTER * ch, int cube_index, int inven_index)
{
	// Does the item exist?
	// Find an empty slot in the cube
	// Cube setup
	// Send message
	CItem *	item;
	CItem *	*cube_item;

	RETURN_IF_CUBE_IS_NOT_OPENED(ch);

	if (inven_index<0 || INVENTORY_MAX_NUM<=inven_index)
		return;
	if (cube_index<0 || CUBE_MAX_NUM<=cube_index)
		return;

	item = ch->GetInventoryItem(inven_index);

	if (nullptr==item)	return;

	cube_item = ch->GetCubeItem();

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

	if (test_server)
		ch->ChatPacket(CHAT_TYPE_INFO, "cube[%d]: inventory[%d]: %s added",
									cube_index, inven_index, item->GetName());

	// Wanted to tell the client what can be made from the items currently in the box
	// but just send how much gold is required instead
	FN_update_cube_status(ch);

	return;
}

// Remove an item from the cube
void Cube_delete_item (CHARACTER * ch, int cube_index)
{
	CItem *	item;
	CItem *	*cube_item;

	RETURN_IF_CUBE_IS_NOT_OPENED(ch);

	if (cube_index<0 || CUBE_MAX_NUM<=cube_index)	return;

	cube_item = ch->GetCubeItem();

	if ( nullptr== cube_item[cube_index] )	return;

	item = cube_item[cube_index];
	cube_item[cube_index] = nullptr;

	if (test_server)
		ch->ChatPacket(CHAT_TYPE_INFO, "cube[%d]: cube[%d]: %s deleted",
				cube_index, item->GetCell(), item->GetName());

	// Wanted to tell the client what can be made from the items currently in the box
	// but just send how much gold is required instead
	FN_update_cube_status(ch);

	return;
}

// Splits an item name into the plain name and refine level (Peerless Sword+5 -> Peerless Sword, 5)
SItemNameAndLevel SplitItemNameAndLevelFromName(const std::string& name)
{
	int level = 0;
	SItemNameAndLevel info;
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

bool FIsLessCubeValue(const CUBE_VALUE& a, const CUBE_VALUE& b)
{
	return a.vnum < b.vnum;
}

void Cube_MakeCubeInformationText()
{
	// Now convert the organized cube results and materials into info to send to the client.
	for (TCubeMapByNPC::iterator iter = cube_info_map.begin(); cube_info_map.end() != iter; ++iter)
	{
		TCubeResultList& resultList = iter->second;

		for (TCubeResultList::iterator resultIter = resultList.begin(); resultList.end() != resultIter; ++resultIter)
		{
			SCubeMaterialInfo& materialInfo = *resultIter;
			std::string& infoText = materialInfo.infoText;

			
			// This one is the bad guy
			if (0 < materialInfo.complicateMaterial.size())
			{
				std::sort(materialInfo.complicateMaterial.begin(), materialInfo.complicateMaterial.end(), FIsLessCubeValue);
				std::sort(materialInfo.material.begin(), materialInfo.material.end(), FIsLessCubeValue);

				//// Remove duplicate materials
				for (TCubeValueVector::iterator iter = materialInfo.complicateMaterial.begin(); materialInfo.complicateMaterial.end() != iter; ++iter)
				{
					for (TCubeValueVector::iterator targetIter = materialInfo.material.begin(); materialInfo.material.end() != targetIter; ++targetIter)
					{
						if (*targetIter == *iter)
						{
							targetIter = materialInfo.material.erase(targetIter);
						}
					}
				}

				// Build text following the agreed format like 72723,1 or 72725,1 or ...
				for (TCubeValueVector::iterator iter = materialInfo.complicateMaterial.begin(); materialInfo.complicateMaterial.end() != iter; ++iter)
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
			for (TCubeValueVector::iterator iter = materialInfo.material.begin(); materialInfo.material.end() != iter; ++iter)
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

bool Cube_InformationInitialize()
{
	for (size_t i = 0; i < s_cube_proto.size(); ++i)
	{
		CUBE_DATA* cubeData = s_cube_proto[i].get();

		const std::vector<CUBE_VALUE>& rewards = cubeData->reward;

		// Sorry for the hard coding
		if (1 != rewards.size())
		{
			sys_err("[CubeInfo] WARNING! Does not support multiple rewards (count: %zu)", rewards.size());
			continue;
		}

		const CUBE_VALUE& reward = rewards.at(0);
		const uint16_t& npcVNUM = cubeData->npc_vnum.at(0);
		bool bComplicate = false;
		
		TCubeMapByNPC& cubeMap = cube_info_map;
		TCubeResultList& resultList = cubeMap[npcVNUM];
		SCubeMaterialInfo materialInfo;

		materialInfo.reward = reward;
		materialInfo.gold = cubeData->gold;
		materialInfo.material = cubeData->item;

		for (TCubeResultList::iterator iter = resultList.begin(); resultList.end() != iter; ++iter)
		{
			SCubeMaterialInfo& existInfo = *iter;

			// If a duplicate reward is already registered, distinguish whether it's made from an entirely different combination, 
			// or an almost identical combination that differs only in one part.
			// e.g. items that differ only in one part are grouped and shown as a single result, like below:
			// Dragon God Sword:
			//		Peerless Sword+5 ~ +9 x 1
			//		Red hilt fragment x1
			//		Green sword ornament fragment x1
			if (reward.vnum == existInfo.reward.vnum)
			{
				for (TCubeValueVector::iterator existMaterialIter = existInfo.material.begin(); existInfo.material.end() != existMaterialIter; ++existMaterialIter)
				{
					TItemTable* existMaterialProto = ITEM_MANAGER::instance().GetTable(existMaterialIter->vnum);
					if (nullptr == existMaterialProto)
					{
						sys_err("There is no item(%u)", existMaterialIter->vnum);
						return false;
					}
					SItemNameAndLevel existItemInfo = SplitItemNameAndLevelFromName(existMaterialProto->szName);

					if (0 < existItemInfo.level)
					{
						// Between the materials of the cube result being added now and the materials of already registered cube results, 
						// search for overlapping parts
						for (TCubeValueVector::iterator currentMaterialIter = materialInfo.material.begin(); materialInfo.material.end() != currentMaterialIter; ++currentMaterialIter)
						{
							TItemTable* currentMaterialProto = ITEM_MANAGER::instance().GetTable(currentMaterialIter->vnum);
							SItemNameAndLevel currentItemInfo = SplitItemNameAndLevelFromName(currentMaterialProto->szName);

							if (currentItemInfo.name == existItemInfo.name)
							{
								bComplicate = true;
								existInfo.complicateMaterial.push_back(*currentMaterialIter);

								if (std::find(existInfo.complicateMaterial.begin(), existInfo.complicateMaterial.end(), *existMaterialIter) == existInfo.complicateMaterial.end())
									existInfo.complicateMaterial.push_back(*existMaterialIter);

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

	Cube_MakeCubeInformationText();

	s_isInitializedCubeMaterialInformation = true;
	return true;
}

// Client to server: request info (list) of items the current NPC can make
void Cube_request_result_list(CHARACTER * ch)
{
	RETURN_IF_CUBE_IS_NOT_OPENED(ch);

	CHARACTER *	npc = ch->GetQuestNPC();
	if (nullptr == npc)
		return;

	uint32_t npcVNUM = npc->GetRaceNum();
	size_t resultCount = 0;

	std::string& resultText = cube_result_info_map_by_npc[npcVNUM];

	// If there's no organized list of what this NPC can make, build the cache
	if (resultText.length() == 0)
	{
		resultText.clear();

		const TCubeResultList& resultList = cube_info_map[npcVNUM];
		for (TCubeResultList::const_iterator iter = resultList.begin(); resultList.end() != iter; ++iter)
		{
			const SCubeMaterialInfo& materialInfo = *iter;
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
			sys_err("[CubeInfo] Too long cube result list text. (NPC: %d, length: %zu)", npcVNUM, resultText.size());
			resultText.clear();
			resultCount = 0;
		}

	}

	// Sends the list of items the current NPC can make in the format below.
	// (Server -> Client) /cube r_list npcVNUM resultCount vnum1,count1/vnum2,count2,/vnum3,count3/...
	// (Server -> Client) /cube r_list 20383 4 123,1/125,1/128,1/130,5
	
	ch->ChatPacket(CHAT_TYPE_COMMAND, "cube r_list %d %d %s", npcVNUM, resultCount, resultText.c_str());
}

// 
void Cube_request_material_info(CHARACTER * ch, int requestStartIndex, int requestCount)
{
	RETURN_IF_CUBE_IS_NOT_OPENED(ch);

	CHARACTER *	npc = ch->GetQuestNPC();
	if (nullptr == npc)
		return;

	uint32_t npcVNUM = npc->GetRaceNum();
	std::string materialInfoText = "";

	int index = 0;
	bool bCatchInfo = false;

	const TCubeResultList& resultList = cube_info_map[npcVNUM];
	for (TCubeResultList::const_iterator iter = resultList.begin(); resultList.end() != iter; ++iter)
	{
		const SCubeMaterialInfo& materialInfo = *iter;

		if (index++ == requestStartIndex)
		{
			bCatchInfo = true;
		}
		
		if (bCatchInfo)
		{
			materialInfoText += materialInfo.infoText + "@";
		}

		if (index >= requestStartIndex + requestCount)
			break;
	}

	// if (false == bCatchInfo)
	if (!bCatchInfo || materialInfoText.size() == 0) // Fix
	{
		sys_err("[CubeInfo] Can't find matched material info (NPC: %d, index: %d, request count: %d)", npcVNUM, requestStartIndex, requestCount);
		return;
	}

	materialInfoText.erase(materialInfoText.size() - 1);

	// 
	// (Server -> Client) /cube m_info start_index count 125,1|126,2|127,2|123,5&555,5&555,4/120000
	if (materialInfoText.size() - 20 >= CHAT_MAX_LEN)
	{
		sys_err("[CubeInfo] Too long material info. (NPC: %d, requestStart: %d, requestCount: %d, length: %zu)", npcVNUM, requestStartIndex, requestCount, materialInfoText.size());
	}

	ch->ChatPacket(CHAT_TYPE_COMMAND, "cube m_info %d %d %s", requestStartIndex, requestCount, materialInfoText.c_str());

	
}
