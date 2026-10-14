#ifndef __INC_LIBTHECORE_UTILS_H__
#define __INC_LIBTHECORE_UTILS_H__

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <source_location>

// Logs where it was called from, then dumps core from a forked child; the server keeps running.
void		core_dump(std::source_location where = std::source_location::current());

void		parse_token(char * src, char * token, char * value);
void		trim_and_lower(const char * src, char * dest, size_t dest_size);

size_t		utf8_sequence_length(unsigned char c);
size_t		utf8_truncated_size(const char * s, size_t len, size_t max);
size_t		utf8_strlcpy(char * dst, const char * src, size_t size);

// A random integer in [from, to]; swapped arguments are logged with the caller and corrected.
int		number(int from, int to, std::source_location where = std::source_location::current());
float		fnumber(float from, float to);

uint32_t		thecore_random();				// Random functions

float		get_float_time();
uint32_t		get_dword_time();
char *		time_str(time_t ct);

#endif	// __INC_UTILS_H__
