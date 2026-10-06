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

		// the passthrough (brief §8)
		bool PassthroughAvailable() override;
		void SampleCamera(CameraSample& a_out) override;
		void TakePassthroughInput(PassthroughInput& a_out) override;
		void SetPlayerHidden(bool a_hidden) override;
		void HideHudThisFrame() override;
		bool ScreenSize(int& a_width, int& a_height) override;
		void SetCompositorActive(bool a_active) override;
		void CompositorPose(float a_yaw, float a_pitch, float a_roll, float a_fovY, double a_x, double a_y, double a_z, float a_nearClip,
			float a_farClip) override;
		void Melee(float a_damage) override;
		void Notify(const char* a_text) override;

	private:
		void Count(bool a_ready, bool a_hit, std::uint32_t a_material);

		struct ProbeStats
		{
			std::uint64_t probes = 0, ready = 0, hits = 0, withMaterial = 0;
		};

		ProbeStats stats_{};
		bool       playerHidden_ = false;
	};
}
