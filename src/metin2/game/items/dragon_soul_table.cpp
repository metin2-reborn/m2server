#include <metin2/game/items/dragon_soul_table.h>

#include <metin2/game/app/constants.h>
#include <metin2/game/data/group_text_parse_tree.h>
#include <metin2/game/items/item_manager.h>

#include <metin2/log/log.h>
#include <metin2/proto/apply_types.h>
#include <metin2/proto/item_types.h>

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

    auto dragon_soul_table::read_dragon_soul_table_file(const char *file_name) -> bool {
        loader_ = std::make_unique<group_text_parse_tree_loader>();
        group_text_parse_tree_loader &loader = *loader_;

        if (false == loader.load(file_name)) {
            metin2::log::error("Cannot load {}", file_name);
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

        if (check_apply_num_settings() && check_weight_tables() && check_refine_grade_tables() && check_refine_step_tables() && check_refine_strength_tables() && check_dragon_heart_ext_tables() && check_dragon_soul_ext_tables()) {
            return true;
        } else {
            metin2::log::error("Dragon soul table check failed");
            return false;
        }
    }

    auto dragon_soul_table::dragon_soul_group_name(uint8_t type, std::string &group_name) const -> bool {
        dragon_soul_table::map_type_to_name::const_iterator it = map_type_to_name_.find(type);
        if (it != map_type_to_name_.end()) {
            group_name = it->second;
            return true;
        } else {
            metin2::log::error("Unknown dragon soul type {:d}", type);
            return false;
        }
    }

    auto dragon_soul_table::read_vnum_mapper() -> bool {
        std::string stName;

        group_node *pGroupNode = loader_->group("vnummapper");

        if (nullptr == pGroupNode) {
            metin2::log::error("dragon_soul_table.txt is missing the VnumMapper group");
            return false;
        }
        {
            int n = pGroupNode->row_count();
            if (0 == n) {
                metin2::log::error("VnumMapper group is empty");
                return false;
            }

            std::set<uint8_t> setTypes;

            for (int i = 0; i < n; i++) {
                const group_node::group_node_row *pRow;
                pGroupNode->row(i, &pRow);

                std::string stDragonSoulName;
                uint8_t bType;
                if (!pRow->value("dragonsoulname", stDragonSoulName)) {
                    metin2::log::error("VnumMapper row has no DragonSoulName column");
                    return false;
                }
                if (!pRow->value("type", bType)) {
                    metin2::log::error("VnumMapper: invalid vnum for {}", stDragonSoulName);
                    return false;
                }

                if (setTypes.end() != setTypes.find(bType)) {
                    metin2::log::error("VnumMapper has duplicate vnums");
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

    auto dragon_soul_table::read_basic_applys() -> bool {
        group_node *pGroupNode = loader_->group("basicapplys");

        if (nullptr == pGroupNode) {
            metin2::log::error("dragon_soul_table.txt is missing the BasicApplys group");
            return false;
        }

        for (uint i = 0; i < dragon_soul_names_.size(); i++) {
            group_node *pChild;
            if (nullptr == (pChild = pGroupNode->child_node(dragon_soul_names_[i]))) {
                metin2::log::error("BasicApplys: group {} is not defined", dragon_soul_names_[i]);
                return false;
            }
            vec_applys vecApplys;
            int n = pChild->row_count();

            for (int j = 1; j <= n; j++) {
                std::stringstream ss;
                ss << j;
                const group_node::group_node_row *pRow = nullptr;

                pChild->row(ss.str(), &pRow);
                if (nullptr == pRow) {
                    metin2::log::error("BasicApplys: row {:d} is missing", j);
                }
                apply_types at;
                int av;

                std::string stTypeName;
                if (!pRow->value("apply_type", stTypeName)) {
                    metin2::log::error("BasicApplys: group {} has an empty apply_type", dragon_soul_names_[i]);
                    return false;
                }
                if (!(at = (apply_types)fn_get_apply_type(stTypeName.c_str()))) {
                    metin2::log::error("BasicApplys: group {} has unknown apply_type {}", dragon_soul_names_[i], stTypeName);
                    return false;
                }
                if (!pRow->value("apply_value", av)) {
                    metin2::log::error("BasicApplys: group {} has invalid apply_value {:d}", dragon_soul_names_[i], av);
                    return false;
                }
                vecApplys.push_back(apply(at, av));
            }
            map_basic_applys_group_.insert(map_apply_group::value_type(map_name_to_type_[dragon_soul_names_[i]], vecApplys));
        }

        return true;
    }

    auto dragon_soul_table::read_additional_applys() -> bool {
        group_node *pGroupNode = loader_->group("additionalapplys");
        if (nullptr == pGroupNode) {
            metin2::log::error("dragon_soul_table.txt is missing the AdditionalApplys group");
            return false;
        }

        for (size_t i = 0; i < dragon_soul_names_.size(); i++) {
            group_node *pChild;
            if (nullptr == (pChild = pGroupNode->child_node(dragon_soul_names_[i]))) {
                metin2::log::error("AdditionalApplys: group {} is not defined", dragon_soul_names_[i]);
                return false;
            }
            vec_applys vecApplys;

            int n = pChild->row_count();

            for (int j = 0; j < n; j++) {
                const group_node::group_node_row *pRow;

                pChild->row(j, &pRow);

                apply_types at;
                int av;
                float prob;
                std::string stTypeName;
                if (!pRow->value("apply_type", stTypeName)) {
                    metin2::log::error("AdditionalApplys: group {} has an empty apply_type", dragon_soul_names_[i]);
                    return false;
                }
                if (!(at = (apply_types)fn_get_apply_type(stTypeName.c_str()))) {
                    metin2::log::error("AdditionalApplys: group {} has unknown apply_type {}", dragon_soul_names_[i], stTypeName);
                    return false;
                }
                if (!pRow->value("apply_value", av)) {
                    metin2::log::error("AdditionalApplys: group {} has invalid apply_value {:d}", dragon_soul_names_[i], av);
                    return false;
                }
                if (!pRow->value("prob", prob)) {
                    metin2::log::error("AdditionalApplys: group {} has invalid probability {:g}", dragon_soul_names_[i], prob);
                    return false;
                }
                vecApplys.push_back(apply(at, av, prob));
            }
            map_additional_applys_group_.insert(map_apply_group::value_type(map_name_to_type_[dragon_soul_names_[i]], vecApplys));
        }

        return true;
    }

    auto dragon_soul_table::check_apply_num_settings() -> bool {
        if (nullptr == apply_num_setting_node_) {
            metin2::log::error("dragon_soul_table.txt is missing the ApplyNumSettings group");
            return false;
        } else {
            for (size_t i = 0; i < dragon_soul_types_.size(); i++) {
                for (int j = 0; j < DRAGON_SOUL_GRADE_MAX; j++) {
                    int basis, add_min, add_max;
                    if (!apply_num_settings(dragon_soul_types_[i], j, basis, add_min, add_max)) {
                        metin2::log::error("ApplyNumSettings: group {} has an invalid row for grade {}",
                            dragon_soul_names_[i], ast_grade_name[j]);
                        return false;
                    }
                }
            }
        }

        return true;
    }

    auto dragon_soul_table::check_weight_tables() -> bool {
        if (nullptr == weight_table_node_) {
            metin2::log::error("dragon_soul_table.txt is missing the WeightTables group");
            return false;
        } else {
            for (size_t i = 0; i < dragon_soul_types_.size(); i++) {
                for (int j = 0; j < DRAGON_SOUL_GRADE_MAX; j++) {
                    for (int k = 0; k < DRAGON_SOUL_STEP_MAX; k++) {
                        for (int l = 0; l < DRAGON_SOUL_STRENGTH_MAX; l++) {
                            float fWeight;
                            if (!weight(dragon_soul_types_[i], j, k, l, fWeight)) {
                                metin2::log::error("WeightTables: group {} has an invalid value for grade {}, step {}, strength {:d}",
                                    dragon_soul_names_[i], ast_grade_name[j], ast_step_name[k], l);
                            }
                        }
                    }
                }
            }
        }
        return true;
    }

    auto dragon_soul_table::check_refine_grade_tables() -> bool {
        if (nullptr == refine_grade_table_node_) {
            metin2::log::error("dragon_soul_table.txt is missing the RefineGradeTables group");
            return false;
        } else {
            for (size_t i = 0; i < dragon_soul_types_.size(); i++) {
                for (int j = 0; j < DRAGON_SOUL_GRADE_MAX - 1; j++) {
                    int need_count, fee;
                    std::vector<float> vec_probs;
                    if (!refine_grade_values(dragon_soul_types_[i], j, need_count, fee, vec_probs)) {
                        metin2::log::error("RefineGradeTables: group {} has an invalid row for grade {}",
                            dragon_soul_names_[i], ast_grade_name[j]);
                        return false;
                    }
                    if (need_count < 1) {
                        metin2::log::error("RefineGradeTables: group {} has need_count below 1 for grade {}",
                            dragon_soul_names_[i], ast_grade_name[j]);
                        return false;
                    }
                    if (fee < 0) {
                        metin2::log::error("RefineGradeTables: group {} has a negative fee for grade {}",
                            dragon_soul_names_[i], ast_grade_name[j]);
                        return false;
                    }
                    if (DRAGON_SOUL_GRADE_MAX != vec_probs.size()) {
                        metin2::log::error("RefineGradeTables: probability list must have {:d} entries", DRAGON_SOUL_GRADE_MAX);
                        return false;
                    }
                    for (size_t k = 0; k < vec_probs.size(); k++) {
                        if (vec_probs[k] < 0.f) {
                            metin2::log::error("RefineGradeTables: probability {:d} is negative", k);
                            return false;
                        }
                    }
                }
            }
        }

        return true;
    }

    auto dragon_soul_table::check_refine_step_tables() -> bool {
        if (nullptr == refine_strength_table_node_) {
            metin2::log::error("dragon_soul_table.txt is missing the RefineStepTables group");
            return false;
        } else {
            for (size_t i = 0; i < dragon_soul_types_.size(); i++) {
                for (int j = 0; j < DRAGON_SOUL_STEP_MAX - 1; j++) {
                    int need_count, fee;
                    std::vector<float> vec_probs;
                    if (!refine_step_values(dragon_soul_types_[i], j, need_count, fee, vec_probs)) {
                        metin2::log::error("RefineStepTables: group {} has an invalid row for step {}",
                            dragon_soul_names_[i], ast_step_name[j]);
                        return false;
                    }
                    if (need_count < 1) {
                        metin2::log::error("RefineStepTables: group {} has need_count below 1 for step {}",
                            dragon_soul_names_[i], ast_step_name[j]);
                        return false;
                    }
                    if (fee < 0) {
                        metin2::log::error("RefineStepTables: group {} has a negative fee for step {}",
                            dragon_soul_names_[i], ast_step_name[j]);
                        return false;
                    }
                    if (DRAGON_SOUL_GRADE_MAX != vec_probs.size()) {
                        metin2::log::error("RefineStepTables: group {} probability list must have {:d} entries",
                            dragon_soul_names_[i], DRAGON_SOUL_GRADE_MAX);
                        return false;
                    }
                    for (size_t k = 0; k < vec_probs.size(); k++) {
                        if (vec_probs[k] < 0.f) {
                            metin2::log::error("RefineStepTables: group {} has negative probability {:d}",
                                dragon_soul_names_[i], k);
                            return false;
                        }
                    }
                }
            }
        }

        return true;
    }

    auto dragon_soul_table::check_refine_strength_tables() -> bool {
        group_node *pGroupNode = refine_strength_table_node_;
        if (nullptr == pGroupNode) {
            metin2::log::error("dragon_soul_table.txt is missing the RefineStrengthTables group");
            return false;
        }
        for (size_t i = 0; i < dragon_soul_types_.size(); i++) {
            for (int j = MATERIAL_DS_REFINE_NORMAL; j <= MATERIAL_DS_REFINE_HOLLY; j++) {
                int fee;
                float prob;
                for (int k = 0; k < DRAGON_SOUL_STRENGTH_MAX - 1; k++) {
                    if (!refine_strength_values(dragon_soul_types_[i], j, k, fee, prob)) {
                        metin2::log::error("RefineStrengthTables: group {} has an invalid value or fee for material {}, strength {:d}",
                            dragon_soul_names_[i], ast_material_name[j], k);
                        return false;
                    }
                    if (fee < 0) {
                        metin2::log::error("RefineStrengthTables: group {} has a negative fee for material {}",
                            dragon_soul_names_[i], ast_material_name[j]);
                        return false;
                    }
                    if (prob < 0.f) {
                        metin2::log::error("RefineStrengthTables: group {} has a negative probability for material {}, strength {:d}",
                            dragon_soul_names_[i], ast_material_name[j], k);
                        return false;
                    }
                }
            }
        }

        return true;
    }

    auto dragon_soul_table::check_dragon_heart_ext_tables() -> bool {
        if (nullptr == dragon_heart_ext_table_node_) {
            metin2::log::error("dragon_soul_table.txt is missing the DragonHeartExtTables group");
            return false;
        }
        for (size_t i = 0; i < dragon_soul_types_.size(); i++) {
            for (int j = 0; j < DRAGON_SOUL_GRADE_MAX; j++) {
                std::vector<float> vec_chargings;
                std::vector<float> vec_probs;

                if (!dragon_heart_ext_values(dragon_soul_types_[i], j, vec_chargings, vec_probs)) {
                    metin2::log::error("DragonHeartExtTables: group {} has an invalid CHARGING row or row for grade {}",
                        dragon_soul_names_[i], ast_grade_name[j]);
                    return false;
                }
                if (vec_chargings.size() != vec_probs.size()) {
                    metin2::log::error("DragonHeartExtTables: group {} has {:d} CHARGING values but {:d} probabilities",
                        dragon_soul_names_[i], vec_chargings.size(), vec_probs.size());
                    return false;
                }
                for (size_t k = 0; k < vec_chargings.size(); k++) {
                    if (vec_chargings[k] < 0.f) {
                        metin2::log::error("DragonHeartExtTables: group {} has negative CHARGING value {:d}",
                            dragon_soul_names_[i], k);
                        return false;
                    }
                }
                for (size_t k = 0; k < vec_probs.size(); k++) {
                    if (vec_probs[k] < 0.f) {
                        metin2::log::error("DragonHeartExtTables: group {} has negative probability for grade {}, index {:d}",
                            dragon_soul_names_[i], ast_grade_name[j], k);
                        return false;
                    }
                }
            }
        }

        return true;
    }

    auto dragon_soul_table::check_dragon_soul_ext_tables() -> bool {
        if (nullptr == dragon_soul_ext_table_node_) {
            metin2::log::error("dragon_soul_table.txt is missing the DragonSoulExtTables group");
            return false;
        }
        for (size_t i = 0; i < dragon_soul_types_.size(); i++) {
            for (int j = 0; j < DRAGON_SOUL_GRADE_MAX; j++) {
                float prob;
                uint32_t by_product;
                if (!dragon_soul_ext_values(dragon_soul_types_[i], j, prob, by_product)) {
                    metin2::log::error("DragonSoulExtTables: group {} has an invalid row for grade {}",
                        dragon_soul_names_[i], ast_grade_name[j]);
                    return false;
                }
                if (prob < 0.f) {
                    metin2::log::error("DragonSoulExtTables: group {} has a negative probability for grade {}",
                        dragon_soul_names_[i], ast_grade_name[j]);
                    return false;
                }
                if (0 != by_product && nullptr == item_manager::instance().table(by_product)) {
                    metin2::log::error("DragonSoulExtTables: group {} has unknown by-product {:d} for grade {}",
                        dragon_soul_names_[i], by_product, ast_grade_name[j]);
                    return false;
                }
            }
        }
        return true;
    }

    auto dragon_soul_table::basic_applys(uint8_t ds_type, vec_applys &vec_basic_applys) -> bool {
        map_apply_group::iterator it = map_basic_applys_group_.find(ds_type);
        if (map_basic_applys_group_.end() == it) {
            return false;
        }
        vec_basic_applys = it->second;
        return true;
    }

    auto dragon_soul_table::additional_applys(uint8_t ds_type, vec_applys &vec_additional_applys) -> bool {
        map_apply_group::iterator it = map_additional_applys_group_.find(ds_type);
        if (map_additional_applys_group_.end() == it) {
            return false;
        }
        vec_additional_applys = it->second;
        return true;
    }

    auto dragon_soul_table::apply_num_settings(uint8_t ds_type, uint8_t grade_idx, int &basis, int &add_min, int &add_max) -> bool {
        if (grade_idx >= DRAGON_SOUL_GRADE_MAX) {
            metin2::log::error("Dragon soul grade index {:d} is out of range", grade_idx);
            return false;
        }

        std::string stValue;
        std::string stDragonSoulName;
        if (!dragon_soul_group_name(ds_type, stDragonSoulName)) {
            metin2::log::error("Unknown dragon soul type {:d}", ds_type);
            return false;
        }

        if (!apply_num_setting_node_->group_value(stDragonSoulName, "basis", ast_grade_name[grade_idx], basis)) {
            metin2::log::error("Invalid basis value for dragon soul group {}, grade {}", stDragonSoulName, ast_grade_name[grade_idx]);
            return false;
        }

        if (!apply_num_setting_node_->group_value(stDragonSoulName, "add_min", ast_grade_name[grade_idx], add_min)) {
            metin2::log::error("Invalid add_min value for dragon soul group {}, grade {}", stDragonSoulName, ast_grade_name[grade_idx]);
            return false;
        }

        if (!apply_num_setting_node_->group_value(stDragonSoulName, "add_max", ast_grade_name[grade_idx], add_max)) {
            metin2::log::error("Invalid add_max value for dragon soul group {}, grade {}", stDragonSoulName, ast_grade_name[grade_idx]);
            return false;
        }

        return true;
    }

    auto dragon_soul_table::weight(uint8_t ds_type, uint8_t grade_idx, uint8_t step_index, uint8_t strength_idx, float &weight) -> bool {
        if (grade_idx >= DRAGON_SOUL_GRADE_MAX || step_index >= DRAGON_SOUL_STEP_MAX || strength_idx >= DRAGON_SOUL_STRENGTH_MAX) {
            metin2::log::error("Dragon soul grade {:d}, step {:d} or strength {:d} is out of range", grade_idx, step_index, strength_idx);
            return false;
        }

        std::string stValue;
        std::string stDragonSoulName;
        if (!dragon_soul_group_name(ds_type, stDragonSoulName)) {
            metin2::log::error("Unknown dragon soul type {:d}", ds_type);
            return false;
        }

        group_node *pDragonSoulGroup = weight_table_node_->child_node(stDragonSoulName);
        if (nullptr != pDragonSoulGroup) {
            if (pDragonSoulGroup->group_value(ast_grade_name[grade_idx], ast_step_name[step_index], strength_idx, weight)) {
                return true;
            }
        }
        pDragonSoulGroup = weight_table_node_->child_node("default");
        if (nullptr != pDragonSoulGroup) {
            if (!pDragonSoulGroup->group_value(ast_grade_name[grade_idx], ast_step_name[step_index], strength_idx, weight)) {
                metin2::log::error("Invalid weight for dragon soul group {}, grade {}, step {}, strength {:d}", stDragonSoulName, ast_grade_name[grade_idx], ast_step_name[step_index], strength_idx);
                return false;
            } else
                return true;
        }
        metin2::log::error("No weight for dragon soul group {}, grade {}, step {}, strength {:d}", stDragonSoulName, ast_grade_name[grade_idx], ast_step_name[step_index], strength_idx);
        return false;
    }

    auto dragon_soul_table::refine_grade_values(uint8_t ds_type, uint8_t grade_idx, int &need_count, int &fee, std::vector<float> &vec_probs) -> bool {
        if (grade_idx >= DRAGON_SOUL_GRADE_MAX - 1) {
            metin2::log::error("Dragon soul grade index {:d} is out of range", grade_idx);
            return false;
        }

        std::string stValue;
        std::string stDragonSoulName;
        if (!dragon_soul_group_name(ds_type, stDragonSoulName)) {
            metin2::log::error("Unknown dragon soul type {:d}", ds_type);
            return false;
        }

        const group_node::group_node_row *pRow;
        if (!refine_grade_table_node_->group_row(stDragonSoulName, ast_grade_name[grade_idx], &pRow)) {
            metin2::log::error("Invalid row for dragon soul group {}, grade {}", stDragonSoulName, ast_grade_name[grade_idx]);
            return false;
        }

        if (!pRow->value("need_count", need_count)) {
            metin2::log::error("Invalid need_count for dragon soul group {}, grade {}", stDragonSoulName, ast_grade_name[grade_idx]);
            return false;
        }

        if (!pRow->value("fee", fee)) {
            metin2::log::error("Invalid fee for dragon soul group {}, grade {}", stDragonSoulName, ast_grade_name[grade_idx]);
            return false;
        }

        vec_probs.resize(DRAGON_SOUL_GRADE_MAX);
        for (int i = 0; i < DRAGON_SOUL_GRADE_MAX; i++) {
            if (!pRow->value(ast_grade_name[i], vec_probs[i])) {
                metin2::log::error("Invalid value for dragon soul group {}, grade {}, column {}", stDragonSoulName, ast_grade_name[grade_idx], ast_grade_name[i]);
                return false;
            }
        }

        return true;
    }

    auto dragon_soul_table::refine_step_values(uint8_t ds_type, uint8_t step_idx, int &need_count, int &fee, std::vector<float> &vec_probs) -> bool {
        if (step_idx >= DRAGON_SOUL_STEP_MAX - 1) {
            metin2::log::error("Dragon soul step index {:d} is out of range", step_idx);
            return false;
        }

        std::string stValue;
        std::string stDragonSoulName;
        if (!dragon_soul_group_name(ds_type, stDragonSoulName)) {
            metin2::log::error("Unknown dragon soul type {:d}", ds_type);
            return false;
        }

        const group_node::group_node_row *pRow;
        if (!refine_step_table_node_->group_row(stDragonSoulName, ast_step_name[step_idx], &pRow)) {
            metin2::log::error("Invalid row for dragon soul group {}, step {}", stDragonSoulName, ast_step_name[step_idx]);
            return false;
        }

        if (!pRow->value("need_count", need_count)) {
            metin2::log::error("Invalid need_count for dragon soul group {}, step {}", stDragonSoulName, ast_step_name[step_idx]);
            return false;
        }

        if (!pRow->value("fee", fee)) {
            metin2::log::error("Invalid fee for dragon soul group {}, step {}", stDragonSoulName, ast_step_name[step_idx]);
            return false;
        }

        vec_probs.resize(DRAGON_SOUL_STEP_MAX);
        for (int i = 0; i < DRAGON_SOUL_STEP_MAX; i++) {
            if (!pRow->value(ast_step_name[i], vec_probs[i])) {
                metin2::log::error("Invalid value for dragon soul group {}, step {}, column {}", stDragonSoulName, ast_step_name[step_idx], ast_step_name[i]);
                return false;
            }
        }

        return true;
    }

    auto dragon_soul_table::refine_strength_values(uint8_t ds_type, uint8_t material_type, uint8_t strength_idx, int &fee, float &prob) -> bool {
        if (material_type < MATERIAL_DS_REFINE_NORMAL || material_type > MATERIAL_DS_REFINE_HOLLY) {
            metin2::log::error("Dragon soul material type {:d} is out of range", material_type);
            return false;
        }

        std::string stValue;
        std::string stDragonSoulName;
        if (!dragon_soul_group_name(ds_type, stDragonSoulName)) {
            metin2::log::error("Unknown dragon soul type {:d}", ds_type);
            return false;
        }

        if (!refine_strength_table_node_->group_value(stDragonSoulName, ast_material_name[material_type], "fee", fee)) {
            metin2::log::error("Invalid fee for dragon soul group {}, material {}",
                stDragonSoulName, ast_material_name[material_type]);
            return false;
        }
        std::string stStrengthIdx = std::to_string(static_cast<int>(strength_idx));

        if (!refine_strength_table_node_->group_value(stDragonSoulName, ast_material_name[material_type], stStrengthIdx, prob)) {
            metin2::log::error("Invalid probability for dragon soul group {}, material {}, strength {:d}",
                stDragonSoulName, ast_material_name[material_type], strength_idx);
            return false;
        }

        return true;
    }

    auto dragon_soul_table::dragon_heart_ext_values(uint8_t ds_type, uint8_t grade_idx, std::vector<float> &vec_chargings, std::vector<float> &vec_probs) -> bool {
        if (grade_idx >= DRAGON_SOUL_GRADE_MAX) {
            metin2::log::error("Dragon soul grade index {:d} is out of range", grade_idx);
            return false;
        }

        std::string stDragonSoulName;
        if (!dragon_soul_group_name(ds_type, stDragonSoulName)) {
            metin2::log::error("Unknown dragon soul type {:d}", ds_type);
            return false;
        }

        const group_node::group_node_row *pRow;
        if (!dragon_heart_ext_table_node_->group_row(stDragonSoulName, "charging", &pRow)) {
            metin2::log::error("Invalid CHARGING row for dragon soul group {}", stDragonSoulName);
            return false;
        }
        int n = pRow->size();
        vec_chargings.resize(n);
        for (int i = 0; i < n; i++) {
            if (!pRow->value(i, vec_chargings[i])) {
                metin2::log::error("Invalid CHARGING value for dragon soul group {}, column {:d}", stDragonSoulName, i);
                return false;
            }
        }

        if (!dragon_heart_ext_table_node_->group_row(stDragonSoulName, ast_grade_name[grade_idx], &pRow)) {
            metin2::log::error("Invalid row for dragon soul group {}, grade {}", stDragonSoulName, ast_grade_name[grade_idx]);
            return false;
        }

        int m = pRow->size();
        if (n != m) {
            metin2::log::error("Row has {:d} values but CHARGING row has {:d} (dragon soul group {}, grade {})", m, n,
                stDragonSoulName, ast_grade_name[grade_idx]);
            return false;
        }
        vec_probs.resize(m);
        for (int i = 0; i < m; i++) {
            if (!pRow->value(i, vec_probs[i])) {
                metin2::log::error("Invalid value for dragon soul group {}, grade {}, column {:d}", stDragonSoulName, ast_grade_name[grade_idx], i);
                return false;
            }
        }

        return true;
    }

    auto dragon_soul_table::dragon_soul_ext_values(uint8_t ds_type, uint8_t grade_idx, float &prob, uint32_t &by_product) -> bool {
        if (grade_idx >= DRAGON_SOUL_GRADE_MAX) {
            metin2::log::error("Dragon soul grade index {:d} is out of range", grade_idx);
            return false;
        }

        std::string stValue;
        std::string stDragonSoulName;
        if (!dragon_soul_group_name(ds_type, stDragonSoulName)) {
            metin2::log::error("Unknown dragon soul type {:d}", ds_type);
            return false;
        }

        if (!dragon_soul_ext_table_node_->group_value(stDragonSoulName, ast_grade_name[grade_idx], "prob", prob)) {
            metin2::log::error("Invalid probability for dragon soul group {}, grade {}",
                stDragonSoulName, ast_grade_name[grade_idx]);
            return false;
        }

        if (!dragon_soul_ext_table_node_->group_value(stDragonSoulName, ast_grade_name[grade_idx], "byproduct", by_product)) {
            metin2::log::error("Invalid fee for dragon soul group {}, grade {} (index {:d})",
                stDragonSoulName, ast_grade_name[grade_idx], grade_idx);
            return false;
        }

        return true;
    }

    dragon_soul_table::dragon_soul_table() {
    }
    dragon_soul_table::~dragon_soul_table() {
    }
}
