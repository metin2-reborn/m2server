#include <metin2/core/utils.h>

#include <metin2/log/log.h>

#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

namespace metin2::game::inline app {

    static int global_time_gap = 0;

    auto get_global_time() -> time_t {
        return time(nullptr) + global_time_gap;
    }

    void set_global_time(time_t t) {
        global_time_gap = t - time(nullptr);

        metin2::log::info("Global time set to {} (offset {:d} seconds)", time_str(get_global_time()), global_time_gap);
    }

    auto str_lower(const char *src, char *dest, size_t dest_size) -> size_t {
        size_t len = 0;

        if (!dest || dest_size == 0)
            return len;

        if (!src) {
            *dest = '\0';
            return len;
        }

        --dest_size;

        while (*src && len < dest_size) {
            *dest = std::tolower(static_cast<unsigned char>(*src));

            ++src;
            ++dest;
            ++len;
        }

        *dest = '\0';
        return len;
    }

    static void skip_spaces(const char **string) {
        for (; **string != '\0' && is_space(**string); ++(*string))
            ;
    }

    auto one_argument(const char *argument, char *first_arg, size_t first_size) -> const char * {
        char mark = false;
        size_t first_len = 0;

        if (!argument || 0 == first_size) {
            metin2::log::error("Argument parser called without input");
            *first_arg = '\0';
            return nullptr;
        }

        --first_size;

        skip_spaces(&argument);

        while (*argument && first_len < first_size) {
            if (*argument == '\"') {
                mark = !mark;
                ++argument;
                continue;
            }

            if (!mark && is_space(*argument))
                break;

            *(first_arg++) = *argument;
            ++argument;
            ++first_len;
        }

        *first_arg = '\0';

        skip_spaces(&argument);
        return (argument);
    }

    auto two_arguments(const char *argument, char *first_arg, size_t first_size, char *second_arg, size_t second_size) -> const char * {
        return (one_argument(one_argument(argument, first_arg, first_size), second_arg, second_size));
    }

    auto first_cmd(const char *argument, char *first_arg, size_t first_arg_size, size_t *first_arg_len_result) -> const char * {
        size_t cur_len = 0;
        skip_spaces(&argument);

        first_arg_size -= 1;

        while (*argument && !is_space(*argument) && cur_len < first_arg_size) {
            *(first_arg++) = std::tolower(static_cast<unsigned char>(*argument));
            ++argument;
            ++cur_len;
        }

        *first_arg_len_result = cur_len;
        *first_arg = '\0';
        return (argument);
    }

    auto calculate_duration(int spd, int dur) -> int {
        int i = 100 - spd;

        if (i > 0)
            i = 100 + i;
        else if (i < 0)
            i = 10000 / (100 - i);
        else
            i = 100;

        return dur * i / 100;
    }
    auto parse_time_str(const char *str) -> int {
        int tmp = 0;
        int secs = 0;

        while (*str != 0) {
            switch (*str) {
            case 'm':
            case 'M':
                secs += tmp * 60;
                tmp = 0;
                break;

            case 'h':
            case 'H':
                secs += tmp * 3600;
                tmp = 0;
                break;

            case 'd':
            case 'D':
                secs += tmp * 86400;
                tmp = 0;
                break;

            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                tmp *= 10;
                tmp += (*str) - '0';
                break;

            case 's':
            case 'S':
                secs += tmp;
                tmp = 0;
                break;
            default:
                return -1;
            }
            ++str;
        }

        return secs + tmp;
    }

    auto wild_case_cmp(const char *w, const char *s) -> bool {
        auto same = [](char a, char b) -> bool {
            return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
        };

        const char *star = nullptr;
        const char *resume = nullptr;

        while (*s) {
            if (*w == '*') {
                star = ++w;
                resume = s;
            } else if (*w && (*w == '?' || same(*w, *s))) {
                ++w;
                ++s;
            } else if (star) {
                w = star;
                s = ++resume;
            } else {
                return false;
            }
        }

        while (*w == '*')
            ++w;

        return !*w;
    }
}
