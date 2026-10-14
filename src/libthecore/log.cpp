/*
 *    Filename: log.cpp
 * Description: local log file related
 *
 *      Author: Biyeop aka. Cronan
 */
#include <libthecore/log.h>

#include "internal.h"

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

namespace
{
	struct FileCloser
	{
		void operator()(std::FILE * fp) const { std::fclose(fp); }
	};
	using FilePtr = std::unique_ptr<std::FILE, FileCloser>;

	struct LogFile
	{
		std::string	filename;
		std::string	openmode;
		FilePtr		fp;
		int		last_hour = 0;
		int		last_day = 0;

		LogFile(std::string name, std::string mode) : filename(std::move(name)), openmode(std::move(mode)) {}

		bool open()
		{
			fp.reset(std::fopen(filename.c_str(), openmode.c_str()));
			return fp != nullptr;
		}
	};

	std::unique_ptr<LogFile>	log_file_sys;
	std::unique_ptr<LogFile>	log_file_err;
	std::unique_ptr<LogFile>	log_file_pt;

	const fs::path			log_dir = "./log";
	unsigned int			log_keep_days = 3;
	unsigned int			log_level_bits = 0;

	std::tm local_now()
	{
		const std::time_t now = std::time(nullptr);
		return *std::localtime(&now);
	}

	// "YYYYMMDD" of a day relative to today (or to curr when given).
	std::string day_name(const std::tm * curr, int days)
	{
		std::tm t = curr ? *curr : local_now();
		t.tm_mday += days;	// mktime normalises the day across month and year boundaries
		t.tm_isdst = -1;
		std::mktime(&t);
		char buf[16];
		std::snprintf(buf, sizeof(buf), "%04d%02d%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
		return buf;
	}

	// "Mon dd hh:mm:ss" (asctime without the weekday and year).
	std::string time_stamp()
	{
		const std::time_t now = std::time(nullptr);
		std::string s = std::asctime(std::localtime(&now));
		return s.substr(4, 15);
	}

	std::unique_ptr<LogFile> log_file_init(const char * filename, const char * openmode)
	{
		auto file = std::make_unique<LogFile>(filename, openmode);
		if (!file->open())
			return nullptr;

		const std::tm curr = local_now();
		file->last_hour = curr.tm_hour;
		file->last_day = curr.tm_mday;
		return file;
	}

	// The file was deleted from outside: open it again.
	void log_file_check(LogFile * file)
	{
		std::error_code ec;
		if (!fs::exists(file->filename, ec))
			file->open();
	}

	void remove_dir(const fs::path & dir)
	{
		std::error_code ec;
		fs::remove_all(dir, ec);
		sys_log(0, "removed log directory %s", dir.c_str());
	}

	// Dated log directories at least log_keep_days old go.
	void log_file_delete_old()
	{
		std::error_code ec;
		if (!fs::is_directory(log_dir, ec))
			return;

		const long oldest_kept = std::atol(day_name(nullptr, -static_cast<int>(log_keep_days)).c_str());

		for (const auto & entry : fs::directory_iterator(log_dir, ec))
		{
			const std::string name = entry.path().filename().string();
			if (name.empty() || name[0] == '.' || !std::isdigit(static_cast<unsigned char>(name[0])))
				continue;

			if (std::atol(name.c_str()) <= oldest_kept)
				remove_dir(entry.path());
		}
	}

	void log_file_rotate(LogFile * file)
	{
		const std::tm curr = local_now();

		if (curr.tm_mday != file->last_day)
		{
			remove_dir(log_dir / day_name(&curr, -3));
			file->last_day = curr.tm_mday;
		}

		if (curr.tm_hour != file->last_hour)
		{
			const fs::path dir = log_dir / day_name(&curr, 0);
			std::error_code ec;
			fs::create_directories(dir, ec);

			sys_log(0, "SYSTEM: LOG ROTATE (%04d-%02d-%02d %d)",
					curr.tm_year + 1900, curr.tm_mon + 1, curr.tm_mday, file->last_hour);

			// close the log file, move it, then reopen.
			file->fp.reset();
			char suffix[16];
			std::snprintf(suffix, sizeof(suffix), ".%02d", file->last_hour);
			fs::rename(file->filename, dir / (file->filename + suffix), ec);

			file->last_hour = curr.tm_hour;
			file->open();
		}
	}
}

void log_set_level(unsigned int bit)
{
	log_level_bits |= bit;
}

void log_set_expiration_days(unsigned int days)
{
	log_keep_days = days;
}

bool log_init()
{
	log_file_delete_old();

	log_file_sys = log_file_init("syslog", "a+");
	log_file_err = log_file_init("syserr", "a+");
	log_file_pt = log_file_init("PTS", "w");
	return log_file_sys && log_file_err && log_file_pt;
}

void log_destroy()
{
	log_file_sys.reset();
	log_file_err.reset();
	log_file_pt.reset();
}

void log_rotate()
{
	for (LogFile * file : {log_file_sys.get(), log_file_err.get(), log_file_pt.get()})
		if (file)
			log_file_check(file);

	if (log_file_sys)
		log_file_rotate(log_file_sys.get());
}

void _sys_err(const char * func, int line, const char * format, ...)
{
	if (!log_file_err || !log_file_err->fp || !log_file_sys || !log_file_sys->fp)
		return;

	char buf[1024 + 2]; // to append \n..
	int len = std::snprintf(buf, 1024, "SYSERR: %s :: %s: ", time_stamp().c_str(), func);

	if (len < 1024)
	{
		va_list args;
		va_start(args, format);
		std::vsnprintf(buf + len, 1024 - len, format, args);
		va_end(args);
	}

	// output to log_file_err and log_file_sys
	for (LogFile * file : {log_file_err.get(), log_file_sys.get()})
	{
		std::fputs(buf, file->fp.get());
		std::fputc('\n', file->fp.get());
		std::fflush(file->fp.get());
	}
}

void sys_log(unsigned int bit, const char * format, ...)
{
	if (bit != 0 && !(log_level_bits & bit))
		return;

	if (log_file_sys && log_file_sys->fp)
	{
		std::FILE * fp = log_file_sys->fp.get();
		std::fprintf(fp, "%s :: ", time_stamp().c_str());

		va_list args;
		va_start(args, format);
		std::vfprintf(fp, format, args);
		va_end(args);

		std::fputc('\n', fp);
		std::fflush(fp);
	}

	// if log_level is 1 or higher it's usually a test, so also print to stdout.
	if (log_level_bits > 1)
	{
		va_list args;
		va_start(args, format);
		std::vfprintf(stdout, format, args);
		va_end(args);

		std::fputc('\n', stdout);
		std::fflush(stdout);
	}
}

void pt_log(const char * format, ...)
{
	if (!log_file_pt || !log_file_pt->fp)
		return;

	va_list args;
	va_start(args, format);
	std::vfprintf(log_file_pt->fp.get(), format, args);
	va_end(args);

	std::fputc('\n', log_file_pt->fp.get());
	std::fflush(log_file_pt->fp.get());
}
