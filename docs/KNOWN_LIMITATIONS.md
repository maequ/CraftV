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

## GTA V plugin (Phase 2 confirmed in Sary's game on 2026-10-05)
- Confirmed in game: the link, the Minecraft player following the ped, friends' ground from GTA's real streets
  with surface materials (every probe returned one), the overlay, and a friend joining (Sary's own account, from
  the normal launcher) showing on the overlay. Two wrong assumptions were found and fixed on the way (DECISIONS
  D-023, D-024).
- **Probe cost:** the synchronous ray costs more than ground-Z. While a lot of new ground is being scanned, the tick
  averaged up to 1.4 ms with single frames up to 22 ms (a visible hitch). Quiet play is about 0.1 ms. Lower
  `[Terrain] ProbesPerTick` to trade speed for smoothness; an asynchronous probe pipeline would fix it properly.
- **GTA pauses scripts when it loses focus,** so the link goes stale while you're in another window. To test with
  Minecraft on the same PC: Graphics, Screen Type = Windowed Borderless and Pause Game On Focus Loss = Off.
- **Stray block messages:** a run with no friends online still counted 73 BLOCK_SET messages from Minecraft (likely
  water spreading from terrain water). Harmless now (counted only); Phase 4 must check them before turning them
  into props.
- Chunks Minecraft already built stay as they were. When the material mapping changes, start a fresh CraftV world
  (move `saves/CraftV` aside) to rebuild them.
- Still unverified in game, marked `// ASSUMPTION:`:
  - Shape-test flag 1 is map collision (terrain, roads, buildings) without props, peds and vehicles.
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

## The Minecraft view (Phase 3, not yet run in GTA)
- **Needs ReShade with add-on support**, loaded as `ReShade64.asi`, and GTA in Windowed Borderless with Pause On Focus
  Loss off (Minecraft runs in its own window next to GTA). Two games render at once, so expect a lower frame rate.
- **Blocks have no collision in GTA yet.** You, people and cars pass through what you build until Phase 4 (invisible
  collision props).
- **Heights: half a block off away from you.** CraftV's terrain stores GTA's ground in whole blocks. Around you, Minecraft's
  view is lifted so its ground meets GTA's (blocks you place sit on the street); further away, on slopes, blocks and
  friends can still sink in or float up to half a block.
- **Indoors and under bridges** you stand inside CraftV's terrain (the ground is probed from above). Minecraft treats it
  as air in your view and never suffocates you, but blocks you place there land on the roof or deck terrain.
- **In a vehicle** GTA shows its own driver and Minecraft doesn't draw yours (it has no car to seat you in). The hand
  and HUD stay. Building from a car isn't meant to work.
- **Your hearts only drop from Minecraft things** (a friend hitting you, hunger). GTA handles your falls and swimming,
  so Minecraft gives you no fall damage and you never drown; GTA damage doesn't touch your hearts yet.
- **Melee reach and knock-back** are the reference project's values, untested in CraftV. Damage is Minecraft's attack
  damage x `MeleeDamagePerHalfHeart` (10), with vanilla's charge scaling.
- **GTA's HUD stays** by default (`[Passthrough] HideGtaHud=1` hides it). Its minimap sits left of Minecraft's hotbar.
- Worlds from before protocol v1.2 rebuild their terrain chunks once (the save didn't keep their columns).

- **No inventory screen yet** (it needs the mouse inside Minecraft's window). The hotbar, wheel and 1-9 work.
- **Breaking the floor** breaks the hidden terrain block: in your view nothing changes (the floor is GTA's). Phase 4 decides what digging does.
