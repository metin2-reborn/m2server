#pragma once

#include <metin2/db/tables/CsvReader.h>

#include <metin2/proto/tables.h>

#include <map>
#include <string>

namespace metin2::db::inline tables {

//Reads a csv file and fills the item table.

int get_item_type_value(std::string inputString);
int get_item_sub_type_value(int type_value, std::string inputString);
int get_item_anti_flag_value(std::string inputString);
int get_item_flag_value(std::string inputString);
int get_item_wear_flag_value(std::string inputString);
int get_item_immune_value(std::string inputString);
int get_item_limit_type_value(std::string inputString);
int get_item_apply_type_value(std::string inputString);

//Can also read the monster proto.
int get_mob_rank_value(std::string inputString);
int get_mob_type_value(std::string inputString);
int get_mob_battle_type_value(std::string inputString);

int get_mob_size_value(std::string inputString);
int get_mob_ai_flag_value(std::string inputString);
int get_mob_race_flag_value(std::string inputString);
int get_mob_immune_flag_value(std::string inputString);

//
bool set_proto_mob_table(mob_table *mobTable, c_csv_table &csvTable, std::map<int,const char*> &nameMap);
bool set_proto_item_table(item_table *itemTable, c_csv_table &csvTable,std::map<int,const char*> &nameMap);
}
