// Host plugin tests with a fake game (no game needed): coordinate conversion, the story-mode gate,
// PLAYER_STATE end to end into a real Minecraft-role endpoint, terrain requests answered, friends and
// session info tracked, link loss, faults, the overlay, config, and "no heap allocation in a
// steady-state tick" (terrain scanning included).
#include "test.h"

#include "core/config.h"
#include "core/coords.h"
#include "core/host_plugin.h"
#include "core/materials.h"

#include "craftv/codec.h"
#include "craftv/endpoint.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

using namespace craftv;
using namespace craftv::host;
using namespace craftv::proto;

// ---- allocation counter (for the hot-path test) ------------------------------------------------
namespace
{
	std::atomic<bool>          g_countAllocs{ false };
	std::atomic<std::uint64_t> g_allocs{ 0 };
}

void* operator new(std::size_t a_size)
{
	if (g_countAllocs.load(std::memory_order_relaxed)) {
		g_allocs.fetch_add(1, std::memory_order_relaxed);
	}
	if (void* p = std::malloc(a_size ? a_size : 1)) {
		return p;
	}
	throw std::bad_alloc();
}

void operator delete(void* a_p) noexcept { std::free(a_p); }
void operator delete(void* a_p, std::size_t) noexcept { std::free(a_p); }

namespace
{
	class FakeGame final : public IGame
	{
	public:
		GameSample               sample;
		bool                     throwOnSample = false;
		bool                     recordDraws = true;
		int                      textCalls = 0, rectCalls = 0;
		std::vector<std::string> texts;
		float                    lastLabelX = 0;
		float                    groundZ = 49.6f;  // flat ground everywhere
		std::uint32_t            material = MaterialHashOf("GRASS");
		int                      probes = 0, collisionRequests = 0;

		void Sample(GameSample& a_out) override
		{
			if (throwOnSample) {
				throw std::runtime_error("fake native failure");
			}
			a_out = sample;
		}
		void DrawLabel(float a_x, float, float, Rgba, const char* a_text) override
		{
			++textCalls;
			lastLabelX = a_x;
			if (recordDraws) {
				texts.emplace_back(a_text);
			}
		}
		void DrawBox(float, float, float, float, Rgba) override { ++rectCalls; }
		bool ProbeGround(float, float, GroundProbe& a_out) override
		{
			++probes;
			a_out = GroundProbe{};
			a_out.hit = true;
			a_out.groundZ = groundZ;
			a_out.materialHash = material;
			return true;
		}
		void RequestCollision(float, float, float) override { ++collisionRequests; }
	};

	std::wstring UniqueName(const wchar_t* a_tag)
	{
		static int n = 0;
		return std::wstring(L"Local\\CraftV_Test_host_") + a_tag + L"_" + std::to_wstring(::GetCurrentProcessId()) + L"_" + std::to_wstring(++n) + L"_" +
		       std::to_wstring(::GetTickCount64());
	}

	GameSample StoryPlayer(float a_x, float a_y, float a_z, float a_heading)
	{
		GameSample s;
		s.playerExists = true;
		s.x = a_x;
		s.y = a_y;
		s.z = a_z;
		s.heading = a_heading;
		s.heightAboveGround = 1.0f;
		return s;
	}

	struct Rig
	{
		std::wstring           name;
		FakeGame               game;
		Config                 config;
		std::unique_ptr<HostPlugin> plugin;
		std::unique_ptr<Endpoint>   mc;
		std::uint64_t          ms = 500000;
		std::vector<PlayerStateMsg> received;
		std::vector<TerrainPatchMsg> patches;

		explicit Rig(const wchar_t* a_tag, bool a_withMc = true) : name(UniqueName(a_tag))
		{
			config.mappingName = name;
			config.mcTimeoutMs = 1000;
			plugin = std::make_unique<HostPlugin>(game, config);
			if (a_withMc) {
				EndpointConfig c;
				c.role = Role::kMc;
				c.mappingName = name.c_str();
				c.peerTimeoutMs = 1000;
				c.software = "test-mc";
				mc = std::make_unique<Endpoint>(c);
			}
		}

		void Tick(int a_n = 1)
		{
			for (int i = 0; i < a_n; ++i) {
				ms += 16;
				plugin->Tick(ms, ms * 1000);
				if (mc) {
					mc->Tick(ms, ms * 1000);
					mc->Drain(ms * 1000, kMaxDrainBytesPerTick, [&](const RecordHeader& h, const std::uint8_t* p) {
						PlayerStateMsg  m;
						TerrainPatchMsg patch;
						if (h.type == kMsgPlayerState && codec::Decode(h, p, m)) {
							received.push_back(m);
						} else if (h.type == kMsgTerrainPatch && codec::Decode(h, p, patch)) {
							patches.push_back(patch);
						}
					});
				}
			}
		}
	};

	bool Near(double a, double b, double eps = 1e-4) { return std::fabs(a - b) < eps; }
}

// ---- coordinates --------------------------------------------------------------------------------

TEST_CASE("host coords: axes, feet offset and round trip")
{
	WorldConfig w;
	w.feetOffset = 1.0;
	w.yOffset = 0.0;
	const McPosition mc = ToMinecraft(100.0, 200.0, 50.0, w);
	CHECK(Near(mc.x, 100.0) && Near(mc.y, 49.0) && Near(mc.z, -200.0));
	double x, y, z;
	FromMinecraft(mc, w, x, y, z);
	CHECK(Near(x, 100.0) && Near(y, 200.0) && Near(z, 50.0));
	w.blocksPerMetre = 2.0;
	w.yOffset = -64.0;
	const McPosition s = ToMinecraft(1.0, 2.0, 3.0, w);
	CHECK(Near(s.x, 2.0) && Near(s.y, (3.0 - 1.0) * 2.0 - 64.0) && Near(s.z, -4.0));
	const McPosition v = VelocityToMinecraft(1.0, 2.0, 3.0, WorldConfig{});
	CHECK(Near(v.x, 1.0) && Near(v.y, 3.0) && Near(v.z, -2.0));
}

TEST_CASE("host coords: heading -> yaw keeps the facing direction")
{
	constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
	for (float h = 0.0f; h < 360.0f; h += 15.0f) {
		// RAGE facing for heading h is (-sin h, cos h) in x/y; in Minecraft x/z that is (-sin h, -cos h).
		const double fx = -std::sin(h * kDegToRad), fz = -std::cos(h * kDegToRad);
		const float  yaw = HeadingToYaw(h);
		// Minecraft looks along (-sin yaw, cos yaw).
		CHECK(Near(-std::sin(yaw * kDegToRad), fx, 1e-3) && Near(std::cos(yaw * kDegToRad), fz, 1e-3));
		CHECK(yaw >= -180.0f && yaw < 180.0f);
		CHECK(Near(std::fmod(YawToHeading(yaw) + 360.0, 360.0), h, 1e-3));
	}
	CHECK(Near(HeadingToYaw(90.0f), 90.0f));    // west is west
	CHECK(Near(HeadingToYaw(270.0f), -90.0f));  // east is east
	CHECK(Near(CamPitchToMcPitch(30.0f), -30.0f));
	CHECK(Near(CamPitchToMcPitch(-120.0f), 90.0f));
}

TEST_CASE("host plugin: PLAYER_STATE built from a sample is valid and flagged correctly")
{
	GameSample s = StoryPlayer(10.0f, 20.0f, 31.0f, 90.0f);
	s.vx = 1.0f;
	s.vy = 2.0f;
	s.vz = 0.5f;
	s.camPitch = 10.0f;
	WorldConfig w;
	PlayerStateMsg m = HostPlugin::MakePlayerState(s, w, 777, 5, true);
	CHECK(codec::Valid(m));
	CHECK(Near(m.x, 10.0) && Near(m.y, 30.0) && Near(m.z, -20.0));
	CHECK(Near(m.vx, 1.0) && Near(m.vy, 0.5) && Near(m.vz, -2.0));
	CHECK(Near(m.yaw, 90.0f) && Near(m.pitch, -10.0f));
	CHECK_EQ(m.flags, std::uint32_t(kPlayerOnGround | kPlayerTeleport));
	CHECK_EQ(m.frame, std::uint32_t(5));
	s.inAir = true;
	CHECK_EQ(HostPlugin::MakePlayerState(s, w, 1, 1, false).flags, std::uint32_t(0));
	s.inAir = false;
	s.swimming = true;
	CHECK_EQ(HostPlugin::MakePlayerState(s, w, 1, 1, false).flags & kPlayerOnGround, std::uint32_t(0));
}

// ---- the story-mode gate --------------------------------------------------------------------------

TEST_CASE("host plugin: any online signal switches it off for good, before touching the link")
{
	for (int which = 0; which < 3; ++which) {
		Rig rig(L"online");
		rig.game.sample = StoryPlayer(0, 0, 50, 0);
		rig.game.sample.networkGameInProgress = which == 0;
		rig.game.sample.networkSessionStarted = which == 1;
		rig.game.sample.networkInSession = which == 2;
		rig.Tick(30);
		CHECK(rig.plugin->State() == PluginState::kOnlineBlocked);
		CHECK(rig.plugin->Link() == nullptr);
		CHECK_EQ(rig.game.textCalls + rig.game.rectCalls, 0);  // draws nothing online
		CHECK(rig.received.empty());
		CHECK(rig.mc->State() != LinkState::kConnected);  // the host never attached
		// Stays off even if the flags clear (only a game restart turns it back on).
		rig.game.sample = StoryPlayer(0, 0, 50, 0);
		rig.Tick(10);
		CHECK(rig.plugin->State() == PluginState::kOnlineBlocked);
	}
}

TEST_CASE("host plugin: going online after playing story mode detaches cleanly")
{
	Rig rig(L"goonline");
	rig.game.sample = StoryPlayer(0, 0, 50, 0);
	rig.Tick(10);
	REQUIRE(rig.mc->State() == LinkState::kConnected);
	rig.mc->TakeEvents();
	rig.game.sample.networkSessionStarted = true;
	rig.Tick(3);
	CHECK(rig.plugin->State() == PluginState::kOnlineBlocked);
	CHECK(rig.mc->State() == LinkState::kAttached);  // host detached at once, no timeout
	CHECK((rig.mc->TakeEvents() & kEvPeerDetached) != 0);
}

// ---- link behaviour -------------------------------------------------------------------------------

TEST_CASE("host plugin: story mode streams PLAYER_STATE to Minecraft, teleport first")
{
	Rig rig(L"stream");
	rig.game.sample = StoryPlayer(100.0f, 200.0f, 51.0f, 90.0f);
	rig.Tick(10);
	CHECK(rig.plugin->State() == PluginState::kActive);
	REQUIRE(rig.mc->State() == LinkState::kConnected);
	REQUIRE(!rig.received.empty());
	CHECK((rig.received.front().flags & kPlayerTeleport) != 0);
	CHECK((rig.received.back().flags & kPlayerTeleport) == 0);
	const PlayerStateMsg& last = rig.received.back();
	CHECK(Near(last.x, 100.0) && Near(last.y, 50.0) && Near(last.z, -200.0) && Near(last.yaw, 90.0f));
	CHECK(rig.mc->Peer().helloSeen);
	// A big jump (fast travel) is a teleport again.
	rig.received.clear();
	rig.game.sample.x += 500.0f;
	rig.Tick(2);
	REQUIRE(!rig.received.empty());
	CHECK((rig.received.front().flags & kPlayerTeleport) != 0);
}

TEST_CASE("host plugin: loading screens, a missing or dead player send no PLAYER_STATE but keep the link")
{
	Rig rig(L"unusable");
	rig.game.sample = StoryPlayer(0, 0, 50, 0);
	rig.game.sample.loadingScreen = true;
	rig.Tick(10);
	CHECK(rig.mc->State() == LinkState::kConnected);
	CHECK(rig.received.empty());
	rig.game.sample.loadingScreen = false;
	rig.game.sample.playerDead = true;
	rig.Tick(5);
	CHECK(rig.received.empty());
	rig.game.sample = GameSample{};  // no player ped
	rig.Tick(5);
	CHECK(rig.received.empty());
	CHECK(rig.mc->State() == LinkState::kConnected);
	rig.game.sample = StoryPlayer(0, 0, 50, 0);
	rig.Tick(2);
	REQUIRE(!rig.received.empty());
	CHECK((rig.received.front().flags & kPlayerTeleport) != 0);  // back from loading: snap
}

TEST_CASE("host plugin: without Minecraft it runs, waits, and draws the status")
{
	Rig rig(L"nomc", false);
	rig.game.sample = StoryPlayer(0, 0, 50, 0);
	rig.Tick(50);
	CHECK(rig.plugin->State() == PluginState::kActive);
	REQUIRE(rig.plugin->Link() != nullptr);
	CHECK(rig.plugin->Link()->State() == LinkState::kAttached);
	CHECK_EQ(rig.plugin->PlayerStatesSent(), std::uint64_t(0));
	CHECK(!rig.game.texts.empty());
	bool sawAttached = false;
	for (const auto& t : rig.game.texts) {
		sawAttached |= t.find("link ATTACHED") != std::string::npos;
	}
	CHECK(sawAttached);
}

TEST_CASE("host plugin: Minecraft dying mid-run goes STALE without disturbing the game")
{
	Rig rig(L"mcdies");
	rig.game.sample = StoryPlayer(0, 0, 50, 0);
	rig.Tick(10);
	REQUIRE(rig.plugin->Link()->State() == LinkState::kConnected);
	rig.mc->SetHeartbeatSuspended(true);  // Minecraft hangs / crashes without detaching
	rig.Tick(100);
	CHECK(rig.plugin->Link()->State() == LinkState::kStale);
	CHECK(rig.plugin->State() == PluginState::kActive);
	const auto sentBefore = rig.plugin->PlayerStatesSent();
	rig.Tick(10);
	CHECK_EQ(rig.plugin->PlayerStatesSent(), sentBefore);  // nothing sent into a dead link
	rig.mc->SetHeartbeatSuspended(false);
	rig.Tick(5);
	CHECK(rig.plugin->Link()->State() == LinkState::kConnected);
	CHECK(rig.plugin->PlayerStatesSent() > sentBefore);
}

TEST_CASE("host plugin: block messages from Minecraft are counted")
{
	Rig rig(L"blocks");
	rig.game.sample = StoryPlayer(0, 0, 50, 0);
	rig.Tick(10);
	REQUIRE(rig.mc->Connected());
	CHECK(rig.mc->Send(BlockSetMsg{ 1, 2, 3, 1, 0, 0 }));
	BlockBreakRequestMsg br{};
	br.requestId = 1;
	br.face = 1;
	CHECK(rig.mc->Send(br));
	rig.Tick(2);
	CHECK_EQ(rig.plugin->BlockMessagesReceived(), std::uint64_t(2));
}

TEST_CASE("host plugin: an exception from the game switches it off and nothing escapes")
{
	Rig rig(L"fault");
	rig.game.sample = StoryPlayer(0, 0, 50, 0);
	rig.Tick(10);
	REQUIRE(rig.mc->Connected());
	rig.game.throwOnSample = true;
	rig.Tick(3);  // must not throw
	CHECK(rig.plugin->State() == PluginState::kFaulted);
	CHECK(rig.plugin->Link() == nullptr);
	rig.game.throwOnSample = false;
	rig.Tick(3);
	CHECK(rig.plugin->State() == PluginState::kFaulted);
	CHECK(rig.mc->State() == LinkState::kAttached);  // clean detach
}

TEST_CASE("host plugin: overlay is three lines top right by default, five with details, top left on request; off when disabled")
{
	Rig rig(L"overlay");
	rig.game.sample = StoryPlayer(0, 0, 50, 0);
	rig.Tick(10);
	rig.game.texts.clear();
	rig.Tick(1);
	REQUIRE(rig.game.texts.size() == 3);
	CHECK(rig.game.texts[0].find("link CONNECTED") != std::string::npos);
	CHECK(rig.game.texts[1].find("friends 0") != std::string::npos);
	CHECK(rig.game.texts[2].find("ground sent 0") != std::string::npos);
	CHECK(rig.game.lastLabelX > 0.5f);  // right half of the screen

	Rig detailed(L"overlaydetails");
	detailed.config.overlayDetails = true;
	detailed.config.overlayCorner = OverlayCorner::kTopLeft;
	detailed.plugin = std::make_unique<HostPlugin>(detailed.game, detailed.config);
	detailed.game.sample = StoryPlayer(0, 0, 50, 0);
	detailed.Tick(10);
	detailed.game.texts.clear();
	detailed.Tick(1);
	REQUIRE(detailed.game.texts.size() == 5);
	CHECK(detailed.game.texts[2].find("terrain sent 0") != std::string::npos);
	CHECK(detailed.game.texts[3].find("-> ") != std::string::npos);
	CHECK(detailed.game.texts[4].find("net game 0 session 0 in 0") != std::string::npos);
	CHECK(detailed.game.lastLabelX < 0.1f);

	Rig quiet(L"overlayoff");
	quiet.config.debugOverlay = false;
	quiet.plugin = std::make_unique<HostPlugin>(quiet.game, quiet.config);
	quiet.game.sample = StoryPlayer(0, 0, 50, 0);
	quiet.Tick(10);
	CHECK_EQ(quiet.game.textCalls + quiet.game.rectCalls, 0);
}

TEST_CASE("host plugin: a steady-state tick allocates nothing and is cheap")
{
	Rig rig(L"alloc");
	rig.game.recordDraws = false;
	rig.game.sample = StoryPlayer(0, 0, 50, 0);
	rig.Tick(20);  // warm up: endpoint created, connected
	REQUIRE(rig.mc->Connected());
	for (int i = 0; i < 20; ++i) {  // terrain work for the counted ticks: 20 chunks to scan and send
		TerrainRequestMsg rq{};
		rq.chunkX = i;
		rq.chunkZ = -i;
		rq.requestId = static_cast<std::uint32_t>(i + 1);
		REQUIRE(rig.mc->Send(rq));
	}
	g_allocs = 0;
	g_countAllocs = true;
	for (int i = 0; i < 2000; ++i) {
		rig.ms += 16;
		rig.game.sample.x += 0.05f;
		rig.plugin->Tick(rig.ms, rig.ms * 1000);
		rig.mc->Tick(rig.ms, rig.ms * 1000);  // keep the link alive; the endpoint is allocation-free too
		rig.mc->Drain(rig.ms * 1000, kMaxDrainBytesPerTick, [](const RecordHeader&, const std::uint8_t*) {});
	}
	g_countAllocs = false;
	CHECK_EQ(g_allocs.load(), std::uint64_t(0));
	CHECK_EQ(rig.plugin->Terrain().Stats().served, std::uint64_t(20));  // the scanning above was really exercised
	const auto& cost = rig.plugin->Cost();
	std::printf("    tick cost: avg %.2f us, worst %.2f us over %llu ticks\n", cost.avgUs, cost.maxEverUs, static_cast<unsigned long long>(cost.ticks));
	CHECK(cost.avgUs < 200.0);  // generous: the real budget is a 16 ms frame
}

// ---- config ---------------------------------------------------------------------------------------

TEST_CASE("host config: reads the .ini, falls back on bad values and missing files")
{
	wchar_t tmp[MAX_PATH];
	::GetTempPathW(MAX_PATH, tmp);
	const std::wstring path = std::wstring(tmp) + L"craftv_test_" + std::to_wstring(::GetCurrentProcessId()) + L".ini";
	{
		std::ofstream f(path);
		f << "[Link]\nMappingName=Local\\Custom_Map\nMcTimeoutMs=2500\n[World]\nBlocksPerMetre=abc\nFeetOffset=0.95\nYOffset=-64\n[Debug]\nOverlay=0\nOverlayCorner=topleft\nOverlayDetails=1\n"
		  << "[Terrain]\nProbesPerTick=128\nCollisionWaitTicks=-5\nNearDistance=300\n";
	}
	Config c;
	CHECK(c.Load(path));
	CHECK(c.mappingName == L"Local\\Custom_Map");
	CHECK_EQ(c.mcTimeoutMs, std::uint64_t(2500));
	CHECK(Near(c.world.blocksPerMetre, 1.0));  // "abc" -> default
	CHECK(Near(c.world.feetOffset, 0.95));
	CHECK(Near(c.world.yOffset, -64.0));
	CHECK(!c.debugOverlay);
	CHECK_EQ(c.terrain.probesPerTick, 128);
	CHECK_EQ(c.terrain.collisionWaitTicks, TerrainConfig{}.collisionWaitTicks);  // -5 -> default
	CHECK(Near(c.terrain.nearDistance, 300.0));
	CHECK(c.overlayCorner == OverlayCorner::kTopLeft);
	CHECK(c.overlayDetails);
	::DeleteFileW(path.c_str());
	Config d;
	CHECK(!d.Load(path + L".missing"));
	CHECK(d.mappingName == L"Local\\CraftV_Shared_v1" && d.debugOverlay);
}

// ---- co-op (protocol v1.1) ---------------------------------------------------------------------------

TEST_CASE("host plugin: answers Minecraft's terrain requests with the game's ground")
{
	Rig rig(L"terrain");
	rig.game.sample = StoryPlayer(0, 0, 50, 0);
	rig.game.groundZ = 70.3f;
	rig.game.material = MaterialHashOf("TARMAC");
	rig.Tick(10);
	REQUIRE(rig.mc->Connected());
	TerrainRequestMsg rq{};
	rq.chunkX = 0;
	rq.chunkZ = -1;
	rq.requestId = 77;
	REQUIRE(rig.mc->Send(rq));
	rig.Tick(10);  // 256 columns at 64 probes a tick, plus delivery
	REQUIRE(rig.patches.size() == 1);
	const TerrainPatchMsg& p = rig.patches[0];
	CHECK(p.chunkX == 0 && p.chunkZ == -1 && p.requestId == 77u);
	CHECK_EQ(p.groundY[0], std::int16_t(69));  // floor(70.3 + 0.5) - 1
	CHECK_EQ(p.material[255], std::uint8_t(kMatRoad));
	CHECK_EQ(p.waterY[10], kNoWater);
	CHECK_EQ(rig.game.probes, 256);
	CHECK_EQ(rig.game.collisionRequests, 0);  // the chunk is next to the player
}

TEST_CASE("host plugin: friends and session info from Minecraft show on the overlay; a new Minecraft forgets them")
{
	Rig rig(L"friends");
	rig.game.sample = StoryPlayer(0, 0, 50, 0);
	rig.Tick(10);
	REQUIRE(rig.mc->Connected());
	RemotePlayerJoinMsg join{};
	join.playerId = 9;
	codec::SetPlayerName(join, "Alex");
	REQUIRE(rig.mc->Send(join));
	RemotePlayerStateMsg st{};
	st.playerId = 9;
	st.x = 30.0;  // 30 m east of the player (mc x = game x)
	st.y = 49.0;
	st.z = 0.0;
	REQUIRE(rig.mc->Send(st));
	SessionInfoMsg info{};
	info.flags = kSessionOpen | kSessionAuth;
	info.port = 25565;
	info.friends = 1;
	info.maxPlayers = 8;
	codec::SetAddress(info, "192.168.1.23:25565");
	REQUIRE(rig.mc->Send(info));
	rig.Tick(2);
	CHECK_EQ(rig.plugin->FriendsTable().Count(), 1);
	CHECK(rig.plugin->HasSessionInfo());
	rig.game.texts.clear();
	rig.Tick(1);
	REQUIRE(rig.game.texts.size() == 3);
	CHECK(rig.game.texts[1].find("friends 1/8") != std::string::npos);
	CHECK(rig.game.texts[1].find("join: 192.168.1.23:25565") != std::string::npos);
	CHECK(rig.game.texts[1].find("Alex 30m") != std::string::npos);

	RemotePlayerLeaveMsg leave{ 9, kLeaveLeft };
	REQUIRE(rig.mc->Send(leave));
	rig.Tick(2);
	CHECK_EQ(rig.plugin->FriendsTable().Count(), 0);

	REQUIRE(rig.mc->Send(join));
	rig.Tick(2);
	CHECK_EQ(rig.plugin->FriendsTable().Count(), 1);
	// Minecraft restarts: a new session, which re-announces its friends itself (§7.10).
	rig.mc.reset();
	EndpointConfig c;
	c.role = Role::kMc;
	c.mappingName = rig.name.c_str();
	c.peerTimeoutMs = 1000;
	c.software = "test-mc-2";
	rig.mc = std::make_unique<Endpoint>(c);
	rig.Tick(20);
	CHECK_EQ(rig.plugin->FriendsTable().Count(), 0);
	CHECK(!rig.plugin->HasSessionInfo());
}

TEST_CASE("host plugin: messages only Minecraft may send are refused from a host, and v1.1 garbage is malformed")
{
	Rig rig(L"v11bad");
	rig.game.sample = StoryPlayer(0, 0, 50, 0);
	rig.Tick(10);
	REQUIRE(rig.mc->Connected());
	RemotePlayerJoinMsg bad{};
	bad.playerId = 0;  // invalid
	bad.nameBytes = 4;
	REQUIRE(rig.mc->Send(bad));
	rig.Tick(2);
	CHECK_EQ(rig.plugin->FriendsTable().Count(), 0);
	CHECK(rig.plugin->Link()->RxStats().malformed >= 1);
}
