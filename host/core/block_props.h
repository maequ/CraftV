// Minecraft blocks made solid in the game (brief §9, PROTOCOL.md §7.19): Minecraft draws them through the
// passthrough; here each one near the player gets an invisible, frozen collision prop, so people, cars and the player
// stop at what friends and the owner build. Fixed-size tables, no heap allocation after construction.
//   - every BLOCK_SET with SOLID adds a block, any other BLOCK_SET removes one (and its prop)
//   - props spawn nearest first within spawnRadius, at most maxProps live; far ones despawn
//   - chunks near the player are asked for with BLOCK_REGION_REQUEST (builds from before, or from while away)
#pragma once

#include "config.h"
#include "game_api.h"

#include "craftv/endpoint.h"

#include <cstdint>

namespace craftv::host
{
	class BlockProps
	{
	public:
		static constexpr int kCapacity = 16384;  // known blocks (power of two)
		static constexpr int kMaxRegions = 1024; // chunk columns asked for

		explicit BlockProps(const BlocksConfig& a_config) : config_(a_config) {}

		void OnBlockSet(const proto::BlockSetMsg& a_msg, IGame& a_game);
		// Spawns and despawns props around the player (game coordinates) and asks for nearby chunks. Script thread.
		void Tick(IGame& a_game, Endpoint& a_link, const WorldConfig& a_world, float a_playerX, float a_playerY);
		// Deletes every prop and forgets every block and region (link lost, a new Minecraft).
		void Clear(IGame& a_game);

		int           Live() const { return live_; }
		int           Known() const { return known_; }
		std::uint64_t CapHits() const { return capHits_; }
		std::uint64_t RegionsAsked() const { return regionsAsked_; }

	private:
		struct Block
		{
			std::int32_t x = 0, y = 0, z = 0;
			int          handle = 0;  // the prop, 0 = none
			std::uint8_t state = 0;   // 0 empty, 1 used, 2 deleted (tombstone)
		};
		int  Find(std::int32_t a_x, std::int32_t a_y, std::int32_t a_z) const;
		void Insert(std::int32_t a_x, std::int32_t a_y, std::int32_t a_z);
		void Remove(int a_slot, IGame& a_game);
		bool Spawn(Block& a_block, IGame& a_game, const WorldConfig& a_world);
		bool RegionAsked(std::int64_t a_key) const;
		void Rehash();

		BlocksConfig  config_;
		Block         blocks_[kCapacity];
		Block         scratch_[kCapacity];  // Rehash only
		int           known_ = 0, tombstones_ = 0, live_ = 0;
		std::int64_t  regions_[kMaxRegions] = {};
		int           regionCount_ = 0;
		std::uint32_t nextRequestId_ = 0;
		std::uint32_t tick_ = 0;
		std::uint64_t capHits_ = 0, regionsAsked_ = 0;
	};
}
