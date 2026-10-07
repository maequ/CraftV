import React from "react";
import { Audio, Easing, Img, Sequence, interpolate, spring, staticFile, useCurrentFrame } from "remotion";
import shots from "./shots.json";
import { C, F, FPS } from "./theme";

// ---- motion helpers ---------------------------------------------------------------------------------
export const clamp = { extrapolateLeft: "clamp", extrapolateRight: "clamp" } as const;
const OUT = Easing.bezier(0.16, 1, 0.3, 1);
/** 0..1 between frames a and b, eased out. */
export const prog = (f: number, a: number, b: number, easing = OUT) => interpolate(f, [a, b], [0, 1], { ...clamp, easing });
/** A springy 0..1 starting at frame `at`. */
export const pop = (f: number, at: number, damping = 13, stiffness = 170) =>
  spring({ frame: f - at, fps: FPS, config: { damping, stiffness, mass: 0.9 } });
export const lerp = (a: number, b: number, t: number) => a + (b - a) * t;
export const sec = (s: number) => Math.round(s * FPS);

/** A sound effect at frame `at` of the current sequence. */
export const Sfx: React.FC<{ at: number; name: string; volume?: number }> = ({ at, name, volume = 0.45 }) => (
  <Sequence from={Math.max(0, Math.round(at))} durationInFrames={sec(2)} layout="none">
    <Audio src={staticFile(`sfx/${name}.wav`)} volume={volume} />
  </Sequence>
);

// ---- recorded Minecraft footage (tools/video/capture.py) -----------------------------------------
type ShotName = keyof typeof shots;
/**
 * A recorded shot, framed so the part of the picture that has something in it fills the box (`fit` of it).
 * `at`: the frame it starts playing; it loops unless `hold`.
 */
export const Shot: React.FC<{ name: ShotName; w: number; h: number; at?: number; fit?: number; speed?: number; hold?: boolean;
  dx?: number; dy?: number; full?: boolean; style?: React.CSSProperties }> = ({ name, w, h, at = 0, fit = 0.92, speed = 1, hold = false, dx = 0, dy = 0, full = false, style }) => {
  const f = useCurrentFrame();
  const s = shots[name];
  const raw = Math.floor(Math.max(0, f - at) * s.fps * speed / FPS);
  const i = hold ? Math.min(raw, s.frames - 1) : raw % s.frames;
  const [x0, y0, x1, y1] = full ? [0, 0, s.width, s.height] : (s.box as number[]);
  const bw = x1 - x0, bh = y1 - y0;
  const k = Math.min(w / bw, h / bh) * fit;
  const left = w / 2 - (x0 + bw / 2) * k + dx, top = h / 2 - (y0 + bh / 2) * k + dy;
  return (
    <div style={{ position: "relative", width: w, height: h, overflow: "hidden", ...style }}>
      <Img src={staticFile(`shots/${name}/${String(i).padStart(4, "0")}.png`)}
        style={{ position: "absolute", left, top, width: s.width * k, height: s.height * k, imageRendering: "pixelated" }} />
    </div>
  );
};

// ---- the backdrop: Los Santos at dusk, in blocks -------------------------------------------------
const rnd = (seed: number) => {
  const x = Math.sin(seed * 9301 + 49297) * 233280;
  return x - Math.floor(x);
};
const layer = (n: number, seed: number, minH: number, steps: number, tall: number) =>
  Array.from({ length: n }, (_, i) => ({
    w: 40 + Math.floor(rnd(i + seed) * 5) * 20,
    h: minH + Math.floor(rnd(i + seed + 100) * steps) * 22 + (i % 9 === 4 ? tall : 0),
    lit: rnd(i + seed + 300),
  }));
const FAR = layer(60, 1000, 90, 8, 160);
const NEAR = layer(46, 0, 60, 7, 200);
const City: React.FC<{ blocks: ReturnType<typeof layer>; drift: number; fill: string; window: number; seed: number }> = ({ blocks, drift, fill, window, seed }) => {
  const total = blocks.reduce((s, b) => s + b.w + 4, 0);
  let x = -(drift % total);
  const out: React.ReactNode[] = [];
  for (let rep = 0; rep < 3; rep++) {
    blocks.forEach((b, i) => {
      const bx = x;
      x += b.w + 4;
      if (bx > 1960 || bx + b.w < -40) return;
      const windows: React.ReactNode[] = [];
      if (window > 0)
        for (let wy = 1080 - b.h + 16; wy < 1064; wy += 26)
          for (let wx = 9; wx < b.w - 9; wx += 18)
            if (rnd(i * 97 + wx * 13 + wy * 7 + seed) < 0.22 + b.lit * 0.2)
              windows.push(<rect key={`${wx}-${wy}`} x={bx + wx} y={wy} width={7} height={10} fill={rnd(wx + wy + seed) > 0.5 ? "#ffcf7a" : "#ffe9b0"} opacity={window} />);
      out.push(<g key={`${rep}-${i}`}><rect x={bx} y={1080 - b.h} width={b.w} height={b.h} fill={fill} />{windows}</g>);
    });
  }
  return <>{out}</>;
};
export const Backdrop: React.FC<{ tint?: string; dim?: number }> = ({ tint, dim = 0 }) => {
  const f = useCurrentFrame();
  return (
    <div style={{ position: "absolute", inset: 0, background: `linear-gradient(180deg, ${C.dusk1} 0%, ${C.dusk2} 50%, ${C.dusk3} 80%, ${C.sun} 100%)` }}>
      <div style={{ position: "absolute", left: 1240, top: 600, width: 420, height: 420, borderRadius: "50%",
        background: "radial-gradient(circle, #ffd27a 0%, #ff9a3c 40%, rgba(255,120,60,0) 70%)", opacity: 0.85 }} />
      <svg width={1920} height={1080} style={{ position: "absolute", inset: 0 }}>
        <City blocks={FAR} drift={f * 0.12} fill="#3a2350" window={0.18} seed={7} />
        <City blocks={NEAR} drift={f * 0.3} fill="#0e0f1e" window={0.5} seed={1} />
      </svg>
      {tint && <div style={{ position: "absolute", inset: 0, background: tint }} />}
      <div style={{ position: "absolute", inset: 0, background: `rgba(6,8,16,${0.3 + dim})` }} />
      <div style={{ position: "absolute", inset: 0, background: "radial-gradient(ellipse at center, rgba(0,0,0,0) 55%, rgba(0,0,0,0.55) 100%)" }} />
    </div>
  );
};

/** Little pixel icons: a heart, a drumstick, a hotbar. */
export const PixelIcon: React.FC<{ kind: "heart" | "food" | "hotbar"; size?: number }> = ({ kind, size = 44 }) => {
  const maps = {
    heart: ["0110110", "1221111", "1211111", "1111111", "0111110", "0011100", "0001000"],
    food: ["0000110", "0001221", "0012211", "0122110", "1221100", "3310000", "3300000"],
    hotbar: ["1111111", "1222221", "1211121", "1211121", "1222221", "1111111", "0000000"],
  } as const;
  const pal: Record<string, Record<string, string>> = {
    heart: { "1": "#d8262b", "2": "#ff8a8a" },
    food: { "1": "#8a4a1e", "2": "#c8742f", "3": "#e8e0d0" },
    hotbar: { "1": "#c9ced8", "2": "#5d6475" },
  };
  const rows = maps[kind];
  const px = size / 7;
  return (
    <svg width={size} height={size}>
      {rows.flatMap((r, y) => r.split("").map((c, x) => (c === "0" ? null : <rect key={`${x}-${y}`} x={x * px} y={y * px} width={px + 0.5} height={px + 0.5} fill={pal[kind][c]} />)))}
    </svg>
  );
};

// ---- a Minecraft grass block, drawn in pixels -----------------------------------------------------
export const GrassBlock: React.FC<{ size: number; kind?: "grass" | "tnt" | "plank" }> = ({ size, kind = "grass" }) => {
  const n = 8, cells: React.ReactNode[] = [];
  const face = (pts: (u: number, v: number) => [number, number], color: (u: number, v: number) => string, key: string) => {
    for (let u = 0; u < n; u++)
      for (let v = 0; v < n; v++) {
        const p = [pts(u, v), pts(u + 1, v), pts(u + 1, v + 1), pts(u, v + 1)].map((q) => q.join(",")).join(" ");
        cells.push(<polygon key={`${key}${u}-${v}`} points={p} fill={color(u, v)} stroke={color(u, v)} strokeWidth={0.6} />);
      }
  };
  const s = size / 2, hx = s * 0.866, hy = s * 0.5;
  const top = (u: number, v: number): [number, number] => [s + (u - v) * hx / n, (u + v) * hy / n];
  const left = (u: number, v: number): [number, number] => [s - hx + u * hx / n, hy + u * hy / n + v * s / n];
  const right = (u: number, v: number): [number, number] => [s + u * hx / n, 2 * hy - u * hy / n + v * s / n];
  const shade = (c: [number, number, number], k: number, u: number, v: number) => {
    const j = (rnd(u * 31 + v * 17 + k * 7) - 0.5) * 26;
    return `rgb(${c.map((x) => Math.max(0, Math.min(255, x * k + j))).join(",")})`;
  };
  if (kind === "tnt") {
    face(top, (u, v) => shade(u > 2 && u < 5 && v > 2 && v < 5 ? [60, 60, 60] : [200, 60, 50], 1.0, u, v), "t");
    const side = (k: number) => (u: number, v: number) => shade(v >= 3 && v <= 4 ? [235, 235, 235] : [215, 60, 45], k, u, v);
    face(left, side(0.82), "l");
    face(right, side(0.66), "r");
  } else if (kind === "plank") {
    const p = (k: number) => (u: number, v: number) => shade(v % 3 === 2 ? [140, 100, 55] : [180, 135, 80], k, u, v);
    face(top, p(1), "t"); face(left, p(0.82), "l"); face(right, p(0.66), "r");
  } else {
    face(top, (u, v) => shade([98, 170, 60], 1.0, u, v), "t");
    const side = (k: number) => (u: number, v: number) => shade(v < 2 || (v === 2 && rnd(u * 5) > 0.5) ? [98, 170, 60] : [134, 90, 50], k, u, v);
    face(left, side(0.82), "l");
    face(right, side(0.66), "r");
  }
  return <svg width={size} height={size} viewBox={`0 0 ${size} ${size}`}>{cells}</svg>;
};

// ---- UI pieces ----------------------------------------------------------------------------------------
export const Glass: React.FC<{ style?: React.CSSProperties; children?: React.ReactNode }> = ({ style, children }) => (
  <div style={{ background: C.panel, border: `1px solid ${C.line}`, borderRadius: 22, boxShadow: "0 24px 70px rgba(0,0,0,0.45)",
    backdropFilter: "blur(10px)", ...style }}>{children}</div>
);

export const Label: React.FC<{ children: React.ReactNode; color?: string; style?: React.CSSProperties }> = ({ children, color = C.grass, style }) => (
  <div style={{ fontFamily: F.pixel, fontSize: 24, letterSpacing: 2, color, textTransform: "uppercase", ...style }}>{children}</div>
);

export const Kbd: React.FC<{ k: string; lit?: number; size?: number }> = ({ k, lit = 0, size = 46 }) => (
  <div style={{ fontFamily: F.mono, fontWeight: 700, fontSize: size * 0.62, color: lit > 0.5 ? "#0b0d17" : C.text, minWidth: size * 1.25,
    height: size * 1.25, padding: `0 ${size * 0.35}px`, display: "inline-flex", alignItems: "center", justifyContent: "center", borderRadius: size * 0.22,
    background: lit > 0 ? `rgba(98,193,59,${0.25 + lit * 0.75})` : "#2a3044", border: `2px solid ${lit > 0 ? C.grass : "#454d66"}`,
    borderBottomWidth: size * 0.14, boxShadow: lit > 0 ? `0 0 ${30 * lit}px rgba(98,193,59,0.6)` : "none", transform: `translateY(${lit * 3}px)` }}>{k}</div>
);

const ICON: Record<string, { bg: string; label: string }> = {
  dll: { bg: "#5b6b8c", label: "DLL" }, asi: { bg: C.grassDark, label: "ASI" }, ini: { bg: "#b08a2e", label: "INI" },
  exe: { bg: "#2f6fd1", label: "EXE" }, jar: { bg: "#c4672b", label: "JAR" }, zip: { bg: "#7b4fb3", label: "ZIP" }, log: { bg: "#555c70", label: "LOG" },
};
export const FileIcon: React.FC<{ kind: string; size?: number }> = ({ kind, size = 34 }) => {
  if (kind === "folder")
    return (
      <div style={{ width: size, height: size * 0.82, position: "relative" }}>
        <div style={{ position: "absolute", left: 0, top: 0, width: size * 0.45, height: size * 0.25, background: "#e8b34a", borderRadius: "4px 4px 0 0" }} />
        <div style={{ position: "absolute", left: 0, top: size * 0.16, width: size, height: size * 0.66, background: "#f5c55d", borderRadius: 4 }} />
      </div>
    );
  const i = ICON[kind] || ICON.log;
  return (
    <div style={{ width: size * 0.8, height: size, background: i.bg, borderRadius: 5, position: "relative", display: "flex", alignItems: "flex-end",
      justifyContent: "center", paddingBottom: 3, fontFamily: F.ui, fontWeight: 800, fontSize: size * 0.27, color: "white" }}>
      <div style={{ position: "absolute", right: 0, top: 0, width: size * 0.26, height: size * 0.26, background: "rgba(255,255,255,0.35)", borderBottomLeftRadius: 4 }} />
      {i.label}
    </div>
  );
};

export type FileItem = { name: string; kind: string; enter?: number; flash?: number; selected?: boolean; dim?: boolean; rename?: { at: number; to: string; kind?: string } };

/** An Explorer-style window: a path bar and file rows. Rows with `enter` slide in at that frame and flash green. */
export const Explorer: React.FC<{ title: string; path: string; files: FileItem[]; w: number; h: number; style?: React.CSSProperties }> = ({ title, path, files, w, h, style }) => {
  const f = useCurrentFrame();
  return (
    <Window title={title} w={w} h={h} style={style}>
      <div style={{ height: 52, display: "flex", alignItems: "center", padding: "0 18px", gap: 10, borderBottom: `1px solid ${C.line}`, fontFamily: F.ui, fontSize: 19, color: C.muted }}>
        <span style={{ opacity: 0.6 }}>‹  ›</span>
        <div style={{ flex: 1, background: "#151927", borderRadius: 8, padding: "7px 14px", color: C.text, whiteSpace: "nowrap", overflow: "hidden" }}>{path}</div>
      </div>
      <div style={{ padding: "10px 12px" }}>
        {files.map((file) => {
          const shown = file.enter === undefined ? 1 : pop(f, file.enter, 15);
          if (file.enter !== undefined && f < file.enter) return null;
          const flash = file.enter !== undefined ? Math.max(0, 1 - (f - file.enter) / sec(1.4)) : 0;
          let name = file.name;
          let editing = false;
          if (file.rename && f >= file.rename.at) {
            const t = f - file.rename.at;
            const erase = sec(0.5), per = 2;
            if (t < erase) name = file.name.slice(0, Math.max(0, Math.round(file.name.length * (1 - t / erase))));
            else name = file.rename.to.slice(0, Math.floor((t - erase) / per));
            editing = t < erase + file.rename.to.length * per + sec(0.4);
          }
          const kind = file.rename?.kind && f >= file.rename.at + sec(0.5) + file.rename.to.length * 2 ? file.rename.kind : file.kind;
          return (
            <div key={file.name} style={{ height: 64, display: "flex", alignItems: "center", gap: 18, padding: "0 16px", borderRadius: 10, marginBottom: 4,
              transform: `translateX(${(1 - shown) * 40}px)`, opacity: Math.min(1, shown * 1.5) * (file.dim ? 0.45 : 1),
              background: file.selected || editing ? "rgba(59,130,246,0.28)" : flash > 0 ? `rgba(98,193,59,${0.35 * flash})` : "transparent",
              outline: file.selected ? `2px solid rgba(59,130,246,0.7)` : "none" }}>
              <FileIcon kind={kind} size={40} />
              <span style={{ fontFamily: F.ui, fontSize: 28, fontWeight: 500, color: C.text, display: "flex", alignItems: "center" }}>
                {editing ? <span style={{ background: "#0f2a55", padding: "2px 6px", borderRadius: 4, border: "1px solid #3b82f6" }}>{name}<span style={{ opacity: f % 20 < 10 ? 1 : 0 }}>|</span></span> : name}
              </span>
              {flash > 0.2 && <span style={{ marginLeft: "auto", fontFamily: F.pixel, fontSize: 18, color: C.grass, opacity: flash }}>NEW</span>}
            </div>
          );
        })}
      </div>
    </Window>
  );
};

export const Window: React.FC<{ title: string; w: number; h: number; style?: React.CSSProperties; children?: React.ReactNode; accent?: string }> = ({ title, w, h, style, children, accent }) => (
  <div style={{ width: w, height: h, borderRadius: 14, background: C.win, border: "1px solid rgba(255,255,255,0.12)", boxShadow: "0 40px 90px rgba(0,0,0,0.55)",
    overflow: "hidden", display: "flex", flexDirection: "column", ...style }}>
    <div style={{ height: 44, flex: "none", background: C.winBar, display: "flex", alignItems: "center", padding: "0 16px", gap: 12, fontFamily: F.ui, fontSize: 17, color: C.muted }}>
      <div style={{ width: 14, height: 14, borderRadius: 3, background: accent || C.grass }} />
      <span style={{ color: C.text }}>{title}</span>
      <div style={{ flex: 1 }} />
      <span style={{ letterSpacing: 18 }}>– ▢ ✕</span>
    </div>
    <div style={{ flex: 1, position: "relative" }}>{children}</div>
  </div>
);

/** The mouse pointer, gliding between keyframes and rippling on clicks. */
export const Cursor: React.FC<{ keys: { f: number; x: number; y: number }[]; clicks?: number[] }> = ({ keys, clicks = [] }) => {
  const f = useCurrentFrame();
  if (f < keys[0].f - 6) return null;
  let x = keys[0].x, y = keys[0].y;
  for (let i = 0; i < keys.length - 1; i++) {
    const a = keys[i], b = keys[i + 1];
    if (f >= a.f && f <= b.f) {
      const t = prog(f, a.f, b.f, Easing.bezier(0.45, 0, 0.2, 1));
      x = lerp(a.x, b.x, t);
      y = lerp(a.y, b.y, t);
    } else if (f > b.f) {
      x = b.x;
      y = b.y;
    }
  }
  const press = clicks.some((c) => f >= c && f < c + 4);
  return (
    <>
      {clicks.map((c) => {
        const t = (f - c) / 14;
        if (t < 0 || t > 1) return null;
        return <div key={c} style={{ position: "absolute", left: x - 30 * t - 10, top: y - 30 * t - 10, width: 20 + 60 * t, height: 20 + 60 * t, borderRadius: "50%",
          border: `3px solid rgba(255,255,255,${0.8 * (1 - t)})` }} />;
      })}
      <svg style={{ position: "absolute", left: x - 4, top: y - 2, transform: `scale(${press ? 0.88 : 1})`, filter: "drop-shadow(0 4px 8px rgba(0,0,0,0.5))" }}
        width={38} height={46} viewBox="0 0 19 23">
        <path d="M1 1 L1 18 L5.5 14 L8.5 21 L11.5 19.7 L8.6 13 L14.5 13 Z" fill="white" stroke="#111" strokeWidth={1.2} strokeLinejoin="round" />
      </svg>
    </>
  );
};

/** A file icon flying along an arc from one point to another (a drag and drop). */
export const FlyingFile: React.FC<{ start: number; dur: number; from: [number, number]; to: [number, number]; kind: string; name: string }> = ({ start, dur, from, to, kind, name }) => {
  const f = useCurrentFrame();
  if (f < start || f > start + dur) return null;
  const t = prog(f, start, start + dur, Easing.bezier(0.45, 0, 0.25, 1));
  const x = lerp(from[0], to[0], t), y = lerp(from[1], to[1], t) - Math.sin(Math.PI * t) * 140;
  return (
    <div style={{ position: "absolute", left: x, top: y, display: "flex", alignItems: "center", gap: 10, padding: "8px 14px", borderRadius: 10,
      background: "rgba(59,130,246,0.85)", transform: `rotate(${Math.sin(Math.PI * t) * -6}deg) scale(${1 + Math.sin(Math.PI * t) * 0.12})`,
      boxShadow: "0 18px 40px rgba(0,0,0,0.45)", fontFamily: F.ui, fontSize: 22, color: "white" }}>
      <FileIcon kind={kind} size={30} />{name}
    </div>
  );
};

/** Text that types itself from frame `at`, `cps` characters a second. */
export const Typed: React.FC<{ text: string; at: number; cps?: number; caret?: boolean; style?: React.CSSProperties }> = ({ text, at, cps = 22, caret = true, style }) => {
  const f = useCurrentFrame();
  const n = Math.max(0, Math.min(text.length, Math.floor((f - at) * cps / FPS)));
  return <span style={style}>{text.slice(0, n)}{caret && n < text.length && f >= at ? <span style={{ opacity: 0.8 }}>|</span> : null}</span>;
};

/** Typing sounds for a Typed text. */
export const TypingSfx: React.FC<{ text: string; at: number; cps?: number }> = ({ text, at, cps = 22 }) => (
  <>{Array.from({ length: Math.ceil(text.length / 2) }, (_, i) => <Sfx key={i} at={at + i * 2 * FPS / cps} name="type" volume={0.25} />)}</>
);

export const Headline: React.FC<{ children: React.ReactNode; at: number; size?: number; style?: React.CSSProperties; color?: string }> = ({ children, at, size = 110, style, color = C.text }) => {
  const f = useCurrentFrame();
  const p = pop(f, at, 14);
  return <div style={{ fontFamily: F.display, fontSize: size, lineHeight: 0.95, color, textTransform: "uppercase", letterSpacing: 1,
    transform: `translateY(${(1 - p) * 60}px)`, opacity: Math.min(1, p * 1.4), textShadow: "0 8px 30px rgba(0,0,0,0.45)", ...style }}>{children}</div>;
};
