#pragma once

#include <libthecore/fdwatch.h>

// State owned by the main loop (main.cpp) and the build version (version.cpp).
extern fdwatch * main_fdw;
extern bool g_bShutdown;

extern int max_bytes_written;
extern int current_bytes_written;
extern int total_bytes_written;

void ContinueOnFatalError();
void WriteVersion();
