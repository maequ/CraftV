# Known limitations

As of Phase 1 (2026-10-02). "Planned" names the phase expected to fix it.

## Link / protocol
- **Blocks changed while the link is down are not sent later.** MC only reports changes while
  CONNECTED. After a reconnect the host doesn't get a snapshot of the world. Planned: Phase 4
  (chunk snapshot on connect, `BLOCK_PALETTE`).
- **Block ids are raw block-state ids** for the pinned MC 26.3 + CraftV mod set. They change if the
  MC version or the mod list changes. Planned: `BLOCK_PALETTE` (id → name) in Phase 4.
- **Full rings drop messages** (counted, logged, never blocking). With 1 MiB rings this needs a burst of
  about 40,000 block changes at once (a huge explosion). Planned: Phase 4 resync.
- **Two processes of the same role:** the second refuses to attach while the first is alive, keyed by
  pid. A reused pid could in theory fool it.
- **Minecraft's side doesn't apply the explicit DACL** when it creates the mapping. This only matters
  if Minecraft runs elevated and RDR2 doesn't, which is unusual. The C++ side (the host) does apply it.

## Minecraft side
- **Puppet mode has one tick (50 ms) of latency** and Minecraft's physics still runs between ticks.
  While the host is sending, mouse look in the dev window is overridden every tick by the host's
  yaw/pitch. When no new state has arrived for 250 ms (host paused), the mouse and keyboard take over.
- **Host place requests skip inventory** in Phase 1 (they place the block directly). Planned: Phase 4
  routes them through the real use-item path.
- **The dev world is superflat**, not RDR2-shaped. Planned: Phase 2 void mirror world with RDR2 collision.
- **The Minecraft window must stay open.** Nothing hides it yet. Planned: Phase 3.

## Tooling
- The JDK comes from the Minecraft Launcher's runtime folder. If the launcher updates or removes it,
  set `CRAFTV_JAVA_HOME` or install Temurin 25.
- The first `runClient` downloads Minecraft's assets (a few hundred MB) and may show a first-launch
  screen. The dev world tries to skip it, but this hasn't been verified on a truly fresh profile.

## RDR2 plugin (built, not yet run in RDR2)
- **Every native call is unverified in game.** The names, hashes and signatures match the SDK's `natives.h`.
  These are marked `// ASSUMPTION:` and get checked in the Phase 2 test:
  - `GET_ENTITY_COORDS(ped, TRUE, TRUE)` argument meaning; `GET_ENTITY_VELOCITY(ped, 0)` world-space.
  - RDR2 uses GTA V's axes and heading convention (`coords.h`). Walk north and Minecraft's Z should go down.
  - The ped's reference point is about 1.0 m above its feet (`FeetOffset`, measure with the overlay's "above ground").
  - The three network natives read false in story mode (otherwise the gate switches CraftV off).
  - RDR2's loading screens may stop script ticks. Minecraft allows 10 s; longer loads show STALE, then resume.
- **Which ScriptHookRDR2 runs on build 1491.50** is unknown until tried (DECISIONS D-012).
- **The Phase 1 dev world is superflat.** RDR2 heights put the Minecraft player well above its ground. Phase 2
  only checks that the position follows; a tall, void "mirror" world comes when it matters (Phase 3/4).
