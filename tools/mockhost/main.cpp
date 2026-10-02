// RedCraft mock host: acts like the RDR2 plugin so Phase 1 needs no game (brief §5.3).
//
// - creates/opens the mapping, sends HELLO and heartbeats (redcraft::Endpoint, role host)
// - walks a simulated player in a circle and sends PLAYER_STATE at ~60 Hz while connected
// - prints every message Minecraft sends
// - CLI on stdin: help, status, setblock, break, place, center, radius, speed, walk, stop,
//   kill-link, resume-link, restart, quit
// - --stress N: sends N TEST_PATTERN records and checks N coming back (cross-process stress test)
//
// Lines starting with '@' are machine-readable (the chaos tests parse them):
//   @EVENT <NAME> ...   @STATUS key=value ...   @STRESS ...   @RX <TYPE> ...
#include "redcraft/clock.h"
#include "redcraft/codec.h"
#include "redcraft/endpoint.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <timeapi.h>

#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

using namespace redcraft;
using namespace redcraft::proto;

namespace
{
	constexpr const char*   kSoftware = "RedCraft-MockHost 0.1.0";
	constexpr double        kPi = 3.14159265358979323846;
	constexpr double        kRadToDeg = 180.0 / kPi;
	constexpr std::uint64_t kDefaultStatusPeriodMs = 5000;
	constexpr std::uint64_t kRestartGapMs = 500;
	constexpr int           kDefaultHz = 60;
	// Superflat dev world: grass top at y = -61, so feet stand at -60 (DECISIONS.md D-006).
	constexpr double kDefaultCenterX = 0.5, kDefaultCenterY = -60.0, kDefaultCenterZ = 0.5;
	constexpr double kDefaultRadius = 6.0;
	constexpr double kDefaultSpeed = 4.317;  // Minecraft walking speed, blocks per second

	// ---- logging: console + optional file ---------------------------------------------------
	FILE*      g_logFile = nullptr;
	std::mutex g_logMutex;
	bool       g_quiet = false;

	void Out(const char* a_fmt, ...)
	{
		char    line[1024];
		va_list args;
		va_start(args, a_fmt);
		std::vsnprintf(line, sizeof(line), a_fmt, args);
		va_end(args);
		SYSTEMTIME t;
		::GetLocalTime(&t);
		std::lock_guard lock(g_logMutex);
		std::printf("%s\n", line);
		std::fflush(stdout);
		if (g_logFile) {
			std::fprintf(g_logFile, "%04d-%02d-%02d %02d:%02d:%02d.%03d %s\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond,
				t.wMilliseconds, line);
			std::fflush(g_logFile);
		}
	}

	void EndpointLog(void*, LogLevel a_level, const char* a_text)
	{
		static const char* names[] = { "TRACE", "DEBUG", "INFO", "WARN", "ERROR" };
		Out("[link %s] %s", names[a_level <= kLogError ? a_level : kLogError], a_text);
	}

	// ---- stdin commands (reader thread -> main loop) ------------------------------------------
	std::mutex              g_cmdMutex;
	std::deque<std::string> g_commands;
	std::atomic<bool>       g_stdinClosed{ false };

	void StdinReader()
	{
		char line[512];
		while (std::fgets(line, sizeof(line), stdin)) {
			std::string s(line);
			while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
				s.pop_back();
			}
			std::lock_guard lock(g_cmdMutex);
			g_commands.push_back(s);
		}
		g_stdinClosed = true;
	}

	bool PopCommand(std::string& a_out)
	{
		std::lock_guard lock(g_cmdMutex);
		if (g_commands.empty()) {
			return false;
		}
		a_out = g_commands.front();
		g_commands.pop_front();
		return true;
	}

	// ---- options ------------------------------------------------------------------------------
	struct Options
	{
		std::wstring  mapping = kDefaultMappingName;
		std::uint64_t mcTimeoutMs = 0;
		std::uint64_t stress = 0;
		int           hz = kDefaultHz;
		double        cx = kDefaultCenterX, cy = kDefaultCenterY, cz = kDefaultCenterZ;
		double        radius = kDefaultRadius;
		double        speed = kDefaultSpeed;
		std::wstring  logPath;
		bool          noStdin = false;
		std::uint64_t exitAfterMs = 0;
		std::uint64_t statusMs = kDefaultStatusPeriodMs;
	};

	void Usage()
	{
		std::printf(
			"mockhost [options]\n"
			"  --mapping NAME        shared-memory name (default Local\\RedCraft_Shared_v1)\n"
			"  --mc-timeout-ms N     how long Minecraft may stay silent before STALE (default 3000)\n"
			"  --hz N                PLAYER_STATE rate (default 60)\n"
			"  --center X Y Z        circle centre (default 0.5 -60 0.5)\n"
			"  --radius R            circle radius in blocks (default 6)\n"
			"  --speed S             walking speed, blocks/s (default 4.317)\n"
			"  --stress N            send N TEST_PATTERN records, check N back, print @STRESS_DONE\n"
			"  --log PATH            also write the log to PATH\n"
			"  --no-stdin            ignore stdin (scripts)\n"
			"  --exit-after-ms N     quit after N ms (tests)\n"
			"  --status-ms N         @STATUS line period (default 5000)\n"
			"  --quiet               don't print every received record\n");
	}

	bool ParseArgs(int argc, wchar_t** argv, Options& o)
	{
		for (int i = 1; i < argc; ++i) {
			std::wstring a = argv[i];
			auto         next = [&](double& v) {
                if (i + 1 >= argc) return false;
                v = _wtof(argv[++i]);
                return true;
			};
			auto nextU = [&](std::uint64_t& v) {
				if (i + 1 >= argc) return false;
				v = _wcstoui64(argv[++i], nullptr, 10);
				return true;
			};
			double d = 0;
			if (a == L"--mapping" && i + 1 < argc) {
				o.mapping = argv[++i];
			} else if (a == L"--mc-timeout-ms") {
				if (!nextU(o.mcTimeoutMs)) return false;
			} else if (a == L"--stress") {
				if (!nextU(o.stress)) return false;
			} else if (a == L"--exit-after-ms") {
				if (!nextU(o.exitAfterMs)) return false;
			} else if (a == L"--status-ms") {
				if (!nextU(o.statusMs) || o.statusMs == 0) return false;
			} else if (a == L"--hz") {
				if (!next(d)) return false;
				o.hz = static_cast<int>(d);
			} else if (a == L"--center") {
				if (!next(o.cx) || !next(o.cy) || !next(o.cz)) return false;
			} else if (a == L"--radius") {
				if (!next(o.radius)) return false;
			} else if (a == L"--speed") {
				if (!next(o.speed)) return false;
			} else if (a == L"--log" && i + 1 < argc) {
				o.logPath = argv[++i];
			} else if (a == L"--no-stdin") {
				o.noStdin = true;
			} else if (a == L"--quiet") {
				g_quiet = true;
			} else {
				return false;
			}
		}
		if (o.hz < 1 || o.hz > 1000 || o.radius <= 0 || o.speed < 0) {
			return false;
		}
		return true;
	}

	const char* FaceName(std::uint8_t a_face)
	{
		static const char* names[] = { "down", "up", "north", "south", "west", "east" };
		return a_face <= kFaceMax ? names[a_face] : "?";
	}

	// ---- the host ---------------------------------------------------------------------------------
	class MockHost
	{
	public:
		explicit MockHost(const Options& a_options) : options_(a_options) { MakeEndpoint(); }

		void MakeEndpoint()
		{
			EndpointConfig c;
			c.role = Role::kHost;
			c.mappingName = options_.mapping.c_str();
			c.peerTimeoutMs = options_.mcTimeoutMs;
			c.software = kSoftware;
			c.log = &EndpointLog;
			endpoint_ = std::make_unique<Endpoint>(c);
		}

		bool Done() const { return quit_; }

		void Tick(std::uint64_t a_nowMs, std::uint64_t a_nowUs)
		{
			if (restartAtMs_ && a_nowMs >= restartAtMs_) {
				restartAtMs_ = 0;
				MakeEndpoint();
				Out("@EVENT RESTARTED");
			}
			if (!endpoint_) {
				return;
			}
			endpoint_->Tick(a_nowMs, a_nowUs);
			ReportEvents();
			endpoint_->Drain(a_nowUs, kMaxDrainBytesPerTick, [&](const RecordHeader& h, const std::uint8_t* p) { OnMessage(h, p); });
			if (endpoint_->Connected() && !killed_) {
				if (options_.stress) {
					PumpStress();
				} else if (walking_) {
					SendPlayerState(a_nowUs);
				}
			}
			if (a_nowMs >= nextStatusMs_) {
				nextStatusMs_ = a_nowMs + options_.statusMs;
				PrintStatus();
			}
		}

		void Command(const std::string& a_line)
		{
			std::istringstream in(a_line);
			std::string        cmd;
			in >> cmd;
			if (cmd.empty()) {
				return;
			}
			if (cmd == "help") {
				Out("commands: status | setblock X Y Z ID | break X Y Z | place X Y Z FACE [ID] | center X Y Z | radius R | speed S |"
					" walk | stop | kill-link | resume-link | restart | quit");
			} else if (cmd == "status") {
				PrintStatus();
			} else if (cmd == "setblock") {
				BlockSetMsg m{};
				if (!(in >> m.x >> m.y >> m.z >> m.blockId)) {
					Out("usage: setblock X Y Z ID   (ID = Minecraft block-state id, 0 = air)");
					return;
				}
				Out("%s BLOCK_SET (%d, %d, %d) = %u", endpoint_->Send(m) ? "sent" : "NOT sent (not connected?)", m.x, m.y, m.z, m.blockId);
			} else if (cmd == "break") {
				BlockBreakRequestMsg m{};
				m.requestId = ++requestId_;
				m.face = kFaceUnknown;
				if (!(in >> m.x >> m.y >> m.z)) {
					Out("usage: break X Y Z");
					return;
				}
				Out("%s BLOCK_BREAK_REQUEST #%u (%d, %d, %d)", endpoint_->Send(m) ? "sent" : "NOT sent", m.requestId, m.x, m.y, m.z);
			} else if (cmd == "place") {
				BlockPlaceRequestMsg m{};
				m.requestId = ++requestId_;
				int face = 1;
				if (!(in >> m.x >> m.y >> m.z >> face) || face < 0 || face > kFaceMax) {
					Out("usage: place X Y Z FACE [ID]   (FACE 0 down 1 up 2 north 3 south 4 west 5 east)");
					return;
				}
				m.face = static_cast<std::uint8_t>(face);
				in >> m.blockId;
				Out("%s BLOCK_PLACE_REQUEST #%u (%d, %d, %d) face %s id %u", endpoint_->Send(m) ? "sent" : "NOT sent", m.requestId, m.x, m.y, m.z,
					FaceName(m.face), m.blockId);
			} else if (cmd == "center") {
				in >> options_.cx >> options_.cy >> options_.cz;
				teleportNext_ = true;
				Out("centre now (%.2f, %.2f, %.2f)", options_.cx, options_.cy, options_.cz);
			} else if (cmd == "radius") {
				double r = 0;
				if (in >> r && r > 0) options_.radius = r;
				Out("radius %.2f", options_.radius);
			} else if (cmd == "speed") {
				double s = -1;
				if (in >> s && s >= 0) options_.speed = s;
				Out("speed %.2f blocks/s", options_.speed);
			} else if (cmd == "walk") {
				walking_ = true;
				Out("walking");
			} else if (cmd == "stop") {
				walking_ = false;
				Out("stopped (no PLAYER_STATE until 'walk')");
			} else if (cmd == "kill-link") {
				killed_ = true;
				endpoint_->SetHeartbeatSuspended(true);
				Out("@EVENT LINK_KILLED (heartbeat frozen, nothing sent; Minecraft should go STALE)");
			} else if (cmd == "resume-link") {
				killed_ = false;
				endpoint_->SetHeartbeatSuspended(false);
				Out("@EVENT LINK_RESUMED");
			} else if (cmd == "restart") {
				Out("@EVENT RESTARTING (detach, wait %llu ms, attach with a new session)", static_cast<unsigned long long>(kRestartGapMs));
				endpoint_.reset();
				killed_ = false;
				restartAtMs_ = clock::NowMs() + kRestartGapMs;
			} else if (cmd == "quit" || cmd == "exit") {
				quit_ = true;
			} else {
				Out("unknown command '%s' (try help)", cmd.c_str());
			}
		}

		void Shutdown()
		{
			if (endpoint_) {
				PrintStatus();
				endpoint_.reset();
			}
		}

	private:
		void ReportEvents()
		{
			const auto ev = endpoint_->TakeEvents();
			struct
			{
				std::uint32_t bit;
				const char*   name;
			} names[] = { { kEvAttached, "ATTACHED" }, { kEvConnected, "CONNECTED" }, { kEvResumed, "RESUMED" }, { kEvStale, "STALE" },
				{ kEvPeerDetached, "PEER_DETACHED" }, { kEvPeerRestarted, "PEER_RESTARTED" }, { kEvPeerHello, "PEER_HELLO" },
				{ kEvVersionMismatch, "VERSION_MISMATCH" }, { kEvDetached, "DETACHED" }, { kEvCorrupt, "CORRUPT" } };
			for (const auto& n : names) {
				if (ev & n.bit) {
					Out("@EVENT %s session=%u peerSession=%u", n.name, endpoint_->Session(), endpoint_->Peer().session);
				}
			}
			if (ev & (kEvConnected | kEvPeerRestarted)) {
				teleportNext_ = true;  // a new Minecraft: snap it onto the circle
			}
			if (ev & kEvPeerRestarted) {
				stressReceived_ = 0;  // not on CONNECTED: records may arrive before our own CONNECTED event
			}
		}

		void SendPlayerState(std::uint64_t a_nowUs)
		{
			if (lastWalkUs_ == 0) {
				lastWalkUs_ = a_nowUs;
			}
			const double dt = static_cast<double>(a_nowUs - lastWalkUs_) / 1e6;
			lastWalkUs_ = a_nowUs;
			const double omega = options_.speed / options_.radius;  // rad/s
			angle_ = std::fmod(angle_ + omega * dt, 2.0 * kPi);
			PlayerStateMsg m{};
			m.x = options_.cx + options_.radius * std::cos(angle_);
			m.y = options_.cy;
			m.z = options_.cz + options_.radius * std::sin(angle_);
			m.vx = static_cast<float>(-options_.radius * omega * std::sin(angle_));
			m.vy = 0.0f;
			m.vz = static_cast<float>(options_.radius * omega * std::cos(angle_));
			// Minecraft looks along (-sin(yaw), 0, cos(yaw)): face the direction of travel.
			m.yaw = static_cast<float>(std::atan2(-m.vx, m.vz) * kRadToDeg);
			m.pitch = 0.0f;
			m.flags = kPlayerOnGround | (teleportNext_ ? kPlayerTeleport : 0u);
			m.timeUs = a_nowUs;
			m.frame = ++frame_;
			if (endpoint_->Send(m)) {
				++sentPlayerStates_;
				teleportNext_ = false;
			}
		}

		void PumpStress()
		{
			std::uint8_t buf[kTestPatternFixedBytes + kTestPatternMaxFill];
			// Keep the ring busy but return often enough to heartbeat and drain.
			for (int burst = 0; burst < 4096 && stressSent_ < options_.stress; ++burst) {
				const std::uint32_t n = codec::BuildTestPattern(stressSent_, buf);
				if (!endpoint_->Send(kMsgTestPattern, buf, n)) {
					break;
				}
				++stressSent_;
			}
			if (!stressDone_ && stressSent_ >= options_.stress && stressReceived_ >= options_.stress) {
				stressDone_ = true;
				Out("@STRESS_DONE sent=%llu received=%llu errors=%llu ok=%d", static_cast<unsigned long long>(stressSent_),
					static_cast<unsigned long long>(stressReceived_), static_cast<unsigned long long>(stressErrors_), stressErrors_ == 0 ? 1 : 0);
			}
		}

		void OnMessage(const RecordHeader& h, const std::uint8_t* p)
		{
			switch (h.type) {
			case kMsgBlockSet:
				{
					BlockSetMsg m;
					if (!codec::Decode(h, p, m)) break;
					Out("@RX BLOCK_SET (%d, %d, %d) = %u%s%s", m.x, m.y, m.z, m.blockId, m.blockId == kBlockAir ? " (air)" : "",
						(m.flags & kBlockSetEcho) ? " [echo of host edit]" : "");
					return;
				}
			case kMsgBlockBreakRequest:
				{
					BlockBreakRequestMsg m;
					if (!codec::Decode(h, p, m)) break;
					Out("@RX BLOCK_BREAK_REQUEST #%u (%d, %d, %d) face %s", m.requestId, m.x, m.y, m.z, FaceName(m.face));
					return;
				}
			case kMsgBlockPlaceRequest:
				{
					BlockPlaceRequestMsg m;
					if (!codec::Decode(h, p, m)) break;
					Out("@RX BLOCK_PLACE_REQUEST #%u against (%d, %d, %d) face %s block %u", m.requestId, m.x, m.y, m.z, FaceName(m.face), m.blockId);
					return;
				}
			case kMsgLog:
				{
					LogMsg m;
					if (!codec::Decode(h, p, m)) break;
					Out("@RX LOG [minecraft] %.*s", static_cast<int>(m.textBytes), m.text);
					return;
				}
			case kMsgTestPattern:
				{
					std::uint64_t idx = 0;
					if (!codec::CheckTestPattern(p, h.payloadBytes, idx) || idx != stressReceived_) {
						if (++stressErrors_ <= 5) {
							Out("@STRESS error: got index %llu, expected %llu", static_cast<unsigned long long>(idx),
								static_cast<unsigned long long>(stressReceived_));
						}
					}
					++stressReceived_;
					return;
				}
			default:
				break;
			}
			endpoint_->CountMalformed();
			if (!g_quiet) {
				Out("@RX MALFORMED type=%u bytes=%u", h.type, h.payloadBytes);
			}
		}

		void PrintStatus()
		{
			if (!endpoint_) {
				Out("@STATUS state=RESTARTING");
				return;
			}
			const auto& tx = endpoint_->TxStats();
			const auto& rx = endpoint_->RxStats();
			const auto& peer = endpoint_->Peer();
			Out("@STATUS state=%s session=%u peerSession=%u peerPid=%u rttUs=%llu playerStates=%llu tx=%llu txDropped=%llu rx=%llu "
				"stale=%llu unknown=%llu malformed=%llu corrupt=%llu notConnected=%llu%s",
				ToString(endpoint_->State()), endpoint_->Session(), peer.session, peer.pid,
				static_cast<unsigned long long>(peer.rttValid ? peer.rttUs : 0), static_cast<unsigned long long>(sentPlayerStates_),
				static_cast<unsigned long long>(tx.produced), static_cast<unsigned long long>(tx.droppedFull),
				static_cast<unsigned long long>(rx.consumed), static_cast<unsigned long long>(rx.stale), static_cast<unsigned long long>(rx.unknown),
				static_cast<unsigned long long>(rx.malformed), static_cast<unsigned long long>(rx.corrupt),
				static_cast<unsigned long long>(endpoint_->DroppedNotConnected()), killed_ ? " (link killed)" : "");
		}

		Options                   options_;
		std::unique_ptr<Endpoint> endpoint_;
		bool                      quit_ = false;
		bool                      walking_ = true;
		bool                      killed_ = false;
		bool                      teleportNext_ = true;
		std::uint64_t             restartAtMs_ = 0;
		std::uint64_t             nextStatusMs_ = 0;
		std::uint64_t             lastWalkUs_ = 0;
		double                    angle_ = 0.0;
		std::uint32_t             frame_ = 0;
		std::uint32_t             requestId_ = 0;
		std::uint64_t             sentPlayerStates_ = 0;
		std::uint64_t             stressSent_ = 0;
		std::uint64_t             stressReceived_ = 0;
		std::uint64_t             stressErrors_ = 0;
		bool                      stressDone_ = false;
	};

	std::atomic<bool> g_ctrlC{ false };
	BOOL WINAPI       OnConsoleCtrl(DWORD)
	{
		g_ctrlC = true;
		return TRUE;
	}
}

int wmain(int argc, wchar_t** argv)
{
	Options options;
	if (!ParseArgs(argc, argv, options)) {
		Usage();
		return 2;
	}
	if (!options.logPath.empty()) {
		_wfopen_s(&g_logFile, options.logPath.c_str(), L"a");
	}
	::SetConsoleCtrlHandler(&OnConsoleCtrl, TRUE);
	::timeBeginPeriod(1);  // 1 ms sleep resolution for a steady 60 Hz

	Out("RedCraft mock host (protocol %u.%u), pid %lu. Type 'help' for commands.", kVersionMajor, kVersionMinor, ::GetCurrentProcessId());
	if (!options.noStdin) {
		std::thread(StdinReader).detach();
	}

	MockHost      host(options);
	const auto    startMs = clock::NowMs();
	const double  periodUs = 1e6 / options.hz;
	double        nextUs = static_cast<double>(clock::NowUs());
	std::string   cmd;
	while (!host.Done() && !g_ctrlC) {
		const auto nowMs = clock::NowMs();
		host.Tick(nowMs, clock::NowUs());
		while (PopCommand(cmd)) {
			host.Command(cmd);
		}
		if (options.exitAfterMs && nowMs - startMs >= options.exitAfterMs) {
			break;
		}
		if (options.stress) {
			std::this_thread::yield();  // run flat out; Tick heartbeats every loop
			continue;
		}
		nextUs += periodUs;
		const double waitUs = nextUs - static_cast<double>(clock::NowUs());
		if (waitUs > 1000.0) {
			::Sleep(static_cast<DWORD>(waitUs / 1000.0));
		} else if (waitUs < -periodUs * 10) {
			nextUs = static_cast<double>(clock::NowUs());  // fell far behind (debugger, sleep): resync
		}
	}
	host.Shutdown();
	Out("@EVENT EXIT");
	::timeEndPeriod(1);
	if (g_logFile) {
		std::fclose(g_logFile);
	}
	return 0;
}
