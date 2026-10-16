#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace metin2::db::inline tables {

    class c_csv_table {
      public:
        auto load(const char *file_name, char separator = ',', char quote = '"') -> bool;

        auto next() -> bool;

        auto as_string_by_index(size_t index) const -> const char *;

        auto row_count() const -> size_t { return rows_.size(); }

      private:
        std::vector<std::vector<std::string>> rows_;
        int cur_row_ = -1;
    };
}
