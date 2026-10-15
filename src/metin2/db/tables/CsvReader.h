#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace metin2::db::inline tables {

////////////////////////////////////////////////////////////////////////////////
/// \class cCsvRow 
/// \brief class encapsulating one row of a CSV file
///
/// The basic CSV format is each cell as seen in Excel separated by the ',' character.
/// However, if a cell contains the special characters ',' or '"', 
/// it looks a bit different. Here is an example of the change.
/// 
/// <pre>
/// As seen in Excel | As stored in the actual CSV file
/// ---------------------+----------------------------------------------------
/// ItemPrice            | ItemPrice
/// Item,Price           | "Item,Price"
/// Item"Price           | "Item""Price"
/// "ItemPrice"          | """ItemPrice"""
/// "Item,Price"         | """Item,Price"""
/// Item",Price          | "Item"",Price"
/// </pre>
/// 
/// From this example we can see the following.
/// - if a cell contains ',' or '"', '"' characters are added on both sides of the cell.
/// - '"' characters inside a cell are replaced with two.
///
/// \sa cCsvFile
////////////////////////////////////////////////////////////////////////////////

class c_csv_row : public std::vector<std::string>
{
public:
    /// \brief default constructor
    c_csv_row() {}

    /// \brief destructor
    ~c_csv_row() {}

public:
    /// \brief returns the cell's data as int.

    /// \brief returns the cell's data as double.

    /// \brief returns the cell's data as a string.
    const char* as_string(size_t index) const { return at(index).c_str(); }

    /// \brief returns the data of the cell with the given name as int.

    /// \brief returns the data of the cell with the given name as int.

    /// \brief returns the data of the cell with the given name as a string.

private:
    c_csv_row(const c_csv_row&) = delete;
    c_csv_row& operator = (const c_csv_row&) = delete;
};

////////////////////////////////////////////////////////////////////////////////
/// \class cCsvFile
/// \brief class for reading/writing CSV (Comma Seperated Values) files
///
/// <b>sample</b>
/// <pre>
/// cCsvFile file;
///
/// cCsvRow row1, row2, row3;
/// row1.push_back("ItemPrice");
/// row1.push_back("Item,Price");
/// row1.push_back("Item\"Price");
///
/// row2.reserve(3);
/// row2[0] = "\"ItemPrice\"";
/// row2[1] = "\"Item,Price\"";
/// row2[2] = "Item\",Price\"";
///
/// row3 = "\"ItemPrice\"\"Item,Price\"Item\",Price\"";
///
/// file.add(row1);
/// file.add(row2);
/// file.add(row3);
/// file.save("test.csv", false);
/// </pre>
///
/// \todo besides reading from files, there should probably be a function 
/// that reads from a memory source.
////////////////////////////////////////////////////////////////////////////////

class c_csv_file
{
private:
    typedef std::vector<std::unique_ptr<c_csv_row>> rows;

    rows rows_; ///< row collection

public:
    /// \brief constructor
    c_csv_file() {}

    /// \brief destructor
    virtual ~c_csv_file() { destroy(); }

public:
    /// \brief loads the CSV file with the given name.
    bool load(const char* fileName, const char seperator=',', const char quote='"');

    /// \brief saves the contents to a CSV file.

    /// \brief deletes all data from memory.
    void destroy();

    /// \brief returns the row at the given index.

    /// \brief returns the row at the given index.
    const c_csv_row* operator [] (size_t index) const;

    /// \brief returns the number of rows.
    size_t row_count() const { return rows_.size(); }

private:
    c_csv_file(const c_csv_file&) = delete;
    c_csv_file& operator = (const c_csv_file&) = delete;
};

////////////////////////////////////////////////////////////////////////////////
/// \class cCsvTable
/// \brief table data is often loaded from CSV files; this class is 
/// a utility class to make that easier.
///
/// <b>sample</b>
/// <pre>
/// cCsvTable table;
///
/// if (table.Load("test.csv"))
/// {
///     while (table.Next())
///     {
///         const char* item_class = table.AsStringByIndex(0);
///     }
/// }
/// </pre>
////////////////////////////////////////////////////////////////////////////////

class c_csv_table
{
public :
    c_csv_file  file;   ///< CSV file object
private:
    int       cur_row_; ///< row number currently being traversed

public:
    /// \brief constructor
    c_csv_table();

    /// \brief destructor
    virtual ~c_csv_table();

public:
    /// \brief loads the CSV file with the given name.
    bool load(const char* fileName, const char seperator=',', const char quote='"');

    /// \brief registers a name to use instead of a number when accessing cells.

    /// \brief moves to the next row.
    bool next();

    /// \brief returns the number of cells in the current row.

    /// \brief returns the cell value as int by index.

    /// \brief returns the cell value as double by index.

    /// \brief returns the cell value as std::string by index.
    const char* as_string_by_index(size_t index) const;

    /// \brief returns the cell value as int by cell name.

    /// \brief returns the cell value as double by cell name.

    /// \brief returns the cell value as std::string by cell name.

    /// \brief deletes all data, including aliases.
    void destroy();

private:
    /// \brief returns the current row.
    const c_csv_row* cur_row() const;

    c_csv_table(const c_csv_table&) = delete;
    c_csv_table& operator = (const c_csv_table&) = delete;
};
}
