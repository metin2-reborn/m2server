#pragma once
#include <metin2/proto/length.h>

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

struct apply
{
	apply (apply_types at, int av, float p = 0.f) : apply_type(at), apply_value(av), prob(p) {}
	apply_types apply_type;
	int apply_value;
	float prob;
};
const int DRAGON_SOUL_ADDITIONAL_ATTR_START_IDX = 3;


class dragon_soul_table
{
public:
	dragon_soul_table();
	~dragon_soul_table();
	typedef std::vector <apply> vec_applys;
	typedef std::map <uint8_t, vec_applys> map_apply_group;
	
	bool	read_dragon_soul_table_file(const char * file_name);
	bool	dragon_soul_group_name(uint8_t type, std::string& group_name) const;

	bool	basic_applys(uint8_t ds_type, vec_applys& vec_basic_applys);
	bool	additional_applys(uint8_t ds_type, vec_applys& vec_additional_applys);

	bool	apply_num_settings(uint8_t ds_type, uint8_t grade_idx, int& basis, int& add_min, int& add_max);
	bool	weight(uint8_t ds_type, uint8_t grade_idx, uint8_t step_index, uint8_t strength_idx, float& weight);
	bool	refine_grade_values(uint8_t ds_type, uint8_t grade_idx, int& need_count, int& fee, std::vector<float>& vec_probs);
	bool	refine_step_values(uint8_t ds_type, uint8_t step_idx, int& need_count, int& fee, std::vector<float>& vec_probs);
	bool	refine_strength_values(uint8_t ds_type, uint8_t material_type, uint8_t strength_idx, int& fee, float& prob);
	bool	dragon_heart_ext_values(uint8_t ds_type, uint8_t grade_idx, std::vector<float>& vec_chargings, std::vector<float>& vec_probs);
	bool	dragon_soul_ext_values(uint8_t ds_type, uint8_t grade_idx, float& prob, uint32_t& by_product);

private:
	std::unique_ptr<group_text_parse_tree_loader> loader_;
	std::string file_name_;

	group_node*	apply_num_setting_node_;
	group_node*	weight_table_node_;
	group_node*	refine_grade_table_node_;
	group_node*	refine_step_table_node_;
	group_node*	refine_strength_table_node_;
	group_node*	dragon_heart_ext_table_node_;
	group_node*	dragon_soul_ext_table_node_;

	typedef std::map <std::string, uint8_t> map_name_to_type;
	typedef std::map <uint8_t, std::string> map_type_to_name;
	std::vector <std::string> dragon_soul_names_;
	std::vector <uint8_t> dragon_soul_types_;

	map_name_to_type map_name_to_type_;
	map_type_to_name map_type_to_name_;
	map_apply_group map_basic_applys_group_;
	map_apply_group map_additional_applys_group_;

	bool	read_vnum_mapper();
	bool	read_basic_applys();
	bool	read_additional_applys();
	bool	check_apply_num_settings();
	bool	check_weight_tables();
	bool	check_refine_grade_tables();
	bool	check_refine_step_tables();
	bool	check_refine_strength_tables();
	bool	check_dragon_heart_ext_tables();
	bool	check_dragon_soul_ext_tables();
};
}
