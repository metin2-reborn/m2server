#pragma once

#include "settings.h"

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace metin2::config {
    auto load(const std::filesystem::path &file) -> std::expected<settings, std::string>;
    auto parse(std::string_view text) -> std::expected<settings, std::string>;

    void set_current(settings value);
    [[nodiscard]] auto current() -> const settings &;

    [[nodiscard]] inline auto current_role() -> config::role { return current().role; }
    [[nodiscard]] inline auto test_server() -> bool { return current().test_server; }
    [[nodiscard]] inline auto database() -> const database_settings & { return current().database; }
    [[nodiscard]] inline auto db() -> const db_settings & { return current().db; }
    [[nodiscard]] inline auto server() -> const server_settings & { return current().server; }
    [[nodiscard]] inline auto auth() -> const auth_settings & { return current().auth; }
    [[nodiscard]] inline auto game() -> const game_settings & { return current().game; }
    [[nodiscard]] inline auto is_auth() -> bool { return current().role == role::auth; }

    [[nodiscard]] auto role_name(config::role value) -> std::string_view;
}
