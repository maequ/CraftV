// CraftV's settings inside the game (F8), drawn the way the game's own menus are: the game's banner and highlight
// textures (commonmenu), its fonts and text sizes, its menu sounds, and the proportions of GTA V's interaction menu
// (the layout NativeUI reproduces). Game-agnostic: everything goes through IGame; HostPlugin owns the rows.
#pragma once

#include "game_api.h"

namespace craftv::host
{
	// One line of the menu. value is shown on the right; choice rows change with left/right.
	struct MenuRow
	{
		const char* label = "";
		const char* value = "";
		const char* description = "";
		bool        choice = true;
	};

	enum class MenuAction
	{
		kNone,
		kPrevious,  // left on a choice row
		kNext,      // right (or Enter) on a choice row
	};

	class SettingsMenu
	{
	public:
		static constexpr int kMaxRows = 12;

		bool Open() const { return open_; }
		void Toggle(IGame& a_game);
		// Reads the menu keys (the game ignores them while the menu is open). a_row is the row an action applies to.
		MenuAction Input(IGame& a_game, int a_rowCount, int& a_row);
		// a_aspect: the screen's width / height.
		void Draw(IGame& a_game, const MenuRow* a_rows, int a_count, float a_aspect) const;
		int  Selected() const { return selected_; }

	private:
		bool open_ = false;
		int  selected_ = 0;
	};
}
