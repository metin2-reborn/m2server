#include <metin2/db/tables/CsvReader.h>

#include <cassert>
#include <cstddef>
#include <fstream>
#include <ios>
#include <memory>
#include <string>
#include <utility>

namespace metin2::db::inline tables {


namespace
{
    /// Parser state enum
    enum parse_state
    {
        STATE_NORMAL = 0, ///< Normal state
        STATE_QUOTE       ///< State after a quote
    };

    /// Returns the string with leading and trailing whitespace removed.
    std::string trim(std::string str)
    {
        str = str.erase(str.find_last_not_of(" \t\r\n") + 1);
        str = str.erase(0, str.find_first_not_of(" \t\r\n"));
        return str;
    }

    /// \brief Converts all letters in the given string to lowercase.
}




////////////////////////////////////////////////////////////////////////////////
/// \brief Loads the CSV file with the given name.
/// \param fileName CSV file name
/// \param seperator Character used as the field separator. Defaults to ','.
/// \param quote Character used as the quote. Defaults to '"'.
/// \return bool true if loaded successfully, false otherwise
////////////////////////////////////////////////////////////////////////////////
bool c_csv_file::load(const char* fileName, const char seperator, const char quote)
{
    assert(seperator != quote);

    std::ifstream file(fileName, std::ios::in);
    if (!file) return false;

    destroy(); // Delete existing data

    std::unique_ptr<c_csv_row> row;
    parse_state state = STATE_NORMAL;
    std::string token = "";
    char buf[2048+1] = {0,};

    while (file.good())
    {
        file.getline(buf, 2048);
        buf[sizeof(buf)-1] = 0;

        std::string line(trim(buf));
        if (line.empty() || (state == STATE_NORMAL && line[0] == '#')) continue;
        
        std::string text  = std::string(line) + "  "; // Appended for the parser lookahead.
        size_t cur = 0;

        while (cur < text.size())
        {
            // When the current mode is QUOTE,
            if (state == STATE_QUOTE)
            {
                // there are two kinds of '"' characters:
                // 1. The ones on either side of a cell, marking that the cell contains special characters
                // 2. A '"' inside a cell that was escaped as two '"'
                // Of these, the left one of case 1 will, if the CSV file is well-formed, 
                // always be caught by STATE_NORMAL.
                // So what gets caught here is only the right one of case 1, or case 2.
                // In case 2 the '"' characters always come in pairs. But that is not true for
                // the right one of case 1. Writing the code based on that...
                if (text[cur] == quote)
                {
                    // If the next character is '"', i.e. consecutive '"' characters, 
                    // this is an escaped '"' inside the cell.
                    if (text[cur+1] == quote)
                    {
                        token += quote;
                        ++cur;
                    }
                    // If the next character is not '"', 
                    // the current '"' marks the end of the cell.
                    else
                    {
                        state = STATE_NORMAL;
                    }
                }
                else
                {
                    token += text[cur];
                }
            }
            // When the current mode is NORMAL,
            else if (state == STATE_NORMAL)
            {
                if (row == nullptr)
                    row = std::make_unique<c_csv_row>();

                // a ',' character marks the end of a cell.
                // Push the token into the cell list and reset the token.
                if (text[cur] == seperator)
                {
                    row->push_back(token);
                    token.clear();
                }
                // On a '"' character, switch to QUOTE mode.
                else if (text[cur] == quote)
                {
                    state = STATE_QUOTE;
                }
                // Any other character is appended to the current token.
                else
                {
                    token += text[cur];
                }
            }

            ++cur;
        }

        // The last cell has no trailing ',' so it must be added here.
        // But strip the two spaces appended at the start for the parser lookahead.
        if (state == STATE_NORMAL)
        {
            assert(row != nullptr);
            row->push_back(token.substr(0, token.size()-2));
            rows_.push_back(std::move(row));
            token.clear();
        }
        else
        {
            token = token.substr(0, token.size()-2) + "\r\n";
        }
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
/// \brief Deletes all data from memory.
////////////////////////////////////////////////////////////////////////////////
void c_csv_file::destroy()
{
    rows_.clear();
}


////////////////////////////////////////////////////////////////////////////////
/// \brief Returns the row at the given index.
/// \param index Index
/// \return const cCsvRow* The row
////////////////////////////////////////////////////////////////////////////////
const c_csv_row* c_csv_file::operator [] (size_t index) const
{
    assert(index < rows_.size());
    return rows_[index].get();
}

////////////////////////////////////////////////////////////////////////////////
/// \brief Constructor
////////////////////////////////////////////////////////////////////////////////
c_csv_table::c_csv_table()
: cur_row_(-1)
{
}

////////////////////////////////////////////////////////////////////////////////
/// \brief Destructor
////////////////////////////////////////////////////////////////////////////////
c_csv_table::~c_csv_table()
{
}

////////////////////////////////////////////////////////////////////////////////
/// \brief Loads the CSV file with the given name.
/// \param fileName CSV file name
/// \param seperator Character used as the field separator. Defaults to ','.
/// \param quote Character used as the quote. Defaults to '"'.
/// \return bool true if loaded successfully, false otherwise
////////////////////////////////////////////////////////////////////////////////
bool c_csv_table::load(const char* fileName, const char seperator, const char quote)
{
    destroy();
    return file.load(fileName, seperator, quote);
}

////////////////////////////////////////////////////////////////////////////////
/// \brief Advances to the next row.
/// \return bool Returns true if it advanced to the next row, and false if there
/// are no more rows to advance to.
////////////////////////////////////////////////////////////////////////////////
bool c_csv_table::next()
{
    // This overflows after about 2 billion calls... should be fine?
    return ++cur_row_ < (int)file.row_count() ? true : false;
}

////////////////////////////////////////////////////////////////////////////////
/// \brief Returns the cell value at the given index as std::string.
/// \param index Cell index
/// \return const char* Cell value
////////////////////////////////////////////////////////////////////////////////
const char* c_csv_table::as_string_by_index(size_t index) const
{
    const c_csv_row* const row = cur_row();
    assert(row);
    assert(index < row->size());
    return row->as_string(index);
}

////////////////////////////////////////////////////////////////////////////////
/// \brief Deletes all data.
////////////////////////////////////////////////////////////////////////////////
void c_csv_table::destroy()
{
    file.destroy();
    cur_row_ = -1;
}

////////////////////////////////////////////////////////////////////////////////
/// \brief Returns the current row.
/// \return const cCsvRow* If an accessible current row exists, returns a pointer
/// to that row; if there are no more accessible rows, returns NULL
////////////////////////////////////////////////////////////////////////////////
const c_csv_row* c_csv_table::cur_row() const
{
    if (cur_row_ < 0)
    {
        assert(false && "call Next() first!");
        return nullptr;
    }
    else if (cur_row_ >= (int)file.row_count())
    {
        assert(false && "no more rows!");
        return nullptr;
    }

    return file[cur_row_];
}
}
