#pragma once

#include <cstddef>
#include <cstdint>

namespace metin2::game::inline empire {

bool load_empire_text_convert_table(uint32_t dwEmpireID, const char* c_szFileName);
void convert_empire_text(uint32_t dwEmpireID, char* szText, size_t len, int iPct);
}
