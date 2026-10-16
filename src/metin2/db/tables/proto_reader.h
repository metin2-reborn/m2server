#pragma once

#include <metin2/db/tables/csv_reader.h>

#include <metin2/proto/tables.h>

#include <map>
#include <string>

namespace metin2::db::inline tables {

    auto set_proto_mob_table(mob_table *out, csv_table &csv, const std::map<int, std::string> &name_map) -> bool;
    auto set_proto_item_table(item_table *out, csv_table &csv, const std::map<int, std::string> &name_map) -> bool;
}
