#include "check.h"

#include <metin2/log/log.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

namespace {
    namespace fs = std::filesystem;

    struct temp_log {
        fs::path directory;

        temp_log() : directory(fs::temp_directory_path() / ("m2_log_test_" + std::to_string(::getpid()))) {
            fs::remove_all(directory);
            fs::create_directories(directory);
            metin2::log::set_level(metin2::log::level::info);
            metin2::log::init(directory);
        }

        ~temp_log() {
            metin2::log::shutdown();
            std::error_code error;
            fs::remove_all(directory, error);
        }

        [[nodiscard]] auto read(const char *name) const -> std::string {
            std::ifstream file(directory / name);
            return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        }
    };

    auto count_lines(const std::string &text) -> std::size_t {
        std::size_t lines = 0;
        for (const char ch : text)
            lines += ch == '\n' ? 1 : 0;
        return lines;
    }

    enum class colour : std::uint8_t { red = 1,
        green = 2 };
}

TEST(log_writes_formatted_messages) {
    temp_log files;
    metin2::log::info("player {} entered map {:d}", "Hero", 41);
    metin2::log::error("cannot open {}", "item_proto");

    const std::string all = files.read("syslog");
    const std::string errors = files.read("syserr");
    CHECK(all.find("[info] player Hero entered map 41\n") != std::string::npos);
    CHECK(all.find("cannot open item_proto") != std::string::npos);
    CHECK(errors.find("[error]") != std::string::npos);
    CHECK(errors.find("cannot open item_proto") != std::string::npos);
    CHECK(errors.find("log_tests.cpp:") != std::string::npos);
    CHECK(errors.find("entered map") == std::string::npos);
}

TEST(log_filters_by_level) {
    temp_log files;
    metin2::log::debug("hidden {}", 1);
    metin2::log::set_level(metin2::log::level::debug);
    metin2::log::debug("shown {}", 2);
    metin2::log::set_level(metin2::log::level::info);

    const std::string all = files.read("syslog");
    CHECK(all.find("hidden") == std::string::npos);
    CHECK(all.find("[debug] shown 2") != std::string::npos);
    CHECK(!metin2::log::enabled(metin2::log::level::debug));
    CHECK(metin2::log::enabled(metin2::log::level::warn));
}

TEST(log_formats_enums_and_bit_fields_as_numbers) {
    temp_log files;
    struct packed {
        unsigned value : 4;
    } bits{9};
    volatile int counter = 7;

    metin2::log::info("values {} {} {}", colour::green, bits.value, counter);
    CHECK(files.read("syslog").find("values 2 9 7\n") != std::string::npos);
}

TEST(log_keeps_lines_whole_across_threads) {
    temp_log files;
    constexpr int threads = 8;
    constexpr int per_thread = 200;

    std::vector<std::thread> workers;
    for (int t = 0; t < threads; ++t)
        workers.emplace_back([t] {
            for (int i = 0; i < per_thread; ++i)
                metin2::log::info("thread {:d} message {:d}", t, i);
        });
    for (auto &worker : workers)
        worker.join();

    const std::string all = files.read("syslog");
    CHECK_EQ(count_lines(all), static_cast<std::size_t>(threads * per_thread));
    CHECK(all.find("thread 7 message 199\n") != std::string::npos);
}
