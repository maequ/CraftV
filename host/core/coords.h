// RDR2 <-> Minecraft coordinate conversion: the only place it happens (PORTING-GUIDE §4,
// PROTOCOL.md §1). The protocol always speaks Minecraft space.
//
// ASSUMPTION: RDR2 uses the same RAGE conventions as GTA V: metres, X east, Y north, Z up, heading
// 0 = north increasing counter-clockwise. Verify in Phase 2 by walking north in game and watching
// Minecraft's Z decrease (Minecraft north is -Z).
#pragma once

#include <algorithm>
#include <cmath>

namespace craftv::host
{
	struct WorldConfig
	{
		double blocksPerMetre = 1.0;  // 1 block = 1 m (Minecraft player 1.8 blocks, RDR2 adults ~1.8 m)
		double feetOffset = 1.0;      // metres from the ped's root (GET_ENTITY_COORDS) down to its feet. ASSUMPTION: measured in Phase 2
		double yOffset = 0.0;         // added to Minecraft Y after scaling (fit the world into the mirror dimension)
	};

	struct McPosition
	{
		double x, y, z;
	};

	// RDR2 (x east, y north, z up) -> Minecraft (x east, y up, z south).
	inline McPosition ToMinecraft(double a_x, double a_y, double a_z, const WorldConfig& a_cfg)
	{
		const double s = a_cfg.blocksPerMetre;
		return { a_x * s, (a_z - a_cfg.feetOffset) * s + a_cfg.yOffset, -a_y * s };
	}

	// A point that isn't the player (the camera): no feet offset.
	inline McPosition PointToMinecraft(double a_x, double a_y, double a_z, const WorldConfig& a_cfg)
	{
		const double s = a_cfg.blocksPerMetre;
		return { a_x * s, a_z * s + a_cfg.yOffset, -a_y * s };
	}

	// Velocity has no offsets: (vx, vy, vz) metres/s -> blocks/s in Minecraft axes.
	inline McPosition VelocityToMinecraft(double a_vx, double a_vy, double a_vz, const WorldConfig& a_cfg)
	{
		const double s = a_cfg.blocksPerMetre;
		return { a_vx * s, a_vz * s, -a_vy * s };
	}

	// Minecraft -> RDR2 (feet position back to the ped root). Used from Phase 3/4 on.
	inline void FromMinecraft(const McPosition& a_mc, const WorldConfig& a_cfg, double& a_x, double& a_y, double& a_z)
	{
		const double s = a_cfg.blocksPerMetre;
		a_x = a_mc.x / s;
		a_y = -a_mc.z / s;
		a_z = (a_mc.y - a_cfg.yOffset) / s + a_cfg.feetOffset;
	}

	// Wraps degrees into [-180, 180).
	inline float WrapDegrees(float a_deg)
	{
		float d = std::fmod(a_deg + 180.0f, 360.0f);
		if (d < 0) {
			d += 360.0f;
		}
		return d - 180.0f;
	}

	// RAGE heading (0 = +Y north, counter-clockwise) -> Minecraft yaw (0 = +Z south, 90 = -X west).
	// Facing (-sin h, cos h) in RDR2 x/y is (-sin h, -cos h) in Minecraft x/z, and Minecraft looks
	// along (-sin yaw, cos yaw), so yaw = 180 - h. (The GTA V passthrough project uses the same formula.)
	inline float HeadingToYaw(float a_heading)
	{
		return WrapDegrees(180.0f - a_heading);
	}

	inline float YawToHeading(float a_yaw)
	{
		float h = std::fmod(180.0f - a_yaw, 360.0f);
		return h < 0 ? h + 360.0f : h;
	}

	// RAGE camera pitch (positive up) -> Minecraft pitch (positive down), clamped to Minecraft's range.
	inline float CamPitchToMcPitch(float a_camPitch)
	{
		return std::clamp(-a_camPitch, -90.0f, 90.0f);
	}
}
