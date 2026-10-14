#ifndef __INC_METIN2_GAME_LOCALE_H__
#define __INC_METIN2_GAME_LOCALE_H__

void locale_init(const char *filename);
const char *locale_find(const char *string);

inline const char * LC_TEXT(const char * str) { return locale_find(str); }

#endif
