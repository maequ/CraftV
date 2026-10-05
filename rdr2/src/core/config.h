// CraftV_RDR2.ini (brief §11): read once at startup with GetPrivateProfileString. Every value has a
// default, so a missing or broken file still gives a working plugin.
#pragma once

#include "coords.h"

#include <cstdint>
#include <string>

namespace craftv::host
{
	struct Config
	{
		// [Link]
		std::wstring  mappingName = L"Local\\CraftV_Shared_v1";
		std::uint64_t mcTimeoutMs = 0;  // 0 = protocol default (3000)
		// [World]
		WorldConfig world{};
		// [Debug]
		bool debugOverlay = true;
		bool logEveryTickCost = false;

		// Reads a_iniPath; missing keys keep their defaults. Returns false if the file doesn't exist.
		bool Load(const std::wstring& a_iniPath);
	};
}
