/*********************************************************************
 * date        : 2006.09.07
 * file        : dev_log.cpp
 * author      : mhh
 */
#include "dev_log.h"

#include "config.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>

#include <sys/time.h>


void dev_log(const char *file, int line, const char *function, int level, const char *fmt, ...)
{
	if (!test_server)
		return;

	FILE * fp = std::fopen("DEV_LOG.log", "a");
	if (!fp)
		return;

	timeval tv;
	gettimeofday(&tv, nullptr);
	tm ltm;
	localtime_r(&tv.tv_sec, &ltm);

	const char * strlevel = level == L_INFO ? "INFO" : level == L_DEB0 ? "DEB0" : level == L_DEB1 ? "DEB1" : "UNKNOWN";

	// "MMDD hh:mm.ss.uuuu LEVL (file, line, function) message"
	std::fprintf(fp, "%02d%02d %02d:%02d.%02d.%04ld %-4s (%-15s,%4d,%-24s) ", ltm.tm_mon + 1, ltm.tm_mday, ltm.tm_hour, ltm.tm_min,
			ltm.tm_sec, static_cast<long>(tv.tv_usec / 100), strlevel, file, line, function);

	va_list args;
	va_start(args, fmt);
	std::vfprintf(fp, fmt, args);
	va_end(args);

	std::fputc('\n', fp);
	std::fclose(fp);
}
