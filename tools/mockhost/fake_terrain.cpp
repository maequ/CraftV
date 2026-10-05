#include "fake_terrain.h"

#include "craftv/codec.h"

#include <algorithm>
#include <cmath>

namespace craftv::mock
{
	using namespace craftv::proto;

	namespace
	{
		constexpr int    kGrid = 64;           // road spacing, blocks
		constexpr int    kRoadHalfWidth = 2;   // roads are 5 wide
		constexpr int    kPavementWidth = 3;   // then 3 of pavement on each side
		constexpr int    kMountainStartZ = -100;
		constexpr double kMountainSlope = 0.3;
		constexpr int    kMountainMaxRise = 60;
		constexpr int    kBeachStartZ = 140;
		constexpr double kBeachSlope = 0.4;
		constexpr int    kLakeX = -70, kLakeZ = 30, kLakeRadius = 24, kLakeDepth = 6;
		constexpr int    kRoofRise = 8;        // building tops stand this far above their lot
		constexpr int    kRoofHalfSize = 10;   // 21 x 21 roofs
		constexpr int    kRockY = 100, kSnowY = 115;

		int FloorDiv(int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }
		int FloorMod(int a, int b) { return a - FloorDiv(a, b) * b; }

		// Smooth natural ground, before roads, lots and water.
		double Natural(double x, double z)
		{
			double h = 66.0 + 10.0 * std::sin(x / 53.0) * std::cos(z / 71.0) + 5.0 * std::sin((x + 2.0 * z) / 29.0) + 3.0 * std::cos((2.0 * x - z) / 17.0);
			if (z < kMountainStartZ) {
				h += std::min((kMountainStartZ - z) * kMountainSlope, static_cast<double>(kMountainMaxRise));
			}
			if (z > kBeachStartZ) {
				h -= (z - kBeachStartZ) * kBeachSlope;
			}
			return h;
		}

		int RoadCenter(int a_coord) { return FloorDiv(a_coord + kGrid / 2, kGrid) * kGrid; }

		// What kind of lot a grid cell is: 0 plain, 1 plaza, 2 building.
		int LotKind(int a_cellX, int a_cellZ)
		{
			const int hash = FloorMod(a_cellX * 7 + a_cellZ * 13, 11);
			return hash == 0 ? 1 : (hash == 5 ? 2 : 0);
		}
	}

	Column FakeColumn(int a_x, int a_z)
	{
		Column c{ 0, kNoWater, kMatGrass };

		const int  dx = a_x - RoadCenter(a_x), dz = a_z - RoadCenter(a_z);
		const int  ax = dx < 0 ? -dx : dx, az = dz < 0 ? -dz : dz;
		const bool roadX = ax <= kRoadHalfWidth, roadZ = az <= kRoadHalfWidth;
		const bool paveX = ax <= kRoadHalfWidth + kPavementWidth, paveZ = az <= kRoadHalfWidth + kPavementWidth;

		double h = Natural(a_x, a_z);
		const bool aboveSea = Natural(RoadCenter(a_x), RoadCenter(a_z)) > kSeaLevel + 1;
		if (aboveSea && (paveX || paveZ)) {
			// Roads are flat across and follow the land along their length.
			const double along = paveX && paveZ ? Natural(RoadCenter(a_x), RoadCenter(a_z)) : (paveX ? Natural(RoadCenter(a_x), a_z) : Natural(a_x, RoadCenter(a_z)));
			c.ground = static_cast<std::int16_t>(std::lround(along));
			c.material = (roadX || roadZ) ? kMatRoad : kMatPavement;
			return c;
		}

		const int cellX = FloorDiv(a_x, kGrid), cellZ = FloorDiv(a_z, kGrid);
		const int lot = aboveSea ? LotKind(cellX, cellZ) : 0;
		if (lot != 0) {
			const int centerX = cellX * kGrid + kGrid / 2, centerZ = cellZ * kGrid + kGrid / 2;
			const int base = static_cast<int>(std::lround(Natural(centerX, centerZ)));
			const bool onRoof = lot == 2 && std::abs(a_x - centerX) <= kRoofHalfSize && std::abs(a_z - centerZ) <= kRoofHalfSize;
			c.ground = static_cast<std::int16_t>(onRoof ? base + kRoofRise : base);
			c.material = onRoof ? kMatBuilding : kMatPavement;
			return c;
		}

		const double lakeD = std::hypot(a_x - kLakeX, a_z - kLakeZ);
		const bool   inLake = lakeD < kLakeRadius;
		if (inLake) {
			h = std::min(h, kSeaLevel - kLakeDepth * (1.0 - lakeD / kLakeRadius) - 1.0);
		}
		c.ground = static_cast<std::int16_t>(std::lround(h));
		if (c.ground < kSeaLevel) {
			c.water = kSeaLevel;
			c.material = inLake ? kMatMud : kMatSand;
		} else if (c.ground <= kSeaLevel + 2 && (inLake || lakeD < kLakeRadius + 3 || a_z > kBeachStartZ)) {
			c.material = kMatSand;
		} else if (c.ground >= kSnowY) {
			c.material = kMatSnow;
		} else if (c.ground >= kRockY) {
			c.material = kMatRock;
		}
		return c;
	}

	void FillPatch(std::int32_t a_chunkX, std::int32_t a_chunkZ, TerrainPatchMsg& a_out)
	{
		a_out.chunkX = a_chunkX;
		a_out.chunkZ = a_chunkZ;
		for (std::uint32_t lz = 0; lz < 16; ++lz) {
			for (std::uint32_t lx = 0; lx < 16; ++lx) {
				const Column        c = FakeColumn(a_chunkX * 16 + static_cast<int>(lx), a_chunkZ * 16 + static_cast<int>(lz));
				const std::uint32_t i = codec::TerrainColumn(lx, lz);
				a_out.groundY[i] = c.ground;
				a_out.waterY[i] = c.water;
				a_out.material[i] = c.material;
			}
		}
	}
}
