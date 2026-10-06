#include "host_plugin.h"

#include "coords.h"
#include "host_log.h"
#include "materials.h"

#include "craftv/clock.h"
#include "craftv/codec.h"

#include <cmath>
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
		// Minecraft sends every friend's state every tick (§7.9). One silent this long while the link is up has
		// left without its LEAVE reaching us. A gap this long between our own ticks means the game was paused.
		constexpr std::uint64_t kFriendSilenceMs = 5000;
		constexpr std::uint64_t kPauseGapMs = 1000;
		// The passthrough's view lift: re-measured every few frames, eased in so stepping between columns doesn't pop.
		constexpr std::uint32_t kLiftProbeEvery = 4;
		constexpr double        kLiftEase = 0.15;

		// Overlay layout (screen fractions).
		constexpr float kOverlayMargin = 0.01f;
		constexpr float kOverlayTopRightY = 0.17f;  // below GTA's wanted stars, cash and weapon/ammo
		constexpr float kPanelPad = 0.005f;
		constexpr float kLineHeight = 0.022f;
		constexpr float kTextScale = 0.28f;
		constexpr float kPanelWidthCompact = 0.25f;
		constexpr float kPanelWidthDetailed = 0.46f;
		constexpr int   kOverlayLinesCompact = 3;
		constexpr int   kOverlayLinesDetailed = 5;
		constexpr int   kOverlayFriends = 3;  // names shown on the friends line
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

	HostPlugin::HostPlugin(IGame& a_game, const Config& a_config) :
		game_(a_game), config_(a_config), terrain_(a_config.terrain), passthroughWanted_(a_config.passthrough.mode == PassthroughMode::kAuto)
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

	CameraMsg HostPlugin::MakeCamera(const CameraSample& a_cam, const GameSample& a_s, const WorldConfig& a_world, std::uint64_t a_frame,
		std::uint64_t a_nowUs)
	{
		CameraMsg m{};
		m.frame = a_frame;
		m.timeUs = a_nowUs;
		const McPosition c = PointToMinecraft(a_cam.x, a_cam.y, a_cam.z, a_world);
		m.x = c.x;
		m.y = c.y;
		m.z = c.z;
		m.yaw = HeadingToYaw(a_cam.heading);
		m.pitch = CamPitchToMcPitch(a_cam.pitch);
		m.roll = a_cam.roll;
		m.fovY = std::clamp(a_cam.fovY, kMinFov, kMaxFov);
		const McPosition f = ToMinecraft(a_s.x, a_s.y, a_s.z, a_world);
		m.feetX = f.x;
		m.feetY = f.y;
		m.feetZ = f.z;
		m.bodyYaw = HeadingToYaw(a_s.heading);
		m.flags = kCameraPassthrough | (a_cam.firstPerson ? kCameraFirstPerson : 0u) | (a_s.inVehicle ? kCameraInVehicle : 0u);
		m.nearClip = std::max(0.0f, a_cam.nearClip);
		m.farClip = std::max(0.0f, a_cam.farClip);
		return m;
	}

	ViewMsg HostPlugin::MakeView(int a_width, int a_height, std::uint64_t a_maxPixels)
	{
		ViewMsg      v{};
		const double w = std::max(1, a_width), h = std::max(1, a_height);
		const double pixels = static_cast<double>(std::min<std::uint64_t>(a_maxPixels, kViewMaxPixels));
		double       scale = std::min(1.0, std::sqrt(pixels / (w * h)));
		scale = std::min({ scale, kViewMaxWidth / w, kViewMaxHeight / h });
		v.width = std::clamp(static_cast<std::uint32_t>(w * scale), kViewMinSide, kViewMaxWidth);
		v.height = std::clamp(static_cast<std::uint32_t>(h * scale), kViewMinSide, kViewMaxHeight);
		v.hostWidth = std::min(static_cast<std::uint32_t>(w), kHostMaxSide);
		v.hostHeight = std::min(static_cast<std::uint32_t>(h), kHostMaxSide);
		return v;
	}

	float HostPlugin::MeleeDamage(const OwnerStateMsg& a_owner, double a_perHalfHeart)
	{
		const double charge = std::clamp(static_cast<double>(a_owner.attackCharge), 0.0, 1.0);
		return static_cast<float>(a_owner.attackDamage * (0.2 + 0.8 * charge * charge) * a_perHalfHeart);
	}

	void HostPlugin::SetPassthrough(bool a_on)
	{
		if (a_on == passthroughActive_) {
			return;
		}
		passthroughActive_ = a_on;
		game_.SetCompositorActive(a_on);
		viewSent_ = ViewMsg{};  // size Minecraft's window again next time
		HostLog::Info("passthrough %s", a_on ? "on: Minecraft draws the player, hand and HUD into the game's picture" : "off");
	}

	// The passthrough (brief §8, PROTOCOL.md §7.15-7.17): every frame the rendered camera, the window size when it
	// changes, and the owner's buttons; a Minecraft melee swing hurts the people in front of the player.
	void HostPlugin::TickPassthrough(bool a_linked, std::uint64_t a_nowUs)
	{
		if (toggleRequested_.exchange(false, std::memory_order_relaxed)) {
			passthroughWanted_ = !passthroughWanted_;
			const bool available = game_.PassthroughAvailable();
			game_.Notify(!available ? "CraftV: the Minecraft view needs ReShade (see INSTALL.txt)"
			             : passthroughWanted_ ? "CraftV: Minecraft view on" : "CraftV: Minecraft view off");
		}
		SetPassthrough(a_linked && passthroughWanted_ && game_.PassthroughAvailable());
		// Minecraft draws the player on foot; in a vehicle the game's own driver shows (Minecraft has no car to seat
		// them in, and hides its player then, PROTOCOL.md §7.15 IN_VEHICLE).
		const bool hide = passthroughActive_ && !sample_.inVehicle;
		if (hide != playerHidden_) {
			playerHidden_ = hide;
			game_.SetPlayerHidden(hide);
		}
		if (!passthroughActive_) {
			return;
		}
		// CraftV's terrain holds the game's ground in whole blocks, so its top is up to half a block off the real ground.
		// Lift Minecraft's camera (and the owner's drawn feet) by that offset at the player: Minecraft's world, blocks
		// placed on it included, then meets the game's ground where the player is. The server never sees the lift.
		if (++liftProbeTick_ % kLiftProbeEvery == 0) {
			GroundProbe ground;
			if (game_.ProbeGround(sample_.x, sample_.y, ground) && ground.hit) {
				const double groundMc = ground.groundZ * config_.world.blocksPerMetre + config_.world.yOffset;
				viewLiftTarget_ = (TerrainScanner::HeightToBlockY(ground.groundZ, config_.world) + 1.0) - groundMc;
			}
		}
		viewLift_ += (viewLiftTarget_ - viewLift_) * kLiftEase;
		CameraSample cam;
		game_.SampleCamera(cam);
		if (cam.valid) {
			CameraMsg m = MakeCamera(cam, sample_, config_.world, ++cameraFrame_, a_nowUs);
			m.y += viewLift_;
			m.feetY += viewLift_;
			if (endpoint_->Send(m)) {
				++camerasSent_;
			}
			game_.CompositorPose(m.yaw, m.pitch, m.roll, m.fovY, m.x, m.y, m.z, m.nearClip, m.farClip);
		}
		int w = 0, h = 0;
		if (game_.ScreenSize(w, h) && w > 0 && h > 0) {
			const ViewMsg v = MakeView(w, h, config_.passthrough.maxPixels);
			if ((v.width != viewSent_.width || v.height != viewSent_.height) && endpoint_->Send(v)) {
				viewSent_ = v;
			}
		}
		PassthroughInput in;
		game_.TakePassthroughInput(in);
		auto button = [&](bool a_pressed, bool a_released, std::uint8_t a_button) {
			if (a_pressed) {
				endpoint_->Send(InputMsg{ kInputButton, a_button, 1, 0, 0 });
			}
			if (a_released) {
				endpoint_->Send(InputMsg{ kInputButton, a_button, 0, 0, 0 });
			}
		};
		button(in.attackPressed, in.attackReleased, kButtonAttack);
		button(in.usePressed, in.useReleased, kButtonUse);
		if (in.slot >= 0 && in.slot < kHotbarSlots) {
			endpoint_->Send(InputMsg{ kInputSlot, 0, 0, static_cast<std::int8_t>(in.slot), 0 });
		}
		if (in.scroll != 0) {
			endpoint_->Send(InputMsg{ kInputScroll, 0, 0, static_cast<std::int8_t>(std::clamp(in.scroll, -int(kHotbarSlots), int(kHotbarSlots))), 0 });
		}
		if (in.attackPressed && hasOwner_ && (owner_.flags & (kOwnerDead | kOwnerScreenOpen)) == 0) {
			const float damage = MeleeDamage(owner_, config_.passthrough.meleeDamagePerHalfHeart);
			if (damage > 0.0f) {
				game_.Melee(damage);
				++meleeHits_;
			}
		}
		if (config_.passthrough.hideGtaHud) {
			game_.HideHudThisFrame();
		}
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
			SetPassthrough(false);
			if (playerHidden_) {
				playerHidden_ = false;
				game_.SetPlayerHidden(false);
			}
			endpoint_.reset();  // clean detach: Minecraft sees the host leave at once
			return;
		}
		if (!endpoint_) {
			EndpointConfig c;
			c.role = Role::kHost;
			c.mappingName = config_.mappingName.c_str();
			c.peerTimeoutMs = config_.mcTimeoutMs;
			c.software = config_.software.c_str();
			c.log = &HostLog::FromLink;
			endpoint_ = std::make_unique<Endpoint>(c);  // the only allocation, on the first story-mode tick
			state_ = PluginState::kActive;
			HostLog::Info("story mode: link starting (%s)", config_.software.c_str());
		}

		const std::uint64_t gapMs = nowMs_ != 0 && a_nowMs > nowMs_ ? a_nowMs - nowMs_ : 0;
		nowMs_ = a_nowMs;
		const bool usable = PlayerUsable(sample_);
		if (usable != inGameReported_) {
			inGameReported_ = usable;
			endpoint_->SetInGame(usable);
		}
		endpoint_->Tick(a_nowMs, a_nowUs);
		HandleEvents();
		endpoint_->Drain(a_nowUs, kMaxDrainBytesPerTick, [this](const RecordHeader& h, const std::uint8_t* p) { OnMessage(h, p); });
		ExpireSilentFriends(a_nowMs, gapMs);

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
				// Ground for the friends: only while the player is in the world, so the game has collision loaded.
				if (const TerrainPatchMsg* patch = terrain_.Tick(game_, config_.world, sample_.x, sample_.y, sample_.z)) {
					endpoint_->Send(*patch);  // if the ring is full, Minecraft asks again (§7.11)
				}
			}
		}
		wasUsable_ = usable;
		TickPassthrough(usable && endpoint_->Connected() && !sample_.screenFadedOut, a_nowUs);

		if (config_.debugOverlay) {
			DrawOverlay();
		}
	}

	void HostPlugin::HandleEvents()
	{
		const std::uint32_t ev = endpoint_->TakeEvents();
		if (ev & (kEvConnected | kEvPeerRestarted)) {
			hasOwner_ = false;
			teleportNext_ = true;  // a new Minecraft: snap its player onto ours
			terrain_.Reset();      // its requests and friends come again (§7.10, §7.11)
			friends_.Clear();
			hasSession_ = false;
		}
	}

	// While the link is stale (Minecraft gone quiet, or this game paused so we stopped reading), Minecraft drops
	// its messages, LEAVEs included. A friend still there is announced again when the link recovers (§7.10); one
	// that stays silent afterwards left meanwhile, so the table forgets it.
	void HostPlugin::ExpireSilentFriends(std::uint64_t a_nowMs, std::uint64_t a_gapMs)
	{
		if (!endpoint_->Connected() || a_gapMs > kPauseGapMs) {
			silenceBaseMs_ = a_nowMs;
		}
		for (int i = 0; i < Friends::kMax; ++i) {
			const Friend&       f = friends_.Slot(i);
			const std::uint64_t since = f.lastUpdateMs > silenceBaseMs_ ? f.lastUpdateMs : silenceBaseMs_;
			if (f.used && a_nowMs > since && a_nowMs - since > kFriendSilenceMs) {
				HostLog::Info("friend %s (#%u) went silent; forgetting them", f.name, f.id);
				friends_.Leave(f.id);
			}
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
		case kMsgTerrainRequest: {
			TerrainRequestMsg m;
			if (!codec::Decode(a_h, a_p, m)) {
				endpoint_->CountMalformed();
				return;
			}
			terrain_.Enqueue(m);
			return;
		}
		case kMsgRemotePlayerJoin: {
			RemotePlayerJoinMsg m;
			if (!codec::Decode(a_h, a_p, m)) {
				endpoint_->CountMalformed();
				return;
			}
			const bool known = friends_.Has(m.playerId);  // Minecraft re-announces everyone after a stall (§7.10)
			if (friends_.Join(m, nowMs_)) {
				if (!known) {
					HostLog::Info("friend joined: %.*s (#%u)", static_cast<int>(m.nameBytes), m.name, m.playerId);
				}
			} else {
				HostLog::Warn("friend table full (%d): ignoring %.*s", Friends::kMax, static_cast<int>(m.nameBytes), m.name);
			}
			return;
		}
		case kMsgRemotePlayerState: {
			RemotePlayerStateMsg m;
			if (!codec::Decode(a_h, a_p, m)) {
				endpoint_->CountMalformed();
				return;
			}
			friends_.State(m, nowMs_);
			return;
		}
		case kMsgRemotePlayerLeave: {
			RemotePlayerLeaveMsg m;
			if (!codec::Decode(a_h, a_p, m)) {
				endpoint_->CountMalformed();
				return;
			}
			friends_.Leave(m.playerId);
			HostLog::Info("friend #%u left (reason %u)", m.playerId, m.reason);
			return;
		}
		case kMsgOwnerState: {
			OwnerStateMsg m;
			if (!codec::Decode(a_h, a_p, m)) {
				endpoint_->CountMalformed();
				return;
			}
			owner_ = m;
			hasOwner_ = true;
			return;
		}
		case kMsgSessionInfo: {
			SessionInfoMsg m;
			if (!codec::Decode(a_h, a_p, m)) {
				endpoint_->CountMalformed();
				return;
			}
			session_ = m;
			hasSession_ = true;
			return;
		}
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
		// Three short lines by default; [Debug] OverlayDetails=1 adds counters, positions and session flags.
		const bool  details = config_.overlayDetails;
		const int   count = details ? kOverlayLinesDetailed : kOverlayLinesCompact;
		char        lines[kOverlayLinesDetailed][200];
		const auto  st = endpoint_->State();
		const auto& peer = endpoint_->Peer();
		const auto& tx = endpoint_->TxStats();
		const auto& rx = endpoint_->RxStats();
		if (details) {
			std::snprintf(lines[0], sizeof(lines[0]), "CraftV %s  link %s  ping %.1f ms  tx %llu rx %llu  bad %llu  tick %.3f ms (max %.3f)", kPluginVersion,
				ToString(st), peer.rttValid ? peer.rttUs / 1000.0 : 0.0, static_cast<unsigned long long>(tx.produced),
				static_cast<unsigned long long>(rx.consumed), static_cast<unsigned long long>(rx.malformed + rx.corrupt), cost_.avgUs / 1000.0,
				cost_.maxUs / 1000.0);
		} else {
			std::snprintf(lines[0], sizeof(lines[0]), "CraftV  link %s  ping %.0f ms  tick %.2f ms%s", ToString(st), peer.rttValid ? peer.rttUs / 1000.0 : 0.0,
				cost_.avgUs / 1000.0, passthroughActive_ ? "  MC view" : "");
		}

		// Friends: how many, how they join, and the first few by name with their distance.
		int        n = 0;
		const int  friends = friends_.Count();
		const auto me = ToMinecraft(sample_.x, sample_.y, sample_.z, config_.world);
		if (hasSession_ && (session_.flags & kSessionOpen)) {
			n = std::snprintf(lines[1], sizeof(lines[1]), "friends %d/%u  join: %.*s", friends, session_.maxPlayers, static_cast<int>(session_.addressBytes),
				session_.address);
		} else {
			n = std::snprintf(lines[1], sizeof(lines[1]), "friends %d  (not open to friends yet)", friends);
		}
		for (int i = 0, shown = 0; i < Friends::kMax && shown < kOverlayFriends && n > 0 && n < static_cast<int>(sizeof(lines[1])); ++i) {
			const Friend& f = friends_.Slot(i);
			if (!f.used) {
				continue;
			}
			const double dx = f.last.x - me.x, dz = f.last.z - me.z;
			n += f.hasState ? std::snprintf(lines[1] + n, sizeof(lines[1]) - n, "  %s %.0fm", f.name, std::sqrt(dx * dx + dz * dz))
			                : std::snprintf(lines[1] + n, sizeof(lines[1]) - n, "  %s", f.name);
			++shown;
		}

		const auto& ts = terrain_.Stats();
		const char* mat = MaterialName(ts.lastMaterialHash);
		const char* surface = mat ? mat : (ts.lastMaterialHash ? "?" : "-");
		if (details) {
			std::snprintf(lines[2], sizeof(lines[2]), "terrain sent %llu  empty %llu  retry %llu  queue %d  %s (%d,%d) %d/256  material %s",
				static_cast<unsigned long long>(ts.served), static_cast<unsigned long long>(ts.empty), static_cast<unsigned long long>(ts.deferred),
				terrain_.Queued(), terrain_.Scanning() ? "scanning" : "idle", terrain_.CurrentChunkX(), terrain_.CurrentChunkZ(), terrain_.Progress(), surface);
			std::snprintf(lines[3], sizeof(lines[3]), "game %.1f %.1f %.1f hdg %.0f above ground %.2f  ->  mc %.1f %.1f %.1f yaw %.0f", sample_.x, sample_.y,
				sample_.z, sample_.heading, sample_.heightAboveGround, me.x, me.y, me.z, HeadingToYaw(sample_.heading));
			std::snprintf(lines[4], sizeof(lines[4]), "net game %d session %d in %d | loading %d faded %d | vehicle %d air %d swim %d",
				sample_.networkGameInProgress, sample_.networkSessionStarted, sample_.networkInSession, sample_.loadingScreen, sample_.screenFadedOut,
				sample_.inVehicle, sample_.inAir, sample_.swimming);
		} else {
			std::snprintf(lines[2], sizeof(lines[2]), "ground sent %llu  queue %d  surface %s", static_cast<unsigned long long>(ts.served), terrain_.Queued(),
				surface);
		}

		// Top right sits below GTA's own wanted stars, cash and weapon; top left is the old spot.
		const float width = details ? kPanelWidthDetailed : kPanelWidthCompact;
		const bool  right = config_.overlayCorner == OverlayCorner::kTopRight;
		const float x = right ? 1.0f - kOverlayMargin - width + kPanelPad : kOverlayMargin;
		const float y = right ? kOverlayTopRightY : kOverlayMargin;
		game_.DrawBox(x - kPanelPad, y - 0.004f, width, kLineHeight * static_cast<float>(count) + 0.008f, kPanel);
		const Rgba status = st == LinkState::kConnected ? kGreen : (st == LinkState::kStale || st == LinkState::kDetached) ? kRed : kYellow;
		for (int i = 0; i < count; ++i) {
			game_.DrawLabel(x, y + kLineHeight * static_cast<float>(i), kTextScale, i == 0 ? status : kWhite, lines[i]);
		}
	}

	void HostPlugin::Fault(const char* a_what)
	{
		if (playerHidden_) {
			try {
				game_.SetPlayerHidden(false);  // never leave the player invisible
			} catch (...) {
			}
			playerHidden_ = false;
		}
		passthroughActive_ = false;
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
				const auto& ts = terrain_.Stats();
				HostLog::Info("status: %s, link %s, tick avg %.1f us max %.1f us (ever %.1f us), %llu PLAYER_STATEs, %llu block msgs, %d friends, "
							  "terrain %llu sent %llu empty %llu retried %llu probes",
					ToString(state_), endpoint_ ? ToString(endpoint_->State()) : "-", cost_.avgUs, cost_.maxUs, cost_.maxEverUs,
					static_cast<unsigned long long>(playerStatesSent_), static_cast<unsigned long long>(blockMessagesReceived_), friends_.Count(),
					static_cast<unsigned long long>(ts.served), static_cast<unsigned long long>(ts.empty), static_cast<unsigned long long>(ts.deferred),
					static_cast<unsigned long long>(ts.probes));
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
