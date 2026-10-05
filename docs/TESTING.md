# Testing

## Automated (no game needed)

```powershell
.\scripts\test-all.ps1
```

| Suite | What it proves | Count |
|---|---|---|
| C++ `craftv_link_tests` | Layout, golden vectors for all 13 messages, spec-literal encodings, validation and direction rules, ring wrap/full/stale/corrupt, mapping create/open/validate, endpoint connect/stale/resume/restart, **1,000,000 records through each ring at once** | 35 |
| Java `gradlew test` | The same golden vectors (C++ and Java agree byte for byte), ring and endpoint behaviour, 1,000,000 records through each ring, terrain column building, the terrain request planner (nearest first, cap, retry), friend tracking (join/state/leave, velocity, swing, resync) | 37 |
| C++ `craftv_host_tests` | The plugin core shared by GTA V and RDR2, with a fake game: coordinates, story-mode gate, PLAYER_STATE end to end, terrain requests answered (near and far chunks, misses, map edge, materials), friends and session info, faults, overlay, config, zero heap allocations per tick | 24 |
| `gradlew integrationTest` | Real `mockhost.exe` against the real Java link in separate processes: start orders, crashes, restarts, timeouts, block messages, terrain requests answered (and a host that answers none), a friend joining/moving/leaving, SESSION_INFO, **1,000,000 records each way** | 11 |

Done on 2026-10-05 with the mock host, the hidden Minecraft and a stand-in friend (a second dev
Minecraft, `scripts\run-friend.ps1 -Bot`): the world opened to friends, the friend spawned next to the host's
player, walked on the mock's terrain, placed and broke blocks (each one reached the mock host), left and was
reported gone, was re-announced after a host restart, and hovered until its ground arrived when terrain was
switched off and on.

## Phase 1 co-op test (Sary)

You need: your normal Minecraft Launcher with Minecraft Java Edition **26.3** (Installations → New installation
→ version "release 26.3", if you don't have it yet).

1. Open PowerShell in the `craftv` folder and run `.\scripts\dev-run.ps1`.
   - A **mock host** console opens. Leave it alone; you don't need to type anything in it.
   - A dev Minecraft opens the world **CraftV** by itself: blocky hills, a lake, grey roads. This window stands
     in for the hidden Minecraft that will run next to GTA. Leave it open.
   - Its top-left shows a green `CraftV state=CONNECTED ...` line and, under it, `friends 0 @ <your-ip>:25565`.
   - If Windows asks whether Java may use the network, choose **Private networks** and Allow.
2. Start your **normal** Minecraft (the launcher, your own account), version 26.3 → **Multiplayer**.
   The world shows up at the bottom under the LAN list as `CraftVDev - CraftV`. Join it.
   (If it doesn't show up: **Direct Connection** → `localhost:25565`.)
3. **You spawn next to the walking player** (`CraftVDev`, the stand-in for your GTA character), in Creative,
   standing on the blocky ground. In the mock host console you'll see `@RX REMOTE_PLAYER_JOIN #... '<your name>'`,
   then once a second `@FRIEND ... at (x, y, z) ...` following you around.
4. **Walk or fly away** from the start. The ground keeps appearing ahead of you, about 100 blocks around you.
5. **Place and break a few blocks.** For each one the console prints `@RX BLOCK_SET (x, y, z) = <id>`, and
   `= 0 (air)` when you break one.
6. **Leave the world** (Esc → Disconnect). The console prints `@RX REMOTE_PLAYER_LEAVE ... left`.

When you're done, close both Minecraft windows and the mock host console.

### If something fails, send me
- `logs\craftv-fabric.log` (the dev Minecraft) and `logs\mockhost.log` (the mock host)
- `fabric\run\logs\latest.log` (the dev Minecraft's own log)
- the error your own Minecraft shows when joining (a screenshot is fine)

### Link checks from the first Phase 1 (still valid, optional)
Type these in the mock host console: `kill-link` (after about 10 s the green line turns red `STALE` and the
walking player stops), then `resume-link` (back to `CONNECTED`); `restart` (the `host=` number goes up by one).
`ground X Z` prints the fake ground at a spot; `terrain off` / `terrain on` stops and restarts the ground supply.

## Phase 2 test (draft; needs GTA V Legacy, story mode)

Sary's GTA V lives in `C:\Rockstar Games\GTA V\` and starts through Sary's own batch launcher. CraftV uses both
as they are and never moves or replaces anything there.

Prerequisites, done by Sary:
1. Script Hook V + its ASI loader (`dinput8.dll`) from http://www.dev-c.com/gtav/scripthookv/ in the GTA V folder.
   I'll confirm the exact version against your game build first.

Steps (the final version comes with the Phase 2 report):
1. `.\scripts\deploy-gta.ps1` copies only `CraftV.asi` + `CraftV.ini` into the GTA V folder.
2. Start Minecraft first (`.\scripts\dev-run.ps1 -NoMock`), then GTA V through your launcher, and load story mode.
3. The overlay shows `link CONNECTED`; walking in GTA moves the Minecraft player; a friend in Minecraft walks on
   blocky ground that matches the streets around them.
