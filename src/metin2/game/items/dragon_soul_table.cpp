#include <metin2/game/items/dragon_soul_table.h>

#include <metin2/game/app/constants.h>
#include <metin2/game/data/group_text_parse_tree.h>
#include <metin2/game/items/item_manager.h>

#include <metin2/proto/length.h>
#include <metin2/log/log.h>


#include <cstdint>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <sys/types.h>

namespace metin2::game::inline items {

const std::string ast_grade_name[] =
{
	"grade_normal",
	"grade_brilliant",
	"grade_rare",
	"grade_ancient",
	"grade_legendary",
};

const std::string ast_step_name[] =
{
	"step_lowest",
	"step_low",
	"step_mid",
	"step_high",
	"step_highest",
};

const std::string ast_material_name[] = 
{
	"material_leather",
	"material_blood",
	"material_root",
	"material_needle",
	"material_jewel",
	"material_ds_refine_normal", 
	"material_ds_refine_blessed", 
	"material_ds_refine_holly",
};

bool dragon_soul_table::read_dragon_soul_table_file(const char * file_name)
{
	loader_ = std::make_unique<group_text_parse_tree_loader>();
	group_text_parse_tree_loader& loader = *loader_;

	if (false == loader.load(file_name))
	{
		metin2::log::error("dragon_soul_table.txt load error");
		return false;
	}

	if (!read_vnum_mapper())
		return false;

	if (!read_basic_applys())
		return false;

	if (!read_additional_applys())
		return false;

	apply_num_setting_node_ = loader.group("applynumsettings");
	weight_table_node_ = loader.group("weighttables");
	refine_grade_table_node_ = loader.group("refinegradetables");
	refine_step_table_node_ = loader.group("refinesteptables");
	refine_strength_table_node_ = loader.group("refinestrengthtables");
	dragon_heart_ext_table_node_ = loader.group("dragonheartexttables");
	dragon_soul_ext_table_node_ = loader.group("dragonsoulexttables");

	if (check_apply_num_settings()
		&& check_weight_tables()
		&& check_refine_grade_tables()
		&& check_refine_step_tables()
		&& check_refine_strength_tables()
		&& check_dragon_heart_ext_tables()
		&& check_dragon_soul_ext_tables())
	{
		return true;
	}
	else
	{
		metin2::log::error("DragonSoul table Check failed.");
		return false;
	}
}

bool dragon_soul_table::dragon_soul_group_name(uint8_t type, std::string& group_name) const
{
	dragon_soul_table::map_type_to_name::const_iterator it = map_type_to_name_.find (type);
	if (it != map_type_to_name_.end())
	{
		group_name = it->second;
		return true;
	}
	else
	{
		metin2::log::error("Invalid DragonSoul Type({:d})", type);
		return false;
	}
}

bool dragon_soul_table::read_vnum_mapper()
{
	std::string stName;

	group_node* pGroupNode = loader_->group("vnummapper");

	if (nullptr == pGroupNode)
	{
		metin2::log::error("dragon_soul_table.txt need VnumMapper.");
		return false;
	}
	{
		int n = pGroupNode->row_count();
		if (0 == n)
		{
			metin2::log::error("Group VnumMapper is Empty.");
			return false;
		}

		std::set <uint8_t> setTypes;

		for (int i = 0; i < n; i++)
		{
			const group_node::group_node_row* pRow;
			pGroupNode->row(i, &pRow);
			
			std::string stDragonSoulName;
			uint8_t bType;
			if (!pRow->value("dragonsoulname", stDragonSoulName))
			{
				metin2::log::error("In Group VnumMapper, No DragonSoulName column.");
				return false;
			}
			if (!pRow->value("type", bType))
			{
				metin2::log::error("In Group VnumMapper, {}'s Vnum is invalid", stDragonSoulName.c_str());
				return false;
			}

			if (setTypes.end() != setTypes.find(bType))
			{
				metin2::log::error("In Group VnumMapper, duplicated vnum exist.");
				return false;
			}
			map_name_to_type_.insert(map_name_to_type::value_type(stDragonSoulName, bType));
			map_type_to_name_.insert(map_type_to_name::value_type(bType, stDragonSoulName));
			dragon_soul_types_.push_back(bType);
			dragon_soul_names_.push_back(stDragonSoulName);
		}
	}
	return true;
}

bool dragon_soul_table::read_basic_applys()
{
	group_node* pGroupNode = loader_->group("basicapplys");

	if (nullptr == pGroupNode)
	{
		metin2::log::error("dragon_soul_table.txt need BasicApplys.");
		return false;
	}

	for (uint i = 0; i < dragon_soul_names_.size(); i++)
	{
		group_node* pChild;
		if (nullptr == (pChild = pGroupNode->child_node(dragon_soul_names_[i])))
		{
			metin2::log::error("In Group BasicApplys, {} group is not defined.", dragon_soul_names_[i].c_str());
			return false;
		}
		vec_applys vecApplys;
		int n = pChild->row_count();
		
		for (int j = 1; j <= n; j++)
		{
			std::stringstream ss;
			ss << j;
			const group_node::group_node_row* pRow = nullptr;

			pChild->row(ss.str(), &pRow);
			if (nullptr == pRow)
			{
				metin2::log::error("In Group BasicApplys, No {:d} row.", j);
			}
			apply_types at;
			int av;

			std::string stTypeName;
			if (!pRow->value("apply_type", stTypeName))
			{
				metin2::log::error("In Group BasicApplys, {} group's apply_type is empty.", dragon_soul_names_[i].c_str());
				return false;
			}
			if (!(at = (apply_types)fn_get_apply_type(stTypeName.c_str())))
			{
				metin2::log::error("In Group BasicApplys, {} group's apply_type {} is invalid.", dragon_soul_names_[i].c_str(), stTypeName.c_str());
				return false;
			}
			if (!pRow->value("apply_value", av))
			{
				metin2::log::error("In Group BasicApplys, {} group's apply_value {:d} is invalid.", dragon_soul_names_[i].c_str(), av);
				return false;
			}
			vecApplys.push_back(apply(at, av));
		}
		map_basic_applys_group_.insert (map_apply_group::value_type (map_name_to_type_[dragon_soul_names_[i]], vecApplys));
	}

	return true;
}

bool dragon_soul_table::read_additional_applys()
{
	group_node* pGroupNode = loader_->group("additionalapplys");
	if (nullptr == pGroupNode)
	{
		metin2::log::error("dragon_soul_table.txt need AdditionalApplys.");
		return false;
	}

	for (size_t i = 0; i < dragon_soul_names_.size(); i++)
	{
		group_node* pChild;
		if (nullptr == (pChild = pGroupNode->child_node(dragon_soul_names_[i])))
		{
			metin2::log::error("In Group AdditionalApplys, {} group is not defined.", dragon_soul_names_[i].c_str());
			return false;
		}
		vec_applys vecApplys;
		
		int n = pChild->row_count();

		for (int j = 0; j < n; j++)
		{
			const group_node::group_node_row* pRow;

			pChild->row(j, &pRow);

			apply_types at;
			int av;
			float prob;
			std::string stTypeName;
			if (!pRow->value("apply_type", stTypeName))
			{
				metin2::log::error("In Group AdditionalApplys, {} group's apply_type is empty.", dragon_soul_names_[i].c_str());
				return false;
			}
			if (!(at = (apply_types)fn_get_apply_type(stTypeName.c_str())))
			{
				metin2::log::error("In Group AdditionalApplys, {} group's apply_type {} is invalid.", dragon_soul_names_[i].c_str(), stTypeName.c_str());
				return false;
			}
			if (!pRow->value("apply_value", av))
			{
				metin2::log::error("In Group AdditionalApplys, {} group's apply_value {:d} is invalid.", dragon_soul_names_[i].c_str(), av);
				return false;
			}
			if (!pRow->value("prob", prob))
			{
				metin2::log::error("In Group AdditionalApplys, {} group's probability {:g} is invalid.", dragon_soul_names_[i].c_str(), prob);
				return false;
			}
			vecApplys.push_back(apply(at, av, prob));
		}
		map_additional_applys_group_.insert (map_apply_group::value_type (map_name_to_type_[dragon_soul_names_[i]], vecApplys));
	}

	return true;
}

bool dragon_soul_table::check_apply_num_settings ()
{
	if (nullptr == apply_num_setting_node_)
	{
		metin2::log::error("dragon_soul_table.txt need ApplyNumSettings.");
		return false;
	}
	else
	{
		for (size_t i = 0; i < dragon_soul_types_.size(); i++)
		{
			for (int j = 0; j < DRAGON_SOUL_GRADE_MAX; j++)
			{
				int basis, add_min, add_max;
				if (!apply_num_settings(dragon_soul_types_[i], j, basis, add_min, add_max))
				{
					metin2::log::error("In {} group of ApplyNumSettings, values in Grade({}) row is invalid.", 
						dragon_soul_names_[i].c_str(), ast_grade_name[j].c_str());
					return false;
				}
			}
		}
	}

	return true;
}

bool dragon_soul_table::check_weight_tables ()
{
	if (nullptr == weight_table_node_)
	{
		metin2::log::error("dragon_soul_table.txt need WeightTables.");
		return false;
	}
	else
	{
		for (size_t i = 0; i < dragon_soul_types_.size(); i++)
		{
			for (int j = 0; j < DRAGON_SOUL_GRADE_MAX; j++)
			{
				for (int k = 0; k < DRAGON_SOUL_STEP_MAX; k++)
				{
					for (int l = 0; l < DRAGON_SOUL_STRENGTH_MAX; l++)
					{
						float fWeight;
						if (!weight(dragon_soul_types_[i], j, k, l, fWeight))
						{
							metin2::log::error("In {} group of WeightTables, value(Grade({}), Step({}), Strength({:d}) is invalid.", 
								dragon_soul_names_[i].c_str(), ast_grade_name[j].c_str(), ast_step_name[k].c_str(), l);
						}
					}
				}
			}
		}
	}
	return true;
}

bool dragon_soul_table::check_refine_grade_tables()
{
	if (nullptr == refine_grade_table_node_)
	{
		metin2::log::error("dragon_soul_table.txt need RefineGradeTables.");
		return false;
	}
	else
	{
		for (size_t i = 0; i < dragon_soul_types_.size(); i++)
		{
			for (int j = 0; j < DRAGON_SOUL_GRADE_MAX - 1; j++)
			{
				int need_count, fee;
				std::vector <float> vec_probs;
				if (!refine_grade_values(dragon_soul_types_[i], j, need_count, fee, vec_probs))
				{
					metin2::log::error("In {} group of RefineGradeTables, values in Grade({}) row is invalid.", 
						dragon_soul_names_[i].c_str(), ast_grade_name[j].c_str());
					return false;
				}
				if (need_count < 1)
				{
					metin2::log::error("In {} group of RefineGradeTables, need_count of Grade({}) is less than 1.", 
						dragon_soul_names_[i].c_str(), ast_grade_name[j].c_str());
					return false;
				}
				if (fee < 0)
				{
					metin2::log::error("In {} group of RefineGradeTables, fee of Grade({}) is less than 0.", 
						dragon_soul_names_[i].c_str(), ast_grade_name[j].c_str());
					return false;
				}
				if (DRAGON_SOUL_GRADE_MAX != vec_probs.size())
				{
					metin2::log::error("In RefineGradeTables, probability list size is not {:d}.", DRAGON_SOUL_GRADE_MAX);
					return false;
				}
				for (size_t k = 0; k < vec_probs.size(); k++)
				{
					if (vec_probs[k] < 0.f)
					{
						metin2::log::error("In RefineGradeTables, probability(index : {:d}) is less than 0.", k);
						return false;
					}
				}
			}
		}
	}

	return true;
}

bool dragon_soul_table::check_refine_step_tables ()
{
	if (nullptr == refine_strength_table_node_)
	{
		metin2::log::error("dragon_soul_table.txt need RefineStepTables.");
		return false;
	}
	else
	{
		for (size_t i = 0; i < dragon_soul_types_.size(); i++)
		{
			for (int j = 0; j < DRAGON_SOUL_STEP_MAX - 1; j++)
			{
				int need_count, fee;
				std::vector <float> vec_probs;
				if (!refine_step_values(dragon_soul_types_[i], j, need_count, fee, vec_probs))
				{
					metin2::log::error("In {} group of RefineStepTables, values in Step({}) row is invalid.", 
						dragon_soul_names_[i].c_str(), ast_step_name[j].c_str());
					return false;
				}
				if (need_count < 1)
				{
					metin2::log::error("In {} group of RefineStepTables, need_count of Step({}) is less than 1.", 
						dragon_soul_names_[i].c_str(), ast_step_name[j].c_str());
					return false;
				}
				if (fee < 0)
				{
					metin2::log::error("In {} group of RefineStepTables, fee of Step({}) is less than 0.", 
						dragon_soul_names_[i].c_str(), ast_step_name[j].c_str());
					return false;
				}
				if (DRAGON_SOUL_GRADE_MAX != vec_probs.size())
				{
					metin2::log::error("In {} group of RefineStepTables, probability list size is not {:d}.", 
						dragon_soul_names_[i].c_str(), DRAGON_SOUL_GRADE_MAX);
					return false;
				}
				for (size_t k = 0; k < vec_probs.size(); k++)
				{
					if (vec_probs[k] < 0.f)
					{
						metin2::log::error("In {} group of RefineStepTables, probability(index : {:d}) is less than 0.", 
							dragon_soul_names_[i].c_str(), k);
						return false;
					}
				}
			}
		}
	}

	return true;
}


bool dragon_soul_table::check_refine_strength_tables()
{
	group_node* pGroupNode = refine_strength_table_node_;
	if (nullptr == pGroupNode)
	{
		metin2::log::error("dragon_soul_table.txt need RefineStrengthTables.");
		return false;
	}
	for (size_t i = 0; i < dragon_soul_types_.size(); i++)
	{
		for (int j = MATERIAL_DS_REFINE_NORMAL; j <= MATERIAL_DS_REFINE_HOLLY; j++)
		{
			int fee;
			float prob;
			for (int k = 0; k < DRAGON_SOUL_STRENGTH_MAX -1; k++)
			{
				if (!refine_strength_values(dragon_soul_types_[i], j, k, fee, prob))
				{
					metin2::log::error("In {} group of RefineStrengthTables, value(Material({}), Strength({:d})) or fee are invalid.", 
						dragon_soul_names_[i].c_str(), ast_material_name[j].c_str(), k);
					return false;
				}
				if (fee < 0)
				{
					metin2::log::error("In {} group of RefineStrengthTables, fee of Material({}) is less than 0.", 
						dragon_soul_names_[i].c_str(), ast_material_name[j].c_str());
					return false;
				}
				if (prob < 0.f)
				{
					metin2::log::error("In {} group of RefineStrengthTables, probability(Material({}), Strength({:d})) is less than 0.", 
						dragon_soul_names_[i].c_str(), ast_material_name[j].c_str(), k);
					return false;
				}
			}
		}
	}

	return true;
}

bool dragon_soul_table::check_dragon_heart_ext_tables()
{
	if (nullptr == dragon_heart_ext_table_node_)
	{
		metin2::log::error("dragon_soul_table.txt need DragonHeartExtTables.");
		return false;
	}
	for (size_t i = 0; i < dragon_soul_types_.size(); i++)
	{
		for (int j = 0; j < DRAGON_SOUL_GRADE_MAX; j++)
		{
			std::vector <float> vec_chargings;
			std::vector <float> vec_probs;

			if (!dragon_heart_ext_values(dragon_soul_types_[i], j, vec_chargings, vec_probs))
			{
				metin2::log::error("In {} group of DragonHeartExtTables, CHARGING row or Grade({}) row are invalid.", 
					dragon_soul_names_[i].c_str(), ast_grade_name[j].c_str());
				return false;
			}
			if (vec_chargings.size() != vec_probs.size())
			{
				metin2::log::error("In {} group of DragonHeartExtTables, CHARGING row size({:d}) are not equal probability row size({:d}).",
					dragon_soul_names_[i].c_str(), vec_chargings.size(), vec_probs.size());
				return false;
			}
			for (size_t k = 0; k < vec_chargings.size(); k++)
			{
				if (vec_chargings[k] < 0.f)
				{
					metin2::log::error("In {} group of DragonHeartExtTables, CHARGING value(index : {:d}) is less than 0", 
						dragon_soul_names_[i].c_str(), k);
					return false;
				}
			}
			for (size_t k = 0; k < vec_probs.size(); k++)
			{
				if (vec_probs[k] < 0.f)
				{
					metin2::log::error("In {} group of DragonHeartExtTables, Probability(Grade : {}, index : {:d}) is less than 0", 
						dragon_soul_names_[i].c_str(), ast_grade_name[j].c_str(), k);
					return false;
				}
			}
		}
	}

	return true;
}

bool dragon_soul_table::check_dragon_soul_ext_tables()
{
	if (nullptr == dragon_soul_ext_table_node_)
	{
		metin2::log::error("dragon_soul_table.txt need DragonSoulExtTables.");
		return false;
	}
	for (size_t i = 0; i < dragon_soul_types_.size(); i++)
	{
		for (int j = 0; j < DRAGON_SOUL_GRADE_MAX; j++)
		{
			float prob;
			uint32_t by_product;
			if (!dragon_soul_ext_values(dragon_soul_types_[i], j, prob, by_product))
			{
				metin2::log::error("In {} group of DragonSoulExtTables, Grade({}) row is invalid.", 
					dragon_soul_names_[i].c_str(), ast_grade_name[j].c_str());
				return false;
			}
			if (prob < 0.f)
			{
				metin2::log::error("In {} group of DragonSoulExtTables, Probability(Grade : {}) is less than 0", 
					dragon_soul_names_[i].c_str(), ast_grade_name[j].c_str());
				return false;
			}
			if (0 != by_product && nullptr == item_manager::instance().table(by_product))
			{
				metin2::log::error("In {} group of DragonSoulExtTables, ByProduct({:d}) of Grade {} is not exist.", 
					dragon_soul_names_[i].c_str(), by_product, ast_grade_name[j].c_str());
				return false;
			}
		}
	}
	return true;
}

bool dragon_soul_table::basic_applys(uint8_t ds_type, vec_applys& vec_basic_applys)
{
	map_apply_group::iterator it = map_basic_applys_group_.find(ds_type);
	if (map_basic_applys_group_.end() == it)
	{
		return false;
	}
	vec_basic_applys = it->second;
	return true;
}

bool dragon_soul_table::additional_applys(uint8_t ds_type, vec_applys& vec_additional_applys)
{
	map_apply_group::iterator it = map_additional_applys_group_.find(ds_type);
	if (map_additional_applys_group_.end() == it)
	{
		return false;
	}
	vec_additional_applys = it->second;
	return true;
}

bool dragon_soul_table::apply_num_settings(uint8_t ds_type, uint8_t grade_idx, int& basis, int& add_min, int& add_max)
{
	if (grade_idx >= DRAGON_SOUL_GRADE_MAX)
	{
		metin2::log::error("Invalid dragon soul grade_idx({:d}).", grade_idx);
		return false;
	}

	std::string stValue;
	std::string stDragonSoulName;
	if (!dragon_soul_group_name(ds_type, stDragonSoulName))
	{
		metin2::log::error("Invalid dragon soul type({:d}).", ds_type);
		return false;
	}

	if (!apply_num_setting_node_->group_value(stDragonSoulName, "basis", ast_grade_name[grade_idx], basis))
	{
		metin2::log::error("Invalid basis value. DragonSoulGroup({}) Grade({})", stDragonSoulName.c_str(), ast_grade_name[grade_idx].c_str());
		return false;
	}

	if (!apply_num_setting_node_->group_value(stDragonSoulName, "add_min", ast_grade_name[grade_idx], add_min))
	{
		metin2::log::error("Invalid add_min value. DragonSoulGroup({}) Grade({})", stDragonSoulName.c_str(), ast_grade_name[grade_idx].c_str());
		return false;
	}

	if (!apply_num_setting_node_->group_value(stDragonSoulName, "add_max", ast_grade_name[grade_idx], add_max))
	{
		metin2::log::error("Invalid add_max value. DragonSoulGroup({}) Grade({})", stDragonSoulName.c_str(), ast_grade_name[grade_idx].c_str());
		return false;
	}

	return true;
}

bool dragon_soul_table::weight(uint8_t ds_type, uint8_t grade_idx, uint8_t step_index, uint8_t strength_idx, float& weight)
{
	if (grade_idx >= DRAGON_SOUL_GRADE_MAX || step_index >= DRAGON_SOUL_STEP_MAX || strength_idx >= DRAGON_SOUL_STRENGTH_MAX)
	{
		metin2::log::error("Invalid dragon soul grade_idx({:d}) step_index({:d}) strength_idx({:d}).", grade_idx, step_index, strength_idx);
		return false;
	}

	std::string stValue;
	std::string stDragonSoulName;
	if (!dragon_soul_group_name(ds_type, stDragonSoulName))
	{
		metin2::log::error("Invalid dragon soul type({:d}).", ds_type);
		return false;
	}

	group_node* pDragonSoulGroup = weight_table_node_->child_node(stDragonSoulName);
	if (nullptr != pDragonSoulGroup)
	{
		if (pDragonSoulGroup->group_value(ast_grade_name[grade_idx], ast_step_name[step_index], strength_idx, weight))
		{
			return true;
		}
	}
	pDragonSoulGroup = weight_table_node_->child_node("default");
	if (nullptr != pDragonSoulGroup)
	{
		if (!pDragonSoulGroup->group_value(ast_grade_name[grade_idx], ast_step_name[step_index], strength_idx, weight))
		{
			metin2::log::error("Invalid float. DragonSoulGroup({}) Grade({}) Row({}) Col({:d}))", stDragonSoulName.c_str(), ast_grade_name[grade_idx].c_str(), ast_step_name[step_index].c_str(), strength_idx);
			return false;
		}
		else
			return true;
	}
	metin2::log::error("Invalid value. DragonSoulGroup({}) Grade({}) Row({}) Col({:d}))", stDragonSoulName.c_str(), ast_grade_name[grade_idx].c_str(), ast_step_name[step_index].c_str(), strength_idx);
	return false;
}

bool dragon_soul_table::refine_grade_values(uint8_t ds_type, uint8_t grade_idx, int& need_count, int& fee, std::vector<float>& vec_probs)
{
	if (grade_idx >= DRAGON_SOUL_GRADE_MAX -1)
	{
		metin2::log::error("Invalid dragon soul grade_idx({:d}).", grade_idx);
		return false;
	}

	std::string stValue;
	std::string stDragonSoulName;
	if (!dragon_soul_group_name(ds_type, stDragonSoulName))
	{
		metin2::log::error("Invalid dragon soul type({:d}).", ds_type);
		return false;
	}

	const group_node::group_node_row * pRow;
	if (!refine_grade_table_node_->group_row(stDragonSoulName, ast_grade_name[grade_idx], &pRow))
	{
		metin2::log::error("Invalid row. DragonSoulGroup({}) Grade({})", stDragonSoulName.c_str(), ast_grade_name[grade_idx].c_str());
		return false;
	}
	
	if (!pRow->value("need_count", need_count))
	{
		metin2::log::error("Invalid value. DragonSoulGroup({}) Grade({}) Col(need_count)", stDragonSoulName.c_str(), ast_grade_name[grade_idx].c_str());
		return false;
	}

	if (!pRow->value("fee", fee))
	{
		metin2::log::error("Invalid value. DragonSoulGroup({}) Grade({}) Col(fee)", stDragonSoulName.c_str(), ast_grade_name[grade_idx].c_str());
		return false;
	}

	vec_probs.resize(DRAGON_SOUL_GRADE_MAX);
	for (int i = 0; i < DRAGON_SOUL_GRADE_MAX; i++)
	{
		if (!pRow->value(ast_grade_name[i], vec_probs[i]))
		{
			metin2::log::error("Invalid value. DragonSoulGroup({}) Grade({}) Col({})", stDragonSoulName.c_str(), ast_grade_name[grade_idx].c_str(), ast_grade_name[i].c_str());
			return false;
		}
	}

	return true;
}

bool dragon_soul_table::refine_step_values(uint8_t ds_type, uint8_t step_idx, int& need_count, int& fee, std::vector<float>& vec_probs)
{
	if (step_idx >= DRAGON_SOUL_STEP_MAX - 1)
	{
		metin2::log::error("Invalid dragon soul step_idx({:d}).", step_idx);
		return false;
	}

	std::string stValue;
	std::string stDragonSoulName;
	if (!dragon_soul_group_name(ds_type, stDragonSoulName))
	{
		metin2::log::error("Invalid dragon soul type({:d}).", ds_type);
		return false;
	}

	const group_node::group_node_row * pRow;
	if (!refine_step_table_node_->group_row(stDragonSoulName, ast_step_name[step_idx], &pRow))
	{
		metin2::log::error("Invalid row. DragonSoulGroup({}) Step({})", stDragonSoulName.c_str(), ast_step_name[step_idx].c_str());
		return false;
	}

	if (!pRow->value("need_count", need_count))
	{
		metin2::log::error("Invalid value. DragonSoulGroup({}) Step({}) Col(need_count)", stDragonSoulName.c_str(), ast_step_name[step_idx].c_str());
		return false;
	}

	if (!pRow->value("fee", fee))
	{
		metin2::log::error("Invalid value. DragonSoulGroup({}) Step({}) Col(fee)", stDragonSoulName.c_str(), ast_step_name[step_idx].c_str());
		return false;
	}

	vec_probs.resize(DRAGON_SOUL_STEP_MAX);
	for (int i = 0; i < DRAGON_SOUL_STEP_MAX; i++)
	{
		if (!pRow->value(ast_step_name[i], vec_probs[i]))
		{
			metin2::log::error("Invalid value. DragonSoulGroup({}) Step({}) Col({})", stDragonSoulName.c_str(), ast_step_name[step_idx].c_str(), ast_step_name[i].c_str());
			return false;
		}
	}

	return true;
}

bool dragon_soul_table::refine_strength_values(uint8_t ds_type, uint8_t material_type, uint8_t strength_idx, int& fee, float& prob)
{
	if (material_type < MATERIAL_DS_REFINE_NORMAL || material_type > MATERIAL_DS_REFINE_HOLLY)
	{
		metin2::log::error("Invalid dragon soul material_type({:d}).", material_type);
		return false;
	}

	std::string stValue;
	std::string stDragonSoulName;
	if (!dragon_soul_group_name(ds_type, stDragonSoulName))
	{
		metin2::log::error("Invalid dragon soul type({:d}).", ds_type);
		return false;
	}

	if (!refine_strength_table_node_->group_value(stDragonSoulName, ast_material_name[material_type], "fee", fee))
	{
		metin2::log::error("Invalid fee. DragonSoulGroup({}) Material({})", 
			stDragonSoulName.c_str(), ast_material_name[material_type].c_str());
		return false;
	}
	std::string stStrengthIdx = std::to_string(static_cast<int>(strength_idx));

	if (!refine_strength_table_node_->group_value(stDragonSoulName, ast_material_name[material_type], stStrengthIdx, prob))
	{
		metin2::log::error("Invalid prob. DragonSoulGroup({}) Material({}) Strength({:d})", 
			stDragonSoulName.c_str(), ast_material_name[material_type].c_str(), strength_idx);
		return false;
	}

	return true;
}

bool dragon_soul_table::dragon_heart_ext_values(uint8_t ds_type, uint8_t grade_idx, std::vector<float>& vec_chargings, std::vector<float>& vec_probs)
{
	if (grade_idx >= DRAGON_SOUL_GRADE_MAX)
	{
		metin2::log::error("Invalid dragon soul grade_idx({:d}).", grade_idx);
		return false;
	}

	std::string stDragonSoulName;
	if (!dragon_soul_group_name(ds_type, stDragonSoulName))
	{
		metin2::log::error("Invalid dragon soul type({:d}).", ds_type);
		return false;
	}

	const group_node::group_node_row * pRow;
	if (!dragon_heart_ext_table_node_->group_row(stDragonSoulName, "charging", &pRow))
	{
		metin2::log::error("Invalid CHARGING row. DragonSoulGroup({})", stDragonSoulName.c_str());
		return false;
	}
	int n = pRow->size();
	vec_chargings.resize(n);
	for (int i = 0; i < n; i++)
	{
		if (!pRow->value(i, vec_chargings[i]))
		{
			metin2::log::error("Invalid CHARGING value. DragonSoulGroup({}), Col({:d})", stDragonSoulName.c_str(), i);
			return false;
		}
	}

	if (!dragon_heart_ext_table_node_->group_row(stDragonSoulName, ast_grade_name[grade_idx], &pRow))
	{
		metin2::log::error("Invalid row. DragonSoulGroup({}) Grade({})", stDragonSoulName.c_str(), ast_grade_name[grade_idx].c_str());
		return false;
	}

	int m = pRow->size();
	if (n != m)
	{
		metin2::log::error("Invalid row size({:d}). It must be same CHARGING row size({:d}). DragonSoulGroup({}) Grade({})", m, n, 
				stDragonSoulName.c_str(), ast_grade_name[grade_idx].c_str());
		return false;
	}
	vec_probs.resize(m);
	for (int i = 0; i < m; i++)
	{
		if (!pRow->value(i, vec_probs[i]))
		{
			metin2::log::error("Invalid value. DragonSoulGroup({}), Grade({}) Col({:d})", stDragonSoulName.c_str(), ast_grade_name[grade_idx].c_str(), i);
			return false;
		}
	}

	return true;
}

bool dragon_soul_table::dragon_soul_ext_values(uint8_t ds_type, uint8_t grade_idx, float& prob, uint32_t& by_product)
{
	if (grade_idx >= DRAGON_SOUL_GRADE_MAX)
	{
		metin2::log::error("Invalid dragon soul grade_idx({:d}).", grade_idx);
		return false;
	}

	std::string stValue;
	std::string stDragonSoulName;
	if (!dragon_soul_group_name(ds_type, stDragonSoulName))
	{
		metin2::log::error("Invalid dragon soul type({:d}).", ds_type);
		return false;
	}

	if (!dragon_soul_ext_table_node_->group_value(stDragonSoulName, ast_grade_name[grade_idx], "prob", prob))
	{
		metin2::log::error("Invalid Prob. DragonSoulGroup({}) Grade({})",
			stDragonSoulName.c_str(), ast_grade_name[grade_idx].c_str());
		return false;
	}

	if (!dragon_soul_ext_table_node_->group_value(stDragonSoulName, ast_grade_name[grade_idx], "byproduct", by_product))
	{
		metin2::log::error("Invalid fee. DragonSoulGroup({}) Grade({}) index({:d})",
			stDragonSoulName.c_str(), ast_grade_name[grade_idx].c_str(), grade_idx);
		return false;
	}

	return true;
}

dragon_soul_table::dragon_soul_table()
{
}
dragon_soul_table::~dragon_soul_table ()
{
}
}
