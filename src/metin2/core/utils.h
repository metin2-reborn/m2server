#pragma once

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <random>
#include <source_location>

namespace metin2::inline core {

    void core_dump(std::source_location where = std::source_location::current());

    inline auto is_digit(char ch) -> bool {
        return std::isdigit(static_cast<unsigned char>(ch)) != 0;
    }

    inline auto is_space(char ch) -> bool {
        return std::isspace(static_cast<unsigned char>(ch)) != 0;
    }

    void parse_token(char *src, char *token, char *value);
    void trim_and_lower(const char *src, char *dest, size_t dest_size);

    auto utf8_sequence_length(unsigned char c) -> size_t;
    auto utf8_truncated_size(const char *s, size_t len, size_t max) -> size_t;
    auto utf8_strlcpy(char *dst, const char *src, size_t size) -> size_t;
    auto utf8_strnlcpy(char *dst, size_t dst_size, const char *src, size_t src_max) -> size_t;

    auto random_engine() -> std::mt19937 &;
    auto number(int from, int to, std::source_location where = std::source_location::current()) -> int;
    auto fnumber(float from, float to) -> float;
    auto prob(int percent) -> bool;
    auto gauss(float mean, float sigma) -> float;
    auto secure_random_u32() -> uint32_t;

    auto get_float_time() -> float;
    auto get_dword_time() -> uint32_t;
    auto time_str(time_t ct) -> char *;
}
