#include "check.h"

#include <db/Config.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include <unistd.h>

namespace {
    // A config file in the temporary directory, removed again at the end of the test.
    struct temp_config {
        std::filesystem::path path;

        explicit temp_config(const std::string &text)
            : path(std::filesystem::temp_directory_path() / ("m2_config_test_" + std::to_string(::getpid()) + ".txt")) {
            std::ofstream(path) << text;
        }

        ~temp_config() {
            std::error_code error;
            std::filesystem::remove(path, error);
        }
    };
}

TEST(config_reads_values) {
    const temp_config file("// comment line\n"
                           "BIND_PORT = 15000\n"
                           "WELCOME_MSG = \"DB Server has been started\"\r\n"
                           "ITEM_ID_RANGE = 2000001 4000000\n");
    CConfig config;
    CHECK(config.LoadFile(file.path.c_str()));

    int port = 0;
    CHECK(config.GetValue("BIND_PORT", &port));
    CHECK_EQ(port, 15000);

    char message[64];
    CHECK(config.GetValue("WELCOME_MSG", message, sizeof(message)));
    CHECK_EQ(std::string(message), std::string("DB Server has been started"));

    uint32_t from = 0;
    uint32_t to = 0;
    CHECK(config.GetTwoValue("ITEM_ID_RANGE", &from, &to));
    CHECK_EQ(from, 2000001u);
    CHECK_EQ(to, 4000000u);
}

TEST(config_reports_missing_values) {
    const temp_config file("ONE_WORD = 5\n");
    CConfig config;
    CHECK(config.LoadFile(file.path.c_str()));

    int value = 0;
    CHECK(!config.GetValue("MISSING", &value));

    // GetTwoValue needs two words.
    uint32_t first = 0;
    uint32_t second = 0;
    CHECK(!config.GetTwoValue("ONE_WORD", &first, &second));
}

TEST(config_fails_on_missing_file) {
    CConfig config;
    CHECK(!config.LoadFile("/nonexistent/metin2/conf.txt"));
}
