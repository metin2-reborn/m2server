#pragma once

#include <concepts>
#include <format>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

// Builds SQL statements with std::format syntax. String arguments are escaped and wrapped in single
// quotes, so values from players can be passed as they are; numbers are written out. Trusted text
// that must not be quoted (the table-name suffix, column lists) goes through sql::raw.
//
//   sql::format("SELECT id FROM player{} WHERE name={}", sql::raw(postfix), name)
//
// Escaping assumes what the server's connections use: a UTF-8 character set and the default
// sql_mode, in which backslash escapes are active.
namespace sql {
    struct raw {
        std::string_view text;

        explicit raw(std::string_view value) : text(value) {}
    };

    // The contents of a quoted string literal for `value` (without the quotes).
    auto escape(std::string_view value) -> std::string;

    namespace detail {
        struct quoted {
            std::string text;
        };

        template <typename T>
        concept string_like = std::is_convertible_v<const T &, std::string_view>;

        template <typename T>
        auto wrap(const T &value) {
            if constexpr (std::same_as<T, raw>) {
                return value;
            } else if constexpr (string_like<T>) {
                return quoted{"'" + escape(std::string_view(value)) + "'"};
            } else if constexpr (std::same_as<T, bool> || std::same_as<T, char>) {
                return static_cast<int>(value);
            } else {
                static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T>, "sql::format takes strings, numbers and sql::raw");
                if constexpr (std::is_enum_v<T>) {
                    return static_cast<std::underlying_type_t<T>>(value);
                } else {
                    return value;
                }
            }
        }

        template <typename T>
        using wrapped = decltype(wrap(std::declval<const std::remove_cvref_t<T> &>()));

        template <typename... Args>
        auto vformat(std::string_view format, Args &&...args) -> std::string {
            return std::vformat(format, std::make_format_args(args...));
        }
    }

    template <typename... Args>
    auto format(std::format_string<detail::wrapped<Args>...> format, const Args &...args) -> std::string {
        return detail::vformat(format.get(), detail::wrap(args)...);
    }
}

template <>
struct std::formatter<sql::raw> : std::formatter<std::string_view> {
    auto format(const sql::raw &value, std::format_context &context) const {
        return std::formatter<std::string_view>::format(value.text, context);
    }
};

template <>
struct std::formatter<sql::detail::quoted> : std::formatter<std::string_view> {
    auto format(const sql::detail::quoted &value, std::format_context &context) const {
        return std::formatter<std::string_view>::format(value.text, context);
    }
};
