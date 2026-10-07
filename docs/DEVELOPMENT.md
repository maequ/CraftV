# CraftV: development notes

You play GTA V story mode. Your friends play **plain Minecraft Java Edition** on their own PCs and join a
world. Each friend shows up in your Los Santos and walks, runs and jumps where they do. The blocks they place
appear in Los Santos as solid objects. They walk around a blocky copy of the streets and hills around them,
built from GTA's own ground. Your GTA character shows up in their Minecraft and walks where you walk.

Your friends install nothing. On your PC, real Minecraft runs hidden next to GTA V and is the server they join.
A Fabric mod and a GTA V ASI plugin talk through shared memory.

> ⚠️ **Story mode only.** CraftV must never be used in GTA Online. Using mods online can get your
> Rockstar account banned. The plugin is designed to do nothing outside story mode, and Script Hook V
> closes the game if you go online. Don't try to work around either.

## Status

| Phase | What | State |
|---|---|---|
| 1 | Co-op on the Minecraft side with a fake GTA: friends join, walk on host terrain, everything reaches the host | **Done** (Sary joined in Phase 2's test) |
| 2 | GTA V plugin: link, overlay, scanning GTA's ground for friends | **Done, confirmed in Sary's game (2026-10-05)** |
| 3 | The Minecraft view: Sary plays as a Minecraft player in GTA (hand, hotbar, hearts, blocks), friends drawn with their skins | **Built and tested without the game; awaiting the in-game test** |
| 4 | Blocks are solid in GTA (invisible collision props) | **Built and tested without the game; awaiting the in-game test** |
| 4b | Two GTA players: a friend plays Minecraft inside his own GTA in the host's world (`gta/FRIEND.txt`) | **Built and tested with two Minecrafts and a simulated GTA; awaiting the in-game test** |
| 5 | Friends from anywhere (no port forwarding), whitelist | Not started |
| 6 | A better blocky Los Santos (buildings) | Not started |

Why things are built the way they are is in [docs/DECISIONS.md](DECISIONS.md).

## Repository

| Path | What |
|---|---|
| `docs/PROTOCOL.md` | The shared-memory spec (v1.1): the source of truth |
| `docs/TESTING.md` | How to test each phase |
| `docs/DECISIONS.md`, `docs/KNOWN_LIMITATIONS.md`, `docs/REFERENCE_NOTES.md` | Why things are the way they are |
| `protocol/cpp/`, `protocol/golden/` | C++ link library + tests, golden byte vectors both languages are tested against |
| `fabric/` | The Minecraft Fabric mod (Java 25, Minecraft 26.3) |
| `tools/mockhost/` | A fake GTA host with a fake Los Santos, for testing without the game |
| `gta/` | The GTA V ASI plugin (`CraftV.asi`, Script Hook V) and its default `CraftV.ini` |
| `rdr2/` | A parked RDR2 plugin from an earlier direction; its game-agnostic core is reused for GTA V |
| `host/` | The plugin core shared by the GTA V and RDR2 plugins (link, terrain scanner, friends, overlay) + tests |
| `tools/hostsim/` | The plugin core with a simulated GTA, for end-to-end tests against real Minecraft |
| `scripts/` | `build-native.ps1`, `dev-run.ps1`, `run-friend.ps1`, `test-all.ps1`, `deploy-gta.ps1`, `env.ps1`, `deploy-rdr2.ps1` |

## Requirements

- Windows 10/11, Git
- Visual Studio 2022 (or Build Tools) with "Desktop development with C++" (MSVC v143, Windows SDK, CMake)
- JDK 25. `scripts/env.ps1` finds `CRAFTV_JAVA_HOME`, `JAVA_HOME`, or the Minecraft Launcher's bundled Java 25
- Internet on first build (Gradle downloads Minecraft 26.3, Fabric Loader 0.19.5, Fabric API 0.161.0+26.3)
- Friends: Minecraft Java Edition 26.3, nothing else
- From Phase 2: GTA V **Legacy** (story mode), Script Hook V + its ASI loader from http://www.dev-c.com/gtav/scripthookv/
- From Phase 3: ReShade **with full add-on support** from https://reshade.me (installed as `ReShade64.asi`, see
  `gta/INSTALL.txt`). Building the compositor needs ReShade's add-on headers (crosire/reshade v6.8.0 `include/`) in
  `sdk/reshade-src`; without them CraftV.asi builds without the Minecraft view.

## Controls in GTA (the Minecraft view)

| Key | What it does |
|---|---|
| F7 | Minecraft view on/off (on by default whenever ReShade is loaded) |
| F8 | CraftV's settings menu (arrows to move and change, Backspace to close) |
| E | Minecraft's inventory (the mouse works it while it's open; Esc or E closes it) |
| Space | A straight Minecraft jump |
| Left mouse | Minecraft's attack: break blocks, hit. People in GTA in front of you get hurt and knocked back |
| Right mouse | Minecraft's use: place blocks, eat, use items |
| Mouse wheel, 1-9 | Minecraft's hotbar |

Walking, driving and the camera stay GTA's. GTA's own attack, aim and weapon keys are off while the view is on.

## Quick start

```powershell
.\scripts\test-all.ps1     # every automated test
.\scripts\dev-run.ps1      # mock host + the Minecraft friends join (opens to LAN on port 25565)
```

Then join from any Minecraft 26.3 on your network: Multiplayer → `CraftVDev - CraftV`. Settings (port, friends'
game mode, terrain radius) are in `fabric/run/config/craftv.properties`.

The JVM flag `--enable-native-access=ALL-UNNAMED` is required, because the link calls kernel32 through
`java.lang.foreign`. `gradlew runClient` already sets it.

## Credits

- **SkyCraft** by chasmlol (MIT): the architecture (hidden Minecraft + host plugin over shared memory),
  ring algorithm, liveness rules, and the mirror world preset. https://github.com/chasmlol/SkyCraft
- **PeakCraft** by aeironnsarmiento: the porting guide and lessons. https://github.com/aeironnsarmiento/PeakCraft
- **minecraft-gta5-passthrough** by rehan-remade (MIT): the GTA V reference.
  https://github.com/rehan-remade/universal-modder/tree/main/examples/minecraft-gta5-passthrough
- **Script Hook V** by Alexander Blade (used from Phase 2).
- **ReShade** by crosire (BSD-3-Clause add-on API headers; `ReShade.fxh`/`ReShadeUI.fxh` are CC0). The Minecraft
  view's compositor (`gta/src/compositor.cpp`), effect (`gta/shaders/CraftV.fx`), frame export and camera hooks are
  adapted from **minecraft-gta5-passthrough** by rehan-remade (MIT).
- **Script Hook V .NET** by crosire, kagikn and contributors (zlib): the GTA V surface material hash list.
  https://github.com/scripthookvdotnet/scripthookvdotnet

CraftV isn't affiliated with or endorsed by Mojang, Microsoft, Rockstar Games or Take-Two.
Minecraft and GTA V aren't included; you need your own copies.

License: MIT (see `LICENSE`, which also carries SkyCraft's license).
