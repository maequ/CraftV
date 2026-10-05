#include "terrain_scanner.h"

#include "host_log.h"
#include "materials.h"

#include "craftv/codec.h"

#include <cmath>

namespace craftv::host
{
	using namespace craftv::proto;

	namespace
	{
		constexpr int kChunkSize = 16;
		constexpr int kColumns = static_cast<int>(kChunkColumns);
	}

	void TerrainScanner::ColumnToGame(std::int32_t a_mcX, std::int32_t a_mcZ, const WorldConfig& a_world, float& a_x, float& a_y)
	{
		// Minecraft column (x, z) covers game x in [x, x+1) and y in (-z-1, -z] at 1 block = 1 m: probe its centre.
		const double s = a_world.blocksPerMetre;
		a_x = static_cast<float>((a_mcX + 0.5) / s);
		a_y = static_cast<float>(-(a_mcZ + 0.5) / s);
	}

	std::int16_t TerrainScanner::HeightToBlockY(float a_z, const WorldConfig& a_world)
	{
		// The surface is where feet stand (PLAYER_STATE uses the same y = z * s + yOffset for the feet), so the
		// top block is the one whose top face is nearest it. Same rounding as the GTA V reference project.
		const double feetY = static_cast<double>(a_z) * a_world.blocksPerMetre + a_world.yOffset;
		const double top = std::floor(feetY + 0.5) - 1.0;
		const double clamped = top < kMinY ? kMinY : (top > kMaxY ? kMaxY : top);
		return static_cast<std::int16_t>(clamped);
	}

	void TerrainScanner::Enqueue(const TerrainRequestMsg& a_request)
	{
		if ((scanning_ && current_.chunkX == a_request.chunkX && current_.chunkZ == a_request.chunkZ) || Contains(a_request.chunkX, a_request.chunkZ)) {
			return;
		}
		if (count_ == kQueueCapacity) {
			++stats_.dropped;  // Minecraft asks again after TERRAIN_RETRY_MS
			return;
		}
		queue_[(head_ + count_) % kQueueCapacity] = a_request;
		++count_;
	}

	bool TerrainScanner::Pop(TerrainRequestMsg& a_out)
	{
		if (count_ == 0) {
			return false;
		}
		a_out = queue_[head_];
		head_ = (head_ + 1) % kQueueCapacity;
		--count_;
		return true;
	}

	bool TerrainScanner::Contains(std::int32_t a_chunkX, std::int32_t a_chunkZ) const
	{
		for (int i = 0; i < count_; ++i) {
			const auto& q = queue_[(head_ + i) % kQueueCapacity];
			if (q.chunkX == a_chunkX && q.chunkZ == a_chunkZ) {
				return true;
			}
		}
		return false;
	}

	bool TerrainScanner::InsideMap(float a_x, float a_y) const
	{
		return a_x >= config_.mapMinX && a_x <= config_.mapMaxX && a_y >= config_.mapMinY && a_y <= config_.mapMaxY;
	}

	int TerrainScanner::Attempt(std::int32_t a_chunkX, std::int32_t a_chunkZ)
	{
		for (auto& a : attempts_) {
			if (a.used && a.chunkX == a_chunkX && a.chunkZ == a_chunkZ) {
				return ++a.misses;
			}
		}
		AttemptSlot& slot = attempts_[nextAttemptSlot_];  // oldest slot is reused: worst case a chunk gets a few extra tries
		nextAttemptSlot_ = (nextAttemptSlot_ + 1) % kAttemptSlots;
		slot = { true, a_chunkX, a_chunkZ, 1 };
		return 1;
	}

	void TerrainScanner::ForgetAttempts(std::int32_t a_chunkX, std::int32_t a_chunkZ)
	{
		for (auto& a : attempts_) {
			if (a.used && a.chunkX == a_chunkX && a.chunkZ == a_chunkZ) {
				a = AttemptSlot{};
			}
		}
	}

	void TerrainScanner::FillEmpty()
	{
		for (int i = 0; i < kColumns; ++i) {
			patch_.groundY[i] = kNoGround;
			patch_.waterY[i] = kNoWater;
			patch_.material[i] = kMatUnknown;
		}
	}

	const TerrainPatchMsg* TerrainScanner::Tick(IGame& a_game, const WorldConfig& a_world, float a_playerX, float a_playerY, float a_playerZ)
	{
		if (!scanning_) {
			if (!Pop(current_)) {
				return nullptr;
			}
			patch_ = TerrainPatchMsg{};
			patch_.chunkX = current_.chunkX;
			patch_.chunkZ = current_.chunkZ;
			patch_.requestId = current_.requestId;
			float cx = 0, cy = 0;
			ColumnToGame(current_.chunkX * kChunkSize + kChunkSize / 2, current_.chunkZ * kChunkSize + kChunkSize / 2, a_world, cx, cy);
			if (!InsideMap(cx, cy)) {
				FillEmpty();
				++stats_.empty;
				return &patch_;
			}
			const float dx = cx - a_playerX, dy = cy - a_playerY;
			waitTicks_ = 0;
			if (dx * dx + dy * dy > config_.nearDistance * config_.nearDistance) {
				a_game.RequestCollision(cx, cy, a_playerZ);  // ASSUMPTION: streams that area's collision within a few frames
				waitTicks_ = config_.collisionWaitTicks;
			}
			scanning_ = true;
			column_ = 0;
			hits_ = 0;
		}
		if (waitTicks_ > 0) {
			--waitTicks_;
			return nullptr;
		}

		const int end = column_ + config_.probesPerTick < kColumns ? column_ + config_.probesPerTick : kColumns;
		for (; column_ < end; ++column_) {
			const int lx = column_ % kChunkSize, lz = column_ / kChunkSize;
			const std::uint32_t i = codec::TerrainColumn(static_cast<std::uint32_t>(lx), static_cast<std::uint32_t>(lz));
			float x = 0, y = 0;
			ColumnToGame(current_.chunkX * kChunkSize + lx, current_.chunkZ * kChunkSize + lz, a_world, x, y);
			GroundProbe g;
			++stats_.probes;
			if (!a_game.ProbeGround(x, y, g)) {
				patch_.groundY[i] = kNoGround;
				patch_.waterY[i] = kNoWater;
				patch_.material[i] = kMatUnknown;
				continue;
			}
			++hits_;
			patch_.groundY[i] = HeightToBlockY(g.groundZ, a_world);
			patch_.waterY[i] = g.water && g.waterZ > g.groundZ ? HeightToBlockY(g.waterZ, a_world) : kNoWater;
			patch_.material[i] = TerrainMaterialFor(g.materialHash);
			stats_.lastMaterialHash = g.materialHash;
			if (patch_.material[i] == kMatUnknown && g.materialHash != 0) {
				++stats_.unknownMaterials;
				bool logged = false;  // log each one once, so the material table can grow
				for (int k = 0; k < unknownLoggedCount_ && !logged; ++k) {
					logged = unknownLogged_[k] == g.materialHash;
				}
				if (!logged && unknownLoggedCount_ < kUnknownLogged) {
					unknownLogged_[unknownLoggedCount_++] = g.materialHash;
					HostLog::Info("terrain: unknown surface material hash 0x%08X at %.1f, %.1f (shown as stone)", g.materialHash, x, y);
				}
			}
		}
		if (column_ < kColumns) {
			return nullptr;
		}

		scanning_ = false;
		if (hits_ == 0) {
			if (Attempt(current_.chunkX, current_.chunkZ) < config_.maxAttempts) {
				++stats_.deferred;  // collision probably not streamed in yet: Minecraft asks again
				return nullptr;
			}
			ForgetAttempts(current_.chunkX, current_.chunkZ);
			++stats_.empty;
			return &patch_;  // every column is already NO_GROUND
		}
		ForgetAttempts(current_.chunkX, current_.chunkZ);
		++stats_.served;
		return &patch_;
	}

	void TerrainScanner::Reset()
	{
		head_ = 0;
		count_ = 0;
		scanning_ = false;
		column_ = 0;
		waitTicks_ = 0;
	}
}
