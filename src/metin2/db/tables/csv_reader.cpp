#include <metin2/db/tables/csv_reader.h>

#include <cassert>
#include <cstddef>
#include <fstream>
#include <ios>
#include <memory>
#include <string>
#include <utility>

namespace metin2::db::inline tables {

    namespace {
        enum parse_state {
            STATE_NORMAL = 0,
            STATE_QUOTE
        };

        auto trim(std::string str) -> std::string {
            str = str.erase(str.find_last_not_of(" \t\r\n") + 1);
            str = str.erase(0, str.find_first_not_of(" \t\r\n"));
            return str;
        }

    }

    auto c_csv_file::load(const char *file_name, const char seperator, const char quote) -> bool {
        assert(seperator != quote);

        std::ifstream file(file_name, std::ios::in);
        if (!file) return false;

        destroy();

        std::unique_ptr<c_csv_row> row;
        parse_state state = STATE_NORMAL;
        std::string token = "";
        char buf[2048 + 1] = {
            0,
        };

        while (file.good()) {
            file.getline(buf, 2048);
            buf[sizeof(buf) - 1] = 0;

            std::string line(trim(buf));
            if (line.empty() || (state == STATE_NORMAL && line[0] == '#')) continue;

            std::string text = std::string(line) + "  ";
            size_t cur = 0;

            while (cur < text.size()) {
                if (state == STATE_QUOTE) {
                    if (text[cur] == quote) {
                        if (text[cur + 1] == quote) {
                            token += quote;
                            ++cur;
                        } else {
                            state = STATE_NORMAL;
                        }
                    } else {
                        token += text[cur];
                    }
                } else if (state == STATE_NORMAL) {
                    if (row == nullptr)
                        row = std::make_unique<c_csv_row>();

                    if (text[cur] == seperator) {
                        row->push_back(token);
                        token.clear();
                    } else if (text[cur] == quote) {
                        state = STATE_QUOTE;
                    } else {
                        token += text[cur];
                    }
                }

                ++cur;
            }

            if (state == STATE_NORMAL) {
                assert(row != nullptr);
                row->push_back(token.substr(0, token.size() - 2));
                rows_.push_back(std::move(row));
                token.clear();
            } else {
                token = token.substr(0, token.size() - 2) + "\r\n";
            }
        }

        return true;
    }

    void c_csv_file::destroy() {
        rows_.clear();
    }

    auto c_csv_file::operator[](size_t index) const -> const c_csv_row * {
        assert(index < rows_.size());
        return rows_[index].get();
    }

    c_csv_table::c_csv_table()
        : cur_row_(-1) {
    }

    c_csv_table::~c_csv_table() {
    }

    auto c_csv_table::load(const char *file_name, const char seperator, const char quote) -> bool {
        destroy();
        return file.load(file_name, seperator, quote);
    }

    auto c_csv_table::next() -> bool {
        return ++cur_row_ < (int)file.row_count() ? true : false;
    }

    auto c_csv_table::as_string_by_index(size_t index) const -> const char * {
        const c_csv_row *const row = cur_row();
        assert(row);
        assert(index < row->size());
        return row->as_string(index);
    }

    void c_csv_table::destroy() {
        file.destroy();
        cur_row_ = -1;
    }

    auto c_csv_table::cur_row() const -> const c_csv_row * {
        if (cur_row_ < 0) {
            assert(false && "call Next() first!");
            return nullptr;
        } else if (cur_row_ >= (int)file.row_count()) {
            assert(false && "no more rows!");
            return nullptr;
        }

        return file[cur_row_];
    }
}
