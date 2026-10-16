#include <metin2/config/config.h>
#include <metin2/game/text/locale_service.h>

#include <metin2/game/app/config.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/mob_manager.h>
#include <metin2/game/combat/skill_power.h>
#include <metin2/game/empire/empire_text_convert.h>
#include <metin2/game/text/banword.h>
#include <metin2/game/text/locale.hpp>

#include <metin2/log/log.h>
#include <metin2/proto/length.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <set>
#include <string>

namespace metin2::game::inline text {

    static const std::string service_base_path = "locale/english";
    static const std::string service_map_path = "locale/english/map";

    const std::string db_charset = "utf8mb4";
    std::string locale_filename = "locale/english/locale_string.txt";

    static_assert(PLAYER_PER_ACCOUNT == 4);

    static auto check_name_independent(const char *str) -> int {
        if (banword_manager::instance().check_string(str, strlen(str)))
            return 0;

        char szTmp[256];
        str_lower(str, szTmp, sizeof(szTmp));

        if (mob_manager::instance().get(szTmp, false))
            return 0;

        return 1;
    }

    auto check_name(const char *str) -> int {
        const char *tmp;

        if (!str || !*str)
            return 0;

        if (strlen(str) < 2)
            return 0;

        for (tmp = str; *tmp; ++tmp) {
            if (std::isalnum(static_cast<unsigned char>(*tmp)))
                continue;
            else
                return 0;
        }

        return check_name_independent(str);
    }

    void locale_service_load_locale_string_file() {
        if (locale_filename.empty())
            return;

        if (metin2::config::is_auth())
            return;

        fprintf(stderr, "LocaleService %s\n", locale_filename.c_str());

        locale_init(locale_filename.c_str());
    }

    void locale_service_load_empire_text_convert_tables() {
        for (int iEmpire = 1; iEmpire <= 3; ++iEmpire) {
            const std::string szFileName = std::format("{}/lang{}.cvt", locale_service_get_base_path(), iEmpire);
            metin2::log::info("Loading empire text conversion table {}", szFileName);

            load_empire_text_convert_table(iEmpire, szFileName.c_str());
        }
    }

    auto locale_service_init(const std::string &rst_service_name) -> bool {
        if (rst_service_name != "english") {
            metin2::log::error("Unsupported locale {} (only english)", rst_service_name);
            return false;
        }

        quest_dir = service_base_path + "/quest";
        quest_object_dir.clear();
        quest_object_dir.insert(quest_dir + "/object");

        fprintf(stdout, "Setting Locale \"%s\" (Path: %s)\n", rst_service_name.c_str(), service_base_path.c_str());
        return true;
    }

    void locale_service_transfer_default_setting() {
        if (!table_by_skill::instance().check())
            exit(1);
    }

    auto locale_service_get_base_path() -> const std::string & {
        return service_base_path;
    }

    auto locale_service_get_map_path() -> const std::string & {
        return service_map_path;
    }

    auto locale_service_get_quest_path() -> const std::string & {
        return quest_dir;
    }
}
