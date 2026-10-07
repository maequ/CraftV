import React from "react";
import { AbsoluteFill, useCurrentFrame } from "remotion";
import { Backdrop, Glass, GrassBlock, Headline, Label, Sfx, pop, prog, sec } from "../kit";
import { C, F } from "../theme";
import { SceneProps, at } from "../types";

export const Warning: React.FC<SceneProps> = ({ beats, frames }) => {
  const f = useCurrentFrame();
  const stamp = at(beats, 0, 0.45);
  const p = pop(f, stamp, 9, 260);
  const shake = f >= stamp && f < stamp + 10 ? Math.sin((f - stamp) * 2.4) * (10 - (f - stamp)) * 1.6 : 0;
  const strike = prog(f, at(beats, 1, 0.12), at(beats, 1, 0.3));
  const shield = pop(f, at(beats, 1, 0.62), 14);
  return (
    <AbsoluteFill style={{ transform: `translate(${shake}px, ${shake * 0.6}px)` }}>
      <Backdrop tint="rgba(120,10,20,0.45)" dim={0.1} />
      <Sfx at={stamp} name="stamp" volume={0.7} />
      <AbsoluteFill style={{ alignItems: "center", justifyContent: "center", gap: 46 }}>
        <Label color="#ffb3b3" style={{ opacity: prog(f, 4, 16) }}>Before anything else</Label>
        <div style={{ transform: `scale(${2.2 - 1.2 * p}) rotate(${-4 + (1 - p) * 8}deg)`, opacity: Math.min(1, p * 2), border: `10px solid ${C.red}`,
          borderRadius: 18, padding: "18px 56px", fontFamily: F.display, fontSize: 170, color: C.red, background: "rgba(20,0,4,0.55)",
          textTransform: "uppercase", lineHeight: 1, boxShadow: "0 0 80px rgba(255,77,77,0.35)" }}>Story mode only</div>
        <div style={{ display: "flex", gap: 40, alignItems: "center", opacity: prog(f, at(beats, 1), at(beats, 1, 0.1)) }}>
          <div style={{ position: "relative", fontFamily: F.display, fontSize: 90, color: C.text, textTransform: "uppercase" }}>
            GTA Online
            <div style={{ position: "absolute", left: -10, right: -10, top: "52%", height: 12, background: C.red, transformOrigin: "left center",
              transform: `scaleX(${strike}) rotate(-3deg)`, borderRadius: 6 }} />
          </div>
          <div style={{ fontFamily: F.ui, fontWeight: 700, fontSize: 40, color: "#ffb3b3", opacity: strike }}>= account ban risk</div>
        </div>
        <Glass style={{ padding: "20px 34px", display: "flex", alignItems: "center", gap: 22, transform: `scale(${shield})`, opacity: shield }}>
          <div style={{ width: 54, height: 62, background: C.grass, clipPath: "polygon(50% 0, 100% 18%, 92% 70%, 50% 100%, 8% 70%, 0 18%)", display: "flex",
            alignItems: "center", justifyContent: "center", fontFamily: F.ui, fontWeight: 800, fontSize: 30, color: C.night }}>✓</div>
          <div style={{ fontFamily: F.ui, fontWeight: 600, fontSize: 34, color: C.text }}>CraftV switches itself off if it sees an online session</div>
        </Glass>
      </AbsoluteFill>
      <Sfx at={at(beats, 1, 0.12)} name="whoosh" volume={0.25} />
      <Sfx at={at(beats, 1, 0.62)} name="pop" volume={0.3} />
    </AbsoluteFill>
  );
};

const Tile: React.FC<{ f: number; at: number; title: string; sub: string; icon: React.ReactNode; accent: string }> = ({ f, at: start, title, sub, icon, accent }) => {
  const p = pop(f, start, 12);
  return (
    <Glass style={{ width: 500, height: 250, padding: 32, display: "flex", gap: 28, alignItems: "center", transform: `translateY(${(1 - p) * 90}px) scale(${0.9 + p * 0.1})`,
      opacity: Math.min(1, p * 1.5), borderColor: accent + "55" }}>
      <div style={{ width: 130, height: 130, flex: "none", borderRadius: 26, background: accent + "26", border: `2px solid ${accent}`, display: "flex",
        alignItems: "center", justifyContent: "center" }}>{icon}</div>
      <div>
        <div style={{ fontFamily: F.display, fontSize: 46, lineHeight: 1, color: C.text, textTransform: "uppercase", whiteSpace: "nowrap" }}>{title}</div>
        <div style={{ fontFamily: F.ui, fontWeight: 500, fontSize: 26, color: C.muted, marginTop: 12 }}>{sub}</div>
      </div>
    </Glass>
  );
};

const Letter: React.FC<{ c: string; color: string }> = ({ c, color }) => <div style={{ fontFamily: F.display, fontSize: 92, color }}>{c}</div>;

export const Need: React.FC<SceneProps> = ({ beats }) => {
  const f = useCurrentFrame();
  const items = [
    { at: at(beats, 0, 0.42), title: "GTA V Legacy", sub: "your own copy, story mode", icon: <Letter c="V" color={C.orange} />, accent: C.orange },
    { at: at(beats, 0, 0.72), title: "Minecraft Java", sub: "your own account", icon: <GrassBlock size={100} />, accent: C.grass },
    { at: at(beats, 1, 0.3), title: "Script Hook V", sub: "dev-c.com", icon: <Letter c="SH" color="#8ab4ff" />, accent: "#5b8cff" },
    { at: at(beats, 1, 0.52), title: "ReShade", sub: "reshade.me · add-on version", icon: <Letter c="R" color="#ff8fd0" />, accent: "#e45bb0" },
    { at: at(beats, 1, 0.75), title: "Fabric", sub: "fabricmc.net · for 26.3", icon: <Letter c="F" color="#e9d8b0" />, accent: "#c8a86a" },
    { at: at(beats, 2, 0.05), title: "CraftV", sub: "GitHub · Releases", icon: <GrassBlock size={100} kind="tnt" />, accent: C.red },
  ];
  const link = pop(f, at(beats, 2, 0.35), 12);
  return (
    <AbsoluteFill>
      <Backdrop dim={0.15} />
      <div style={{ position: "absolute", left: 150, top: 80 }}>
        <Headline at={4} size={120}>What you need</Headline>
      </div>
      <div style={{ position: "absolute", left: 150, top: 250, display: "grid", gridTemplateColumns: "repeat(3, 500px)", gap: 36 }}>
        {items.map((x) => <Tile key={x.title} f={f} {...x} />)}
      </div>
      {items.map((x) => <Sfx key={x.title} at={x.at} name="pop" volume={0.3} />)}
      <div style={{ position: "absolute", left: 150, top: 812, display: "flex", alignItems: "center", gap: 20, opacity: link,
        transform: `translateY(${(1 - link) * 30}px)` }}>
        <div style={{ fontFamily: F.ui, fontWeight: 700, fontSize: 36, color: C.text }}>Every link is in the description</div>
        <div style={{ fontFamily: F.display, fontSize: 60, color: C.grass, transform: `translateY(${Math.sin(f / 5) * 6}px)` }}>↓</div>
        <div style={{ fontFamily: F.ui, fontSize: 28, color: C.muted, marginLeft: 20 }}>official sites only</div>
      </div>
    </AbsoluteFill>
  );
};
