#pragma once

#include <cstdint>
#include <filesystem>
#include <format>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>

namespace metin2::log {
    enum class level : std::uint8_t {
        debug,
        info,
        warn,
        error,
    };

    template <typename... Args>
    struct format_with_location {
        std::format_string<Args...> format;
        std::source_location location;

        template <typename String>
            requires std::is_convertible_v<const String &, std::string_view>
        consteval format_with_location(const String &text, std::source_location where = std::source_location::current())
            : format(text), location(where) {}
    };

    namespace detail {
        template <typename T, bool = std::is_enum_v<T>>
        struct loggable_type {
            using type = std::conditional_t<std::is_volatile_v<T>, std::remove_cv_t<T>, const T &>;
        };

        template <typename T>
        struct loggable_type<T, true> {
            using type = std::underlying_type_t<T>;
        };

        template <typename T>
        using loggable_t = typename loggable_type<T>::type;

        template <typename T>
        constexpr auto loggable(const T &value) -> loggable_t<T> {
            if constexpr (std::is_enum_v<T>) {
                return static_cast<std::underlying_type_t<T>>(value);
            } else {
                return value;
            }
        }
    }

    template <typename... Args>
    using format_string = format_with_location<detail::loggable_t<std::type_identity_t<Args>>...>;

    template <typename... Args>
    auto format(const format_string<Args...> &format, const Args &...args) -> std::string {
        return std::format(format.format, detail::loggable(args)...);
    }

    auto init(const std::filesystem::path &directory) -> bool;
    void shutdown();

    void set_keep_days(unsigned int days);

    void set_level(level value);
    [[nodiscard]] auto enabled(level severity) -> bool;

    void rotate();

    void write(level severity, const std::source_location &where, std::string_view message);

    template <typename... Args>
    void debug(format_string<Args...> format, const Args &...args) {
        if (enabled(level::debug))
            write(level::debug, format.location, log::format(format, args...));
    }

    template <typename... Args>
    void info(format_string<Args...> format, const Args &...args) {
        if (enabled(level::info))
            write(level::info, format.location, log::format(format, args...));
    }

    template <typename... Args>
    void warn(format_string<Args...> format, const Args &...args) {
        if (enabled(level::warn))
            write(level::warn, format.location, log::format(format, args...));
    }

    template <typename... Args>
    void error(format_string<Args...> format, const Args &...args) {
        if (enabled(level::error))
            write(level::error, format.location, log::format(format, args...));
    }
}
