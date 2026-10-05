# Decisions

Newest last. Format: decision, reason, date.

### D-001: Target RDR2 instead of GTA V (2026-10-02)
**Decision:** Build "Minecraft inside Red Dead Redemption 2" (story mode). Working name CraftV;
the folder was renamed from `craftv/` the same day.
**Reason:** Minecraft-in-GTA V already exists ([rehan-remade/universal-modder](https://github.com/rehan-remade/universal-modder/tree/main/examples/minecraft-gta5-passthrough)).
Ports also exist for Skyrim, PEAK, Fallout 4 and Outer Wilds. Searches on 2026-10-02 found no
real-Minecraft port for RDR2 (only skins, a weapon pack, and RDR2-themed Minecraft modpacks). Sary
wants something that isn't done yet. RDR2 is RAGE-engine like GTA V and has ScriptHookRDR2 by the
same author, so the GTA V project is a close technical reference.

### D-002: Pin Minecraft 26.3, Fabric Loader 0.19.5, Fabric API 0.161.0+26.3, Java 25, Gradle 9.7.1, Loom 1.18 (2026-10-02)
**Reason:** These are exactly what SkyCraft 0.1.2 (commit `bfcaf17`) builds with, verified from its
`fabric/gradle.properties`, `build.gradle` and wrapper. The GTA V project uses the same versions.

### D-003: Our own protocol v1.0, built from SkyCraft's primitives (2026-10-02)
**Decision:** A header with a section table, two typed SPSC byte rings (host→MC, MC→host), fixed-size
versioned messages, per-record session tags, and symmetric create-or-open.
**Reason:** The brief asks for exactly this shape (HELLO/HEARTBEAT/PLAYER_STATE/BLOCK_*/LOG; either
side starts first). SkyCraft's v11 layout is a fixed set of Skyrim-specific regions, and only
Skyrim may create it. Kept from SkyCraft: u64 running head/tail on separate cache lines,
pad-to-wrap, skip-unknown-by-length, acquire/release, "magic written last", explicit SDDL,
heartbeat timeouts. New: session tags, so records left by a dead peer are dropped instead of
replayed. The section table lets SkyCraft-style regions (overlay, collision ring) be added later as
minor versions.

### D-004: Mock host in C++ sharing `protocol/cpp` with the future ASI (2026-10-02)
**Reason:** The brief says the RDR2 plugin should reuse the code the mock host proved. Writing the
mock in Python or Rust would prove a different implementation. The `craftv_link` static library
is the exact code `CraftV.asi` will link.

### D-005: Minecraft's link runs on its own thread (2026-10-02)
**Decision:** `LinkService` owns the Endpoint on a daemon thread (2 ms loop). Game threads only read
the latest PLAYER_STATE, take queued host block ops, and queue sends.
**Reason:** SkyCraft drives the link from the render thread, so world loading stalls its heartbeat.
PeakCraft's worst bug was a heartbeat thread stalled by a full ring. A dedicated thread keeps beating
through world loads and never blocks on the game. It also keeps each ring single-producer and
single-consumer.

### D-006: Phase 1 dev world is an auto-created superflat Creative world (2026-10-02)
**Reason:** In SkyCraft's void mirror world a host-driven player has nothing to look at, so you can't
see the circle. Superflat grass makes motion obvious. Grass top is y = -61, so feet are at -60, which
is the mock host's default circle height. Creative lets Sary break and place freely. The void
"mirror" preset comes back in Phase 2, where real RDR2 terrain matters.

### D-007: PLAYER_STATE is host→MC in Phase 1/2 ("puppet"); MC→host is reserved (2026-10-02)
**Reason:** The brief has the host drive the player in Phase 1. SkyCraft's model (MC physics drives
the host character) is one of the two Phase 3 options, so the reverse direction is reserved in the
spec now (v1.0 receivers reject it) and needs no protocol change later.

### D-008: Block message semantics (2026-10-02)
**Decision:** `BLOCK_SET` M→H is authoritative, from every server-side block change (a mixin on
`LevelChunk.setBlockState`, verified with javap). H→M is a host edit that MC applies and echoes back
flagged `ECHO`. Break/place requests are player intents in either direction: H→M (Phase 4) MC performs
them, M→H MC reports its own player's actions.
**Reason:** MC must stay the authority (brief §8), the mock's `setblock` needs an H→M path, and the
Phase 1 test wants the mock host to log block requests when Sary breaks or places blocks in MC.

### D-009: Build with VS 2022 Build Tools + bundled CMake; JDK 25 from the Minecraft Launcher runtime (2026-10-02)
**Reason:** Both are already on this PC, so nothing new needed installing. `scripts/env.ps1` looks for
`CRAFTV_JAVA_HOME`, then `JAVA_HOME`, then the launcher's `java-runtime-epsilon` (a full JDK 25.0.1
with javac). Installing Temurin 25 later is recommended, because the launcher may update or remove its runtime.

### D-010: Timeouts: MC gives the host 10 s, the host gives MC 3 s (2026-10-02)
**Reason:** SkyCraft uses 8 s and 3 s. RDR2's loading screens may pause script ticks (`ASSUMPTION`, to
measure in Phase 2), and Minecraft beats from its own thread. Both can be overridden for tests.

### D-011: Story-mode gate: any network signal switches the plugin off until the game restarts (2026-10-02)
**Decision:** Every tick, before touching the link, the plugin reads `NETWORK_IS_GAME_IN_PROGRESS`,
`NETWORK_IS_SESSION_STARTED` and `NETWORK_IS_IN_SESSION`. If any is true it detaches, draws nothing and
stays off for the rest of the process (`PluginState::kOnlineBlocked`).
**Reason:** Brief §2.1. ScriptHookRDR2 already closes the game when you go online. This is a second,
independent guard that latches, so a flicker can't turn CraftV back on. `// ASSUMPTION:` all three read
false in story mode. The debug overlay's last line shows them, so Sary's Phase 2 test verifies it.

### D-012: Build against the official ScriptHookRDR2 SDK; test with whichever hook runs on 1491.50 (2026-10-02)
**Decision:** `CraftV.asi` compiles against `ScriptHookRDR2_SDK_1.0.1207.73` from dev-c.com (kept in the
gitignored `sdk/` folder, because its readme forbids redistribution). Sary's game is build 1491.50. The official
runtime v1.0.1491.17 (Feb 2023) claims "1491.17 and above", but players report 1491.50 broke the official hook,
and the community "ScriptHookRDR2 V2" (kepmehz, Nexus mod 1472) targets 1491.50.
**Reason:** The SDK is the official ABI. Plugins built for it are meant to load in either runtime.
`// ASSUMPTION:` V2 exports the same functions (`scriptRegister`, `scriptWait`, `nativeInit`, `nativePush64`,
`nativeCall`, `scriptUnregister`; checked with dumpbin as our only imports besides kernel32/advapi32).
Which runtime to install is Sary's call (the brief prefers official sources).

### D-013: Plugin logic behind a game interface; SEH guard around the tick (2026-10-02)
**Decision:** `rdr2/src/core` (link, gate, coordinates, overlay, config, cost) talks to RDR2 only through
`IGame`. `rdr2/src/asi` implements it with natives. `HostPlugin::Tick` catches C++ exceptions, and
`main.cpp` wraps it in `__try/__except`. Either fault switches CraftV off and the game keeps running.
**Reason:** Everything except the natives is unit-tested without RDR2 (14 tests, including zero heap
allocations in a steady-state tick). Brief §6 safety: the plugin must never crash RDR2.

### D-014: Pivot to cross-game co-op in GTA V (2026-10-05)
**Decision:** Drop "Minecraft inside RDR2". New goal (docs/BRIEF.md): Sary plays GTA V story mode, friends in
plain vanilla Minecraft join the hidden Minecraft's world, appear in Los Santos as characters, and their blocks
appear as props. Friends walk on a blocky copy of Los Santos built from GTA's ground.
**Reason:** On 2026-10-04 Sary found [@Theyoungpixel's video](https://x.com/Theyoungpixel/status/2105758613520421303)
of Minecraft in RDR2, posted 2026-10-01 (video only, no release). The 2026-10-02 search missed it because it
didn't cover X. A search on 2026-10-05 (GitHub, Nexus, YouTube, TikTok, X) found ports of Skyrim, GTA V, GTA IV,
GTA SA, Fallout 4, Cyberpunk, Max Payne 2, R.E.P.O., PEAK and Zelda TP. All of them are either "you play game X
as a Minecraft player" alone, or SkyCraft-style multiplayer where every friend needs game X too. None lets a
friend in plain Minecraft appear inside the host game. GTA V over RDR2: Script Hook V is the most mature hook,
the GTA V reference project solves ground probing and block props, and our host core ports over almost unchanged.

### D-015: Name CraftV; the RDR2 plugin is parked as CraftV_RDR2 (2026-10-05)
**Decision:** Project, folder (`craftv/`), mod id, Java package (`dev.craftv`), C++ namespace, mapping name
(`Local\CraftV_Shared_v1`) and magic (`"CRFV"`, `0x56465243`) all say CraftV again. The RDR2 plugin stays in
`rdr2/` and builds `CraftV_RDR2.asi` / `CraftV_RDR2.ini`, leaving `CraftV.asi` for GTA V.
**Reason:** Sary picked the name (it was the original GTA V title). The brief says keep all existing work.
The magic changed with the name, so a RedCraft-era peer and a CraftV peer refuse each other's mapping instead
of mixing. Nothing was released under the old name, so there's no compatibility to keep.
