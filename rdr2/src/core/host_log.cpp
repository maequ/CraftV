#include "host_log.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <share.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace redcraft::host
{
	namespace
	{
		constexpr std::size_t kLineBytes = 512;
		FILE*                 g_file = nullptr;
		std::mutex            g_mutex;

		void Write(const char* a_level, const char* a_fmt, va_list a_args)
		{
			char line[kLineBytes];
			std::vsnprintf(line, sizeof(line), a_fmt, a_args);
			SYSTEMTIME t;
			::GetLocalTime(&t);
			std::lock_guard lock(g_mutex);
			if (g_file) {
				std::fprintf(g_file, "%04d-%02d-%02d %02d:%02d:%02d.%03d [%s] %s\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond,
					t.wMilliseconds, a_level, line);
				std::fflush(g_file);
			}
		}
	}

	void HostLog::Open(const std::wstring& a_path)
	{
		std::lock_guard lock(g_mutex);
		if (!g_file) {
			g_file = _wfsopen(a_path.c_str(), L"a", _SH_DENYNO);  // readable while the game runs
		}
	}

	void HostLog::Close()
	{
		std::lock_guard lock(g_mutex);
		if (g_file) {
			std::fclose(g_file);
			g_file = nullptr;
		}
	}

	void HostLog::Info(const char* a_fmt, ...)
	{
		va_list args;
		va_start(args, a_fmt);
		Write("INFO", a_fmt, args);
		va_end(args);
	}

	void HostLog::Warn(const char* a_fmt, ...)
	{
		va_list args;
		va_start(args, a_fmt);
		Write("WARN", a_fmt, args);
		va_end(args);
	}

	void HostLog::Error(const char* a_fmt, ...)
	{
		va_list args;
		va_start(args, a_fmt);
		Write("ERROR", a_fmt, args);
		va_end(args);
	}

	void HostLog::FromLink(void*, redcraft::proto::LogLevel a_level, const char* a_text)
	{
		if (a_level >= redcraft::proto::kLogError) {
			Error("link: %s", a_text);
		} else if (a_level == redcraft::proto::kLogWarn) {
			Warn("link: %s", a_text);
		} else {
			Info("link: %s", a_text);
		}
	}
}
