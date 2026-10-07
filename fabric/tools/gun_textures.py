# Draws CraftV's gun item textures (16x16, Minecraft style) from the ASCII maps below into the mod's assets.
# Run from the repo root after changing a map:  python fabric/tools/gun_textures.py
import pathlib

from PIL import Image

OUT = pathlib.Path(__file__).resolve().parents[1] / "src/main/resources/assets/craftv/textures/item"
PALETTE = {
    ".": None,
    "k": (24, 24, 26), "d": (48, 49, 53), "m": (78, 80, 86), "l": (116, 119, 126), "h": (160, 164, 170),
    "w": (92, 58, 30), "W": (128, 84, 44), "o": (74, 84, 38), "O": (104, 116, 54),
    "r": (150, 36, 30), "R": (204, 70, 44), "y": (222, 178, 60), "g": (58, 92, 40), "G": (86, 128, 58),
}
GUNS = {
    "pistol": [
        "................",
        "................",
        "................",
        "................",
        "...hhhhhhhhhh...",
        "...lmmmmmmmmmk..",
        "...dddddddddkk..",
        "...dddkddd......",
        "...kd.k.dd......",
        "........ddk.....",
        "........ddk.....",
        ".........ddk....",
        ".........ddk....",
        ".........kkk....",
        "................",
        "................",
    ],
    "smg": [
        "................",
        "................",
        "................",
        "......hh........",
        "..hhhhhhhhhhhh..",
        "..lmmmmmmmmmmmkk",
        "..dddddddddddk..",
        "dddddk.dd.dk....",
        "dk.....dd.dk....",
        ".......dd.dk....",
        ".......kk.dk....",
        "..........dk....",
        "..........dk....",
        "..........kk....",
        "................",
        "................",
    ],
    "assault_rifle": [
        "................",
        "................",
        "................",
        "........hh......",
        ".WW.hhhhhhhhhhh.",
        "WWwwlmmmmmmmmmmk",
        "wwwwddddddddddk.",
        "ww.ww.dd.dkkk...",
        "w...w.dd..dk....",
        "......kk..dk....",
        "..........dk....",
        "...........dk...",
        "...........kk...",
        "................",
        "................",
        "................",
    ],
    "shotgun": [
        "................",
        "................",
        "................",
        "................",
        ".WW.hhhhhhhhhhhh",
        "WWwwmmmmmmmmmmmk",
        "wwwwddddWWWWWddk",
        "ww.wd.kkwwwwwk..",
        "w...ddk.........",
        "....kk..........",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
    ],
    "sniper_rifle": [
        "................",
        "................",
        "......kmmmk.....",
        "......kdddk.....",
        ".OO....k.k......",
        "OOoohhhhhhhhhhhh",
        "oooommmmmmmmmmmk",
        "oo.oddddk.......",
        "o...dd.k........",
        "....kk..........",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
    ],
    "rpg": [
        "................",
        "................",
        "................",
        "............RR..",
        "...........RRRR.",
        "OOOOOOOOOOORRRRR",
        "oooooooooooRRRRr",
        "kooooooooooRRRr.",
        ".....kk.kk..rr..",
        ".....dk.dk......",
        ".....kk.kk......",
        "................",
        "................",
        "................",
        "................",
        "................",
    ],
    "minigun": [
        "................",
        "................",
        "................",
        "...mmmmmmmmmmmmk",
        "..lhhhhhhhhhhhhk",
        ".dlmmmmmmmmmmmmk",
        "ddlhhhhhhhhhhhhk",
        "ddlmmmmmmmmmmmmk",
        ".dddddkk........",
        "...ddk.yy.......",
        "...kk..yy.......",
        "........y.......",
        "................",
        "................",
        "................",
        "................",
    ],
    "grenade": [
        "................",
        "................",
        "......kk........",
        ".....kmmkk......",
        "......kk.k......",
        ".....gggg.k.....",
        "....gGGGgg......",
        "...gGgGgGgg.....",
        "...ggGgGgGg.....",
        "...gGgGgGgg.....",
        "...ggGgGgGg.....",
        "....gggggg......",
        ".....gggg.......",
        "................",
        "................",
        "................",
    ],
}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name, rows in GUNS.items():
        assert len(rows) == 16 and all(len(r) == 16 for r in rows), name
        img = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
        for y, row in enumerate(rows):
            for x, ch in enumerate(row):
                c = PALETTE[ch]
                if c:
                    img.putpixel((x, y), c + (255,))
        img.save(OUT / f"{name}.png")
    print("wrote", len(GUNS), "textures to", OUT)


if __name__ == "__main__":
    main()
