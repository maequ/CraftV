// RDR2 host plugin tests with a fake game (no RDR2 needed): coordinate conversion, the story-mode
// gate, PLAYER_STATE end to end into a real Minecraft-role endpoint, link loss, faults, the
// overlay, config, and "no heap allocation in a steady-state tick".
#include "test.h"

#include "core/config.h"
#include "core/coords.h"
#include "core/host_plugin.h"

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

		void Sample(GameSample& a_out) override
		{
			if (throwOnSample) {
				throw std::runtime_error("fake native failure");
			}
			a_out = sample;
		}
		void DrawLabel(float, float, float, Rgba, const char* a_text) override
		{
			++textCalls;
			if (recordDraws) {
				texts.emplace_back(a_text);
			}
		}
		void DrawBox(float, float, float, float, Rgba) override { ++rectCalls; }
	};

	std::wstring UniqueName(const wchar_t* a_tag)
	{
		static int n = 0;
		return std::wstring(L"Local\\CraftV_Test_rdr2_") + a_tag + L"_" + std::to_wstring(::GetCurrentProcessId()) + L"_" + std::to_wstring(++n) + L"_" +
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
						PlayerStateMsg m;
						if (h.type == kMsgPlayerState && codec::Decode(h, p, m)) {
							received.push_back(m);
						}
					});
				}
			}
		}
	};

	bool Near(double a, double b, double eps = 1e-4) { return std::fabs(a - b) < eps; }
}

// ---- coordinates --------------------------------------------------------------------------------

TEST_CASE("rdr2 coords: axes, feet offset and round trip")
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

TEST_CASE("rdr2 coords: heading -> yaw keeps the facing direction")
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

TEST_CASE("rdr2 plugin: PLAYER_STATE built from a sample is valid and flagged correctly")
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

TEST_CASE("rdr2 plugin: any online signal switches it off for good, before touching the link")
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

TEST_CASE("rdr2 plugin: going online after playing story mode detaches cleanly")
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

TEST_CASE("rdr2 plugin: story mode streams PLAYER_STATE to Minecraft, teleport first")
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

TEST_CASE("rdr2 plugin: loading screens, a missing or dead player send no PLAYER_STATE but keep the link")
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

TEST_CASE("rdr2 plugin: without Minecraft it runs, waits, and draws the status")
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

TEST_CASE("rdr2 plugin: Minecraft dying mid-run goes STALE without disturbing the game")
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

TEST_CASE("rdr2 plugin: block messages from Minecraft are counted")
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

TEST_CASE("rdr2 plugin: an exception from the game switches it off and nothing escapes")
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

TEST_CASE("rdr2 plugin: overlay shows four lines and CONNECTED when linked; off when disabled")
{
	Rig rig(L"overlay");
	rig.game.sample = StoryPlayer(0, 0, 50, 0);
	rig.Tick(10);
	rig.game.texts.clear();
	rig.Tick(1);
	REQUIRE(rig.game.texts.size() == 4);
	CHECK(rig.game.texts[0].find("link CONNECTED") != std::string::npos);
	CHECK(rig.game.texts[2].find("-> ") != std::string::npos);
	CHECK(rig.game.texts[3].find("net game 0 session 0 in 0") != std::string::npos);

	Rig quiet(L"overlayoff");
	quiet.config.debugOverlay = false;
	quiet.plugin = std::make_unique<HostPlugin>(quiet.game, quiet.config);
	quiet.game.sample = StoryPlayer(0, 0, 50, 0);
	quiet.Tick(10);
	CHECK_EQ(quiet.game.textCalls + quiet.game.rectCalls, 0);
}

TEST_CASE("rdr2 plugin: a steady-state tick allocates nothing and is cheap")
{
	Rig rig(L"alloc");
	rig.game.recordDraws = false;
	rig.game.sample = StoryPlayer(0, 0, 50, 0);
	rig.Tick(20);  // warm up: endpoint created, connected
	REQUIRE(rig.mc->Connected());
	g_allocs = 0;
	g_countAllocs = true;
	for (int i = 0; i < 2000; ++i) {
		rig.ms += 16;
		rig.game.sample.x += 0.05f;
		rig.plugin->Tick(rig.ms, rig.ms * 1000);
	}
	g_countAllocs = false;
	CHECK_EQ(g_allocs.load(), std::uint64_t(0));
	const auto& cost = rig.plugin->Cost();
	std::printf("    tick cost: avg %.2f us, worst %.2f us over %llu ticks\n", cost.avgUs, cost.maxEverUs, static_cast<unsigned long long>(cost.ticks));
	CHECK(cost.avgUs < 200.0);  // generous: the real budget is a 16 ms frame
}

// ---- config ---------------------------------------------------------------------------------------

TEST_CASE("rdr2 config: reads CraftV_RDR2.ini, falls back on bad values and missing files")
{
	wchar_t tmp[MAX_PATH];
	::GetTempPathW(MAX_PATH, tmp);
	const std::wstring path = std::wstring(tmp) + L"craftv_test_" + std::to_wstring(::GetCurrentProcessId()) + L".ini";
	{
		std::ofstream f(path);
		f << "[Link]\nMappingName=Local\\Custom_Map\nMcTimeoutMs=2500\n[World]\nBlocksPerMetre=abc\nFeetOffset=0.95\nYOffset=-64\n[Debug]\nOverlay=0\n";
	}
	Config c;
	CHECK(c.Load(path));
	CHECK(c.mappingName == L"Local\\Custom_Map");
	CHECK_EQ(c.mcTimeoutMs, std::uint64_t(2500));
	CHECK(Near(c.world.blocksPerMetre, 1.0));  // "abc" -> default
	CHECK(Near(c.world.feetOffset, 0.95));
	CHECK(Near(c.world.yOffset, -64.0));
	CHECK(!c.debugOverlay);
	::DeleteFileW(path.c_str());
	Config d;
	CHECK(!d.Load(path + L".missing"));
	CHECK(d.mappingName == L"Local\\CraftV_Shared_v1" && d.debugOverlay);
}
