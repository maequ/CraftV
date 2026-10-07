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

	// [Blocks]: Minecraft blocks made solid in the game (brief §9).
	struct BlocksConfig
	{
		int   maxProps = 400;         // GTA crashes at ~1500 script objects (measured by minecraft-gta5-passthrough)
		float spawnRadius = 48.0f;    // metres: blocks this near get collision, nearest first
		float despawnRadius = 64.0f;  // farther props are removed
		float forgetRadius = 160.0f;  // farther blocks are forgotten (asked for again on the way back)
		int   regionRadius = 3;       // chunks around the player asked for with BLOCK_REGION_REQUEST
		int   regionsPerTick = 2;
		int   spawnsPerTick = 20;
	};

	// [Minecraft]: how much of the game is Minecraft while the passthrough is on (the settings menu changes these).
	struct MinecraftConfig
	{
		bool   crosshair = true;      // Minecraft's crosshair, in third person too
		bool   hand = true;           // the first-person hand and held item
		int    outline = 1;           // block outline: 0 none, 1 placed blocks only (not the hidden ground), 2 everywhere
		int    frameRate = 90;        // Minecraft's frames per second while composited; 0 = unlimited
		bool   hud = true;            // Minecraft's hotbar, hearts and hunger
		bool   steveInCars = true;    // Steve sits in cars (the game's driver is hidden); off: the game's driver shows
		bool   minecraftJump = true;  // a straight-up Minecraft jump instead of the game's
		double jumpHeight = 1.3;      // metres (Minecraft's jump is 1.25 blocks)
		bool   gtaDamage = true;      // getting hurt in the game costs Minecraft hearts; dying in Minecraft is "wasted"
		bool   explosions = true;     // Minecraft TNT explodes in the game too
		bool   arrowsHurt = true;     // Minecraft arrows hurt the game's people
		bool   torchLight = true;     // Minecraft torches light up the game
		int    inventoryKey = 0x45;   // a Windows virtual key: E (0x45), Tab (0x09) or I (0x49)
		bool   fallDamage = true;     // falls cost hearts the way Minecraft counts them (past 3 blocks)
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
		// [Blocks]
		BlocksConfig blocks{};
		// [Minecraft]
		MinecraftConfig minecraft{};
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
