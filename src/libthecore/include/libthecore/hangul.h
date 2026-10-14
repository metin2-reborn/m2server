#ifndef __INC_LIBTHECORE_HANGUL_H__
#define __INC_LIBTHECORE_HANGUL_H__

#include <cctype>

// isdigit/isspace for one byte of UTF-8 text (bytes of multi-byte characters are neither).
inline bool isnhdigit(char ch) { return std::isdigit(static_cast<unsigned char>(ch)) != 0; }
inline bool isnhspace(char ch) { return std::isspace(static_cast<unsigned char>(ch)) != 0; }

// True when the last character of the UTF-8 text (ignoring a trailing "(...)") is a Hangul
// syllable with a final consonant (받침), which selects 을/를, 이/가 and similar particles.
bool under_han(const char * text);

#endif
