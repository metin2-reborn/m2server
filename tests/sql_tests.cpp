#include "check.h"

#include <metin2/sql/query.h>

#include <cstdint>
#include <string>
#include <string_view>

using namespace metin2;

TEST(escape_handles_special_characters) {
    CHECK_EQ(sql::escape("plain"), std::string("plain"));
    CHECK_EQ(sql::escape("O'Brien"), std::string("O\\'Brien"));
    CHECK_EQ(sql::escape("a\\b"), std::string("a\\\\b"));
    CHECK_EQ(sql::escape("\"q\""), std::string("\\\"q\\\""));
    CHECK_EQ(sql::escape(std::string_view("nul\0x", 5)), std::string("nul\\0x"));
    CHECK_EQ(sql::escape("line\r\nend\x1a"), std::string("line\\r\\nend\\Z"));
    CHECK_EQ(sql::escape("검사"), std::string("검사"));
}

TEST(format_quotes_strings) {
    const std::string name = "x' OR '1'='1";
    CHECK_EQ(sql::format("SELECT id FROM player WHERE name={}", name),
        std::string("SELECT id FROM player WHERE name='x\\' OR \\'1\\'=\\'1'"));

    const char login[16] = "admin";
    CHECK_EQ(sql::format("{}", login), std::string("'admin'"));
    CHECK_EQ(sql::format("{}", "literal"), std::string("'literal'"));
    CHECK_EQ(sql::format("{}", std::string_view("view")), std::string("'view'"));
    CHECK_EQ(sql::format("{}", ""), std::string("''"));
}

TEST(format_writes_numbers) {
    const uint32_t id = 4000000000u;
    const int8_t small = -5;
    const uint8_t byte = 200;
    CHECK_EQ(sql::format("{} {} {} {} {}", id, small, byte, true, 1.5), std::string("4000000000 -5 200 1 1.5"));

    enum class grade : uint8_t { master = 3 };
    CHECK_EQ(sql::format("{}", grade::master), std::string("3"));
}

TEST(format_keeps_raw_text) {
    const std::string postfix = "_test";
    CHECK_EQ(sql::format("SELECT * FROM item{} WHERE owner_id={}", sql::raw(postfix), 7u),
        std::string("SELECT * FROM item_test WHERE owner_id=7"));
    CHECK_EQ(sql::format("{}", sql::raw("")), std::string(""));
}
