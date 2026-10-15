#pragma once

namespace metin2::game::inline text {

void locale_init(const char *filename);
const char *locale_find(const char *string);

inline const char * lc_text(const char * str) { return locale_find(str); }
}
