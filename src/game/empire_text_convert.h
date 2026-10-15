#pragma once

#include <cstddef>
#include <cstdint>

bool LoadEmpireTextConvertTable(uint32_t dwEmpireID, const char* c_szFileName);
void ConvertEmpireText(uint32_t dwEmpireID, char* szText, size_t len, int iPct);
