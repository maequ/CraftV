// CraftV mock host: acts like the GTA V plugin so Phase 1 needs no game (brief §6.3).
//
// - creates/opens the mapping, sends HELLO and heartbeats (craftv::Endpoint, role host)
// - walks a simulated player in a circle and sends PLAYER_STATE at ~60 Hz while connected
// - serves TERRAIN_REQUEST from a procedural fake Los Santos (fake_terrain.h); its player walks on it
// - prints every message Minecraft sends; each friend's position at most once a second
// - CLI on stdin: help, status, friends, ground, terrain on|off, setblock, break, place, center, radius, speed,
//   walk, stop, kill-link, resume-link, restart, quit
// - --stress N: sends N TEST_PATTERN records and checks N coming back (cross-process stress test)
//
// Lines starting with '@' are machine-readable (the chaos tests parse them):
//   @EVENT <NAME> ...   @STATUS key=value ...   @STRESS ...   @RX <TYPE> ...   @FRIEND ...
#include "craftv/clock.h"
#include "craftv/codec.h"
#include "craftv/endpoint.h"
#include "fake_terrain.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <timeapi.h>

#include <share.h>

#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

using namespace craftv;
using namespace craftv::proto;

namespace
{
	constexpr const char*   kSoftware = "CraftV-MockHost 0.1.0";
	constexpr double        kPi = 3.14159265358979323846;
	constexpr double        kRadToDeg = 180.0 / kPi;
	constexpr std::uint64_t kDefaultStatusPeriodMs = 5000;
	constexpr std::uint64_t kRestartGapMs = 500;
	constexpr int           kDefaultHz = 60;
	constexpr int           kPatchesPerTick = 8;    // TERRAIN_PATCH answers per tick (1.3 KiB each)
	constexpr std::uint64_t kFriendPrintMs = 1000;  // an @FRIEND line per friend at most this often
	// --no-terrain keeps the old superflat behaviour: grass top at y = -61, feet at -60 (DECISIONS.md D-006).
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
			// Some writers (PowerShell's redirected stdin) start the stream with a UTF-8 BOM.
			if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF && static_cast<unsigned char>(s[1]) == 0xBB &&
				static_cast<unsigned char>(s[2]) == 0xBF) {
				s.erase(0, 3);
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
		bool          terrain = true;
	};

	void Usage()
	{
		std::printf(
			"mockhost [options]\n"
			"  --mapping NAME        shared-memory name (default Local\\CraftV_Shared_v1)\n"
			"  --mc-timeout-ms N     how long Minecraft may stay silent before STALE (default 3000)\n"
			"  --hz N                PLAYER_STATE rate (default 60)\n"
			"  --center X Y Z        circle centre (default 0.5 -60 0.5; Y only matters with --no-terrain)\n"
			"  --radius R            circle radius in blocks (default 6)\n"
			"  --speed S             walking speed, blocks/s (default 4.317)\n"
			"  --stress N            send N TEST_PATTERN records, check N back, print @STRESS_DONE\n"
			"  --log PATH            also write the log to PATH\n"
			"  --no-stdin            ignore stdin (scripts)\n"
			"  --exit-after-ms N     quit after N ms (tests)\n"
			"  --status-ms N         @STATUS line period (default 5000)\n"
			"  --quiet               don't print every received record\n"
			"  --no-terrain          ignore TERRAIN_REQUEST and walk at the centre's Y (old superflat mode)\n");
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
			} else if (a == L"--no-terrain") {
				o.terrain = false;
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
				ServeTerrain();
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
				Out("commands: status | friends | ground X Z | terrain on|off | setblock X Y Z ID | break X Y Z | place X Y Z FACE [ID] | center X Y Z |"
					" radius R | speed S | walk | stop | kill-link | resume-link | restart | quit");
			} else if (cmd == "status") {
				PrintStatus();
			} else if (cmd == "friends") {
				PrintFriends();
			} else if (cmd == "ground") {
				int x = 0, z = 0;
				if (!(in >> x >> z)) {
					Out("usage: ground X Z   (the fake terrain column there)");
					return;
				}
				static const char* materials[] = { "unknown", "grass", "dirt", "sand", "rock", "road", "pavement", "gravel", "snow", "wood", "metal",
					"building", "mud" };
				const auto c = mock::FakeColumn(x, z);
				Out("ground at (%d, %d): top block y %d, %s, water %s", x, z, c.ground, materials[c.material],
					c.water == kNoWater ? "none" : std::to_string(c.water).c_str());
			} else if (cmd == "terrain") {
				std::string arg;
				in >> arg;
				if (arg == "on" || arg == "off") {
					options_.terrain = arg == "on";
					if (!options_.terrain) {
						terrainQueue_.clear();
					}
				}
				Out("terrain %s (served %llu patches)", options_.terrain ? "on" : "off (requests are ignored; Minecraft asks again)",
					static_cast<unsigned long long>(terrainServed_));
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
				friends_.clear();      // it re-sends REMOTE_PLAYER_JOIN for everyone (PROTOCOL.md §7.10)
				terrainQueue_.clear();
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
			m.z = options_.cz + options_.radius * std::sin(angle_);
			// Feet on top of the fake ground's top block.
			m.y = options_.terrain ? mock::FakeColumn(static_cast<int>(std::floor(m.x)), static_cast<int>(std::floor(m.z))).ground + 1.0 : options_.cy;
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
			case kMsgRemotePlayerJoin:
				{
					RemotePlayerJoinMsg m;
					if (!codec::Decode(h, p, m)) break;
					Friend& f = friends_[m.playerId];
					f.name.assign(m.name, m.nameBytes);
					f.hasState = false;
					f.nextPrintMs = 0;
					Out("@RX REMOTE_PLAYER_JOIN #%u '%s' uuid %02x%02x%02x%02x-... (%zu friend%s online)", m.playerId, f.name.c_str(), m.uuid[0], m.uuid[1],
						m.uuid[2], m.uuid[3], friends_.size(), friends_.size() == 1 ? "" : "s");
					return;
				}
			case kMsgRemotePlayerState:
				{
					RemotePlayerStateMsg m;
					if (!codec::Decode(h, p, m)) break;
					auto it = friends_.find(m.playerId);
					if (it == friends_.end()) {
						return;  // a state before its JOIN: normal right after a restart (§7.10)
					}
					Friend& f = it->second;
					f.last = m;
					f.hasState = true;
					++friendStates_;
					const auto nowMs = clock::NowMs();
					if (nowMs >= f.nextPrintMs) {
						f.nextPrintMs = nowMs + kFriendPrintMs;
						PrintFriend(m.playerId, f);
					}
					return;
				}
			case kMsgRemotePlayerLeave:
				{
					RemotePlayerLeaveMsg m;
					if (!codec::Decode(h, p, m)) break;
					static const char* reasons[] = { "left", "went to another dimension", "world closing" };
					auto               it = friends_.find(m.playerId);
					Out("@RX REMOTE_PLAYER_LEAVE #%u '%s' %s", m.playerId, it != friends_.end() ? it->second.name.c_str() : "?", reasons[m.reason]);
					if (it != friends_.end()) {
						friends_.erase(it);
					}
					return;
				}
			case kMsgTerrainRequest:
				{
					TerrainRequestMsg m;
					if (!codec::Decode(h, p, m)) break;
					++terrainRequests_;
					if (options_.terrain) {
						terrainQueue_.push_back(m);
					}
					return;
				}
			case kMsgSessionInfo:
				{
					SessionInfoMsg m;
					if (!codec::Decode(h, p, m)) break;
					Out("@RX SESSION_INFO %s port=%u friends=%u/%u gameMode=%u auth=%d whitelist=%d address='%.*s'",
						(m.flags & kSessionOpen) ? "OPEN" : "CLOSED", m.port, m.friends, m.maxPlayers, m.gameMode, (m.flags & kSessionAuth) ? 1 : 0,
						(m.flags & kSessionWhitelist) ? 1 : 0, static_cast<int>(m.addressBytes), m.address);
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

		struct Friend
		{
			std::string          name;
			RemotePlayerStateMsg last{};
			bool                 hasState = false;
			std::uint64_t        nextPrintMs = 0;
		};

		void PrintFriend(std::uint32_t a_id, const Friend& a_friend)
		{
			if (!a_friend.hasState) {
				Out("@FRIEND #%u '%s' (no position yet)", a_id, a_friend.name.c_str());
				return;
			}
			const auto& s = a_friend.last;
			const int   ground = mock::FakeColumn(static_cast<int>(std::floor(s.x)), static_cast<int>(std::floor(s.z))).ground;
			Out("@FRIEND #%u '%s' at (%.1f, %.1f, %.1f) yaw %.0f speed %.1f%s%s%s%s%s mode %u health %u (fake ground top y %d)", a_id,
				a_friend.name.c_str(), s.x, s.y, s.z, s.yaw, std::sqrt(s.vx * s.vx + s.vz * s.vz), (s.flags & kRemoteOnGround) ? " on-ground" : "",
				(s.flags & kRemoteSprinting) ? " sprinting" : "", (s.flags & kRemoteCrouching) ? " crouching" : "",
				(s.flags & kRemoteSwimming) ? " swimming" : "", (s.flags & kRemoteFlying) ? " flying" : "", s.gameMode, s.health, ground);
		}

		void PrintFriends()
		{
			Out("%zu friend%s online", friends_.size(), friends_.size() == 1 ? "" : "s");
			for (const auto& [id, f] : friends_) {
				PrintFriend(id, f);
			}
		}

		void ServeTerrain()
		{
			for (int i = 0; i < kPatchesPerTick && !terrainQueue_.empty(); ++i) {
				const TerrainRequestMsg rq = terrainQueue_.front();
				TerrainPatchMsg         patch{};
				mock::FillPatch(rq.chunkX, rq.chunkZ, patch);
				patch.requestId = rq.requestId;
				if (!endpoint_->Send(patch)) {
					return;  // ring full or not connected: try again next tick
				}
				terrainQueue_.pop_front();
				++terrainServed_;
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
				"stale=%llu unknown=%llu malformed=%llu corrupt=%llu notConnected=%llu friends=%zu friendStates=%llu terrainRequests=%llu "
				"terrainServed=%llu terrainQueued=%zu%s",
				ToString(endpoint_->State()), endpoint_->Session(), peer.session, peer.pid,
				static_cast<unsigned long long>(peer.rttValid ? peer.rttUs : 0), static_cast<unsigned long long>(sentPlayerStates_),
				static_cast<unsigned long long>(tx.produced), static_cast<unsigned long long>(tx.droppedFull),
				static_cast<unsigned long long>(rx.consumed), static_cast<unsigned long long>(rx.stale), static_cast<unsigned long long>(rx.unknown),
				static_cast<unsigned long long>(rx.malformed), static_cast<unsigned long long>(rx.corrupt),
				static_cast<unsigned long long>(endpoint_->DroppedNotConnected()), friends_.size(), static_cast<unsigned long long>(friendStates_),
				static_cast<unsigned long long>(terrainRequests_), static_cast<unsigned long long>(terrainServed_), terrainQueue_.size(),
				killed_ ? " (link killed)" : "");
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
		std::map<std::uint32_t, Friend> friends_;
		std::deque<TerrainRequestMsg>   terrainQueue_;
		std::uint64_t                   friendStates_ = 0;
		std::uint64_t                   terrainRequests_ = 0;
		std::uint64_t                   terrainServed_ = 0;
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
		g_logFile = _wfsopen(options.logPath.c_str(), L"a", _SH_DENYNO);  // others may read it while we run
	}
	::SetConsoleCtrlHandler(&OnConsoleCtrl, TRUE);
	::timeBeginPeriod(1);  // 1 ms sleep resolution for a steady 60 Hz

	Out("CraftV mock host (protocol %u.%u), pid %lu. Type 'help' for commands.", kVersionMajor, kVersionMinor, ::GetCurrentProcessId());
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
