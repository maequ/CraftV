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
		void StartDiagnosticRay(float a_x, float a_y);
		void PollDiagnosticRays();

		struct ProbeStats
		{
			std::uint64_t probes = 0, syncReady = 0, syncHits = 0, withMaterial = 0, fallbacks = 0;
		};
		struct DiagnosticRay
		{
			bool  pending = false;
			int   handle = 0;
			int   polls = 0;
			float x = 0, y = 0;
		};
		ProbeStats    stats_{};
		DiagnosticRay diagnostic_[8]{};
	};
}
