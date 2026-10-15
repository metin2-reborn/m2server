#include <metin2/game/data/file_loader.h>

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace metin2::game::inline data {

memory_text_file_loader::memory_text_file_loader()
{
}

memory_text_file_loader::~memory_text_file_loader()
{
}

bool memory_text_file_loader::split_line(uint32_t dwLine, std::vector<std::string>* pstTokenVector, const char * c_szDelimeter)
{
	pstTokenVector->clear();

	std::string stToken;
	const std::string & c_rstLine = line_string(dwLine);

	uint32_t basePos = 0;

	do
	{
		int beginPos = c_rstLine.find_first_not_of(c_szDelimeter, basePos);
		if (beginPos < 0)
			return false;

		int endPos;

		if (c_rstLine[beginPos] == '#' && c_rstLine.compare(beginPos, 4, "#--#") != 0)
		{
			return false;
		}
		else if (c_rstLine[beginPos] == '"')
		{
			++beginPos;
			endPos = c_rstLine.find_first_of("\"", beginPos);

			if (endPos < 0)
				return false;

			basePos = endPos + 1;
		}
		else
		{
			endPos = c_rstLine.find_first_of(c_szDelimeter, beginPos);
			basePos = endPos;
		}

		pstTokenVector->push_back(c_rstLine.substr(beginPos, endPos - beginPos));

		// Added code. Checks for a trailing tab. - [levites]
		if (int(c_rstLine.find_first_not_of(c_szDelimeter, basePos)) < 0)
			break;
	} while (basePos < c_rstLine.length());

	return true;
}

uint32_t memory_text_file_loader::line_count()
{
	return line_vector_.size();
}

bool memory_text_file_loader::check_line_index(uint32_t dwLine)
{
	if (dwLine >= line_vector_.size())
		return false;

	return true;
}

const std::string & memory_text_file_loader::line_string(uint32_t dwLine)
{
	assert(check_line_index(dwLine));
	return line_vector_[dwLine];
}

void memory_text_file_loader::bind(int bufSize, const void* c_pvBuf)
{
	line_vector_.clear();

	const char * c_pcBuf = (const char *)c_pvBuf;
	std::string stLine;
	int pos = 0;

	while (pos < bufSize)
	{
		const char c = c_pcBuf[pos++];

		if ('\n' == c || '\r' == c)
		{
			if (pos < bufSize)
				if ('\n' == c_pcBuf[pos] || '\r' == c_pcBuf[pos])
					++pos;

			line_vector_.push_back(stLine);
			stLine = "";
		}
		else if (c < 0)
		{
			stLine.append(c_pcBuf + (pos-1), 2);
			++pos;
		}
		else
		{
			stLine += c;
		}
	}

	line_vector_.push_back(stLine);
}
}
