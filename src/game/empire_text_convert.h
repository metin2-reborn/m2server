#ifndef __INC_EMPIRE_CHAT_CONVERT
#define __INC_EMPIRE_CHAT_CONVERT

#include <cstddef>
#include <cstdint>

bool LoadEmpireTextConvertTable(uint32_t dwEmpireID, const char* c_szFileName);
void ConvertEmpireText(uint32_t dwEmpireID, char* szText, size_t len, int iPct);

#endif
