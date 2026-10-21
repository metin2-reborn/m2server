#pragma once

#include <string>

namespace metin2::game::inline text {

    auto locale_service_init(const std::string &service_name) -> bool;
    void locale_service_load_locale_string_file();
    void locale_service_load_empire_text_convert_tables();
    void locale_service_transfer_default_setting();
    auto locale_service_get_base_path() -> const std::string &;
    auto locale_service_get_map_path() -> const std::string &;
    auto locale_service_get_quest_path() -> const std::string &;

    auto check_name(const char *str) -> int;

    extern const std::string db_charset;
    extern std::string locale_filename;
}
