/*
 *    Filename: hangul.cpp
 * Description: Hangul-related implementation
 *
 *      Author: Biyeop aka. Cronan
 */
#include <libthecore/hangul.h>

#include <cstring>

bool under_han(const char * text)
{
    if (!text)
	return false;

    size_t len = std::strlen(text);

    // "이름(10)" -> "이름"
    if (len > 0 && text[len - 1] == ')')
    {
	const char * open = std::strrchr(text, '(');
	if (!open)
	    return false;
	len = static_cast<size_t>(open - text);
    }

    // A Hangul syllable (U+AC00..U+D7A3) is three bytes in UTF-8: 1110xxxx 10xxxxxx 10xxxxxx.
    if (len < 3)
	return false;

    const auto * p = reinterpret_cast<const unsigned char *>(text + len - 3);
    if ((p[0] & 0xF0) != 0xE0 || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80)
	return false;

    const unsigned cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    if (cp < 0xAC00 || cp > 0xD7A3)
	return false;

    // Syllables are ordered initial*588 + medial*28 + final; final 0 means no final consonant (batchim).
    return (cp - 0xAC00) % 28 != 0;
}
