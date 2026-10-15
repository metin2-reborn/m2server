#pragma once

#include <string>

namespace metin2::game::inline text {

bool locale_service_init(const std::string& c_rstServiceName);
void locale_service_load_locale_string_file();
void locale_service_load_empire_text_convert_tables();
void locale_service_transfer_default_setting();
const std::string& locale_service_get_base_path();
const std::string& locale_service_get_map_path();
const std::string& locale_service_get_quest_path();

int check_name(const char * str);

extern std::string locale;
extern std::string locale_filename;
}
