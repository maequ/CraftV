import React from "react";
import { AbsoluteFill, useCurrentFrame } from "remotion";
import { Backdrop, Glass, Kbd, Label, Sfx, Shot, Window, pop, prog, sec } from "../kit";
import { C, F } from "../theme";
import { SceneProps, at, phase } from "../types";

/** A list menu in the style of GTA's pause-menu settings: white rows on a dark panel, the chosen row highlighted. */
const GameMenu: React.FC<{ title: string; rows: { label: string; value: string }[]; sel: number; w?: number; header?: React.ReactNode; tick?: number; compact?: boolean }> = ({ title, rows, sel, w = 900, header, tick = 0, compact = false }) => (
  <div style={{ width: w, fontFamily: F.ui }}>
    {header}
    <div style={{ background: "#000", padding: "12px 22px", display: "flex", justifyContent: "space-between", color: C.text, fontWeight: 700, fontSize: 26 }}>
      <span>{title}</span><span>{sel + 1} / {rows.length}</span>
    </div>
    {rows.map((r, i) => {
      const on = i === sel;
      return (
        <div key={r.label} style={{ display: "flex", justifyContent: "space-between", alignItems: "center", padding: compact ? "8px 22px" : "14px 22px", fontSize: compact ? 24 : 28,
          background: on ? "#f0f0f0" : "rgba(0,0,0,0.72)", color: on ? "#111" : C.text, transition: "none" }}>
          <span>{r.label}</span>
          <span style={{ fontWeight: on ? 700 : 400, transform: on ? `scale(${1 + tick * 0.08})` : "none" }}>{on ? `‹  ${r.value}  ›` : r.value}</span>
        </div>
      );
    })}
  </div>
);

export const Settings: React.FC<SceneProps> = ({ beats }) => {
  const f = useCurrentFrame();
  const t1 = [at(beats, 0, 0.55), at(beats, 0, 0.68), at(beats, 0, 0.81)];
  const screen = f < t1[0] ? "Fullscreen" : f < t1[1] ? "Windowed" : "Windowed Borderless";
  const move = [at(beats, 1, 0.1), at(beats, 1, 0.2), at(beats, 1, 0.3)];
  const sel = f < move[0] ? 1 : f < move[1] ? 4 : f < move[2] ? 6 : 8;
  const flip = at(beats, 1, 0.45);
  const lastTick = [...t1, flip].filter((t) => f >= t).pop();
  const tick = lastTick === undefined ? 0 : Math.max(0, 1 - (f - lastTick) / 6);
  const rows = [
    { label: "Ignore Suggested Limits", value: "Off" },
    { label: "Screen Type", value: screen },
    { label: "Resolution", value: "2560 x 1440" },
    { label: "Aspect Ratio", value: "Auto" },
    { label: "Refresh Rate", value: "144Hz" },
    { label: "Output Monitor", value: "1" },
    { label: "VSync", value: "Off" },
    { label: "DirectX Version", value: "DirectX 11" },
    { label: "Pause Game On Focus Loss", value: f >= flip ? "Off" : "On" },
  ];
  const p = pop(f, 2, 15);
  return (
    <AbsoluteFill>
      <Backdrop dim={0.35} />
      <div style={{ position: "absolute", left: 120, top: 90 }}>
        <div style={{ fontFamily: F.display, fontSize: 96, color: C.text, opacity: prog(f, 0, 12) }}>SETTINGS</div>
        <div style={{ display: "flex", gap: 30, fontFamily: F.ui, fontWeight: 700, fontSize: 26, color: C.muted, marginTop: 10, opacity: prog(f, 4, 16) }}>
          {["Display", "Graphics", "Advanced Graphics", "Camera"].map((t) => <span key={t} style={{ color: t === "Graphics" ? C.text : undefined, borderBottom: t === "Graphics" ? `4px solid ${C.grass}` : "none", paddingBottom: 6 }}>{t}</span>)}
        </div>
      </div>
      <div style={{ position: "absolute", left: 120, top: 300, transform: `translateY(${(1 - p) * 50}px)`, opacity: p }}>
        <GameMenu title="GRAPHICS" rows={rows} sel={sel} w={1000} tick={tick} compact />
      </div>
      <div style={{ position: "absolute", left: 1220, top: 380, width: 560, display: "flex", flexDirection: "column", gap: 26 }}>
        {[{ at: t1[2], text: "Screen Type: Windowed Borderless" }, { at: flip, text: "Pause Game On Focus Loss: Off" }].map((x) => {
          const q = pop(f, x.at, 13);
          return (
            <Glass key={x.text} style={{ padding: "22px 28px", display: "flex", gap: 18, alignItems: "center", transform: `translateX(${(1 - q) * 60}px)`, opacity: q, borderColor: C.grass }}>
              <div style={{ fontFamily: F.display, fontSize: 44, color: C.grass }}>✓</div>
              <div style={{ fontFamily: F.ui, fontWeight: 700, fontSize: 28, color: C.text }}>{x.text}</div>
            </Glass>
          );
        })}
      </div>
      {[...t1, flip].map((t) => <Sfx key={t} at={t} name="tick" volume={0.4} />)}
      {move.map((t) => <Sfx key={t} at={t} name="tick" volume={0.2} />)}
      <Sfx at={t1[2] + 2} name="success" volume={0.3} />
      <Sfx at={flip + 2} name="success" volume={0.3} />
    </AbsoluteFill>
  );
};

const OverlayBox: React.FC<{ on: number }> = ({ on }) => (
  <div style={{ background: "rgba(0,0,0,0.6)", padding: "10px 16px", borderRadius: 6, fontFamily: F.ui, fontSize: 18, lineHeight: 1.45, transform: `scale(${0.8 + on * 0.2})`, opacity: on }}>
    <div style={{ color: "#5aff5a", fontWeight: 700 }}>CraftV  link CONNECTED  ping 2 ms  MC view</div>
    <div style={{ color: "#e8e8e8" }}>friends 0  join: 192.168.1.23:25565</div>
    <div style={{ color: "#e8e8e8" }}>ground sent 412  surface PAVEMENT  solid 12</div>
  </div>
);

export const Play: React.FC<SceneProps> = ({ beats }) => {
  const f = useCurrentFrame();
  const load = prog(f, at(beats, 0, 0.35), at(beats, 0, 0.75));
  const world = f >= at(beats, 0, 0.75);
  const gta = pop(f, at(beats, 1, 0.3), 15);
  const box = pop(f, at(beats, 2, 0.35), 12);
  const steve = pop(f, at(beats, 2, 0.15), 14);
  return (
    <AbsoluteFill>
      <Backdrop dim={0.25} />
      <div style={{ position: "absolute", left: 120, top: 150, transform: `translateY(${(1 - pop(f, 2, 15)) * 60}px) scale(${1 - gta * 0.08})`, opacity: pop(f, 2, 15) * (1 - gta * 0.5), transformOrigin: "0 0" }}>
        <Window title="Minecraft* 26.3" w={980} h={600} accent={C.grass}>
          {!world ? (
            <div style={{ width: "100%", height: "100%", background: "#2b1f14", display: "flex", flexDirection: "column", alignItems: "center", justifyContent: "center", gap: 24 }}>
              <div style={{ fontFamily: F.pixel, fontSize: 34, color: C.text }}>Loading terrain</div>
              <div style={{ width: 520, height: 18, background: "#1a120b", border: "3px solid #fff" }}>
                <div style={{ width: `${load * 100}%`, height: "100%", background: C.grass }} />
              </div>
              <div style={{ fontFamily: F.ui, fontSize: 24, color: C.muted }}>World: CraftV</div>
            </div>
          ) : (
            <div style={{ width: "100%", height: "100%", background: "linear-gradient(180deg,#78a7ff,#bcd6ff)" }}>
              <Shot name="hero_orbit" w={980} h={556} at={at(beats, 0, 0.75)} fit={0.9} />
            </div>
          )}
        </Window>
      </div>
      <div style={{ position: "absolute", left: 120, top: 790, opacity: prog(f, at(beats, 0, 0.8), at(beats, 0, 0.95)) * (1 - gta) }}>
        <Label>It opens the CraftV world by itself</Label>
      </div>
      <div style={{ position: "absolute", left: 700, top: 120, transform: `translateX(${(1 - gta) * 900}px)`, opacity: gta }}>
        <Window title="Grand Theft Auto V" w={1120} h={680} accent={C.orange}>
          <div style={{ position: "relative", width: 1120, height: 636, overflow: "hidden", background: "linear-gradient(180deg,#2b2350,#8a3a6a 65%,#ff9a3c)" }}>
            <svg width={1120} height={636} style={{ position: "absolute", inset: 0 }}>
              {Array.from({ length: 22 }, (_, i) => {
                const bw = 40 + ((i * 37) % 60), bh = 90 + ((i * 73) % 220);
                return <rect key={i} x={i * 54 - 10} y={636 - bh - 90} width={bw} height={bh} fill="#141225" />;
              })}
              <rect x={0} y={546} width={1120} height={90} fill="#2a2a33" />
              {Array.from({ length: 12 }, (_, i) => <rect key={i} x={i * 100 + 20} y={588} width={50} height={6} fill="#d8d8d8" opacity={0.6} />)}
            </svg>
            {f < at(beats, 2) && (
              <div style={{ position: "absolute", right: 40, bottom: 40, display: "flex", alignItems: "center", gap: 14, fontFamily: F.ui, fontWeight: 700, fontSize: 24, color: "white" }}>
                <div style={{ width: 30, height: 30, borderRadius: "50%", border: "4px solid rgba(255,255,255,0.3)", borderTopColor: "white", transform: `rotate(${f * 18}deg)` }} />
                STORY MODE
              </div>
            )}
            <div style={{ position: "absolute", left: 0, top: 0, opacity: steve, transform: `scale(${1.04 - 0.04 * steve})` }}>
              <Shot name="hud_fp" w={1120} h={636} at={at(beats, 2, 0.15)} fit={1.0} full />
            </div>
            <div style={{ position: "absolute", right: 22, top: 22 }}><OverlayBox on={box} /></div>
          </div>
        </Window>
      </div>
      <div style={{ position: "absolute", left: 1240, top: 830, transform: `scale(${pop(f, at(beats, 2, 0.55), 10)})`, opacity: prog(f, at(beats, 2, 0.55), at(beats, 2, 0.6)) }}>
        <div style={{ fontFamily: F.display, fontSize: 90, color: C.grass, textShadow: "0 0 40px rgba(98,193,59,0.6)", transform: "rotate(-4deg)" }}>YOU'RE IN</div>
      </div>
      <Sfx at={at(beats, 0, 0.75)} name="pop" volume={0.3} />
      <Sfx at={at(beats, 1, 0.3)} name="whoosh" volume={0.3} />
      <Sfx at={at(beats, 2, 0.35)} name="pop" volume={0.35} />
      <Sfx at={at(beats, 2, 0.55)} name="success" volume={0.4} />
    </AbsoluteFill>
  );
};

const Mouse: React.FC<{ left: number; right: number }> = ({ left, right }) => (
  <svg width={260} height={380} viewBox="0 0 260 380">
    <defs>
      <clipPath id="body"><rect x={10} y={10} width={240} height={360} rx={120} /></clipPath>
    </defs>
    <g clipPath="url(#body)">
      <rect x={10} y={10} width={240} height={360} fill="#2a3044" />
      <rect x={10} y={10} width={119} height={160} fill={`rgba(98,193,59,${0.15 + left * 0.85})`} />
      <rect x={131} y={10} width={119} height={160} fill={`rgba(255,138,61,${0.15 + right * 0.85})`} />
      <rect x={118} y={40} width={24} height={60} rx={12} fill="#11141e" />
    </g>
    <rect x={10} y={10} width={240} height={360} rx={120} fill="none" stroke="#454d66" strokeWidth={6} />
    <line x1={130} y1={10} x2={130} y2={170} stroke="#454d66" strokeWidth={5} />
    <line x1={10} y1={170} x2={250} y2={170} stroke="#454d66" strokeWidth={5} />
  </svg>
);

export const Controls: React.FC<SceneProps> = ({ beats, frames }) => {
  const f = useCurrentFrame();
  const p0 = phase(f, beats, 0, 1, frames), p1 = phase(f, beats, 1, 2, frames), p2 = phase(f, beats, 2, null, frames);
  const keyLit = (start: number, len = sec(1.6)) => (f >= start && f < start + len ? Math.min(1, (f - start) / 4, (start + len - f) / 6) : 0);
  const eAt = at(beats, 2, 0.02), spAt = at(beats, 2, 0.4), f8At = at(beats, 2, 0.72);
  const clip = (name: "hud_fp" | "bow" | "inventory" | "build_fp", start: number, vis: number, fit = 1) => (
    <div style={{ position: "absolute", inset: 0, opacity: vis }}>
      <Shot name={name} w={1000} h={620} at={start} fit={fit} />
    </div>
  );
  return (
    <AbsoluteFill>
      <Backdrop dim={0.3} />
      <div style={{ position: "absolute", left: 130, top: 120 }}>
        <div style={{ fontFamily: F.display, fontSize: 110, color: C.text, opacity: prog(f, 0, 12) }}>CONTROLS</div>
      </div>
      <div style={{ position: "absolute", left: 160, top: 330, transform: `scale(${0.9 + 0.1 * pop(f, 4, 14)})` }}>
        <Mouse left={p0 * (0.6 + 0.4 * Math.abs(Math.sin(f / 4)))} right={p1 * (0.6 + 0.4 * Math.abs(Math.sin(f / 4)))} />
      </div>
      <div style={{ position: "absolute", left: 140, top: 760, display: "flex", gap: 20, alignItems: "center" }}>
        <Kbd k="E" lit={keyLit(eAt)} size={56} />
        <Kbd k="Space" lit={keyLit(spAt)} size={56} />
        <Kbd k="F8" lit={keyLit(f8At)} size={56} />
      </div>
      <div style={{ position: "absolute", left: 450, top: 360, display: "flex", flexDirection: "column", gap: 18, width: 380 }}>
        {[{ v: p0, t: "Break · hit", c: C.grass }, { v: p1, t: "Place · eat · bow", c: C.orange }].map((x) => (
          <div key={x.t} style={{ fontFamily: F.display, fontSize: 44, whiteSpace: "nowrap", color: x.c, opacity: 0.25 + x.v * 0.75, textTransform: "uppercase" }}>{x.t}</div>
        ))}
      </div>
      <Glass style={{ position: "absolute", left: 830, top: 250, width: 1000, height: 620, overflow: "hidden", background: "linear-gradient(180deg,#78a7ff,#bcd6ff)" }}>
        <div style={{ position: "absolute", inset: 0, opacity: p0 }}><Shot name="hud_fp" w={1000} h={620} at={at(beats, 0)} fit={1} full /></div>
        {clip("bow", at(beats, 1), p1, 0.9)}
        <div style={{ position: "absolute", inset: 0, opacity: p2 * (f < spAt ? 1 : 0) }}><Shot name="inventory" w={1000} h={620} at={eAt} fit={1.0} full /></div>
        <div style={{ position: "absolute", inset: 0, opacity: p2 * (f >= spAt && f < f8At ? 1 : 0) }}><Shot name="hero_walk" w={1000} h={620} at={spAt} fit={0.9} /></div>
        <div style={{ position: "absolute", inset: 0, opacity: p2 * (f >= f8At ? 1 : 0), display: "flex", alignItems: "center", justifyContent: "center", background: "rgba(0,0,0,0.35)" }}>
          <div style={{ fontFamily: F.display, fontSize: 80, color: C.text }}>F8 · SETTINGS</div>
        </div>
      </Glass>
      <Sfx at={at(beats, 0, 0.05)} name="click" volume={0.4} />
      <Sfx at={at(beats, 1, 0.05)} name="click" volume={0.4} />
      {[eAt, spAt, f8At].map((t) => <Sfx key={t} at={t} name="type" volume={0.6} />)}
    </AbsoluteFill>
  );
};

export const Menu: React.FC<SceneProps> = ({ beats }) => {
  const f = useCurrentFrame();
  const steps: [number, number, string?][] = [
    [at(beats, 0, 0.35), 3, "Off"], [at(beats, 0, 0.55), 4, "Off"], [at(beats, 0, 0.78), 2, "120"],
    [at(beats, 1, 0.1), 7], [at(beats, 1, 0.35), 9, "Off"], [at(beats, 1, 0.45), 10, "Off"], [at(beats, 1, 0.55), 12, "Off"], [at(beats, 1, 0.82), 1, "Medium"],
  ];
  const values: Record<number, string> = { 0: "On", 1: "High", 2: "90", 3: "On", 4: "On", 5: "Placed blocks", 6: "On", 7: "On", 8: "Minecraft", 9: "On", 10: "On", 11: "On", 12: "On" };
  let sel = 0, lastAt = -100;
  for (const [t, row, v] of steps) {
    if (f >= t) {
      sel = row;
      lastAt = t;
      if (v && f >= t + 8) values[row] = v;
    }
  }
  const labels = ["Minecraft view", "Minecraft quality", "Minecraft frame rate", "Crosshair", "Hand", "Block outlines", "Minecraft HUD", "Steve in cars", "Jump",
    "GTA damage", "TNT in GTA", "Arrows hurt people", "Torch light"];
  const rows = labels.map((l, i) => ({ label: l, value: values[i] }));
  const tick = Math.max(0, 1 - (f - lastAt - 8) / 6);
  const inset = (name: "seated" | "phone" | "tnt" | "hud_fp", from: number, to: number) => {
    const v = Math.min(prog(f, from, from + 8), 1 - prog(f, to - 8, to));
    return (
      <Glass style={{ position: "absolute", left: 1100, top: 260, width: 680, height: 560, overflow: "hidden", opacity: v, transform: `scale(${0.94 + 0.06 * v})`,
        background: "linear-gradient(180deg,#78a7ff,#bcd6ff)" }}>
        <Shot name={name} w={680} h={560} at={from} fit={name === "tnt" ? 2.4 : name === "hud_fp" ? 1 : 0.9} full={name === "hud_fp"} dx={name === "tnt" ? 30 : 0} dy={name === "tnt" ? -60 : 0} />
      </Glass>
    );
  };
  const header = (
    <div style={{ height: 110, background: "linear-gradient(135deg,#2c6fb3,#1d4f86)", display: "flex", alignItems: "center", justifyContent: "center",
      fontFamily: F.display, fontSize: 84, color: "white" }}>CraftV</div>
  );
  return (
    <AbsoluteFill>
      <Backdrop dim={0.3} />
      <div style={{ position: "absolute", left: 140, top: 70, transform: `translateY(${(1 - pop(f, 2, 15)) * 50}px)`, opacity: pop(f, 2, 15) }}>
        <GameMenu title="SETTINGS" rows={rows} sel={sel} w={760} header={header} tick={tick} compact />
      </div>
      {inset("hud_fp", at(beats, 0, 0.3), at(beats, 1, 0.05))}
      {inset("seated", at(beats, 1, 0.08), at(beats, 1, 0.33))}
      {inset("tnt", at(beats, 1, 0.42), at(beats, 1, 0.75))}
      <div style={{ position: "absolute", left: 1100, top: 300, width: 680, opacity: Math.min(prog(f, at(beats, 1, 0.8), at(beats, 1, 0.88)), 1) }}>
        <Glass style={{ padding: "30px 34px", borderColor: C.orange }}>
          <Label color={C.orange}>Low FPS?</Label>
          <div style={{ fontFamily: F.ui, fontWeight: 700, fontSize: 38, color: C.text, marginTop: 12 }}>Lower Minecraft quality</div>
        </Glass>
      </div>
      {steps.map(([t]) => <Sfx key={t} at={t} name="tick" volume={0.35} />)}
    </AbsoluteFill>
  );
};
