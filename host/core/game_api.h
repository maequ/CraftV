// The only window the host plugin has onto the game. Each ASI implements it with its script hook's
// natives (gta/src/gta_game.cpp, rdr2/src/asi/rdr2_game.cpp); tests implement it with a fake. Everything
// above this line is game-agnostic and unit-tested without a game.
#pragma once

#include <cstdint>

namespace craftv::host
{
	// One snapshot of what the plugin needs from the game, taken once per script tick.
	// Units: RAGE world space (metres, X east, Y north, Z up). Angles in degrees as the game reports them.
	// The comments name the natives the ASIs read.
	struct GameSample
	{
		// Session / safety (DECISIONS D-011)
		bool networkGameInProgress = false;  // NETWORK_IS_GAME_IN_PROGRESS
		bool networkSessionStarted = false;  // NETWORK_IS_SESSION_STARTED
		bool networkInSession = false;       // NETWORK_IS_IN_SESSION
		bool loadingScreen = false;          // a loading screen is up
		bool screenFadedOut = false;         // IS_SCREEN_FADED_OUT

		// The player ped
		bool  playerExists = false;    // DOES_ENTITY_EXIST(PLAYER_PED_ID())
		bool  playerDead = false;      // IS_PLAYER_DEAD
		float x = 0, y = 0, z = 0;     // GET_ENTITY_COORDS (ped root, not the feet)
		float heading = 0;             // GET_ENTITY_HEADING: 0 = north (+Y), counter-clockwise
		float vx = 0, vy = 0, vz = 0;  // GET_ENTITY_VELOCITY, metres/second
		float heightAboveGround = 0;   // GET_ENTITY_HEIGHT_ABOVE_GROUND (for measuring the feet offset)
		bool  inAir = false;           // IS_ENTITY_IN_AIR
		bool  falling = false;         // IS_PED_FALLING
		bool  onMount = false;         // RDR2: on a horse. GTA V: on a bike
		bool  inVehicle = false;       // IS_PED_IN_ANY_VEHICLE
		bool  swimming = false;        // IS_PED_SWIMMING

		// The gameplay camera
		float camPitch = 0;  // GET_GAMEPLAY_CAM_ROT(2).x, positive = looking up
	};

	// What the game says about the ground of one column (terrain scanner, PROTOCOL.md §7.12).
	struct GroundProbe
	{
		bool          hit = false;       // solid ground was found
		float         groundZ = 0;       // its top, metres
		std::uint32_t materialHash = 0;  // the game's surface material, 0 = not reported
		bool          water = false;     // water above the ground here
		float         waterZ = 0;        // the water surface, metres
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
		// The ground at world (x, y), found by probing straight down from high above the map. Returns
		// a_out.hit. A miss means nothing is there or the game hasn't streamed that area's collision in.
		virtual bool ProbeGround(float a_x, float a_y, GroundProbe& a_out) = 0;
		// Asks the game to stream collision in around a point, so later probes there can hit.
		virtual void RequestCollision(float a_x, float a_y, float a_z) = 0;
	};
}
