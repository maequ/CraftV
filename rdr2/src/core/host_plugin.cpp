#include "host_plugin.h"

#include "coords.h"
#include "host_log.h"

#include "craftv/clock.h"
#include "craftv/codec.h"

#include <cstdio>
#include <exception>

namespace craftv::host
{
	using namespace craftv::proto;

	namespace
	{
		// A jump bigger than this between ticks is a teleport (fast travel, respawn, cutscene).
		constexpr double        kTeleportDistance = 50.0;
		constexpr double        kCostEmaWeight = 0.05;
		constexpr std::uint64_t kCostReportPeriodMs = 30000;
		constexpr const char*   kSoftware = "CraftV-RDR2 0.1.0";

		// Overlay layout (screen fractions).
		constexpr float kOverlayX = 0.01f;
		constexpr float kOverlayY = 0.01f;
		constexpr float kLineHeight = 0.022f;
		constexpr float kTextScale = 0.28f;
		constexpr float kPanelWidth = 0.42f;
		constexpr int   kOverlayLines = 4;
		constexpr Rgba  kPanel{ 0, 0, 0, 140 };
		constexpr Rgba  kGreen{ 90, 255, 90, 255 };
		constexpr Rgba  kYellow{ 255, 230, 90, 255 };
		constexpr Rgba  kRed{ 255, 90, 90, 255 };
		constexpr Rgba  kWhite{ 235, 235, 235, 255 };
	}

	const char* ToString(PluginState a_state)
	{
		switch (a_state) {
		case PluginState::kStarting:
			return "STARTING";
		case PluginState::kActive:
			return "ACTIVE";
		case PluginState::kOnlineBlocked:
			return "OFF (online session)";
		case PluginState::kFaulted:
			return "OFF (fault)";
		}
		return "?";
	}

	HostPlugin::HostPlugin(IGame& a_game, const Config& a_config) : game_(a_game), config_(a_config)
	{
	}

	HostPlugin::~HostPlugin()
	{
		Shutdown();
	}

	bool HostPlugin::PlayerUsable(const GameSample& a_sample)
	{
		return a_sample.playerExists && !a_sample.playerDead && !a_sample.loadingScreen;
	}

	bool HostPlugin::OnlineSession(const GameSample& a_sample)
	{
		// ASSUMPTION: all three read false in story mode (verified on the debug overlay in Phase 2).
		return a_sample.networkGameInProgress || a_sample.networkSessionStarted || a_sample.networkInSession;
	}

	PlayerStateMsg HostPlugin::MakePlayerState(const GameSample& a_s, const WorldConfig& a_world, std::uint64_t a_nowUs, std::uint32_t a_frame,
		bool a_teleport)
	{
		const McPosition pos = ToMinecraft(a_s.x, a_s.y, a_s.z, a_world);
		const McPosition vel = VelocityToMinecraft(a_s.vx, a_s.vy, a_s.vz, a_world);
		PlayerStateMsg   m{};
		m.x = pos.x;
		m.y = pos.y;
		m.z = pos.z;
		m.vx = static_cast<float>(vel.x);
		m.vy = static_cast<float>(vel.y);
		m.vz = static_cast<float>(vel.z);
		m.yaw = HeadingToYaw(a_s.heading);
		m.pitch = CamPitchToMcPitch(a_s.camPitch);
		const bool onGround = !a_s.inAir && !a_s.falling && !a_s.swimming;
		m.flags = (onGround ? kPlayerOnGround : 0u) | (a_teleport ? kPlayerTeleport : 0u);
		m.timeUs = a_nowUs;
		m.frame = a_frame;
		return m;
	}

	void HostPlugin::Tick(std::uint64_t a_nowMs, std::uint64_t a_nowUs)
	{
		const std::uint64_t start = clock::NowUs();
		try {
			TickImpl(a_nowMs, a_nowUs);
		} catch (const std::exception& e) {
			Fault(e.what());
		} catch (...) {
			Fault("unknown exception");
		}
		MeasureTick(start, a_nowMs);
	}

	void HostPlugin::TickImpl(std::uint64_t a_nowMs, std::uint64_t a_nowUs)
	{
		if (state_ == PluginState::kFaulted || state_ == PluginState::kOnlineBlocked) {
			return;
		}
		game_.Sample(sample_);
		if (OnlineSession(sample_)) {
			HostLog::Warn("online session detected (gameInProgress=%d sessionStarted=%d inSession=%d): CraftV is off until the game restarts",
				sample_.networkGameInProgress, sample_.networkSessionStarted, sample_.networkInSession);
			state_ = PluginState::kOnlineBlocked;
			endpoint_.reset();  // clean detach: Minecraft sees the host leave at once
			return;
		}
		if (!endpoint_) {
			EndpointConfig c;
			c.role = Role::kHost;
			c.mappingName = config_.mappingName.c_str();
			c.peerTimeoutMs = config_.mcTimeoutMs;
			c.software = kSoftware;
			c.log = &HostLog::FromLink;
			endpoint_ = std::make_unique<Endpoint>(c);  // the only allocation, on the first story-mode tick
			state_ = PluginState::kActive;
			HostLog::Info("story mode: link starting (mapping in CraftV_RDR2.ini [Link] MappingName)");
		}

		const bool usable = PlayerUsable(sample_);
		if (usable != inGameReported_) {
			inGameReported_ = usable;
			endpoint_->SetInGame(usable);
		}
		endpoint_->Tick(a_nowMs, a_nowUs);
		HandleEvents();
		endpoint_->Drain(a_nowUs, kMaxDrainBytesPerTick, [this](const RecordHeader& h, const std::uint8_t* p) { OnMessage(h, p); });

		if (usable) {
			const double dx = sample_.x - lastX_, dy = sample_.y - lastY_, dz = sample_.z - lastZ_;
			if (!wasUsable_ || dx * dx + dy * dy + dz * dz > kTeleportDistance * kTeleportDistance) {
				teleportNext_ = true;  // first frame after loading, respawn or fast travel
			}
			lastX_ = sample_.x;
			lastY_ = sample_.y;
			lastZ_ = sample_.z;
			if (endpoint_->Connected()) {
				const PlayerStateMsg m = MakePlayerState(sample_, config_.world, a_nowUs, ++frame_, teleportNext_);
				if (endpoint_->Send(m)) {
					lastSent_ = m;
					++playerStatesSent_;
					teleportNext_ = false;
				}
			}
		}
		wasUsable_ = usable;

		if (config_.debugOverlay) {
			DrawOverlay();
		}
	}

	void HostPlugin::HandleEvents()
	{
		const std::uint32_t ev = endpoint_->TakeEvents();
		if (ev & (kEvConnected | kEvPeerRestarted)) {
			teleportNext_ = true;  // a new Minecraft: snap its player onto ours
		}
	}

	void HostPlugin::OnMessage(const RecordHeader& a_h, const std::uint8_t* a_p)
	{
		// Phase 2 only counts and logs these; Phase 4 turns BLOCK_SET into props.
		switch (a_h.type) {
		case kMsgBlockSet: {
			BlockSetMsg m;
			if (!codec::Decode(a_h, a_p, m)) {
				endpoint_->CountMalformed();
				return;
			}
			++blockMessagesReceived_;
			return;
		}
		case kMsgBlockBreakRequest:
		case kMsgBlockPlaceRequest:
			++blockMessagesReceived_;
			return;
		case kMsgLog: {
			LogMsg m;
			if (!codec::Decode(a_h, a_p, m)) {
				endpoint_->CountMalformed();
				return;
			}
			HostLog::Info("[minecraft] %.*s", static_cast<int>(m.textBytes), m.text);
			return;
		}
		default:
			return;  // TEST_PATTERN etc.
		}
	}

	void HostPlugin::DrawOverlay()
	{
		char        lines[kOverlayLines][160];
		const auto  st = endpoint_->State();
		const auto& peer = endpoint_->Peer();
		const auto& tx = endpoint_->TxStats();
		const auto& rx = endpoint_->RxStats();
		std::snprintf(lines[0], sizeof(lines[0]), "CraftV %s  link %s  ping %.1f ms  MC session %u", kPluginVersion, ToString(st),
			peer.rttValid ? peer.rttUs / 1000.0 : 0.0, peer.session);
		std::snprintf(lines[1], sizeof(lines[1]), "tx %llu  rx %llu  blocks %llu  dropped %llu  bad %llu  tick %.3f ms (max %.3f)",
			static_cast<unsigned long long>(tx.produced), static_cast<unsigned long long>(rx.consumed),
			static_cast<unsigned long long>(blockMessagesReceived_), static_cast<unsigned long long>(tx.droppedFull),
			static_cast<unsigned long long>(rx.malformed + rx.corrupt), cost_.avgUs / 1000.0, cost_.maxUs / 1000.0);
		const McPosition mc = ToMinecraft(sample_.x, sample_.y, sample_.z, config_.world);
		std::snprintf(lines[2], sizeof(lines[2]), "rdr %.1f %.1f %.1f hdg %.0f above ground %.2f  ->  mc %.1f %.1f %.1f yaw %.0f", sample_.x, sample_.y,
			sample_.z, sample_.heading, sample_.heightAboveGround, mc.x, mc.y, mc.z, HeadingToYaw(sample_.heading));
		std::snprintf(lines[3], sizeof(lines[3]), "net game %d session %d in %d | loading %d faded %d | mount %d vehicle %d air %d swim %d",
			sample_.networkGameInProgress, sample_.networkSessionStarted, sample_.networkInSession, sample_.loadingScreen, sample_.screenFadedOut,
			sample_.onMount, sample_.inVehicle, sample_.inAir, sample_.swimming);

		game_.DrawBox(kOverlayX - 0.005f, kOverlayY - 0.004f, kPanelWidth, kLineHeight * kOverlayLines + 0.008f, kPanel);
		const Rgba status = st == LinkState::kConnected ? kGreen : (st == LinkState::kStale || st == LinkState::kDetached) ? kRed : kYellow;
		for (int i = 0; i < kOverlayLines; ++i) {
			game_.DrawLabel(kOverlayX, kOverlayY + kLineHeight * static_cast<float>(i), kTextScale, i == 0 ? status : kWhite, lines[i]);
		}
	}

	void HostPlugin::Fault(const char* a_what)
	{
		if (state_ == PluginState::kFaulted) {
			return;
		}
		state_ = PluginState::kFaulted;
		HostLog::Error("tick failed (%s): CraftV is off until the game restarts; the game keeps running", a_what ? a_what : "?");
		try {
			endpoint_.reset();
		} catch (...) {
			// nothing more to do
		}
	}

	void HostPlugin::MeasureTick(std::uint64_t a_startUs, std::uint64_t a_nowMs)
	{
		const double us = static_cast<double>(clock::NowUs() - a_startUs);
		cost_.lastUs = us;
		cost_.avgUs = cost_.ticks == 0 ? us : cost_.avgUs + (us - cost_.avgUs) * kCostEmaWeight;
		cost_.maxUs = us > cost_.maxUs ? us : cost_.maxUs;
		cost_.maxEverUs = us > cost_.maxEverUs ? us : cost_.maxEverUs;
		++cost_.ticks;
		if (config_.logEveryTickCost) {
			HostLog::Info("tick %.1f us", us);
		}
		if (a_nowMs >= nextCostReportMs_) {
			if (nextCostReportMs_ != 0) {
				HostLog::Info("status: %s, link %s, tick avg %.1f us max %.1f us (ever %.1f us), %llu PLAYER_STATEs, %llu block msgs", ToString(state_),
					endpoint_ ? ToString(endpoint_->State()) : "-", cost_.avgUs, cost_.maxUs, cost_.maxEverUs,
					static_cast<unsigned long long>(playerStatesSent_), static_cast<unsigned long long>(blockMessagesReceived_));
			}
			nextCostReportMs_ = a_nowMs + kCostReportPeriodMs;
			cost_.maxUs = 0;
		}
	}

	void HostPlugin::Shutdown()
	{
		if (endpoint_) {
			HostLog::Info("shutting down (clean detach)");
			endpoint_.reset();
		}
	}
}
