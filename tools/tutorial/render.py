"""Draws the setup video's slides (1920x1080 PNG) with Pillow and Windows' own fonts: no browser, nothing downloaded.

Text markup inside strings: **bold**, `file or code`, {link} (green, monospace).
"""
import re

from PIL import Image, ImageDraw, ImageFont

W, H = 1920, 1080
LEFT, RIGHT, TOP = 140, 1780, 150
BG = (21, 25, 26)
PANEL = (31, 37, 38)
LINE = (58, 68, 69)
FG = (238, 242, 238)
MUTED = (154, 168, 160)
GRASS = (111, 191, 74)
GRASS_DARK = (90, 168, 60)
WARN = (242, 179, 61)
RED = (229, 88, 79)
FONTS = r"C:\Windows\Fonts"
_cache = {}


def font(kind, size):
    key = (kind, size)
    if key not in _cache:
        name = {"sans": "segoeui.ttf", "bold": "segoeuib.ttf", "semi": "seguisb.ttf", "mono": "consola.ttf", "monob": "consolab.ttf",
                "label": "bahnschrift.ttf"}[kind]
        f = ImageFont.truetype(f"{FONTS}\\{name}", size)
        if kind == "label":
            try:
                f.set_variation_by_name("Bold")
            except Exception:
                pass
        _cache[key] = f
    return _cache[key]


def tokens(text):
    """Splits markup into (word, style) pieces; spaces stay attached as separate tokens."""
    out = []
    for part in re.split(r"(\*\*.+?\*\*|`.+?`|\{.+?\})", text):
        if not part:
            continue
        if part.startswith("**"):
            style, body = "bold", part[2:-2]
        elif part.startswith("`"):
            style, body = "code", part[1:-1]
        elif part.startswith("{"):
            style, body = "link", part[1:-1]
        else:
            style, body = "plain", part
        if style in ("code", "link"):
            out.append((body, style))  # never broken across lines
        else:
            for w in re.split(r"(\s+)", body):
                if w:
                    out.append((w, style))
    return out


class Slide:
    def __init__(self, step=""):
        self.img = Image.new("RGB", (W, H), BG)
        self.d = ImageDraw.Draw(self.img)
        self._background()
        self.d.text((LEFT, 46), "CRAFTV", font=font("label", 34), fill=GRASS)
        self.d.text((LEFT + 150, 52), "SETUP GUIDE", font=font("label", 24), fill=MUTED)
        if step:
            w = self.d.textlength(step, font=font("label", 26))
            self.d.text((RIGHT - w, 50), step, font=font("label", 26), fill=MUTED)

    def _background(self):
        for x in range(0, W, 128):
            self.d.rectangle([x, 0, x + 63, H], fill=(24, 28, 29))
        glow = Image.new("RGB", (W, 340), BG)
        gd = ImageDraw.Draw(glow)
        for y in range(340):
            a = 1 - y / 340
            c = tuple(int(BG[i] + (GRASS[i] - BG[i]) * 0.10 * a) for i in range(3))
            gd.line([(0, y), (W, y)], fill=c)
        self.img.paste(Image.blend(self.img.crop((0, 0, W, 340)), glow, 0.6), (0, 0))
        for x in range(0, W, 36):
            self.d.rectangle([x, H - 18, x + 17, H], fill=GRASS)
            self.d.rectangle([x + 18, H - 18, x + 35, H], fill=GRASS_DARK)

    # ---- text ---------------------------------------------------------------------------------------
    def rich(self, x, y, text, size=40, color=FG, width=None, line=1.45):
        """Wrapped text with markup. Returns the y below it."""
        width = width or (RIGHT - x)
        lh = int(size * line)
        cx, cy = x, y
        space = self.d.textlength(" ", font=font("sans", size))
        pending = False
        for word, style in tokens(text):
            if style == "plain" and word.isspace():
                pending = cx > x
                continue
            f = {"plain": font("sans", size), "bold": font("bold", size), "code": font("mono", int(size * 0.88)),
                 "link": font("mono", int(size * 0.88))}[style]
            w = self.d.textlength(word, font=f) + (24 if style == "code" else 0)
            if pending and cx + space + w <= x + width:
                cx += space
            elif cx + w > x + width and cx > x:
                cx, cy = x, cy + lh
            pending = False
            if style == "code":
                self.d.rounded_rectangle([cx, cy + size * 0.12, cx + w, cy + size * 1.22], radius=8, fill=PANEL, outline=LINE, width=2)
                self.d.text((cx + 12, cy + size * 0.2), word, font=f, fill=FG)
            else:
                fill = GRASS if style == "link" else color
                self.d.text((cx, cy + (size * 0.1 if style == "link" else 0)), word, font=f, fill=fill)
            cx += w
        return cy + lh

    def badge(self, x, y, text):
        f = font("label", 28)
        w = self.d.textlength(text, font=f)
        self.d.rounded_rectangle([x, y, x + w + 36, y + 50], radius=6, fill=GRASS)
        self.d.text((x + 18, y + 8), text, font=f, fill=BG)
        return y + 50

    def title(self, x, y, text, size=96):
        for i, ln in enumerate(text.split("\n")):
            self.d.text((x, y + i * int(size * 1.08)), ln, font=font("bold", size), fill=FG)
        return y + len(text.split("\n")) * int(size * 1.08)

    def h2(self, x, y, text, size=68):
        self.d.text((x, y), text, font=font("bold", size), fill=FG)
        return y + int(size * 1.35)

    def num(self, x, y, n):
        self.d.text((x, y), str(n), font=font("label", 130), fill=GRASS)
        return y + 150

    # ---- blocks -------------------------------------------------------------------------------------
    def panel(self, box, color=PANEL, outline=LINE, width=2, radius=16):
        self.d.rounded_rectangle(box, radius=radius, fill=color, outline=outline, width=width)

    def bullets(self, x, y, items, size=38, width=None, gap=22):
        for it in items:
            self.d.rectangle([x, y + size * 0.42, x + 18, y + size * 0.42 + 18], fill=GRASS)
            y = self.rich(x + 44, y, it, size=size, width=(width or RIGHT - x) - 44) + gap - int(size * 0.45)
        return y

    def checks(self, x, y, items, size=40, width=None):
        for it in items:
            self.d.text((x, y), ">", font=font("label", size), fill=GRASS)
            y = self.rich(x + 50, y, it, size=size, width=(width or RIGHT - x) - 50) + 6
        return y

    def tree(self, box, lines):
        """lines: '/name' a folder (muted), '+name' new (green, NEW tag), 'name' an existing file. Leading spaces indent."""
        self.panel(box)
        x, y = box[0] + 44, box[1] + 36
        f = font("mono", 36)
        for ln in lines:
            indent = len(ln) - len(ln.lstrip(" "))
            s = ln.strip()
            kind = s[0] if s[0] in "/+" else ""
            name = s[1:] if kind else s
            cx = x + indent * 22
            col = MUTED if kind == "/" else GRASS if kind == "+" else FG
            self.d.text((cx, y), name, font=f, fill=col)
            if kind == "+":
                tx = cx + self.d.textlength(name, font=f) + 22
                self.d.text((tx, y + 8), "NEW", font=font("label", 24), fill=GRASS)
            y += 60

    def keys(self, x, y, rows, keyw=360):
        for k, desc in rows:
            f = font("mono", 34)
            w = self.d.textlength(k, font=f) + 44
            self.d.rounded_rectangle([x, y, x + w, y + 64], radius=12, fill=(42, 49, 50), outline=(74, 85, 86), width=2)
            self.d.rectangle([x + 2, y + 56, x + w - 2, y + 62], fill=(74, 85, 86))
            self.d.text((x + 22, y + 8), k, font=f, fill=FG)
            self.rich(x + keyw, y + 4, desc, size=38)
            y += 88
        return y

    def rename(self, x, y, old, new):
        f = font("mono", 70)
        self.d.text((x, y), old, font=f, fill=MUTED)
        w = self.d.textlength(old, font=f)
        self.d.line([(x, y + 44), (x + w, y + 44)], fill=RED, width=6)
        ax = x + w + 50
        self.d.text((ax, y - 4), ">>", font=font("label", 76), fill=FG)
        self.d.text((ax + 140, y), new, font=f, fill=GRASS)
        return y + 110

    def warn(self, box, head, paras):
        self.panel(box, color=(40, 36, 26), outline=WARN, width=5, radius=20)
        x, y = box[0] + 60, box[1] + 50
        self.d.text((x, y), head, font=font("bold", 68), fill=WARN)
        y += 110
        for i, p in enumerate(paras):
            y = self.rich(x, y, p, size=40, color=FG if i == 0 else MUTED, width=box[2] - box[0] - 120) + 18

    def menu(self, x, y, rows, selected, counter):
        w = 660
        self.d.rectangle([x, y, x + w, y + 126], fill=(36, 92, 150))
        for i in range(126):
            c = (int(44 - i * 0.1), int(111 - i * 0.2), int(179 - i * 0.3))
            self.d.line([(x, y + i), (x + w, y + i)], fill=c)
        t = "CraftV"
        f = font("bold", 76)
        self.d.text((x + (w - self.d.textlength(t, font=f)) / 2, y + 16), t, font=f, fill=FG)
        y += 126
        self.d.rectangle([x, y, x + w, y + 50], fill=(0, 0, 0))
        self.d.text((x + 18, y + 9), "SETTINGS", font=font("semi", 26), fill=FG)
        cw = self.d.textlength(counter, font=font("semi", 26))
        self.d.text((x + w - 18 - cw, y + 9), counter, font=font("semi", 26), fill=FG)
        y += 50
        for i, (label, value) in enumerate(rows):
            sel = i == selected
            self.d.rectangle([x, y, x + w, y + 52], fill=(236, 236, 236) if sel else (12, 14, 15))
            col = (17, 17, 17) if sel else FG
            self.d.text((x + 18, y + 10), label, font=font("sans", 27), fill=col)
            v = f"<  {value}  >" if sel else value
            vw = self.d.textlength(v, font=font("sans", 27))
            self.d.text((x + w - 18 - vw, y + 10), v, font=font("sans", 27), fill=col)
            y += 52
        return y

    def save(self, path):
        self.img.save(path, optimize=True)
