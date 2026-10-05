#include "config.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cwchar>

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
		const double attempts = ReadDouble(L"Terrain", L"MaxAttempts", terrain.maxAttempts, a_iniPath);
		terrain.maxAttempts = attempts >= 1 && attempts <= 100 ? static_cast<int>(attempts) : TerrainConfig{}.maxAttempts;

		debugOverlay = ReadBool(L"Debug", L"Overlay", debugOverlay, a_iniPath);
		const std::wstring corner = ReadString(L"Debug", L"OverlayCorner", L"TopRight", a_iniPath);
		overlayCorner = _wcsicmp(corner.c_str(), L"TopLeft") == 0 ? OverlayCorner::kTopLeft : OverlayCorner::kTopRight;
		overlayDetails = ReadBool(L"Debug", L"OverlayDetails", overlayDetails, a_iniPath);
		logEveryTickCost = ReadBool(L"Debug", L"LogTickCost", logEveryTickCost, a_iniPath);
		return exists;
	}
}
