// Renders chosen frames of the video to build/tutorial/check/ (one bundle, one browser): node stills.mjs 101 306 ...
import { bundle } from "@remotion/bundler";
import { renderStill, selectComposition } from "@remotion/renderer";
import path from "node:path";
import fs from "node:fs";

const frames = process.argv.slice(2).map(Number);
const out = path.resolve("../../build/tutorial/check");
fs.mkdirSync(out, { recursive: true });
const serveUrl = await bundle({ entryPoint: path.resolve("src/index.ts") });
const composition = await selectComposition({ serveUrl, id: "CraftVTutorial" });
for (const frame of frames) {
  await renderStill({ serveUrl, composition, frame, output: path.join(out, `f${String(frame).padStart(5, "0")}.png`), imageFormat: "png" });
  console.log("frame", frame);
}
