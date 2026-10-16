
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

        uint32_t aid;
        uint32_t pid_value[4];
        memset(pid_value, 0, sizeof(pid_value));

        {
            const std::string query = sql::format(
                "SELECT id, pid1, pid2, pid3, pid4 FROM player_index WHERE (pid1={} OR pid2={} OR pid3={} OR pid4={}) AND empire={}", player_id(), player_id(), player_id(), player_id(), empire());

            auto msg = db_manager::instance().direct_query(query);

            if (msg->get()->num_rows == 0) {
                return 0;
            }

            MYSQL_ROW row = mysql_fetch_row(msg->get()->res.get());

            str_to_number(aid, row[0]);
            str_to_number(pid_value[0], row[1]);
            str_to_number(pid_value[1], row[2]);
            str_to_number(pid_value[2], row[3]);
            str_to_number(pid_value[3], row[4]);
        }

        const int loop = 4;

        {
            uint32_t guild_id_value[4];
            guild *guild[4];
            for (int i = 0; i < loop; ++i) {
                const std::string query = sql::format("SELECT guild_id FROM guild_member WHERE pid={}", pid_value[i]);

                auto msg_value = db_manager::instance().direct_query(query);

                if (msg_value != nullptr) {
                    if (msg_value->get()->num_rows > 0) {
                        MYSQL_ROW row = mysql_fetch_row(msg_value->get()->res.get());

                        str_to_number(guild_id_value[i], row[0]);

                        guild[i] = guild_manager::instance().find_guild(guild_id_value[i]);

                        if (guild[i] != nullptr) {
                            return 2;
                        }
                    } else {
                        guild_id_value[i] = 0;
                        guild[i] = nullptr;
                    }
                }
            }
        }

        {
            for (int i = 0; i < loop; ++i) {
                if (marriage::manager::instance().is_engaged_or_married(pid_value[i]) == true)
                    return 3;
            }
        }

        {
            const std::string query = sql::format("UPDATE player_index SET empire={} WHERE (pid1={} OR pid2={} OR pid3={} OR pid4={}) AND empire={}", empire_value, player_id(), player_id(), player_id(), player_id(), empire());

            auto msg = db_manager::instance().direct_query(query);

            if (msg->get()->affected_rows > 0) {
                set_change_empire_count();

                return 999;
            }
        }

        return 0;
    }

    auto character::change_empire_count() const -> int {
        uint32_t aid_value = aid();

        if (aid_value == 0)
            return 0;

        auto msg = db_manager::instance().direct_query(sql::format("SELECT change_count FROM change_empire WHERE account_id = {}", aid_value));

        if (msg != nullptr) {
            if (msg->get()->num_rows == 0) {
                return 0;
            }

            MYSQL_ROW row = mysql_fetch_row(msg->get()->res.get());

            uint32_t count = 0;
            str_to_number(count, row[0]);

            return count;
        }

        return 0;
    }

    void character::set_change_empire_count() {
        std::string query;

        uint32_t aid_value = aid();

        if (aid_value == 0) return;

        int count = change_empire_count();

        if (count == 0) {
            count++;
            query = sql::format("INSERT INTO change_empire VALUES({}, {}, NOW())", aid_value, count);
        } else {
            count++;
            query = sql::format("UPDATE change_empire SET change_count={} WHERE account_id={}", count, aid_value);
        }

        auto pmsg = db_manager::instance().direct_query(query);
    }

    auto character::aid() const -> uint32_t {
        uint32_t aid = 0;

        const std::string query = sql::format("SELECT id FROM player_index WHERE (pid1={} OR pid2={} OR pid3={} OR pid4={}) AND empire={}", player_id(), player_id(), player_id(), player_id(), empire());

        auto msg = db_manager::instance().direct_query(query);

        if (msg != nullptr) {
            if (msg->get()->num_rows == 0) {
                return 0;
            }

            MYSQL_ROW row = mysql_fetch_row(msg->get()->res.get());

            str_to_number(aid, row[0]);

            return aid;
        } else {
            return 0;
        }
    }
}
