// Reads script.mjs with the Kokoro voice (offline, Apache-2.0) into public/voice/*.wav and writes src/timeline.json:
// every beat's frames, so the animation lines up with what's being said.  npm run voice   (CRAFTV_VOICE=am_puck to change)
import { KokoroTTS } from "kokoro-js";
import fs from "node:fs";
import { SCENES, VOICE } from "./script.mjs";

const FPS = 30;
const GAP = 0.35;  // seconds between beats
const TAIL = 0.6;  // seconds after a scene's last beat
fs.mkdirSync("public/voice", { recursive: true });
const tts = await KokoroTTS.from_pretrained("onnx-community/Kokoro-82M-v1.0-ONNX", { dtype: "fp32", device: "cpu" });

const scenes = [];
for (const scene of SCENES) {
  const beats = [];
  let at = 0;
  for (const beat of scene.beats) {
    const file = `voice/${beat.id}.wav`;
    const audio = await tts.generate(beat.say, { voice: VOICE, speed: 1.0 });
    audio.save(`public/${file}`);
    const seconds = audio.audio.length / audio.sampling_rate;
    const frames = Math.ceil(seconds * FPS);
    beats.push({ id: beat.id, file, caption: beat.caption || beat.say, from: at, frames });
    at += frames + Math.round(GAP * FPS);
    console.log(`${beat.id}: ${seconds.toFixed(1)} s`);
  }
  scenes.push({ id: scene.id, part: scene.part || 0, beats, frames: at - Math.round(GAP * FPS) + Math.round(TAIL * FPS) });
}
fs.writeFileSync("src/timeline.json", JSON.stringify({ fps: FPS, voice: VOICE, scenes }, null, 1));
const total = scenes.reduce((s, x) => s + x.frames, 0) / FPS;
console.log(`timeline: ${scenes.length} scenes, ${(total / 60).toFixed(1)} min`);
