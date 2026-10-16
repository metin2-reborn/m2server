#pragma once

#include <metin2/db/tables/csv_reader.h>

#include <metin2/proto/tables.h>

#include <map>
#include <string>

namespace metin2::db::inline tables {

    auto get_item_type_value(std::string input_string) -> int;
    auto get_item_sub_type_value(int type_value, std::string input_string) -> int;
    auto get_item_anti_flag_value(std::string input_string) -> int;
    auto get_item_flag_value(std::string input_string) -> int;
    auto get_item_wear_flag_value(std::string input_string) -> int;
    auto get_item_immune_value(std::string input_string) -> int;
    auto get_item_limit_type_value(std::string input_string) -> int;
    auto get_item_apply_type_value(std::string input_string) -> int;

    auto get_mob_rank_value(std::string input_string) -> int;
    auto get_mob_type_value(std::string input_string) -> int;
    auto get_mob_battle_type_value(std::string input_string) -> int;

    auto get_mob_size_value(std::string input_string) -> int;
    auto get_mob_ai_flag_value(std::string input_string) -> int;
    auto get_mob_race_flag_value(std::string input_string) -> int;
    auto get_mob_immune_flag_value(std::string input_string) -> int;

    auto set_proto_mob_table(mob_table *mob_table_value, c_csv_table &csv_table, std::map<int, const char *> &name_map) -> bool;
    auto set_proto_item_table(item_table *item_table_value, c_csv_table &csv_table, std::map<int, const char *> &name_map) -> bool;
}
