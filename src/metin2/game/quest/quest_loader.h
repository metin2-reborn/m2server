#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace metin2::game::quest {

    // The .quest files translated to Lua, keyed like the object/ tree the old quest compiler wrote:
    //   state/<quest>                                   state numbers and the quest's functions
    //   begin_condition/<quest>                         the quest's "with" condition
    //   <npc>/<event>/<quest>.<state>                   handler code
    //   <npc>/<event>/<quest>.<state>.<n>.script|when|arg   handlers with an argument (chat menus, targets)
    class quest_scripts {
      public:
        void add(const std::string &dir, const std::string &name, std::string content);

        [[nodiscard]] auto list(std::string_view dir) const -> std::vector<std::string>;
        [[nodiscard]] auto find(std::string_view dir, std::string_view name) const -> const std::string *;
        [[nodiscard]] auto has_npc(std::string_view npc) const -> bool;

        [[nodiscard]] auto entries() const -> const std::map<std::string, std::map<std::string, std::string>, std::less<>> & { return dirs_; }

        // Functions the scripts call, for a check against the Lua state once everything is loaded.
        std::set<std::string> called_functions;

      private:
        std::map<std::string, std::map<std::string, std::string>, std::less<>> dirs_;
    };

    // Translates every *.quest file directly in dir (subdirectories, such as disabled/, are not read).
    auto load_quest_scripts(const std::filesystem::path &dir) -> std::expected<quest_scripts, std::string>;

    // Translates one quest source; file is only used in messages.
    auto translate_quest(std::string_view source, const std::string &file, quest_scripts &out) -> std::expected<void, std::string>;
}
