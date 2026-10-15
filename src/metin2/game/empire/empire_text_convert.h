#pragma once

#include <cstddef>
#include <cstdint>

namespace metin2::game::inline empire {

bool load_empire_text_convert_table(uint32_t empire_id, const char* file_name);
void convert_empire_text(uint32_t empire_id, char* text, size_t len, int pct);
}
