#pragma once

// syslog (everything), syserr (errors, also copied to syslog) and PTS in the working directory;
// rotated hourly into ./log/YYYYMMDD/.
void log_rotate();
void log_set_level(unsigned int level);
void log_set_expiration_days(unsigned int days);

void _sys_err(const char *func, int line, const char *format, ...) __attribute__((format(printf, 3, 4)));
void sys_log(unsigned int lv, const char *format, ...) __attribute__((format(printf, 2, 3)));
void pt_log(const char *format, ...) __attribute__((format(printf, 1, 2)));

#define sys_err(fmt, ...) _sys_err(__FUNCTION__, __LINE__, fmt __VA_OPT__(, ) __VA_ARGS__)
