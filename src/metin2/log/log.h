#pragma once

#include <cstdint>
#include <filesystem>
#include <format>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>

// The process log: `syslog` gets every message at or above the threshold, `syserr` warnings and errors
// with the place they came from. Both rotate hourly into `log/YYYYMMDD/`. Thread safe; messages logged
// before init() go to stderr.
//
//   metin2::log::info("player {} entered map {}", name, map_index);
//   metin2::log::error("cannot open {}", path);
namespace metin2::log {
    enum class level : std::uint8_t {
        debug,
        info,
        warn,
        error,
    };

    // A format string together with the place of the call, so the variadic functions below can take a
    // defaulted std::source_location after their arguments.
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
        // Enums are logged as their numbers, like printf did.
        template <typename T, bool = std::is_enum_v<T>>
        struct loggable_type {
            // Volatile values (counters written by signal handlers) are copied.
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

    // The message text of a format string and its arguments.
    template <typename... Args>
    auto format(const format_string<Args...> &format, const Args &...args) -> std::string {
        return std::format(format.format, detail::loggable(args)...);
    }

    // Opens `syslog` and `syserr` in directory; dated folders older than the retention are removed.
    auto init(const std::filesystem::path &directory) -> bool;
    void shutdown();

    // Days of rotated logs to keep (default 3).
    void set_keep_days(unsigned int days);

    // Messages below the threshold are dropped (default: info).
    void set_level(level threshold);
    [[nodiscard]] auto enabled(level severity) -> bool;

    // Rotates the files when the hour changed; call it from the main loop.
    void rotate();

    // Writes a finished message.
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
