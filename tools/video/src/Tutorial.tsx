import React from "react";
import { AbsoluteFill, Audio, Sequence, interpolate, staticFile, useCurrentFrame } from "remotion";
import music from "./music.json";
import timeline from "./timeline.json";
import { Sfx, prog } from "./kit";
import { C, F } from "./theme";
import { Beat } from "./types";
import { SCENES } from "./scenes";

const WIPE = 22; // frames a scene change's block wipe takes, centred on the cut

/** Minecraft-style transition: a wave of blocks covers the cut and clears again. */
const BlockWipe: React.FC<{ seed: number }> = ({ seed }) => {
  const f = useCurrentFrame();
  const cols = 16, rows = 9, size = 120;
  const cells: React.ReactNode[] = [];
  for (let y = 0; y < rows; y++)
    for (let x = 0; x < cols; x++) {
      const d = (seed % 2 ? cols - 1 - x : x) + y * 0.8;
      const t = f - d * 0.45;
      const s = t < WIPE / 2 ? Math.min(1, Math.max(0, t / 5)) : Math.max(0, 1 - (t - WIPE / 2) / 5);
      if (s <= 0) continue;
      const shade = (x * 7 + y * 13 + seed) % 5;
      const color = y < 1 ? ["#5aa83c", "#62c13b", "#4f9a34", "#6cc845", "#58a93a"][shade] : ["#7a4f27", "#8b5a2b", "#6f4622", "#94612f", "#80542a"][shade];
      cells.push(<div key={`${x}-${y}`} style={{ position: "absolute", left: x * size, top: y * size, width: size, height: size, background: color,
        transform: `scale(${s})`, boxShadow: "inset 0 0 0 3px rgba(0,0,0,0.12)" }} />);
    }
  return <AbsoluteFill>{cells}</AbsoluteFill>;
};

/** What's being said, with the words lighting up as they're spoken. */
const Captions: React.FC<{ beats: { beat: Beat; start: number }[] }> = ({ beats }) => {
  const f = useCurrentFrame();
  const cur = beats.find(({ beat, start }) => f >= start - 3 && f < start + beat.frames + 8);
  if (!cur) return null;
  const words = cur.beat.caption.split(" ");
  const t = Math.max(0, Math.min(1, (f - cur.start) / cur.beat.frames));
  const lit = Math.floor(t * words.length * 1.04);
  const fade = Math.min(prog(f, cur.start - 3, cur.start + 4), 1 - prog(f, cur.start + cur.beat.frames + 2, cur.start + cur.beat.frames + 8));
  return (
    <div style={{ position: "absolute", left: 0, right: 0, bottom: 54, display: "flex", justifyContent: "center", opacity: fade }}>
      <div style={{ maxWidth: 1400, padding: "16px 30px", borderRadius: 16, background: "rgba(8,10,18,0.72)", fontFamily: F.ui, fontWeight: 600, fontSize: 36,
        lineHeight: 1.35, textAlign: "center", color: "rgba(244,245,248,0.55)" }}>
        {words.map((w, i) => <span key={i} style={{ color: i < lit ? C.text : undefined }}>{w} </span>)}
      </div>
    </div>
  );
};

const PartTag: React.FC<{ part: number; frames: number }> = ({ part, frames }) => {
  const f = useCurrentFrame();
  const names = ["", "GTA V", "Minecraft", "GTA settings"];
  const show = Math.min(prog(f, 6, 18), 1 - prog(f, frames - 10, frames));
  return (
    <div style={{ position: "absolute", left: 70, top: 56, display: "flex", alignItems: "center", gap: 16, opacity: show,
      transform: `translateX(${(1 - show) * -30}px)` }}>
      {[1, 2, 3].map((p) => (
        <div key={p} style={{ width: p === part ? 64 : 18, height: 18, borderRadius: 9, background: p <= part ? C.grass : "rgba(255,255,255,0.25)" }} />
      ))}
      <div style={{ fontFamily: F.pixel, fontSize: 24, letterSpacing: 2, color: C.text, marginLeft: 6 }}>PART {part} · {names[part].toUpperCase()}</div>
    </div>
  );
};

export const Tutorial: React.FC = () => {
  let start = 0;
  const placed = timeline.scenes.map((s) => {
    const p = { ...s, start };
    start += s.frames;
    return p;
  });
  const allBeats = placed.flatMap((s) => s.beats.map((b) => ({ beat: b as Beat, start: s.start + b.from })));
  return (
    <AbsoluteFill style={{ background: C.night }}>
      {placed.map((s) => {
        const Scene = SCENES[s.id];
        return (
          <Sequence key={s.id} from={s.start} durationInFrames={s.frames} name={s.id}>
            {Scene ? <Scene beats={s.beats as Beat[]} frames={s.frames} /> : null}
            {s.part > 0 && <PartTag part={s.part} frames={s.frames} />}
          </Sequence>
        );
      })}
      {allBeats.map(({ beat, start: at }) => (
        <Sequence key={beat.id} from={at} durationInFrames={beat.frames + 15} name={`voice ${beat.id}`}>
          <Audio src={staticFile(beat.file)} volume={1} />
        </Sequence>
      ))}
      {music.file && (
        <Audio src={staticFile(music.file)}
          volume={(f) => {
            // under the voice the music sits lower; between lines it comes back up (eased over ~0.4 s)
            let gap = Infinity;
            for (const { beat, start: s } of allBeats) {
              const d = f < s ? s - f : f > s + beat.frames ? f - (s + beat.frames) : 0;
              gap = Math.min(gap, d);
            }
            const duck = 0.45 + 0.55 * Math.min(1, gap / 12);
            const edge = interpolate(f, [0, 30, start - 60, start], [0, 1, 1, 0], { extrapolateLeft: "clamp", extrapolateRight: "clamp" });
            return music.volume * duck * edge;
          }} />
      )}
      <Captions beats={allBeats} />
      {placed.slice(1).map((s, i) => (
        <Sequence key={`wipe-${s.id}`} from={s.start - WIPE / 2} durationInFrames={WIPE + 16} name={`wipe ${s.id}`}>
          <BlockWipe seed={i} />
          <Sfx at={2} name="whoosh" volume={0.3} />
        </Sequence>
      ))}
    </AbsoluteFill>
  );
};

export const TOTAL_FRAMES = timeline.scenes.reduce((n, s) => n + s.frames, 0);
