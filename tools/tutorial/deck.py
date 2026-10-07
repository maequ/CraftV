"""The setup video's slides, drawn with render.py. The narration ("say") is in slides.py, in the same order."""
from render import LEFT, RIGHT, TOP, Slide


def s_title():
    s = Slide()
    y = s.badge(LEFT, 260, "GTA V STORY MODE + MINECRAFT")
    y = s.title(LEFT, y + 40, "CraftV\nsetup guide")
    s.rich(LEFT, y + 30, "Play Minecraft inside GTA V, with your friends. This video takes you from nothing to playing.", size=42,
           color=(154, 168, 160), width=1400)
    return s


def s_warning():
    s = Slide("BEFORE YOU START")
    s.warn((LEFT, 230, RIGHT, 760), "Story mode only", [
        "Never use mods in GTA Online. You can get your Rockstar account banned.",
        "CraftV turns itself off if it sees an online session, and Script Hook V closes the game if you go online.",
    ])
    return s


def s_need():
    s = Slide("WHAT YOU NEED")
    y = s.h2(LEFT, TOP, "What you need")
    s.bullets(LEFT, y, [
        "GTA V **Legacy**, your own copy",
        "Script Hook V   {dev-c.com/gtav/scripthookv}",
        "ReShade, the **add-on** version   {reshade.me}",
        "Minecraft Java Edition   {minecraft.net}",
        "Fabric for 26.3   {fabricmc.net/use/installer}",
        "Fabric API   {modrinth.com/mod/fabric-api}",
        "CraftV   {Releases on the GitHub page}",
    ])
    return s


def step_slide(step, n, heading, text, tree=None, checks=None, extra=None):
    s = Slide(step)
    split = tree or checks
    width = 820 if split else RIGHT - LEFT
    y = s.num(LEFT, TOP - 10, n)
    y = s.h2(LEFT, y + 10, heading)
    y = s.rich(LEFT, y, text, size=40, width=width)
    if extra:
        extra(s, y)
    if tree:
        s.tree((1040, 260, RIGHT, 260 + 72 + 60 * len(tree)), tree)
    if checks:
        box = (1040, 280, RIGHT, 280 + 80 + 70 * len(checks))
        s.panel(box)
        s.checks(1084, 316, checks, size=38, width=RIGHT - 1084 - 30)
    return s


def s_rename():
    s = Slide("PART 1 OF 3: GTA V")
    y = s.num(LEFT, TOP - 10, 4)
    y = s.h2(LEFT, y + 10, "Rename one file")
    y = s.rich(LEFT, y, "In your GTA V folder:", size=40)
    y = s.rename(LEFT, y + 50, "dxgi.dll", "ReShade64.asi")
    s.rich(LEFT, y + 30, "GTA V doesn't load ReShade's dxgi.dll. Script Hook V loads anything that ends in .asi.", size=40,
           color=(154, 168, 160), width=1500)
    return s


def s_folder():
    def extra(s, y):
        s.rich(LEFT, y + 30, "Pick a new, empty folder, for example:", size=40)
        s.rich(LEFT, y + 100, "{C:\\Users\\YOU\\AppData\\Roaming\\.minecraft-craftv}", size=40)
        s.rich(LEFT, y + 190, "Your normal Minecraft and its settings stay as they are.", size=40, color=(154, 168, 160))

    return step_slide("PART 2 OF 3: MINECRAFT", 7, "Its own folder",
                      "Launcher: **Installations** > `fabric-loader-26.3` > **Edit** > **Game directory**.", extra=extra)


def s_settings_gta():
    s = Slide("PART 3 OF 3: GTA SETTINGS")
    y = s.num(LEFT, TOP - 10, 9)
    y = s.h2(LEFT, y + 10, "GTA settings")
    y = s.rich(LEFT, y, "Settings > Graphics:", size=40)
    s.panel((LEFT, y + 30, 1500, y + 30 + 260))
    s.checks(LEFT + 44, y + 66, ["Screen Type: **Windowed Borderless**", "Pause Game On Focus Loss: **Off**",
                                 "Depth of Field: Off  (recommended)"], size=40)
    return s


def s_play():
    s = Slide("PLAY")
    y = s.h2(LEFT, TOP, "Every time you play")
    for i, t in enumerate(["Start **fabric-loader-26.3** in the Minecraft Launcher", "Wait for the **CraftV** world to open, and leave it open",
                           "Start GTA V and load story mode", "The CraftV box (top right) says **MC view**"]):
        s.d.text((LEFT, y + i * 140 - 10), str(i + 1), font=__import__("render").font("label", 96), fill=(111, 191, 74))
        s.rich(LEFT + 110, y + i * 140 + 14, t, size=42)
    return s


def s_controls():
    s = Slide("CONTROLS")
    y = s.h2(LEFT, TOP, "Controls")
    s.keys(LEFT, y + 20, [("Left mouse", "Break blocks, hit people and cars"), ("Right mouse", "Place blocks, eat, bow, light TNT"),
                     ("1-9 / wheel", "Hotbar"), ("E", "Inventory (Esc closes it)"), ("Space", "Minecraft jump"),
                     ("F7", "Minecraft view on and off"), ("F8", "CraftV settings")])
    return s


def s_menu():
    s = Slide("SETTINGS")
    y = s.h2(LEFT, TOP, "F8 settings")
    y = s.rich(LEFT, y, "Crosshair, hand, HUD, block outlines, frame rate, Steve in cars, jump, GTA damage, TNT, arrows, torch light, "
                        "inventory key and more.", size=40, width=820)
    s.rich(LEFT, y + 40, "Low FPS? Lower **Minecraft quality** and **frame rate**.", size=40, color=(154, 168, 160), width=820)
    s.menu(1080, 230, [("Minecraft view", "On"), ("Minecraft quality", "High"), ("Minecraft frame rate", "90"), ("Crosshair", "On"),
                       ("Hand", "On"), ("Block outlines", "Placed blocks"), ("Steve in cars", "On"), ("Jump", "Minecraft"),
                       ("GTA damage", "On")], 2, "3 / 20")
    return s


def s_friends():
    s = Slide("FRIENDS")
    y = s.h2(LEFT, TOP, "Playing with friends")
    half = (RIGHT - LEFT - 50) // 2
    for i, (head, body) in enumerate([("Plain Minecraft 26.3", "Multiplayer > Direct Connect > your address (F8 > Friends join at)."),
                                      ("Their own GTA V", "Same three parts, then your address after `guest.join=` in `craftv.properties`.")]):
        x = LEFT + i * (half + 50)
        s.panel((x, y, x + half, y + 330))
        s.rich(x + 40, y + 34, f"**{head}**", size=42)
        s.rich(x + 40, y + 110, body, size=36, color=(154, 168, 160), width=half - 80)
    s.rich(LEFT, y + 380, "Not on your Wi-Fi? Forward TCP port **25565** on your router and give them your public IP.", size=40,
           color=(154, 168, 160))
    return s


def s_problems():
    s = Slide("PROBLEMS")
    y = s.h2(LEFT, TOP, "If something's wrong")
    s.bullets(LEFT, y, [
        "No CraftV box: Script Hook V files or CraftV.asi aren't next to GTA5.exe",
        "Box but no Minecraft: rename to ReShade64.asi, use the add-on version, start Minecraft first",
        "Freezes when you click away: Pause Game On Focus Loss = **Off**",
        "Still stuck: open an issue on GitHub with CraftV.log and ReShade.log",
    ], size=36)
    return s


def s_end():
    s = Slide()
    y = s.badge(LEFT, 300, "HAVE FUN")
    y = s.title(LEFT, y + 40, "You're set up.")
    s.rich(LEFT, y + 30, "Links, settings and the full guide are on the GitHub page. Story mode only.", size=42, color=(154, 168, 160), width=1400)
    return s


DECK = [
    s_title,
    s_warning,
    s_need,
    lambda: step_slide("PART 1 OF 3: GTA V", 1, "Script Hook V",
                       "Open the zip, go into the `bin` folder, and copy two files into your GTA V folder, the one with `GTA5.exe`.",
                       tree=["/Grand Theft Auto V\\", "  GTA5.exe", "  +ScriptHookV.dll", "  +dinput8.dll"]),
    lambda: step_slide("PART 1 OF 3: GTA V", 2, "CraftV files", "Unzip CraftV, then copy `CraftV.asi` and `CraftV.ini` into the same folder.",
                       tree=["/Grand Theft Auto V\\", "  GTA5.exe", "  ScriptHookV.dll", "  dinput8.dll", "  +CraftV.asi", "  +CraftV.ini"]),
    lambda: step_slide("PART 1 OF 3: GTA V", 3, "ReShade", "Download the version **with full add-on support**. The normal version won't work.",
                       checks=["Pick `GTA5.exe`", "Pick **DirectX 10/11/12**", "Untick every effect package", "Finish"]),
    s_rename,
    lambda: step_slide("PART 1 OF 3: GTA V", 5, "The Minecraft view",
                       "Open `Minecraft view (ReShade)` in the CraftV zip and copy everything inside it into your GTA V folder.",
                       tree=["/Grand Theft Auto V\\", "  ReShade64.asi", "  +ReShade.ini", "  +ReShadePreset.ini", "  +reshade-shaders\\"]),
    lambda: step_slide("PART 2 OF 3: MINECRAFT", 6, "Fabric",
                       "Run the Fabric installer, pick Minecraft **26.3**, and click Install. A `fabric-loader-26.3` installation shows up "
                       "in the Minecraft Launcher."),
    s_folder,
    lambda: step_slide("PART 2 OF 3: MINECRAFT", 8, "The mods", "Make a `mods` folder in that game directory and put two files in it.",
                       tree=["/.minecraft-craftv\\", "  /mods\\", "    +fabric-api-0.161.0+26.3.jar", "    +craftv-0.1.0.jar"]),
    s_settings_gta,
    s_play,
    s_controls,
    s_menu,
    s_friends,
    s_problems,
    s_end,
]
