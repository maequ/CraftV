# Reference notes: SkyCraft and PeakCraft

Studied 2026-10-02 from shallow clones in `reference/` (gitignored, not shipped):

| Repo | Commit | License |
|---|---|---|
| [chasmlol/SkyCraft](https://github.com/chasmlol/SkyCraft) | `bfcaf17` (0.1.2, 2026-10-01) | MIT, (c) 2026 chasmlol |
| [aeironnsarmiento/PeakCraft](https://github.com/aeironnsarmiento/PeakCraft) | `d07030e` (2026-10-02) | MIT (inherits SkyCraft's) |

Key files read: SkyCraft `README.md`, `docs/DESIGN.md`, `protocol/skycraft_protocol.h`,
`fabric/` (Gradle files, `link/Proto.java`, `link/SkyLink.java`, `SkyCraft.java`,
`client/SkyCraftClient.java`, `client/SkyClient.java`, `client/MirrorWorld.java`), `skse/src/Link.cpp`;
PeakCraft `docs/PORTING-GUIDE.md`, `docs/solutions/.../porting-a-two-game-shared-memory-link-to-a-new-host.md`,
`docs/PEAKCRAFT-PROGRESS.md`, `peak/src/PeakCraft/Link/Rings.cs`, and the diff of its `fabric/` against SkyCraft's.

## 1. The architecture in one paragraph

Real Minecraft Java 26.3 runs alongside the host game with a Fabric mod (the **guest**). The host
game gets a native plugin (the **host**). The two talk only through one named Windows file
mapping (`Local\SkyCraft_v1`, about 191 MB). No sockets, no serialisation library: fixed-size
little-endian structs at fixed offsets. **Minecraft is authoritative for the player** (physics,
inventory, health, blocks). The host is authoritative for the world (terrain, NPCs, scripts, saves).
The host tells the guest what the world is shaped like (collision triangles), what the keyboard did,
and where NPCs are. The guest tells the host where the player and camera are, what to draw (HUD
frames and block meshes), and what the player hit. PeakCraft's main claim is that a new host
needs **no guest change and no protocol change**. Its port only made four small optional changes
to the Fabric mod.

## 2. Shared-memory layout (SkyCraft protocol v11)

`protocol/skycraft_protocol.h` is SkyCraft's source of truth and `fabric/.../link/Proto.java` mirrors it by hand.
Skyrim creates the mapping; Minecraft only ever opens it (`OpenFileMappingW`, retried once a second).

| Offset | Region | Shape | Direction |
|---|---|---|---|
| `0x0` | `Header` {magic `"SKYC"`, version, skyrimPid, mcPid, skyrimHeartbeatMs, mcHeartbeatMs} | plain | both (each writes its own fields) |
| `0x100` | `SkyState` (flags, worldId, pos, yaw/pitch, teleportSeq, viewport, gameHour) | seqlock | host → MC |
| `0x200` | `McState` (flags, pos, look, eye, fov, bob, raw 20 Hz tick pair + QPC stamp, camera mode) | seqlock | MC → host |
| `0x300` | Overlay control word + 3 slot headers | triple buffer | MC → host |
| `0x400` | `WaterGrid` (16x16 water surface heights) | seqlock | host → MC |
| `0x1000` | Input ring (4096 x 16-byte `InputEvent`: SDL keys, buttons, scroll, cursor, text, hurt) | entry ring | host → MC |
| `0x12000` | `ActorTable` (256 nearby NPCs as hittable proxies) | seqlock | host → MC |
| `0x17000` | Event ring (512 x 32-byte `McEvent`: hit actor, died, explosion, arrow stuck, skill use) | entry ring | MC → host |
| `0x1C000` | `WorldEntities` (arrows, dropped items, cracks, block outline) | seqlock | MC → host |
| `0x20000` | Collision ring (32 MB: `ColTris`, `ColRegion` 1/8-block occupancy, `ColClear` epoch) | byte ring | host → MC |
| after | Overlay pixels (3 x 3840x2160 RGBA) | triple buffer | MC → host |
| after | Render ring (64 MB: atlas, section meshes, avatar, scene, lights) | byte ring | MC → host |

### The four shapes (all reusable as concepts)

- **Seqlock slot:** the writer bumps `seq` to odd, writes the fields, then bumps it to even (release).
  The reader copies, then accepts only if `seq` is even and unchanged. On a torn read it keeps the
  previous value.
- **Entry ring:** SPSC, `head`/`tail` are running u64 counts on separate 64-byte cache lines
  (`+0x00` and `+0x40`), with data at `+0x80`. Index = `count & (entries-1)`. The producer reads
  `tail` with acquire and publishes `head` with release. The consumer does the mirror image. If the
  consumer is lapped it drops the oldest entries.
- **Byte ring:** variable-size messages, 8-byte aligned, each with an 8-byte `{u32 type, u32 payloadBytes}` header.
  A message never straddles the end. Instead a `type 0` pad means "skip to the start". Unknown types are
  skipped by length. Messages larger than half the ring are refused.
- **Triple buffer:** used for the HUD image. One atomic word holds the middle slot index plus a dirty
  bit, and each side swaps it with `xchg`, so neither ever waits.

### Liveness and restarts

- Both sides stamp `GetTickCount64()` heartbeats every frame. Skyrim treats MC as dead after 3 s.
  MC treats Skyrim as dead after **8 s**, because loading screens stall the host's heartbeat.
- Skyrim, on creating the mapping (including when it reuses one that MC still holds open), **zeroes
  everything Skyrim owns** (ring heads, tables, overlay word), writes version and pid, and writes
  **magic last** with release.
- MC notices a changed `skyrimPid` and bumps a `generation`, which tells everything cached to resend.
- A named mutex `<mapping>_minecraft` tells the host that a Minecraft is already running.
- The mapping gets an explicit SDDL (this user + SYSTEM + admins, medium integrity). Without it an
  elevated host creates an admin-only mapping that a normal Minecraft can't open (error 5).

## 3. How the Fabric mod reads and writes it (Java)

- **Native access is `java.lang.foreign` (FFM).** `Linker.nativeLinker()` downcalls into `kernel32`
  (`OpenFileMappingW` with `captureCallState("GetLastError")`, `MapViewOfFile`, `GetTickCount64`,
  `QueryPerformanceCounter`, `CreateMutexW`). The JVM needs **`--enable-native-access=ALL-UNNAMED`**
  (set in `loom.runs.client.vmArgs`). Without it Java 25 warns, and later releases are set to refuse.
- The view is wrapped as `MemorySegment.reinterpret(MAPPING_BYTES)`. Ordering comes from
  `ValueLayout.JAVA_LONG.varHandle()` / `JAVA_INT.varHandle()` with `getAcquire` / `setRelease`,
  plus `VarHandle.loadLoadFence()` / `storeStoreFence()` around seqlock bodies. Plain fields use
  `segment.get/set(JAVA_INT, off)`, which is native order (little-endian on x64).
- **Everything runs on the render thread:** `MinecraftMixin` calls `SkyClient.beginFrame()` at
  `Minecraft.runTick` HEAD. That polls the mapping, reads `SkyState`, drains input, and applies
  teleports. `END_CLIENT_TICK` publishes `McState`. The render ring writer sleeps up to about 1 s when the
  ring is full. PeakCraft's first serious bug came from this: the host didn't drain the render ring,
  so MC's heartbeat thread stalled and the link flapped.

## 4. How the host drives Minecraft's player and world

- **The player (SkyCraft's model):** MC physics is authoritative. The host publishes its character's
  position and bumps `teleportSeq` only on handoffs (load, door, respawn). MC teleports its player there,
  acks with `teleportAck`, and from then on the host follows MC. The host keeps its own character as
  a hidden "follower" moved to MC's interpolated feet every frame, interpolating MC's raw 20 Hz tick
  pair on its own clock. Look direction goes the other way: the host integrates the mouse with MC's
  sensitivity curve and publishes yaw/pitch, which MC writes into the player every frame (zero-latency camera).
- **Ownership state machine (PeakCraft):** HostOwns / Handoff / GuestOwns / HostMenu / Cutscene,
  decided once per frame in one function. Link loss is simply HostOwns.
- **The world:** MC runs a void "mirror" world, created automatically by `MirrorWorld` on the title
  screen. It uses a custom `dimension_type` (`min_y -1024`, `height 2048`) and a flat generator with
  no layers. Game rules: no time/weather advance, no mob spawning, **`PLAYER_MOVEMENT_CHECK=false`**
  (so the integrated server doesn't rubber-band host-driven moves), keep inventory, immediate respawn.
  The host's collision reaches MC physics as triangles plus a 1/8-block occupancy grid, injected by
  mixins into MC's collision queries. MC's own movement code then runs unchanged.

## 5. How block state is mirrored

- MC blocks are **real MC blocks in the mirror world**, so they persist in MC's own save, and the
  inventory is MC's.
- MC **renders** them for the host. Section meshes from MC's block renderer (with tint, AO, light),
  plus the atlas, go through the render ring, and the host draws them in its own 3D pass so the host's
  depth buffer occludes them. PeakCraft did the same in Unity with a borrowed particle shader.
- Host NPCs collide with builds through `RenSolids` (a 16³ solid bitset per section).
- "Digging" host geometry (SkyCraft `SkyDig`) works because Skyrim's Havok shapes can be cut. `RenDug` tells
  the host which cells were dug so it can hide geometry. This doesn't transfer to RDR2 (see section 6).

## 6. Reuse vs rewrite for RDR2

| Piece | Verdict | Why |
|---|---|---|
| MIT license, credit | **Reuse** | Keep SkyCraft's copyright notice in `LICENSE` and credit in README and file headers |
| Gradle/Loom setup, MC 26.3 / Loader 0.19.5 / Fabric API 0.161.0+26.3 / Java 25 / Gradle 9.7.1 | **Reuse as-is** | Proven working; pinned to the same versions |
| FFM kernel32 bindings and `--enable-native-access` | **Reuse** (adapted) | Same Windows APIs. I add `CreateFileMappingW` (MC may start first) and `VirtualQuery` (validate the view size) |
| SPSC ring algorithm (u64 running counters, separate cache lines, pad-to-wrap, skip unknown by length) | **Reuse** | Proven. RedCraft uses it for both rings |
| "Host resets what it owns, magic written last", heartbeat timeouts, explicit SDDL, `_minecraft` mutex | **Reuse** | RDR2 can run elevated too |
| `MirrorWorld` auto-create/open + game rules (`PLAYER_MOVEMENT_CHECK=false` etc.) | **Reuse** (adapted) | Same need. Phase 1 uses a superflat dev world so motion is visible |
| Void mirror preset + tall `dimension_type` | **Reuse later** (Phase 2+) | RDR2's height range should fit in -1024..1023 (verify in Phase 2) at 1 block = 1 m |
| SkyCraft's exact v11 protocol | **Rewrite** (RedCraft protocol v1) | The brief asks for a header with section table + two typed message rings + HELLO/HEARTBEAT/... Both sides may create the mapping. See `docs/PROTOCOL.md` and DECISIONS D-003 |
| Triangle collider, collision ring, `SkyCollision`/`TriCollider`/`PlayerEdgeMixin` | **Park** for Phase 3 | Needed only if Phase 3 chooses "MC physics drives the ped" (SkyCraft's model). RDR2 exposes shape tests, not shapes, so export would be PeakCraft-style ray sampling |
| Input bridge (SDL scancodes, release-all, cursor) | **Park** for Phase 3 | Directly portable if MC owns movement |
| Overlay triple buffer + `FrameExporter` | **Park** for Phase 3 | Needs a Vulkan/DX12 overlay or a ReShade add-on (as the GTA V project does). ScriptHookRDR2 draw natives can't stream frames. Evaluate in Phase 3 |
| Render ring / `WorldExporter` (MC meshes drawn by the host) | **Rewrite** as props | ScriptHookRDR2 has no custom-mesh draw. Phase 4 spawns cube props from `BLOCK_SET`, or composites like the GTA V project |
| Combat (`SkyrimActorEntity`, hit/hurt events), water grid, dig, block lights, Discord, e4mc, LAN `/join` | **Strip** | Skyrim-specific or later phase. LAN multiplayer is a "later phase" and can be lifted back from upstream |

### Phase 1 guest contents after stripping

Kept and adapted: Gradle project, FFM link (rewritten for the RedCraft protocol), world auto-open
(`DevWorld`, from `MirrorWorld`), server game rules, the running-Minecraft mutex.
New: the puppet that applies `PLAYER_STATE`, block-change reporting (`BLOCK_SET`, player
break/place intents), handling of host block messages, a link status line in MC's HUD, and the
rate-limited `logs/redcraft-fabric.log`.

## 7. Lessons from the porting guide that apply directly

1. Mirror the layout and **self-check it at startup** (sizes and offsets). Refuse to run on mismatch.
2. **Self-test the rings** on a private buffer at startup, including wrap-around.
3. Use the **same clocks** on both sides (`GetTickCount64`, `QueryPerformanceCounter`).
4. **Drain every ring every frame from day one.** Never block a heartbeat thread on a full ring.
5. A failed mapping **disables the plugin completely**. The host must run unmodified.
6. Send **release-all** on every routing change (Phase 3).
7. **Link loss = host owns** (Phase 2/3 ownership state machine).
8. A fake peer (`fake_minecraft.py`, `fake_skyrim.py`) proves link, layout and link loss before
   the real game is involved. RedCraft's mock host is that fake peer, written in C++ so its link code is
   exactly what the ASI will run.
9. A dev harness that reads a command file helps in-game testing (consider for Phase 2).
