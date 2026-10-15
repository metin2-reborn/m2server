#pragma once

namespace metin2::game::inline app {

// State owned by the main loop (main.cpp) and the build version (version.cpp).
extern bool shutting_down;

void continue_on_fatal_error();
}
