#pragma once
#include <metin2/proto/apply_types.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace metin2::game::inline data {
    class group_node;
    class group_text_parse_tree_loader;
}

namespace metin2::game::inline items {

    struct apply {
        apply(apply_types at, int av, float p = 0.f) : apply_type(at), apply_value(av), prob(p) {}
        apply_types apply_type;
        int apply_value;
        float prob;
    };
    const int DRAGON_SOUL_ADDITIONAL_ATTR_START_IDX = 3;

    class dragon_soul_table {
      public:
        dragon_soul_table();
        ~dragon_soul_table();
        using apply_list = std::vector<apply>;
        using apply_group_map = std::map<uint8_t, apply_list>;

        auto read_dragon_soul_table_file(const char *file_name) -> bool;
        auto dragon_soul_group_name(uint8_t type, std::string &group_name) const -> bool;

        auto basic_applys(uint8_t ds_type, apply_list &basic_applys) -> bool;
        auto additional_applys(uint8_t ds_type, apply_list &additional_applys) -> bool;

        auto apply_num_settings(uint8_t ds_type, uint8_t grade_idx, int &basis, int &add_min, int &add_max) -> bool;
        auto weight(uint8_t ds_type, uint8_t grade_idx, uint8_t step_index, uint8_t strength_idx, float &weight) -> bool;
        auto refine_grade_values(uint8_t ds_type, uint8_t grade_idx, int &need_count, int &fee, std::vector<float> &probs) -> bool;
        auto refine_step_values(uint8_t ds_type, uint8_t step_idx, int &need_count, int &fee, std::vector<float> &probs) -> bool;
        auto refine_strength_values(uint8_t ds_type, uint8_t material_type, uint8_t strength_idx, int &fee, float &prob) -> bool;
        auto dragon_heart_ext_values(uint8_t ds_type, uint8_t grade_idx, std::vector<float> &chargings, std::vector<float> &probs) -> bool;
        auto dragon_soul_ext_values(uint8_t ds_type, uint8_t grade_idx, float &prob, uint32_t &product) -> bool;

      private:
        std::unique_ptr<group_text_parse_tree_loader> loader_;
        std::string file_name_;

        group_node *apply_num_setting_node_;
        group_node *weight_table_node_;
        group_node *refine_grade_table_node_;
        group_node *refine_step_table_node_;
        group_node *refine_strength_table_node_;
        group_node *dragon_heart_ext_table_node_;
        group_node *dragon_soul_ext_table_node_;

        using name_to_type_map = std::map<std::string, uint8_t>;
        using type_to_name_map = std::map<uint8_t, std::string>;
        std::vector<std::string> dragon_soul_names_;
        std::vector<uint8_t> dragon_soul_types_;

        name_to_type_map name_to_type_;
        type_to_name_map type_to_name_;
        apply_group_map basic_applys_groups_;
        apply_group_map additional_applys_groups_;

        auto read_vnum_mapper() -> bool;
        auto read_basic_applys() -> bool;
        auto read_additional_applys() -> bool;
        auto check_apply_num_settings() -> bool;
        auto check_weight_tables() -> bool;
        auto check_refine_grade_tables() -> bool;
        auto check_refine_step_tables() -> bool;
        auto check_refine_strength_tables() -> bool;
        auto check_dragon_heart_ext_tables() -> bool;
        auto check_dragon_soul_ext_tables() -> bool;
    };
}
