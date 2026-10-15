#pragma once

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <source_location>

// Logs where it was called from, then dumps core from a forked child; the server keeps running.
void core_dump(std::source_location where = std::source_location::current());

// std::isdigit/std::isspace for one byte of UTF-8 text (bytes of multi-byte characters are neither).
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

// A random integer in [from, to]; swapped arguments are logged with the caller and corrected.
auto number(int from, int to, std::source_location where = std::source_location::current()) -> int;
auto fnumber(float from, float to) -> float;

auto thecore_random() -> uint32_t; // Random functions

auto get_float_time() -> float;
auto get_dword_time() -> uint32_t;
auto time_str(time_t ct) -> char *;
