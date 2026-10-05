# Known limitations

As of Phase 2 (2026-10-05). "Planned" names the phase expected to fix it.

## Co-op (Minecraft side)
- **The ground is fake until Phase 2.** The mock host serves a made-up map. Real Los Santos ground comes from
  the GTA V plugin's scanner (Phase 2).
- **Ground is built only near players** (`terrain.radius`, 6 chunks ≈ 100 blocks) and only `terrain.depth`
  blocks thick (8; building tops 40), with void under it. A friend who digs through the bottom is lifted back
  up. Flying far ahead outruns the ground for a few seconds; you hover until it arrives.
- **A chunk's ground is built once.** If the host's ground changes later (Phase 6 buildings), already-built
  chunks keep the old blocks until the world is reset. Planned: Phase 6.
- **Friends can dig the host's ground in Minecraft**, but GTA's ground can't be dug, so the two disagree there.
  Planned: decide in Phase 4 (the brief asks for a proposal).
- **LAN only.** Friends on the same network, or the same PC. Internet play (no port forwarding) is Phase 5,
  together with a whitelist setting. Today the whitelist flag is only reported, not configurable.
- **Friends must run exactly Minecraft 26.3** (the hidden Minecraft's version).
- **The server doesn't check movement** (`playerMovementCheck` is off so the host-driven player is never
  rubber-banded). That applies to friends too, so a modded friend client could move freely. Fine among friends.
- **The world shows as `CraftVDev - CraftV`** in friends' LAN list, because the dev Minecraft's offline name is
  `CraftVDev`. Planned: the hidden Minecraft runs under its own name in the release setup (Phase 5).
- **Vanilla-client check:** automated tests use a stand-in friend that loads the CraftV mod (in a mode that only
  joins). Joining from a truly unmodded Minecraft is Sary's Phase 1 test.
- **The stand-in friend has no Minecraft account,** so test runs switch authentication off
  (`CRAFTV_DEV_NO_AUTH`), and its chat shows "Chat messages can't be verified". Normal runs never do this.

## Link / protocol
- **Blocks changed while the link is down are not sent later.** MC only reports changes while
  CONNECTED. After a reconnect the host doesn't get a snapshot of the world. Planned: Phase 4
  (chunk snapshot on connect, `BLOCK_PALETTE`).
- **Block ids are raw block-state ids** for the pinned MC 26.3 + CraftV mod set. They change if the
  MC version or the mod list changes. Planned: `BLOCK_PALETTE` (id → name) in Phase 4.
- **Full rings drop messages** (counted, logged, never blocking). With 1 MiB rings this needs a burst of
  about 40,000 block changes at once (a huge explosion). Planned: Phase 4 resync. Terrain patches that don't
  fit are simply asked for again.
- **Two processes of the same role:** the second refuses to attach while the first is alive, keyed by
  pid. A reused pid could in theory fool it.
- **Minecraft's side doesn't apply the explicit DACL** when it creates the mapping. This only matters
  if Minecraft runs elevated and GTA doesn't, which is unusual. The C++ side (the host) does apply it.

## Minecraft side (host's own player)
- **Puppet mode has one tick (50 ms) of latency** and Minecraft's physics still runs between ticks.
  While the host is sending, mouse look in the dev window is overridden every tick by the host's
  yaw/pitch. When no new state has arrived for 250 ms (host paused), the mouse and keyboard take over.
- **Host place requests skip inventory** (they place the block directly). Planned: Phase 4 routes them
  through the real use-item path.
- **The Minecraft window must stay open.** Nothing hides it yet. Planned: Phase 5 (release setup).

## Tooling
- The JDK comes from the Minecraft Launcher's runtime folder. If the launcher updates or removes it,
  set `CRAFTV_JAVA_HOME` or install Temurin 25.
- The first `runClient` / `runFriend` downloads Minecraft's assets (a few hundred MB) and may show a
  first-launch screen; both skip it automatically.
- Piping `test-all.ps1` into another command can hang after the tests finish (the Gradle daemon keeps the pipe
  open). Run it plainly or redirect it to a file.

## GTA V plugin (built and tested without the game, not yet run in GTA V)
- **Every native is unverified in game.** Names, hashes and signatures match the SDK's `natives.h`. These are
  marked `// ASSUMPTION:` and get checked in the Phase 2 test:
  - `0x7EE9F5D83DD4F90E` is the synchronous ray and its result is ready at once (otherwise ground still works
    through `GET_GROUND_Z_FOR_3D_COORD`, without materials).
  - Shape-test flag 1 is map collision (terrain, roads, buildings) without props, peds and vehicles.
  - The reported material hash is joaat of the materials.dat name (the overlay shows the name it matched).
  - `GET_WATER_HEIGHT_NO_WAVES(x, y, groundZ)` reports the water surface above the ground.
  - `REQUEST_COLLISION_AT_COORD` loads collision for a far chunk within 10 frames.
  - The ped's root is 1.0 m above its feet (`FeetOffset`); the network natives read false in story mode.
- **Ground is probed from above,** so friends stand on roofs and bridge decks, and under a bridge the ground is
  hidden. Planned: Phase 6 (several hits per column).
- **Chunks far from you may come slowly** (GTA only streams collision near the player). A chunk with no hits is
  retried twice, then left empty. The map bounds that count as "outside" are an estimate.
- **Probe cost per frame is unmeasured in GTA.** 64 probes per frame by default (`[Terrain] ProbesPerTick`). The
  overlay and CraftV.log report the tick cost so it can be tuned.
- **Script Hook V must match the game build exactly;** after a GTA update, wait for a new Script Hook V.
- The parked RDR2 plugin (`rdr2/`, `CraftV_RDR2.asi`) was built against ScriptHookRDR2 and never run in RDR2. It
  has no terrain scanning.
