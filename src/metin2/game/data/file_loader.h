#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace metin2::game::inline data {

    using token_vector = std::vector<std::string>;

    class memory_text_file_loader {
      public:
        memory_text_file_loader();
        virtual ~memory_text_file_loader();

        void bind(int buf_size, const void *buf);
        auto line_count() -> uint32_t;
        auto check_line_index(uint32_t line) -> bool;
        auto split_line(uint32_t line, std::vector<std::string> *pst_token_vector, const char *delimeter = " \t") -> bool;
        auto line_string(uint32_t line) -> const std::string &;

      protected:
        std::vector<std::string> line_vector_;
    };
}
