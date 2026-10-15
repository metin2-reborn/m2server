#pragma once

namespace metin2::game::inline text {

void locale_init(const char *filename);
auto locale_find(const char *string) -> const char *;

inline auto lc_text(const char * str) -> const char * { return locale_find(str); }
}
