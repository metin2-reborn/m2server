#include <metin2/sql/query.h>

#include <string>
#include <string_view>

namespace metin2::sql {
    auto escape(std::string_view value) -> std::string {
        std::string result;
        result.reserve(value.size());

        for (const char ch : value) {
            switch (ch) {
            case '\0':
                result += "\\0";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\\':
                result += "\\\\";
                break;
            case '\'':
                result += "\\'";
                break;
            case '"':
                result += "\\\"";
                break;
            case '\x1a':
                result += "\\Z";
                break;
            default:
                result += ch;
                break;
            }
        }

        return result;
    }
}
