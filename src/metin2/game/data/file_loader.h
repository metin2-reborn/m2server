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

		void			bind(int buf_size, const void* buf);
		uint32_t			line_count();
		bool			check_line_index(uint32_t line);
		bool			split_line(uint32_t line, std::vector<std::string> * pst_token_vector, const char * delimeter = " \t");
		const std::string &	line_string(uint32_t line);

	protected:
		std::vector<std::string> line_vector_;
};
}
