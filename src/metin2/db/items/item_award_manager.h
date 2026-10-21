#pragma once
#include <metin2/proto/item_types.h>
#include <metin2/proto/limits.h>
#include <metin2/sql/async_sql.h>

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>

namespace metin2::db::inline items {

    struct item_award {
        uint32_t id;
        char login[LOGIN_MAX_LEN + 1];
        uint32_t vnum;
        uint32_t count;
        uint32_t socket0;
        uint32_t socket1;
        uint32_t socket2;
        char why[ITEM_AWARD_WHY_MAX_LEN + 1];
        bool taken;
        bool mall;
    };

    class item_award_manager {
      public:
        item_award_manager();

        void request_load();
        void load(sql_msg &msg);
        auto by_login(const char *login) -> std::set<item_award *> *;

        void taken(uint32_t award_id, uint64_t item_id);
        auto remove(uint32_t award_id) -> bool;

      private:
        std::map<uint32_t, std::unique_ptr<item_award>> awards_;
        std::map<std::string, std::set<item_award *>> set_awards_by_login_;
    };

    inline auto get_item_award_manager() -> item_award_manager & {
        static item_award_manager instance;
        return instance;
    }
}
