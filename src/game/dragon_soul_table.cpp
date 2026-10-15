#include "dragon_soul_table.h"

#include "constants.h"
#include "group_text_parse_tree.h"
#include "item_manager.h"

#include <common/length.h>
#include <metin2/log/log.h>


#include <cstdint>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <sys/types.h>

namespace metin2::game {

const std::string g_astGradeName[] =
{
	"grade_normal",
	"grade_brilliant",
	"grade_rare",
	"grade_ancient",
	"grade_legendary",
};

const std::string g_astStepName[] =
{
	"step_lowest",
	"step_low",
	"step_mid",
	"step_high",
	"step_highest",
};

const std::string g_astMaterialName[] = 
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

bool DragonSoulTable::ReadDragonSoulTableFile(const char * c_pszFileName)
{
	m_pLoader = std::make_unique<CGroupTextParseTreeLoader>();
	CGroupTextParseTreeLoader& loader = *m_pLoader;

	if (false == loader.Load(c_pszFileName))
	{
		metin2::log::error("dragon_soul_table.txt load error");
		return false;
	}

	// Group VnumMapper Reading.
	if (!ReadVnumMapper())
		return false;

	// Group BasicApplys Reading.
	if (!ReadBasicApplys())
		return false;

	// Group AdditionalApplys Reading.
	if (!ReadAdditionalApplys())
		return false;

	m_pApplyNumSettingNode = loader.GetGroup("applynumsettings");
	m_pWeightTableNode = loader.GetGroup("weighttables");
	m_pRefineGradeTableNode = loader.GetGroup("refinegradetables");
	m_pRefineStepTableNode = loader.GetGroup("refinesteptables");
	m_pRefineStrengthTableNode = loader.GetGroup("refinestrengthtables");
	m_pDragonHeartExtTableNode = loader.GetGroup("dragonheartexttables");
	m_pDragonSoulExtTableNode = loader.GetGroup("dragonsoulexttables");

	if (CheckApplyNumSettings()
		&& CheckWeightTables()
		&& CheckRefineGradeTables()
		&& CheckRefineStepTables()
		&& CheckRefineStrengthTables()
		&& CheckDragonHeartExtTables()
		&& CheckDragonSoulExtTables())
	{
		return true;
	}
	else
	{
		metin2::log::error("DragonSoul table Check failed.");
		return false;
	}
}

bool DragonSoulTable::GetDragonSoulGroupName(uint8_t bType, std::string& stGroupName) const
{
	DragonSoulTable::TMapTypeToName::const_iterator it = m_map_type_to_name.find (bType);
	if (it != m_map_type_to_name.end())
	{
		stGroupName = it->second;
		return true;
	}
	else
	{
		metin2::log::error("Invalid DragonSoul Type({:d})", bType);
		return false;
	}
}

bool DragonSoulTable::ReadVnumMapper()
{
	std::string stName;

	// Group VnumMapper Reading.
	CGroupNode* pGroupNode = m_pLoader->GetGroup("vnummapper");

	if (nullptr == pGroupNode)
	{
		metin2::log::error("dragon_soul_table.txt need VnumMapper.");
		return false;
	}
	{
		int n = pGroupNode->GetRowCount();
		if (0 == n)
		{
			metin2::log::error("Group VnumMapper is Empty.");
			return false;
		}

		std::set <uint8_t> setTypes;

		for (int i = 0; i < n; i++)
		{
			const CGroupNode::CGroupNodeRow* pRow;
			pGroupNode->GetRow(i, &pRow);
			
			std::string stDragonSoulName;
			uint8_t bType;
			if (!pRow->GetValue("dragonsoulname", stDragonSoulName))
			{
				metin2::log::error("In Group VnumMapper, No DragonSoulName column.");
				return false;
			}
			if (!pRow->GetValue("type", bType))
			{
				metin2::log::error("In Group VnumMapper, {}'s Vnum is invalid", stDragonSoulName.c_str());
				return false;
			}

			if (setTypes.end() != setTypes.find(bType))
			{
				metin2::log::error("In Group VnumMapper, duplicated vnum exist.");
				return false;
			}
			m_map_name_to_type.insert(TMapNameToType::value_type(stDragonSoulName, bType));
			m_map_type_to_name.insert(TMapTypeToName::value_type(bType, stDragonSoulName));
			m_vecDragonSoulTypes.push_back(bType);
			m_vecDragonSoulNames.push_back(stDragonSoulName);
		}
	}
	return true;
}

bool DragonSoulTable::ReadBasicApplys()
{
	CGroupNode* pGroupNode = m_pLoader->GetGroup("basicapplys");

	if (nullptr == pGroupNode)
	{
		metin2::log::error("dragon_soul_table.txt need BasicApplys.");
		return false;
	}

	for (uint i = 0; i < m_vecDragonSoulNames.size(); i++)
	{
		CGroupNode* pChild;
		if (nullptr == (pChild = pGroupNode->GetChildNode(m_vecDragonSoulNames[i])))
		{
			metin2::log::error("In Group BasicApplys, {} group is not defined.", m_vecDragonSoulNames[i].c_str());
			return false;
		}
		TVecApplys vecApplys;
		int n = pChild->GetRowCount();
		
		// BasicApply Group keys start at 1.
		for (int j = 1; j <= n; j++)
		{
			std::stringstream ss;
			ss << j;
			const CGroupNode::CGroupNodeRow* pRow = nullptr;

			pChild->GetRow(ss.str(), &pRow);
			if (nullptr == pRow)
			{
				metin2::log::error("In Group BasicApplys, No {:d} row.", j);
			}
			EApplyTypes at;
			int av;

			std::string stTypeName;
			if (!pRow->GetValue("apply_type", stTypeName))
			{
				metin2::log::error("In Group BasicApplys, {} group's apply_type is empty.", m_vecDragonSoulNames[i].c_str());
				return false;
			}
			if (!(at = (EApplyTypes)FN_get_apply_type(stTypeName.c_str())))
			{
				metin2::log::error("In Group BasicApplys, {} group's apply_type {} is invalid.", m_vecDragonSoulNames[i].c_str(), stTypeName.c_str());
				return false;
			}
			if (!pRow->GetValue("apply_value", av))
			{
				metin2::log::error("In Group BasicApplys, {} group's apply_value {:d} is invalid.", m_vecDragonSoulNames[i].c_str(), av);
				return false;
			}
			vecApplys.push_back(SApply(at, av));
		}
		m_map_basic_applys_group.insert (TMapApplyGroup::value_type (m_map_name_to_type[m_vecDragonSoulNames[i]], vecApplys));
	}

	return true;
}

bool DragonSoulTable::ReadAdditionalApplys()
{
	CGroupNode* pGroupNode = m_pLoader->GetGroup("additionalapplys");
	if (nullptr == pGroupNode)
	{
		metin2::log::error("dragon_soul_table.txt need AdditionalApplys.");
		return false;
	}

	for (size_t i = 0; i < m_vecDragonSoulNames.size(); i++)
	{
		CGroupNode* pChild;
		if (nullptr == (pChild = pGroupNode->GetChildNode(m_vecDragonSoulNames[i])))
		{
			metin2::log::error("In Group AdditionalApplys, {} group is not defined.", m_vecDragonSoulNames[i].c_str());
			return false;
		}
		TVecApplys vecApplys;
		
		int n = pChild->GetRowCount();

		for (int j = 0; j < n; j++)
		{
			const CGroupNode::CGroupNodeRow* pRow;

			pChild->GetRow(j, &pRow);

			EApplyTypes at;
			int av;
			float prob;
			std::string stTypeName;
			if (!pRow->GetValue("apply_type", stTypeName))
			{
				metin2::log::error("In Group AdditionalApplys, {} group's apply_type is empty.", m_vecDragonSoulNames[i].c_str());
				return false;
			}
			if (!(at = (EApplyTypes)FN_get_apply_type(stTypeName.c_str())))
			{
				metin2::log::error("In Group AdditionalApplys, {} group's apply_type {} is invalid.", m_vecDragonSoulNames[i].c_str(), stTypeName.c_str());
				return false;
			}
			if (!pRow->GetValue("apply_value", av))
			{
				metin2::log::error("In Group AdditionalApplys, {} group's apply_value {:d} is invalid.", m_vecDragonSoulNames[i].c_str(), av);
				return false;
			}
			if (!pRow->GetValue("prob", prob))
			{
				metin2::log::error("In Group AdditionalApplys, {} group's probability {:g} is invalid.", m_vecDragonSoulNames[i].c_str(), prob);
				return false;
			}
			vecApplys.push_back(SApply(at, av, prob));
		}
		m_map_additional_applys_group.insert (TMapApplyGroup::value_type (m_map_name_to_type[m_vecDragonSoulNames[i]], vecApplys));
	}

	return true;
}

bool DragonSoulTable::CheckApplyNumSettings ()
{
	// Group ApplyNumSettings Reading.
	if (nullptr == m_pApplyNumSettingNode)
	{
		metin2::log::error("dragon_soul_table.txt need ApplyNumSettings.");
		return false;
	}
	else
	{
		for (size_t i = 0; i < m_vecDragonSoulTypes.size(); i++)
		{
			for (int j = 0; j < DRAGON_SOUL_GRADE_MAX; j++)
			{
				int basis, add_min, add_max;
				if (!GetApplyNumSettings(m_vecDragonSoulTypes[i], j, basis, add_min, add_max))
				{
					metin2::log::error("In {} group of ApplyNumSettings, values in Grade({}) row is invalid.", 
						m_vecDragonSoulNames[i].c_str(), g_astGradeName[j].c_str());
					return false;
				}
			}
		}
	}

	return true;
}

bool DragonSoulTable::CheckWeightTables ()
{
	// Group WeightTables Reading.
	if (nullptr == m_pWeightTableNode)
	{
		metin2::log::error("dragon_soul_table.txt need WeightTables.");
		return false;
	}
	else
	{
		for (size_t i = 0; i < m_vecDragonSoulTypes.size(); i++)
		{
			for (int j = 0; j < DRAGON_SOUL_GRADE_MAX; j++)
			{
				for (int k = 0; k < DRAGON_SOUL_STEP_MAX; k++)
				{
					for (int l = 0; l < DRAGON_SOUL_STRENGTH_MAX; l++)
					{
						float fWeight;
						if (!GetWeight(m_vecDragonSoulTypes[i], j, k, l, fWeight))
						{
							metin2::log::error("In {} group of WeightTables, value(Grade({}), Step({}), Strength({:d}) is invalid.", 
								m_vecDragonSoulNames[i].c_str(), g_astGradeName[j].c_str(), g_astStepName[k].c_str(), l);
						}
					}
				}
			}
		}
	}
	return true;
}

bool DragonSoulTable::CheckRefineGradeTables()
{
	// Group UpgradeTables Reading.
	if (nullptr == m_pRefineGradeTableNode)
	{
		metin2::log::error("dragon_soul_table.txt need RefineGradeTables.");
		return false;
	}
	else
	{
		for (size_t i = 0; i < m_vecDragonSoulTypes.size(); i++)
		{
			for (int j = 0; j < DRAGON_SOUL_GRADE_MAX - 1; j++)
			{
				int need_count, fee;
				std::vector <float> vec_probs;
				if (!GetRefineGradeValues(m_vecDragonSoulTypes[i], j, need_count, fee, vec_probs))
				{
					metin2::log::error("In {} group of RefineGradeTables, values in Grade({}) row is invalid.", 
						m_vecDragonSoulNames[i].c_str(), g_astGradeName[j].c_str());
					return false;
				}
				if (need_count < 1)
				{
					metin2::log::error("In {} group of RefineGradeTables, need_count of Grade({}) is less than 1.", 
						m_vecDragonSoulNames[i].c_str(), g_astGradeName[j].c_str());
					return false;
				}
				if (fee < 0)
				{
					metin2::log::error("In {} group of RefineGradeTables, fee of Grade({}) is less than 0.", 
						m_vecDragonSoulNames[i].c_str(), g_astGradeName[j].c_str());
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

bool DragonSoulTable::CheckRefineStepTables ()
{
	// Group ImproveTables Reading.
	if (nullptr == m_pRefineStrengthTableNode)
	{
		metin2::log::error("dragon_soul_table.txt need RefineStepTables.");
		return false;
	}
	else
	{
		for (size_t i = 0; i < m_vecDragonSoulTypes.size(); i++)
		{
			for (int j = 0; j < DRAGON_SOUL_STEP_MAX - 1; j++)
			{
				int need_count, fee;
				std::vector <float> vec_probs;
				if (!GetRefineStepValues(m_vecDragonSoulTypes[i], j, need_count, fee, vec_probs))
				{
					metin2::log::error("In {} group of RefineStepTables, values in Step({}) row is invalid.", 
						m_vecDragonSoulNames[i].c_str(), g_astStepName[j].c_str());
					return false;
				}
				if (need_count < 1)
				{
					metin2::log::error("In {} group of RefineStepTables, need_count of Step({}) is less than 1.", 
						m_vecDragonSoulNames[i].c_str(), g_astStepName[j].c_str());
					return false;
				}
				if (fee < 0)
				{
					metin2::log::error("In {} group of RefineStepTables, fee of Step({}) is less than 0.", 
						m_vecDragonSoulNames[i].c_str(), g_astStepName[j].c_str());
					return false;
				}
				if (DRAGON_SOUL_GRADE_MAX != vec_probs.size())
				{
					metin2::log::error("In {} group of RefineStepTables, probability list size is not {:d}.", 
						m_vecDragonSoulNames[i].c_str(), DRAGON_SOUL_GRADE_MAX);
					return false;
				}
				for (size_t k = 0; k < vec_probs.size(); k++)
				{
					if (vec_probs[k] < 0.f)
					{
						metin2::log::error("In {} group of RefineStepTables, probability(index : {:d}) is less than 0.", 
							m_vecDragonSoulNames[i].c_str(), k);
						return false;
					}
				}
			}
		}
	}

	return true;
}


bool DragonSoulTable::CheckRefineStrengthTables()
{
	CGroupNode* pGroupNode = m_pRefineStrengthTableNode;
	// Group RefineTables Reading.
	if (nullptr == pGroupNode)
	{
		metin2::log::error("dragon_soul_table.txt need RefineStrengthTables.");
		return false;
	}
	for (size_t i = 0; i < m_vecDragonSoulTypes.size(); i++)
	{
		for (int j = MATERIAL_DS_REFINE_NORMAL; j <= MATERIAL_DS_REFINE_HOLLY; j++)
		{
			int fee;
			float prob;
			for (int k = 0; k < DRAGON_SOUL_STRENGTH_MAX -1; k++)
			{
				if (!GetRefineStrengthValues(m_vecDragonSoulTypes[i], j, k, fee, prob))
				{
					metin2::log::error("In {} group of RefineStrengthTables, value(Material({}), Strength({:d})) or fee are invalid.", 
						m_vecDragonSoulNames[i].c_str(), g_astMaterialName[j].c_str(), k);
					return false;
				}
				if (fee < 0)
				{
					metin2::log::error("In {} group of RefineStrengthTables, fee of Material({}) is less than 0.", 
						m_vecDragonSoulNames[i].c_str(), g_astMaterialName[j].c_str());
					return false;
				}
				if (prob < 0.f)
				{
					metin2::log::error("In {} group of RefineStrengthTables, probability(Material({}), Strength({:d})) is less than 0.", 
						m_vecDragonSoulNames[i].c_str(), g_astMaterialName[j].c_str(), k);
					return false;
				}
			}
		}
	}

	return true;
}

bool DragonSoulTable::CheckDragonHeartExtTables()
{
	// Group DragonHeartExtTables Reading.
	if (nullptr == m_pDragonHeartExtTableNode)
	{
		metin2::log::error("dragon_soul_table.txt need DragonHeartExtTables.");
		return false;
	}
	for (size_t i = 0; i < m_vecDragonSoulTypes.size(); i++)
	{
		for (int j = 0; j < DRAGON_SOUL_GRADE_MAX; j++)
		{
			std::vector <float> vec_chargings;
			std::vector <float> vec_probs;

			if (!GetDragonHeartExtValues(m_vecDragonSoulTypes[i], j, vec_chargings, vec_probs))
			{
				metin2::log::error("In {} group of DragonHeartExtTables, CHARGING row or Grade({}) row are invalid.", 
					m_vecDragonSoulNames[i].c_str(), g_astGradeName[j].c_str());
				return false;
			}
			if (vec_chargings.size() != vec_probs.size())
			{
				metin2::log::error("In {} group of DragonHeartExtTables, CHARGING row size({:d}) are not equal probability row size({:d}).",
					m_vecDragonSoulNames[i].c_str(), vec_chargings.size(), vec_probs.size());
				return false;
			}
			for (size_t k = 0; k < vec_chargings.size(); k++)
			{
				if (vec_chargings[k] < 0.f)
				{
					metin2::log::error("In {} group of DragonHeartExtTables, CHARGING value(index : {:d}) is less than 0", 
						m_vecDragonSoulNames[i].c_str(), k);
					return false;
				}
			}
			for (size_t k = 0; k < vec_probs.size(); k++)
			{
				if (vec_probs[k] < 0.f)
				{
					metin2::log::error("In {} group of DragonHeartExtTables, Probability(Grade : {}, index : {:d}) is less than 0", 
						m_vecDragonSoulNames[i].c_str(), g_astGradeName[j].c_str(), k);
					return false;
				}
			}
		}
	}

	return true;
}

bool DragonSoulTable::CheckDragonSoulExtTables()
{
	// Group DragonSoulExtTables Reading.
	if (nullptr == m_pDragonSoulExtTableNode)
	{
		metin2::log::error("dragon_soul_table.txt need DragonSoulExtTables.");
		return false;
	}
	for (size_t i = 0; i < m_vecDragonSoulTypes.size(); i++)
	{
		for (int j = 0; j < DRAGON_SOUL_GRADE_MAX; j++)
		{
			float prob;
			uint32_t by_product;
			if (!GetDragonSoulExtValues(m_vecDragonSoulTypes[i], j, prob, by_product))
			{
				metin2::log::error("In {} group of DragonSoulExtTables, Grade({}) row is invalid.", 
					m_vecDragonSoulNames[i].c_str(), g_astGradeName[j].c_str());
				return false;
			}
			if (prob < 0.f)
			{
				metin2::log::error("In {} group of DragonSoulExtTables, Probability(Grade : {}) is less than 0", 
					m_vecDragonSoulNames[i].c_str(), g_astGradeName[j].c_str());
				return false;
			}
			if (0 != by_product && nullptr == ITEM_MANAGER::instance().GetTable(by_product))
			{
				metin2::log::error("In {} group of DragonSoulExtTables, ByProduct({:d}) of Grade {} is not exist.", 
					m_vecDragonSoulNames[i].c_str(), by_product, g_astGradeName[j].c_str());
				return false;
			}
		}
	}
	return true;
}

bool DragonSoulTable::GetBasicApplys(uint8_t ds_type, TVecApplys& vec_basic_applys)
{
	TMapApplyGroup::iterator it = m_map_basic_applys_group.find(ds_type);
	if (m_map_basic_applys_group.end() == it)
	{
		return false;
	}
	vec_basic_applys = it->second;
	return true;
}

bool DragonSoulTable::GetAdditionalApplys(uint8_t ds_type, TVecApplys& vec_additional_applys)
{
	TMapApplyGroup::iterator it = m_map_additional_applys_group.find(ds_type);
	if (m_map_additional_applys_group.end() == it)
	{
		return false;
	}
	vec_additional_applys = it->second;
	return true;
}

bool DragonSoulTable::GetApplyNumSettings(uint8_t ds_type, uint8_t grade_idx, int& basis, int& add_min, int& add_max)
{
	if (grade_idx >= DRAGON_SOUL_GRADE_MAX)
	{
		metin2::log::error("Invalid dragon soul grade_idx({:d}).", grade_idx);
		return false;
	}

	std::string stValue;
	std::string stDragonSoulName;
	if (!GetDragonSoulGroupName(ds_type, stDragonSoulName))
	{
		metin2::log::error("Invalid dragon soul type({:d}).", ds_type);
		return false;
	}

	if (!m_pApplyNumSettingNode->GetGroupValue(stDragonSoulName, "basis", g_astGradeName[grade_idx], basis))
	{
		metin2::log::error("Invalid basis value. DragonSoulGroup({}) Grade({})", stDragonSoulName.c_str(), g_astGradeName[grade_idx].c_str());
		return false;
	}

	if (!m_pApplyNumSettingNode->GetGroupValue(stDragonSoulName, "add_min", g_astGradeName[grade_idx], add_min))
	{
		metin2::log::error("Invalid add_min value. DragonSoulGroup({}) Grade({})", stDragonSoulName.c_str(), g_astGradeName[grade_idx].c_str());
		return false;
	}

	if (!m_pApplyNumSettingNode->GetGroupValue(stDragonSoulName, "add_max", g_astGradeName[grade_idx], add_max))
	{
		metin2::log::error("Invalid add_max value. DragonSoulGroup({}) Grade({})", stDragonSoulName.c_str(), g_astGradeName[grade_idx].c_str());
		return false;
	}

	return true;
}

bool DragonSoulTable::GetWeight(uint8_t ds_type, uint8_t grade_idx, uint8_t step_index, uint8_t strength_idx, float& fWeight)
{
	if (grade_idx >= DRAGON_SOUL_GRADE_MAX || step_index >= DRAGON_SOUL_STEP_MAX || strength_idx >= DRAGON_SOUL_STRENGTH_MAX)
	{
		metin2::log::error("Invalid dragon soul grade_idx({:d}) step_index({:d}) strength_idx({:d}).", grade_idx, step_index, strength_idx);
		return false;
	}

	std::string stValue;
	std::string stDragonSoulName;
	if (!GetDragonSoulGroupName(ds_type, stDragonSoulName))
	{
		metin2::log::error("Invalid dragon soul type({:d}).", ds_type);
		return false;
	}

	CGroupNode* pDragonSoulGroup = m_pWeightTableNode->GetChildNode(stDragonSoulName);
	if (nullptr != pDragonSoulGroup)
	{
		if (pDragonSoulGroup->GetGroupValue(g_astGradeName[grade_idx], g_astStepName[step_index], strength_idx, fWeight))
		{
			return true;
		}
	}
	// Look at the default group.
	pDragonSoulGroup = m_pWeightTableNode->GetChildNode("default");
	if (nullptr != pDragonSoulGroup)
	{
		if (!pDragonSoulGroup->GetGroupValue(g_astGradeName[grade_idx], g_astStepName[step_index], strength_idx, fWeight))
		{
			metin2::log::error("Invalid float. DragonSoulGroup({}) Grade({}) Row({}) Col({:d}))", stDragonSoulName.c_str(), g_astGradeName[grade_idx].c_str(), g_astStepName[step_index].c_str(), strength_idx);
			return false;
		}
		else
			return true;
	}
	metin2::log::error("Invalid value. DragonSoulGroup({}) Grade({}) Row({}) Col({:d}))", stDragonSoulName.c_str(), g_astGradeName[grade_idx].c_str(), g_astStepName[step_index].c_str(), strength_idx);
	return false;
}

bool DragonSoulTable::GetRefineGradeValues(uint8_t ds_type, uint8_t grade_idx, int& need_count, int& fee, std::vector<float>& vec_probs)
{
	if (grade_idx >= DRAGON_SOUL_GRADE_MAX -1)
	{
		metin2::log::error("Invalid dragon soul grade_idx({:d}).", grade_idx);
		return false;
	}

	std::string stValue;
	std::string stDragonSoulName;
	if (!GetDragonSoulGroupName(ds_type, stDragonSoulName))
	{
		metin2::log::error("Invalid dragon soul type({:d}).", ds_type);
		return false;
	}

	const CGroupNode::CGroupNodeRow * pRow;
	if (!m_pRefineGradeTableNode->GetGroupRow(stDragonSoulName, g_astGradeName[grade_idx], &pRow))
	{
		metin2::log::error("Invalid row. DragonSoulGroup({}) Grade({})", stDragonSoulName.c_str(), g_astGradeName[grade_idx].c_str());
		return false;
	}
	
	if (!pRow->GetValue("need_count", need_count))
	{
		metin2::log::error("Invalid value. DragonSoulGroup({}) Grade({}) Col(need_count)", stDragonSoulName.c_str(), g_astGradeName[grade_idx].c_str());
		return false;
	}

	if (!pRow->GetValue("fee", fee))
	{
		metin2::log::error("Invalid value. DragonSoulGroup({}) Grade({}) Col(fee)", stDragonSoulName.c_str(), g_astGradeName[grade_idx].c_str());
		return false;
	}

	vec_probs.resize(DRAGON_SOUL_GRADE_MAX);
	for (int i = 0; i < DRAGON_SOUL_GRADE_MAX; i++)
	{
		if (!pRow->GetValue(g_astGradeName[i], vec_probs[i]))
		{
			metin2::log::error("Invalid value. DragonSoulGroup({}) Grade({}) Col({})", stDragonSoulName.c_str(), g_astGradeName[grade_idx].c_str(), g_astGradeName[i].c_str());
			return false;
		}
	}

	return true;
}

bool DragonSoulTable::GetRefineStepValues(uint8_t ds_type, uint8_t step_idx, int& need_count, int& fee, std::vector<float>& vec_probs)
{
	if (step_idx >= DRAGON_SOUL_STEP_MAX - 1)
	{
		metin2::log::error("Invalid dragon soul step_idx({:d}).", step_idx);
		return false;
	}

	std::string stValue;
	std::string stDragonSoulName;
	if (!GetDragonSoulGroupName(ds_type, stDragonSoulName))
	{
		metin2::log::error("Invalid dragon soul type({:d}).", ds_type);
		return false;
	}

	const CGroupNode::CGroupNodeRow * pRow;
	if (!m_pRefineStepTableNode->GetGroupRow(stDragonSoulName, g_astStepName[step_idx], &pRow))
	{
		metin2::log::error("Invalid row. DragonSoulGroup({}) Step({})", stDragonSoulName.c_str(), g_astStepName[step_idx].c_str());
		return false;
	}

	if (!pRow->GetValue("need_count", need_count))
	{
		metin2::log::error("Invalid value. DragonSoulGroup({}) Step({}) Col(need_count)", stDragonSoulName.c_str(), g_astStepName[step_idx].c_str());
		return false;
	}

	if (!pRow->GetValue("fee", fee))
	{
		metin2::log::error("Invalid value. DragonSoulGroup({}) Step({}) Col(fee)", stDragonSoulName.c_str(), g_astStepName[step_idx].c_str());
		return false;
	}

	vec_probs.resize(DRAGON_SOUL_STEP_MAX);
	for (int i = 0; i < DRAGON_SOUL_STEP_MAX; i++)
	{
		if (!pRow->GetValue(g_astStepName[i], vec_probs[i]))
		{
			metin2::log::error("Invalid value. DragonSoulGroup({}) Step({}) Col({})", stDragonSoulName.c_str(), g_astStepName[step_idx].c_str(), g_astStepName[i].c_str());
			return false;
		}
	}

	return true;
}

bool DragonSoulTable::GetRefineStrengthValues(uint8_t ds_type, uint8_t material_type, uint8_t strength_idx, int& fee, float& prob)
{
	if (material_type < MATERIAL_DS_REFINE_NORMAL || material_type > MATERIAL_DS_REFINE_HOLLY)
	{
		metin2::log::error("Invalid dragon soul material_type({:d}).", material_type);
		return false;
	}

	std::string stValue;
	std::string stDragonSoulName;
	if (!GetDragonSoulGroupName(ds_type, stDragonSoulName))
	{
		metin2::log::error("Invalid dragon soul type({:d}).", ds_type);
		return false;
	}

	if (!m_pRefineStrengthTableNode->GetGroupValue(stDragonSoulName, g_astMaterialName[material_type], "fee", fee))
	{
		metin2::log::error("Invalid fee. DragonSoulGroup({}) Material({})", 
			stDragonSoulName.c_str(), g_astMaterialName[material_type].c_str());
		return false;
	}
	std::string stStrengthIdx = std::to_string(static_cast<int>(strength_idx));

	if (!m_pRefineStrengthTableNode->GetGroupValue(stDragonSoulName, g_astMaterialName[material_type], stStrengthIdx, prob))
	{
		metin2::log::error("Invalid prob. DragonSoulGroup({}) Material({}) Strength({:d})", 
			stDragonSoulName.c_str(), g_astMaterialName[material_type].c_str(), strength_idx);
		return false;
	}

	return true;
}

bool DragonSoulTable::GetDragonHeartExtValues(uint8_t ds_type, uint8_t grade_idx, std::vector<float>& vec_chargings, std::vector<float>& vec_probs)
{
	if (grade_idx >= DRAGON_SOUL_GRADE_MAX)
	{
		metin2::log::error("Invalid dragon soul grade_idx({:d}).", grade_idx);
		return false;
	}

	std::string stDragonSoulName;
	if (!GetDragonSoulGroupName(ds_type, stDragonSoulName))
	{
		metin2::log::error("Invalid dragon soul type({:d}).", ds_type);
		return false;
	}

	const CGroupNode::CGroupNodeRow * pRow;
	if (!m_pDragonHeartExtTableNode->GetGroupRow(stDragonSoulName, "charging", &pRow))
	{
		metin2::log::error("Invalid CHARGING row. DragonSoulGroup({})", stDragonSoulName.c_str());
		return false;
	}
	int n = pRow->GetSize();
	vec_chargings.resize(n);
	for (int i = 0; i < n; i++)
	{
		if (!pRow->GetValue(i, vec_chargings[i]))
		{
			metin2::log::error("Invalid CHARGING value. DragonSoulGroup({}), Col({:d})", stDragonSoulName.c_str(), i);
			return false;
		}
	}

	if (!m_pDragonHeartExtTableNode->GetGroupRow(stDragonSoulName, g_astGradeName[grade_idx], &pRow))
	{
		metin2::log::error("Invalid row. DragonSoulGroup({}) Grade({})", stDragonSoulName.c_str(), g_astGradeName[grade_idx].c_str());
		return false;
	}

	int m = pRow->GetSize();
	if (n != m)
	{
		metin2::log::error("Invalid row size({:d}). It must be same CHARGING row size({:d}). DragonSoulGroup({}) Grade({})", m, n, 
				stDragonSoulName.c_str(), g_astGradeName[grade_idx].c_str());
		return false;
	}
	vec_probs.resize(m);
	for (int i = 0; i < m; i++)
	{
		if (!pRow->GetValue(i, vec_probs[i]))
		{
			metin2::log::error("Invalid value. DragonSoulGroup({}), Grade({}) Col({:d})", stDragonSoulName.c_str(), g_astGradeName[grade_idx].c_str(), i);
			return false;
		}
	}

	return true;
}

bool DragonSoulTable::GetDragonSoulExtValues(uint8_t ds_type, uint8_t grade_idx, float& prob, uint32_t& by_product)
{
	if (grade_idx >= DRAGON_SOUL_GRADE_MAX)
	{
		metin2::log::error("Invalid dragon soul grade_idx({:d}).", grade_idx);
		return false;
	}

	std::string stValue;
	std::string stDragonSoulName;
	if (!GetDragonSoulGroupName(ds_type, stDragonSoulName))
	{
		metin2::log::error("Invalid dragon soul type({:d}).", ds_type);
		return false;
	}

	if (!m_pDragonSoulExtTableNode->GetGroupValue(stDragonSoulName, g_astGradeName[grade_idx], "prob", prob))
	{
		metin2::log::error("Invalid Prob. DragonSoulGroup({}) Grade({})",
			stDragonSoulName.c_str(), g_astGradeName[grade_idx].c_str());
		return false;
	}

	if (!m_pDragonSoulExtTableNode->GetGroupValue(stDragonSoulName, g_astGradeName[grade_idx], "byproduct", by_product))
	{
		metin2::log::error("Invalid fee. DragonSoulGroup({}) Grade({}) index({:d})",
			stDragonSoulName.c_str(), g_astGradeName[grade_idx].c_str(), grade_idx);
		return false;
	}

	return true;
}

DragonSoulTable::DragonSoulTable()
{
}
DragonSoulTable::~DragonSoulTable ()
{
}
}
