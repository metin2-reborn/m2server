#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace metin2::game::inline data {

typedef std::vector< std::string > token_vector;

class memory_text_file_loader
{
	public:
		memory_text_file_loader();
		virtual ~memory_text_file_loader();

		void			bind(int bufSize, const void* c_pvBuf);
		uint32_t			line_count();
		bool			check_line_index(uint32_t dwLine);
		bool			split_line(uint32_t dwLine, std::vector<std::string> * pstTokenVector, const char * c_szDelimeter = " \t");
		const std::string &	line_string(uint32_t dwLine);

	protected:
		std::vector<std::string> line_vector_;
};
}
