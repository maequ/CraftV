export type Beat = { id: string; file: string; caption: string; from: number; frames: number };
export type SceneProps = { beats: Beat[]; frames: number };

/** The frame `frac` of the way through beat `i` (relative to the scene). */
export const at = (beats: Beat[], i: number, frac = 0) => beats[i].from + Math.round(beats[i].frames * frac);
/** 0..1 visibility of a phase that runs from beat `i` until beat `j` starts (or the scene ends), with short fades. */
export const phase = (f: number, beats: Beat[], i: number, j: number | null, end: number, fade = 8) => {
  const a = i === 0 ? -fade : beats[i].from;
  const b = j === null ? end + fade : beats[j].from;
  if (f < a - fade || f > b + fade) return 0;
  return Math.min(1, (f - (a - fade)) / fade, (b + fade - f) / fade);
};
