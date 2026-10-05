// IGame implemented with ScriptHookRDR2 natives. Script thread only.
#pragma once

#include "core/game_api.h"

namespace craftv::host
{
	class Rdr2Game final : public IGame
	{
	public:
		void Sample(GameSample& a_out) override;
		void DrawLabel(float a_x, float a_y, float a_scale, Rgba a_color, const char* a_text) override;
		void DrawBox(float a_x, float a_y, float a_w, float a_h, Rgba a_color) override;
		// The RDR2 plugin is parked (DECISIONS D-014): no terrain scanning, so friends get no ground from it.
		bool ProbeGround(float, float, GroundProbe& a_out) override
		{
			a_out = GroundProbe{};
			return false;
		}
		void RequestCollision(float, float, float) override {}
	};
}
