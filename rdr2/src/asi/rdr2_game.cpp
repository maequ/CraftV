// Every native used here was checked against the ScriptHookRDR2 SDK's natives.h (v1.0.1207.73):
// name, namespace, hash and signature. Text drawing copies the SDK sample (NativeTrainer/scriptmenu.cpp).
#include "rdr2_game.h"

#pragma warning(push, 0)
#include "natives.h"
#pragma warning(pop)

namespace craftv::host
{
	namespace
	{
		constexpr int  kRotationOrderZxy = 2;   // the rotation order the SDK samples and RAGE scripts use
		constexpr int  kLiteralStringFlags = 10;  // CREATE_STRING flags for a literal, as in the SDK sample
		constexpr char kLiteralString[] = "LITERAL_STRING";
	}

	void Rdr2Game::Sample(GameSample& s)
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
		// ASSUMPTION: (alive = TRUE, realCoords = TRUE), the documented meaning of the two BOOLs.
		const Vector3 pos = ENTITY::GET_ENTITY_COORDS(ped, TRUE, TRUE);
		s.x = pos.x;
		s.y = pos.y;
		s.z = pos.z;
		s.heading = ENTITY::GET_ENTITY_HEADING(ped);
		// ASSUMPTION: second argument 0 = world-space velocity.
		const Vector3 vel = ENTITY::GET_ENTITY_VELOCITY(ped, 0);
		s.vx = vel.x;
		s.vy = vel.y;
		s.vz = vel.z;
		s.heightAboveGround = ENTITY::GET_ENTITY_HEIGHT_ABOVE_GROUND(ped);
		s.inAir = ENTITY::IS_ENTITY_IN_AIR(ped, 0) != FALSE;  // the SDK sample passes 0
		s.falling = PED::IS_PED_FALLING(ped) != FALSE;
		s.onMount = PED::IS_PED_ON_MOUNT(ped) != FALSE;
		s.inVehicle = PED::IS_PED_IN_ANY_VEHICLE(ped, FALSE) != FALSE;
		s.swimming = PED::IS_PED_SWIMMING(ped) != FALSE;
		s.camPitch = CAM::GET_GAMEPLAY_CAM_ROT(kRotationOrderZxy).x;
	}

	void Rdr2Game::DrawLabel(float a_x, float a_y, float a_scale, Rgba a_color, const char* a_text)
	{
		UI::SET_TEXT_SCALE(0.0f, a_scale);
		UI::SET_TEXT_COLOR_RGBA(a_color.r, a_color.g, a_color.b, a_color.a);
		UI::SET_TEXT_CENTRE(FALSE);
		UI::SET_TEXT_DROPSHADOW(0, 0, 0, 0, 0);
		UI::DRAW_TEXT(GAMEPLAY::CREATE_STRING(kLiteralStringFlags, const_cast<char*>(kLiteralString), const_cast<char*>(a_text)), a_x, a_y);
	}

	void Rdr2Game::DrawBox(float a_x, float a_y, float a_w, float a_h, Rgba a_color)
	{
		// DRAW_RECT takes the centre.
		GRAPHICS::DRAW_RECT(a_x + a_w * 0.5f, a_y + a_h * 0.5f, a_w, a_h, a_color.r, a_color.g, a_color.b, a_color.a, FALSE, FALSE);
	}
}
