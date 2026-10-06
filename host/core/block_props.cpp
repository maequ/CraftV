#include "block_props.h"

#include "coords.h"
#include "host_log.h"
#include "terrain_scanner.h"

#include <cmath>

namespace craftv::host
{
	using namespace craftv::proto;

	namespace
	{
		constexpr int           kMask = BlockProps::kCapacity - 1;
		constexpr std::uint32_t kRegionEvery = 15;   // ticks between looking for chunks to ask for
		constexpr std::uint32_t kEvictEvery = 120;   // ticks between forgetting far blocks
		constexpr int           kChunkSize = 16;
		constexpr double        kMaxGroundShift = 0.6;  // metres: the terrain's rounding is at most half a block

		std::uint32_t Hash(std::int32_t a_x, std::int32_t a_y, std::int32_t a_z)
		{
			std::uint32_t h = static_cast<std::uint32_t>(a_x) * 73856093u ^ static_cast<std::uint32_t>(a_y) * 19349663u ^ static_cast<std::uint32_t>(a_z) * 83492791u;
			return h ^ (h >> 15);
		}

		std::int64_t RegionKey(std::int32_t a_cx, std::int32_t a_cz)
		{
			return (static_cast<std::int64_t>(a_cx) << 32) ^ static_cast<std::uint32_t>(a_cz);
		}

		// Distance in the ground plane from the player (game metres) to a block's centre.
		double Distance2(const WorldConfig& a_world, float a_px, float a_py, std::int32_t a_bx, std::int32_t a_bz)
		{
			const double s = a_world.blocksPerMetre;
			const double dx = (a_bx + 0.5) / s - a_px, dy = -(a_bz + 0.5) / s - a_py;
			return dx * dx + dy * dy;
		}
	}

	int BlockProps::Find(std::int32_t a_x, std::int32_t a_y, std::int32_t a_z) const
	{
		for (std::uint32_t i = Hash(a_x, a_y, a_z), n = 0; n < kCapacity; ++n, ++i) {
			const Block& b = blocks_[i & kMask];
			if (b.state == 0) {
				return -1;
			}
			if (b.state == 1 && b.x == a_x && b.y == a_y && b.z == a_z) {
				return static_cast<int>(i & kMask);
			}
		}
		return -1;
	}

	void BlockProps::Insert(std::int32_t a_x, std::int32_t a_y, std::int32_t a_z)
	{
		if (Find(a_x, a_y, a_z) >= 0) {
			return;
		}
		if (tombstones_ > kCapacity / 4) {
			Rehash();
		}
		if (known_ + tombstones_ >= kCapacity * 3 / 4) {
			HostLog::Warn("blocks: %d known blocks, the table is full; this one gets no collision", known_);
			return;
		}
		for (std::uint32_t i = Hash(a_x, a_y, a_z);; ++i) {
			Block& b = blocks_[i & kMask];
			if (b.state != 1) {
				tombstones_ -= b.state == 2 ? 1 : 0;
				b = Block{ a_x, a_y, a_z, 0, 1 };
				++known_;
				return;
			}
		}
	}

	void BlockProps::Remove(int a_slot, IGame& a_game)
	{
		Block& b = blocks_[a_slot];
		if (b.handle != 0) {
			a_game.DeleteBlockProp(b.handle);
			--live_;
		}
		b = Block{};
		b.state = 2;
		--known_;
		++tombstones_;
	}

	void BlockProps::OnBlockSet(const BlockSetMsg& a_msg, IGame& a_game)
	{
		if (a_msg.flags & kBlockSetSolid) {
			Insert(a_msg.x, a_msg.y, a_msg.z);
		} else if (const int slot = Find(a_msg.x, a_msg.y, a_msg.z); slot >= 0) {
			Remove(slot, a_game);
		}
	}

	bool BlockProps::Spawn(Block& a_block, IGame& a_game, const WorldConfig& a_world)
	{
		// The block's floor in game coordinates (a point: no feet offset), moved by the terrain's rounding in that
		// column so the block stands on the game's real ground, as the passthrough's view lift shows it.
		const double s = a_world.blocksPerMetre;
		const float  x = static_cast<float>((a_block.x + 0.5) / s), y = static_cast<float>(-(a_block.z + 0.5) / s);
		double       floor = (a_block.y - a_world.yOffset) / s;
		GroundProbe  ground;
		if (a_game.ProbeGround(x, y, ground) && ground.hit) {
			const double terrainTop = (TerrainScanner::HeightToBlockY(ground.groundZ, a_world) + 1.0 - a_world.yOffset) / s;
			const double shift = ground.groundZ - terrainTop;
			if (std::fabs(shift) <= kMaxGroundShift) {
				floor += shift;
			}
		}
		a_block.handle = a_game.SpawnBlockProp(x, y, static_cast<float>(floor), static_cast<float>(1.0 / s));
		if (a_block.handle == 0) {
			return false;
		}
		++live_;
		return true;
	}

	bool BlockProps::RegionAsked(std::int64_t a_key) const
	{
		for (int i = 0; i < regionCount_; ++i) {
			if (regions_[i] == a_key) {
				return true;
			}
		}
		return false;
	}

	void BlockProps::Tick(IGame& a_game, Endpoint& a_link, const WorldConfig& a_world, float a_px, float a_py)
	{
		++tick_;
		const double s = a_world.blocksPerMetre;
		const std::int32_t pcx = static_cast<std::int32_t>(std::floor(a_px * s / kChunkSize));
		const std::int32_t pcz = static_cast<std::int32_t>(std::floor(-a_py * s / kChunkSize));

		// Ask for the chunks around the player that haven't been asked for (nearest ring first).
		if (tick_ % kRegionEvery == 0) {
			int asked = 0;
			for (int r = 0; r <= config_.regionRadius && asked < config_.regionsPerTick; ++r) {
				for (int dz = -r; dz <= r && asked < config_.regionsPerTick; ++dz) {
					for (int dx = -r; dx <= r && asked < config_.regionsPerTick; ++dx) {
						if (std::abs(dx) != r && std::abs(dz) != r) {
							continue;  // only this ring
						}
						const std::int64_t key = RegionKey(pcx + dx, pcz + dz);
						if (RegionAsked(key) || regionCount_ >= kMaxRegions) {
							continue;
						}
						BlockRegionRequestMsg m{ pcx + dx, pcz + dz, ++nextRequestId_ == 0 ? ++nextRequestId_ : nextRequestId_, 0 };
						if (!a_link.Send(m)) {
							return;
						}
						regions_[regionCount_++] = key;
						++regionsAsked_;
						++asked;
					}
				}
			}
		}

		// Forget blocks (and the chunks they came from) far behind: they're asked for again on the way back.
		if (tick_ % kEvictEvery == 0) {
			const double evict2 = static_cast<double>(config_.forgetRadius) * config_.forgetRadius;
			for (int i = 0; i < kCapacity; ++i) {
				if (blocks_[i].state == 1 && Distance2(a_world, a_px, a_py, blocks_[i].x, blocks_[i].z) > evict2) {
					Remove(i, a_game);
				}
			}
			const int keepChunks = static_cast<int>(config_.forgetRadius * s / kChunkSize) + 1;
			for (int i = 0; i < regionCount_;) {
				const std::int32_t cx = static_cast<std::int32_t>(regions_[i] >> 32), cz = static_cast<std::int32_t>(regions_[i] & 0xFFFFFFFF);
				if (std::abs(cx - pcx) > keepChunks || std::abs(cz - pcz) > keepChunks) {
					regions_[i] = regions_[--regionCount_];
				} else {
					++i;
				}
			}
		}

		if (known_ == 0) {
			return;
		}
		// One pass: despawn far props, and keep the few nearest blocks that have none.
		const double spawn2 = static_cast<double>(config_.spawnRadius) * config_.spawnRadius;
		const double despawn2 = static_cast<double>(config_.despawnRadius) * config_.despawnRadius;
		constexpr int kMaxCandidates = 32;
		int          candidates[kMaxCandidates];
		double       candidate2[kMaxCandidates];
		const int    want = config_.spawnsPerTick < kMaxCandidates ? config_.spawnsPerTick : kMaxCandidates;
		int          count = 0;
		for (int i = 0; i < kCapacity; ++i) {
			Block& b = blocks_[i];
			if (b.state != 1) {
				continue;
			}
			const double d2 = Distance2(a_world, a_px, a_py, b.x, b.z);
			if (b.handle != 0) {
				if (d2 > despawn2) {
					a_game.DeleteBlockProp(b.handle);
					b.handle = 0;
					--live_;
				}
				continue;
			}
			if (d2 > spawn2 || (count == want && d2 >= candidate2[count - 1])) {
				continue;
			}
			int at = count < want ? count++ : want - 1;  // insert sorted, nearest first
			while (at > 0 && candidate2[at - 1] > d2) {
				candidates[at] = candidates[at - 1];
				candidate2[at] = candidate2[at - 1];
				--at;
			}
			candidates[at] = i;
			candidate2[at] = d2;
		}
		for (int k = 0; k < count; ++k) {
			if (live_ >= config_.maxProps) {
				if (capHits_++ % 600 == 0) {
					HostLog::Warn("blocks: %d solid blocks live, the most GTA takes safely; farther ones have no collision", live_);
				}
				break;
			}
			if (!a_game.BlockPropReady() || !Spawn(blocks_[candidates[k]], a_game, a_world)) {
				break;  // the model is still streaming in, or the game has no room: next frame
			}
		}
	}

	// Deleted slots slow every lookup down: rebuild the table without them (rare: after many breaks).
	void BlockProps::Rehash()
	{
		int n = 0;
		for (const Block& b : blocks_) {
			if (b.state == 1) {
				scratch_[n++] = b;
			}
		}
		for (Block& b : blocks_) {
			b = Block{};
		}
		for (int k = 0; k < n; ++k) {
			const Block& s = scratch_[k];
			for (std::uint32_t i = Hash(s.x, s.y, s.z);; ++i) {
				if (blocks_[i & kMask].state == 0) {
					blocks_[i & kMask] = s;
					break;
				}
			}
		}
		tombstones_ = 0;
	}

	void BlockProps::Clear(IGame& a_game)
	{
		for (Block& b : blocks_) {
			if (b.state == 1 && b.handle != 0) {
				a_game.DeleteBlockProp(b.handle);
			}
			b = Block{};
		}
		known_ = tombstones_ = live_ = 0;
		regionCount_ = 0;
	}
}
