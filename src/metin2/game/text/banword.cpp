#include <metin2/game/text/banword.h>

#include <metin2/game/admin/cmd.h>

#include <metin2/core/utils.h>
#include <metin2/proto/tables.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <string>

#include <sys/types.h>

namespace metin2::game::inline text {

    banword_manager::banword_manager() = default;

    banword_manager::~banword_manager() = default;

    auto banword_manager::initialize(banword_table *p, uint16_t size_value) -> bool {
        hashmap_words_.clear();

        for (uint16_t i = 0; i < size_value; ++i, ++p)
            hashmap_words_[p->word] = true;

        send_log(std::format("Banword reloaded! (total {} banwords)", hashmap_words_.size()).c_str());
        return true;
    }

    auto banword_manager::check_string(const char *string_value, size_t len_value) -> bool {
        if (hashmap_words_.empty())
            return false;

        auto it = hashmap_words_.begin();

        while (it != hashmap_words_.end()) {
            const std::string &r = it->first;
            const char *tmp = string_value;
            ssize_t len = len_value;

            while (len > 0) {
                if (!strncmp(tmp, r.c_str(), r.size()))
                    return true;

                size_t step = utf8_sequence_length((unsigned char)*tmp);
                tmp += step;
                len -= step;
            }

            it++;
        }

        return false;
    }

    void banword_manager::convert_string(char *string_value, size_t len_value) {
        auto it = hashmap_words_.begin();

        while (it != hashmap_words_.end()) {
            const std::string &r = it->first;

            char *tmp = string_value;
            ssize_t len = len_value;

            while (len > 0) {
                if (*tmp == '*') {
                    ++tmp;
                    --len;
                    continue;
                }

                if (!strncmp(tmp, r.c_str(), r.size())) {
                    memset(tmp, '*', r.size());
                    tmp += r.size();
                    len -= r.size();
                } else {
                    size_t step = utf8_sequence_length((unsigned char)*tmp);
                    tmp += step;
                    len -= step;
                }
            }

            it++;
        }
    }
}
