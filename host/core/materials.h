// GTA V surface materials -> the protocol's terrain materials (PROTOCOL.md §7.12).
//
// A shape test reports a surface as a material hash. These are NOT joaat of the materials.dat names (the
// first in-game run reported 0x10DD5498 for tarmac; joaat("tarmac") is 0x0DD8089A), so the table lists the
// hashes explicitly, taken from Script Hook V .NET's MaterialHash enum.
#pragma once

#include <cstdint>

namespace craftv::host
{
	// RAGE's name hash (Jenkins one-at-a-time over the lower-cased bytes), as used for model names.
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

	// The hash GTA reports for a materials.dat name ("TARMAC"), or 0 if it isn't in the table (tests, hostsim).
	std::uint32_t MaterialHashOf(const char* a_name);
}
