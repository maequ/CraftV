// Every native used here was checked against the Script Hook V SDK's natives.h (v1.0.617.1a): name,
// namespace, hash and signature. The 2016 SDK predates some modern native names; those are noted.
// Text drawing copies the SDK sample (NativeTrainer/script.cpp).
#include "gta_game.h"

#include "core/host_log.h"

#pragma warning(push, 0)
#include "natives.h"
#pragma warning(pop)

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

		// ASSUMPTION: 0x7EE9F5D83DD4F90E is START_EXPENSIVE_SYNCHRONOUS_SHAPE_TEST_LOS_PROBE (unnamed in the 2016
		// SDK): a ray whose result is ready at once. 0x65287525D951F6BE (_GET_RAYCAST_RESULT_2 in the SDK) is
		// GET_SHAPE_TEST_RESULT_INCLUDING_MATERIAL. If the result isn't ready, ProbeGround falls back to
		// GET_GROUND_Z_FOR_3D_COORD (height only).
		int StartSynchronousRay(float a_x1, float a_y1, float a_z1, float a_x2, float a_y2, float a_z2)
		{
			return WORLDPROBE::_0x7EE9F5D83DD4F90E(a_x1, a_y1, a_z1, a_x2, a_y2, a_z2, kShapeTestMap, 0, kShapeTestOptions);
		}

		constexpr std::uint64_t kDiagnosticProbes = 8;        // log every detail of the first probes
		constexpr std::uint64_t kProbeSummaryEvery = 16384;   // then a summary line every this many probes
		constexpr int           kDiagnosticMaxPolls = 120;    // frames to wait for an asynchronous diagnostic ray

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
		s.camPitch = CAM::GET_GAMEPLAY_CAM_ROT(kRotationOrderZxy).x;
	}

	bool GtaGame::ProbeGround(float a_x, float a_y, GroundProbe& a_out)
	{
		a_out = GroundProbe{};
		PollDiagnosticRays();
		BOOL    hit = FALSE;
		Vector3 end{}, normal{};
		Hash    material = 0;
		Entity  entity = 0;
		const int ray = StartSynchronousRay(a_x, a_y, kProbeTopZ, a_x, a_y, kProbeBottomZ);
		const int status = WORLDPROBE::_GET_RAYCAST_RESULT_2(ray, &hit, &end, &normal, &material, &entity);
		++stats_.probes;
		if (status == kShapeTestReady) {
			++stats_.syncReady;
			stats_.syncHits += hit ? 1 : 0;
			stats_.withMaterial += hit && material != 0 ? 1 : 0;
		} else {
			++stats_.fallbacks;
		}
		if (stats_.probes <= kDiagnosticProbes) {
			HostLog::Info("probe %llu at %.1f, %.1f: synchronous ray status %d hit %d z %.2f material 0x%08X entity %d", static_cast<unsigned long long>(stats_.probes),
				a_x, a_y, status, hit, end.z, material, entity);
			StartDiagnosticRay(a_x, a_y);
		} else if (stats_.probes % kProbeSummaryEvery == 0) {
			HostLog::Info("probes so far %llu: synchronous ready %llu, hit %llu, with material %llu, fell back to ground-Z %llu",
				static_cast<unsigned long long>(stats_.probes), static_cast<unsigned long long>(stats_.syncReady),
				static_cast<unsigned long long>(stats_.syncHits), static_cast<unsigned long long>(stats_.withMaterial),
				static_cast<unsigned long long>(stats_.fallbacks));
		}
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

	// Diagnostics for the first probes (remove once the material path is verified in game): the same ray
	// through the asynchronous shape test (0x377906D8A31E5586, _CAST_RAY_POINT_TO_POINT in the SDK), whose
	// result arrives a frame or more later, to compare what each kind reports.
	void GtaGame::StartDiagnosticRay(float a_x, float a_y)
	{
		for (auto& d : diagnostic_) {
			if (!d.pending) {
				d.handle = WORLDPROBE::_CAST_RAY_POINT_TO_POINT(a_x, a_y, kProbeTopZ, a_x, a_y, kProbeBottomZ, kShapeTestMap, 0, kShapeTestOptions);
				d.x = a_x;
				d.y = a_y;
				d.polls = 0;
				d.pending = true;
				return;
			}
		}
	}

	void GtaGame::PollDiagnosticRays()
	{
		for (auto& d : diagnostic_) {
			if (!d.pending) {
				continue;
			}
			BOOL      hit = FALSE;
			Vector3   end{}, normal{};
			Hash      material = 0;
			Entity    entity = 0;
			const int status = WORLDPROBE::_GET_RAYCAST_RESULT_2(d.handle, &hit, &end, &normal, &material, &entity);
			++d.polls;
			if (status != 1 || d.polls > kDiagnosticMaxPolls) {  // 1 = still pending
				HostLog::Info("asynchronous ray at %.1f, %.1f after %d polls: status %d hit %d z %.2f material 0x%08X entity %d", d.x, d.y, d.polls, status,
					hit, end.z, material, entity);
				d.pending = false;
			}
		}
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
}
