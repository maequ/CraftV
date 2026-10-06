#include "settings_menu.h"

#include <cstdio>

namespace craftv::host
{
	namespace
	{
		// GTA V's interaction menu, in pixels of a 1080-line screen (NativeUI's measurements of it).
		constexpr float kLeft = 20, kTop = 20, kWidth = 431;
		constexpr float kBannerHeight = 107, kSubtitleHeight = 37, kRowHeight = 38;
		constexpr float kTextPad = 8, kTextTop = 3, kTitleTop = 20, kSubtitleTextTop = 4;
		constexpr float kArrow = 30, kArrowPad = 5, kArrowTop = 4;
		constexpr float kDescriptionGap = 3, kDescriptionBar = 4, kDescriptionHeight = 34, kDescriptionTextTop = 4;
		constexpr float kLineHeight = 1080.0f;
		constexpr float kItemScale = 0.35f, kTitleScale = 1.15f;
		constexpr int   kFontChalet = 0, kFontHouseScript = 1;  // the game's menu fonts
		constexpr Rgba  kWhite{ 255, 255, 255, 255 }, kBlack{ 0, 0, 0, 255 }, kGrey{ 180, 180, 180, 255 };
		constexpr const char* kDict = "commonmenu";
		constexpr const char* kSoundUpDown = "NAV_UP_DOWN";
		constexpr const char* kSoundLeftRight = "NAV_LEFT_RIGHT";
		constexpr const char* kSoundSelect = "SELECT";
		constexpr const char* kSoundBack = "BACK";

		TextStyle Style(int a_font, float a_scale, Rgba a_color, TextAlign a_align = TextAlign::kLeft)
		{
			TextStyle s;
			s.font = a_font;
			s.scale = a_scale;
			s.color = a_color;
			s.align = a_align;
			return s;
		}
	}

	void SettingsMenu::Toggle(IGame& a_game)
	{
		open_ = !open_;
		a_game.PlayMenuSound(open_ ? kSoundSelect : kSoundBack);
	}

	MenuAction SettingsMenu::Input(IGame& a_game, int a_rowCount, int& a_row)
	{
		MenuInput in;
		a_game.TakeMenuInput(in);
		a_row = selected_;
		if (!open_ || a_rowCount <= 0) {
			return MenuAction::kNone;
		}
		if (in.back) {
			Toggle(a_game);
			return MenuAction::kNone;
		}
		if (in.up || in.down) {
			selected_ = (selected_ + (in.down ? 1 : -1) + a_rowCount) % a_rowCount;
			a_row = selected_;
			a_game.PlayMenuSound(kSoundUpDown);
			return MenuAction::kNone;
		}
		if (selected_ >= a_rowCount) {
			selected_ = a_rowCount - 1;
			a_row = selected_;
		}
		if (in.left || in.right || in.accept) {
			a_game.PlayMenuSound(in.accept ? kSoundSelect : kSoundLeftRight);
			return in.left ? MenuAction::kPrevious : MenuAction::kNext;
		}
		return MenuAction::kNone;
	}

	void SettingsMenu::Draw(IGame& a_game, const MenuRow* a_rows, int a_count, float a_aspect) const
	{
		if (!open_ || a_count <= 0 || !a_game.SpritesReady(kDict)) {
			return;
		}
		const float px = 1.0f / (kLineHeight * (a_aspect > 0.1f ? a_aspect : 16.0f / 9.0f)), py = 1.0f / kLineHeight;
		const float x = kLeft * px, w = kWidth * px;
		float       y = kTop * py;

		a_game.DrawSprite(kDict, "interaction_bgd", x, y, w, kBannerHeight * py, kWhite);
		a_game.DrawMenuText(x + w * 0.5f, y + kTitleTop * py, Style(kFontHouseScript, kTitleScale, kWhite, TextAlign::kCenter), "CraftV");
		y += kBannerHeight * py;

		a_game.DrawBox(x, y, w, kSubtitleHeight * py, kBlack);
		a_game.DrawMenuText(x + kTextPad * px, y + kSubtitleTextTop * py, Style(kFontChalet, kItemScale, kWhite), "SETTINGS");
		char counter[16];
		std::snprintf(counter, sizeof(counter), "%d / %d", selected_ + 1, a_count);
		TextStyle right = Style(kFontChalet, kItemScale, kWhite, TextAlign::kRight);
		right.wrapRight = x + w - kTextPad * px;
		a_game.DrawMenuText(x + w - kTextPad * px, y + kSubtitleTextTop * py, right, counter);
		y += kSubtitleHeight * py;

		a_game.DrawSprite(kDict, "gradient_bgd", x, y, w, kRowHeight * py * static_cast<float>(a_count), kWhite);
		for (int i = 0; i < a_count; ++i) {
			const MenuRow& row = a_rows[i];
			const bool     selected = i == selected_;
			const float    top = y + kRowHeight * py * static_cast<float>(i);
			if (selected) {
				a_game.DrawSprite(kDict, "gradient_nav", x, top, w, kRowHeight * py, kWhite);
			}
			const Rgba text = selected ? kBlack : kWhite;
			a_game.DrawMenuText(x + kTextPad * px, top + kTextTop * py, Style(kFontChalet, kItemScale, text), row.label);
			TextStyle value = Style(kFontChalet, kItemScale, row.choice ? text : (selected ? kBlack : kGrey), TextAlign::kRight);
			if (row.choice && selected) {
				// < value >: the game's arrows either side of the value, as its own option lists draw it
				const float arrowRight = x + w - kArrowPad * px - kArrow * px;
				const float valueRight = arrowRight;
				value.wrapRight = valueRight;
				const float textWidth = a_game.TextWidth(value, row.value);
				a_game.DrawSprite(kDict, "arrowright", arrowRight, top + kArrowTop * py, kArrow * px, kArrow * py, kBlack);
				a_game.DrawSprite(kDict, "arrowleft", valueRight - textWidth - kArrow * px, top + kArrowTop * py, kArrow * px, kArrow * py, kBlack);
				a_game.DrawMenuText(valueRight, top + kTextTop * py, value, row.value);
			} else {
				value.wrapRight = x + w - kTextPad * px;
				a_game.DrawMenuText(x + w - kTextPad * px, top + kTextTop * py, value, row.value);
			}
		}
		y += kRowHeight * py * static_cast<float>(a_count) + kDescriptionGap * py;

		const MenuRow& current = a_rows[selected_ < a_count ? selected_ : 0];
		a_game.DrawBox(x, y, w, kDescriptionBar * py, kBlack);
		a_game.DrawSprite(kDict, "gradient_bgd", x, y + kDescriptionBar * py, w, kDescriptionHeight * py, kWhite);
		TextStyle description = Style(kFontChalet, kItemScale, kWhite);
		description.wrapLeft = x + kTextPad * px;
		description.wrapRight = x + w - kTextPad * px;
		a_game.DrawMenuText(x + kTextPad * px, y + kDescriptionBar * py + kDescriptionTextTop * py, description, current.description);
	}
}
