#pragma once

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <random>
#include <source_location>
#include <span>
#include <string>
#include <string_view>

namespace metin2::inline core {

    void core_dump(std::source_location where = std::source_location::current());

    inline auto is_digit(char ch) -> bool {
        return std::isdigit(static_cast<unsigned char>(ch)) != 0;
    }

    inline auto is_space(char ch) -> bool {
        return std::isspace(static_cast<unsigned char>(ch)) != 0;
    }

    constexpr auto ascii_lower(char ch) -> char {
        return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch;
    }

    void trim_and_lower(std::string_view src, std::span<char> dest);
    void trim_and_lower(const char *src, std::span<char> dest);

    auto utf8_sequence_length(unsigned char c) -> size_t;
    auto utf8_truncated_size(std::string_view s, size_t max) -> size_t;
    auto utf8_strlcpy(std::span<char> dst, std::string_view src) -> size_t;
    auto utf8_strnlcpy(std::span<char> dst, std::span<const char> src) -> size_t;

    auto random_engine() -> std::mt19937 &;
    auto number(int from, int to, std::source_location where = std::source_location::current()) -> int;
    auto fnumber(float from, float to) -> float;
    auto prob(int percent) -> bool;
    auto gauss(float mean, float sigma) -> float;
    auto secure_random_u32() -> uint32_t;

    auto get_float_time() -> float;
    auto get_dword_time() -> uint32_t;
    auto time_str(time_t ct) -> std::string;
}
