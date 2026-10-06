// The only window the host plugin has onto the game. Each ASI implements it with its script hook's
// natives (gta/src/gta_game.cpp, rdr2/src/asi/rdr2_game.cpp); tests implement it with a fake. Everything
// above this line is game-agnostic and unit-tested without a game.
#pragma once

#include <cstdint>
#include <cstring>

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

	// The camera the game rendered this frame with (the passthrough, brief §8, PROTOCOL.md §7.15).
	struct CameraSample
	{
		bool  valid = false;
		float x = 0, y = 0, z = 0;               // GET_FINAL_RENDERED_CAM_COORD
		float pitch = 0, roll = 0, heading = 0;  // GET_FINAL_RENDERED_CAM_ROT(2): x (positive up), y, z (heading, as GameSample)
		float fovY = 50;                         // GET_FINAL_RENDERED_CAM_FOV: vertical, degrees
		float nearClip = 0, farClip = 0;         // GET_FINAL_RENDERED_CAM_NEAR_CLIP / FAR_CLIP
		bool  firstPerson = false;               // the follow cam's view mode is first person
	};

	// Text drawn the way the game's menus draw it. font: the game's (0 Chalet London, 1 HouseScript).
	enum class TextAlign
	{
		kLeft,
		kCenter,
		kRight,  // x is the right edge
	};
	struct TextStyle
	{
		int       font = 0;
		float     scale = 0.35f;
		Rgba      color{ 255, 255, 255, 255 };
		TextAlign align = TextAlign::kLeft;
		float     wrapLeft = 0.0f, wrapRight = 1.0f;  // screen fractions
	};

	// The settings menu's keys this frame (the game ignores them while the menu is open).
	struct MenuInput
	{
		bool up = false, down = false, left = false, right = false, accept = false, back = false;
	};

	// The owner's Minecraft buttons this frame while the passthrough has them (PROTOCOL.md §7.17).
	struct PassthroughInput
	{
		bool attackPressed = false, attackReleased = false;  // left mouse
		bool usePressed = false, useReleased = false;        // right mouse
		int  scroll = 0;                                     // +1 next hotbar slot, -1 previous
		int  slot = -1;                                      // 0..8 when a number key was pressed
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

		// ---- the passthrough (brief §8). The defaults are a game without it.
		// Whether Minecraft's frames can be composited into the game's picture (the ReShade add-on is loaded).
		virtual bool PassthroughAvailable() { return false; }
		virtual void SampleCamera(CameraSample& a_out) { a_out = CameraSample{}; }
		// Disables the game's own attack, aim and weapon controls for this frame and reports the owner's buttons.
		virtual void TakePassthroughInput(PassthroughInput& a_out) { a_out = PassthroughInput{}; }
		// Minecraft draws the owner while the passthrough is on, so the game hides its player (and their weapon).
		virtual void SetPlayerHidden(bool) {}
		virtual void HideHudThisFrame() {}
		// The game's picture in pixels. False if unknown.
		virtual bool ScreenSize(int& a_width, int& a_height)
		{
			a_width = a_height = 0;
			return false;
		}
		// Whether the compositor draws Minecraft this frame, and the camera in Minecraft's terms (§7.15) it
		// re-projects Minecraft's frame to.
		virtual void SetCompositorActive(bool) {}
		virtual void CompositorPose(float /*yaw*/, float /*pitch*/, float /*roll*/, float /*fovY*/, double /*x*/, double /*y*/, double /*z*/,
			float /*nearClip*/, float /*farClip*/)
		{
		}
		// A Minecraft melee swing: hurt and knock back the people in front of the player, shove cars.
		virtual void Melee(float /*damage*/) {}
		// A short on-screen message (the game's notification feed).
		virtual void Notify(const char* /*text*/) {}

		// ---- the settings menu (F8). Defaults: no menu.
		virtual void DrawMenuText(float /*x*/, float /*y*/, const TextStyle& /*style*/, const char* /*text*/) {}
		virtual float TextWidth(const TextStyle& a_style, const char* a_text)
		{
			return static_cast<float>(std::strlen(a_text)) * 0.0055f * a_style.scale / 0.35f;  // rough; the game measures it
		}
		// A texture from a game texture dictionary, top-left corner and size in screen fractions.
		virtual void DrawSprite(const char* /*dict*/, const char* /*name*/, float /*x*/, float /*y*/, float /*w*/, float /*h*/, Rgba /*color*/) {}
		// Streams a texture dictionary in; true once it can be drawn.
		virtual bool SpritesReady(const char* /*dict*/) { return true; }
		virtual void PlayMenuSound(const char* /*name*/) {}
		virtual void TakeMenuInput(MenuInput& a_out) { a_out = MenuInput{}; }
	};
}
