# Phase 4 plan: blocks are solid in GTA (proposal, awaiting Sary's OK)

Written 2026-10-06 while Phase 3 waits for its in-game test. Nothing here is built yet (brief §9: propose first).

## Goal

A block you or a friend places in Minecraft stops GTA's people, cars and your own character. Minecraft already draws
the block through the passthrough (Phase 3), so GTA only needs **invisible collision**.

## How (adapted from minecraft-gta5-passthrough, MIT)

- **One invisible, frozen GTA prop per block**, spawned where the block is and deleted when it's broken. The reference
  uses `prop_box_wood01a` (0.97 × 0.96 × 0.80 m). That isn't a full block high, so stacked blocks would have gaps.
  The Phase 3 test run logs the real size of 16 candidate box models (`prop candidate ...` lines in CraftV.log), and
  I'll pick the closest to 1 × 1 × 1 m from those numbers.
- **Limit: about 400 live props.** GTA crashes at around 1,500 script objects (measured by the reference). Props are
  spawned nearest-first around you (about 48 m) and despawned further away. Minecraft keeps every block; GTA only holds
  the nearby ones. When the limit is hit, it's logged and shown on the overlay.
- **Only blocks above the ground become props.** Blocks inside CraftV's terrain columns are GTA's ground already.
  Non-solid blocks (torches, flowers, water, snow layers) get no prop.
- **Protocol v1.3:** a `BLOCK_REGION_REQUEST` (host → MC: "send me every placed block in chunk X, Z"), answered with
  `BLOCK_SET`s. That way blocks built in an earlier session, or while GTA was closed, get collision too, and GTA can
  forget far chunks and ask again later.
- **Stray block changes:** with the rule "only solid blocks above the terrain surface", natural changes (water
  settling, grass spreading) don't spawn props. The new log line (Phase 3 build) shows what they really are first.

## Questions for Sary (answer when you're back)

1. **Digging into GTA's ground.** GTA's streets can't have holes. When someone breaks a terrain block in Minecraft:
   - **a) Allow it (recommended).** The hole exists in Minecraft only: friends can dig and fall in. In GTA the street
     stays, and you walk over the hole.
   - **b) Protect the terrain.** Nobody can break terrain blocks. Minecraft and GTA always match, but friends can't
     dig.
2. **About 400 solid blocks around you at a time:** OK? Builds bigger than that stay in Minecraft. Only the nearest
   400 are solid in GTA.

## Test plan

- Without GTA: the mock host and hostsim keep a fake prop table; a test checks spawn and despawn order, the limit, the
  region requests, and that terrain or natural changes make no props.
- In GTA (Sary): build a wall across a street. Peds and cars stop at it, and you can stand on top of it.
