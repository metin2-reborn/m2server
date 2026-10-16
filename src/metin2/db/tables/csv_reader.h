#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace metin2::db::inline tables {

    class c_csv_row : public std::vector<std::string> {
      public:
        c_csv_row() {}

        ~c_csv_row() {}

      public:
        auto as_string(size_t index) const -> const char * { return at(index).c_str(); }

      private:
        c_csv_row(const c_csv_row &) = delete;
        auto operator=(const c_csv_row &) -> c_csv_row & = delete;
    };

    class c_csv_file {
      private:
        typedef std::vector<std::unique_ptr<c_csv_row>> rows;

        rows rows_;

      public:
        c_csv_file() {}

        virtual ~c_csv_file() { destroy(); }

      public:
        auto load(const char *file_name, const char seperator = ',', const char quote = '"') -> bool;

        void destroy();

        auto operator[](size_t index) const -> const c_csv_row *;

        auto row_count() const -> size_t { return rows_.size(); }

      private:
        c_csv_file(const c_csv_file &) = delete;
        auto operator=(const c_csv_file &) -> c_csv_file & = delete;
    };

    class c_csv_table {
      public:
        c_csv_file file;

      private:
        int cur_row_;

      public:
        c_csv_table();

        virtual ~c_csv_table();

      public:
        auto load(const char *file_name, const char seperator = ',', const char quote = '"') -> bool;

        auto next() -> bool;

        auto as_string_by_index(size_t index) const -> const char *;

        void destroy();

      private:
        auto cur_row() const -> const c_csv_row *;

        c_csv_table(const c_csv_table &) = delete;
        auto operator=(const c_csv_table &) -> c_csv_table & = delete;
    };
}
