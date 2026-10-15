
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ctime>

namespace metin2::game::inline app {

template <typename T, typename U> constexpr auto is_set(T flag, U bit) { return flag & bit; }
template <typename T, typename U> constexpr void set_bit(T & var, U bit) { var |= bit; }
template <typename T, typename U> constexpr void remove_bit(T & var, U bit) { var &= ~bit; }

inline float distance_sqrt(int32_t dx, int32_t dy)
{
    return std::sqrt((float)dx * dx + (float)dy * dy);
}

inline int distance_approx(int dx, int dy)
{
	int min, max;

	if (dx < 0)
		dx = -dx;

	if (dy < 0)
		dy = -dy;

	if (dx < dy)
	{
		min = dx;
		max = dy;
	}
	else
	{
		min = dy;
		max = dx;
	}

    // coefficients equivalent to ( 123/128 * max ) and ( 51/128 * min )
    return ((( max << 8 ) + ( max << 3 ) - ( max << 4 ) - ( max << 1 ) +
		( min << 7 ) - ( min << 5 ) + ( min << 3 ) - ( min << 1 )) >> 8 );
}

extern void set_global_time(time_t t);
extern time_t get_global_time();

extern size_t str_lower(const char * src, char * dest, size_t dest_size);

extern void	skip_spaces(char **string);

extern const char *	one_argument(const char *argument, char *first_arg, size_t first_size);
extern const char *	two_arguments(const char *argument, char *first_arg, size_t first_size, char *second_arg, size_t second_size);
extern const char *	first_cmd(const char *argument, char *first_arg, size_t first_arg_size, size_t *first_arg_len_result);

extern int calculate_duration(int spd, int dur);

extern float gauss_random(float avg = 0, float sigma = 1);

extern int parse_time_str(const char* str);

extern bool wild_case_cmp(const char *w, const char *s);

// Inside a config parser loop: compares the current token_string, ignoring case.
#define TOKEN(string) if (!strcasecmp(token_string, string))
}
