#include "config.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cwchar>
#include <string>

namespace craftv::host
{
	namespace
	{
		constexpr DWORD kValueChars = 256;

		std::wstring ReadString(const wchar_t* a_section, const wchar_t* a_key, const std::wstring& a_default, const std::wstring& a_path)
		{
			wchar_t buffer[kValueChars] = {};
			::GetPrivateProfileStringW(a_section, a_key, a_default.c_str(), buffer, kValueChars, a_path.c_str());
			return buffer;
		}

		double ReadDouble(const wchar_t* a_section, const wchar_t* a_key, double a_default, const std::wstring& a_path)
		{
			const std::wstring text = ReadString(a_section, a_key, L"", a_path);
			if (text.empty()) {
				return a_default;
			}
			wchar_t*     end = nullptr;
			const double value = std::wcstod(text.c_str(), &end);
			return end && *end == L'\0' ? value : a_default;
		}

		bool ReadBool(const wchar_t* a_section, const wchar_t* a_key, bool a_default, const std::wstring& a_path)
		{
			return ::GetPrivateProfileIntW(a_section, a_key, a_default ? 1 : 0, a_path.c_str()) != 0;
		}
	}

	bool Config::Load(const std::wstring& a_iniPath)
	{
		iniPath = a_iniPath;
		const bool exists = ::GetFileAttributesW(a_iniPath.c_str()) != INVALID_FILE_ATTRIBUTES;
		mappingName = ReadString(L"Link", L"MappingName", mappingName, a_iniPath);
		const double timeout = ReadDouble(L"Link", L"McTimeoutMs", static_cast<double>(mcTimeoutMs), a_iniPath);
		mcTimeoutMs = timeout >= 0 && timeout < 600000 ? static_cast<std::uint64_t>(timeout) : 0;

		const double scale = ReadDouble(L"World", L"BlocksPerMetre", world.blocksPerMetre, a_iniPath);
		world.blocksPerMetre = scale > 0.01 && scale < 100 ? scale : 1.0;
		world.feetOffset = ReadDouble(L"World", L"FeetOffset", world.feetOffset, a_iniPath);
		world.yOffset = ReadDouble(L"World", L"YOffset", world.yOffset, a_iniPath);

		const double probes = ReadDouble(L"Terrain", L"ProbesPerTick", terrain.probesPerTick, a_iniPath);
		terrain.probesPerTick = probes >= 1 && probes <= 1024 ? static_cast<int>(probes) : TerrainConfig{}.probesPerTick;
		const double wait = ReadDouble(L"Terrain", L"CollisionWaitTicks", terrain.collisionWaitTicks, a_iniPath);
		terrain.collisionWaitTicks = wait >= 0 && wait <= 600 ? static_cast<int>(wait) : TerrainConfig{}.collisionWaitTicks;
		const double nearM = ReadDouble(L"Terrain", L"NearDistance", terrain.nearDistance, a_iniPath);
		terrain.nearDistance = nearM >= 0 && nearM <= 5000 ? static_cast<float>(nearM) : TerrainConfig{}.nearDistance;
		const double budget = ReadDouble(L"Terrain", L"ProbeBudgetUs", terrain.probeBudgetUs, a_iniPath);
		terrain.probeBudgetUs = static_cast<int>(std::clamp(budget, 100.0, 16000.0));
		const double attempts = ReadDouble(L"Terrain", L"MaxAttempts", terrain.maxAttempts, a_iniPath);
		terrain.maxAttempts = attempts >= 1 && attempts <= 100 ? static_cast<int>(attempts) : TerrainConfig{}.maxAttempts;

		const std::wstring mode = ReadString(L"Passthrough", L"Mode", L"Auto", a_iniPath);
		passthrough.mode = _wcsicmp(mode.c_str(), L"Off") == 0 ? PassthroughMode::kOff : PassthroughMode::kAuto;
		const double pixels = ReadDouble(L"Passthrough", L"MaxPixels", static_cast<double>(passthrough.maxPixels), a_iniPath);
		passthrough.maxPixels = static_cast<std::uint64_t>(std::clamp(pixels, 320.0 * 180.0, static_cast<double>(proto::kViewMaxPixels)));
		passthrough.hideGtaHud = ReadBool(L"Passthrough", L"HideGtaHud", passthrough.hideGtaHud, a_iniPath);
		passthrough.meleeDamagePerHalfHeart = std::clamp(ReadDouble(L"Passthrough", L"MeleeDamagePerHalfHeart", passthrough.meleeDamagePerHalfHeart, a_iniPath), 0.0, 1000.0);
		blocks.maxProps = static_cast<int>(std::clamp(ReadDouble(L"Blocks", L"MaxProps", blocks.maxProps, a_iniPath), 0.0, 1000.0));
		blocks.spawnRadius = static_cast<float>(std::clamp(ReadDouble(L"Blocks", L"Radius", blocks.spawnRadius, a_iniPath), 8.0, 200.0));
		blocks.despawnRadius = blocks.spawnRadius + 16.0f;
		auto& mc = minecraft;
		mc.crosshair = ReadBool(L"Minecraft", L"Crosshair", mc.crosshair, a_iniPath);
		mc.hand = ReadBool(L"Minecraft", L"Hand", mc.hand, a_iniPath);
		mc.outline = static_cast<int>(std::clamp(ReadDouble(L"Minecraft", L"BlockOutline", mc.outline, a_iniPath), 0.0, 2.0));
		mc.frameRate = static_cast<int>(std::clamp(ReadDouble(L"Minecraft", L"FrameRate", mc.frameRate, a_iniPath), 0.0, 240.0));
		mc.hud = ReadBool(L"Minecraft", L"Hud", mc.hud, a_iniPath);
		mc.steveInCars = ReadBool(L"Minecraft", L"SteveInCars", mc.steveInCars, a_iniPath);
		mc.minecraftJump = ReadBool(L"Minecraft", L"MinecraftJump", mc.minecraftJump, a_iniPath);
		mc.jumpHeight = std::clamp(ReadDouble(L"Minecraft", L"JumpHeight", mc.jumpHeight, a_iniPath), 0.3, 5.0);
		mc.gtaDamage = ReadBool(L"Minecraft", L"GtaDamage", mc.gtaDamage, a_iniPath);
		mc.explosions = ReadBool(L"Minecraft", L"Explosions", mc.explosions, a_iniPath);
		mc.arrowsHurt = ReadBool(L"Minecraft", L"ArrowsHurt", mc.arrowsHurt, a_iniPath);
		mc.torchLight = ReadBool(L"Minecraft", L"TorchLight", mc.torchLight, a_iniPath);
		const std::wstring key = ReadString(L"Minecraft", L"InventoryKey", L"E", a_iniPath);
		mc.inventoryKey = _wcsicmp(key.c_str(), L"Tab") == 0 ? 0x09 : _wcsicmp(key.c_str(), L"I") == 0 ? 0x49 : 0x45;
		debugOverlay = ReadBool(L"Debug", L"Overlay", debugOverlay, a_iniPath);
		const std::wstring corner = ReadString(L"Debug", L"OverlayCorner", L"TopRight", a_iniPath);
		overlayCorner = _wcsicmp(corner.c_str(), L"TopLeft") == 0 ? OverlayCorner::kTopLeft : OverlayCorner::kTopRight;
		overlayDetails = ReadBool(L"Debug", L"OverlayDetails", overlayDetails, a_iniPath);
		logEveryTickCost = ReadBool(L"Debug", L"LogTickCost", logEveryTickCost, a_iniPath);
		return exists;
	}

	namespace
	{
		bool Write(const wchar_t* a_section, const wchar_t* a_key, const std::wstring& a_value, const std::wstring& a_path)
		{
			return ::WritePrivateProfileStringW(a_section, a_key, a_value.c_str(), a_path.c_str()) != FALSE;
		}
	}

	bool Config::Save() const
	{
		if (iniPath.empty()) {
			return false;
		}
		bool ok = Write(L"Passthrough", L"Mode", passthrough.mode == PassthroughMode::kOff ? L"Off" : L"Auto", iniPath);
		ok = Write(L"Passthrough", L"MaxPixels", std::to_wstring(passthrough.maxPixels), iniPath) && ok;
		ok = Write(L"Passthrough", L"HideGtaHud", passthrough.hideGtaHud ? L"1" : L"0", iniPath) && ok;
		ok = Write(L"Passthrough", L"MeleeDamagePerHalfHeart", std::to_wstring(static_cast<int>(passthrough.meleeDamagePerHalfHeart)), iniPath) && ok;
		ok = Write(L"Terrain", L"ProbesPerTick", std::to_wstring(terrain.probesPerTick), iniPath) && ok;
		ok = Write(L"Terrain", L"ProbeBudgetUs", std::to_wstring(terrain.probeBudgetUs), iniPath) && ok;
		ok = Write(L"Debug", L"Overlay", debugOverlay ? L"1" : L"0", iniPath) && ok;
		ok = Write(L"Debug", L"OverlayCorner", overlayCorner == OverlayCorner::kTopLeft ? L"TopLeft" : L"TopRight", iniPath) && ok;
		ok = Write(L"Debug", L"OverlayDetails", overlayDetails ? L"1" : L"0", iniPath) && ok;
		const auto& mc = minecraft;
		auto b = [](bool v) { return v ? L"1" : L"0"; };
		ok = Write(L"Minecraft", L"Crosshair", b(mc.crosshair), iniPath) && ok;
		ok = Write(L"Minecraft", L"Hand", b(mc.hand), iniPath) && ok;
		ok = Write(L"Minecraft", L"BlockOutline", std::to_wstring(mc.outline), iniPath) && ok;
		ok = Write(L"Minecraft", L"FrameRate", std::to_wstring(mc.frameRate), iniPath) && ok;
		ok = Write(L"Minecraft", L"Hud", b(mc.hud), iniPath) && ok;
		ok = Write(L"Minecraft", L"SteveInCars", b(mc.steveInCars), iniPath) && ok;
		ok = Write(L"Minecraft", L"MinecraftJump", b(mc.minecraftJump), iniPath) && ok;
		ok = Write(L"Minecraft", L"GtaDamage", b(mc.gtaDamage), iniPath) && ok;
		ok = Write(L"Minecraft", L"Explosions", b(mc.explosions), iniPath) && ok;
		ok = Write(L"Minecraft", L"ArrowsHurt", b(mc.arrowsHurt), iniPath) && ok;
		ok = Write(L"Minecraft", L"TorchLight", b(mc.torchLight), iniPath) && ok;
		ok = Write(L"Minecraft", L"InventoryKey", mc.inventoryKey == 0x09 ? L"Tab" : mc.inventoryKey == 0x49 ? L"I" : L"E", iniPath) && ok;
		return ok;
	}
}
