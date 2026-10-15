#pragma once

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstring>
#include <string>
#include <system_error>
#include <type_traits>

namespace metin2::inline common {

// Parses a decimal number like strtol/strtoul/strtof: leading whitespace and '+' are skipped,
// parsing stops at the first character that does not belong to the number, a negative value
// read into an unsigned type wraps, and anything unparsable reads as 0. Returns false only for
// a null or empty string.
template <typename T>
    requires std::is_arithmetic_v<T>
inline auto str_to_number(T &out, const char *in) -> bool {
    if (!in || !in[0]) {
        return false;
    }

    while (std::isspace(static_cast<unsigned char>(*in))) {
        ++in;
    }

    if (*in == '+') {
        ++in;
    }

    const char *end = in + std::strlen(in);

    if constexpr (std::is_floating_point_v<T>) {
        T value{};

        out = std::from_chars(in, end, value).ec == std::errc{} ? value : T{};
    } else {
        long long value = 0;

        out = std::from_chars(in, end, value).ec == std::errc{} ? static_cast<T>(value) : T{};
        if constexpr (std::is_same_v<T, bool>) {
            out = value != 0;
        }
    }
    return true;
}

inline void to_lower(std::string &str) {
    std::ranges::transform(str, str.begin(),
        [](unsigned char c) -> char {
            return static_cast<char>(std::tolower(c));
        });
}
}
