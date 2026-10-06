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

	enum class PassthroughMode
	{
		kAuto,  // on whenever the compositor (ReShade add-on) is loaded; F7 toggles
		kOff,   // never (F7 still turns it on)
	};

	struct PassthroughConfig
	{
		PassthroughMode mode = PassthroughMode::kAuto;
		std::uint64_t   maxPixels = 1600ull * 900;  // Minecraft's frame at most this many pixels (the effect scales it up)
		bool            hideGtaHud = false;          // hide the game's own HUD and minimap while the passthrough is on
		double          meleeDamagePerHalfHeart = 10.0;  // game damage per Minecraft half heart of attack damage
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
		// [Passthrough]
		PassthroughConfig passthrough{};
		// [Debug]
		bool          debugOverlay = true;
		OverlayCorner overlayCorner = OverlayCorner::kTopRight;
		bool          overlayDetails = false;
		bool logEveryTickCost = false;

		// Reads a_iniPath; missing keys keep their defaults. Returns false if the file doesn't exist.
		bool Load(const std::wstring& a_iniPath);
		// Writes what the settings menu changes back to the file Load read (no-op without one).
		bool Save() const;
		std::wstring iniPath;
	};
}
