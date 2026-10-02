# RedCraft: Minecraft inside Red Dead Redemption 2

Play RDR2's story mode as a Minecraft player: Minecraft movement, HUD, hotbar, placing and breaking
blocks on the frontier. Real Minecraft Java Edition runs hidden next to RDR2. A Fabric mod and an RDR2
ASI plugin talk through shared memory.

> ⚠️ **Story mode only.** RedCraft must never be used in Red Dead Online. Using mods online can get
> your Rockstar account banned. The plugin is designed to do nothing outside story mode, and
> ScriptHookRDR2 disables itself online. Don't try to work around either.

## Status

| Phase | What | State |
|---|---|---|
| 1 | Protocol + Fabric mod + mock host, no game needed | **Built and tested; awaiting Sary's test** |
| 2 | RDR2 ASI plugin handshake | Not started (needs RDR2) |
| 3 | Minecraft-style player and HUD | Not started |
| 4 | Place and break blocks in RDR2's world | Not started |
| 5 | Digging into the world (experimental) | Not started |

## Repository

| Path | What |
|---|---|
| `docs/PROTOCOL.md` | The shared-memory spec: the source of truth |
| `docs/TESTING.md` | How to test each phase |
| `docs/DECISIONS.md`, `docs/KNOWN_LIMITATIONS.md`, `docs/REFERENCE_NOTES.md` | Why things are the way they are |
| `protocol/cpp/` | C++ link library (used by the mock host now and the RDR2 plugin later) + tests |
| `protocol/golden/` | Golden byte vectors both languages are tested against |
| `fabric/` | The Minecraft Fabric mod (Java 25, Minecraft 26.3) |
| `tools/mockhost/` | A fake RDR2 host for testing without the game |
| `scripts/` | `build-native.ps1`, `dev-run.ps1`, `test-all.ps1`, `env.ps1` |

## Requirements (Phase 1)

- Windows 10/11, Git
- Visual Studio 2022 (or Build Tools) with "Desktop development with C++" (MSVC v143, Windows SDK, CMake)
- JDK 25. `scripts/env.ps1` finds `REDCRAFT_JAVA_HOME`, `JAVA_HOME`, or the Minecraft Launcher's bundled Java 25
- Internet on first build (Gradle downloads Minecraft 26.3, Fabric Loader 0.19.5, Fabric API 0.161.0+26.3)

Phase 2 will add: RDR2 for PC (story mode), ScriptHookRDR2 + an ASI loader from the official site.

## Quick start

```powershell
.\scripts\test-all.ps1     # every automated test
.\scripts\dev-run.ps1      # mock host + Minecraft: watch the player walk a circle
```

The JVM flag `--enable-native-access=ALL-UNNAMED` is required, because the link calls kernel32 through
`java.lang.foreign`. `gradlew runClient` already sets it.

## Credits

- **SkyCraft** by chasmlol (MIT): the architecture (hidden Minecraft + host plugin over shared memory),
  ring algorithm, liveness rules, mirror-world setup. https://github.com/chasmlol/SkyCraft
- **PeakCraft** by aeironnsarmiento: the porting guide and lessons. https://github.com/aeironnsarmiento/PeakCraft
- **Minecraft x GTA V** by rehan-remade (MIT): the closest Rockstar-engine reference.
  https://github.com/rehan-remade/universal-modder/tree/main/examples/minecraft-gta5-passthrough
- **ScriptHookRDR2** by Alexander Blade (used from Phase 2).

RedCraft isn't affiliated with or endorsed by Mojang, Microsoft, Rockstar Games or Take-Two.
Minecraft and RDR2 aren't included; you need your own copies.

License: MIT (see `LICENSE`, which also carries SkyCraft's license).
