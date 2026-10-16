#include <metin2/game/empire/empire_text_convert.h>

#include <metin2/core/utils.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>

namespace metin2::game::inline empire {

    namespace {
        struct text_convert_table {
            char ac_upper[26];
            char ac_lower[26];
        } text_conv_table[3];

        bool text_conv_table_loaded[3];

        // The file continues with CP949 hangul and jamo tables, which UTF-8 text never uses.
        constexpr std::streamsize cp949_tables_size = (0xC8 - 0xB0 + 1) * (0xFE - 0xA1 + 1) * 2 + 60 + 42;
    }

    auto load_empire_text_convert_table(uint32_t empire_id, const char *file_name) -> bool {
        if (empire_id < 1 || empire_id > 3)
            return false;

        std::ifstream file(file_name, std::ios::binary);

        if (!file)
            return false;

        text_convert_table &text_conv_table_value = text_conv_table[empire_id - 1];

        const bool ok = file.read(text_conv_table_value.ac_upper, sizeof(text_conv_table_value.ac_upper)) &&
                        file.read(text_conv_table_value.ac_lower, sizeof(text_conv_table_value.ac_lower)) &&
                        file.ignore(cp949_tables_size).gcount() == cp949_tables_size;

        text_conv_table_loaded[empire_id - 1] = ok;
        return ok;
    }

    void convert_empire_text(uint32_t empire_id, char *text, size_t len, int pct) {
        if (empire_id < 1 || empire_id > 3 || len == 0 || !text_conv_table_loaded[empire_id - 1])
            return;

        static const char *asz_symbols2[] = {"¤", "§", "¶", "±", "×"};
        static const char *asz_symbols3[] = {"∫", "＃", "＄", "⊃", "∝"};

        const text_convert_table &text_conv_table_value = text_conv_table[empire_id - 1];

        auto *text_value = reinterpret_cast<uint8_t *>(text);
        size_t pos = 0;
        while (pos < len && text_value[pos] != '\0') {
            size_t seq = utf8_sequence_length(text_value[pos]);
            if (pos + seq > len)
                break;

            if (number(1, 100) > pct) {
                uint8_t &c = text_value[pos];
                if (seq == 1) {
                    if (c >= 'a' && c <= 'z')
                        c = text_conv_table_value.ac_lower[c - 'a'];
                    else if (c >= 'A' && c <= 'Z')
                        c = text_conv_table_value.ac_upper[c - 'A'];
                } else if (seq == 2)
                    memcpy(text_value + pos, asz_symbols2[number(0, 4)], 2);
                else if (seq == 3)
                    memcpy(text_value + pos, asz_symbols3[number(0, 4)], 3);
            }

            pos += seq;
        }
    }
}
