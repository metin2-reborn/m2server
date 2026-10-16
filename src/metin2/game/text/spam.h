#pragma once

#include <metin2/game/app/utils.h>

#include <metin2/common/singleton.h>
#include <metin2/core/utils.h>
#include <metin2/log/log.h>

#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace metin2::game::inline text {

    class spam_manager : public singleton<spam_manager> {
      public:
        inline auto spam_score(const char *src, size_t len, unsigned int &score) -> const char * {
            const char *word = nullptr;
            score = 0;

            std::string orig(src);
            std::erase_if(orig, is_space);

            for (size_t i = 0; i < vec_word_.size(); ++i) {
                std::pair<std::string, unsigned int> &r = vec_word_[i];

                if (true == wild_case_cmp(r.first.c_str(), orig.c_str())) {
                    word = r.first.c_str();
                    score += r.second;
                }
            }

            return word;
        }

        inline void clear() {
            vec_word_.clear();
        }

        inline void insert(const char *str, unsigned int score = 10) {
            vec_word_.push_back(std::make_pair(str, score));
            metin2::log::debug("Spam word \"{}\" (score {:d})", str, score);
        }

      private:
        std::vector<std::pair<std::string, unsigned int>> vec_word_;
    };
}
