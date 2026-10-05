// GTA V surface materials -> the protocol's terrain materials (PROTOCOL.md §7.12).
//
// ASSUMPTION: the material hash a shape test reports is Jenkins one-at-a-time ("joaat") of the material's
// name in materials.dat (lower-cased), like every other RAGE name hash. Verified in game in Phase 2: the
// overlay shows the last hash and how it was classified, and unknown hashes are logged.
#pragma once

#include <cstdint>

namespace craftv::host
{
	// RAGE's name hash (Jenkins one-at-a-time over the lower-cased bytes).
	constexpr std::uint32_t Joaat(const char* a_text)
	{
		std::uint32_t h = 0;
		for (; *a_text; ++a_text) {
			char c = *a_text;
			if (c >= 'A' && c <= 'Z') {
				c = static_cast<char>(c - 'A' + 'a');
			}
			h += static_cast<std::uint8_t>(c);
			h += h << 10;
			h ^= h >> 6;
		}
		h += h << 3;
		h ^= h >> 11;
		h += h << 15;
		return h;
	}

	// A proto::TerrainMaterial. Unknown hashes (and 0) give kMatUnknown.
	std::uint8_t TerrainMaterialFor(std::uint32_t a_gameMaterialHash);

	// The materials.dat name behind a hash, or nullptr if it isn't in the table (overlay/logs).
	const char* MaterialName(std::uint32_t a_gameMaterialHash);
}
