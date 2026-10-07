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
		void TakeScreenInput(PassthroughInput& a_out) override;
		void SetPlayerHidden(bool a_hidden) override;
		void HideHudThisFrame() override;
		bool ScreenSize(int& a_width, int& a_height) override;
		void SetCompositorActive(bool a_active) override;
		void CompositorPose(float a_yaw, float a_pitch, float a_roll, float a_fovY, double a_x, double a_y, double a_z, float a_nearClip,
			float a_farClip) override;
		void Melee(float a_damage) override;
		void SetGun(int a_index) override;
		void SetCrouch(bool a_crouching) override;
		void TickFly(bool a_on) override;
		void SetInvincible(bool a_on) override;
		int  Shoot(float a_damage) override;
		void SetCompositorMask(float a_x0, float a_y0, float a_x1, float a_y1) override;
		void SetCompositorTranslation(bool a_on) override;
		void Notify(const char* a_text) override;
		void TickJump(bool a_minecraft, float a_heightMetres) override;
		void SetPlayerHealth(int a_health) override;
		void KillPlayer() override;
		void Explode(float a_x, float a_y, float a_z, float a_power) override;
		int  ProjectileHit(float a_x, float a_y, float a_z, float a_damage) override;
		void DrawLight(float a_x, float a_y, float a_z, Rgba a_color, float a_range, float a_intensity) override;

		// solid blocks (Phase 4): invisible, frozen collision boxes
		bool BlockPropReady() override;
		int  SpawnBlockProp(float a_x, float a_y, float a_floorZ, float a_size) override;
		void DeleteBlockProp(int a_handle) override;
		bool BlockSpaceFree(float a_x, float a_y, float a_floorZ, float a_size) override;

		// the settings menu (F8), drawn with the game's own fonts, textures and sounds
		void  DrawMenuText(float a_x, float a_y, const TextStyle& a_style, const char* a_text) override;
		float TextWidth(const TextStyle& a_style, const char* a_text) override;
		void  DrawSprite(const char* a_dict, const char* a_name, float a_x, float a_y, float a_w, float a_h, Rgba a_color) override;
		bool  SpritesReady(const char* a_dict) override;
		void  PlayMenuSound(const char* a_name) override;
		void  TakeMenuInput(MenuInput& a_out) override;

		// Phase 4 research, once per session: logs the size of candidate box props (CraftV.log), one model at a time.
		void MeasurePropCandidates();

	private:
		void Count(bool a_ready, bool a_hit, std::uint32_t a_material);

		struct RayHit
		{
			bool  hit = false;
			float x = 0, y = 0, z = 0;  // where it hit
			float dx = 0, dy = 0;       // the ray's direction on the ground plane
			int   ped = 0, vehicle = 0;  // what it hit, if a person or a car
		};
		RayHit CrosshairRay(float a_range);
		void   HurtPed(int a_ped, float a_damage, float a_fx, float a_fy, bool a_arrow);
		void   HurtVehicle(int a_vehicle, float a_damage, float a_x, float a_y, float a_z, float a_fx, float a_fy);

		struct ProbeStats
		{
			std::uint64_t probes = 0, ready = 0, hits = 0, withMaterial = 0;
		};

		ProbeStats stats_{};
		bool       playerHidden_ = false;
		int        jumpFrames_ = 0;  // frames left pushing the player up after a Minecraft jump
		int        jumpsLogged_ = 0;
		int        groundedFrames_ = 0;  // frames on the ground in a row (a jump needs a few)
		int        gun_ = -1;            // the game's gun in hand for CraftV's gun item, -1 none
		bool       gunGiven_ = false;    // CraftV gave that gun (taken away again when put down)
		bool       crouching_ = false;
		bool       flying_ = false;
		bool       screenLeft_ = false, screenRight_ = false;  // mouse buttons last frame, in a Minecraft screen
		int        propCandidate_ = 0;
		int        propWaitFrames_ = 0;
	};
}
