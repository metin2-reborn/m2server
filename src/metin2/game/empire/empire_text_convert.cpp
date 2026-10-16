#include <metin2/game/empire/empire_text_convert.h>

#include <metin2/core/utils.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace metin2::game::inline empire {

    namespace {
        struct text_convert_table {
            char ac_upper[26];
            char ac_lower[26];
            uint8_t aac_han[5000][2];
            uint8_t aac_jaum[50][2];
            uint8_t aac_moum[50][2];
        } text_conv_table[3];

        bool text_conv_table_loaded[3];
    }

    auto load_empire_text_convert_table(uint32_t empire_id, const char *file_name) -> bool {
        if (empire_id < 1 || empire_id > 3)
            return false;

        FILE *fp = fopen(file_name, "rb");

        if (!fp)
            return false;

        uint32_t dwEngCount = 26;
        uint32_t dwHanCount = (0xC8 - 0xB0 + 1) * (0xFE - 0xA1 + 1);
        uint32_t dwHanSize = dwHanCount * 2;

        text_convert_table &rkTextConvTable = text_conv_table[empire_id - 1];

        bool ok = fread(rkTextConvTable.ac_upper, 1, dwEngCount, fp) == dwEngCount;
        ok = fread(rkTextConvTable.ac_lower, 1, dwEngCount, fp) == dwEngCount && ok;
        ok = fread(rkTextConvTable.aac_han, 1, dwHanSize, fp) == dwHanSize && ok;

        ok = fread(rkTextConvTable.aac_jaum, 1, 60, fp) == 60 && ok;
        ok = fread(rkTextConvTable.aac_moum, 1, 42, fp) == 42 && ok;

        fclose(fp);

        text_conv_table_loaded[empire_id - 1] = ok;
        return ok;
    }

    void convert_empire_text(uint32_t empire_id, char *text, size_t len, int pct) {
        if (empire_id < 1 || empire_id > 3 || len == 0 || !text_conv_table_loaded[empire_id - 1])
            return;

        static const char *s_aszSymbols2[] = {"¤", "§", "¶", "±", "×"};
        static const char *s_aszSymbols3[] = {"∫", "＃", "＄", "⊃", "∝"};

        const text_convert_table &rkTextConvTable = text_conv_table[empire_id - 1];

        uint8_t *pbText = reinterpret_cast<uint8_t *>(text);
        size_t pos = 0;
        while (pos < len && pbText[pos] != '\0') {
            size_t seq = utf8_sequence_length(pbText[pos]);
            if (pos + seq > len)
                break;

            if (number(1, 100) > pct) {
                uint8_t &c = pbText[pos];
                if (seq == 1) {
                    if (c >= 'a' && c <= 'z')
                        c = rkTextConvTable.ac_lower[c - 'a'];
                    else if (c >= 'A' && c <= 'Z')
                        c = rkTextConvTable.ac_upper[c - 'A'];
                } else if (seq == 2)
                    memcpy(pbText + pos, s_aszSymbols2[number(0, 4)], 2);
                else if (seq == 3)
                    memcpy(pbText + pos, s_aszSymbols3[number(0, 4)], 3);
            }

            pos += seq;
        }
    }
}
