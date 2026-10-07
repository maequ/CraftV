// Minecraft blocks that give off light (torches, lanterns, glowstone, lit lamps: BLOCK_SET LIGHT, PROTOCOL.md §7.20)
// light up the game around them too: every frame the nearest few get a game light. Fixed-size table, no heap.
#pragma once

#include "config.h"
#include "game_api.h"

#include "craftv/protocol.h"

#include <cstdint>

namespace craftv::host
{
	class BlockLights
	{
	public:
		static constexpr int kCapacity = 2048;  // known lights
		static constexpr int kPerFrame = 16;    // drawn each frame, nearest first

		// Adds a light for a BLOCK_SET with LIGHT; any other BLOCK_SET at that block removes it.
		void OnBlockSet(const proto::BlockSetMsg& a_msg);
		// Draws the nearest lights within a_radius metres of the player (game coordinates); forgets far ones.
		void Draw(IGame& a_game, const WorldConfig& a_world, float a_playerX, float a_playerY, float a_playerZ, float a_radius);
		void Clear() { count_ = 0; }
		int  Count() const { return count_; }

	private:
		struct Light
		{
			std::int32_t x, y, z;
		};
		Light lights_[kCapacity];
		int   count_ = 0;
	};
}
