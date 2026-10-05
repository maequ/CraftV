# CraftV shared-memory protocol, version 1.1

This document is the **source of truth** for the bytes shared between the host (the GTA V ASI
plugin, the parked RDR2 plugin, or the mock host) and the Minecraft guest (the Fabric mod). The code mirrors it in:

- C++: `protocol/cpp/include/craftv/protocol.h` (constants, structs, `static_assert`s on every size/offset)
- Java: `fabric/src/main/java/dev/craftv/link/Proto.java`

Both are checked against the same golden byte vectors in `protocol/golden/golden_vectors.txt`.
If you change anything here, change both mirrors, regenerate the goldens, and bump the version (§9).

Design lineage: SkyCraft protocol v11 (chasmlol, MIT). The ring algorithm, heartbeat timing,
"magic written last" and the access rules are SkyCraft's. The section table, the two typed message
rings, symmetric create-or-open, and per-message session tags are new (see `DECISIONS.md` D-003).

## 1. Conventions

| Rule | Value |
|---|---|
| Byte order | **Little-endian** for every multi-byte field. Both implementations refuse to run on a big-endian machine |
| Integer types | `u8/u16/u32/u64` unsigned, `i32` two's complement |
| Floats | IEEE-754 binary32 (`f32`) and binary64 (`f64`) |
| Strings | UTF-8, length given by an explicit field, not NUL-terminated (unused bytes are zero) |
| Alignment | Every struct field is naturally aligned. Sections are 4 KiB aligned. Ring records are 16-byte aligned |
| Coordinates | **Minecraft space** everywhere: X east, Y up, Z south, 1 unit = 1 block. The host converts at its edge (GTA V and RDR2 are Z-up in metres: 1 block = 1 m, `mc.x = gta.x`, `mc.y = gta.z`, `mc.z = -gta.y`, as in the GTA V reference project. `// ASSUMPTION:` verified in game in Phase 2) |
| Angles | Minecraft degrees: yaw 0 looks along +Z (south), yaw 90 along -X (west). Pitch positive looks down, range [-90, 90] |
| Time | Heartbeat liveness uses each reader's own monotonic clock (§6). Timestamps in messages are microseconds of `QueryPerformanceCounter`, which is one system-wide clock on a given PC |
| "Host" / "MC" | Host = the game plugin or the mock host (role `1`). MC = the Minecraft guest (role `2`), which is also the Minecraft server friends join |
| "Friend" | A player in MC's world other than the host's own player (the integrated server's owner) |

## 2. The mapping

| Property | Value |
|---|---|
| Name | `Local\CraftV_Shared_v1` (override: `-Dcraftv.link=` in Java, `--mapping` in the mock host. Tests use their own name) |
| Backing | Page file (`CreateFileMappingW(INVALID_HANDLE_VALUE, ...)`, `PAGE_READWRITE`). New mappings are zero-filled by Windows |
| Size | `MAPPING_BYTES = 0x203000` (2,109,440 bytes) |
| Access | Created with an explicit DACL: SYSTEM, Administrators and **the current user**, medium integrity label. An elevated RDR2 can still share it with a non-elevated Minecraft (SkyCraft lesson) |
| Who creates it | **Whoever starts first.** Both sides call `CreateFileMappingW`. The side for which it does *not* fail with `ERROR_ALREADY_EXISTS` is the **creator** |

### 2.1 Region map

| Offset | Size | Region |
|---|---|---|
| `0x000000` | `0x1000` | Header (§3) |
| `0x001000` | `0x101000` | Section 1: ring **Host → MC** (control `0x1000` + data `0x100000`) |
| `0x102000` | `0x101000` | Section 2: ring **MC → Host** (control `0x1000` + data `0x100000`) |
| `0x203000` | | end (`MAPPING_BYTES`) |

Readers must locate sections **through the section table**, not the constants above. The constants
are what a v1.0 creator writes.

## 3. Header (offset 0, 4096 bytes)

The header has three parts: creator-written immutable fields, then one 64-byte block per side, each
on its own cache line and written only by that side.

| Offset | Size | Type | Field | Written by | Notes |
|---|---|---|---|---|---|
| `0x000` | 4 | u32 | `magic` | creator, **last** | `0x56465243` (bytes `43 52 46 56` = `"CRFV"`). Zero until the creator finished initialising |
| `0x004` | 2 | u16 | `versionMajor` | creator | `1` |
| `0x006` | 2 | u16 | `versionMinor` | creator | `0` |
| `0x008` | 4 | u32 | `headerBytes` | creator | `0x1000` |
| `0x00C` | 4 | u32 | `sectionCount` | creator | `2` (max `16`) |
| `0x010` | 8 | u64 | `mappingBytes` | creator | `0x203000` |
| `0x018` | 4 | u32 | `creatorRole` | creator | `1` host, `2` MC |
| `0x01C` | 4 | u32 | `creatorPid` | creator | informational |
| `0x020` | 32 | | reserved | | zero |
| `0x040` | 64 | | **host block** | host | §3.1 |
| `0x080` | 64 | | **MC block** | MC | §3.1 |
| `0x0C0` | 64 | | reserved | | zero |
| `0x100` | 32 × n | | **section table** | creator | §3.2 |
| … | | | reserved up to `0x1000` | | zero |

### 3.1 Side block (host at `0x040`, MC at `0x080`)

| Offset | Size | Type | Field | Notes |
|---|---|---|---|---|
| `+0x00` | 4 | u32 | `pid` | process id of the attached side |
| `+0x04` | 4 | u32 | `session` | this side's current session id. `0` = never attached. See §5 |
| `+0x08` | 8 | u64 | `heartbeat` | **counter**, +1 at least every 100 ms while attached. Release store |
| `+0x10` | 8 | u64 | `heartbeatUs` | QPC µs at the last increment (informational, for logs) |
| `+0x18` | 4 | u32 | `state` | bit 0 `ATTACHED`, bit 1 `IN_GAME` (host: a story-mode player exists; MC: in a world). Other bits 0 |
| `+0x1C` | 36 | | reserved | zero |

### 3.2 Section table entry (32 bytes, at `0x100 + 32·i`)

| Offset | Size | Type | Field | Notes |
|---|---|---|---|---|
| `+0x00` | 4 | u32 | `id` | `1` = ring Host→MC, `2` = ring MC→Host. Unknown ids are ignored |
| `+0x04` | 4 | u32 | `flags` | `0` |
| `+0x08` | 8 | u64 | `offset` | from the mapping base, 4 KiB aligned |
| `+0x10` | 8 | u64 | `bytes` | total section size including the ring control page |
| `+0x18` | 8 | | reserved | zero |

**Validation (every reader, on attach):** `magic`, `versionMajor == 1`, `headerBytes == 0x1000`,
`1 <= sectionCount <= 16`, `mappingBytes <= actual view size` (from `VirtualQuery`), and for
each ring section: `offset` 4 KiB aligned, `offset >= headerBytes`, `offset + bytes <= mappingBytes`,
data size (`bytes - 0x1000`) is a power of two between 64 KiB and 64 MiB, the control page's
`dataBytes` matches, and the two rings don't overlap. Both ring ids must be present. Any failure means
the reader does not attach (it logs the reason and retries every 5 s).

## 4. Rings

Two **single-producer / single-consumer byte rings**, one per direction. Each side is the only
producer of one ring and the only consumer of the other. Within a process, only one thread may
produce (or consume) a given ring. Both implementations funnel through a single link thread or the
host's script tick.

### 4.1 Ring control page (first 4 KiB of the section)

| Offset | Size | Type | Field | Written by |
|---|---|---|---|---|
| `+0x00` | 8 | u64 | `head` | producer: total bytes ever published (monotonic) |
| `+0x40` | 8 | u64 | `tail` | consumer: total bytes ever consumed (monotonic) |
| `+0x80` | 8 | u64 | `dataBytes` | creator (= section bytes − `0x1000`) |
| `+0x88` | 4 | u32 | `producerRole` | creator (`1` for Host→MC, `2` for MC→Host) |
| `+0x8C` | … | | reserved | |

`head` and `tail` sit on separate 64-byte cache lines. Data starts at section offset + `0x1000`.
Position of a byte count `c` in the data area: `c mod dataBytes` (dataBytes is a power of two).
Invariant: `0 <= head − tail <= dataBytes`, and both are multiples of 16.

### 4.2 Record format

Every record starts 16-byte aligned with a 16-byte header, followed by the payload, then zero
padding up to the next 16-byte boundary:

| Offset | Size | Type | Field | Notes |
|---|---|---|---|---|
| `0` | 2 | u16 | `type` | §7. `0` = PAD |
| `2` | 2 | u16 | `typeVersion` | version of this message's payload layout (all are `1` in v1.0) |
| `4` | 4 | u32 | `payloadBytes` | `0 ..= MAX_PAYLOAD (4096)` |
| `8` | 4 | u32 | `session` | producer's session id when the record was written |
| `12` | 4 | u32 | `seq` | producer's record counter for this ring and session, starts at 1 |

Record size = `align16(16 + payloadBytes)`.

**Produce** (never blocks):

1. `size = align16(16 + payloadBytes)`. `pos = head mod dataBytes`. `pad = (pos + size > dataBytes) ? dataBytes − pos : 0`.
2. `tail = load_acquire(tail)`. If `dataBytes − (head − tail) < size + pad`, the ring is **full**: drop the
   message, count it, and return failure. The producer never waits.
3. If `pad > 0`: write a PAD header (`type 0`, `payloadBytes 0`) at `pos`, `head += pad`, `pos = 0`.
4. Write header + payload + zero padding at `pos` (plain stores).
5. `store_release(head, head + size)`. One release store may publish several records.

**Consume** (bounded per call):

1. `head = load_acquire(head)`. Read the producer's current `session` from its side block (acquire).
2. Check `head − tail` is within `[0, dataBytes]` and 16-aligned. If not, the ring is **corrupt** (§4.3).
3. While `tail < head` and under the per-call byte budget: `pos = tail mod dataBytes`.
   - If `dataBytes − pos < 16`, `tail += dataBytes − pos`. This can't happen with 16-byte
     alignment, but readers handle it.
   - `type == 0` (PAD): `tail += dataBytes − pos`.
   - Else validate: `payloadBytes <= MAX_PAYLOAD`, `pos + size <= dataBytes` (no straddling),
     `size <= head − tail`. Failure means **corrupt**.
   - If `record.session != producer session`, the record is **stale** (left by a previous incarnation
     of the peer): skip it and count it.
   - Otherwise decode it (§7). Unknown `type`: skip it by length and count it. Known type with
     `payloadBytes` smaller than that type's size: **malformed**, skip it and count it. Larger is fine:
     read the known prefix (forward compatibility).
   - `tail += size`.
4. `store_release(tail, tail)`.

### 4.3 Corruption

A consumer that finds a corrupt ring sets `tail = head` (drops everything pending), counts a
corruption, and logs it (rate-limited). It never reads outside the data area, whatever the peer
wrote. A producer that finds `head − tail` out of range treats the ring as full until the consumer resyncs.

## 5. Startup, sessions and reconnection

Either side may start first, and either side may restart at any time.

### 5.1 Create-or-open

1. `CreateFileMappingW(..., MAPPING_BYTES, name)`.
   - Succeeds without `ERROR_ALREADY_EXISTS`: this side is the **creator**. The memory is zero.
     Write every header field except `magic`, the section table and each ring control page
     (`head = tail = 0`, `dataBytes`, `producerRole`), then `store_release(magic, MAGIC)`.
   - `ERROR_ALREADY_EXISTS`: map it and **wait (without blocking the caller's loop)** until
     `load_acquire(magic) == MAGIC`, then validate (§3.2). If `magic` is still 0 after
     `INIT_TIMEOUT_MS = 2000`, the creator died mid-initialisation. This side then initialises it
     as the creator would. That is benign even if the creator was merely slow, because both write
     the same values to a mapping nobody has attached to.
2. If any step fails, the side stays **DETACHED** and retries every `RETRY_MS = 1000`. A host
   that can't create or open the mapping must leave the game completely unmodified.

### 5.2 Attach

Done once per process incarnation, after validation:

1. `session = previous value in my side block + 1` (skip 0). That makes it differ from any
   session the peer may still hold records from.
2. **Discard the stale backlog of the ring I consume:** `store_release(tail, load_acquire(head))`.
   I am its only consumer, so this is safe.
3. **Continue the ring I produce** from its current `head`. Old unread records stay in it and the
   peer drops them as stale (§4.2).
4. Write `pid`, `heartbeat += 1`, `state = ATTACHED`, then `store_release(session)`. Only then
   produce records. Every record I write carries `session`.
5. Send `HELLO`.

### 5.3 Link states (each side computes its own)

| State | Meaning |
|---|---|
| `DETACHED` | No mapping (yet) |
| `WAITING` | Mapping open, not yet initialised or validated |
| `ATTACHED` | Attached, peer not present: peer `session == 0`, not `ATTACHED`, or never seen beating |
| `CONNECTED` | Peer `ATTACHED` and its `heartbeat` changed within the timeout |
| `STALE` | Peer still `ATTACHED` but its `heartbeat` hasn't changed for longer than the timeout |

- A **new peer session** (the value in the peer's side block changed) means the peer restarted. Drop all
  per-peer state and send `HELLO` again (and, from Phase 2, resend anything the peer must know).
  This is how each side tells "peer restarted" apart from "peer resumed".
- **`STALE` → `CONNECTED` with the same session** means the peer resumed (for example after a loading screen).
  Nothing is resent.
- **Clean shutdown:** clear `ATTACHED` in my `state` (keep `session`, so the next incarnation can do +1)
  and stop beating. The peer drops to `ATTACHED` at once without waiting for the timeout.
- While not `CONNECTED`, a side keeps beating, keeps **draining** its incoming ring, and doesn't produce
  gameplay messages (only `HELLO`/`HEARTBEAT` and `LOG`).
- Each side computes its state from what it has observed, so the two flip to `CONNECTED` a few
  milliseconds apart. A receiver therefore **accepts gameplay records in any attached state**. It must
  not reset per-peer state on its own `CONNECTED` event, only on a new peer session. (The chaos
  tests found this: the first 1,598 stress records arrived before the receiver's own `CONNECTED`.)

### 5.4 Timeouts

| Constant | Value | Why |
|---|---|---|
| `HEARTBEAT_PERIOD_MS` | ≤ 100 | Counter increment period while attached. The host beats every script tick, the MC link thread every loop (about 2 ms) |
| `HOST_TIMEOUT_MS` (MC's view of the host) | **10000** | RDR2's loading screens may stop script ticks (`// ASSUMPTION:` checked in Phase 2). SkyCraft uses 8 s for Skyrim |
| `MC_TIMEOUT_MS` (host's view of MC) | **3000** | MC beats from a dedicated link thread, so it keeps beating during world loads. Same value as SkyCraft |
| `HEARTBEAT_MSG_PERIOD_MS` | 500 | `HEARTBEAT` message rate (used for ping) |
| `INIT_TIMEOUT_MS` | 2000 | §5.1 |
| `RETRY_MS` | 1000 | Create/open retry |

Both timeouts can be overridden for tests (`--host-timeout-ms`, `--mc-timeout-ms`, `-Dcraftv.hostTimeoutMs`).

## 6. Memory ordering

Shared memory is accessed by two processes, so every publication point uses acquire/release.

| Operation | Ordering | C++ | Java |
|---|---|---|---|
| Producer publishes `head` | release | `std::atomic_ref<u64>(head).store(v, memory_order_release)` | `LONG.setRelease(seg, off, v)` |
| Consumer reads `head` | acquire | `.load(memory_order_acquire)` | `LONG.getAcquire(seg, off)` |
| Consumer publishes `tail` | release | `.store(v, memory_order_release)` | `LONG.setRelease` |
| Producer reads `tail` | acquire | `.load(memory_order_acquire)` | `LONG.getAcquire` |
| Creator writes `magic` last | release | `atomic_ref<u32>.store(MAGIC, release)` | `INT.setRelease` |
| Reader checks `magic` | acquire | `.load(acquire)` | `INT.getAcquire` |
| `session`, `heartbeat`, `state` | release / acquire | same pattern | same pattern |
| Record bytes, header fields | plain | `memcpy` | `MemorySegment.get/set`, `MemorySegment.copy` |

`LONG`/`INT` are `ValueLayout.JAVA_LONG.varHandle()` / `JAVA_INT.varHandle()`. Every atomic field is
naturally aligned in a mapped view (64 KiB-aligned base), which aligned VarHandle access requires.
Release/acquire pairs give the needed happens-before: everything written before a `head` release
(the records), or before a `session` release (the side block), is visible to the reader after its
acquire. On x64 these compile to plain moves plus compiler barriers. The orderings matter for the
compiler and the JIT, and for ARM64 if it is ever a target.

## 7. Messages

Type ranges: `1–0xFF` core (`1–7` since v1.0, `8–13` since v1.1), `0x100–0x7EFF` reserved for later phases, `0x7F00–0x7FFF`
test/debug only. Every core payload has a **fixed size**. "Dir" says who may send it: H = host, M = MC.
A message received from a side that may not send it is counted as malformed and ignored.

| Type | Name | Dir | Payload bytes |
|---|---|---|---|
| `0` | PAD | both | — (ring framing only) |
| `1` | `HELLO` | H, M | 64 |
| `2` | `HEARTBEAT` | H, M | 32 |
| `3` | `PLAYER_STATE` | H (M reserved, §7.3) | 64 |
| `4` | `BLOCK_SET` | H, M | 24 |
| `5` | `BLOCK_BREAK_REQUEST` | H, M | 24 |
| `6` | `BLOCK_PLACE_REQUEST` | H, M | 24 |
| `7` | `LOG` | H, M | 264 |
| `8` | `REMOTE_PLAYER_JOIN` | M | 64 (v1.1) |
| `9` | `REMOTE_PLAYER_STATE` | M | 64 (v1.1) |
| `10` | `REMOTE_PLAYER_LEAVE` | M | 8 (v1.1) |
| `11` | `TERRAIN_REQUEST` | M | 16 (v1.1) |
| `12` | `TERRAIN_PATCH` | H | 1296 (v1.1) |
| `13` | `SESSION_INFO` | M | 96 (v1.1) |
| `0x7F00` | `TEST_PATTERN` | H, M | 16–272 (variable, test only) |

### 7.1 `HELLO` (1), 64 bytes

Sent right after attach and again whenever a new peer session is seen.

| Offset | Size | Type | Field | Notes |
|---|---|---|---|---|
| 0 | 2 | u16 | `versionMajor` | `1` |
| 2 | 2 | u16 | `versionMinor` | `1` (v1.1) |
| 4 | 4 | u32 | `role` | `1` host, `2` MC |
| 8 | 4 | u32 | `pid` | |
| 12 | 4 | u32 | `session` | same as the record header's |
| 16 | 4 | u32 | `capabilities` | bitmask, `0` in v1.0 |
| 20 | 2 | u16 | `softwareBytes` | ≤ 40 |
| 22 | 2 | | reserved | |
| 24 | 40 | u8[40] | `software` | UTF-8, e.g. `CraftV-MockHost 0.1.0` |

A `HELLO` with a different `versionMajor` makes the receiver log an error and refuse gameplay messages.

### 7.2 `HEARTBEAT` (2), 32 bytes

Liveness comes from the header counters (§5.3). This message measures round-trip time.

| Offset | Size | Type | Field | Notes |
|---|---|---|---|---|
| 0 | 8 | u64 | `sentUs` | sender's QPC µs when sent |
| 8 | 8 | u64 | `echoUs` | `sentUs` of the newest `HEARTBEAT` received from the peer, `0` if none |
| 16 | 8 | u64 | `echoHoldUs` | µs between receiving that heartbeat and sending this one |
| 24 | 4 | u32 | `counter` | sender's side-block heartbeat counter (low 32 bits) |
| 28 | 4 | | reserved | |

`rttUs = nowUs − echoUs − echoHoldUs` (ignored when `echoUs == 0`).

### 7.3 `PLAYER_STATE` (3), 64 bytes

Phase 1/2: **host → MC**. MC moves its player there ("puppet" mode). The **MC → host** direction is
reserved for the Phase 3 option where MC physics drives the ped. A v1.0 host ignores it.

| Offset | Size | Type | Field | Notes |
|---|---|---|---|---|
| 0 | 8 | f64 | `x` | feet position, MC blocks |
| 8 | 8 | f64 | `y` | |
| 16 | 8 | f64 | `z` | |
| 24 | 4 | f32 | `vx` | velocity, blocks per **second** |
| 28 | 4 | f32 | `vy` | |
| 32 | 4 | f32 | `vz` | |
| 36 | 4 | f32 | `yaw` | MC degrees |
| 40 | 4 | f32 | `pitch` | MC degrees |
| 44 | 4 | u32 | `flags` | bit 0 `ON_GROUND`, bit 1 `TELEPORT` (snap, don't interpolate). Other bits 0 |
| 48 | 8 | u64 | `timeUs` | sender QPC µs when sampled |
| 56 | 4 | u32 | `frame` | sender's frame/tick counter |
| 60 | 4 | | reserved | |

The receiver validates: every float is finite, `|x|,|z| <= 30,000,000`, `-2048 <= y <= 4096`,
`-90 <= pitch <= 90`, `|v| <= 1000`. Anything else is malformed and ignored.

### 7.4 `BLOCK_SET` (4), 24 bytes

**M → H: authoritative.** "Block `(x,y,z)` is now `blockId`." MC sends one for every change in its
world that the host must mirror. **H → M: a host-originated edit** (debug command, or later the host
mirroring RDR2 geometry as blocks). MC applies it and answers with its own authoritative `BLOCK_SET`
flagged `ECHO`. MC is always the authority.

| Offset | Size | Type | Field | Notes |
|---|---|---|---|---|
| 0 | 4 | i32 | `x` | block coordinates |
| 4 | 4 | i32 | `y` | |
| 8 | 4 | i32 | `z` | |
| 12 | 4 | u32 | `blockId` | Minecraft **block-state id** (`Block.getId(state)`) for the pinned MC version. **`0` = air (removal)** |
| 16 | 4 | u32 | `flags` | bit 0 `ECHO` (M→H: result of a host-originated edit). Other bits 0 |
| 20 | 4 | u32 | `requestId` | the `requestId` that caused it, `0` = unsolicited |

Block-state ids depend on the exact MC version and mod set. They are stable for the pinned 26.3 +
CraftV set. A `BLOCK_PALETTE` message (id → name) is planned for Phase 4 so the host can choose props by name.

### 7.5 `BLOCK_BREAK_REQUEST` (5) and `BLOCK_PLACE_REQUEST` (6), 24 bytes each

**Player action intents.** Either side may send them:

- **H → M** (Phase 4): the host's raycast picked a block. MC performs the action as its player
  would (tool, game mode, inventory) and reports the outcome with `BLOCK_SET` (`requestId` echoed).
- **M → H** (Phase 1 onward): MC's own player did this (for example in the dev client). Informational: the
  host may log it or play effects, and Phase 5 may use break intents to hide RDR2 objects. The world
  change itself always arrives separately as `BLOCK_SET`.

| Offset | Size | Type | Field | Notes |
|---|---|---|---|---|
| 0 | 4 | u32 | `requestId` | sender-chosen, non-zero, increasing per session |
| 4 | 4 | i32 | `x` | break: the block. Place: the block clicked against |
| 8 | 4 | i32 | `y` | |
| 12 | 4 | i32 | `z` | |
| 16 | 1 | u8 | `face` | MC `Direction` ordinal: 0 down, 1 up, 2 north, 3 south, 4 west, 5 east, `0xFF` unknown |
| 17 | 3 | | reserved | |
| 20 | 4 | u32 | break: `flags` (0) / place: `blockId` | place: state to place, `0` = whatever the player holds |

For a place, the target cell is the clicked block moved one step along `face`, unless the clicked
block is replaceable (grass, snow layer), in which case it is the clicked block itself. MC decides.

### 7.6 `LOG` (7), 264 bytes

| Offset | Size | Type | Field | Notes |
|---|---|---|---|---|
| 0 | 1 | u8 | `level` | 0 trace, 1 debug, 2 info, 3 warn, 4 error |
| 1 | 1 | | reserved | |
| 2 | 2 | u16 | `textBytes` | ≤ 256 |
| 4 | 4 | | reserved | |
| 8 | 256 | u8[256] | `text` | UTF-8, `textBytes` long, rest zero |

Receivers print it into their own log, prefixed with the peer's role. Senders rate-limit.

### 7.8 `REMOTE_PLAYER_JOIN` (8), 64 bytes, M → H (v1.1)

A friend is now in the mirror world (joined the server, or came back from another dimension).

| Offset | Size | Type | Field | Notes |
|---|---|---|---|---|
| 0 | 4 | u32 | `playerId` | non-zero. MC's entity id: unique while that MC process runs. Used by §7.9 and §7.10 |
| 4 | 4 | u32 | `flags` | `0` in v1.1 |
| 8 | 16 | u8[16] | `uuid` | the Minecraft account UUID, most significant byte first |
| 24 | 2 | u16 | `nameBytes` | 1–32 |
| 26 | 6 | | reserved | |
| 32 | 32 | u8[32] | `name` | UTF-8 player name (Minecraft names are at most 16 ASCII characters) |

### 7.9 `REMOTE_PLAYER_STATE` (9), 64 bytes, M → H (v1.1)

One per friend per MC server tick (20 Hz) while the link is `CONNECTED`.

| Offset | Size | Type | Field | Notes |
|---|---|---|---|---|
| 0 | 4 | u32 | `playerId` | from §7.8 |
| 4 | 4 | u32 | `flags` | bit 0 `ON_GROUND`, 1 `CROUCHING`, 2 `SPRINTING`, 3 `SWIMMING`, 4 `GLIDING` (elytra), 5 `FLYING` (creative flight), 6 `IN_WATER`, 7 `SWING` (an arm swing started since the previous state), 8 `SLEEPING`, 9 `RIDING`. Other bits 0 |
| 8 | 8 | f64 | `x` | feet position, MC blocks |
| 16 | 8 | f64 | `y` | |
| 24 | 8 | f64 | `z` | |
| 32 | 4 | f32 | `vx` | blocks per second, from the change in position since the previous tick |
| 36 | 4 | f32 | `vy` | |
| 40 | 4 | f32 | `vz` | |
| 44 | 4 | f32 | `yaw` | head yaw, MC degrees |
| 48 | 4 | f32 | `pitch` | MC degrees |
| 52 | 4 | f32 | `bodyYaw` | MC degrees |
| 56 | 4 | u32 | `tick` | MC server tick counter |
| 60 | 1 | u8 | `gameMode` | 0 survival, 1 creative, 2 adventure, 3 spectator |
| 61 | 1 | u8 | `health` | health rounded up, clamped to 0–255 (20 = full) |
| 62 | 2 | | reserved | |

Validation: as `PLAYER_STATE` (§7.3) for the floats and ranges, `playerId != 0`, `gameMode <= 3`, no unknown flags.

### 7.10 `REMOTE_PLAYER_LEAVE` (10), 8 bytes, M → H (v1.1)

| Offset | Size | Type | Field | Notes |
|---|---|---|---|---|
| 0 | 4 | u32 | `playerId` | from §7.8 |
| 4 | 4 | u32 | `reason` | 0 `LEFT` (disconnected), 1 `OTHER_DIMENSION` (left the mirror world), 2 `RESET` (MC's world is closing) |

**Friend tracking rules (§7.8–7.10).** MC sends `REMOTE_PLAYER_JOIN` before any state for that id. When the link
becomes `CONNECTED`, and again when the host restarts (new host session), MC re-sends `REMOTE_PLAYER_JOIN` for
every friend currently in the mirror world, because the host forgot them. A `REMOTE_PLAYER_STATE` for an id the host
doesn't know is ignored (normal during that race). A `REMOTE_PLAYER_JOIN` for a known id replaces it. Before its
world closes, MC sends `REMOTE_PLAYER_LEAVE` with `RESET` for every friend. The host's own player (the integrated
server's owner, driven by `PLAYER_STATE`) is never reported.

### 7.11 `TERRAIN_REQUEST` (11), 16 bytes, M → H (v1.1)

"I need the ground of chunk column `(chunkX, chunkZ)`." Terrain is pulled by MC, so the host scans wherever any
player is, not only around its own player.

| Offset | Size | Type | Field | Notes |
|---|---|---|---|---|
| 0 | 4 | i32 | `chunkX` | `blockX >> 4` |
| 4 | 4 | i32 | `chunkZ` | `blockZ >> 4` |
| 8 | 4 | u32 | `requestId` | non-zero, increasing per MC session |
| 12 | 2 | u16 | `distance` | chunk distance (Chebyshev) to the nearest player when requested; lower = more urgent |
| 14 | 2 | | reserved | |

Validation: `requestId != 0`, `|chunkX|, |chunkZ| <= 1,875,000`.

MC keeps at most `TERRAIN_MAX_IN_FLIGHT = 32` requests unanswered, asks for the nearest chunks first, and asks
again after `TERRAIN_RETRY_MS = 5000` without an answer. The host answers each request with one `TERRAIN_PATCH`
when it can, in any order, and may skip requests it can't serve yet (MC asks again). A chunk MC has built is
remembered in its world save and never requested again. A patch with no ground in any column isn't remembered:
MC asks for that chunk again after 60 s (the host may have lacked collision there, or used an older map edge).

### 7.12 `TERRAIN_PATCH` (12), 1296 bytes, H → M (v1.1)

The ground of one 16 × 16 chunk column. Column `i` is block `(chunkX·16 + (i & 15), chunkZ·16 + (i >> 4))`, so
`i = localZ · 16 + localX`.

| Offset | Size | Type | Field | Notes |
|---|---|---|---|---|
| 0 | 4 | i32 | `chunkX` | |
| 4 | 4 | i32 | `chunkZ` | |
| 8 | 4 | u32 | `requestId` | the `TERRAIN_REQUEST` it answers, `0` = unsolicited |
| 12 | 4 | u32 | `flags` | `0` in v1.1 |
| 16 | 512 | i16[256] | `groundY` | MC Y of the top solid block (the one you stand on), or `NO_GROUND = -32768` |
| 528 | 512 | i16[256] | `waterY` | MC Y of the top water block, or `NO_WATER = -32768`. Water fills `groundY + 1 … waterY`; ignored unless `waterY > groundY` and the column has ground |
| 1040 | 256 | u8[256] | `material` | surface material (table below) |

Validation: chunk coordinates as in §7.11; every `groundY` and `waterY` is the sentinel or within `[-2048, 4096]`.
Unknown material values are not malformed: they are treated as `UNKNOWN`. MC skips blocks outside its world's height.

| Material | Value | Meaning (MC picks the blocks: `fabric/.../terrain/TerrainBlocks.java`) |
|---|---|---|
| `UNKNOWN` | 0 | not classified |
| `GRASS` | 1 | grass, lawns, fields |
| `DIRT` | 2 | dirt, trails |
| `SAND` | 3 | beaches, desert |
| `ROCK` | 4 | rock, cliffs |
| `ROAD` | 5 | asphalt |
| `PAVEMENT` | 6 | sidewalks, concrete, plazas |
| `GRAVEL` | 7 | gravel |
| `SNOW` | 8 | snow |
| `WOOD` | 9 | wooden decks, piers |
| `METAL` | 10 | metal surfaces |
| `BUILDING` | 11 | roofs and building tops |
| `MUD` | 12 | mud, swamp |

### 7.13 `SESSION_INFO` (13), 96 bytes, M → H (v1.1)

How friends can join, for the host's overlay. Sent when the link becomes `CONNECTED`, when the host restarts, and
whenever a field changes (at most once a second).

| Offset | Size | Type | Field | Notes |
|---|---|---|---|---|
| 0 | 4 | u32 | `flags` | bit 0 `OPEN` (friends can join), bit 1 `AUTH` (Minecraft accounts are verified), bit 2 `WHITELIST`. Other bits 0 |
| 4 | 2 | u16 | `port` | TCP port friends connect to, `0` when not open |
| 6 | 2 | u16 | `friends` | friends connected now |
| 8 | 2 | u16 | `maxPlayers` | |
| 10 | 1 | u8 | `gameMode` | friends' game mode (as §7.9) |
| 11 | 1 | | reserved | |
| 12 | 2 | u16 | `addressBytes` | ≤ 64 |
| 14 | 2 | | reserved | |
| 16 | 64 | u8[64] | `address` | UTF-8, best effort, e.g. `192.168.1.23:25565` |
| 80 | 16 | | reserved | |

Validation: no unknown flags, `gameMode <= 3`, `addressBytes <= 64`.

### 7.14 `TEST_PATTERN` (0x7F00), 16 + n bytes (n ≤ 256), test only

Used by the stress and chaos tests. Normal builds ignore it unless a test mode is on.

| Offset | Size | Type | Field |
|---|---|---|---|
| 0 | 8 | u64 | `index` (0, 1, 2, … per stream) |
| 8 | 4 | u32 | `fillBytes` (= n) |
| 12 | 4 | u32 | `checksum` = FNV-1a-32 of the fill bytes |
| 16 | n | u8[n] | fill: byte `i` = `(index · 31 + i) & 0xFF` |

The stress sender uses `n = index mod 257`, so record sizes vary and wrap-around is exercised.

## 8. Defensive rules (both sides)

- Validate every size, offset and count read from shared memory before using it (§3.2, §4.2).
  Never index with a peer-written value without a bounds check.
- Never block: a full ring drops the message and counts it. The host's per-tick drain is bounded
  (`MAX_DRAIN_BYTES_PER_TICK = 256 KiB`).
- Validate every decoded value (§7.3 ranges, `face <= 5` or `0xFF`, `textBytes <= 256`,
  `softwareBytes <= 40`, the §7.8–7.13 rules) and treat out-of-range values as malformed.
- Messages from the wrong direction are malformed.
- Count everything (sent, received, dropped-full, stale, unknown, malformed, corrupt) and expose the
  counts in logs and the debug overlay.

## 9. Versioning

- `versionMajor` changes when a v1 reader could misread the layout: header, section table, ring control,
  record header, or an existing payload field changing meaning. Peers with different majors don't attach.
  The mapping name embeds the major (`_v1`), so mismatched majors don't even meet.
- `versionMinor` changes for compatible additions: new message types, new sections, new
  fields **appended** to a payload (with its `typeVersion` bumped). Readers ignore what they don't know
  and read known prefixes.
- Every change updates this file, both mirrors, the goldens, and gets an entry in `DECISIONS.md`.
- **History:** v1.0 (2026-10-02) core messages 1–7. v1.1 (2026-10-05) co-op messages 8–13 (`DECISIONS.md`
  D-016). A v1.0 peer skips them as unknown types, so v1.0 and v1.1 still link up.

## 10. Golden vectors

`protocol/golden/golden_vectors.txt` holds one line per message: `name hex-bytes`, where the bytes
are the **complete record** (header + payload + padding) with `session = 0x11223344`, `seq = 7`.
The C++ and Java test suites both encode the documented field values and compare byte for byte,
then decode the bytes and compare fields. This is how the two languages are proven to agree.
