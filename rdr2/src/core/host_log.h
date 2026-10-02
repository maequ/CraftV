// RedCraft.log next to the game (brief §6). Timestamped, flushed per line, safe from any thread.
// Formatting uses a stack buffer; the file is opened once at startup.
#pragma once

#include "redcraft/protocol.h"

#include <string>

namespace redcraft::host
{
	class HostLog
	{
	public:
		static void Open(const std::wstring& a_path);
		static void Close();
		static void Info(const char* a_fmt, ...);
		static void Warn(const char* a_fmt, ...);
		static void Error(const char* a_fmt, ...);
		// For redcraft::EndpointConfig::log (level = proto::LogLevel).
		static void FromLink(void* a_user, redcraft::proto::LogLevel a_level, const char* a_text);
	};
}
