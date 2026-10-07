import React from "react";
import { AbsoluteFill, useCurrentFrame } from "remotion";
import { Backdrop, Glass, GrassBlock, Headline, Label, Sfx, Shot, Typed, TypingSfx, pop, prog, sec } from "../kit";
import { C, F } from "../theme";
import { SceneProps, at, phase } from "../types";

const Node: React.FC<{ x: number; y: number; p: number; title: string; sub: string; children?: React.ReactNode; accent?: string }> = ({ x, y, p, title, sub, children, accent = C.grass }) => (
  <div style={{ position: "absolute", left: x, top: y, transform: `translate(-50%, -50%) scale(${p})`, opacity: Math.min(1, p * 1.5) }}>
    <Glass style={{ width: 420, padding: "26px 30px", borderColor: accent + "88", textAlign: "center" }}>
      <div style={{ height: 150, display: "flex", alignItems: "center", justifyContent: "center" }}>{children}</div>
      <div style={{ fontFamily: F.display, fontSize: 48, color: C.text, textTransform: "uppercase", marginTop: 10 }}>{title}</div>
      <div style={{ fontFamily: F.ui, fontWeight: 500, fontSize: 24, color: C.muted, marginTop: 6 }}>{sub}</div>
    </Glass>
  </div>
);

const Wire: React.FC<{ a: [number, number]; b: [number, number]; p: number; f: number; color?: string }> = ({ a, b, p, f, color = C.grass }) => {
  const len = Math.hypot(b[0] - a[0], b[1] - a[1]);
  const ang = Math.atan2(b[1] - a[1], b[0] - a[0]);
  return (
    <div style={{ position: "absolute", left: a[0], top: a[1], width: len * p, height: 6, background: `repeating-linear-gradient(90deg, ${color} 0 18px, transparent 18px 30px)`,
      backgroundPosition: `${-f * 2}px 0`, transformOrigin: "0 50%", transform: `rotate(${ang}rad)`, opacity: 0.9, borderRadius: 3 }} />
  );
};

export const Friends: React.FC<SceneProps> = ({ beats }) => {
  const f = useCurrentFrame();
  const you = pop(f, 4, 13);
  const addr = pop(f, at(beats, 0, 0.55), 12);
  const plain = pop(f, at(beats, 1, 0.1), 13), wire1 = prog(f, at(beats, 1, 0.3), at(beats, 1, 0.6));
  const gta = pop(f, at(beats, 2, 0.05), 13), wire2 = prog(f, at(beats, 2, 0.2), at(beats, 2, 0.45));
  const cfgAt = at(beats, 2, 0.3);
  const router = pop(f, at(beats, 3, 0.1), 13);
  const YOU: [number, number] = [960, 260], A: [number, number] = [330, 630], B: [number, number] = [1590, 630];
  return (
    <AbsoluteFill>
      <Backdrop dim={0.3} />
      <Wire a={[YOU[0] - 210, YOU[1] + 60]} b={[A[0] + 100, A[1] - 150]} p={wire1} f={f} />
      <Wire a={[YOU[0] + 210, YOU[1] + 60]} b={[B[0] - 100, B[1] - 150]} p={wire2} f={f} color={C.orange} />
      <Node x={YOU[0]} y={YOU[1]} p={you} title="You" sub="GTA V + CraftV, hosting the world">
        <Shot name="hero_orbit" w={260} h={150} at={0} fit={1.1} />
      </Node>
      <div style={{ position: "absolute", left: YOU[0], top: YOU[1] + 175, transform: `translateX(-50%) scale(${addr})`, opacity: addr }}>
        <div style={{ padding: "12px 26px", borderRadius: 40, background: C.grass, fontFamily: F.mono, fontWeight: 700, fontSize: 26, color: C.night, whiteSpace: "nowrap",
          boxShadow: "0 12px 40px rgba(98,193,59,0.45)" }}>192.168.1.23:25565</div>
      </div>
      <Node x={A[0]} y={A[1]} p={plain} title="Plain Minecraft" sub="Multiplayer › Direct Connect">
        <GrassBlock size={140} />
      </Node>
      <Node x={B[0]} y={B[1]} p={gta} title="Their own GTA" sub="installs CraftV too" accent={C.orange}>
        <Shot name="phone" w={260} h={150} at={at(beats, 2)} fit={1} />
      </Node>
      <div style={{ position: "absolute", left: B[0] - 300, top: B[1] + 170, width: 500, opacity: prog(f, cfgAt - 4, cfgAt + 6) }}>
        <Glass style={{ padding: "16px 20px", fontFamily: F.mono, fontSize: 24, color: C.text, background: C.panelSolid }}>
          <div style={{ color: C.muted, fontSize: 20 }}>config\craftv.properties</div>
          <div style={{ marginTop: 6 }}>guest.join=<Typed text="192.168.1.23:25565" at={cfgAt + 8} cps={18} style={{ color: C.grass }} /></div>
        </Glass>
      </div>
      <div style={{ position: "absolute", left: 960, top: 660, transform: `translate(-50%, -50%) scale(${router})`, opacity: router }}>
        <Glass style={{ padding: "22px 30px", textAlign: "center", borderColor: C.orange }}>
          <Label color={C.orange}>Not on your Wi-Fi?</Label>
          <div style={{ fontFamily: F.ui, fontWeight: 700, fontSize: 32, color: C.text, marginTop: 10 }}>Router › forward TCP port</div>
          <div style={{ fontFamily: F.display, fontSize: 86, color: C.orange, lineHeight: 1.05 }}>25565</div>
        </Glass>
      </div>
      <Sfx at={4} name="pop" volume={0.3} />
      <Sfx at={at(beats, 0, 0.55)} name="pop" volume={0.35} />
      <Sfx at={at(beats, 1, 0.1)} name="pop" volume={0.3} />
      <Sfx at={at(beats, 2, 0.05)} name="pop" volume={0.3} />
      <TypingSfx text="192.168.1.23:25565" at={cfgAt + 8} cps={18} />
      <Sfx at={at(beats, 3, 0.1)} name="pop" volume={0.35} />
    </AbsoluteFill>
  );
};

export const Trouble: React.FC<SceneProps> = ({ beats }) => {
  const f = useCurrentFrame();
  const items = [
    { at: at(beats, 0, 0.05), problem: "No CraftV box", fix: "Script Hook V files next to GTA5.exe" },
    { at: at(beats, 1, 0.05), problem: "Box, but no Minecraft", fix: "Rename to ReShade64.asi · start Minecraft first" },
    { at: at(beats, 2, 0.05), problem: "Still stuck", fix: "Open an issue on GitHub with CraftV.log and ReShade.log" },
  ];
  return (
    <AbsoluteFill>
      <Backdrop dim={0.3} />
      <div style={{ position: "absolute", left: 150, top: 110 }}>
        <Headline at={2} size={110}>If something's off</Headline>
      </div>
      <div style={{ position: "absolute", left: 150, top: 300, display: "flex", flexDirection: "column", gap: 30 }}>
        {items.map((x) => {
          const p = pop(f, x.at, 13);
          const fixed = prog(f, x.at + sec(1.2), x.at + sec(1.6));
          return (
            <div key={x.problem} style={{ display: "flex", alignItems: "center", gap: 30, transform: `translateX(${(1 - p) * -120}px)`, opacity: Math.min(1, p * 1.5) }}>
              <Glass style={{ width: 640, padding: "24px 30px", display: "flex", alignItems: "center", gap: 20, borderColor: C.red + "88" }}>
                <div style={{ fontFamily: F.display, fontSize: 52, color: C.red }}>✕</div>
                <div style={{ fontFamily: F.display, fontSize: 48, color: C.text, textTransform: "uppercase", whiteSpace: "nowrap" }}>{x.problem}</div>
              </Glass>
              <div style={{ fontFamily: F.display, fontSize: 60, color: C.muted, opacity: fixed }}>→</div>
              <Glass style={{ width: 860, padding: "24px 30px", display: "flex", alignItems: "center", gap: 20, borderColor: C.grass + "88", opacity: fixed,
                transform: `translateX(${(1 - fixed) * 40}px)` }}>
                <div style={{ fontFamily: F.display, fontSize: 52, color: C.grass }}>✓</div>
                <div style={{ fontFamily: F.ui, fontWeight: 700, fontSize: 34, color: C.text }}>{x.fix}</div>
              </Glass>
            </div>
          );
        })}
      </div>
      {items.map((x) => <Sfx key={x.problem} at={x.at} name="pop" volume={0.3} />)}
      {items.map((x) => <Sfx key={x.fix} at={x.at + sec(1.2)} name="success" volume={0.25} />)}
    </AbsoluteFill>
  );
};

export const Outro: React.FC<SceneProps> = ({ beats, frames }) => {
  const f = useCurrentFrame();
  const p = pop(f, 4, 12);
  const out = 1 - prog(f, frames - 20, frames);
  return (
    <AbsoluteFill style={{ opacity: out }}>
      <Backdrop dim={0.05} />
      <div style={{ position: "absolute", right: 120, top: 80 }}>
        <Shot name="hero_orbit" w={720} h={860} at={0} fit={0.95} />
      </div>
      <div style={{ position: "absolute", left: 150, top: 260, transform: `translateY(${(1 - p) * 60}px)`, opacity: p }}>
        <div style={{ display: "flex", alignItems: "center", gap: 30 }}>
          <GrassBlock size={150} />
          <div style={{ fontFamily: F.display, fontSize: 200, color: C.text, lineHeight: 1 }}>CRAFT<span style={{ color: C.orange }}>V</span></div>
        </div>
        <div style={{ fontFamily: F.ui, fontWeight: 600, fontSize: 44, color: C.muted, marginTop: 30, opacity: prog(f, at(beats, 0, 0.15), at(beats, 0, 0.3)) }}>
          Go build something ridiculous.
        </div>
        <div style={{ marginTop: 50, display: "inline-block", padding: "14px 28px", borderRadius: 12, border: `3px solid ${C.red}`, fontFamily: F.display, fontSize: 48,
          color: C.red, opacity: prog(f, at(beats, 0, 0.5), at(beats, 0, 0.6)) }}>STORY MODE ONLY</div>
        <div style={{ fontFamily: F.mono, fontSize: 30, color: C.text, marginTop: 40, opacity: prog(f, at(beats, 0, 0.7), at(beats, 0, 0.8)) }}>Links in the description</div>
      </div>
      <Sfx at={4} name="whoosh" volume={0.3} />
      <Sfx at={at(beats, 0, 0.5)} name="stamp" volume={0.35} />
    </AbsoluteFill>
  );
};
