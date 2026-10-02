# Testing

## Automated (no game needed)

```powershell
.\scripts\test-all.ps1
```

| Suite | What it proves | Count |
|---|---|---|
| C++ `redcraft_link_tests` | Layout, golden vectors, spec-literal encodings, validation, ring wrap/full/stale/corrupt, mapping create/open/validate, endpoint connect/stale/resume/restart, **1,000,000 records through each ring at once** | 31 |
| Java `gradlew test` | Same golden vectors (C++ and Java agree byte for byte), ring behaviour, endpoint behaviour, **1,000,000 records through each ring** of a real mapping | 16 |
| C++ `redcraft_rdr2_tests` | RDR2 plugin logic with a fake game: coordinate/heading conversion, story-mode gate, PLAYER_STATE into a real Minecraft-role endpoint, loading/dead/missing player, Minecraft dying, faults, overlay, config, **zero heap allocations per steady-state tick** | 14 |
| `gradlew integrationTest` | Real `mockhost.exe` against the real Java link in separate processes: host first, MC first, either killed mid-run and restarted, `kill-link`/`resume-link`, `restart`, both killed, block messages, **1,000,000 records each way across processes and languages** | 9 |

## Phase 1 manual test (Sary)

**Setup, once:** nothing to install. Build with `.\scripts\build-native.ps1`.

1. In PowerShell, from the repo root: `.\scripts\dev-run.ps1`
   - A **mock host** console opens (you can type commands there).
   - Minecraft starts. The first run downloads assets. A world called **RedCraft Dev**
     (superflat, Creative) opens by itself.
2. **Movement:** within a second of the world loading, the player walks a circle (radius 6, centre
   0.5/0.5, walking speed) and turns to face the way it walks. Press F5 to watch from behind. The green
   line at the top-left reads `RedCraft state=CONNECTED ... 'RedCraft-MockHost 0.1.0' ping=...`.
3. **Block requests:** break a block, then place a block (e.g. stone from the Creative inventory). The mock
   host console prints, for each:
   - `@RX BLOCK_BREAK_REQUEST #n (x, y, z) face up` and `@RX BLOCK_SET (x, y, z) = 0 (air)`
   - `@RX BLOCK_PLACE_REQUEST #n against (...) face ... block <id>` and `@RX BLOCK_SET (x, y, z) = <id>`
4. **Host edits:** in the mock host console type `setblock 2 -60 2 1`. Stone appears next to the circle,
   and the console prints `@RX BLOCK_SET (2, -60, 2) = 1 [echo of host edit]`.
   Also try `place 2 -60 2 1 1` (stone on top) and `break 2 -60 2`.
5. **Timeouts:** type `kill-link`. After about 10 s the top-left line turns red/yellow (`STALE`) and the
   player stops. Type `resume-link`: back to `CONNECTED` and walking (same session number).
6. **Restart:** type `restart`. The `host=` number goes up by one and the player snaps back onto the circle.
7. **Crash recovery:** close the mock host window. After about 10 s Minecraft shows `STALE`. Start
   `build\tools\mockhost\Release\mockhost.exe` again: back to `CONNECTED` and walking.
   Then the reverse: close Minecraft, keep the mock running, start `.\scripts\dev-run.ps1 -NoMock`: it reconnects.

**Other mock commands:** `help`, `status`, `center X Y Z`, `radius R`, `speed S`, `walk`, `stop`, `quit`.

### If something fails, send me
- `logs\redcraft-fabric.log` (Minecraft side)
- `logs\mockhost.log` (host side)
- `fabric\run\logs\latest.log` (Minecraft's own log), plus a screenshot if it's visual

## Phase 2 test (draft; needs RDR2 build 1491.50, story mode)

Prerequisites, done by Sary:
1. A ScriptHookRDR2 runtime + 64-bit `dinput8.dll` ASI loader in the RDR2 folder (see DECISIONS D-012
   for official vs V2).
2. Tell me the RDR2 folder path. I'll set it up for `deploy-rdr2.ps1` (env var `REDCRAFT_RDR2_DIR`).

Steps (final version comes with the Phase 2 report):
1. `.\scripts\deploy-rdr2.ps1`, which copies `RedCraft.asi` + `RedCraft.ini` and checks the hook and loader are there.
2. Start Minecraft first: `.\scripts\dev-run.ps1 -NoMock`. Then start RDR2 and load a story save.
3. The top-left overlay in RDR2 shows `link CONNECTED`. Walking in RDR2 moves the Minecraft player.
   Walking north should make Minecraft's Z go down.
4. Check the overlay's last line reads `net game 0 session 0 in 0` in story mode.
5. Close Minecraft: RDR2 keeps running and the overlay shows STALE. Restart Minecraft: CONNECTED again.
6. Send `RedCraft.log` (next to RDR2.exe), `logs\redcraft-fabric.log`, and a screenshot of the overlay.
