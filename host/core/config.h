// The plugin's .ini (CraftV.ini for GTA V, CraftV_RDR2.ini for RDR2; brief §13): read once at startup with
// GetPrivateProfileString. Every value has a default, so a missing or broken file still gives a working plugin.
#pragma once

#include "coords.h"
#include "terrain_scanner.h"

#include <cstdint>
#include <string>

namespace craftv::host
{
	enum class OverlayCorner
	{
		kTopRight,  // default: below GTA's own HUD on the right
		kTopLeft,
	};

	struct Config
	{
		// Set by the ASI, not the .ini: the name in HELLO and the logs, e.g. "CraftV-GTA5 0.1.0".
		std::string software = "CraftV-Host 0.1.0";
		// [Link]
		std::wstring  mappingName = L"Local\\CraftV_Shared_v1";
		std::uint64_t mcTimeoutMs = 0;  // 0 = protocol default (3000)
		// [World]
		WorldConfig world{};
		// [Terrain]
		TerrainConfig terrain{};
		// [Debug]
		bool          debugOverlay = true;
		OverlayCorner overlayCorner = OverlayCorner::kTopRight;
		bool          overlayDetails = false;
		bool logEveryTickCost = false;

		// Reads a_iniPath; missing keys keep their defaults. Returns false if the file doesn't exist.
		bool Load(const std::wstring& a_iniPath);
	};
}
