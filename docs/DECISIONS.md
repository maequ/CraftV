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

### D-006: Phase 1 dev world is an auto-created superflat Creative world (2026-10-02, superseded by D-019)
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

### D-016: Protocol v1.1: co-op messages, terrain pulled by Minecraft one chunk at a time (2026-10-05)
**Decision:** Six additive messages (PROTOCOL.md §7.8-7.13): REMOTE_PLAYER_JOIN/STATE/LEAVE, TERRAIN_REQUEST,
TERRAIN_PATCH, SESSION_INFO. Terrain is a fixed 1296-byte patch per 16x16 chunk column (ground Y, water Y and a
surface material per column), requested by MC for chunks near any player, nearest first, 32 in flight, retried
after 5 s. MC picks the blocks for each material; built chunks are remembered in the world save.
**Reason:** Friends wander away from the host's player, so the host must scan where they are, not just around
itself. Pull keeps the host stateless about the world. A chunk patch matches Minecraft's own unit and fits a
ring record. A material enum, not block ids, keeps GTA's side free of Minecraft knowledge. Minor version bump:
a v1.0 peer skips the new types and still links up.

### D-017: The hidden Minecraft is the server; opened to LAN automatically, authentication always on (2026-10-05)
**Decision:** Once the owner is in the world, the client thread calls `publishServer(LAN, commands off, port)`
(config `friends.port`, default 25565, a free port if busy). Friends get `friends.gameMode` (Creative for now).
Authentication stays on; only the dev-only system property `craftv.devNoAuth` (set solely by the stand-in-friend
test runs) turns it off.
**Reason:** Minecraft's own multiplayer carries everything, so CraftV writes no netcode (brief §1). Vanilla's
"Open to LAN" runs on the client thread, so this does too. Brief §2.4: friends must own Minecraft.

### D-018: Friends spawn next to the host's player and never fall into the void (2026-10-05)
**Decision:** On join, a friend is placed 2 blocks east of the owner, on the built ground there. While a friend's
chunk has no ground yet, they hover (flying is switched on); when it arrives they're set down. A friend below the
built ground (it arrived above them, or they dug through the bottom) is lifted back onto the surface.
**Reason:** The mirror world is void until the host's ground arrives, and the ground is only `terrain.depth`
blocks thick. Spawning beside the host is the point of co-op.

### D-019: Mirror world from SkyCraft's preset replaces the superflat dev world (2026-10-05)
**Decision:** The hidden Minecraft opens or creates the world "CraftV" from SkyCraft's `mirror` world preset and
dimension type (void, min_y -1024, height 2048), copied under `data/craftv/`. Supersedes D-006.
**Reason:** GTA's heights fit at 1 metre = 1 block, and all ground comes from the host. The preset is MIT and
proven on 26.3. Vanilla clients accept data-driven dimension types, so friends need nothing installed.

### D-020: A stand-in friend for automated co-op tests (2026-10-05)
**Decision:** A second dev client (`gradlew runFriend`, `scripts/run-friend.ps1`, its own `fabric/run-friend`
folder) joins the host like a Multiplayer player. With `-Bot` it walks, jumps, places and breaks blocks by itself.
The mock host serves a procedural fake Los Santos and prints friends and their blocks.
**Reason:** Brief §0: test everything possible before asking Sary. It also gives later phases a moving, building
friend without a second person. It loads the CraftV mod (in friend mode, doing nothing but joining and the
autopilot), so the final "plain vanilla Minecraft" check is Sary's own launcher (TESTING.md).

### D-021: One plugin core for every game: host/ (2026-10-05)
**Decision:** The game-agnostic plugin core (link, story-mode gate, coordinates, terrain scanner, friends,
overlay, config) lives in `host/core` and is linked by both `gta/` (CraftV.asi) and the parked `rdr2/`. Games
plug in through `IGame` (sample, draw, probe ground, request collision). Its tests run with a fake game.
**Reason:** Everything but the natives is tested without a game (brief §0), and moving to GTA V reused the RDR2
work unchanged.

### D-022: Terrain scanning: one chunk at a time, a probe budget per frame, collision requested for far chunks (2026-10-05)
**Decision:** `TerrainScanner` answers TERRAIN_REQUESTs in arrival order (Minecraft asks nearest first): 64
probes per frame by default (`[Terrain] ProbesPerTick`); chunks more than 150 m from the player first get
`REQUEST_COLLISION_AT_COORD` and a 10-frame wait; a chunk where nothing is hit isn't answered (Minecraft asks
again) until the third try, then it's answered with no ground; chunks outside the map are answered empty at
once. Each column is one straight-down ray from 1200 m to -250 m against map collision, plus a water-height check.
**Reason:** A bounded per-frame cost (brief §13), and GTA only has collision near the player. Probing from above
means friends stand on roofs and bridges, which is the "blocky copy" a friend sees; underneath bridges isn't
modelled yet (KNOWN_LIMITATIONS).

### D-026: Phase 3 is the passthrough: Sary plays as a Minecraft player in GTA (2026-10-05)
**Decision:** Phase 3 draws the hidden Minecraft into GTA's picture with a ReShade add-on, like
minecraft-gta5-passthrough (rehan-remade, MIT), instead of spawning GTA characters for friends. The camera,
frame export, compositor and effect are adapted from that project with credit. CraftV hides its blocky terrain in
Sary's view only, so friends keep walking on it.
**Reason:** Sary chose it ("me as Minecraft") after Phase 2 passed. It gives the Minecraft hand, hotbar and hearts
they asked for, and friends get drawn with their real skins and builds for free. Friends as GTA characters
becomes the fallback.

### D-023: GTA surface materials by name hash (2026-10-05)
**Decision:** A compile-time table of GTA V materials.dat names, hashed with RAGE's joaat, maps the shape test's
material hash to the protocol's 13 materials. Unknown hashes become stone and are logged once each.
**Reason:** No hash list to copy, and names are readable. `// ASSUMPTION:` the reported hash is joaat(name);
Phase 2's in-game test checks it (the overlay shows the material name).
**Revised 2026-10-05 after the first in-game run:** the assumption was wrong. GTA reported 0x10DD5498 for a road,
and joaat("tarmac") is 0x0DD8089A. The table now lists each name's hash explicitly, taken from Script Hook V .NET's
`MaterialHash` enum (zlib), where 0x10DD5498 is Tarmac. Names stay for the overlay and logs.

### D-024: Natives from the 2016 Script Hook V SDK, with a fallback for the material ray (2026-10-05)
**Decision:** `gta/` compiles against the official SDK v1.0.617.1a (gitignored; its readme forbids
redistribution). Its natives.h predates some names, so the synchronous ray `0x7EE9F5D83DD4F90E` is wrapped under
a clear name. If its result isn't ready at once, `ProbeGround` falls back to `GET_GROUND_Z_FOR_3D_COORD` (height
without material). `CraftV.asi` imports only ScriptHookV.dll, KERNEL32 and ADVAPI32.
**Reason:** The official SDK is the stable ABI (the runtime translates hashes per game build). The fallback keeps
friends' ground working even if the assumption about the ray is wrong.
**Revised 2026-10-05 after the first in-game run:** `0x7EE9F5D83DD4F90E` stayed pending (status 1) on every
probe, so all ground came from the fallback, without materials. It's the asynchronous START_SHAPE_TEST_LOS_PROBE.
The SDK's `_CAST_RAY_POINT_TO_POINT` (`0x377906D8A31E5586`, START_EXPENSIVE_SYNCHRONOUS_SHAPE_TEST_LOS_PROBE) was
ready on the first poll with hit, height and material (tarmac), so `ProbeGround` uses that now.

### D-025: hostsim: the plugin core against real Minecraft without GTA (2026-10-05)
**Decision:** `tools/hostsim` runs `HostPlugin` with a simulated game (a player walking on the mock's fake map,
probes reading that map with GTA material names) and prints the overlay to the console.
**Reason:** It tests everything CraftV.asi does except the natives, end to end with the real Minecraft and the
stand-in friend (brief §0).
