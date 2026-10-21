#pragma once

#include <concepts>
#include <format>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace metin2::sql {
    struct raw {
        std::string_view text;

        explicit raw(std::string_view value) : text(value) {}
    };

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
            } else if constexpr (std::is_enum_v<T>) {
                return std::to_underlying(value);
            } else {
                static_assert(std::is_arithmetic_v<T>, "sql::format takes strings, numbers and sql::raw");
                return value;
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
struct std::formatter<metin2::sql::raw> : std::formatter<std::string_view> {
    auto format(const metin2::sql::raw &value, std::format_context &context) const {
        return std::formatter<std::string_view>::format(value.text, context);
    }
};

template <>
struct std::formatter<metin2::sql::detail::quoted> : std::formatter<std::string_view> {
    auto format(const metin2::sql::detail::quoted &value, std::format_context &context) const {
        return std::formatter<std::string_view>::format(value.text, context);
    }
};
