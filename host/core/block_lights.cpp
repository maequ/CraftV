#include "block_lights.h"

#include "coords.h"

namespace craftv::host
{
	namespace
	{
		constexpr Rgba  kTorch{ 255, 190, 110, 255 };  // a torch's warm orange
		constexpr float kRange = 9.0f;                 // metres: about a torch's reach in Minecraft (light 14)
		constexpr float kIntensity = 4.0f;
		constexpr float kForgetRadius = 200.0f;
	}

	void BlockLights::OnBlockSet(const proto::BlockSetMsg& a_msg)
	{
		for (int i = 0; i < count_; ++i) {
			if (lights_[i].x == a_msg.x && lights_[i].y == a_msg.y && lights_[i].z == a_msg.z) {
				if ((a_msg.flags & proto::kBlockSetLight) == 0) {
					lights_[i] = lights_[--count_];
				}
				return;
			}
		}
		if ((a_msg.flags & proto::kBlockSetLight) != 0 && count_ < kCapacity) {
			lights_[count_++] = Light{ a_msg.x, a_msg.y, a_msg.z };
		}
	}

	void BlockLights::Draw(IGame& a_game, const WorldConfig& a_world, float a_playerX, float a_playerY, float a_playerZ, float a_radius)
	{
		int   best[kPerFrame];
		float bestD[kPerFrame];
		int   n = 0;
		for (int i = 0; i < count_;) {
			double x, y, z;
			PointFromMinecraft(lights_[i].x + 0.5, lights_[i].y + 0.5, lights_[i].z + 0.5, a_world, x, y, z);
			const float dx = static_cast<float>(x) - a_playerX, dy = static_cast<float>(y) - a_playerY, dz = static_cast<float>(z) - a_playerZ;
			const float d = dx * dx + dy * dy + dz * dz;
			if (d > kForgetRadius * kForgetRadius) {
				lights_[i] = lights_[--count_];  // far away: Minecraft sends it again on the way back (BLOCK_REGION_REQUEST)
				continue;
			}
			if (d <= a_radius * a_radius) {
				// keep the kPerFrame nearest (insertion into a small sorted list)
				int at = n < kPerFrame ? n++ : kPerFrame;
				while (at > 0 && bestD[at - 1] > d) {
					if (at < kPerFrame) {
						best[at] = best[at - 1];
						bestD[at] = bestD[at - 1];
					}
					--at;
				}
				if (at < kPerFrame) {
					best[at] = i;
					bestD[at] = d;
				}
			}
			++i;
		}
		for (int k = 0; k < n; ++k) {
			double x, y, z;
			const Light& l = lights_[best[k]];
			PointFromMinecraft(l.x + 0.5, l.y + 0.5, l.z + 0.5, a_world, x, y, z);
			a_game.DrawLight(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), kTorch, kRange, kIntensity);
		}
	}
}
