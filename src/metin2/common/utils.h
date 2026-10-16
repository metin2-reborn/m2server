#pragma once

#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace metin2::inline common {

    template <typename T>
        requires std::is_arithmetic_v<T>
    auto parse(std::string_view text) -> std::optional<T> {
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
            text.remove_prefix(1);
        if (text.starts_with('+'))
            text.remove_prefix(1);

        const char *begin = text.data();
        const char *end = begin + text.size();

        if constexpr (std::is_floating_point_v<T>) {
            T value{};
            if (std::from_chars(begin, end, value).ec != std::errc{})
                return std::nullopt;
            return value;
        } else if constexpr (std::is_unsigned_v<T> && !std::is_same_v<T, bool>) {
            if (!text.starts_with('-')) {
                unsigned long long value = 0;
                if (std::from_chars(begin, end, value).ec != std::errc{})
                    return std::nullopt;
                return static_cast<T>(value);
            }
            long long value = 0;
            if (std::from_chars(begin, end, value).ec != std::errc{})
                return std::nullopt;
            return static_cast<T>(value);
        } else {
            long long value = 0;
            if (std::from_chars(begin, end, value).ec != std::errc{})
                return std::nullopt;
            if constexpr (std::is_same_v<T, bool>)
                return value != 0;
            else
                return static_cast<T>(value);
        }
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    auto str_to_number(T &out, const char *in) -> bool {
        if (!in || !in[0])
            return false;

        const auto value = parse<T>(in);
        out = value.value_or(T{});
        return value.has_value();
    }

    inline void to_lower(std::string &str) {
        std::ranges::transform(str, str.begin(),
            [](unsigned char c) -> char {
                return static_cast<char>(std::tolower(c));
            });
    }
}
