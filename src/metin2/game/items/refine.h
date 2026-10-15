#pragma once

#include <metin2/common/singleton.h>
#include <metin2/proto/tables.h>

#include <cstdint>
#include <map>

namespace metin2::game::inline items {

enum
{
	BLACKSMITH_MOB = 20016, // Probability-based upgrade
	ALCHEMIST_MOB = 20001, // 100% upgrade success 

	BLACKSMITH_WEAPON_MOB = 20044,
	BLACKSMITH_ARMOR_MOB = 20045,
	BLACKSMITH_ACCESSORY_MOB = 20046,

	DEVILTOWER_BLACKSMITH_WEAPON_MOB = 20074,
	DEVILTOWER_BLACKSMITH_ARMOR_MOB = 20075,
	DEVILTOWER_BLACKSMITH_ACCESSORY_MOB = 20076,

	BLACKSMITH2_MOB	= 20091,
};

class refine_manager : public singleton<refine_manager>
{
	typedef std::map<uint32_t, refine_table> refine_recipe_map;
	public:
	refine_manager();
	~refine_manager() override;

	bool	initialize(refine_table * table, int size);
	const refine_table* refine_recipe(uint32_t vnum);

	private:
	refine_recipe_map    map_refine_recipe_;

};
}
