// IGame implemented with Script Hook V natives. Script thread only.
#pragma once

#include "core/game_api.h"

#include <cstdint>

namespace craftv::host
{
	class GtaGame final : public IGame
	{
	public:
		void Sample(GameSample& a_out) override;
		void DrawLabel(float a_x, float a_y, float a_scale, Rgba a_color, const char* a_text) override;
		void DrawBox(float a_x, float a_y, float a_w, float a_h, Rgba a_color) override;
		bool ProbeGround(float a_x, float a_y, GroundProbe& a_out) override;
		void RequestCollision(float a_x, float a_y, float a_z) override;

	private:
		void Count(bool a_ready, bool a_hit, std::uint32_t a_material);

		struct ProbeStats
		{
			std::uint64_t probes = 0, ready = 0, hits = 0, withMaterial = 0;
		};
		static constexpr int kUnknownSlots = 32;  // distinct unknown surface hashes logged per session

		ProbeStats    stats_{};
		std::uint32_t unknown_[kUnknownSlots]{};
		int           unknownCount_ = 0;
	};
}
