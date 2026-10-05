// Terrain scanner and material tests with a fake game (PROTOCOL.md §7.11-7.12): column -> game
// coordinates, heights -> block Y, water, materials, far chunks waiting for collision, misses retried
// then answered empty, the map edge, duplicates and the queue bound.
#include "test.h"

#include "core/materials.h"
#include "core/terrain_scanner.h"

#include "craftv/codec.h"

#include <cmath>
#include <string>
#include <vector>

using namespace craftv;
using namespace craftv::host;
using namespace craftv::proto;

namespace
{
	struct Probe
	{
		float x, y;
	};

	class GroundFake final : public IGame
	{
	public:
		bool               hitAnything = true;
		std::vector<Probe> probes;
		int                collisionRequests = 0;
		float              lastCollisionX = 0, lastCollisionY = 0;

		void Sample(GameSample&) override {}
		void DrawLabel(float, float, float, Rgba, const char*) override {}
		void DrawBox(float, float, float, float, Rgba) override {}
		bool ProbeGround(float a_x, float a_y, GroundProbe& a_out) override
		{
			probes.push_back({ a_x, a_y });
			a_out = GroundProbe{};
			if (!hitAnything) {
				return false;
			}
			a_out.hit = true;
			a_out.groundZ = 70.3f;
			a_out.materialHash = a_x < 40.0f ? Joaat("TARMAC") : Joaat("GRASS");
			if (a_x > 45.0f) {
				a_out.water = true;
				a_out.waterZ = 72.2f;
			}
			return true;
		}
		void RequestCollision(float a_x, float a_y, float) override
		{
			++collisionRequests;
			lastCollisionX = a_x;
			lastCollisionY = a_y;
		}
	};

	TerrainRequestMsg Request(std::int32_t a_cx, std::int32_t a_cz, std::uint32_t a_id)
	{
		TerrainRequestMsg r{};
		r.chunkX = a_cx;
		r.chunkZ = a_cz;
		r.requestId = a_id;
		return r;
	}

	// Ticks until a patch comes out (or a_max ticks pass); counts the ticks it took.
	const TerrainPatchMsg* Run(TerrainScanner& a_s, GroundFake& a_g, float a_px, float a_py, int a_max, int* a_ticks = nullptr)
	{
		for (int i = 1; i <= a_max; ++i) {
			if (const TerrainPatchMsg* p = a_s.Tick(a_g, WorldConfig{}, a_px, a_py, 50.0f)) {
				if (a_ticks) {
					*a_ticks = i;
				}
				return p;
			}
		}
		return nullptr;
	}

	bool Near(double a, double b, double eps = 1e-3) { return std::fabs(a - b) < eps; }
}

TEST_CASE("terrain: a near chunk is probed ProbesPerTick at a time into blocks, water and materials")
{
	GroundFake     game;
	TerrainScanner scanner;  // 64 probes a tick
	scanner.Enqueue(Request(2, -3, 5));
	int                    ticks = 0;
	const TerrainPatchMsg* p = Run(scanner, game, 40.0f, 40.0f, 10, &ticks);
	REQUIRE(p != nullptr);
	CHECK_EQ(ticks, 4);
	CHECK(p->chunkX == 2 && p->chunkZ == -3 && p->requestId == 5u);
	REQUIRE(game.probes.size() == 256u);
	// Column 0 is Minecraft (32, -48): game (32.5, 47.5). Column 255 is (47, -33): game (47.5, 32.5).
	CHECK(Near(game.probes[0].x, 32.5) && Near(game.probes[0].y, 47.5));
	CHECK(Near(game.probes[255].x, 47.5) && Near(game.probes[255].y, 32.5));
	CHECK_EQ(p->groundY[0], std::int16_t(69));
	CHECK_EQ(p->material[0], std::uint8_t(kMatRoad));    // x 32.5 < 40: tarmac
	CHECK_EQ(p->material[15], std::uint8_t(kMatGrass));  // x 47.5: grass
	CHECK_EQ(p->waterY[0], kNoWater);
	CHECK_EQ(p->waterY[15], std::int16_t(71));  // floor(72.2 + 0.5) - 1, above the ground at 69
	CHECK(codec::Valid(*p));
	CHECK_EQ(scanner.Stats().served, std::uint64_t(1));
	CHECK_EQ(game.collisionRequests, 0);
}

TEST_CASE("terrain: a far chunk asks the game for collision and waits before probing")
{
	GroundFake    game;
	TerrainConfig cfg;
	cfg.collisionWaitTicks = 5;
	TerrainScanner scanner(cfg);
	scanner.Enqueue(Request(50, 50, 1));  // centre (808.5, -808.5): far from a player at the origin
	for (int i = 0; i < 5; ++i) {  // the first tick starts the wait
		CHECK(scanner.Tick(game, WorldConfig{}, 0.0f, 0.0f, 50.0f) == nullptr);
	}
	CHECK_EQ(game.collisionRequests, 1);
	CHECK(Near(game.lastCollisionX, 808.5) && Near(game.lastCollisionY, -808.5));
	CHECK(game.probes.empty());
	const TerrainPatchMsg* p = Run(scanner, game, 0.0f, 0.0f, 10);
	REQUIRE(p != nullptr);
	CHECK_EQ(game.probes.size(), 256u);
}

TEST_CASE("terrain: a chunk where nothing is hit is left for Minecraft to ask again, then answered empty")
{
	GroundFake game;
	game.hitAnything = false;
	TerrainScanner scanner;  // MaxAttempts 3
	for (int attempt = 1; attempt <= 2; ++attempt) {
		scanner.Enqueue(Request(0, 0, attempt));
		CHECK(Run(scanner, game, 8.0f, -8.0f, 10) == nullptr);
		CHECK_EQ(scanner.Stats().deferred, std::uint64_t(attempt));
	}
	scanner.Enqueue(Request(0, 0, 3));
	const TerrainPatchMsg* p = Run(scanner, game, 8.0f, -8.0f, 10);
	REQUIRE(p != nullptr);
	CHECK_EQ(p->requestId, 3u);
	for (std::uint32_t i = 0; i < kChunkColumns; ++i) {
		CHECK(p->groundY[i] == kNoGround);
	}
	CHECK_EQ(scanner.Stats().empty, std::uint64_t(1));
}

TEST_CASE("terrain: outside the map a chunk is answered empty at once, without probing")
{
	GroundFake     game;
	TerrainScanner scanner;
	scanner.Enqueue(Request(1000, 0, 9));  // x 16008 m: far beyond GTA V's map
	const TerrainPatchMsg* p = scanner.Tick(game, WorldConfig{}, 0.0f, 0.0f, 50.0f);
	REQUIRE(p != nullptr);
	CHECK(p->groundY[0] == kNoGround && p->groundY[255] == kNoGround);
	CHECK(game.probes.empty());
	CHECK_EQ(scanner.Stats().empty, std::uint64_t(1));
}

TEST_CASE("terrain: duplicates are ignored, the queue is bounded, and reset clears it")
{
	TerrainScanner scanner;
	scanner.Enqueue(Request(1, 1, 1));
	scanner.Enqueue(Request(1, 1, 2));
	CHECK_EQ(scanner.Queued(), 1);
	for (int i = 0; i < TerrainScanner::kQueueCapacity + 10; ++i) {
		scanner.Enqueue(Request(100 + i, 0, static_cast<std::uint32_t>(i + 10)));
	}
	CHECK_EQ(scanner.Queued(), TerrainScanner::kQueueCapacity);
	CHECK_EQ(scanner.Stats().dropped, std::uint64_t(11));
	GroundFake game;
	scanner.Tick(game, WorldConfig{}, 24.0f, -24.0f, 50.0f);  // starts (1,1)
	scanner.Enqueue(Request(1, 1, 3));                        // in progress: ignored
	CHECK_EQ(scanner.Queued(), TerrainScanner::kQueueCapacity - 1);
	scanner.Reset();
	CHECK_EQ(scanner.Queued(), 0);
	CHECK(!scanner.Scanning());
}

TEST_CASE("terrain: heights round to the nearest block top; scale and offset apply; extremes clamp")
{
	WorldConfig w;
	CHECK_EQ(TerrainScanner::HeightToBlockY(70.3f, w), std::int16_t(69));
	CHECK_EQ(TerrainScanner::HeightToBlockY(70.6f, w), std::int16_t(70));
	CHECK_EQ(TerrainScanner::HeightToBlockY(-0.2f, w), std::int16_t(-1));
	w.yOffset = -64.0;
	CHECK_EQ(TerrainScanner::HeightToBlockY(70.3f, w), std::int16_t(5));
	w.yOffset = 0.0;
	w.blocksPerMetre = 2.0;
	CHECK_EQ(TerrainScanner::HeightToBlockY(70.3f, w), std::int16_t(140));
	CHECK_EQ(TerrainScanner::HeightToBlockY(90000.0f, w), std::int16_t(4096));
	CHECK_EQ(TerrainScanner::HeightToBlockY(-90000.0f, w), std::int16_t(-2048));
	float x = 0, y = 0;
	TerrainScanner::ColumnToGame(-1, -1, WorldConfig{}, x, y);
	CHECK(Near(x, -0.5) && Near(y, 0.5));
}

TEST_CASE("materials: joaat matches RAGE and materials.dat names map to terrain materials")
{
	CHECK_EQ(Joaat("adder"), 0xB779A091u);  // the well-known GTA V vehicle hash
	CHECK_EQ(Joaat("GRASS"), Joaat("grass"));
	CHECK_EQ(TerrainMaterialFor(Joaat("TARMAC")), std::uint8_t(kMatRoad));
	CHECK_EQ(TerrainMaterialFor(Joaat("SAND_LOOSE")), std::uint8_t(kMatSand));
	CHECK_EQ(TerrainMaterialFor(Joaat("GRASS_LONG")), std::uint8_t(kMatGrass));
	CHECK_EQ(TerrainMaterialFor(Joaat("CONCRETE")), std::uint8_t(kMatPavement));
	CHECK_EQ(TerrainMaterialFor(Joaat("ROOF_TILE")), std::uint8_t(kMatBuilding));
	CHECK_EQ(TerrainMaterialFor(0), std::uint8_t(kMatUnknown));
	CHECK_EQ(TerrainMaterialFor(0x12345678u), std::uint8_t(kMatUnknown));
	REQUIRE(MaterialName(Joaat("MUD_SOFT")) != nullptr);
	CHECK(std::string(MaterialName(Joaat("MUD_SOFT"))) == "MUD_SOFT");
	CHECK(MaterialName(0x12345678u) == nullptr);
}
