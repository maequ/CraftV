#include "materials.h"

#include "craftv/protocol.h"

#include <algorithm>
#include <array>

namespace craftv::host
{
	using namespace craftv::proto;

	namespace
	{
		struct Named
		{
			const char*  name;
			std::uint8_t material;
		};

		// GTA V materials.dat names, grouped by what a friend should stand on. ASSUMPTION: names as used by
		// the game's material table; anything missing falls back to UNKNOWN (stone) and is logged.
		constexpr Named kNames[] = {
			{ "TARMAC", kMatRoad }, { "TARMAC_PAINTED", kMatRoad }, { "TARMAC_POTHOLE", kMatRoad }, { "RUMBLE_STRIPS", kMatRoad },
			{ "ICE_TARMAC", kMatRoad }, { "SNOW_TARMAC", kMatRoad },

			{ "CONCRETE", kMatPavement }, { "CONCRETE_POTHOLE", kMatPavement }, { "CONCRETE_DUSTY", kMatPavement }, { "PAVING_SLAB", kMatPavement },
			{ "BREEZE_BLOCK", kMatPavement }, { "COBBLESTONE", kMatPavement }, { "BRICK", kMatPavement }, { "MARBLE", kMatPavement },
			{ "CERAMIC", kMatPavement }, { "LINOLEUM", kMatPavement }, { "LAMINATE", kMatPavement },

			{ "ROCK", kMatRock }, { "ROCK_MOSSY", kMatRock }, { "STONE", kMatRock }, { "SANDSTONE_SOLID", kMatRock }, { "SANDSTONE_BRITTLE", kMatRock },

			{ "SAND_LOOSE", kMatSand }, { "SAND_COMPACT", kMatSand }, { "SAND_WET", kMatSand }, { "SAND_TRACK", kMatSand },
			{ "SAND_UNDERWATER", kMatSand }, { "SAND_DRY_DEEP", kMatSand }, { "SAND_WET_DEEP", kMatSand },

			{ "ICE", kMatSnow }, { "SNOW_LOOSE", kMatSnow }, { "SNOW_COMPACT", kMatSnow }, { "SNOW_DEEP", kMatSnow },

			{ "GRAVEL_SMALL", kMatGravel }, { "GRAVEL_LARGE", kMatGravel }, { "GRAVEL_DEEP", kMatGravel }, { "GRAVEL_TRAIN_TRACK", kMatGravel },

			{ "DIRT_TRACK", kMatDirt }, { "SOIL", kMatDirt }, { "CLAY_HARD", kMatDirt }, { "CLAY_SOFT", kMatDirt },

			{ "MUD_HARD", kMatMud }, { "MUD_POTHOLE", kMatMud }, { "MUD_SOFT", kMatMud }, { "MUD_UNDERWATER", kMatMud }, { "MUD_DEEP", kMatMud },
			{ "MARSH", kMatMud }, { "MARSH_DEEP", kMatMud },

			{ "GRASS", kMatGrass }, { "GRASS_LONG", kMatGrass }, { "GRASS_SHORT", kMatGrass }, { "HAY", kMatGrass }, { "BUSHES", kMatGrass },
			{ "TWIGS", kMatGrass }, { "LEAVES", kMatGrass }, { "WOODCHIPS", kMatGrass },

			{ "WOOD_SOLID_SMALL", kMatWood }, { "WOOD_SOLID_MEDIUM", kMatWood }, { "WOOD_SOLID_LARGE", kMatWood }, { "WOOD_SOLID_POLISHED", kMatWood },
			{ "WOOD_FLOOR_DUSTY", kMatWood }, { "WOOD_HOLLOW_SMALL", kMatWood }, { "WOOD_HOLLOW_MEDIUM", kMatWood }, { "WOOD_HOLLOW_LARGE", kMatWood },
			{ "WOOD_CHIPBOARD", kMatWood }, { "WOOD_OLD_CREAKY", kMatWood }, { "WOOD_HIGH_DENSITY", kMatWood }, { "WOOD_LATTICE", kMatWood },
			{ "TREE_BARK", kMatWood },

			{ "METAL_SOLID_SMALL", kMatMetal }, { "METAL_SOLID_MEDIUM", kMatMetal }, { "METAL_SOLID_LARGE", kMatMetal }, { "METAL_HOLLOW_SMALL", kMatMetal },
			{ "METAL_HOLLOW_MEDIUM", kMatMetal }, { "METAL_HOLLOW_LARGE", kMatMetal }, { "METAL_CHAINLINK_SMALL", kMatMetal },
			{ "METAL_CHAINLINK_LARGE", kMatMetal }, { "METAL_CORRUGATED_IRON", kMatMetal }, { "METAL_GRILLE", kMatMetal }, { "METAL_RAILING", kMatMetal },
			{ "METAL_DUCT", kMatMetal }, { "METAL_GARAGE_DOOR", kMatMetal }, { "METAL_MANHOLE", kMatMetal },

			{ "ROOF_TILE", kMatBuilding }, { "ROOF_FELT", kMatBuilding }, { "PLASTER_SOLID", kMatBuilding }, { "PLASTER_BRITTLE", kMatBuilding },
			{ "FIBREGLASS", kMatBuilding }, { "TARPAULIN", kMatBuilding },
		};
		constexpr std::size_t kCount = sizeof(kNames) / sizeof(kNames[0]);

		struct Entry
		{
			std::uint32_t hash;
			std::uint16_t index;  // into kNames
		};

		constexpr std::array<Entry, kCount> MakeTable()
		{
			std::array<Entry, kCount> t{};
			for (std::size_t i = 0; i < kCount; ++i) {
				t[i] = { Joaat(kNames[i].name), static_cast<std::uint16_t>(i) };
			}
			std::sort(t.begin(), t.end(), [](const Entry& a, const Entry& b) { return a.hash < b.hash; });
			return t;
		}
		constexpr auto kTable = MakeTable();

		constexpr bool NoCollisions()
		{
			for (std::size_t i = 1; i < kCount; ++i) {
				if (kTable[i].hash == kTable[i - 1].hash) {
					return false;
				}
			}
			return true;
		}
		static_assert(NoCollisions(), "two material names hash alike");

		const Named* Find(std::uint32_t a_hash)
		{
			auto it = std::lower_bound(kTable.begin(), kTable.end(), a_hash, [](const Entry& e, std::uint32_t h) { return e.hash < h; });
			return it != kTable.end() && it->hash == a_hash ? &kNames[it->index] : nullptr;
		}
	}

	std::uint8_t TerrainMaterialFor(std::uint32_t a_gameMaterialHash)
	{
		const Named* n = a_gameMaterialHash ? Find(a_gameMaterialHash) : nullptr;
		return n ? n->material : static_cast<std::uint8_t>(kMatUnknown);
	}

	const char* MaterialName(std::uint32_t a_gameMaterialHash)
	{
		const Named* n = Find(a_gameMaterialHash);
		return n ? n->name : nullptr;
	}
}
