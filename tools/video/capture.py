"""Records Minecraft's passthrough picture (PROTOCOL.md §11) as transparent PNG frames for the video:
python capture.py NAME SECONDS [FPS]  ->  public/shots/NAME/0000.png ...
The terrain is hidden in that picture, so what's left (Steve, blocks, the hand, the HUD) sits on a transparent background.
Frames are copied raw while recording (fast) and turned into PNGs afterwards, in parallel."""
import concurrent.futures
import json
import mmap
import pathlib
import struct
import sys
import time

import numpy as np
from PIL import Image

NAME = "Local\\CraftV_Frame_v1"
HEADER, SLOT_DESC, SLOT_DESC_BYTES, SLOTS = 4096, 256, 128, 3
LAYER_MAX = 2560 * 1440 * 4
STRIDE = LAYER_MAX * 3


def grab_raw(view, last):
    """The newest published frame's (published, w, h, flags, world bytes, overlay bytes), or None if nothing new."""
    published = struct.unpack_from("<q", view, 32)[0]
    if published == last:
        return None
    slot = struct.unpack_from("<i", view, 40)[0]
    if slot < 0 or slot >= SLOTS:
        return None
    desc = SLOT_DESC + SLOT_DESC_BYTES * slot
    seq = struct.unpack_from("<Q", view, desc)[0]
    if seq & 1:
        return None
    w, h = struct.unpack_from("<II", view, desc + 24)
    flags = struct.unpack_from("<I", view, desc + 44)[0]
    n = w * h * 4
    base = HEADER + STRIDE * slot
    world = view[base:base + n]
    overlay = view[base + 2 * n:base + 3 * n]
    if struct.unpack_from("<Q", view, desc)[0] != seq:
        return None
    return published, w, h, flags, world, overlay


def convert(args):
    raw, png, w, h, flags = args
    data = np.fromfile(raw, dtype=np.uint8)
    n = w * h * 4
    world = data[:n].reshape(h, w, 4).astype(np.float32) / 255
    overlay = data[n:].reshape(h, w, 4).astype(np.float32) / 255
    # both layers are premultiplied: overlay over world over nothing
    a = overlay[..., 3:4] + world[..., 3:4] * (1 - overlay[..., 3:4])
    c = overlay[..., :3] + world[..., :3] * (1 - overlay[..., 3:4])
    rgb = np.where(a > 0, c / np.maximum(a, 1e-6), 0)
    out = np.concatenate([rgb, a], axis=2)
    if flags & 2:  # bottom-up rows
        out = out[::-1]
    Image.fromarray((np.clip(out, 0, 1) * 255 + 0.5).astype(np.uint8), "RGBA").save(png, compress_level=3)
    pathlib.Path(raw).unlink()


def main():
    if sys.argv[1] == "--convert":  # leftover raw frames of a shot (a conversion that ran out of memory)
        out = pathlib.Path(__file__).parent / "public" / "shots" / sys.argv[2]
        w, h, flags = int(sys.argv[3]), int(sys.argv[4]), 2
        jobs = [(str(r), str(r.with_suffix(".png")), w, h, flags) for r in sorted(out.glob("*.raw"))]
        with concurrent.futures.ProcessPoolExecutor(max_workers=3) as pool:
            list(pool.map(convert, jobs))
        print(f"converted {len(jobs)}")
        return
    name, seconds = sys.argv[1], float(sys.argv[2])
    fps = float(sys.argv[3]) if len(sys.argv) > 3 else 15
    out = pathlib.Path(__file__).parent / "public" / "shots" / name
    out.mkdir(parents=True, exist_ok=True)
    for old in list(out.glob("*.png")) + list(out.glob("*.raw")):
        old.unlink()
    view = mmap.mmap(-1, HEADER + STRIDE * SLOTS, tagname=NAME, access=mmap.ACCESS_READ)
    jobs, last, start, count = [], None, time.time(), 0
    while time.time() - start < seconds:
        due = start + count / fps
        if time.time() < due:
            time.sleep(due - time.time())
        got = grab_raw(view, last)
        if got is None:
            time.sleep(0.003)
            continue
        last, w, h, flags, world, overlay = got
        raw = out / f"{count:04d}.raw"
        with open(raw, "wb") as f:
            f.write(world)
            f.write(overlay)
        jobs.append((str(raw), str(out / f"{count:04d}.png"), w, h, flags))
        count += 1
    with concurrent.futures.ProcessPoolExecutor(max_workers=3) as pool:  # Minecraft is running too: keep memory low
        list(pool.map(convert, jobs))
    (out / "info.json").write_text(json.dumps({"frames": count, "fps": fps, "width": jobs[0][2] if jobs else 0, "height": jobs[0][3] if jobs else 0}))
    print(f"{name}: {count} frames at {fps:g} fps")


if __name__ == "__main__":
    main()
