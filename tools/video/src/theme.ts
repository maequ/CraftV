import { loadFont as anton } from "@remotion/google-fonts/Anton";
import { loadFont as inter } from "@remotion/google-fonts/Inter";
import { loadFont as mono } from "@remotion/google-fonts/JetBrainsMono";
import { loadFont as pixel } from "@remotion/google-fonts/Silkscreen";

// Los Santos at dusk, built from blocks: a deep dusk sky, Minecraft grass for "do this", sunset orange for emphasis.
export const C = {
  night: "#0b0d17",
  dusk1: "#151a3a",
  dusk2: "#3a1d4f",
  dusk3: "#c2416b",
  sun: "#ff9a3c",
  panel: "rgba(16,19,30,0.86)",
  panelSolid: "#141826",
  line: "rgba(255,255,255,0.10)",
  text: "#f4f5f8",
  muted: "#a7adbd",
  grass: "#62c13b",
  grassDark: "#3f8f24",
  dirt: "#8b5a2b",
  orange: "#ff8a3d",
  red: "#ff4d4d",
  blue: "#3b82f6",
  win: "#1c2030",
  winBar: "#232838",
};

export const F = {
  display: anton("normal", { weights: ["400"], subsets: ["latin"] }).fontFamily,
  ui: inter("normal", { weights: ["400", "500", "600", "700", "800"], subsets: ["latin"] }).fontFamily,
  mono: mono("normal", { weights: ["400", "500", "700"], subsets: ["latin"] }).fontFamily,
  pixel: pixel("normal", { weights: ["400", "700"], subsets: ["latin"] }).fontFamily,
};

export const FPS = 30;
