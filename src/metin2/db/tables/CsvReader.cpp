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
    enum ParseState
    {
        STATE_NORMAL = 0, ///< Normal state
        STATE_QUOTE       ///< State after a quote
    };

    /// Returns the string with leading and trailing whitespace removed.
    std::string Trim(std::string str)
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
bool cCsvFile::Load(const char* fileName, const char seperator, const char quote)
{
    assert(seperator != quote);

    std::ifstream file(fileName, std::ios::in);
    if (!file) return false;

    Destroy(); // Delete existing data

    std::unique_ptr<cCsvRow> row;
    ParseState state = STATE_NORMAL;
    std::string token = "";
    char buf[2048+1] = {0,};

    while (file.good())
    {
        file.getline(buf, 2048);
        buf[sizeof(buf)-1] = 0;

        std::string line(Trim(buf));
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
                    row = std::make_unique<cCsvRow>();

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
            m_Rows.push_back(std::move(row));
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
void cCsvFile::Destroy()
{
    m_Rows.clear();
}


////////////////////////////////////////////////////////////////////////////////
/// \brief Returns the row at the given index.
/// \param index Index
/// \return const cCsvRow* The row
////////////////////////////////////////////////////////////////////////////////
const cCsvRow* cCsvFile::operator [] (size_t index) const
{
    assert(index < m_Rows.size());
    return m_Rows[index].get();
}

////////////////////////////////////////////////////////////////////////////////
/// \brief Constructor
////////////////////////////////////////////////////////////////////////////////
cCsvTable::cCsvTable()
: m_CurRow(-1)
{
}

////////////////////////////////////////////////////////////////////////////////
/// \brief Destructor
////////////////////////////////////////////////////////////////////////////////
cCsvTable::~cCsvTable()
{
}

////////////////////////////////////////////////////////////////////////////////
/// \brief Loads the CSV file with the given name.
/// \param fileName CSV file name
/// \param seperator Character used as the field separator. Defaults to ','.
/// \param quote Character used as the quote. Defaults to '"'.
/// \return bool true if loaded successfully, false otherwise
////////////////////////////////////////////////////////////////////////////////
bool cCsvTable::Load(const char* fileName, const char seperator, const char quote)
{
    Destroy();
    return m_File.Load(fileName, seperator, quote);
}

////////////////////////////////////////////////////////////////////////////////
/// \brief Advances to the next row.
/// \return bool Returns true if it advanced to the next row, and false if there
/// are no more rows to advance to.
////////////////////////////////////////////////////////////////////////////////
bool cCsvTable::Next()
{
    // This overflows after about 2 billion calls... should be fine?
    return ++m_CurRow < (int)m_File.GetRowCount() ? true : false;
}

////////////////////////////////////////////////////////////////////////////////
/// \brief Returns the cell value at the given index as std::string.
/// \param index Cell index
/// \return const char* Cell value
////////////////////////////////////////////////////////////////////////////////
const char* cCsvTable::AsStringByIndex(size_t index) const
{
    const cCsvRow* const row = CurRow();
    assert(row);
    assert(index < row->size());
    return row->AsString(index);
}

////////////////////////////////////////////////////////////////////////////////
/// \brief Deletes all data.
////////////////////////////////////////////////////////////////////////////////
void cCsvTable::Destroy()
{
    m_File.Destroy();
    m_CurRow = -1;
}

////////////////////////////////////////////////////////////////////////////////
/// \brief Returns the current row.
/// \return const cCsvRow* If an accessible current row exists, returns a pointer
/// to that row; if there are no more accessible rows, returns NULL
////////////////////////////////////////////////////////////////////////////////
const cCsvRow* cCsvTable::CurRow() const
{
    if (m_CurRow < 0)
    {
        assert(false && "call Next() first!");
        return nullptr;
    }
    else if (m_CurRow >= (int)m_File.GetRowCount())
    {
        assert(false && "no more rows!");
        return nullptr;
    }

    return m_File[m_CurRow];
}
}
