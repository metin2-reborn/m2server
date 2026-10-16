#include <metin2/config/config.h>
#include <metin2/game/app/config.h>

#include <metin2/game/admin/cmd.h>
#include <metin2/game/app/typedef.h>
#include <metin2/game/app/utils.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/combat/skill_power.h>
#include <metin2/game/guilds/war_map.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/persistence/log.h>
#include <metin2/game/protocol/desc.h>
#include <metin2/game/protocol/desc_manager.h>
#include <metin2/game/text/locale.hpp>
#include <metin2/game/text/locale_service.h>

#include <metin2/log/log.h>
#include <metin2/proto/character_types.h>
#include <metin2/proto/chat_types.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <set>
#include <string>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/socket.h>

namespace metin2::game::inline app {

    char public_ip[16] = "0";
    std::string proxy_ip;
    bool no_more_client = false;
    int full_user_count = 1200;
    int busy_user_count = 650;

    bool check_client_version_enabled = true;
    std::string client_version = "1215955205";

    uint32_t gold_drop_time_limit_value = 0;

    std::string quest_dir = "./quest";
    std::set<std::string> quest_object_dir;

    static std::set<int> map_allows;

    auto map_allow_find(int index) -> bool {
        if (metin2::config::is_auth())
            return false;

        if (map_allows.find(index) == map_allows.end())
            return false;

        return true;
    }

    void map_allow_log() {
        std::set<int>::iterator i;

        for (i = map_allows.begin(); i != map_allows.end(); ++i)
            metin2::log::info("Hosting map {:d}", *i);
    }

    void map_allow_copy(int32_t *pl, int size) {
        int count = 0;
        auto it = map_allows.begin();

        if (map_allows.size() > static_cast<size_t>(size)) {
            metin2::log::error("Too many allowed maps (at most {:d})", size);
            exit(1);
        }
        while (it != map_allows.end() && count < size) {
            *(pl++) = *(it++);
            ++count;
        }
    }

    static auto get_ip_info() -> bool {
        char internal_ip[16] = "0";
        struct ifaddrs *ifaddrp = nullptr;

        if (0 != getifaddrs(&ifaddrp))
            return false;

        for (struct ifaddrs *ifap = ifaddrp; nullptr != ifap; ifap = ifap->ifa_next) {
            if (!ifap->ifa_addr || ifap->ifa_addr->sa_family != AF_INET) continue;
            auto *sai = (struct sockaddr_in *)ifap->ifa_addr;

            if (!ifap->ifa_netmask ||
                sai->sin_addr.s_addr == 0 ||
                sai->sin_addr.s_addr == 16777343)
                continue;

            char *netip = inet_ntoa(sai->sin_addr);

            if (!strncmp(netip, "192.168", 7)) {
                strlcpy(internal_ip, netip, sizeof(internal_ip));
            } else if (!strncmp(netip, "10.", 3)) {
                strlcpy(internal_ip, netip, sizeof(internal_ip));
            } else if (public_ip[0] == '0') {
                strlcpy(public_ip, netip, sizeof(public_ip));
            }
        }

        freeifaddrs(ifaddrp);

        if (public_ip[0] != '0') {
            return true;
        } else {
            if (internal_ip[0] == '0') {
                return false;
            } else {
                strlcpy(public_ip, internal_ip, sizeof(public_ip));
                return true;
            }
        }
    }

    void config_init() {
        const metin2::config::server_settings &server = metin2::config::server();
        const metin2::config::database_settings &db = metin2::config::database();

        if (!server.bind_ip.empty())
            strlcpy(public_ip, server.bind_ip.c_str(), sizeof(public_ip));
        else if (!get_ip_info()) {
            metin2::log::error("Cannot detect the address to bind to; set server.bind_ip");
            exit(1);
        }

        if (!server.proxy_ip.empty()) {
            in_addr address{};
            if (inet_pton(AF_INET, server.proxy_ip.c_str(), &address) == 1)
                proxy_ip = server.proxy_ip;
            else if (const hostent *host = gethostbyname(server.proxy_ip.c_str()); host && host->h_addr_list[0]) {
                proxy_ip = inet_ntoa(*reinterpret_cast<const in_addr *>(host->h_addr_list[0]));
                metin2::log::info("server.proxy_ip {} resolves to {}", server.proxy_ip, proxy_ip);
            } else {
                metin2::log::error("Cannot resolve server.proxy_ip {}", server.proxy_ip);
                exit(1);
            }
        }

        no_more_client = metin2::config::game().refuse_clients;

        for (long index : metin2::config::game().map_allow)
            map_allows.insert(static_cast<int>(index));

        if (metin2::config::game().max_level < 1 || metin2::config::game().max_level > PLAYER_MAX_LEVEL_CONST) {
            metin2::log::error("game.max_level must be from 1 to {:d}", PLAYER_MAX_LEVEL_CONST);
            exit(1);
        }

        get_account_db().connect(db.host.c_str(), db.port, db.user.c_str(), db.password.c_str(), db.common.c_str());

        if (false == get_account_db().is_connected()) {
            metin2::log::error("Cannot connect to the common database");
            exit(1);
        }

        metin2::log::info("Common database connected");

        // Must not get a WHERE clause.
        {
            const std::string query = sql::format("SELECT mKey, mValue FROM locale");

            auto msg = get_account_db().direct_query(query);

            if (msg->get()->num_rows == 0) {
                metin2::log::error("Common database locale table is empty or unreadable: {}", query);
                exit(1);
            }

            MYSQL_ROW row;

            while (nullptr != (row = mysql_fetch_row(msg->get()->res.get()))) {
                if (strcasecmp(row[0], "LOCALE") == 0) {
                    if (locale_service_init(row[1]) == false) {
                        metin2::log::error("Invalid locale {} in common database", row[1]);
                        exit(1);
                    }
                }
            }
        }

        get_account_db().set_locale(db_charset);

        get_account_db().connect_async(db.host.c_str(), db.port, db.user.c_str(), db.password.c_str(), db.common.c_str(), db_charset.c_str());

        const std::string &player_db = metin2::config::is_auth() ? db.account : db.player;
        get_db_manager().connect(db.host.c_str(), db.port, db.user.c_str(), db.password.c_str(), player_db.c_str());

        if (!get_db_manager().is_connected()) {
            metin2::log::error("Cannot connect to the player database {}", player_db);
            exit(1);
        }

        metin2::log::info("Player database connected");

        if (!metin2::config::is_auth()) {
            get_log_manager().connect(db.host.c_str(), db.port, db.user.c_str(), db.password.c_str(), db.log.c_str());

            if (!get_log_manager().is_connected()) {
                metin2::log::error("Cannot connect to the log database {}", db.log);
                exit(1);
            }

            metin2::log::info("Log database connected");

            get_log_manager().boot_log(server.hostname.c_str(), server.channel);
        }

        {
            const std::string query = sql::format("SELECT mValue FROM locale WHERE mKey='SKILL_POWER_BY_LEVEL'");
            auto msg = get_account_db().direct_query(query);

            if (msg->get()->num_rows == 0) {
                metin2::log::error("Locale table has no SKILL_POWER_BY_LEVEL: {}", query);
                exit(1);
            }

            MYSQL_ROW row;

            row = mysql_fetch_row(msg->get()->res.get());

            const char *p = row[0];
            int cnt = 0;
            char num[128];
            int ai_base_skill_power_by_level_table[SKILL_MAX_LEVEL + 1];

            metin2::log::debug("Skill power by level: {}", p);
            while (*p != '\0' && cnt < (SKILL_MAX_LEVEL + 1)) {
                p = one_argument(p, num, sizeof(num));
                ai_base_skill_power_by_level_table[cnt++] = atoi(num);

                if (*p == '\0') {
                    if (cnt != (SKILL_MAX_LEVEL + 1)) {
                        metin2::log::error("Locale table has too few skill power values ({:d}): {}", cnt, query);
                        exit(1);
                    }

                    metin2::log::info("Loaded {:d} skill power levels", cnt);
                    break;
                }
            }

            for (int job = 0; job < JOB_MAX_NUM * 2; ++job) {
                const std::string type_query = sql::format("SELECT mValue from locale where mKey='SKILL_POWER_BY_LEVEL_TYPE{}' ORDER BY CAST(mValue AS unsigned)", job);
                auto msg = get_account_db().direct_query(type_query);

                if (msg->get()->num_rows == 0) {
                    get_table_by_skill().set_skill_power_by_level_from_type(job, ai_base_skill_power_by_level_table);
                    continue;
                }

                row = mysql_fetch_row(msg->get()->res.get());
                cnt = 0;
                p = row[0];
                int ai_skill_table[SKILL_MAX_LEVEL + 1];

                metin2::log::debug("Skill power of type {:d}: {}", job, p);
                while (*p != '\0' && cnt < (SKILL_MAX_LEVEL + 1)) {
                    p = one_argument(p, num, sizeof(num));
                    ai_skill_table[cnt++] = atoi(num);

                    if (*p == '\0') {
                        if (cnt != (SKILL_MAX_LEVEL + 1)) {
                            metin2::log::error("Locale table has too few skill power values ({:d}): {}", cnt, type_query);
                            exit(1);
                        }

                        metin2::log::debug("Loaded skill power of type {:d} ({:d} levels)", job, cnt);
                        break;
                    }
                }

                get_table_by_skill().set_skill_power_by_level_from_type(job, ai_skill_table);
            }
        }

        if (!metin2::config::game().quest.dir.empty())
            quest_dir = metin2::config::game().quest.dir;
        for (const std::string &dir : metin2::config::game().quest.object_dirs)
            quest_object_dir.insert(dir);

        locale_service_load_locale_string_file();
        locale_service_transfer_default_setting();
        locale_service_load_empire_text_convert_tables();

        if (FILE *fp = fopen("CMD", "r")) {
            char buf[256];
            while (fgets(buf, 256, fp)) {
                char cmd[32], levelname[32];
                int level;

                two_arguments(buf, cmd, sizeof(cmd), levelname, sizeof(levelname));

                if (!*cmd || !*levelname) {
                    metin2::log::error("CMD file: each line must be <command> <DISABLE | LOW_WIZARD | WIZARD | HIGH_WIZARD | GOD | IMPLEMENTOR>");
                    exit(1);
                }

                if (!strcasecmp(levelname, "LOW_WIZARD"))
                    level = GM_LOW_WIZARD;
                else if (!strcasecmp(levelname, "WIZARD"))
                    level = GM_WIZARD;
                else if (!strcasecmp(levelname, "HIGH_WIZARD"))
                    level = GM_HIGH_WIZARD;
                else if (!strcasecmp(levelname, "GOD"))
                    level = GM_GOD;
                else if (!strcasecmp(levelname, "IMPLEMENTOR"))
                    level = GM_IMPLEMENTOR;
                else if (!strcasecmp(levelname, "DISABLE"))
                    level = GM_IMPLEMENTOR + 1;
                else {
                    metin2::log::error("CMD file: unknown level {} for command {}", levelname, cmd);
                    exit(1);
                }

                interpreter_set_privilege(cmd, level);
            }

            fclose(fp);
        }

        load_state_user_count();

        get_war_map_manager().load_war_map_info(nullptr);
    }

    void check_client_version() {
        check_client_version_enabled = true;

        const desc_manager::desc_set &descs = get_desc_manager().client_set();
        auto it = descs.begin();

        while (it != descs.end()) {
            desc *d = *(it++);

            if (!d->get_character())
                continue;

            int version = atoi(client_version.c_str());
            int date = atoi(d->client_version());

            if (version != date) {
                d->get_character()->chat_packet(CHAT_TYPE_NOTICE, lc_text("클라이언트 버전이 틀려 로그아웃 됩니다. 정상적으로 패치 후 접속하세요."));
                d->delayed_disconnect(10);
            }
        }
    }

    void load_state_user_count() {
        FILE *fp = fopen("state_user_count", "r");

        if (!fp)
            return;

        fscanf(fp, " %d %d ", &full_user_count, &busy_user_count);

        fclose(fp);
    }
}
