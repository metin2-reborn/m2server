#include <metin2/game/data/file_loader.h>

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace metin2::game::inline data {

    memory_text_file_loader::memory_text_file_loader() = default;

    memory_text_file_loader::~memory_text_file_loader() = default;

    auto memory_text_file_loader::split_line(uint32_t line, std::vector<std::string> *pst_token_vector, const char *delimeter) -> bool {
        pst_token_vector->clear();

        std::string token;
        const std::string &c_rst_line = line_string(line);

        uint32_t base_pos = 0;

        do {
            int begin_pos = c_rst_line.find_first_not_of(delimeter, base_pos);
            if (begin_pos < 0)
                return false;

            int end_pos;

            if (c_rst_line[begin_pos] == '#' && c_rst_line.compare(begin_pos, 4, "#--#") != 0) {
                return false;
            } else if (c_rst_line[begin_pos] == '"') {
                ++begin_pos;
                end_pos = c_rst_line.find_first_of("\"", begin_pos);

                if (end_pos < 0)
                    return false;

                base_pos = end_pos + 1;
            } else {
                end_pos = c_rst_line.find_first_of(delimeter, begin_pos);
                base_pos = end_pos;
            }

            pst_token_vector->push_back(c_rst_line.substr(begin_pos, end_pos - begin_pos));

            if (int(c_rst_line.find_first_not_of(delimeter, base_pos)) < 0)
                break;
        } while (base_pos < c_rst_line.length());

        return true;
    }

    auto memory_text_file_loader::line_count() -> uint32_t {
        return line_vector_.size();
    }

    auto memory_text_file_loader::check_line_index(uint32_t line) -> bool {
        if (line >= line_vector_.size())
            return false;

        return true;
    }

    auto memory_text_file_loader::line_string(uint32_t line) -> const std::string & {
        assert(check_line_index(line));
        return line_vector_[line];
    }

    void memory_text_file_loader::bind(int buf_size, const void *buf) {
        line_vector_.clear();

        const char *c_pc_buf = (const char *)buf;
        std::string line;
        int pos = 0;

        while (pos < buf_size) {
            const char c = c_pc_buf[pos++];

            if ('\n' == c || '\r' == c) {
                if (pos < buf_size)
                    if ('\n' == c_pc_buf[pos] || '\r' == c_pc_buf[pos])
                        ++pos;

                line_vector_.push_back(line);
                line = "";
            } else {
                line += c;
            }
        }

        line_vector_.push_back(line);
    }
}
