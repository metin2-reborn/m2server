#include "check.h"

#include <metin2/config/config.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>

namespace {
    namespace fs = std::filesystem;
    using metin2::config::role;

    auto contains(const std::string &text, const std::string &part) -> bool {
        return text.find(part) != std::string::npos;
    }

    struct temp_dir {
        fs::path path = fs::temp_directory_path() / ("m2_config_test_" + std::to_string(::getpid()));

        temp_dir() { fs::create_directories(path); }
        ~temp_dir() {
            std::error_code error;
            fs::remove_all(path, error);
        }

        auto write(const char *name, const std::string &text) const -> fs::path {
            std::ofstream(path / name) << text;
            return path / name;
        }
    };

    constexpr const char *game_config = R"(
role = "game"

[database]
user = "metin2"
password = "secret"

[server]
hostname = "first"
channel = 1
port = 13000
p2p_allowed_hosts = ["auth", "channel1-first"]

[game]
map_allow = [1, 21, 41]
max_level = 105

[game.spam]
block_score = 50
)";
}

TEST(config_reads_a_game_file) {
    const auto settings = metin2::config::parse(game_config);
    CHECK(settings.has_value());
    if (!settings)
        return;

    CHECK(settings->role == role::game);
    CHECK_EQ(settings->database.user, std::string("metin2"));
    CHECK_EQ(settings->server.hostname, std::string("first"));
    CHECK_EQ(settings->server.channel, 1);
    CHECK_EQ(settings->server.p2p_allowed_hosts.size(), std::size_t{2});
    CHECK_EQ(settings->game.map_allow.size(), std::size_t{3});
    CHECK_EQ(settings->game.max_level, 105);
    CHECK_EQ(settings->game.spam.block_score, 50u);
    CHECK_EQ(settings->game.spam.block_duration, 900u);
    CHECK_EQ(settings->server.passes_per_sec, 25);
    CHECK(settings->game.empire_whisper);
}

TEST(config_reads_a_db_file) {
    const auto settings = metin2::config::parse(R"(
role = "db"
[database]
user = "metin2"
[db]
port = 15000
player_id_start = 3
item_id_range = [10000001, 20000000]
[db.cache]
item_flush_seconds = 60
)");
    CHECK(settings.has_value());
    if (!settings)
        return;
    CHECK(settings->role == role::db);
    CHECK_EQ(settings->db.item_id_min, 10000001u);
    CHECK_EQ(settings->db.item_id_max, 20000000u);
    CHECK_EQ(settings->db.item_cache_flush_seconds, 60);
    CHECK_EQ(settings->db.player_cache_flush_seconds, 420);
}

TEST(config_reports_required_keys) {
    const auto settings = metin2::config::parse("role = \"db\"\n[database]\nuser = \"x\"\n");
    CHECK(!settings.has_value());
    if (settings)
        return;
    CHECK(contains(settings.error(), "db.player_id_start is required"));
    CHECK(contains(settings.error(), "db.item_id_range"));

    const auto no_role = metin2::config::parse("[database]\nuser = \"x\"\n");
    CHECK(!no_role.has_value());
}

TEST(config_reports_wrong_types_and_unknown_keys) {
    const auto settings = metin2::config::parse(R"(
role = "game"
[database]
user = "metin2"
[server]
hostname = "first"
channel = 300
port = "13000"
prot = 1
)");
    CHECK(!settings.has_value());
    if (settings)
        return;
    CHECK(contains(settings.error(), "server.channel must be an integer from 0 to 255"));
    CHECK(contains(settings.error(), "server.port must be an integer"));
    CHECK(contains(settings.error(), "unknown key server.prot"));
    CHECK(contains(settings.error(), "line "));
}

TEST(config_merges_files_in_order) {
    temp_dir dir;
    const std::vector<fs::path> files = {
        dir.write("server.toml", game_config),
        dir.write("runtime.toml", "[database]\nhost = \"mariadb\"\npassword = \"from-env\"\n[server]\nbind_ip = \"10.0.0.5\"\n"),
    };

    const auto settings = metin2::config::load(files);
    CHECK(settings.has_value());
    if (!settings)
        return;
    CHECK_EQ(settings->database.host, std::string("mariadb"));
    CHECK_EQ(settings->database.password, std::string("from-env"));
    CHECK_EQ(settings->database.user, std::string("metin2"));
    CHECK_EQ(settings->server.bind_ip, std::string("10.0.0.5"));
    CHECK_EQ(settings->server.hostname, std::string("first"));
}

TEST(config_reports_syntax_errors_with_position) {
    temp_dir dir;
    const std::vector<fs::path> files = {dir.write("server.toml", "role = \"game\"\n[server\nhostname = 1\n")};
    const auto settings = metin2::config::load(files);
    CHECK(!settings.has_value());
    if (!settings)
        CHECK(contains(settings.error(), "server.toml:2:"));
}

TEST(config_accepts_the_shipped_files) {
    temp_dir dir;
    const fs::path runtime = dir.write("runtime.toml", R"(
[database]
host = "mariadb"
user = "metin2"
password = "secret"
)");
    for (const char *role_dir : {"db", "auth", "channel1-first"}) {
        const std::vector<fs::path> files = {fs::path(M2_SOURCE_DIR) / "dist/config" / role_dir / "server.toml", runtime};
        const auto settings = metin2::config::load(files);
        CHECK(settings.has_value());
        if (!settings)
            std::fprintf(stderr, "%s: %s", role_dir, settings.error().c_str());
    }
}
