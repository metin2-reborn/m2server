#include "banword.h"

#include "cmd.h"

#include <common/tables.h>
#include <libthecore/utils.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include <sys/types.h>


CBanwordManager::CBanwordManager()
{
}

CBanwordManager::~CBanwordManager()
{
}

bool CBanwordManager::Initialize(TBanwordTable * p, uint16_t wSize)
{
	m_hashmap_words.clear();

	for (uint16_t i = 0; i < wSize; ++i, ++p)
		m_hashmap_words[p->szWord] = true;

	char szBuf[256];
	snprintf(szBuf, sizeof(szBuf), "Banword reloaded! (total %zu banwords)", m_hashmap_words.size());
	SendLog(szBuf);
	return true;
}

bool CBanwordManager::CheckString(const char * c_pszString, size_t _len)
{
	if (m_hashmap_words.empty())
		return false;

	__typeof(m_hashmap_words.begin()) it = m_hashmap_words.begin();

	while (it != m_hashmap_words.end())
	{
		const std::string & r = it->first;
		const char * tmp = c_pszString;
		ssize_t len = _len;

		// words start on a character boundary (text is UTF-8)
		while (len > 0)
		{
			if (!strncmp(tmp, r.c_str(), r.size()))
				return true;

			size_t step = utf8_sequence_length((unsigned char) *tmp);
			tmp += step;
			len -= step;
		}

		it++;
	}

	return false;
}

void CBanwordManager::ConvertString(char * c_pszString, size_t _len)
{
	__typeof(m_hashmap_words.begin()) it = m_hashmap_words.begin();

	while (it != m_hashmap_words.end())
	{
		const std::string & r = it->first;

		char * tmp = c_pszString;
		ssize_t len = _len;

		// words start on a character boundary (text is UTF-8)
		while (len > 0)
		{
			if (*tmp == '*')
			{
				++tmp;
				--len;
				continue;
			}

			if (!strncmp(tmp, r.c_str(), r.size()))
			{
				memset(tmp, '*', r.size());
				tmp += r.size();
				len -= r.size();
			}
			else
			{
				size_t step = utf8_sequence_length((unsigned char) *tmp);
				tmp += step;
				len -= step;
			}
		}

		it++;
	}
}

