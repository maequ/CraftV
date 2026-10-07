import React from "react";
import { AbsoluteFill, useCurrentFrame } from "remotion";
import { Backdrop, Cursor, Explorer, FileItem, FlyingFile, Glass, Label, Sfx, TypingSfx, Window, pop, prog, sec } from "../kit";
import { C, F } from "../theme";
import { SceneProps, at, phase } from "../types";

// Explorer geometry: rows start 44 (title) + 52 (path) + 10 below the window top, 60 apart.
const rowY = (top: number, i: number) => top + 44 + 52 + 10 + 68 * i + 32;

const GTA_FILES: FileItem[] = [
  { name: "GTA5.exe", kind: "exe" },
  { name: "PlayGTAV.exe", kind: "exe" },
  { name: "x64", kind: "folder" },
];

export const ScriptHook: React.FC<SceneProps> = ({ beats }) => {
  const f = useCurrentFrame();
  const L = { x: 90, y: 190 }, R = { x: 1010, y: 190 };
  const g2 = at(beats, 1), g3 = at(beats, 2);
  const d1 = at(beats, 1, 0.3), d2 = at(beats, 1, 0.58), d3 = at(beats, 2, 0.4), d4 = at(beats, 2, 0.62);
  const fly = 22;
  const crafty = f >= g3 - 4;
  const left: FileItem[] = crafty
    ? [{ name: "CraftV.asi", kind: "asi", selected: f >= d3 - 14 && f < d3 }, { name: "CraftV.ini", kind: "ini", selected: f >= d4 - 14 && f < d4 },
      { name: "Minecraft mod", kind: "folder", dim: true }, { name: "Minecraft view (ReShade)", kind: "folder", dim: true }]
    : [{ name: "ScriptHookV.dll", kind: "dll", selected: f >= d1 - 14 && f < d1 }, { name: "dinput8.dll", kind: "dll", selected: f >= d2 - 14 && f < d2 },
      { name: "NativeTrainer.asi", kind: "asi", dim: true }];
  const right: FileItem[] = [...GTA_FILES, { name: "ScriptHookV.dll", kind: "dll", enter: d1 + fly }, { name: "dinput8.dll", kind: "dll", enter: d2 + fly },
    { name: "CraftV.asi", kind: "asi", enter: d3 + fly }, { name: "CraftV.ini", kind: "ini", enter: d4 + fly }];
  const winIn = pop(f, 2, 15), win2 = pop(f, 8, 15);
  const swap = prog(f, g3 - 8, g3 + 6);
  return (
    <AbsoluteFill>
      <Backdrop dim={0.25} />
      <div style={{ position: "absolute", left: L.x, top: L.y, transform: `translateY(${(1 - winIn) * 80}px) rotateY(${swap < 1 ? swap * 0 : 0}deg)`, opacity: winIn }}>
        <div style={{ opacity: 1 - Math.sin(Math.PI * swap) * 0.85 }}>
          <Explorer title={crafty ? "CraftV-0.1.0 (zip)" : "ScriptHookV (zip)"} path={crafty ? "Downloads › CraftV-0.1.0" : "Downloads › ScriptHookV › bin"} files={left} w={820} h={620} />
        </div>
      </div>
      <div style={{ position: "absolute", left: R.x, top: R.y, transform: `translateY(${(1 - win2) * 80}px)`, opacity: win2 }}>
        <Explorer title="Grand Theft Auto V" path="C: › Games › Grand Theft Auto V" files={right} w={820} h={620} />
      </div>
      <div style={{ position: "absolute", left: R.x + 20, top: R.y - 70, opacity: prog(f, at(beats, 1, 0.75), at(beats, 1, 0.85)) }}>
        <Label>The folder with GTA5.exe</Label>
      </div>
      {[[d1, 0, "ScriptHookV.dll", "dll", 3], [d2, 1, "dinput8.dll", "dll", 4], [d3, 0, "CraftV.asi", "asi", 5], [d4, 1, "CraftV.ini", "ini", 6]].map(
        ([d, row, name, kind, slot]) => (
          <React.Fragment key={name as string}>
            <FlyingFile start={d as number} dur={fly} from={[L.x + 60, rowY(L.y, row as number) - 24]} to={[R.x + 60, rowY(R.y, slot as number) - 24]} kind={kind as string} name={name as string} />
            <Sfx at={(d as number) - 12} name="click" volume={0.4} />
            <Sfx at={(d as number) + fly} name="drop" volume={0.45} />
          </React.Fragment>
        ))}
      <Cursor
        keys={[
          { f: g2 - 10, x: 1500, y: 950 },
          { f: d1 - 14, x: L.x + 120, y: rowY(L.y, 0) },
          { f: d1, x: L.x + 120, y: rowY(L.y, 0) },
          { f: d1 + fly, x: R.x + 140, y: rowY(R.y, 3) },
          { f: d2 - 14, x: L.x + 120, y: rowY(L.y, 1) },
          { f: d2, x: L.x + 120, y: rowY(L.y, 1) },
          { f: d2 + fly, x: R.x + 140, y: rowY(R.y, 4) },
          { f: d3 - 14, x: L.x + 120, y: rowY(L.y, 0) },
          { f: d3, x: L.x + 120, y: rowY(L.y, 0) },
          { f: d3 + fly, x: R.x + 140, y: rowY(R.y, 5) },
          { f: d4 - 14, x: L.x + 120, y: rowY(L.y, 1) },
          { f: d4, x: L.x + 120, y: rowY(L.y, 1) },
          { f: d4 + fly, x: R.x + 140, y: rowY(R.y, 6) },
          { f: d4 + fly + 30, x: 1700, y: 1000 },
        ]}
        clicks={[d1 - 12, d2 - 12, d3 - 12, d4 - 12]}
      />
    </AbsoluteFill>
  );
};

const Browser: React.FC<{ f: number; clickAt: number; badAt: number }> = ({ f, clickAt, badAt }) => {
  const hit = f >= clickAt;
  const bad = prog(f, badAt, badAt + 10);
  return (
    <Window title="reshade.me" w={1100} h={640} accent="#e45bb0">
      <div style={{ padding: "60px 70px", fontFamily: F.ui }}>
        <div style={{ fontWeight: 800, fontSize: 64, color: C.text }}>ReShade</div>
        <div style={{ fontSize: 26, color: C.muted, marginTop: 10 }}>The latest version</div>
        <div style={{ display: "flex", flexDirection: "column", gap: 26, marginTop: 50 }}>
          <div style={{ position: "relative", width: 520, padding: "22px 30px", borderRadius: 12, background: "#2c3246", fontSize: 30, fontWeight: 700, color: C.text,
            opacity: 1 - bad * 0.5 }}>
            Download
            <div style={{ position: "absolute", left: 560, top: 16, fontFamily: F.pixel, fontSize: 22, color: C.red, opacity: bad, whiteSpace: "nowrap",
              transform: `translateX(${(1 - bad) * 20}px)` }}>✕ won't work with CraftV</div>
          </div>
          <div style={{ width: 640, padding: "22px 30px", borderRadius: 12, background: hit ? C.grass : "#2c3246", fontSize: 30, fontWeight: 700,
            color: hit ? C.night : C.text, boxShadow: hit ? "0 0 50px rgba(98,193,59,0.55)" : "none", transform: `scale(${hit ? 1 + 0.04 * Math.max(0, 1 - (f - clickAt) / 10) : 1})` }}>
            Download with full add-on support
          </div>
        </div>
      </div>
    </Window>
  );
};

const Installer: React.FC<{ f: number; beats: SceneProps["beats"] }> = ({ f, beats }) => {
  const r3 = at(beats, 2), r4 = at(beats, 3);
  const pickGame = at(beats, 2, 0.22), pickApi = at(beats, 2, 0.62);
  const page = f < pickGame + 14 ? 0 : f < r4 ? 1 : 2;
  const packs = ["Standard effects", "SweetFX by CeeJay.dk", "qUINT by Marty McFly", "Depth3D by BlueSkyDefender", "AstrayFX by BlueSkyDefender"];
  const untick = (i: number) => f >= at(beats, 3, 0.1 + i * 0.1);
  return (
    <Window title="ReShade Setup" w={1100} h={640} accent="#e45bb0">
      <div style={{ padding: "40px 56px", fontFamily: F.ui, color: C.text }}>
        <div style={{ fontSize: 34, fontWeight: 700, marginBottom: 26 }}>{["Select a game", "Select the rendering API", "Select effect packages"][page]}</div>
        {page === 0 && (
          <div style={{ display: "flex", flexDirection: "column", gap: 10 }}>
            {["Cyberpunk 2077", "GTA5.exe   Grand Theft Auto V", "Skyrim Special Edition"].map((g, i) => (
              <div key={g} style={{ padding: "16px 22px", borderRadius: 10, fontSize: 28, background: i === 1 && f >= pickGame ? "rgba(59,130,246,0.4)" : "#232838",
                outline: i === 1 && f >= pickGame ? "2px solid #3b82f6" : "none" }}>{g}</div>
            ))}
          </div>
        )}
        {page === 1 && (
          <div style={{ display: "flex", flexDirection: "column", gap: 18 }}>
            {["Direct3D 9", "Direct3D 10/11/12", "OpenGL", "Vulkan"].map((a, i) => {
              const on = i === 1 && f >= pickApi;
              return (
                <div key={a} style={{ display: "flex", alignItems: "center", gap: 20, fontSize: 30 }}>
                  <div style={{ width: 32, height: 32, borderRadius: "50%", border: `3px solid ${on ? C.grass : C.muted}`, display: "flex", alignItems: "center", justifyContent: "center" }}>
                    {on && <div style={{ width: 16, height: 16, borderRadius: "50%", background: C.grass }} />}
                  </div>
                  <span style={{ color: on ? C.text : C.muted, fontWeight: on ? 700 : 400 }}>{a}</span>
                </div>
              );
            })}
          </div>
        )}
        {page === 2 && (
          <div style={{ display: "flex", flexDirection: "column", gap: 16 }}>
            {packs.map((p, i) => (
              <div key={p} style={{ display: "flex", alignItems: "center", gap: 20, fontSize: 28, color: untick(i) ? C.muted : C.text }}>
                <div style={{ width: 32, height: 32, borderRadius: 6, border: `3px solid ${untick(i) ? C.muted : C.blue}`, background: untick(i) ? "transparent" : C.blue,
                  display: "flex", alignItems: "center", justifyContent: "center", fontWeight: 800, fontSize: 22, color: "white" }}>{untick(i) ? "" : "✓"}</div>
                <span style={{ textDecoration: untick(i) ? "line-through" : "none" }}>{p}</span>
              </div>
            ))}
          </div>
        )}
      </div>
    </Window>
  );
};

export const ReShade: React.FC<SceneProps> = ({ beats, frames }) => {
  const f = useCurrentFrame();
  const p0 = phase(f, beats, 0, 1, frames), p1 = phase(f, beats, 1, 2, frames), p2 = phase(f, beats, 2, null, frames);
  const clickAt = at(beats, 1, 0.45), badAt = at(beats, 1, 0.72);
  const boxes = ["Minecraft's picture", "ReShade", "GTA's picture"];
  const W = { x: 410, y: 210 };
  return (
    <AbsoluteFill>
      <Backdrop dim={0.25} />
      <AbsoluteFill style={{ opacity: p0, alignItems: "center", justifyContent: "center", gap: 50 }}>
        <Label>What ReShade does</Label>
        <div style={{ display: "flex", alignItems: "center", gap: 30 }}>
          {boxes.map((b, i) => {
            const p = pop(f, at(beats, 0, 0.15 + i * 0.22), 13);
            return (
              <React.Fragment key={b}>
                {i > 0 && <div style={{ fontFamily: F.display, fontSize: 80, color: C.grass, opacity: prog(f, at(beats, 0, 0.1 + i * 0.22), at(beats, 0, 0.2 + i * 0.22)) }}>→</div>}
                <Glass style={{ width: 400, height: 240, display: "flex", alignItems: "center", justifyContent: "center", transform: `scale(${p})`,
                  borderColor: i === 1 ? "#e45bb0" : C.line, background: i === 1 ? "rgba(228,91,176,0.18)" : C.panel }}>
                  <div style={{ fontFamily: F.display, fontSize: 60, color: C.text, textTransform: "uppercase", textAlign: "center", padding: 20 }}>{b}</div>
                </Glass>
              </React.Fragment>
            );
          })}
        </div>
        {[0.15, 0.37, 0.59].map((k) => <Sfx key={k} at={at(beats, 0, k)} name="pop" volume={0.3} />)}
      </AbsoluteFill>
      <div style={{ position: "absolute", left: W.x, top: W.y, opacity: p1, transform: `translateY(${(1 - pop(f, at(beats, 1) - 6, 15)) * 60}px)` }}>
        <Browser f={f} clickAt={clickAt} badAt={badAt} />
      </div>
      <div style={{ position: "absolute", left: W.x, top: W.y, opacity: p2, transform: `translateY(${(1 - pop(f, at(beats, 2) - 6, 15)) * 60}px)` }}>
        <Installer f={f} beats={beats} />
      </div>
      {p1 > 0.5 && <Cursor keys={[{ f: at(beats, 1, 0.1), x: 1500, y: 900 }, { f: clickAt - 4, x: W.x + 420, y: W.y + 44 + 60 + 64 + 26 + 50 + 86 + 40 }]} clicks={[clickAt]} />}
      {p2 > 0.5 && (
        <Cursor
          keys={[
            { f: at(beats, 2), x: 1500, y: 900 },
            { f: at(beats, 2, 0.22) - 4, x: W.x + 300, y: W.y + 44 + 40 + 60 + 72 + 32 },
            { f: at(beats, 2, 0.62) - 4, x: W.x + 160, y: W.y + 44 + 40 + 60 + 50 + 18 },
            ...[0, 1, 2, 3, 4].map((i) => ({ f: at(beats, 3, 0.1 + i * 0.1) - 3, x: W.x + 72, y: W.y + 44 + 40 + 60 + 16 + i * 48 })),
          ]}
          clicks={[at(beats, 2, 0.22), at(beats, 2, 0.62), ...[0, 1, 2, 3, 4].map((i) => at(beats, 3, 0.1 + i * 0.1))]}
        />
      )}
      <Sfx at={clickAt} name="click" volume={0.45} />
      <Sfx at={clickAt + 2} name="success" volume={0.3} />
      <Sfx at={badAt} name="tick" volume={0.35} />
      <Sfx at={at(beats, 2, 0.22)} name="click" volume={0.45} />
      <Sfx at={at(beats, 2, 0.62)} name="click" volume={0.45} />
      {[0, 1, 2, 3, 4].map((i) => <Sfx key={i} at={at(beats, 3, 0.1 + i * 0.1)} name="tick" volume={0.35} />)}
      <div style={{ position: "absolute", left: W.x + 1140, top: W.y + 200, width: 330, opacity: prog(f, at(beats, 3, 0.65), at(beats, 3, 0.8)) }}>
        <Glass style={{ padding: 26 }}>
          <Label>Good</Label>
          <div style={{ fontFamily: F.ui, fontWeight: 600, fontSize: 30, color: C.text, marginTop: 10 }}>CraftV brings its own effect</div>
        </Glass>
      </div>
    </AbsoluteFill>
  );
};

export const Rename: React.FC<SceneProps> = ({ beats, frames }) => {
  const f = useCurrentFrame();
  const W = { x: 560, y: 200 }, V = { x: 70, y: 260 };
  const renameAt = at(beats, 1, 0.08);
  const r3 = at(beats, 2);
  const moveAside = prog(f, r3 - 6, r3 + 12);
  const flyAt = [at(beats, 2, 0.45), at(beats, 2, 0.58), at(beats, 2, 0.71)];
  const files: FileItem[] = [
    { name: "GTA5.exe", kind: "exe" },
    { name: "ScriptHookV.dll", kind: "dll" },
    { name: "CraftV.asi", kind: "asi" },
    { name: "dxgi.dll", kind: "dll", selected: f >= at(beats, 0, 0.55) && f < renameAt, rename: { at: renameAt, to: "ReShade64.asi", kind: "asi" } },
    { name: "ReShade.ini", kind: "ini", enter: flyAt[0] + 20 },
    { name: "ReShadePreset.ini", kind: "ini", enter: flyAt[1] + 20 },
    { name: "reshade-shaders", kind: "folder", enter: flyAt[2] + 20 },
  ];
  const callout = pop(f, at(beats, 0, 0.6), 12);
  const why = pop(f, at(beats, 1, 0.6), 13);
  const gx = W.x + moveAside * 380;
  return (
    <AbsoluteFill>
      <Backdrop dim={0.25} />
      <div style={{ position: "absolute", left: gx, top: W.y }}>
        <Explorer title="Grand Theft Auto V" path="C: › Games › Grand Theft Auto V" files={files} w={840} h={640} />
      </div>
      <div style={{ position: "absolute", left: gx - 330, top: rowY(W.y, 3) - 36, opacity: callout * (1 - moveAside), transform: `translateX(${(1 - callout) * -40}px)` }}>
        <Glass style={{ padding: "16px 24px", display: "flex", alignItems: "center", gap: 14, borderColor: C.orange }}>
          <div style={{ fontFamily: F.ui, fontWeight: 700, fontSize: 30, color: C.orange }}>this one</div>
          <div style={{ fontFamily: F.display, fontSize: 50, color: C.orange }}>→</div>
        </Glass>
      </div>
      <div style={{ position: "absolute", left: W.x + 860, top: W.y + 180, opacity: why * (1 - moveAside), transform: `scale(${0.9 + why * 0.1})` }}>
        <Glass style={{ padding: "26px 30px", width: 440 }}>
          <Label>Why</Label>
          <div style={{ fontFamily: F.ui, fontWeight: 600, fontSize: 30, color: C.text, marginTop: 12, lineHeight: 1.3 }}>Script Hook V loads every <span style={{ color: C.grass }}>.asi</span> file next to GTA5.exe</div>
        </Glass>
      </div>
      <div style={{ position: "absolute", left: V.x, top: V.y, opacity: moveAside, transform: `translateX(${(1 - moveAside) * -120}px)` }}>
        <Explorer title="CraftV-0.1.0 (zip)" path="CraftV-0.1.0 › Minecraft view (ReShade)" w={760} h={420}
          files={[{ name: "ReShade.ini", kind: "ini", dim: f > flyAt[0] }, { name: "ReShadePreset.ini", kind: "ini", dim: f > flyAt[1] }, { name: "reshade-shaders", kind: "folder", dim: f > flyAt[2] }]} />
      </div>
      {["ReShade.ini", "ReShadePreset.ini", "reshade-shaders"].map((n, i) => (
        <React.Fragment key={n}>
          <FlyingFile start={flyAt[i]} dur={20} from={[V.x + 60, rowY(V.y, i) - 24]} to={[W.x + 380 + 60, rowY(W.y, 4 + i) - 24]} kind={i === 2 ? "folder" : "ini"} name={n} />
          <Sfx at={flyAt[i] + 20} name="drop" volume={0.4} />
        </React.Fragment>
      ))}
      <Sfx at={at(beats, 0, 0.55)} name="click" volume={0.4} />
      <TypingSfx text="ReShade64.asi" at={renameAt + sec(0.5)} cps={15} />
      <Sfx at={renameAt + sec(0.5) + 26 + sec(0.3)} name="success" volume={0.35} />
      <Cursor keys={[{ f: 0, x: 1600, y: 950 }, { f: at(beats, 0, 0.5), x: W.x + 200, y: rowY(W.y, 3) }, { f: renameAt + 40, x: W.x + 200, y: rowY(W.y, 3) }, { f: renameAt + 70, x: 1650, y: 980 }]}
        clicks={[at(beats, 0, 0.55)]} />
    </AbsoluteFill>
  );
};
