// The game side of CraftV, minus the natives: one object, ticked once per script frame.
//   - story mode only: any sign of an online session switches it off for good (DECISIONS D-011)
//   - owns the craftv::Endpoint (role host, PROTOCOL.md §5) and drains Minecraft's ring every tick
//   - sends PLAYER_STATE from the player ped (PROTOCOL.md §7.3), converted by coords.h
//   - answers TERRAIN_REQUESTs by probing the game's ground (§7.11-7.12, terrain_scanner.h)
//   - keeps the friends Minecraft reports (§7.8-7.10) and how they join (§7.13) for the overlay
//   - the passthrough (brief §8): sends the game's camera, window size and the owner's buttons, hides the
//     player, turns Minecraft melee hits into game damage (PROTOCOL.md §7.15-7.18)
//   - draws a debug overlay through IGame
//   - never throws, never blocks; no heap allocation after the first tick
#pragma once

#include "config.h"
#include "friends.h"
#include "game_api.h"
#include "terrain_scanner.h"

#include "craftv/endpoint.h"

#include <atomic>
#include <cstdint>
#include <memory>

namespace craftv::host
{
	inline constexpr const char* kPluginVersion = "0.1.0";

	enum class PluginState
	{
		kStarting,       // nothing sampled yet
		kActive,         // story mode, link running
		kOnlineBlocked,  // an online session was seen: inert until the game restarts
		kFaulted,        // an exception escaped a tick: inert, logged
	};
	const char* ToString(PluginState a_state);

	struct TickCost
	{
		double        lastUs = 0;
		double        avgUs = 0;     // exponential moving average
		double        maxUs = 0;     // max over the current reporting window
		double        maxEverUs = 0;
		std::uint64_t ticks = 0;
	};

	class HostPlugin
	{
	public:
		HostPlugin(IGame& a_game, const Config& a_config);
		HostPlugin(const HostPlugin&) = delete;
		HostPlugin& operator=(const HostPlugin&) = delete;
		~HostPlugin();

		// One script frame. Catches everything; a throw puts the plugin in kFaulted.
		void Tick(std::uint64_t a_nowMs, std::uint64_t a_nowUs);
		// Clean detach (DLL unload / game exit).
		void Shutdown();

		PluginState                  State() const { return state_; }
		const Endpoint*              Link() const { return endpoint_.get(); }
		const TickCost&              Cost() const { return cost_; }
		const GameSample&            LastSample() const { return sample_; }
		const proto::PlayerStateMsg& LastSentPlayerState() const { return lastSent_; }
		std::uint64_t                PlayerStatesSent() const { return playerStatesSent_; }
		std::uint64_t                BlockMessagesReceived() const { return blockMessagesReceived_; }
		const TerrainScanner&        Terrain() const { return terrain_; }
		const Friends&               FriendsTable() const { return friends_; }
		bool                         HasSessionInfo() const { return hasSession_; }
		const proto::SessionInfoMsg& Session() const { return session_; }
		bool                         PassthroughActive() const { return passthroughActive_; }
		// F7: any thread (the game's keyboard hook). Applied on the next tick.
		void RequestPassthroughToggle() { toggleRequested_.store(true, std::memory_order_relaxed); }

		// True while the game is in a state where the player's position means something.
		static bool PlayerUsable(const GameSample& a_sample);
		// Any of the network natives says we're online (PROTOCOL safety rule, brief §2.1).
		static bool OnlineSession(const GameSample& a_sample);
		// Builds the PLAYER_STATE for a sample (public for tests).
		static proto::PlayerStateMsg MakePlayerState(const GameSample& a_sample, const WorldConfig& a_world, std::uint64_t a_nowUs,
			std::uint32_t a_frame, bool a_teleport);
		// The passthrough's CAMERA (§7.15) for the rendered camera and the player (public for tests).
		static proto::CameraMsg MakeCamera(const CameraSample& a_camera, const GameSample& a_sample, const WorldConfig& a_world, std::uint64_t a_frame,
			std::uint64_t a_nowUs);
		// Minecraft's window for the game's picture: same aspect, at most a_maxPixels (§7.16).
		static proto::ViewMsg MakeView(int a_width, int a_height, std::uint64_t a_maxPixels);
		// Game damage for a Minecraft melee swing: vanilla's charge scaling (0.2 + 0.8 * charge²) of the held item's damage.
		static float MeleeDamage(const proto::OwnerStateMsg& a_owner, double a_perHalfHeart);

	private:
		void TickImpl(std::uint64_t a_nowMs, std::uint64_t a_nowUs);
		void HandleEvents();
		void OnMessage(const proto::RecordHeader& a_header, const std::uint8_t* a_payload);
		void DrawOverlay();
		void Fault(const char* a_what);
		void MeasureTick(std::uint64_t a_startUs, std::uint64_t a_nowMs);
		void ExpireSilentFriends(std::uint64_t a_nowMs, std::uint64_t a_gapMs);
		void TickPassthrough(bool a_linked, std::uint64_t a_nowUs);
		void SetPassthrough(bool a_on);

		IGame&                    game_;
		Config                    config_;
		std::unique_ptr<Endpoint> endpoint_;
		PluginState               state_ = PluginState::kStarting;
		GameSample                sample_{};
		bool                      inGameReported_ = false;
		bool                      teleportNext_ = true;
		bool                      wasUsable_ = false;
		double                    lastX_ = 0, lastY_ = 0, lastZ_ = 0;
		std::uint32_t             frame_ = 0;
		proto::PlayerStateMsg     lastSent_{};
		std::uint64_t             playerStatesSent_ = 0;
		std::uint64_t             blockMessagesReceived_ = 0;
		TerrainScanner            terrain_;
		Friends                   friends_;
		proto::SessionInfoMsg     session_{};
		bool                      hasSession_ = false;
		std::uint64_t             nowMs_ = 0;
		std::uint64_t             silenceBaseMs_ = 0;  // friends' silence counts from here at the earliest
		// the passthrough
		std::atomic<bool>         toggleRequested_{ false };
		bool                      passthroughWanted_ = true;   // Mode=Auto, flipped by F7
		bool                      passthroughActive_ = false;
		std::uint64_t             cameraFrame_ = 0;
		std::uint64_t             camerasSent_ = 0;
		proto::ViewMsg            viewSent_{};
		proto::OwnerStateMsg      owner_{};
		bool                      hasOwner_ = false;
		std::uint64_t             meleeHits_ = 0;
		TickCost                  cost_{};
		std::uint64_t             nextCostReportMs_ = 0;
	};
}
