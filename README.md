# CraftV: play Minecraft inside GTA V, together

CraftV puts **real Minecraft** into **GTA V story mode**. You walk Los Santos as Steve, with Minecraft's
hand, hotbar, hearts and hunger. You build on the streets, blow up cars with TNT and shoot people with a bow.
Your friends join you: in plain Minecraft, or in **their own GTA**, where each of you sees the other's Minecraft
character, and everything either of you builds, in Los Santos.

> [!WARNING]
> **Story mode only.** Never take mods into GTA Online: using mods online can get your Rockstar account
> banned. CraftV switches itself off the moment it sees an online session, and Script Hook V closes the game
> if you go online. Don't try to work around either.

> [!NOTE]
> **Early version.** CraftV is new, and not everything on this page has been tried in a real game yet. See
> [What works](#what-works) for what has, and please report what you find.

## What it is

CraftV runs a real Minecraft Java Edition (with the CraftV Fabric mod) next to GTA V. A small GTA V plugin
(`CraftV.asi`) and a ReShade add-on draw Minecraft's picture into GTA's, matched to GTA's camera and depth:
Minecraft things stand in front of and behind GTA's cars and buildings correctly. GTA moves you; Minecraft
draws you.

- **You are Steve.** Your Minecraft body walks, runs, sits in cars and holds up a phone when you use yours. You
  see your Minecraft hand, hotbar, hearts and hunger.
- **Survival.** You start with a kit (diamond sword and pickaxe, bow and arrows, TNT, flint and steel, blocks,
  torches, food, redstone). Running costs hunger, and you eat with right click. Getting hurt in GTA costs
  Minecraft hearts, and dying in Minecraft gets you wasted in GTA.
- **The world reacts.**
  - Your sword hurts and knocks back GTA's people (some run, some fight back) and dents cars.
  - Arrows hit people.
  - TNT explodes in GTA too.
  - Torches light up the streets at night.
  - Space is a straight Minecraft jump.
- **Building.** Blocks go on GTA's streets and roofs. They're solid in GTA: people, cars and you bump into them,
  and you can stand on them. GTA's own ground stays GTA's.
- **Friends.**
  - With **plain Minecraft 26.3**, a friend joins your world and walks a blocky copy of Los Santos built from
    GTA's own ground.
  - With **their own GTA V and CraftV**, a friend plays exactly like you, in your world: you see each other as
    Minecraft characters in your own Los Santos.
- **Settings in GTA's style.** Press F8 for CraftV's menu, drawn like GTA's own menus. It has the crosshair,
  hand, HUD, frame rate, jump, damage, TNT, torch light, inventory key and more.

## What works

Tried in a real GTA V (Legacy, build 3725) so far:

- Minecraft drawn into GTA, with Minecraft's hand, hotbar and HUD over GTA's picture.
- Placing blocks on GTA's streets.
- Melee hits on GTA's people.
- A plain-Minecraft friend joining the world.
- The GTA ground copy.

Built and tested without the game (automated tests, two Minecrafts on one PC, a simulated GTA), and not yet tried
in a real GTA:

- Steve in cars and the phone pose.
- The Minecraft jump.
- GTA damage turning into hearts, and wasted on death.
- TNT and arrows in GTA.
- Torch light.
- Solid blocks.
- The inventory.
- The F8 menu.
- Two GTA players.

## You need

| | Where to get it (official sources only) |
|---|---|
| GTA V **Legacy**, story mode | Your own copy (Steam, Rockstar Games Launcher or Epic) |
| Script Hook V | http://www.dev-c.com/gtav/scripthookv/ |
| ReShade **with full add-on support** | https://reshade.me |
| Minecraft Java Edition | Your own account, https://www.minecraft.net |
| Fabric Loader for Minecraft 26.3 | https://fabricmc.net/use/installer/ |
| Fabric API 0.161.0+26.3 | https://modrinth.com/mod/fabric-api |
| CraftV | This repository's [Releases](../../releases) page |

A PC that runs GTA V and Minecraft at the same time. Windows 10 or 11.

## Install

<details open>
<summary><b>1. Minecraft (the hidden half)</b></summary>

1. Run the Fabric installer, choose Minecraft **26.3**, and click Install. A `fabric-loader-26.3` installation
   appears in the Minecraft Launcher.
2. In the launcher, go to Installations > `fabric-loader-26.3` > Edit > **Game directory**, and pick a new, empty
   folder (for example `%APPDATA%\.minecraft-craftv`). CraftV changes that Minecraft's settings; your normal
   Minecraft stays as it is.
3. Put `fabric-api-0.161.0+26.3.jar` and `craftv-0.1.0.jar` (from the release's `Minecraft mod` folder) into that
   folder's `mods` folder.

When it starts, this Minecraft opens a world called **CraftV** by itself. It runs next to GTA, so keep it open.
</details>

<details open>
<summary><b>2. GTA V</b></summary>

1. Install Script Hook V: copy `ScriptHookV.dll` and `dinput8.dll` from its `bin` folder into the GTA V folder
   (the one with `GTA5.exe`).
2. Copy `CraftV.asi` and `CraftV.ini` from the release into the same folder.
3. Install ReShade (the **add-on** version). Pick `GTA5.exe`, choose DirectX 10/11/12, and untick every effect
   package. Then rename the `dxgi.dll` it made to **`ReShade64.asi`** (Script Hook V's loader loads it that way).
4. Copy everything in the release's `Minecraft view (ReShade)` folder into the GTA V folder: `ReShade.ini`,
   `ReShadePreset.ini` and `reshade-shaders`.
5. In GTA's settings, go to Graphics and set **Screen Type: Windowed Borderless** and **Pause Game On Focus
   Loss: Off**. Turning Depth of Field off is recommended.
</details>

<details>
<summary><b>3. A friend in their own GTA</b></summary>

Your friend does steps 1 and 2 on their PC. Before the first start, they copy the release's
`Minecraft mod/For a friend/config` folder into their Minecraft game directory and write your address after `guest.join=` in
`config/craftv.properties`. Their Minecraft then joins your world instead of hosting one.

Your address is in GTA: F8 > **Friends join at**. On the same Wi-Fi that's all. Over the internet, forward TCP
port **25565** on your router to your PC and give your friend your public IP (`1.2.3.4:25565`). Only share it
with people you trust.

A friend with **plain Minecraft 26.3** just uses Multiplayer > Direct Connect with the same address.
</details>

## Play

1. Start the CraftV Minecraft from the launcher and wait for the world.
2. Start GTA V and load story mode. The CraftV box (top right) says **MC view**.

| Key | What it does |
|---|---|
| Left mouse | Minecraft's attack: break blocks; hits hurt GTA's people and dent cars |
| Right mouse | Place blocks, eat, draw the bow, light TNT, flip levers |
| Mouse wheel, 1-9 | Hotbar |
| E | Minecraft's inventory, worked with the mouse (Esc closes it) |
| Space | A straight Minecraft jump |
| F7 | Minecraft view on and off |
| F8 | CraftV's settings |

Walking, driving and the camera are GTA's. GTA's own weapons are off while the Minecraft view is on.

## Settings (F8)

| Setting | Choices |
|---|---|
| Minecraft view | On, Off |
| Minecraft quality | Low to Max (how sharp; lower runs faster) |
| Minecraft frame rate | 60, 90, 120, 144, Unlimited (higher is smoother when you turn) |
| Crosshair, Hand, Minecraft HUD | On, Off |
| Block outlines | Off, Placed blocks, Everywhere |
| Steve in cars | On (Steve sits in the car), Off (GTA's driver shows) |
| Jump | Minecraft, GTA |
| GTA damage | On (GTA hurts your hearts), Off |
| TNT in GTA, Arrows hurt people, Torch light | On, Off |
| Inventory key | E, Tab, I |
| Hit strength | Weak, Normal, Strong, Brutal |
| Hide GTA HUD, CraftV overlay, Overlay details, Ground scanning | |

Everything is also in `CraftV.ini`, next to `CraftV.asi`.

## Known limitations

- Only Minecraft is shared between friends. Each GTA has its own people, cars and traffic.
- GTA's ground is copied in whole blocks, so on slopes Minecraft things can float or sink by up to half a block.
- Indoors and under bridges, the ground copy is the roof. Blocks placed there land on the roof.
- At most 400 blocks near you are solid in GTA (GTA crashes with too many objects).
- Two games render at once, so expect a lower frame rate.

The full list is in [docs/KNOWN_LIMITATIONS.md](docs/KNOWN_LIMITATIONS.md).

## Problems?

Open an issue with your `CraftV.log` (next to `GTA5.exe`), `ReShade.log` (same folder) and `craftv-fabric.log`
(the `logs` folder of the CraftV Minecraft game directory).

## Building from source

See [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md). Short version: Visual Studio 2022 with C++, JDK 25, the Script
Hook V SDK in `sdk/ScriptHookV_SDK`, ReShade's add-on headers in `sdk/reshade-src`, then
`scripts\build-native.ps1`, `cd fabric; .\gradlew build`, and `scripts\make-dist.ps1`.

## Credits

- **minecraft-gta5-passthrough** by rehan-remade (MIT): the Minecraft-into-GTA compositor, effect, frame export
  and camera hooks CraftV's view is adapted from.
  https://github.com/rehan-remade/universal-modder/tree/main/examples/minecraft-gta5-passthrough
- **SkyCraft** by chasmlol (MIT): the architecture (a hidden Minecraft next to the game, talking over shared
  memory). https://github.com/chasmlol/SkyCraft
- **PeakCraft** by aeironnsarmiento: porting notes. https://github.com/aeironnsarmiento/PeakCraft
- **Script Hook V** by Alexander Blade. **ReShade** by crosire (add-on API headers BSD-3-Clause; `ReShade.fxh` and
  `ReShadeUI.fxh` CC0). **Script Hook V .NET** (zlib): GTA's surface material list.

CraftV isn't affiliated with or endorsed by Mojang, Microsoft, Rockstar Games or Take-Two. Minecraft and GTA V
aren't included; you need your own copies.

License: MIT (see [LICENSE](LICENSE)).
