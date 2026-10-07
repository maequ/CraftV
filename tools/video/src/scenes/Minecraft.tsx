import React from "react";
import { AbsoluteFill, useCurrentFrame } from "remotion";
import { Backdrop, Cursor, Explorer, FileItem, FlyingFile, Glass, Label, Sfx, Shot, Typed, TypingSfx, Window, pop, prog, sec } from "../kit";
import { C, F } from "../theme";
import { SceneProps, at, phase } from "../types";

const rowY = (top: number, i: number) => top + 44 + 52 + 10 + 68 * i + 32;

/** A small stylised GTA picture: the dusk skyline in a window. */
const GtaPicture: React.FC<{ w: number; h: number; children?: React.ReactNode }> = ({ w, h, children }) => (
  <div style={{ position: "relative", width: w, height: h, overflow: "hidden", background: "linear-gradient(180deg,#2b2350,#8a3a6a 70%,#ff9a3c)" }}>
    <svg width={w} height={h} style={{ position: "absolute", inset: 0 }}>
      {Array.from({ length: 24 }, (_, i) => {
        const bw = 30 + ((i * 37) % 50), bh = 60 + ((i * 73) % 170);
        return <rect key={i} x={i * (w / 22) - 10} y={h - bh} width={bw} height={bh} fill="#100f20" />;
      })}
    </svg>
    {children}
  </div>
);

export const Fabric: React.FC<SceneProps> = ({ beats, frames }) => {
  const f = useCurrentFrame();
  const p0 = phase(f, beats, 0, 1, frames), p1 = phase(f, beats, 1, null, frames);
  const link = prog(f, at(beats, 0, 0.35), at(beats, 0, 0.55));
  const W = { x: 460, y: 200 };
  const openAt = at(beats, 1, 0.15), pickAt = at(beats, 1, 0.38), installAt = at(beats, 1, 0.6), doneAt = installAt + sec(1.1);
  const open = f >= openAt && f < pickAt + 4;
  const bar = prog(f, installAt + 4, doneAt);
  const done = pop(f, doneAt, 12);
  return (
    <AbsoluteFill>
      <Backdrop dim={0.25} />
      <AbsoluteFill style={{ opacity: p0, alignItems: "center", justifyContent: "center", flexDirection: "row", gap: 60 }}>
        <div style={{ transform: `translateX(${(1 - pop(f, 4, 14)) * -200}px)` }}>
          <Window title="Grand Theft Auto V" w={720} h={460} accent={C.orange}>
            <GtaPicture w={720} h={416} />
          </Window>
        </div>
        <div style={{ display: "flex", flexDirection: "column", alignItems: "center", gap: 14, opacity: link }}>
          <div style={{ display: "flex", gap: 10 }}>
            {[0, 1, 2, 3, 4].map((i) => <div key={i} style={{ width: 22, height: 22, borderRadius: 4, background: C.grass, opacity: 0.3 + 0.7 * ((Math.floor(f / 4) + i) % 5 === 0 ? 1 : 0.2) }} />)}
          </div>
          <div style={{ fontFamily: F.pixel, fontSize: 20, color: C.grass }}>LINKED</div>
        </div>
        <div style={{ transform: `translateX(${(1 - pop(f, 10, 14)) * 200}px)` }}>
          <Window title="Minecraft* 26.3" w={720} h={460} accent={C.grass}>
            <div style={{ width: 720, height: 416, background: "linear-gradient(180deg,#78a7ff,#bcd6ff)" }}>
              <Shot name="hero_walk" w={720} h={416} at={10} fit={0.95} />
            </div>
          </Window>
        </div>
      </AbsoluteFill>
      <div style={{ position: "absolute", left: 0, right: 0, top: 830, textAlign: "center", opacity: p0 * prog(f, at(beats, 0, 0.6), at(beats, 0, 0.75)) }}>
        <span style={{ fontFamily: F.ui, fontWeight: 600, fontSize: 34, color: C.text }}>Same PC · your normal Minecraft account</span>
      </div>

      <div style={{ position: "absolute", left: W.x, top: W.y, opacity: p1, transform: `translateY(${(1 - pop(f, at(beats, 1) - 6, 15)) * 60}px)` }}>
        <Window title="Fabric Installer" w={1000} h={620} accent="#c8a86a">
          <div style={{ padding: "40px 56px", fontFamily: F.ui, color: C.text }}>
            <div style={{ display: "flex", gap: 14, marginBottom: 40 }}>
              {["Client", "Server"].map((t, i) => <div key={t} style={{ padding: "10px 26px", borderRadius: 8, background: i === 0 ? "#2f3650" : "transparent", fontSize: 26 }}>{t}</div>)}
            </div>
            {[["Minecraft Version", f >= pickAt ? "26.3" : "26.4-snapshot-2"], ["Loader Version", "0.19.5"]].map(([k, v], i) => (
              <div key={k} style={{ display: "flex", alignItems: "center", gap: 30, marginBottom: 28 }}>
                <div style={{ width: 300, fontSize: 28, color: C.muted }}>{k}</div>
                <div style={{ width: 420, padding: "14px 20px", borderRadius: 10, background: "#232838", border: `2px solid ${i === 0 && f >= pickAt && f < pickAt + 30 ? C.grass : "#3a4157"}`,
                  fontSize: 28, display: "flex", justifyContent: "space-between" }}>{v}<span style={{ color: C.muted }}>▾</span></div>
              </div>
            ))}
            {open && (
              <div style={{ position: "absolute", left: 386, top: 214, width: 420, background: "#1b2030", border: "2px solid #3a4157", borderRadius: 10, overflow: "hidden" }}>
                {["26.4-snapshot-2", "26.3", "26.2", "26.1"].map((v) => (
                  <div key={v} style={{ padding: "12px 20px", fontSize: 26, background: v === "26.3" ? "rgba(98,193,59,0.35)" : "transparent" }}>{v}</div>
                ))}
              </div>
            )}
            <div style={{ marginTop: 50, display: "flex", alignItems: "center", gap: 40 }}>
              <div style={{ padding: "18px 60px", borderRadius: 12, background: f >= installAt ? C.grass : C.blue, fontSize: 30, fontWeight: 800, color: f >= installAt ? C.night : "white" }}>Install</div>
              <div style={{ flex: 1, height: 16, borderRadius: 8, background: "#232838", overflow: "hidden", opacity: f >= installAt ? 1 : 0 }}>
                <div style={{ width: `${bar * 100}%`, height: "100%", background: C.grass }} />
              </div>
            </div>
          </div>
          <div style={{ position: "absolute", left: 230, top: 170, transform: `scale(${done})`, opacity: done }}>
            <Glass style={{ padding: "34px 44px", background: C.panelSolid, borderColor: C.grass }}>
              <div style={{ fontFamily: F.ui, fontWeight: 800, fontSize: 36, color: C.grass }}>✓ Successfully installed</div>
              <div style={{ fontFamily: F.ui, fontSize: 26, color: C.muted, marginTop: 10 }}>fabric-loader-26.3 is in your Minecraft Launcher</div>
            </Glass>
          </div>
        </Window>
      </div>
      {p1 > 0.5 && (
        <Cursor keys={[{ f: at(beats, 1), x: 1600, y: 950 }, { f: openAt - 4, x: W.x + 640, y: W.y + 44 + 40 + 60 + 40 + 30 }, { f: pickAt - 4, x: W.x + 600, y: W.y + 44 + 214 + 52 + 26 },
          { f: installAt - 4, x: W.x + 160, y: W.y + 44 + 40 + 60 + 40 + 180 + 50 + 30 }, { f: installAt + 40, x: 1650, y: 980 }]} clicks={[openAt, pickAt, installAt]} />
      )}
      <Sfx at={openAt} name="click" volume={0.45} />
      <Sfx at={pickAt} name="click" volume={0.45} />
      <Sfx at={installAt} name="click" volume={0.45} />
      <Sfx at={doneAt} name="success" volume={0.4} />
    </AbsoluteFill>
  );
};

export const Launcher: React.FC<SceneProps> = ({ beats, frames }) => {
  const f = useCurrentFrame();
  const p01 = phase(f, beats, 0, 2, frames), p2 = phase(f, beats, 2, null, frames);
  const W = { x: 260, y: 170 };
  const editAt = at(beats, 0, 0.62), dirAt = at(beats, 1, 0.12), saveAt = at(beats, 1, 0.82);
  const editing = f >= editAt + 6;
  const path = "C:\\Users\\YOU\\AppData\\Roaming\\.minecraft-craftv";
  const E = { x: 980, y: 240 };
  const newFolderAt = at(beats, 2, 0.12), jar1 = at(beats, 2, 0.55), jar2 = at(beats, 2, 0.72);
  const mods: FileItem[] = [{ name: "New folder", kind: "folder", enter: newFolderAt, rename: { at: newFolderAt + 8, to: "mods" } }];
  const inside: FileItem[] = [{ name: "fabric-api-0.161.0+26.3.jar", kind: "jar", enter: jar1 + 20 }, { name: "craftv-0.1.0.jar", kind: "jar", enter: jar2 + 20 }];
  return (
    <AbsoluteFill>
      <Backdrop dim={0.25} />
      <div style={{ position: "absolute", left: W.x, top: W.y, opacity: p01, transform: `translateY(${(1 - pop(f, 2, 15)) * 60}px)` }}>
        <Window title="Minecraft Launcher" w={1250} h={720} accent={C.grass}>
          <div style={{ display: "flex", height: "100%", fontFamily: F.ui, color: C.text }}>
            <div style={{ width: 260, background: "#171b28", padding: "30px 0" }}>
              {["Play", "Installations", "Skins", "Patch Notes"].map((t) => (
                <div key={t} style={{ padding: "16px 34px", fontSize: 26, fontWeight: t === "Installations" ? 800 : 500, color: t === "Installations" ? C.text : C.muted,
                  borderLeft: `5px solid ${t === "Installations" ? C.grass : "transparent"}` }}>{t}</div>
              ))}
            </div>
            <div style={{ flex: 1, padding: "34px 44px", position: "relative" }}>
              {!editing ? (
                <>
                  <div style={{ fontSize: 34, fontWeight: 800, marginBottom: 26 }}>Installations</div>
                  {["Latest release · 26.3", "fabric-loader-26.3"].map((t, i) => (
                    <div key={t} style={{ display: "flex", alignItems: "center", gap: 24, padding: "22px 26px", borderRadius: 12, marginBottom: 14,
                      background: i === 1 ? "rgba(98,193,59,0.15)" : "#20263a", border: i === 1 ? `2px solid ${C.grass}` : "2px solid transparent" }}>
                      <div style={{ width: 56, height: 56, borderRadius: 10, background: i === 1 ? "#c8a86a" : C.grass }} />
                      <div style={{ fontSize: 28, fontWeight: 700, flex: 1 }}>{t}</div>
                      {i === 1 && <div style={{ padding: "10px 30px", borderRadius: 8, background: C.grass, color: C.night, fontWeight: 800, fontSize: 24 }}>Play</div>}
                      {i === 1 && <div style={{ fontSize: 34, color: C.muted }}>•••</div>}
                    </div>
                  ))}
                  {f >= editAt - 6 && (
                    <div style={{ position: "absolute", left: 680, top: 250, background: "#1b2030", border: "2px solid #3a4157", borderRadius: 10, padding: "8px 0", width: 220 }}>
                      {["Edit", "Duplicate", "Delete"].map((t) => <div key={t} style={{ padding: "10px 22px", fontSize: 24, background: t === "Edit" ? "rgba(98,193,59,0.3)" : "transparent" }}>{t}</div>)}
                    </div>
                  )}
                </>
              ) : (
                <>
                  <div style={{ fontSize: 34, fontWeight: 800, marginBottom: 30 }}>Edit installation</div>
                  {[["Name", "fabric-loader-26.3"], ["Version", "release 26.3 (fabric)"]].map(([k, v]) => (
                    <div key={k} style={{ marginBottom: 24 }}>
                      <div style={{ fontSize: 22, color: C.muted, marginBottom: 8 }}>{k.toUpperCase()}</div>
                      <div style={{ padding: "14px 20px", borderRadius: 10, background: "#232838", fontSize: 26 }}>{v}</div>
                    </div>
                  ))}
                  <div style={{ fontSize: 22, color: C.grass, marginBottom: 8, fontWeight: 700 }}>GAME DIRECTORY</div>
                  <div style={{ padding: "14px 20px", borderRadius: 10, background: "#232838", border: `2px solid ${C.grass}`, fontSize: 26, fontFamily: F.mono, minHeight: 64 }}>
                    <Typed text={path} at={dirAt} cps={34} />
                  </div>
                  <div style={{ marginTop: 34, display: "inline-block", padding: "14px 46px", borderRadius: 10, background: f >= saveAt ? C.grass : C.blue,
                    color: f >= saveAt ? C.night : "white", fontWeight: 800, fontSize: 26 }}>{f >= saveAt ? "✓ Saved" : "Save"}</div>
                </>
              )}
            </div>
          </div>
        </Window>
      </div>
      <div style={{ position: "absolute", left: W.x + 1280, top: W.y + 330, width: 330, opacity: p01 * prog(f, at(beats, 1, 0.5), at(beats, 1, 0.65)) }}>
        <Glass style={{ padding: 24 }}>
          <Label>Why</Label>
          <div style={{ fontFamily: F.ui, fontWeight: 600, fontSize: 26, color: C.text, marginTop: 10 }}>Your normal Minecraft stays exactly how it is</div>
        </Glass>
      </div>
      {p01 > 0.5 && (
        <Cursor keys={[{ f: 0, x: 1650, y: 980 }, { f: at(beats, 0, 0.5) - 4, x: W.x + 260 + 44 + 860, y: W.y + 44 + 34 + 60 + 100 + 46 },
          { f: editAt - 4, x: W.x + 260 + 680 + 90, y: W.y + 44 + 250 + 28 }, { f: dirAt - 6, x: W.x + 260 + 44 + 400, y: W.y + 44 + 34 + 64 + 2 * 106 + 60 },
          { f: saveAt - 4, x: W.x + 260 + 44 + 100, y: W.y + 44 + 34 + 64 + 2 * 106 + 30 + 64 + 34 + 30 }]} clicks={[at(beats, 0, 0.5), editAt, dirAt - 2, saveAt]} />
      )}
      <Sfx at={at(beats, 0, 0.5)} name="click" volume={0.45} />
      <Sfx at={editAt} name="click" volume={0.45} />
      <TypingSfx text={path} at={dirAt} cps={34} />
      <Sfx at={saveAt} name="success" volume={0.35} />

      <AbsoluteFill style={{ opacity: p2 }}>
        <div style={{ position: "absolute", left: 160, top: 240 }}>
          <Explorer title=".minecraft-craftv" path="AppData › Roaming › .minecraft-craftv" files={mods} w={700} h={330} />
        </div>
        <div style={{ position: "absolute", left: E.x, top: E.y, opacity: prog(f, at(beats, 2, 0.35), at(beats, 2, 0.45)) }}>
          <Explorer title="mods" path=".minecraft-craftv › mods" files={inside} w={800} h={330} />
        </div>
        <div style={{ position: "absolute", left: 160, top: 640, display: "flex", gap: 20, opacity: prog(f, at(beats, 2, 0.4), at(beats, 2, 0.5)) }}>
          {["fabric-api-0.161.0+26.3.jar", "craftv-0.1.0.jar"].map((n, i) => (
            <Glass key={n} style={{ padding: "14px 22px", fontFamily: F.ui, fontSize: 24, color: C.text, opacity: f > [jar1, jar2][i] ? 0.35 : 1 }}>⬇ {n}</Glass>
          ))}
        </div>
        {["fabric-api-0.161.0+26.3.jar", "craftv-0.1.0.jar"].map((n, i) => (
          <React.Fragment key={n}>
            <FlyingFile start={[jar1, jar2][i]} dur={20} from={[180 + i * 420, 640]} to={[E.x + 60, rowY(E.y, i) - 24]} kind="jar" name={n} />
            <Sfx at={[jar1, jar2][i] + 20} name="drop" volume={0.4} />
          </React.Fragment>
        ))}
        <TypingSfx text="mods" at={newFolderAt + 8 + sec(0.5)} cps={15} />
      </AbsoluteFill>
    </AbsoluteFill>
  );
};
