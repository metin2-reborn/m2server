#include "log.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <print>
#include <string>
#include <system_error>

namespace metin2::log {
    namespace {
        namespace fs = std::filesystem;
        namespace chrono = std::chrono;

        struct sink {
            fs::path path;
            std::ofstream stream;
            int hour = -1;

            auto open() -> bool {
                stream.close();
                stream.clear();
                stream.open(path, std::ios::app);
                return stream.is_open();
            }
        };

        struct state {
            std::mutex mutex;
            fs::path directory;
            unsigned int keep_days = 3;
            std::optional<sink> all;
            std::optional<sink> errors;
            chrono::local_days day{};
        };

        auto shared() -> state & {
            static state instance;
            return instance;
        }

        std::atomic<level> threshold{level::info};

        auto local_now() -> chrono::local_seconds {
            return chrono::floor<chrono::seconds>(chrono::current_zone()->to_local(chrono::system_clock::now()));
        }

        auto day_of(chrono::local_seconds t) -> chrono::local_days {
            return chrono::floor<chrono::days>(t);
        }

        auto hour_of(chrono::local_seconds t) -> int {
            return static_cast<int>((t - day_of(t)) / chrono::hours(1));
        }

        auto day_name(chrono::local_days day) -> std::string {
            return std::format("{:%Y%m%d}", day);
        }

        auto timestamp() -> std::string {
            return std::format("{:%Y-%m-%d %H:%M:%S}", local_now());
        }

        constexpr std::array<std::string_view, 4> names{"debug", "info", "warn", "error"};

        auto name_of(level severity) -> std::string_view {
            return names[static_cast<std::size_t>(severity)];
        }

        auto file_name(const char *path) -> std::string_view {
            const std::string_view text(path);
            const auto slash = text.rfind('/');
            return slash == std::string_view::npos ? text : text.substr(slash + 1);
        }

        void remove_old(state &s, chrono::local_seconds now) {
            std::error_code error;
            if (!fs::is_directory(s.directory, error))
                return;

            const std::string oldest_kept = day_name(day_of(now) - chrono::days(s.keep_days));
            for (const auto &entry : fs::directory_iterator(s.directory, error)) {
                const std::string name = entry.path().filename().string();
                if (name.size() == 8 && name.find_first_not_of("0123456789") == std::string::npos && name <= oldest_kept)
                    fs::remove_all(entry.path(), error);
            }
        }

        void rotate_sink(state &s, sink &file, chrono::local_seconds now) {
            const int hour = hour_of(now);
            if (file.hour == hour)
                return;

            if (file.hour >= 0) {
                const fs::path folder = s.directory / day_name(day_of(now) - chrono::days(hour < file.hour ? 1 : 0));
                std::error_code error;
                fs::create_directories(folder, error);
                file.stream.close();
                fs::rename(file.path, folder / std::format("{}.{:02}", file.path.filename().string(), file.hour), error);
                file.open();
            }
            file.hour = hour;
        }
    }

    void set_keep_days(unsigned int days) {
        state &s = shared();
        const std::scoped_lock lock(s.mutex);
        s.keep_days = days;
    }

    auto init(const fs::path &directory) -> bool {
        state &s = shared();
        const std::scoped_lock lock(s.mutex);

        std::error_code error;
        fs::create_directories(directory, error);

        s.directory = directory;
        s.all.emplace();
        s.all->path = directory / "syslog";
        s.errors.emplace();
        s.errors->path = directory / "syserr";

        if (!s.all->open() || !s.errors->open()) {
            std::println(std::cerr, "cannot open log files in {}", directory.string());
            s.all.reset();
            s.errors.reset();
            return false;
        }

        const chrono::local_seconds now = local_now();
        s.all->hour = s.errors->hour = hour_of(now);
        s.day = day_of(now);
        remove_old(s, now);
        return true;
    }

    void shutdown() {
        state &s = shared();
        const std::scoped_lock lock(s.mutex);
        s.all.reset();
        s.errors.reset();
    }

    void set_level(level value) {
        threshold.store(value, std::memory_order_relaxed);
    }

    auto enabled(level severity) -> bool {
        return severity >= threshold.load(std::memory_order_relaxed);
    }

    void rotate() {
        state &s = shared();
        const std::scoped_lock lock(s.mutex);
        if (!s.all)
            return;

        const chrono::local_seconds now = local_now();
        rotate_sink(s, *s.all, now);
        rotate_sink(s, *s.errors, now);

        if (day_of(now) != s.day) {
            s.day = day_of(now);
            remove_old(s, now);
        }
    }

    void write(level severity, const std::source_location &where, std::string_view message) {
        const std::string stamp = timestamp();
        state &s = shared();
        const std::scoped_lock lock(s.mutex);

        if (!s.all) {
            std::println(std::cerr, "{} [{}] {}", stamp, name_of(severity), message);
            return;
        }

        if (severity >= level::warn) {
            const std::string line = std::format("{} [{}] {} ({}:{}): {}\n", stamp, name_of(severity), where.function_name(),
                file_name(where.file_name()), where.line(), message);
            s.errors->stream << line << std::flush;
            s.all->stream << line << std::flush;
        } else {
            std::println(s.all->stream, "{} [{}] {}", stamp, name_of(severity), message);
            s.all->stream.flush();
        }

        if (threshold.load(std::memory_order_relaxed) == level::debug) {
            std::println(std::cout, "[{}] {}", name_of(severity), message);
            std::cout.flush();
        }
    }
}
