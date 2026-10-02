// The only window the host plugin has onto RDR2. The ASI implements it with ScriptHookRDR2
// natives (src/asi/rdr2_game.cpp); tests implement it with a fake. Everything above this line is
// game-agnostic and unit-tested without RDR2.
#pragma once

#include <cstdint>

namespace redcraft::host
{
	// One snapshot of what the plugin needs from the game, taken once per script tick.
	// Units: RDR2 world space (metres, Z up). Angles in degrees as the game reports them.
	struct GameSample
	{
		// Session / safety (DECISIONS D-011)
		bool networkGameInProgress = false;  // NETWORK::NETWORK_IS_GAME_IN_PROGRESS
		bool networkSessionStarted = false;  // NETWORK::NETWORK_IS_SESSION_STARTED
		bool networkInSession = false;       // NETWORK::NETWORK_IS_IN_SESSION
		bool loadingScreen = false;          // DLC2::GET_IS_LOADING_SCREEN_ACTIVE
		bool screenFadedOut = false;         // CAM::IS_SCREEN_FADED_OUT

		// The player ped
		bool  playerExists = false;  // ENTITY::DOES_ENTITY_EXIST(PLAYER_PED_ID())
		bool  playerDead = false;    // PLAYER::IS_PLAYER_DEAD
		float x = 0, y = 0, z = 0;   // ENTITY::GET_ENTITY_COORDS (ped root, not the feet)
		float heading = 0;           // ENTITY::GET_ENTITY_HEADING: 0 = north (+Y), counter-clockwise
		float vx = 0, vy = 0, vz = 0;  // ENTITY::GET_ENTITY_VELOCITY, metres/second
		float heightAboveGround = 0;   // ENTITY::GET_ENTITY_HEIGHT_ABOVE_GROUND (for measuring the feet offset)
		bool  inAir = false;           // ENTITY::IS_ENTITY_IN_AIR
		bool  falling = false;         // PED::IS_PED_FALLING
		bool  onMount = false;         // PED::IS_PED_ON_MOUNT (horse)
		bool  inVehicle = false;       // PED::IS_PED_IN_ANY_VEHICLE (wagon, train, boat)
		bool  swimming = false;        // PED::IS_PED_SWIMMING

		// The gameplay camera
		float camPitch = 0;  // CAM::GET_GAMEPLAY_CAM_ROT(2).x, positive = looking up
	};

	struct Rgba
	{
		std::uint8_t r, g, b, a;
	};

	class IGame
	{
	public:
		virtual ~IGame() = default;
		// Fills a_out. Called once per tick from the script thread.
		virtual void Sample(GameSample& a_out) = 0;
		// Screen-space text and rectangles, 0..1 coordinates, top-left origin. Script thread only.
		virtual void DrawLabel(float a_x, float a_y, float a_scale, Rgba a_color, const char* a_text) = 0;
		virtual void DrawBox(float a_x, float a_y, float a_w, float a_h, Rgba a_color) = 0;
	};
}
