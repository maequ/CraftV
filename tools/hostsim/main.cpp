// CraftV host simulator: the real GTA V plugin core (host/core, HostPlugin) with a simulated game instead of
// Script Hook V natives. The player walks a circle on the mock host's fake Los Santos, ground probes read that
// same map (reported with GTA material names), and the overlay is printed to the console. Run it against the
// real Minecraft to test everything the ASI does except the natives (brief §0: test before asking Sary).
//
//   hostsim [--mapping NAME] [--seconds N] [--quiet]
#include "core/config.h"
#include "core/host_log.h"
#include "core/host_plugin.h"
#include "core/materials.h"

#include "../mockhost/fake_terrain.h"

#include "craftv/clock.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <timeapi.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace craftv;
using namespace craftv::host;
using namespace craftv::proto;

namespace
{
	constexpr double        kPi = 3.14159265358979323846;
	constexpr double        kRadius = 6.0, kSpeed = 4.3;  // metres, metres per second
	constexpr double        kFeetOffset = 1.0;            // the ped root is this far above the feet (CraftV.ini default)
	constexpr int           kHz = 60;
	constexpr std::uint64_t kPrintMs = 2000;

	// A GTA material name for each fake-map material, so the core's real hash table is exercised.
	std::uint32_t GameMaterialFor(std::uint8_t a_terrain)
	{
		switch (a_terrain) {
		case kMatGrass:
			return Joaat("GRASS");
		case kMatSand:
			return Joaat("SAND_LOOSE");
		case kMatRock:
			return Joaat("ROCK");
		case kMatRoad:
			return Joaat("TARMAC");
		case kMatPavement:
			return Joaat("CONCRETE");
		case kMatSnow:
			return Joaat("SNOW_LOOSE");
		case kMatBuilding:
			return Joaat("ROOF_TILE");
		case kMatMud:
			return Joaat("MUD_SOFT");
		default:
			return 0;
		}
	}

	class SimGame final : public IGame
	{
	public:
		double                   angle = 0.0;
		std::vector<std::string> lines;
		std::uint64_t            probes = 0, collisionRequests = 0;

		// Feet height of the fake ground at a game position (the top block's top face).
		static double FeetZ(double a_x, double a_y)
		{
			return mock::FakeColumn(static_cast<int>(std::floor(a_x)), static_cast<int>(std::floor(-a_y))).ground + 1.0;
		}

		void Advance(double a_dt) { angle = std::fmod(angle + kSpeed / kRadius * a_dt, 2.0 * kPi); }

		void Sample(GameSample& s) override
		{
			s = GameSample{};
			s.playerExists = true;
			s.x = static_cast<float>(0.5 + kRadius * std::cos(angle));
			s.y = static_cast<float>(-0.5 + kRadius * std::sin(angle));
			s.z = static_cast<float>(FeetZ(s.x, s.y) + kFeetOffset);
			const double vx = -kSpeed * std::sin(angle), vy = kSpeed * std::cos(angle);
			s.vx = static_cast<float>(vx);
			s.vy = static_cast<float>(vy);
			// RAGE heading: 0 = north (+Y), counter-clockwise; facing (-sin h, cos h).
			s.heading = static_cast<float>(std::fmod(std::atan2(-vx, vy) * 180.0 / kPi + 360.0, 360.0));
			s.heightAboveGround = static_cast<float>(kFeetOffset);
		}

		void DrawLabel(float, float, float, Rgba, const char* a_text) override { lines.emplace_back(a_text); }
		void DrawBox(float, float, float, float, Rgba) override {}

		bool ProbeGround(float a_x, float a_y, GroundProbe& a_out) override
		{
			++probes;
			const auto c = mock::FakeColumn(static_cast<int>(std::floor(a_x)), static_cast<int>(std::floor(-a_y)));
			a_out = GroundProbe{};
			a_out.hit = true;
			a_out.groundZ = static_cast<float>(c.ground + 1);
			a_out.materialHash = GameMaterialFor(c.material);
			if (c.water != kNoWater && c.water > c.ground) {
				a_out.water = true;
				a_out.waterZ = static_cast<float>(c.water + 1);
			}
			return true;
		}

		void RequestCollision(float, float, float) override { ++collisionRequests; }
	};
}

int wmain(int argc, wchar_t** argv)
{
	Config config;
	config.software = "CraftV-HostSim 0.1.0";
	double seconds = 0;
	bool   quiet = false;
	for (int i = 1; i < argc; ++i) {
		const std::wstring a = argv[i];
		if (a == L"--mapping" && i + 1 < argc) {
			config.mappingName = argv[++i];
		} else if (a == L"--seconds" && i + 1 < argc) {
			seconds = _wtof(argv[++i]);
		} else if (a == L"--quiet") {
			quiet = true;
		} else {
			std::printf("hostsim [--mapping NAME] [--seconds N] [--quiet]\n");
			return 2;
		}
	}
	HostLog::Open(L"logs\\hostsim.log");
	::timeBeginPeriod(1);
	SimGame    game;
	HostPlugin plugin(game, config);
	std::printf("CraftV host simulator: the GTA V plugin core with a simulated game. Ctrl+C to stop.\n");
	const std::uint64_t start = clock::NowMs();
	std::uint64_t       nextPrint = start, last = clock::NowUs();
	while (seconds <= 0 || clock::NowMs() - start < static_cast<std::uint64_t>(seconds * 1000)) {
		const std::uint64_t nowUs = clock::NowUs();
		game.Advance((nowUs - last) / 1e6);
		last = nowUs;
		game.lines.clear();
		plugin.Tick(clock::NowMs(), nowUs);
		if (clock::NowMs() >= nextPrint) {
			nextPrint += kPrintMs;
			if (!quiet) {
				for (const auto& l : game.lines) {
					std::printf("@OVERLAY %s\n", l.c_str());
				}
				std::printf("@SIM probes=%llu collisionRequests=%llu\n", static_cast<unsigned long long>(game.probes),
					static_cast<unsigned long long>(game.collisionRequests));
				std::fflush(stdout);
			}
		}
		::Sleep(1000 / kHz);
	}
	plugin.Shutdown();
	::timeEndPeriod(1);
	HostLog::Close();
	return 0;
}
