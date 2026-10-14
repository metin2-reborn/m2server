#include "empire_text_convert.h"

#include <libthecore/utils.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace 
{
	struct STextConvertTable 
	{
		char acUpper[26];
		char acLower[26];
		uint8_t aacHan[5000][2];
		uint8_t aacJaum[50][2];
		uint8_t aacMoum[50][2];
	} g_aTextConvTable[3];

	bool g_abTextConvTableLoaded[3];

	// Length of the UTF-8 sequence that starts with c (1 for ASCII and stray bytes).
	size_t Utf8SequenceLength(uint8_t c)
	{
		return c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
	}
}

bool LoadEmpireTextConvertTable(uint32_t dwEmpireID, const char* c_szFileName)
{
	if (dwEmpireID < 1 || dwEmpireID > 3)
		return false;

	FILE * fp = fopen(c_szFileName, "rb");

	if (!fp)
		return false;

	uint32_t dwEngCount = 26;
	uint32_t dwHanCount = (0xC8 - 0xB0+1) * (0xFE - 0xA1+1);
	uint32_t dwHanSize = dwHanCount * 2;

	STextConvertTable& rkTextConvTable=g_aTextConvTable[dwEmpireID-1];

	bool ok = fread(rkTextConvTable.acUpper, 1, dwEngCount, fp) == dwEngCount;
	ok = fread(rkTextConvTable.acLower, 1, dwEngCount, fp) == dwEngCount && ok;
	ok = fread(rkTextConvTable.aacHan, 1, dwHanSize, fp) == dwHanSize && ok;

	ok = fread(rkTextConvTable.aacJaum, 1, 60, fp) == 60 && ok;
	ok = fread(rkTextConvTable.aacMoum, 1, 42, fp) == 42 && ok;

	fclose(fp);

	g_abTextConvTableLoaded[dwEmpireID - 1] = ok;
	return ok;
}

// Scrambles chat from another empire (text is UTF-8). With probability 100 - iPct per character:
// ASCII letters go through the empire's letter table, any other character is replaced in place by
// a symbol of the same UTF-8 length (as the original did with two-byte symbols). Without the
// empire's table (lang*.cvt) the text is left as it is.
void ConvertEmpireText(uint32_t dwEmpireID, char* szText, size_t len, int iPct)
{
	if (dwEmpireID < 1 || dwEmpireID > 3 || len == 0 || !g_abTextConvTableLoaded[dwEmpireID - 1])
		return;

	static const char* s_aszSymbols2[] = { "¤", "§", "¶", "±", "×" };
	static const char* s_aszSymbols3[] = { "∫", "＃", "＄", "⊃", "∝" };

	const STextConvertTable& rkTextConvTable = g_aTextConvTable[dwEmpireID - 1];

	uint8_t* pbText = reinterpret_cast<uint8_t*>(szText);
	size_t pos = 0;
	while (pos < len && pbText[pos] != '\0')
	{
		size_t seq = Utf8SequenceLength(pbText[pos]);
		if (pos + seq > len)
			break;

		if (number(1,100) > iPct)
		{
			uint8_t& c = pbText[pos];
			if (seq == 1)
			{
				if (c >= 'a' && c <= 'z')
					c = rkTextConvTable.acLower[c - 'a'];
				else if (c >= 'A' && c <= 'Z')
					c = rkTextConvTable.acUpper[c - 'A'];
			}
			else if (seq == 2)
				memcpy(pbText + pos, s_aszSymbols2[number(0, 4)], 2);
			else if (seq == 3)
				memcpy(pbText + pos, s_aszSymbols3[number(0, 4)], 3);
		}

		pos += seq;
	}
}
