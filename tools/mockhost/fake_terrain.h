// A procedural stand-in for Los Santos, served to Minecraft as TERRAIN_PATCH (PROTOCOL.md §7.12) so
// co-op can be tested without GTA (brief §6.3): rolling hills, mountains to the north, a beach and sea
// to the south, a lake, a road grid with pavements, flat plazas and building roofs.
// Deterministic: the same (x, z) always gives the same column.
#pragma once

#include "craftv/protocol.h"

#include <cstdint>

namespace craftv::mock
{
	inline constexpr int kSeaLevel = 62;  // MC Y of the top water block in the sea and the lake

	struct Column
	{
		std::int16_t ground;    // MC Y of the top solid block
		std::int16_t water;     // MC Y of the top water block, or proto::kNoWater
		std::uint8_t material;  // proto::TerrainMaterial
	};

	Column FakeColumn(int a_x, int a_z);

	// Fills every column of one chunk (requestId and flags are left to the caller).
	void FillPatch(std::int32_t a_chunkX, std::int32_t a_chunkZ, proto::TerrainPatchMsg& a_out);
}
