#include <metin2/db/tables/csv_reader.h>

#include <cassert>
#include <cstddef>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>

namespace metin2::db::inline tables {

    namespace {
        auto trim(std::string_view text) -> std::string_view {
            constexpr std::string_view whitespace = " \t\r\n";
            const auto first = text.find_first_not_of(whitespace);
            if (first == std::string_view::npos)
                return {};
            return text.substr(first, text.find_last_not_of(whitespace) - first + 1);
        }
    }

    auto csv_table::load(const char *file_name, char separator, char quote) -> bool {
        assert(separator != quote);

        rows_.clear();
        cur_row_ = -1;

        std::ifstream file(file_name);
        if (!file)
            return false;

        std::vector<std::string> row;
        bool in_quote = false;
        std::string token;
        std::string raw_line;

        while (std::getline(file, raw_line)) {
            const std::string_view line = trim(raw_line);
            if (line.empty() || (!in_quote && line[0] == '#'))
                continue;

            for (size_t cur = 0; cur < line.size(); ++cur) {
                const char ch = line[cur];
                if (in_quote) {
                    if (ch != quote) {
                        token += ch;
                    } else if (cur + 1 < line.size() && line[cur + 1] == quote) {
                        token += quote;
                        ++cur;
                    } else {
                        in_quote = false;
                    }
                } else if (ch == separator) {
                    row.push_back(std::move(token));
                    token.clear();
                } else if (ch == quote) {
                    in_quote = true;
                } else {
                    token += ch;
                }
            }

            if (in_quote) {
                token += "\r\n";
            } else {
                row.push_back(std::move(token));
                token.clear();
                rows_.push_back(std::move(row));
                row.clear();
            }
        }

        return true;
    }

    auto csv_table::next() -> bool {
        return ++cur_row_ < static_cast<int>(rows_.size());
    }

    auto csv_table::as_string_by_index(size_t index) const -> const char * {
        assert(cur_row_ >= 0 && "call next() first!");
        assert(cur_row_ < static_cast<int>(rows_.size()) && "no more rows!");
        const auto &row = rows_[cur_row_];
        assert(index < row.size());
        return row.at(index).c_str();
    }
}
