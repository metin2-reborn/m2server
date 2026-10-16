#pragma once

#include <metin2/common/singleton.h>
#include <metin2/proto/tables.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace metin2::db::inline empire {

    class monarch : public singleton<monarch> {
      public:
        typedef std::map<uint32_t, std::unique_ptr<monarch_election_info>> map_monarchelection;
        typedef std::vector<monarch_candidacy> vec_monarchcandidacy;

        monarch();

        auto vote_monarch(uint32_t pid, uint32_t selectdpid) -> bool;
        void elect_monarch();

        auto is_candidacy(uint32_t pid) -> bool;
        auto add_candidacy(uint32_t pid, const char *name) -> bool;
        auto del_candidacy(const char *name) -> bool;

        auto load_monarch() -> bool;
        auto set_monarch(const char *name) -> bool;
        auto del_monarch(const char *name) -> bool;

        auto is_monarch(int empire, uint32_t pid) -> bool;
        auto add_money(int empire, int64_t money_value) -> bool;
        auto dec_money(int empire, int64_t money_value) -> bool;
        auto take_money(int empire, uint32_t pid, int64_t money_value) -> bool;

        auto get_monarch() -> monarch_info * {
            return &monarch_info_;
        }

        auto vec_monarch_candidacy() -> vec_monarchcandidacy & {
            return vec_monarch_candidacy_;
        }

        auto monarch_candidacy_size() -> size_t {
            return vec_monarch_candidacy_.size();
        }

      private:
        auto candidacy_index(uint32_t pid) -> int;

        map_monarchelection map_monarch_election_;
        vec_monarchcandidacy vec_monarch_candidacy_;
        monarch_info monarch_info_;
    };
}
