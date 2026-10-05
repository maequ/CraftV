// IGame implemented with ScriptHookRDR2 natives. Script thread only.
#pragma once

#include "../core/game_api.h"

namespace craftv::host
{
	class Rdr2Game final : public IGame
	{
	public:
		void Sample(GameSample& a_out) override;
		void DrawLabel(float a_x, float a_y, float a_scale, Rgba a_color, const char* a_text) override;
		void DrawBox(float a_x, float a_y, float a_w, float a_h, Rgba a_color) override;
	};
}
