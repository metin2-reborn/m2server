#include "log.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>

namespace metin2::log {
    namespace {
        namespace fs = std::filesystem;

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
            int day = -1;
        };

        auto shared() -> state & {
            static state instance;
            return instance;
        }

        std::atomic<level> threshold{level::info};

        auto local_now() -> std::tm {
            const std::time_t now = std::time(nullptr);
            std::tm result{};
            localtime_r(&now, &result);
            return result;
        }

        auto day_name(std::tm t, int days_offset) -> std::string {
            t.tm_mday += days_offset;
            t.tm_isdst = -1;
            std::mktime(&t);
            return std::format("{:04}{:02}{:02}", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
        }

        auto timestamp() -> std::string {
            const std::tm t = local_now();
            return std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour,
                               t.tm_min, t.tm_sec);
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

        void remove_old(state &s, const std::tm &now) {
            std::error_code error;
            if (!fs::is_directory(s.directory / "log", error))
                return;

            const std::string oldest_kept = day_name(now, -static_cast<int>(s.keep_days));
            for (const auto &entry : fs::directory_iterator(s.directory / "log", error)) {
                const std::string name = entry.path().filename().string();
                if (name.size() == 8 && name.find_first_not_of("0123456789") == std::string::npos && name <= oldest_kept)
                    fs::remove_all(entry.path(), error);
            }
        }

        void rotate_sink(state &s, sink &file, const std::tm &now) {
            if (file.hour == now.tm_hour)
                return;

            if (file.hour >= 0) {
                const fs::path folder = s.directory / "log" / day_name(now, now.tm_hour < file.hour ? -1 : 0);
                std::error_code error;
                fs::create_directories(folder, error);
                file.stream.close();
                fs::rename(file.path, folder / std::format("{}.{:02}", file.path.filename().string(), file.hour), error);
                file.open();
            }
            file.hour = now.tm_hour;
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

        s.directory = directory;
        s.all.emplace();
        s.all->path = directory / "syslog";
        s.errors.emplace();
        s.errors->path = directory / "syserr";

        if (!s.all->open() || !s.errors->open()) {
            std::cerr << std::format("cannot open log files in {}\n", directory.string());
            s.all.reset();
            s.errors.reset();
            return false;
        }

        const std::tm now = local_now();
        s.all->hour = s.errors->hour = now.tm_hour;
        s.day = now.tm_mday;
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

        const std::tm now = local_now();
        rotate_sink(s, *s.all, now);
        rotate_sink(s, *s.errors, now);

        if (now.tm_mday != s.day) {
            s.day = now.tm_mday;
            remove_old(s, now);
        }
    }

    void write(level severity, const std::source_location &where, std::string_view message) {
        const std::string stamp = timestamp();
        state &s = shared();
        const std::scoped_lock lock(s.mutex);

        if (!s.all) {
            std::cerr << std::format("{} [{}] {}\n", stamp, name_of(severity), message);
            return;
        }

        if (severity >= level::warn) {
            const std::string line = std::format("{} [{}] {} ({}:{}): {}\n", stamp, name_of(severity), where.function_name(),
                                                 file_name(where.file_name()), where.line(), message);
            s.errors->stream << line << std::flush;
            s.all->stream << line << std::flush;
        } else {
            s.all->stream << std::format("{} [{}] {}\n", stamp, name_of(severity), message) << std::flush;
        }

        if (threshold.load(std::memory_order_relaxed) == level::debug)
            std::cout << std::format("[{}] {}\n", name_of(severity), message) << std::flush;
    }
}
