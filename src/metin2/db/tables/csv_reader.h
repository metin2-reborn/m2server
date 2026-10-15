#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace metin2::db::inline tables {


class c_csv_row : public std::vector<std::string>
{
public:
    c_csv_row() {}

    ~c_csv_row() {}

public:


    const char* as_string(size_t index) const { return at(index).c_str(); }




private:
    c_csv_row(const c_csv_row&) = delete;
    c_csv_row& operator = (const c_csv_row&) = delete;
};


class c_csv_file
{
private:
    typedef std::vector<std::unique_ptr<c_csv_row>> rows;

    rows rows_;

public:
    c_csv_file() {}

    virtual ~c_csv_file() { destroy(); }

public:
    bool load(const char* file_name, const char seperator=',', const char quote='"');


    void destroy();


    const c_csv_row* operator [] (size_t index) const;

    size_t row_count() const { return rows_.size(); }

private:
    c_csv_file(const c_csv_file&) = delete;
    c_csv_file& operator = (const c_csv_file&) = delete;
};


class c_csv_table
{
public :
    c_csv_file  file;
private:
    int       cur_row_;

public:
    c_csv_table();

    virtual ~c_csv_table();

public:
    bool load(const char* file_name, const char seperator=',', const char quote='"');


    bool next();




    const char* as_string_by_index(size_t index) const;




    void destroy();

private:
    const c_csv_row* cur_row() const;

    c_csv_table(const c_csv_table&) = delete;
    c_csv_table& operator = (const c_csv_table&) = delete;
};
}
