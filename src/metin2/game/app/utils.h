
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ctime>

namespace metin2::game::inline app {

    template <typename T, typename U>
    constexpr auto is_set(T flag, U bit) { return flag & bit; }
    template <typename T, typename U>
    constexpr void set_bit(T &var, U bit) { var |= bit; }
    template <typename T, typename U>
    constexpr void remove_bit(T &var, U bit) { var &= ~bit; }

    inline auto distance_sqrt(int32_t dx, int32_t dy) -> float {
        return std::sqrt((float)dx * dx + (float)dy * dy);
    }

    inline auto distance_approx(int dx, int dy) -> int {
        int min, max;

        if (dx < 0)
            dx = -dx;

        if (dy < 0)
            dy = -dy;

        if (dx < dy) {
            min = dx;
            max = dy;
        } else {
            min = dy;
            max = dx;
        }

        return (((max << 8) + (max << 3) - (max << 4) - (max << 1) +
                    (min << 7) - (min << 5) + (min << 3) - (min << 1)) >>
                8);
    }

    extern void set_global_time(time_t t);
    extern auto get_global_time() -> time_t;

    extern auto str_lower(const char *src, char *dest, size_t dest_size) -> size_t;

    extern void skip_spaces(char **string);

    extern auto one_argument(const char *argument, char *first_arg, size_t first_size) -> const char *;
    extern auto two_arguments(const char *argument, char *first_arg, size_t first_size, char *second_arg, size_t second_size) -> const char *;
    extern auto first_cmd(const char *argument, char *first_arg, size_t first_arg_size, size_t *first_arg_len_result) -> const char *;

    extern auto calculate_duration(int spd, int dur) -> int;


    extern auto parse_time_str(const char *str) -> int;

    extern auto wild_case_cmp(const char *w, const char *s) -> bool;

#define TOKEN(string) if (!strcasecmp(token_string, string))
}
