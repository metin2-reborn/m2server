#pragma once

#include "utils.h"

#include <common/singleton.h>
#include <metin2/log/log.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace metin2::game {

class SpamManager : public singleton<SpamManager>
{
	public:
		inline const char * GetSpamScore(const char * src, size_t len, unsigned int & score)
		{
			const char * word = nullptr;
			score = 0;

			std::string strOrig(src);
			strOrig.erase( remove_if(strOrig.begin(), strOrig.end(), isspace), strOrig.end() );

			for (size_t i = 0; i < m_vec_word.size(); ++i)
			{
				std::pair<std::string, unsigned int> & r = m_vec_word[i];

				if (true == WildCaseCmp(r.first.c_str(), strOrig.c_str()))
				{
					word = r.first.c_str();
					score += r.second;
				}
			}

			return word;
		}

		inline void Clear()
		{
			m_vec_word.clear();
		}

		inline void Insert(const char * str, unsigned int score = 10)
		{
			m_vec_word.push_back(std::make_pair(str, score));
			metin2::log::info( "SPAM: {:2d} {}", score, str);
		}

	private:
		std::vector< std::pair<std::string, unsigned int> > m_vec_word;
};
}
