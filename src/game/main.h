#pragma once

namespace metin2::game {

// State owned by the main loop (main.cpp) and the build version (version.cpp).
extern bool g_bShutdown;

void ContinueOnFatalError();
}
