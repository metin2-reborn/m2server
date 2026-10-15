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

auto memory_text_file_loader::split_line(uint32_t line, std::vector<std::string>* pst_token_vector, const char * delimeter) -> bool
{
	pst_token_vector->clear();

	std::string stToken;
	const std::string & c_rstLine = line_string(line);

	uint32_t basePos = 0;

	do
	{
		int beginPos = c_rstLine.find_first_not_of(delimeter, basePos);
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
			endPos = c_rstLine.find_first_of(delimeter, beginPos);
			basePos = endPos;
		}

		pst_token_vector->push_back(c_rstLine.substr(beginPos, endPos - beginPos));

		if (int(c_rstLine.find_first_not_of(delimeter, basePos)) < 0)
			break;
	} while (basePos < c_rstLine.length());

	return true;
}

auto memory_text_file_loader::line_count() -> uint32_t
{
	return line_vector_.size();
}

auto memory_text_file_loader::check_line_index(uint32_t line) -> bool
{
	if (line >= line_vector_.size())
		return false;

	return true;
}

auto memory_text_file_loader::line_string(uint32_t line) -> const std::string &
{
	assert(check_line_index(line));
	return line_vector_[line];
}

void memory_text_file_loader::bind(int buf_size, const void* buf)
{
	line_vector_.clear();

	const char * c_pcBuf = (const char *)buf;
	std::string stLine;
	int pos = 0;

	while (pos < buf_size)
	{
		const char c = c_pcBuf[pos++];

		if ('\n' == c || '\r' == c)
		{
			if (pos < buf_size)
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
