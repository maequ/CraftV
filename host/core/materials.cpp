#include "materials.h"

#include "craftv/protocol.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace craftv::host
{
	using namespace craftv::proto;

	namespace
	{
		struct Named
		{
			const char*   name;
			std::uint32_t hash;
			std::uint8_t  material;
		};

		// GTA V materials.dat names and the hash a shape test reports for each, grouped by what a friend
		// should stand on. Hashes from Script Hook V .NET's MaterialHash enum (scripting_v3/GTA/MaterialHash.cs,
		// zlib license); tarmac (0x10DD5498) was confirmed in game. Anything missing falls back to UNKNOWN (stone).
		constexpr Named kNames[] = {
			{ "TARMAC", 0x10DD5498, kMatRoad }, { "TARMAC_PAINTED", 0xB26EEFB0, kMatRoad }, { "TARMAC_POTHOLE", 0x70726A55, kMatRoad },
			{ "RUMBLE_STRIPS", 0xF116BC2D, kMatRoad }, { "ICE_TARMAC", 0x8CE6E7D9, kMatRoad }, { "SNOW_TARMAC", 0x5C67C62A, kMatRoad },
			{ "METAL_SOLID_ROAD_SURFACE", 0xD48AA0F2, kMatRoad }, { "PUDDLE", 0x3B982E13, kMatRoad },

			{ "CONCRETE", 0x46CA81E8, kMatPavement }, { "CONCRETE_POTHOLE", 0x1567BF52, kMatPavement }, { "CONCRETE_DUSTY", 0xBF59B491, kMatPavement },
			{ "CONCRETE_PAVEMENT", 0x78239B1A, kMatPavement }, { "PAVING_SLAB", 0x71AB3FEE, kMatPavement }, { "BRICK_PAVEMENT", 0xBB9CA6D8, kMatPavement },
			{ "BREEZE_BLOCK", 0xC72165D6, kMatPavement }, { "COBBLESTONE", 0x2257A573, kMatPavement }, { "BRICK", 0x61B1F936, kMatPavement },
			{ "MARBLE", 0x73EF7697, kMatPavement }, { "CERAMIC", 0xB94A2EB5, kMatPavement }, { "LINOLEUM", 0x11436942, kMatPavement },
			{ "LAMINATE", 0x6E02C9AA, kMatPavement },

			{ "ROCK", 0xCDEB5023, kMatRock }, { "ROCK_MOSSY", 0xF8902AC8, kMatRock }, { "ROCK_NOINST", 0x079E4953, kMatRock },
			{ "STONE", 0x2D9C1E0D, kMatRock }, { "SANDSTONE_SOLID", 0x23500534, kMatRock }, { "SANDSTONE_BRITTLE", 0x7209440E, kMatRock },

			{ "SAND_LOOSE", 0xA0EBF7E4, kMatSand }, { "SAND_COMPACT", 0x1E6D775E, kMatSand }, { "SAND_WET", 0x363CBCD5, kMatSand },
			{ "SAND_TRACK", 0x8E4D8AFF, kMatSand }, { "SAND_UNDERWATER", 0xBC4922A4, kMatSand }, { "SAND_DRY_DEEP", 0x1E5E7A48, kMatSand },
			{ "SAND_WET_DEEP", 0x4CCC2AFF, kMatSand },

			{ "ICE", 0xD125AA55, kMatSnow }, { "SNOW_LOOSE", 0x8C8308CA, kMatSnow }, { "SNOW_COMPACT", 0xCBA23987, kMatSnow },
			{ "SNOW_DEEP", 0x608ABC80, kMatSnow },

			{ "GRAVEL_SMALL", 0x38BBD00C, kMatGravel }, { "GRAVEL_LARGE", 0x7EDC5571, kMatGravel }, { "GRAVEL_DEEP", 0xEABD174E, kMatGravel },
			{ "GRAVEL_TRAIN_TRACK", 0x72C668B6, kMatGravel },

			{ "DIRT_TRACK", 0x8F9CD58F, kMatDirt }, { "SOIL", 0xD63CCDDB, kMatDirt }, { "CLAY_HARD", 0x4434DFE7, kMatDirt },
			{ "CLAY_SOFT", 0x216FF3F0, kMatDirt },

			{ "MUD_HARD", 0x8C31B7EA, kMatMud }, { "MUD_POTHOLE", 0x129ECA2A, kMatMud }, { "MUD_SOFT", 0x61826E7A, kMatMud },
			{ "MUD_UNDERWATER", 0xEFB2DF09, kMatMud }, { "MUD_DEEP", 0x42251DC0, kMatMud }, { "MARSH", 0x0D4C07E2, kMatMud },
			{ "MARSH_DEEP", 0x5E73A22E, kMatMud },

			{ "GRASS", 0x4F747B87, kMatGrass }, { "GRASS_LONG", 0xE47A3E41, kMatGrass }, { "GRASS_SHORT", 0xB34E900D, kMatGrass },
			{ "HAY", 0x92B69883, kMatGrass }, { "BUSHES", 0x22AD7B72, kMatGrass }, { "BUSHES_NOINST", 0x55E5AAEE, kMatGrass },
			{ "TWIGS", 0xC98F5B61, kMatGrass }, { "LEAVES", 0x8653C6CD, kMatGrass }, { "WOODCHIPS", 0xED932E53, kMatGrass },

			{ "WOOD_SOLID_SMALL", 0xE82A6F1C, kMatWood }, { "WOOD_SOLID_MEDIUM", 0x2114B37D, kMatWood }, { "WOOD_SOLID_LARGE", 0x309F8BB7, kMatWood },
			{ "WOOD_SOLID_POLISHED", 0x0789C7AB, kMatWood }, { "WOOD_FLOOR_DUSTY", 0xD35443DE, kMatWood }, { "WOOD_HOLLOW_SMALL", 0x76D9AC2F, kMatWood },
			{ "WOOD_HOLLOW_MEDIUM", 0xEA3746BD, kMatWood }, { "WOOD_HOLLOW_LARGE", 0xC8D738E7, kMatWood }, { "WOOD_CHIPBOARD", 0x461D0E9B, kMatWood },
			{ "WOOD_OLD_CREAKY", 0x2B13503D, kMatWood }, { "WOOD_HIGH_DENSITY", 0x981E5200, kMatWood }, { "WOOD_LATTICE", 0x77E08A22, kMatWood },
			{ "WOOD_HIGH_FRICTION", 0x8070DCF9, kMatWood }, { "TREE_BARK", 0x8DD4EBB9, kMatWood },

			{ "METAL_SOLID_SMALL", 0xA9BC4217, kMatMetal }, { "METAL_SOLID_MEDIUM", 0xEA34E8F8, kMatMetal }, { "METAL_SOLID_LARGE", 0x2CD49BD1, kMatMetal },
			{ "METAL_HOLLOW_SMALL", 0x00F3B93B, kMatMetal }, { "METAL_HOLLOW_MEDIUM", 0x6E3DBFB8, kMatMetal }, { "METAL_HOLLOW_LARGE", 0xDD3CDCF9, kMatMetal },
			{ "METAL_CHAINLINK_SMALL", 0x2D6E26CD, kMatMetal }, { "METAL_CHAINLINK_LARGE", 0x0781FA34, kMatMetal },
			{ "METAL_CORRUGATED_IRON", 0x31B80AD6, kMatMetal }, { "METAL_GRILLE", 0xE699F485, kMatMetal }, { "METAL_RAILING", 0x7D368D93, kMatMetal },
			{ "METAL_DUCT", 0x68FEB9FD, kMatMetal }, { "METAL_GARAGE_DOOR", 0xF2373DE9, kMatMetal }, { "METAL_MANHOLE", 0xD2FFA63D, kMatMetal },

			{ "PHYS_ELECTRIC_METAL", 0x87F87187, kMatMetal }, { "PHYS_ELECTRIC_FENCE", 0xBA428CAB, kMatMetal },
			{ "PHYS_BARBED_WIRE", 0xA402C0C0, kMatMetal }, { "VFX_METAL_ELECTRIFIED", 0xED92FC47, kMatMetal },
			{ "VFX_METAL_WATER_TOWER", 0x2473B1BF, kMatMetal }, { "VFX_METAL_STEAM", 0xD6CBF212, kMatMetal },
			{ "VFX_METAL_FLAME", 0x13D5CB0D, kMatMetal }, { "STUNT_RAMP_SURFACE", 0x8388FA6C, kMatMetal },

			// "DEFAULT" was the most common unknown on the second run, on roofs and walls around Forum Drive.
			{ "DEFAULT", 0x962C3F7B, kMatBuilding }, { "PLASTIC", 0x846BC4FF, kMatBuilding }, { "PLASTIC_HOLLOW", 0x25612338, kMatBuilding },
			{ "PLASTIC_HIGH_DENSITY", 0x9F154729, kMatBuilding }, { "FIBREGLASS_HOLLOW", 0xD256ED46, kMatBuilding },
			{ "RUBBER", 0xF7503F13, kMatBuilding }, { "CARPET_SOLID", 0x27E49616, kMatBuilding }, { "CARPET_SOLID_DUSTY", 0x0973AE44, kMatBuilding },
			{ "CARPET_FLOORBOARD", 0xACC354B1, kMatBuilding }, { "CLOTH", 0x07519E5D, kMatBuilding }, { "FEATHER_PILLOW", 0x4FFB413F, kMatBuilding },
			{ "CARDBOARD_SHEET", 0x0E18DFF5, kMatBuilding }, { "CARDBOARD_BOX", 0xAC038918, kMatBuilding }, { "POLYSTYRENE", 0x97476A9D, kMatBuilding },
			{ "GLASS_SHOOT_THROUGH", 0x37E12A0B, kMatBuilding }, { "GLASS_BULLETPROOF", 0x0E931A0E, kMatBuilding }, { "GLASS_OPAQUE", 0x596C55D1, kMatBuilding },
			{ "PERSPEX", 0x9F73E76C, kMatBuilding }, { "EMISSIVE_GLASS", 0x5978A2ED, kMatBuilding }, { "EMISSIVE_PLASTIC", 0x3F28ABAC, kMatBuilding },
			{ "ROOF_TILE", 0x689E0E75, kMatBuilding }, { "ROOF_FELT", 0xAB87C845, kMatBuilding }, { "PLASTER_SOLID", 0xDDC7963F, kMatBuilding },
			{ "PLASTER_BRITTLE", 0xF0FC7AFE, kMatBuilding }, { "FIBREGLASS", 0x50B728DB, kMatBuilding }, { "TARPAULIN", 0xD9B1CDE0, kMatBuilding },
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
				t[i] = { kNames[i].hash, static_cast<std::uint16_t>(i) };
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
		static_assert(NoCollisions(), "two materials share a hash");

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

	std::uint32_t MaterialHashOf(const char* a_name)
	{
		for (const Named& n : kNames) {
			if (std::strcmp(n.name, a_name) == 0) {
				return n.hash;
			}
		}
		return 0;
	}
}
