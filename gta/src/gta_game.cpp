// Every native used here was checked against the Script Hook V SDK's natives.h (v1.0.617.1a): name,
// namespace, hash and signature. The 2016 SDK predates some modern native names; those are noted.
// Text drawing copies the SDK sample (NativeTrainer/script.cpp).
#include "gta_game.h"

#include "compositor.h"
#include "core/host_log.h"
#include "core/materials.h"

#pragma warning(push, 0)
#include "natives.h"
#pragma warning(pop)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cmath>
#include <cstring>

namespace craftv::host
{
	namespace
	{
		constexpr int kRotationOrderZxy = 2;  // the rotation order RAGE scripts use for the gameplay camera

		// Ground probes go straight down from above the tallest point of the map to below the sea floor.
		constexpr float kProbeTopZ = 1200.0f;     // Mount Chiliad is ~800 m, the Maze Bank tower ~300 m
		constexpr float kProbeBottomZ = -250.0f;  // deepest sea floor is about -200 m
		// ASSUMPTION: shape-test flag 1 = map collision (terrain, roads, buildings), without props, vehicles
		// or peds; options 7 as RAGE scripts and the GTA V reference project pass.
		constexpr int kShapeTestMap = 1;
		constexpr int kShapeTestOptions = 7;
		constexpr int kShapeTestReady = 2;  // GET_SHAPE_TEST_RESULT status: 0 failed, 1 pending, 2 ready

		// 0x377906D8A31E5586 (_CAST_RAY_POINT_TO_POINT in the 2016 SDK, START_EXPENSIVE_SYNCHRONOUS_SHAPE_TEST_LOS_PROBE
		// today) answers at once: in the first in-game run its result was ready on the first poll, with hit, height
		// and material. 0x7EE9F5D83DD4F90E, which I had assumed was the synchronous one, stayed pending (it's the
		// asynchronous START_SHAPE_TEST_LOS_PROBE). 0x65287525D951F6BE (_GET_RAYCAST_RESULT_2) is
		// GET_SHAPE_TEST_RESULT_INCLUDING_MATERIAL. If a result still isn't ready, ProbeGround falls back to
		// GET_GROUND_Z_FOR_3D_COORD (height only).
		int StartSynchronousRay(float a_x1, float a_y1, float a_z1, float a_x2, float a_y2, float a_z2)
		{
			return WORLDPROBE::_CAST_RAY_POINT_TO_POINT(a_x1, a_y1, a_z1, a_x2, a_y2, a_z2, kShapeTestMap, 0, kShapeTestOptions);
		}

		constexpr std::uint64_t kProbeSummaryEvery = 65536;  // a probe summary line in CraftV.log every this many probes

		// ---- the passthrough (brief §8). Control ids, hashes and argument counts follow minecraft-gta5-passthrough
		// (rehan-remade, MIT), proven on GTA V Legacy 3889; the 2016 SDK has older names or none for some of them.
		constexpr int kFollowCamFirstPerson = 4;  // GET_FOLLOW_PED_CAM_VIEW_MODE / GET_FOLLOW_VEHICLE_CAM_VIEW_MODE
		constexpr Hash kWeaponUnarmed = 0xA2719263;
		// GTA's own attack, aim, melee, weapon wheel, weapon slots, cover, reload, grenade and vehicle weapons: off
		// while Minecraft has the mouse.
		constexpr int kDisabledControls[] = {
			24, 25, 257, 140, 141, 142, 143, 263, 264,    // attack, aim, attack 2, melee
			14, 15, 16, 17, 37, 261, 262,                 // weapon wheel, next, previous
			157, 158, 159, 160, 161, 162, 163, 164, 165,  // weapon slots (the number keys)
			44, 45, 47, 58,                               // cover, reload, detonate, throw grenade
			68, 69, 70, 91, 92, 99, 100, 114, 115, 116,   // vehicle and passenger weapons
		};
		constexpr int kControlAttack = 24, kControlAim = 25;
		// With one of CraftV's gun items in hand these stay on: the game's own aim, fire, melee and reload.
		constexpr int kGunControls[] = { 24, 25, 257, 140, 141, 142, 143, 263, 264, 45 };
		// CraftV's gun items, in the order of the Minecraft side's table (PROTOCOL.md §7.18, held 16+).
		constexpr Hash kGuns[] = {
			0x1B06D571,  // WEAPON_PISTOL
			0x2BE6766B,  // WEAPON_SMG
			0xBFEFFF6D,  // WEAPON_ASSAULTRIFLE
			0x1D073A89,  // WEAPON_PUMPSHOTGUN
			0x05FC3C11,  // WEAPON_SNIPERRIFLE
			0xB1CA77B1,  // WEAPON_RPG
			0x42BF8A85,  // WEAPON_MINIGUN
			0x93E220BD,  // WEAPON_GRENADE
		};
		constexpr int kGunAmmo = 9999;
		constexpr int kControlDuck = 36;                // Left Ctrl: GTA's stealth, Minecraft's sneak here
		constexpr int kControlSprint = 21;              // Shift: faster flying
		constexpr int kControlMoveLr = 30, kControlMoveUd = 31;
		char          g_stealthAction[] = "DEFAULT_ACTION";
		char          g_phoneScript[] = "cellphone_flashhand";  // runs while the phone is out
		constexpr float kFlySpeed = 12.0f, kFlyFastSpeed = 40.0f;  // metres per second
		// Shape tests that hit people and cars: map, vehicles, peds (both kinds), objects.
		constexpr int   kRayEverything = 1 | 2 | 4 | 8 | 16;
		constexpr float kShotRange = 250.0f;
		constexpr float kDegToRadF = 3.14159265f / 180.0f;
		constexpr int kControlJump = 22;  // Space: GTA's jump (and climb), replaced by Minecraft's straight jump
		constexpr float kGravity = 9.81f;
		constexpr int   kJumpPushFrames = 12;   // frames a jump lasts before the player may land and jump again
		constexpr int   kJumpBoostFrames = 5;   // the first frames of it keep pushing up (the ground task eats a single push)
		constexpr int   kJumpGroundFrames = 3;  // frames on the ground before the next jump: no jumping in the air
		constexpr float kJumpLift = 0.08f;      // metres: lifts the player off the ground so the push isn't cancelled
		// Minecraft arrows: people this close to an arrow's path are hit (metres, around the ped's middle).
		constexpr float kArrowReach = 0.9f, kArrowHeight = 1.1f;
		// ADD_EXPLOSION's types (eExplosionType): 0 grenade, 2 sticky bomb, 4 rocket. TNT (power 4) is a sticky bomb.
		constexpr int kExplosionSticky = 2;
		void AddExplosion(float a_x, float a_y, float a_z, int a_type, float a_scale, float a_shake)
		{
			// ASSUMPTION: the modern signature (alloc8or's nativedb) ends with noDamage; the 2016 SDK stops before it.
			invoke<Void>(0xE3AD2BDBAEE269AC, a_x, a_y, a_z, a_type, a_scale, TRUE, FALSE, a_shake, FALSE);
		}
		// While Minecraft's inventory is open: the cursor and its buttons (INPUT_CURSOR_X/Y, ACCEPT, CANCEL), Esc.
		constexpr int kCursorX = 239, kCursorY = 240, kCursorAccept = 237, kCursorCancel = 238, kPause = 199, kPauseAlt = 200;
		constexpr int kControlNext[] = { 14, 16 }, kControlPrevious[] = { 15, 17 };  // wheel down/up: the next/previous weapon
		// Number keys 1..9 are GTA's weapon-slot controls in this order.
		constexpr int kHotbarControls[9] = { 157, 158, 160, 164, 165, 159, 161, 162, 163 };
		// ASSUMPTION: modern signatures (alloc8or's nativedb, as the reference project calls them on 3889): the 2016 SDK
		// wraps these hashes with fewer or untyped arguments.
		float FinalRenderedCamFov() { return invoke<float>(0x80EC114669DAEFF4); }
		float FinalRenderedCamNearClip() { return invoke<float>(0xD0082607100D7193); }
		float FinalRenderedCamFarClip() { return invoke<float>(0xDFC8CBC606FDB0FC); }
		void  ApplyDamageToPed(Ped a_ped, int a_damage) { invoke<Void>(0x697157CED63F18D4, a_ped, a_damage, FALSE, 0, 0); }
		void  ApplyForce(Entity a_entity, float a_x, float a_y, float a_z)
		{
			ENTITY::APPLY_FORCE_TO_ENTITY(a_entity, 1, a_x, a_y, a_z, 0.0f, 0.0f, 0.0f, 0, FALSE, TRUE, TRUE, FALSE, TRUE);
		}
		// Melee reach and knock-back. ASSUMPTION: tuned by eye in the reference; to check in game.
		// Minecraft's reach is 3 blocks from the eyes: the crosshair's target must be this close to the player's middle.
		constexpr float kMeleeReach = 3.2f, kCarReach = 4.0f;
		constexpr float kStumbleDamage = 30.0f;  // weaker (uncharged, spammed) hits only hurt; no stumble
		// A hit stumbles the person back a little (Minecraft's knock-back); only a strong hit knocks them down. Then
		// most run, some fight back. Cars dent where they're hit and get shoved; their driver drives off.
		constexpr float kPushPerDamage = 0.06f, kPushUp = 1.5f, kCarPushPerDamage = 0.35f, kCarDentPerDamage = 6.0f;
		constexpr float kKnockDownDamage = 35.0f;  // a charged sword hit or more
		constexpr int   kStumbleMs = 350, kKnockDownMs = 2500;
		constexpr int   kFightBackPercent = 35;
		constexpr int   kMaxWorldEntities = 512;
		constexpr float kDegToRad = 3.14159265f / 180.0f;
		char            g_notificationEntry[] = "STRING";

		// Phase 4 (blocks solid in GTA): box props that might stand in for a 1 m block. Invalid names are skipped.
		constexpr const char* kPropCandidates[] = { "prop_box_wood01a", "prop_box_wood02a", "prop_box_wood03a", "prop_box_wood04a",
			"prop_box_wood05a", "prop_box_wood06a", "prop_box_wood07a", "prop_box_wood08a", "prop_crate_01a", "prop_crate_11e",
			"prop_mb_crate_01a", "prop_cs_cardbox_01", "prop_ld_crate_01", "prop_cons_crate", "prop_boxpile_07d", "prop_rub_boxpile_04" };
		constexpr int kPropLoadFrames = 120;

		// The settings menu: the frontend controls the game's own menus use (arrows, Enter, Backspace/Esc), turned off for
		// the game while the menu is open, with the phone, weapon and radio wheels and attacks.
		constexpr int kMenuUp = 172, kMenuDown = 173, kMenuLeft = 174, kMenuRight = 175, kMenuAccept = 201, kMenuSelect = 176;
		constexpr int kMenuBack = 202, kMenuPhoneBack = 177, kMenuPause = 199, kMenuPauseAlt = 200;
		constexpr int kMenuBlocked[] = { 27, 172, 173, 174, 175, 176, 177, 178, 199, 200, 201, 202, 19, 20, 37, 85, 24, 25, 140, 141, 142, 143, 257, 263, 264 };
		char g_soundSet[] = "HUD_FRONTEND_DEFAULT_SOUNDSET";

		// Solid blocks: the box closest to a 1 m block of the 16 measured in Sary's game (CraftV.log, 2026-10-06):
		// 0.965 x 0.965 x 0.795 m, its origin at its bottom. Its top is put level with the block's top (that's what
		// people stand on); it's 2 cm narrower each side and 20 cm short underneath.
		constexpr const char* kBlockPropModel = "prop_box_wood01a";
		constexpr float       kBlockPropHeight = 0.795f;

		// _ADD_TEXT_COMPONENT_STRING takes at most 99 characters; longer lines go in as several components.
		constexpr std::size_t kTextComponentChars = 90;
		constexpr int         kFontChaletLondon = 0;
		char                  g_textEntry[] = "STRING";
	}

	void GtaGame::Sample(GameSample& s)
	{
		s = GameSample{};
		s.networkGameInProgress = NETWORK::NETWORK_IS_GAME_IN_PROGRESS() != FALSE;
		s.networkSessionStarted = NETWORK::NETWORK_IS_SESSION_STARTED() != FALSE;
		s.networkInSession = NETWORK::NETWORK_IS_IN_SESSION() != FALSE;
		s.loadingScreen = DLC2::GET_IS_LOADING_SCREEN_ACTIVE() != FALSE;
		s.screenFadedOut = CAM::IS_SCREEN_FADED_OUT() != FALSE;

		const Ped ped = PLAYER::PLAYER_PED_ID();
		s.playerExists = ENTITY::DOES_ENTITY_EXIST(ped) != FALSE;
		if (!s.playerExists) {
			return;
		}
		s.playerDead = PLAYER::IS_PLAYER_DEAD(PLAYER::PLAYER_ID()) != FALSE;
		const Vector3 pos = ENTITY::GET_ENTITY_COORDS(ped, TRUE);  // the ped's root, about 1 m above its feet
		s.x = pos.x;
		s.y = pos.y;
		s.z = pos.z;
		s.heading = ENTITY::GET_ENTITY_HEADING(ped);
		const Vector3 vel = ENTITY::GET_ENTITY_VELOCITY(ped);  // world space, metres per second
		s.vx = vel.x;
		s.vy = vel.y;
		s.vz = vel.z;
		s.heightAboveGround = ENTITY::GET_ENTITY_HEIGHT_ABOVE_GROUND(ped);
		s.inAir = ENTITY::IS_ENTITY_IN_AIR(ped) != FALSE;
		s.falling = PED::IS_PED_FALLING(ped) != FALSE;
		s.onMount = PED::IS_PED_ON_ANY_BIKE(ped) != FALSE;
		s.inVehicle = PED::IS_PED_IN_ANY_VEHICLE(ped, FALSE) != FALSE;
		s.swimming = PED::IS_PED_SWIMMING(ped) != FALSE;
		s.sprinting = AI::IS_PED_SPRINTING(ped) != FALSE;
		// The phone task alone missed Sary's phone in game (2026-10-07): its script running is the other sign.
		s.phone = PED::IS_PED_RUNNING_MOBILE_PHONE_TASK(ped) != FALSE || SCRIPT::_GET_NUMBER_OF_INSTANCES_OF_STREAMED_SCRIPT(GAMEPLAY::GET_HASH_KEY(g_phoneScript)) > 0;
		s.parachute = PED::GET_PED_PARACHUTE_STATE(ped) >= 0;  // -1: no parachute
		s.ragdoll = PED::IS_PED_RAGDOLL(ped) != FALSE;
		s.health = ENTITY::GET_ENTITY_HEALTH(ped);
		s.maxHealth = ENTITY::GET_ENTITY_MAX_HEALTH(ped);
		s.camPitch = CAM::GET_GAMEPLAY_CAM_ROT(kRotationOrderZxy).x;
	}

	bool GtaGame::ProbeGround(float a_x, float a_y, GroundProbe& a_out)
	{
		a_out = GroundProbe{};
		BOOL    hit = FALSE;
		Vector3 end{}, normal{};
		Hash    material = 0;
		Entity  entity = 0;
		const int ray = StartSynchronousRay(a_x, a_y, kProbeTopZ, a_x, a_y, kProbeBottomZ);
		const int status = WORLDPROBE::_GET_RAYCAST_RESULT_2(ray, &hit, &end, &normal, &material, &entity);
		Count(status == kShapeTestReady, hit != FALSE, material);
		if (status == kShapeTestReady) {
			if (!hit) {
				return false;  // nothing there, or its collision isn't streamed in
			}
			a_out.groundZ = end.z;
			a_out.materialHash = material;
		} else {
			float z = 0.0f;
			if (!GAMEPLAY::GET_GROUND_Z_FOR_3D_COORD(a_x, a_y, kProbeTopZ, &z, FALSE)) {
				return false;
			}
			a_out.groundZ = z;
		}
		a_out.hit = true;
		float water = 0.0f;
		// ASSUMPTION: GET_WATER_HEIGHT_NO_WAVES reports the still water surface over (x, y) when z is at or below it.
		if (WATER::GET_WATER_HEIGHT_NO_WAVES(a_x, a_y, a_out.groundZ, &water) && water > a_out.groundZ) {
			a_out.water = true;
			a_out.waterZ = water;
		}
		return true;
	}

	// Counters for the probe summary line in CraftV.log.
	void GtaGame::Count(bool a_ready, bool a_hit, std::uint32_t a_material)
	{
		++stats_.probes;
		stats_.ready += a_ready ? 1 : 0;
		stats_.hits += a_ready && a_hit ? 1 : 0;
		stats_.withMaterial += a_ready && a_hit && a_material != 0 ? 1 : 0;
		if (stats_.probes % kProbeSummaryEvery == 0) {
			HostLog::Info("probes so far %llu: ready %llu, hit %llu, with material %llu, fell back to ground-Z %llu",
				static_cast<unsigned long long>(stats_.probes), static_cast<unsigned long long>(stats_.ready),
				static_cast<unsigned long long>(stats_.hits), static_cast<unsigned long long>(stats_.withMaterial),
				static_cast<unsigned long long>(stats_.probes - stats_.ready));
		}
	}

	bool GtaGame::PassthroughAvailable()
	{
		return compositor::registered();
	}

	void GtaGame::SampleCamera(CameraSample& a_out)
	{
		a_out = CameraSample{};
		// 0xA200EB1EE790F448 and 0x5B4E4C817FCC2DFB are GET_FINAL_RENDERED_CAM_COORD / _ROT (the SDK calls them
		// _GET_GAMEPLAY_CAM_COORDS / _ROT): the camera this frame is drawn with, whichever camera is active.
		const Vector3 p = CAM::_GET_GAMEPLAY_CAM_COORDS();
		const Vector3 r = CAM::_GET_GAMEPLAY_CAM_ROT(kRotationOrderZxy);
		a_out.x = p.x;
		a_out.y = p.y;
		a_out.z = p.z;
		a_out.pitch = r.x;
		a_out.roll = r.y;
		a_out.heading = r.z;
		a_out.fovY = FinalRenderedCamFov();
		a_out.nearClip = FinalRenderedCamNearClip();
		a_out.farClip = FinalRenderedCamFarClip();
		const Ped  ped = PLAYER::PLAYER_PED_ID();
		const bool inVehicle = PED::IS_PED_IN_ANY_VEHICLE(ped, FALSE) != FALSE;
		a_out.firstPerson = (inVehicle ? CAM::GET_FOLLOW_VEHICLE_CAM_VIEW_MODE() : CAM::GET_FOLLOW_PED_CAM_VIEW_MODE()) == kFollowCamFirstPerson;
		a_out.valid = std::isfinite(a_out.x) && std::isfinite(a_out.y) && std::isfinite(a_out.z) && std::isfinite(a_out.fovY) && a_out.fovY > 0.0f;
	}

	void GtaGame::TakePassthroughInput(PassthroughInput& a_out)
	{
		a_out = PassthroughInput{};
		const Ped ped = PLAYER::PLAYER_PED_ID();
		for (int control : kDisabledControls) {
			bool gunControl = false;
			for (int g : kGunControls) {
				gunControl = gunControl || (gun_ >= 0 && g == control);
			}
			if (!gunControl) {
				CONTROLS::DISABLE_CONTROL_ACTION(0, control, TRUE);
			}
		}
		if (gun_ >= 0) {
			// The game's gun of that kind, kept in hand (a respawn or character switch takes it away)
			const Hash want = kGuns[gun_];
			Hash       current = 0;
			WEAPON::GET_CURRENT_PED_WEAPON(ped, &current, TRUE);
			if (current != want) {
				if (!WEAPON::HAS_PED_GOT_WEAPON(ped, want, FALSE)) {
					WEAPON::GIVE_WEAPON_TO_PED(ped, want, kGunAmmo, FALSE, TRUE);
				}
				WEAPON::SET_CURRENT_PED_WEAPON(ped, want, TRUE);
			}
		} else {
			a_out.attackPressed = CONTROLS::IS_DISABLED_CONTROL_JUST_PRESSED(0, kControlAttack) != FALSE;
			a_out.attackReleased = CONTROLS::IS_DISABLED_CONTROL_JUST_RELEASED(0, kControlAttack) != FALSE;
			a_out.usePressed = CONTROLS::IS_DISABLED_CONTROL_JUST_PRESSED(0, kControlAim) != FALSE;
			a_out.useReleased = CONTROLS::IS_DISABLED_CONTROL_JUST_RELEASED(0, kControlAim) != FALSE;
		}
		for (int c : kControlNext) {
			a_out.scroll += CONTROLS::IS_DISABLED_CONTROL_JUST_PRESSED(0, c) ? 1 : 0;
		}
		for (int c : kControlPrevious) {
			a_out.scroll -= CONTROLS::IS_DISABLED_CONTROL_JUST_PRESSED(0, c) ? 1 : 0;
		}
		a_out.scroll = a_out.scroll > 0 ? 1 : a_out.scroll < 0 ? -1 : 0;  // the wheel reports through two controls at once
		for (int i = 0; i < 9; ++i) {
			if (CONTROLS::IS_DISABLED_CONTROL_JUST_PRESSED(0, kHotbarControls[i])) {
				a_out.slot = i;
			}
		}
		// Ctrl is Minecraft's sneak (GTA's stealth toggle is off; SetCrouch makes the player creep while it's held)
		CONTROLS::DISABLE_CONTROL_ACTION(0, kControlDuck, TRUE);
		a_out.sneak = CONTROLS::IS_DISABLED_CONTROL_PRESSED(0, kControlDuck) != FALSE;
		if (playerHidden_) {
			// Kept hidden every frame: switching character, respawning or a cutscene gives a new or visible ped.
			ENTITY::SET_ENTITY_VISIBLE(ped, FALSE, FALSE);
		}
	}

	void GtaGame::TakeScreenInput(PassthroughInput& a_out)
	{
		a_out = PassthroughInput{};
		CONTROLS::DISABLE_ALL_CONTROL_ACTIONS(0);  // the player stands still while the inventory is open
		UI::_SHOW_CURSOR_THIS_FRAME();
		a_out.cursorValid = true;
		a_out.cursorX = CONTROLS::GET_DISABLED_CONTROL_NORMAL(0, kCursorX);
		a_out.cursorY = CONTROLS::GET_DISABLED_CONTROL_NORMAL(0, kCursorY);
		// The mouse buttons straight from Windows: GTA's cursor-accept control missed left clicks in Sary's game
		// (2026-10-07: only right click moved items). GTA has the focus, so these are the player's clicks.
		const bool left = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0, right = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
		a_out.attackPressed = left && !screenLeft_;
		a_out.attackReleased = !left && screenLeft_;
		a_out.usePressed = right && !screenRight_;
		a_out.useReleased = !right && screenRight_;
		screenLeft_ = left;
		screenRight_ = right;
		a_out.closeScreen = CONTROLS::IS_DISABLED_CONTROL_JUST_PRESSED(0, kPause) || CONTROLS::IS_DISABLED_CONTROL_JUST_PRESSED(0, kPauseAlt);
		if (playerHidden_) {
			ENTITY::SET_ENTITY_VISIBLE(PLAYER::PLAYER_PED_ID(), FALSE, FALSE);
		}
	}

	void GtaGame::SetPlayerHidden(bool a_hidden)
	{
		playerHidden_ = a_hidden;
		const Ped ped = PLAYER::PLAYER_PED_ID();
		ENTITY::SET_ENTITY_VISIBLE(ped, a_hidden ? FALSE : TRUE, FALSE);
		if (a_hidden && gun_ < 0) {
			WEAPON::SET_CURRENT_PED_WEAPON(ped, kWeaponUnarmed, TRUE);  // Minecraft's item is what the player holds
		}
	}

	void GtaGame::HideHudThisFrame()
	{
		UI::HIDE_HUD_AND_RADAR_THIS_FRAME();
	}

	bool GtaGame::ScreenSize(int& a_width, int& a_height)
	{
		compositor::backbuffer_size(a_width, a_height);  // ReShade's view of the backbuffer: exact
		if (a_width <= 0 || a_height <= 0) {
			GRAPHICS::_GET_SCREEN_ACTIVE_RESOLUTION(&a_width, &a_height);
		}
		return a_width > 0 && a_height > 0;
	}

	void GtaGame::SetCompositorActive(bool a_active)
	{
		compositor::set_active(a_active);
	}

	void GtaGame::CompositorPose(float a_yaw, float a_pitch, float a_roll, float a_fovY, double a_x, double a_y, double a_z, float a_nearClip,
		float a_farClip)
	{
		compositor::set_host_planes(a_nearClip, a_farClip);
		compositor::set_host_pose(a_yaw, a_pitch, a_roll, a_fovY, a_x, a_y, a_z);
	}

	// What the crosshair is on: a ray from the camera through the middle of the screen (people, cars, the map).
	GtaGame::RayHit GtaGame::CrosshairRay(float a_range)
	{
		RayHit        r{};
		const Vector3 c = CAM::_GET_GAMEPLAY_CAM_COORDS();
		const Vector3 rot = CAM::_GET_GAMEPLAY_CAM_ROT(kRotationOrderZxy);
		const float   p = rot.x * kDegToRadF, h = rot.z * kDegToRadF;
		const float   dx = -std::sin(h) * std::cos(p), dy = std::cos(h) * std::cos(p), dz = std::sin(p);
		const int     ray = WORLDPROBE::_CAST_RAY_POINT_TO_POINT(c.x, c.y, c.z, c.x + dx * a_range, c.y + dy * a_range, c.z + dz * a_range, kRayEverything,
			PLAYER::PLAYER_PED_ID(), kShapeTestOptions);
		BOOL    hit = FALSE;
		Vector3 end{}, normal{};
		Entity  entity = 0;
		if (WORLDPROBE::_GET_RAYCAST_RESULT(ray, &hit, &end, &normal, &entity) != kShapeTestReady || !hit) {
			return r;
		}
		r.hit = true;
		r.x = end.x;
		r.y = end.y;
		r.z = end.z;
		r.dx = dx;
		r.dy = dy;
		if (entity != 0 && ENTITY::DOES_ENTITY_EXIST(entity)) {
			r.ped = ENTITY::IS_ENTITY_A_PED(entity) ? entity : 0;
			r.vehicle = ENTITY::IS_ENTITY_A_VEHICLE(entity) ? entity : 0;
		}
		return r;
	}

	// Hurts a person the way Minecraft hits do: a weak hit only hurts, a strong one (or an arrow) stumbles or knocks them
	// down and pushes them back. Then most run, some fight back.
	void GtaGame::HurtPed(int a_ped, float a_damage, float a_fx, float a_fy, bool a_arrow)
	{
		const Ped q = a_ped, me = PLAYER::PLAYER_PED_ID();
		if (q == me || ENTITY::IS_ENTITY_DEAD(q)) {
			return;
		}
		if (a_arrow || a_damage >= kStumbleDamage) {
			const int ms = a_arrow || a_damage >= kKnockDownDamage ? kKnockDownMs : kStumbleMs;
			PED::SET_PED_TO_RAGDOLL(q, ms, ms, 0, FALSE, FALSE, FALSE);
			const float push = 2.0f + a_damage * kPushPerDamage;
			ApplyForce(q, a_fx * push, a_fy * push, kPushUp);
		}
		ApplyDamageToPed(q, static_cast<int>(a_damage + 0.5f));
		if (!ENTITY::IS_ENTITY_DEAD(q) && !PED::IS_PED_IN_ANY_VEHICLE(q, FALSE)) {
			if (!a_arrow && GAMEPLAY::GET_RANDOM_INT_IN_RANGE(0, 100) < kFightBackPercent) {
				AI::TASK_COMBAT_PED(q, me, 0, 16);
			} else {
				AI::TASK_SMART_FLEE_PED(q, me, 100.0f, static_cast<Any>(-1), FALSE, FALSE);  // -1: no time limit
			}
		}
	}

	void GtaGame::HurtVehicle(int a_vehicle, float a_damage, float a_x, float a_y, float a_z, float a_fx, float a_fy)
	{
		const Vehicle v = a_vehicle;
		const Ped     me = PLAYER::PLAYER_PED_ID();
		const Vector3 local = ENTITY::GET_OFFSET_FROM_ENTITY_GIVEN_WORLD_COORDS(v, a_x, a_y, a_z);  // where it was hit
		VEHICLE::SET_VEHICLE_DAMAGE(v, local.x, local.y, local.z, a_damage * kCarDentPerDamage, 0.8f, TRUE);
		ApplyForce(v, a_fx * a_damage * kCarPushPerDamage, a_fy * a_damage * kCarPushPerDamage, 0.0f);
		const Ped driver = VEHICLE::GET_PED_IN_VEHICLE_SEAT(v, -1);
		if (driver != 0 && driver != me && !ENTITY::IS_ENTITY_DEAD(driver)) {
			AI::TASK_SMART_FLEE_PED(driver, me, 200.0f, static_cast<Any>(-1), FALSE, FALSE);
		}
	}

	// A Minecraft swing: the person or car under the crosshair, if it's within Minecraft's reach of the player. (It used
	// to hit everyone in a cone in front of the player, which reached people Sary wasn't aiming at.)
	void GtaGame::Melee(float a_damage)
	{
		const RayHit r = CrosshairRay(40.0f);
		if (!r.hit || (r.ped == 0 && r.vehicle == 0)) {
			return;
		}
		const Vector3 at = ENTITY::GET_ENTITY_COORDS(PLAYER::PLAYER_PED_ID(), TRUE);
		const float   dx = r.x - at.x, dy = r.y - at.y, dz = r.z - at.z, d = std::sqrt(dx * dx + dy * dy + dz * dz);
		if (d > (r.ped != 0 ? kMeleeReach : kCarReach)) {
			return;
		}
		if (r.ped != 0) {
			HurtPed(r.ped, a_damage, r.dx, r.dy, false);
		} else {
			HurtVehicle(r.vehicle, a_damage, r.x, r.y, r.z, r.dx, r.dy);
		}
		HostLog::Info("melee: hit a %s %.1f m away for %.0f", r.ped != 0 ? "person" : "car", d, a_damage);
	}

	// A Minecraft bow shot: whatever the crosshair is on gets hit at once, like a bullet.
	int GtaGame::Shoot(float a_damage)
	{
		const RayHit r = CrosshairRay(kShotRange);
		if (!r.hit || (r.ped == 0 && r.vehicle == 0)) {
			return 0;
		}
		if (r.ped != 0) {
			HurtPed(r.ped, a_damage, r.dx, r.dy, true);
		} else {
			HurtVehicle(r.vehicle, a_damage, r.x, r.y, r.z, r.dx, r.dy);
		}
		HostLog::Info("bow: hit a %s for %.0f", r.ped != 0 ? "person" : "car", a_damage);
		return 1;
	}

	void GtaGame::MeasurePropCandidates()
	{
		constexpr int kCount = static_cast<int>(sizeof(kPropCandidates) / sizeof(kPropCandidates[0]));
		if (propCandidate_ >= kCount) {
			return;
		}
		const char* name = kPropCandidates[propCandidate_];
		const Hash  model = Joaat(name);
		if (!STREAMING::IS_MODEL_VALID(model)) {
			HostLog::Info("prop candidate %s: not a model in this game", name);
			++propCandidate_;
			return;
		}
		STREAMING::REQUEST_MODEL(model);
		if (!STREAMING::HAS_MODEL_LOADED(model) && ++propWaitFrames_ < kPropLoadFrames) {
			return;
		}
		Vector3 mn{}, mx{};
		GAMEPLAY::GET_MODEL_DIMENSIONS(model, &mn, &mx);
		HostLog::Info("prop candidate %s: %.3f x %.3f x %.3f m (min %.3f %.3f %.3f)%s", name, mx.x - mn.x, mx.y - mn.y, mx.z - mn.z, mn.x, mn.y, mn.z,
			propWaitFrames_ >= kPropLoadFrames ? " (didn't stream in)" : "");
		STREAMING::SET_MODEL_AS_NO_LONGER_NEEDED(model);
		propWaitFrames_ = 0;
		++propCandidate_;
	}

	namespace
	{
		void AddText(const char* a_text)
		{
			char        part[kTextComponentChars + 1];
			std::size_t left = std::strlen(a_text);
			for (const char* p = a_text; left > 0;) {
				const std::size_t n = left < kTextComponentChars ? left : kTextComponentChars;
				std::memcpy(part, p, n);
				part[n] = '\0';
				UI::_ADD_TEXT_COMPONENT_STRING(part);
				p += n;
				left -= n;
			}
		}
	}

	void GtaGame::DrawMenuText(float a_x, float a_y, const TextStyle& a_style, const char* a_text)
	{
		UI::SET_TEXT_FONT(a_style.font);
		UI::SET_TEXT_SCALE(0.0f, a_style.scale);
		UI::SET_TEXT_COLOUR(a_style.color.r, a_style.color.g, a_style.color.b, a_style.color.a);
		UI::SET_TEXT_DROPSHADOW(0, 0, 0, 0, 0);
		UI::SET_TEXT_EDGE(0, 0, 0, 0, 0);
		switch (a_style.align) {
		case TextAlign::kCenter:
			UI::SET_TEXT_CENTRE(TRUE);
			break;
		case TextAlign::kRight:
			UI::SET_TEXT_RIGHT_JUSTIFY(TRUE);
			UI::SET_TEXT_WRAP(0.0f, a_style.wrapRight);
			break;
		default:
			UI::SET_TEXT_WRAP(a_style.wrapLeft, a_style.wrapRight);
			break;
		}
		UI::_SET_TEXT_ENTRY(g_textEntry);
		AddText(a_text);
		UI::_DRAW_TEXT(a_x, a_y);
	}

	float GtaGame::TextWidth(const TextStyle& a_style, const char* a_text)
	{
		UI::_SET_TEXT_ENTRY_FOR_WIDTH(g_textEntry);
		AddText(a_text);
		UI::SET_TEXT_FONT(a_style.font);
		UI::SET_TEXT_SCALE(0.0f, a_style.scale);
		return UI::_GET_TEXT_SCREEN_WIDTH(TRUE);
	}

	void GtaGame::DrawSprite(const char* a_dict, const char* a_name, float a_x, float a_y, float a_w, float a_h, Rgba a_color)
	{
		// ASSUMPTION: DRAW_SPRITE's modern signature has a 12th argument (p11, false), as the reference project's era of
		// natives lists it; the 2016 SDK's wrapper stops at alpha. DRAW_SPRITE takes the centre.
		invoke<Void>(0xE7FFAE5EBF23D890, const_cast<char*>(a_dict), const_cast<char*>(a_name), a_x + a_w * 0.5f, a_y + a_h * 0.5f, a_w, a_h, 0.0f,
			static_cast<int>(a_color.r), static_cast<int>(a_color.g), static_cast<int>(a_color.b), static_cast<int>(a_color.a), FALSE);
	}

	bool GtaGame::SpritesReady(const char* a_dict)
	{
		GRAPHICS::REQUEST_STREAMED_TEXTURE_DICT(const_cast<char*>(a_dict), FALSE);
		return GRAPHICS::HAS_STREAMED_TEXTURE_DICT_LOADED(const_cast<char*>(a_dict)) != FALSE;
	}

	void GtaGame::PlayMenuSound(const char* a_name)
	{
		AUDIO::PLAY_SOUND_FRONTEND(-1, const_cast<char*>(a_name), g_soundSet, TRUE);
	}

	void GtaGame::TakeMenuInput(MenuInput& a_out)
	{
		a_out = MenuInput{};
		for (int control : kMenuBlocked) {
			CONTROLS::DISABLE_CONTROL_ACTION(0, control, TRUE);
		}
		auto pressed = [](int c) { return CONTROLS::IS_DISABLED_CONTROL_JUST_PRESSED(0, c) != FALSE; };
		a_out.up = pressed(kMenuUp);
		a_out.down = pressed(kMenuDown);
		a_out.left = pressed(kMenuLeft);
		a_out.right = pressed(kMenuRight);
		a_out.accept = pressed(kMenuAccept) || pressed(kMenuSelect);
		a_out.back = pressed(kMenuBack) || pressed(kMenuPhoneBack) || pressed(kMenuPause) || pressed(kMenuPauseAlt);
	}

	bool GtaGame::BlockPropReady()
	{
		const Hash model = Joaat(kBlockPropModel);
		if (!STREAMING::IS_MODEL_VALID(model)) {
			return false;
		}
		STREAMING::REQUEST_MODEL(model);
		return STREAMING::HAS_MODEL_LOADED(model) != FALSE;
	}

	int GtaGame::SpawnBlockProp(float a_x, float a_y, float a_floorZ, float a_size)
	{
		const Object o = OBJECT::CREATE_OBJECT_NO_OFFSET(Joaat(kBlockPropModel), a_x, a_y, a_floorZ + a_size - kBlockPropHeight, FALSE, TRUE, FALSE);
		if (o == 0) {
			return 0;
		}
		ENTITY::SET_ENTITY_ROTATION(o, 0.0f, 0.0f, 0.0f, kRotationOrderZxy, TRUE);
		ENTITY::FREEZE_ENTITY_POSITION(o, TRUE);
		ENTITY::SET_ENTITY_CAN_BE_DAMAGED(o, FALSE);
		ENTITY::SET_ENTITY_COLLISION(o, TRUE, TRUE);
		ENTITY::SET_ENTITY_VISIBLE(o, FALSE, FALSE);  // Minecraft draws the block
		return o;
	}

	void GtaGame::DeleteBlockProp(int a_handle)
	{
		Object o = a_handle;
		if (ENTITY::DOES_ENTITY_EXIST(o)) {
			ENTITY::SET_ENTITY_AS_MISSION_ENTITY(o, TRUE, TRUE);
			OBJECT::DELETE_OBJECT(&o);
		}
	}

	void GtaGame::Notify(const char* a_text)
	{
		UI::_SET_NOTIFICATION_TEXT_ENTRY(g_notificationEntry);
		UI::_ADD_TEXT_COMPONENT_STRING(const_cast<char*>(a_text));
		UI::_DRAW_NOTIFICATION(FALSE, FALSE);
	}

	// Minecraft's jump: straight up, no run-up, no climbing, only from the ground. GTA's own jump is off while this is on.
	// The first version only pushed the player up for 3 frames: the game's ground movement ate most of it (a small hop)
	// and the player never counted as in the air, so Space worked again and again. Now the player is lifted off the
	// ground first, pushed up for longer, and can't jump again until standing on the ground for a moment.
	void GtaGame::TickJump(bool a_minecraft, float a_heightMetres)
	{
		if (!a_minecraft) {
			jumpFrames_ = 0;
			return;
		}
		CONTROLS::DISABLE_CONTROL_ACTION(0, kControlJump, TRUE);
		const Ped     ped = PLAYER::PLAYER_PED_ID();
		const Vector3 v = ENTITY::GET_ENTITY_VELOCITY(ped);
		const float   up = std::sqrt(2.0f * kGravity * a_heightMetres);
		const bool    grounded = !ENTITY::IS_ENTITY_IN_AIR(ped) && !PED::IS_PED_FALLING(ped) && !PED::IS_PED_RAGDOLL(ped) && !PED::IS_PED_SWIMMING(ped) &&
		                      !PED::IS_PED_CLIMBING(ped) && PED::IS_PED_ON_FOOT(ped) && std::fabs(v.z) < 0.6f;
		groundedFrames_ = grounded && jumpFrames_ == 0 ? groundedFrames_ + 1 : 0;
		if (jumpFrames_ > 0) {
			--jumpFrames_;
			if (v.z < up * 0.85f && jumpFrames_ > kJumpPushFrames - kJumpBoostFrames) {
				ENTITY::SET_ENTITY_VELOCITY(ped, v.x, v.y, up);
			}
			return;
		}
		if (!CONTROLS::IS_DISABLED_CONTROL_JUST_PRESSED(0, kControlJump) || groundedFrames_ < kJumpGroundFrames) {
			return;  // Minecraft can't jump in the air either
		}
		const Vector3 p = ENTITY::GET_ENTITY_COORDS(ped, TRUE);
		ENTITY::SET_ENTITY_COORDS_NO_OFFSET(ped, p.x, p.y, p.z + kJumpLift, FALSE, FALSE, FALSE);  // off the ground: the push sticks
		ENTITY::SET_ENTITY_VELOCITY(ped, v.x, v.y, up);
		jumpFrames_ = kJumpPushFrames;
		groundedFrames_ = 0;
		if (jumpsLogged_ < 3) {
			++jumpsLogged_;
			HostLog::Info("jump: Minecraft jump, %.1f m/s up", up);
		}
	}

	void GtaGame::SetPlayerHealth(int a_health)
	{
		ENTITY::SET_ENTITY_HEALTH(PLAYER::PLAYER_PED_ID(), a_health);
	}

	void GtaGame::KillPlayer()
	{
		ENTITY::SET_ENTITY_HEALTH(PLAYER::PLAYER_PED_ID(), 0);
	}

	void GtaGame::Explode(float a_x, float a_y, float a_z, float a_power)
	{
		const float scale = std::fmin(std::fmax(a_power / 4.0f, 0.25f), 4.0f);
		AddExplosion(a_x, a_y, a_z, kExplosionSticky, scale, 0.6f * scale);
	}

	int GtaGame::ProjectileHit(float a_x, float a_y, float a_z, float a_damage)
	{
		const Ped me = PLAYER::PLAYER_PED_ID();
		int       handles[kMaxWorldEntities];
		const int peds = worldGetAllPeds(handles, kMaxWorldEntities);
		int       hit = 0;
		for (int i = 0; i < peds; ++i) {
			const Ped q = handles[i];
			if (q == me || ENTITY::IS_ENTITY_DEAD(q) || PED::IS_PED_IN_ANY_VEHICLE(q, FALSE)) {
				continue;
			}
			const Vector3 o = ENTITY::GET_ENTITY_COORDS(q, TRUE);
			const float   dx = o.x - a_x, dy = o.y - a_y;
			if (dx * dx + dy * dy > kArrowReach * kArrowReach || std::fabs(o.z - a_z) > kArrowHeight) {
				continue;
			}
			PED::SET_PED_TO_RAGDOLL(q, kKnockDownMs, kKnockDownMs, 0, FALSE, FALSE, FALSE);
			ApplyDamageToPed(q, static_cast<int>(a_damage + 0.5f));
			if (!ENTITY::IS_ENTITY_DEAD(q)) {
				AI::TASK_SMART_FLEE_PED(q, me, 100.0f, static_cast<Any>(-1), FALSE, FALSE);
			}
			++hit;
		}
		if (hit > 0) {
			HostLog::Info("arrow: hit %d (damage %.0f)", hit, a_damage);
		}
		return hit;
	}

	void GtaGame::DrawLight(float a_x, float a_y, float a_z, Rgba a_color, float a_range, float a_intensity)
	{
		GRAPHICS::DRAW_LIGHT_WITH_RANGE(a_x, a_y, a_z, a_color.r, a_color.g, a_color.b, a_range, a_intensity);
	}

	void GtaGame::RequestCollision(float a_x, float a_y, float a_z)
	{
		STREAMING::REQUEST_COLLISION_AT_COORD(a_x, a_y, a_z);
	}

	void GtaGame::DrawLabel(float a_x, float a_y, float a_scale, Rgba a_color, const char* a_text)
	{
		UI::SET_TEXT_FONT(kFontChaletLondon);
		UI::SET_TEXT_SCALE(0.0f, a_scale);
		UI::SET_TEXT_COLOUR(a_color.r, a_color.g, a_color.b, a_color.a);
		UI::SET_TEXT_CENTRE(FALSE);
		UI::SET_TEXT_DROPSHADOW(0, 0, 0, 0, 0);
		UI::SET_TEXT_EDGE(0, 0, 0, 0, 0);
		UI::_SET_TEXT_ENTRY(g_textEntry);
		char        part[kTextComponentChars + 1];
		std::size_t left = std::strlen(a_text);
		for (const char* p = a_text; left > 0;) {
			const std::size_t n = left < kTextComponentChars ? left : kTextComponentChars;
			std::memcpy(part, p, n);
			part[n] = '\0';
			UI::_ADD_TEXT_COMPONENT_STRING(part);
			p += n;
			left -= n;
		}
		UI::_DRAW_TEXT(a_x, a_y);
	}

	void GtaGame::DrawBox(float a_x, float a_y, float a_w, float a_h, Rgba a_color)
	{
		// DRAW_RECT takes the centre.
		GRAPHICS::DRAW_RECT(a_x + a_w * 0.5f, a_y + a_h * 0.5f, a_w, a_h, a_color.r, a_color.g, a_color.b, a_color.a);
	}

	void GtaGame::SetGun(int a_index)
	{
		const Ped ped = PLAYER::PLAYER_PED_ID();
		const int index = a_index >= 0 && a_index < static_cast<int>(sizeof(kGuns) / sizeof(kGuns[0])) ? a_index : -1;
		if (gun_ >= 0 && gun_ != index && gunGiven_) {
			WEAPON::REMOVE_WEAPON_FROM_PED(ped, kGuns[gun_]);  // only the guns CraftV gave: the player's own stay
		}
		gun_ = index;
		gunGiven_ = false;
		if (index < 0) {
			WEAPON::SET_CURRENT_PED_WEAPON(ped, kWeaponUnarmed, TRUE);  // Minecraft's item is what the player holds
			return;
		}
		const Hash want = kGuns[index];
		if (!WEAPON::HAS_PED_GOT_WEAPON(ped, want, FALSE)) {
			WEAPON::GIVE_WEAPON_TO_PED(ped, want, kGunAmmo, FALSE, TRUE);
			gunGiven_ = true;
		} else {
			WEAPON::SET_PED_AMMO(ped, want, kGunAmmo);
		}
		WEAPON::SET_CURRENT_PED_WEAPON(ped, want, TRUE);
	}

	void GtaGame::SetCrouch(bool a_crouching)
	{
		if (a_crouching == crouching_) {
			return;
		}
		crouching_ = a_crouching;
		PED::SET_PED_STEALTH_MOVEMENT(PLAYER::PLAYER_PED_ID(), a_crouching ? TRUE : FALSE, g_stealthAction);
	}

	// Flying (experimental): the player is held in the air and moved with the movement keys, the way trainers fly.
	void GtaGame::TickFly(bool a_on)
	{
		const Ped ped = PLAYER::PLAYER_PED_ID();
		if (!a_on) {
			if (flying_) {
				flying_ = false;
				ENTITY::FREEZE_ENTITY_POSITION(ped, FALSE);
			}
			return;
		}
		if (!flying_) {
			flying_ = true;
			ENTITY::FREEZE_ENTITY_POSITION(ped, TRUE);
		}
		for (int c : { kControlJump, kControlDuck, kControlSprint, kControlMoveLr, kControlMoveUd }) {
			CONTROLS::DISABLE_CONTROL_ACTION(0, c, TRUE);
		}
		const float forward = -CONTROLS::GET_DISABLED_CONTROL_NORMAL(0, kControlMoveUd);  // -1 is forward
		const float right = CONTROLS::GET_DISABLED_CONTROL_NORMAL(0, kControlMoveLr);
		const float rise = (CONTROLS::IS_DISABLED_CONTROL_PRESSED(0, kControlJump) ? 1.0f : 0.0f) - (CONTROLS::IS_DISABLED_CONTROL_PRESSED(0, kControlDuck) ? 1.0f : 0.0f);
		const float speed = (CONTROLS::IS_DISABLED_CONTROL_PRESSED(0, kControlSprint) ? kFlyFastSpeed : kFlySpeed) * GAMEPLAY::GET_FRAME_TIME();
		const float h = CAM::_GET_GAMEPLAY_CAM_ROT(kRotationOrderZxy).z;
		const float fx = -std::sin(h * kDegToRadF), fy = std::cos(h * kDegToRadF);
		Vector3     p = ENTITY::GET_ENTITY_COORDS(ped, TRUE);
		p.x += (fx * forward + fy * right) * speed;
		p.y += (fy * forward - fx * right) * speed;
		p.z += rise * speed;
		ENTITY::SET_ENTITY_COORDS_NO_OFFSET(ped, p.x, p.y, p.z, FALSE, FALSE, FALSE);
		if (forward != 0.0f || right != 0.0f) {
			ENTITY::SET_ENTITY_HEADING(ped, h);
		}
	}

	void GtaGame::SetInvincible(bool a_on)
	{
		PLAYER::SET_PLAYER_INVINCIBLE(PLAYER::PLAYER_ID(), a_on ? TRUE : FALSE);
	}

	void GtaGame::SetCompositorMask(float a_x0, float a_y0, float a_x1, float a_y1)
	{
		compositor::set_mask(a_x0, a_y0, a_x1, a_y1);
	}

	void GtaGame::SetCompositorTranslation(bool a_on)
	{
		compositor::set_translation(a_on);
	}

	// A solid block's space must be clear: cars (by their model's box) and people standing in it.
	bool GtaGame::BlockSpaceFree(float a_x, float a_y, float a_floorZ, float a_size)
	{
		const float cz = a_floorZ + a_size * 0.5f, half = a_size * 0.5f;
		int         handles[kMaxWorldEntities];
		const int   cars = worldGetAllVehicles(handles, kMaxWorldEntities);
		for (int i = 0; i < cars; ++i) {
			const Vehicle v = handles[i];
			const Vector3 o = ENTITY::GET_ENTITY_COORDS(v, TRUE);
			if (std::fabs(o.x - a_x) > 8.0f || std::fabs(o.y - a_y) > 8.0f || std::fabs(o.z - cz) > 8.0f) {
				continue;
			}
			Vector3 mn{}, mx{};
			GAMEPLAY::GET_MODEL_DIMENSIONS(ENTITY::GET_ENTITY_MODEL(v), &mn, &mx);
			const Vector3 l = ENTITY::GET_OFFSET_FROM_ENTITY_GIVEN_WORLD_COORDS(v, a_x, a_y, cz);  // the block's centre, in the car's frame
			if (l.x + half > mn.x && l.x - half < mx.x && l.y + half > mn.y && l.y - half < mx.y && l.z + half > mn.z && l.z - half < mx.z) {
				return false;
			}
		}
		const int peds = worldGetAllPeds(handles, kMaxWorldEntities);
		for (int i = 0; i < peds; ++i) {
			const Vector3 o = ENTITY::GET_ENTITY_COORDS(handles[i], TRUE);  // the ped's middle, about 1 m up
			if (std::fabs(o.x - a_x) < half + 0.35f && std::fabs(o.y - a_y) < half + 0.35f && o.z - 1.0f < a_floorZ + a_size && o.z + 1.0f > a_floorZ) {
				return false;
			}
		}
		return true;
	}
}
