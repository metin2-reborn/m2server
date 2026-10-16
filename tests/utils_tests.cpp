#include "check.h"

#include <metin2/common/utils.h>
#include <metin2/core/utils.h>

#include <cstdint>
#include <cstring>
#include <ctime>
#include <limits>
#include <string>

using namespace metin2;

TEST(str_to_number_parses_like_strtol) {
    int value = -1;
    CHECK(str_to_number(value, "42"));
    CHECK_EQ(value, 42);

    CHECK(str_to_number(value, "  +7 apples"));
    CHECK_EQ(value, 7);

    CHECK(!str_to_number(value, "abc"));
    CHECK_EQ(value, 0);

    CHECK(!str_to_number(value, ""));
    CHECK(!str_to_number(value, nullptr));
}

TEST(str_to_number_wraps_negative_unsigned) {
    uint8_t byte = 0;
    CHECK(str_to_number(byte, "-1"));
    CHECK_EQ(byte, uint8_t{255});

    bool flag = false;
    CHECK(str_to_number(flag, "2"));
    CHECK(flag);
}

TEST(str_to_number_reads_floats) {
    float value = 0;
    CHECK(str_to_number(value, "3.5"));
    CHECK_EQ(value, 3.5f);
}

TEST(to_lower_is_ascii_only) {
    std::string text = "MiXeD 가";
    to_lower(text);
    CHECK_EQ(text, std::string("mixed 가"));
}

TEST(trim_and_lower_trims_and_truncates) {
    char buffer[16];
    trim_and_lower("  HeLLo World \t", buffer);
    CHECK_EQ(std::string(buffer), std::string("hello world"));

    char small[4];
    trim_and_lower("ABCDEF", small);
    CHECK_EQ(std::string(small), std::string("abc"));

    trim_and_lower(nullptr, buffer);
    CHECK_EQ(std::string(buffer), std::string(""));

    trim_and_lower(std::string_view("\tMiXeD Case  "), buffer);
    CHECK_EQ(std::string(buffer), std::string("mixed case"));
}

TEST(time_str_matches_asctime_slice) {
    for (const std::time_t t : {std::time_t{0}, std::time_t{1700000000}, std::time_t{1717200000}}) {
        std::string expected = std::asctime(std::localtime(&t));
        expected = expected.substr(4, expected.size() - 4 - 6);
        CHECK_EQ(time_str(t), expected);
    }
}

TEST(utf8_truncation_keeps_whole_characters) {
    const char *text = "가나다";
    CHECK_EQ(utf8_sequence_length(static_cast<unsigned char>(text[0])), size_t{3});
    CHECK_EQ(utf8_truncated_size(text, 7), size_t{6});
    CHECK_EQ(utf8_truncated_size(text, 2), size_t{0});

    char buffer[8];
    CHECK_EQ(utf8_strlcpy(buffer, text), size_t{9});
    CHECK_EQ(std::string(buffer), std::string("가나"));
}

TEST(utf8_strnlcpy_bounds_unterminated_fields) {
    const char field[4] = {'a', 'b', 'c', 'd'};
    char buffer[16];
    CHECK_EQ(utf8_strnlcpy(buffer, field), size_t{4});
    CHECK_EQ(std::string(buffer), std::string("abcd"));

    char small[5];
    CHECK_EQ(utf8_strnlcpy(small, {"가나", 6}), size_t{3});
    CHECK_EQ(std::string(small), std::string("가"));

    CHECK_EQ(utf8_strnlcpy(buffer, {"hi\0junk", 7}), size_t{2});
    CHECK_EQ(std::string(buffer), std::string("hi"));

    CHECK_EQ(utf8_strnlcpy({buffer, 0}, {"x", 1}), size_t{0});
}

TEST(number_stays_in_range) {
    for (int i = 0; i < 1000; ++i) {
        const int value = number(1, 6);
        CHECK(value >= 1 && value <= 6);
    }

    const int swapped = number(10, 5);
    CHECK(swapped >= 5 && swapped <= 10);
    CHECK_EQ(number(3, 3), 3);
}

TEST(parse_handles_wide_unsigned_values) {
    CHECK_EQ(parse<uint64_t>("18446744073709551615").value_or(0), UINT64_MAX);
    CHECK_EQ(parse<int>(" -12x").value_or(0), -12);
    CHECK(!parse<int>("x").has_value());
    CHECK_EQ(parse<uint8_t>("300").value_or(0), uint8_t{44});
}

TEST(random_helpers_stay_in_range) {
    for (int i = 0; i < 1000; ++i) {
        const int n = number(-3, 3);
        CHECK(n >= -3 && n <= 3);
        const float f = fnumber(0.5f, 1.5f);
        CHECK(f >= 0.5f && f <= 1.5f);
        CHECK(!prob(0));
        CHECK(prob(100));
    }
    CHECK_EQ(number(std::numeric_limits<int>::min(), std::numeric_limits<int>::min()), std::numeric_limits<int>::min());
}
