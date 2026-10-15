#pragma once

// Developer log, written on test servers only, to DEV_LOG.log in the working directory:
//   dev_log(LOG_DEB0, "My Name is %s", name);
inline constexpr int L_INFO = 1 << 4;
inline constexpr int L_DEB0 = 1 << 11;
inline constexpr int L_DEB1 = 1 << 12;

#define LOG_INFO __FILE__, __LINE__, __FUNCTION__, L_INFO
#define LOG_DEB0 __FILE__, __LINE__, __FUNCTION__, L_DEB0
#define LOG_DEB1 __FILE__, __LINE__, __FUNCTION__, L_DEB1

void dev_log(const char *file, int line, const char *function, int level, const char *fmt, ...) __attribute__((format(printf, 5, 6)));
