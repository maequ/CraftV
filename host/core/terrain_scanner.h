// Answers Minecraft's TERRAIN_REQUESTs by probing the game's ground (PROTOCOL.md §7.11-7.12, brief §7).
// Fixed storage, no heap; a bounded number of probes per script tick so the game's frame stays light.
//
// One chunk at a time, in the order Minecraft asked (it asks nearest first):
//   - outside the map: answered at once with no ground, so Minecraft stops asking;
//   - far from the player: the game is asked to stream collision there, and probing waits a few ticks;
//   - every column's ground, water and surface material is probed, ProbesPerTick at a time;
//   - if nothing at all was hit, the collision probably wasn't loaded: no answer, Minecraft asks again
//     after 5 s; after MaxAttempts such misses the chunk is answered with no ground.
#pragma once

#include "coords.h"
#include "game_api.h"

#include "craftv/protocol.h"

#include <cstdint>

namespace craftv::host
{
	struct TerrainConfig
	{
		int   probesPerTick = 64;       // [Terrain] ProbesPerTick
		int   probeBudgetUs = 1500;     // [Terrain] ProbeBudgetUs: stop probing for this frame once this much time is spent
		int   collisionWaitTicks = 10;  // [Terrain] CollisionWaitTicks
		float nearDistance = 150.0f;    // [Terrain] NearDistance, metres: closer chunks are probed at once
		int   maxAttempts = 3;          // [Terrain] MaxAttempts
		// ASSUMPTION: GTA V's world, North Yankton (around x 5300, y -5200) included, lies within these bounds
		// (metres). Chunks outside are answered empty. Too wide only costs a few empty scans of open sea.
		float mapMinX = -6000.0f, mapMaxX = 7000.0f, mapMinY = -7000.0f, mapMaxY = 9000.0f;
	};

	struct TerrainStats
	{
		std::uint64_t budgetStops = 0;  // frames that stopped probing early on the time budget
		std::uint64_t served = 0;      // patches with ground
		std::uint64_t empty = 0;       // patches with no ground (outside the map, or nothing after retries)
		std::uint64_t deferred = 0;    // scans that hit nothing and weren't answered (Minecraft asks again)
		std::uint64_t dropped = 0;     // requests ignored because the queue was full
		std::uint64_t probes = 0;
		std::uint64_t unknownMaterials = 0;
		std::uint32_t lastMaterialHash = 0;
	};

	class TerrainScanner
	{
	public:
		static constexpr int kQueueCapacity = 128;
		static constexpr int kAttemptSlots = 64;

		explicit TerrainScanner(const TerrainConfig& a_config = {}) : config_(a_config) {}

		void Configure(const TerrainConfig& a_config) { config_ = a_config; }

		// A request from Minecraft. Duplicates of a queued or in-progress chunk are ignored.
		void Enqueue(const proto::TerrainRequestMsg& a_request);

		// Probes up to ProbesPerTick columns. Returns a finished patch to send now, or nullptr.
		// (a_playerX/Y/Z: the player's position, game metres.)
		const proto::TerrainPatchMsg* Tick(IGame& a_game, const WorldConfig& a_world, float a_playerX, float a_playerY, float a_playerZ);

		// The host restarted or Minecraft did: forget everything queued.
		void Reset();

		int                 Queued() const { return count_; }
		bool                Scanning() const { return scanning_; }
		int                 Progress() const { return column_; }  // columns done in the current chunk, 0..256
		std::int32_t        CurrentChunkX() const { return current_.chunkX; }
		std::int32_t        CurrentChunkZ() const { return current_.chunkZ; }
		const TerrainStats& Stats() const { return stats_; }

		// Minecraft column centre -> game x/y (public for tests).
		static void ColumnToGame(std::int32_t a_mcX, std::int32_t a_mcZ, const WorldConfig& a_world, float& a_x, float& a_y);
		// Game height -> the Y of the top block whose top is nearest that height.
		static std::int16_t HeightToBlockY(float a_z, const WorldConfig& a_world);

	private:
		bool        Pop(proto::TerrainRequestMsg& a_out);
		bool        Contains(std::int32_t a_chunkX, std::int32_t a_chunkZ) const;
		bool        InsideMap(float a_x, float a_y) const;
		int         Attempt(std::int32_t a_chunkX, std::int32_t a_chunkZ);  // counts a miss, returns the total
		void        ForgetAttempts(std::int32_t a_chunkX, std::int32_t a_chunkZ);
		void        FillEmpty();

		struct AttemptSlot
		{
			bool         used = false;
			std::int32_t chunkX = 0, chunkZ = 0;
			int          misses = 0;
		};

		TerrainConfig              config_;
		proto::TerrainRequestMsg   queue_[kQueueCapacity] = {};
		int                        head_ = 0, count_ = 0;
		bool                       scanning_ = false;
		proto::TerrainRequestMsg   current_{};
		int                        column_ = 0;
		int                        waitTicks_ = 0;
		int                        hits_ = 0;
		proto::TerrainPatchMsg     patch_{};
		AttemptSlot                attempts_[kAttemptSlots];
		int                        nextAttemptSlot_ = 0;
		static constexpr int       kUnknownLogged = 32;  // distinct unknown surface hashes logged per session
		std::uint32_t              unknownLogged_[kUnknownLogged]{};
		int                        unknownLoggedCount_ = 0;
		TerrainStats               stats_{};
	};
}
