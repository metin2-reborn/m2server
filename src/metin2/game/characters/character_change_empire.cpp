
#include <metin2/game/app/config.h>
#include <metin2/game/characters/character.h>
#include <metin2/game/characters/character_manager.h>
#include <metin2/game/guilds/guild_manager.h>
#include <metin2/game/persistence/db.h>
#include <metin2/game/social/marriage.h>

#include <metin2/common/utils.h>
#include <metin2/sql/async_sql.h>
#include <metin2/sql/query.h>

#include <mysql.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

namespace metin2::game::inline characters {

    auto character::change_empire(uint8_t empire_value) -> int {
        if (empire() == empire_value)
            return 1;

        uint32_t dwAID;
        uint32_t dwPID[4];
        memset(dwPID, 0, sizeof(dwPID));

        {
            const std::string query = sql::format(
                "SELECT id, pid1, pid2, pid3, pid4 FROM player_index{} WHERE (pid1={} OR pid2={} OR pid3={} OR pid4={}) AND empire={}",
                sql::raw(get_table_postfix()), player_id(), player_id(), player_id(), player_id(), empire());

            auto msg = db_manager::instance().direct_query(query);

            if (msg->get()->num_rows == 0) {
                return 0;
            }

            MYSQL_ROW row = mysql_fetch_row(msg->get()->res.get());

            str_to_number(dwAID, row[0]);
            str_to_number(dwPID[0], row[1]);
            str_to_number(dwPID[1], row[2]);
            str_to_number(dwPID[2], row[3]);
            str_to_number(dwPID[3], row[4]);
        }

        const int loop = 4;

        {
            uint32_t dwGuildID[4];
            guild *pGuild[4];
            for (int i = 0; i < loop; ++i) {
                const std::string query = sql::format("SELECT guild_id FROM guild_member{} WHERE pid={}", sql::raw(get_table_postfix()), dwPID[i]);

                auto pMsg = db_manager::instance().direct_query(query);

                if (pMsg != nullptr) {
                    if (pMsg->get()->num_rows > 0) {
                        MYSQL_ROW row = mysql_fetch_row(pMsg->get()->res.get());

                        str_to_number(dwGuildID[i], row[0]);

                        pGuild[i] = guild_manager::instance().find_guild(dwGuildID[i]);

                        if (pGuild[i] != nullptr) {
                            return 2;
                        }
                    } else {
                        dwGuildID[i] = 0;
                        pGuild[i] = nullptr;
                    }
                }
            }
        }

        {
            for (int i = 0; i < loop; ++i) {
                if (marriage::manager::instance().is_engaged_or_married(dwPID[i]) == true)
                    return 3;
            }
        }

        {
            const std::string query = sql::format("UPDATE player_index{} SET empire={} WHERE (pid1={} OR pid2={} OR pid3={} OR pid4={}) AND empire={}",
                sql::raw(get_table_postfix()), empire_value, player_id(), player_id(), player_id(), player_id(), empire());

            auto msg = db_manager::instance().direct_query(query);

            if (msg->get()->affected_rows > 0) {
                set_change_empire_count();

                return 999;
            }
        }

        return 0;
    }

    auto character::change_empire_count() const -> int {
        uint32_t dwAID = aid();

        if (dwAID == 0)
            return 0;

        auto pMsg = db_manager::instance().direct_query(sql::format("SELECT change_count FROM change_empire WHERE account_id = {}", dwAID));

        if (pMsg != nullptr) {
            if (pMsg->get()->num_rows == 0) {
                return 0;
            }

            MYSQL_ROW row = mysql_fetch_row(pMsg->get()->res.get());

            uint32_t count = 0;
            str_to_number(count, row[0]);

            return count;
        }

        return 0;
    }

    void character::set_change_empire_count() {
        std::string query;

        uint32_t dwAID = aid();

        if (dwAID == 0) return;

        int count = change_empire_count();

        if (count == 0) {
            count++;
            query = sql::format("INSERT INTO change_empire VALUES({}, {}, NOW())", dwAID, count);
        } else {
            count++;
            query = sql::format("UPDATE change_empire SET change_count={} WHERE account_id={}", count, dwAID);
        }

        auto pmsg = db_manager::instance().direct_query(query);
    }

    auto character::aid() const -> uint32_t {
        uint32_t dwAID = 0;

        const std::string query = sql::format("SELECT id FROM player_index{} WHERE (pid1={} OR pid2={} OR pid3={} OR pid4={}) AND empire={}",
            sql::raw(get_table_postfix()), player_id(), player_id(), player_id(), player_id(), empire());

        auto pMsg = db_manager::instance().direct_query(query);

        if (pMsg != nullptr) {
            if (pMsg->get()->num_rows == 0) {
                return 0;
            }

            MYSQL_ROW row = mysql_fetch_row(pMsg->get()->res.get());

            str_to_number(dwAID, row[0]);

            return dwAID;
        } else {
            return 0;
        }
    }
}
