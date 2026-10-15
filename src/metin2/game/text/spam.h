#pragma once

#include <metin2/game/app/utils.h>

#include <metin2/common/singleton.h>
#include <metin2/log/log.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace metin2::game::inline text {

class spam_manager : public singleton<spam_manager>
{
	public:
		inline const char * spam_score(const char * src, size_t len, unsigned int & score)
		{
			const char * word = nullptr;
			score = 0;

			std::string strOrig(src);
			strOrig.erase( remove_if(strOrig.begin(), strOrig.end(), isspace), strOrig.end() );

			for (size_t i = 0; i < vec_word_.size(); ++i)
			{
				std::pair<std::string, unsigned int> & r = vec_word_[i];

				if (true == wild_case_cmp(r.first.c_str(), strOrig.c_str()))
				{
					word = r.first.c_str();
					score += r.second;
				}
			}

			return word;
		}

		inline void clear()
		{
			vec_word_.clear();
		}

		inline void insert(const char * str, unsigned int score = 10)
		{
			vec_word_.push_back(std::make_pair(str, score));
			metin2::log::info( "SPAM: {:2d} {}", score, str);
		}

	private:
		std::vector< std::pair<std::string, unsigned int> > vec_word_;
};
}
