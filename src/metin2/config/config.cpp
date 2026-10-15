#include "config.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace metin2::config {
    namespace {
        auto where(const toml::node &node) -> std::string {
            const toml::source_region &source = node.source();
            if (!source.path)
                return std::format("line {}", source.begin.line);
            return std::format("{}:{}", *source.path, source.begin.line);
        }

        class reader {
          public:
            reader(const toml::table *table, std::string path, std::vector<std::string> &errors)
                : table_(table), path_(std::move(path)), errors_(errors) {}

            [[nodiscard]] auto has(std::string_view key) const -> bool { return table_ && table_->contains(key); }

            template <typename T>
            void get(std::string_view key, T &out) {
                used_.emplace(key);
                const toml::node *node = table_ ? table_->get(key) : nullptr;
                if (node)
                    read(*node, name(key), out);
            }

            void range(std::string_view key, std::uint32_t &low, std::uint32_t &high) {
                used_.emplace(key);
                const toml::node *node = table_ ? table_->get(key) : nullptr;
                if (!node)
                    return;
                const toml::array *array = node->as_array();
                if (!array || array->size() != 2 || !(*array)[0].is_integer() || !(*array)[1].is_integer()) {
                    fail(*node, name(key), "an array of two integers");
                    return;
                }
                read((*array)[0], name(key), low);
                read((*array)[1], name(key), high);
            }

            auto table(std::string_view key) -> reader {
                used_.emplace(key);
                const toml::node *node = table_ ? table_->get(key) : nullptr;
                if (node && !node->is_table()) {
                    fail(*node, name(key), "a table");
                    node = nullptr;
                }
                return {node ? node->as_table() : nullptr, name(key), errors_};
            }

            void finish() const {
                if (!table_)
                    return;
                for (const auto &[key, node] : *table_)
                    if (!used_.contains(std::string(key.str())))
                        errors_.push_back(std::format("{}: unknown key {}", where(node), name(key.str())));
            }

            void fail(const toml::node &node, const std::string &key, std::string_view expected) {
                errors_.push_back(std::format("{}: {} must be {}", where(node), key, expected));
            }

          private:
            [[nodiscard]] auto name(std::string_view key) const -> std::string {
                return path_.empty() ? std::string(key) : std::format("{}.{}", path_, key);
            }

            void read(const toml::node &node, const std::string &key, bool &out) {
                if (const auto value = node.value_exact<bool>())
                    out = *value;
                else
                    fail(node, key, "true or false");
            }

            void read(const toml::node &node, const std::string &key, std::string &out) {
                if (const auto value = node.value_exact<std::string>())
                    out = *value;
                else
                    fail(node, key, "a string");
            }

            template <std::integral T>
            void read(const toml::node &node, const std::string &key, T &out) {
                const auto value = node.value_exact<std::int64_t>();
                if (!value || *value < static_cast<std::int64_t>(std::numeric_limits<T>::min()) ||
                    static_cast<std::uint64_t>(*value) > static_cast<std::uint64_t>(std::numeric_limits<T>::max()) ||
                    (*value < 0 && std::is_unsigned_v<T>)) {
                    fail(node, key, std::format("an integer from {} to {}", +std::numeric_limits<T>::min(), +std::numeric_limits<T>::max()));
                    return;
                }
                out = static_cast<T>(*value);
            }

            template <typename T>
            void read(const toml::node &node, const std::string &key, std::vector<T> &out) {
                const toml::array *array = node.as_array();
                if (!array) {
                    fail(node, key, "an array");
                    return;
                }
                out.clear();
                for (const toml::node &element : *array) {
                    T value{};
                    read(element, key, value);
                    out.push_back(std::move(value));
                }
            }

            void read(const toml::node &node, const std::string &key, log::level &out) {
                static constexpr std::pair<std::string_view, log::level> levels[] = {
                    {"debug", log::level::debug}, {"info", log::level::info}, {"warn", log::level::warn}, {"error", log::level::error}};
                const auto value = node.value_exact<std::string>();
                const auto *match = value ? std::ranges::find(levels, *value, &std::pair<std::string_view, log::level>::first) : std::end(levels);
                if (match == std::end(levels))
                    fail(node, key, R"("debug", "info", "warn" or "error")");
                else
                    out = match->second;
            }

            void read(const toml::node &node, const std::string &key, config::role &out) {
                const auto value = node.value_exact<std::string>();
                if (value == "db")
                    out = role::db;
                else if (value == "auth")
                    out = role::auth;
                else if (value == "game")
                    out = role::game;
                else
                    fail(node, key, R"("db", "auth" or "game")");
            }

            const toml::table *table_;
            std::string path_;
            std::vector<std::string> &errors_;
            std::set<std::string, std::less<>> used_;
        };

        void merge(toml::table &base, const toml::table &over) {
            for (const auto &[key, node] : over) {
                toml::node *existing = base.get(key);
                if (existing && existing->is_table() && node.is_table())
                    merge(*existing->as_table(), *node.as_table());
                else
                    base.insert_or_assign(key, node);
            }
        }

        auto read_settings(const toml::table &root) -> std::expected<settings, std::string> {
            settings s;
            std::vector<std::string> errors;
            reader top(&root, "", errors);

            if (!top.has("role"))
                errors.emplace_back("role is required (\"db\", \"auth\" or \"game\")");
            top.get("role", s.role);
            top.get("test_server", s.test_server);

            {
                reader r = top.table("log");
                r.get("level", s.log.level);
                r.get("keep_days", s.log.keep_days);
                r.finish();
            }
            {
                reader r = top.table("database");
                r.get("host", s.database.host);
                r.get("port", s.database.port);
                r.get("user", s.database.user);
                r.get("password", s.database.password);
                r.get("table_postfix", s.database.table_postfix);
                reader names = r.table("names");
                names.get("account", s.database.account);
                names.get("common", s.database.common);
                names.get("player", s.database.player);
                names.get("log", s.database.log);
                names.finish();
                r.finish();
            }
            {
                reader r = top.table("db");
                r.get("bind_ip", s.db.bind_ip);
                r.get("port", s.db.port);
                r.get("allowed_peers", s.db.allowed_peers);
                r.get("heart_fps", s.db.heart_fps);
                r.get("login_log", s.db.login_log);
                r.get("player_id_start", s.db.player_id_start);
                r.range("item_id_range", s.db.item_id_min, s.db.item_id_max);
                r.get("player_delete_level_limit", s.db.player_delete_level_limit);
                r.get("player_delete_level_limit_lower", s.db.player_delete_level_limit_lower);
                r.get("name_column", s.db.name_column);
                r.get("guild_war_power", s.db.guild_war_power);
                r.get("guild_war_handicap", s.db.guild_war_handicap);
                reader cache = r.table("cache");
                cache.get("player_flush_seconds", s.db.player_cache_flush_seconds);
                cache.get("item_flush_seconds", s.db.item_cache_flush_seconds);
                cache.get("pricelist_flush_seconds", s.db.pricelist_cache_flush_seconds);
                cache.get("flush_limit_per_second", s.db.cache_flush_limit_per_second);
                cache.finish();
                r.finish();
            }
            {
                reader r = top.table("server");
                r.get("hostname", s.server.hostname);
                r.get("channel", s.server.channel);
                r.get("bind_ip", s.server.bind_ip);
                r.get("proxy_ip", s.server.proxy_ip);
                r.get("port", s.server.port);
                r.get("p2p_port", s.server.p2p_port);
                r.get("p2p_allowed_hosts", s.server.p2p_allowed_hosts);
                r.get("db_host", s.server.db_host);
                r.get("db_port", s.server.db_port);
                r.get("passes_per_sec", s.server.passes_per_sec);
                r.get("ping_seconds", s.server.ping_seconds);
                r.get("user_limit", s.server.user_limit);
                reader admin = r.table("admin");
                admin.get("ips", s.server.admin.ips);
                admin.get("password", s.server.admin.password);
                admin.finish();
                r.finish();
            }
            {
                reader r = top.table("auth");
                r.get("master", s.auth.master);
                r.get("master_host", s.auth.master_host);
                r.get("master_port", s.auth.master_port);
                r.get("block_login_date", s.auth.block_login_date);
                r.finish();
            }
            {
                reader r = top.table("game");
                r.get("map_allow", s.game.map_allow);
                r.get("save_seconds", s.game.save_seconds);
                r.get("view_range", s.game.view_range);
                r.get("max_level", s.game.max_level);
                r.get("pk_protect_level", s.game.pk_protect_level);
                r.get("refuse_clients", s.game.refuse_clients);
                r.get("no_regen", s.game.no_regen);
                r.get("no_wander", s.game.no_wander);
                r.get("speed_server", s.game.speed_server);
                r.get("distribution_test_server", s.game.distribution_test_server);
                r.get("skill_disable", s.game.skill_disable);
                r.get("empire_whisper", s.game.empire_whisper);
                r.get("protect_normal_player", s.game.protect_normal_player);
                r.get("notice_battle_zone", s.game.notice_battle_zone);
                r.get("block_char_creation", s.game.block_char_creation);
                r.get("server_id", s.game.server_id);
                r.get("mall_url", s.game.mall_url);
                reader mark = r.table("mark");
                mark.get("server", s.game.mark.server);
                mark.get("min_level", s.game.mark.min_level);
                mark.finish();
                reader quest = r.table("quest");
                quest.get("dir", s.game.quest.dir);
                quest.get("object_dirs", s.game.quest.object_dirs);
                quest.finish();
                reader spam = r.table("spam");
                spam.get("block_duration", s.game.spam.block_duration);
                spam.get("block_score", s.game.spam.block_score);
                spam.get("reload_cycle", s.game.spam.reload_cycle);
                spam.get("block_max_level", s.game.spam.block_max_level);
                spam.finish();
                reader cheat = r.table("anti_cheat");
                cheat.get("sync_hack_limit", s.game.anti_cheat.sync_hack_limit);
                cheat.get("speed_hack_limit", s.game.anti_cheat.speed_hack_limit);
                cheat.get("speed_hack_bonus", s.game.anti_cheat.speed_hack_bonus);
                cheat.get("check_multihack", s.game.anti_cheat.check_multihack);
                cheat.finish();
                r.finish();
            }
            top.finish();

            if (s.role == role::db) {
                if (s.db.player_id_start <= 0)
                    errors.emplace_back("db.player_id_start is required");
                if (s.db.item_id_min == 0 || s.db.item_id_max <= s.db.item_id_min)
                    errors.emplace_back("db.item_id_range = [min, max] is required, with min < max");
                if (s.db.heart_fps <= 0)
                    errors.emplace_back("db.heart_fps must be positive");
            } else {
                if (s.server.hostname.empty())
                    errors.emplace_back("server.hostname is required");
                if (s.server.channel == 0)
                    errors.emplace_back("server.channel is required (1 or more)");
                if (s.server.passes_per_sec <= 0)
                    errors.emplace_back("server.passes_per_sec must be positive");
                if (s.role == role::auth && !s.auth.master && (s.auth.master_host.empty() || s.auth.master_port == 0))
                    errors.emplace_back("auth.master_host and auth.master_port are required when auth.master is false");
            }
            if (s.database.user.empty())
                errors.emplace_back("database.user is required");
            s.log.keep_days = std::clamp(s.log.keep_days, 1u, 90u);

            if (!errors.empty()) {
                std::string text;
                for (const std::string &error : errors)
                    text += error + "\n";
                return std::unexpected(text);
            }
            return s;
        }

        std::optional<settings> current_settings;
    }

    auto load(std::span<const std::filesystem::path> files) -> std::expected<settings, std::string> {
        toml::table root;
        for (const std::filesystem::path &file : files) {
            toml::parse_result result = toml::parse_file(file.string());
            if (!result) {
                const toml::parse_error &error = result.error();
                return std::unexpected(std::format("{}:{}:{}: {}\n", file.string(), error.source().begin.line,
                                                   error.source().begin.column, error.description()));
            }
            merge(root, result.table());
        }
        return read_settings(root);
    }

    auto parse(std::string_view text) -> std::expected<settings, std::string> {
        toml::parse_result result = toml::parse(text);
        if (!result) {
            const toml::parse_error &error = result.error();
            return std::unexpected(std::format("line {}:{}: {}\n", error.source().begin.line, error.source().begin.column, error.description()));
        }
        return read_settings(result.table());
    }

    void set_current(settings value) {
        current_settings = std::move(value);
    }

    auto current() -> const settings & {
        if (!current_settings) {
            static const settings defaults;
            return defaults;
        }
        return *current_settings;
    }

    auto role_name(config::role value) -> std::string_view {
        switch (value) {
        case role::db: return "db";
        case role::auth: return "auth";
        case role::game: return "game";
        }
        return "?";
    }
}
