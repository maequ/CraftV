import React from "react";
import { AbsoluteFill, useCurrentFrame } from "remotion";
import { Backdrop, Glass, GrassBlock, Label, PixelIcon, Sfx, Shot, pop, prog, sec } from "../kit";
import { C, F } from "../theme";
import { SceneProps, at, phase } from "../types";

const Logo: React.FC<{ f: number; scale: number; y: number }> = ({ f, scale, y }) => {
  const letters = "CRAFTV".split("");
  return (
    <div style={{ position: "absolute", left: 0, right: 0, top: y, display: "flex", justifyContent: "center", alignItems: "center", gap: 36 * scale,
      transform: `scale(${scale})`, transformOrigin: "50% 0%" }}>
      <div style={{ transform: `translateY(${(1 - pop(f, 2, 11)) * -600}px) rotate(${(1 - pop(f, 2, 11)) * 40}deg)` }}>
        <GrassBlock size={190} />
      </div>
      <div style={{ display: "flex" }}>
        {letters.map((l, i) => {
          const p = pop(f, 8 + i * 3, 10, 200);
          return (
            <span key={i} style={{ fontFamily: F.display, fontSize: 230, lineHeight: 1, color: i === 5 ? C.orange : C.text,
              display: "inline-block", transform: `translateY(${(1 - p) * -260}px)`, opacity: Math.min(1, p * 2), textShadow: "0 14px 40px rgba(0,0,0,0.5)" }}>{l}</span>
          );
        })}
      </div>
    </div>
  );
};

export const Intro: React.FC<SceneProps> = ({ beats, frames }) => {
  const f = useCurrentFrame();
  const b1 = at(beats, 1), b2 = at(beats, 2), b3 = at(beats, 3);
  const logoMove = prog(f, b1 - 6, b1 + 14);
  const p0 = phase(f, beats, 0, 1, frames), p1 = phase(f, beats, 1, 2, frames), p2 = phase(f, beats, 2, 3, frames), p3 = phase(f, beats, 3, null, frames);
  const features = [
    { shot: "build_fp" as const, title: "Build anywhere", at: at(beats, 2, 0.02) },
    { shot: "tnt" as const, title: "Blow up cars", at: at(beats, 2, 0.32) },
    { shot: "hero_walk" as const, title: "Bring friends", at: at(beats, 2, 0.66) },
  ];
  const parts = ["GTA V", "Minecraft", "Settings"];
  return (
    <AbsoluteFill>
      <Backdrop dim={0.05} />
      <Sfx at={4} name="whoosh" volume={0.35} />
      {[8, 11, 14, 17, 20, 23].map((t) => <Sfx key={t} at={t + 6} name="pop" volume={0.18} />)}
      <div style={{ opacity: 1 - prog(f, b1 - 4, b1 + 10) * 1 }}>
        <Logo f={f} scale={1 - logoMove * 0.55} y={300 - logoMove * 250} />
        <div style={{ position: "absolute", top: 600, width: "100%", textAlign: "center", fontFamily: F.ui, fontWeight: 600, fontSize: 46, color: C.muted,
          opacity: prog(f, 26, 44) * p0, transform: `translateY(${(1 - prog(f, 26, 44)) * 20}px)` }}>
          Real Minecraft, inside <span style={{ color: C.text }}>GTA V</span> story mode
        </div>
      </div>

      {/* you play as Steve: the hero shot with the HUD pieces called out */}
      <AbsoluteFill style={{ opacity: p1 }}>
        <div style={{ position: "absolute", right: 120, top: 70, transform: `scale(${0.9 + 0.1 * pop(f, b1, 14)})` }}>
          <Shot name="hero_orbit" w={820} h={900} at={b1} fit={0.95} />
        </div>
        <div style={{ position: "absolute", left: 150, top: 230 }}>
          <Label style={{ opacity: prog(f, b1, b1 + 10) }}>You play as Steve</Label>
          {["Your hotbar", "Your hearts", "Your hunger"].map((t, i) => {
            const p = pop(f, at(beats, 1, 0.3 + i * 0.12), 14);
            return (
              <div key={t} style={{ marginTop: 34, display: "flex", alignItems: "center", gap: 26, transform: `translateX(${(1 - p) * -80}px)`, opacity: Math.min(1, p * 1.5) }}>
                <div style={{ width: 74, height: 74, borderRadius: 16, background: "rgba(98,193,59,0.18)", border: `2px solid ${C.grass}`, display: "flex",
                  alignItems: "center", justifyContent: "center", }}><PixelIcon kind={(["hotbar", "heart", "food"] as const)[i]} /></div>
                <div style={{ fontFamily: F.display, fontSize: 84, color: C.text, textTransform: "uppercase" }}>{t}</div>
              </div>
            );
          })}
          <div style={{ marginTop: 40, fontFamily: F.ui, fontSize: 34, color: C.muted, opacity: prog(f, at(beats, 1, 0.7), at(beats, 1, 0.85)) }}>
            ...on the streets of Los Santos
          </div>
        </div>
        {[0.3, 0.42, 0.54].map((k) => <Sfx key={k} at={at(beats, 1, k)} name="pop" volume={0.3} />)}
      </AbsoluteFill>

      {/* three things you can do */}
      <AbsoluteFill style={{ opacity: p2, display: "flex", flexDirection: "row", alignItems: "center", justifyContent: "center", gap: 44 }}>
        {features.map((x) => {
          const p = pop(f, x.at, 13);
          return (
            <Glass key={x.title} style={{ width: 520, height: 600, overflow: "hidden", transform: `translateY(${(1 - p) * 120}px) rotate(${(1 - p) * 4}deg)`,
              opacity: Math.min(1, p * 1.6), position: "relative" }}>
              <div style={{ position: "absolute", inset: 0, background: "linear-gradient(180deg, rgba(58,29,79,0.0), rgba(194,65,107,0.35))" }} />
              <Shot name={x.shot} w={520} h={470} at={x.at} fit={x.shot === "tnt" ? 2.6 : 0.9} dx={x.shot === "tnt" ? 42 : 0} dy={x.shot === "tnt" ? -70 : 0} />
              <div style={{ position: "absolute", bottom: 34, left: 34, fontFamily: F.display, fontSize: 66, color: C.text, textTransform: "uppercase" }}>{x.title}</div>
            </Glass>
          );
        })}
        {features.map((x) => <Sfx key={x.title} at={x.at} name="whoosh" volume={0.22} />)}
      </AbsoluteFill>

      {/* three parts, about fifteen minutes */}
      <AbsoluteFill style={{ opacity: p3, alignItems: "center", justifyContent: "center" }}>
        <Label style={{ opacity: prog(f, b3, b3 + 10), marginBottom: 40 }}>Setup · about 15 minutes</Label>
        <div style={{ display: "flex", alignItems: "center" }}>
          {parts.map((t, i) => {
            const p = pop(f, at(beats, 3, 0.1 + i * 0.18), 13);
            const line = prog(f, at(beats, 3, 0.1 + i * 0.18), at(beats, 3, 0.28 + i * 0.18));
            return (
              <React.Fragment key={t}>
                {i > 0 && (
                  <div style={{ width: 160, height: 8, background: "rgba(255,255,255,0.12)", borderRadius: 4, overflow: "hidden" }}>
                    <div style={{ width: `${line * 100}%`, height: "100%", background: C.grass }} />
                  </div>
                )}
                <div style={{ transform: `scale(${p})`, display: "flex", flexDirection: "column", alignItems: "center", gap: 18 }}>
                  <div style={{ width: 150, height: 150, borderRadius: 30, background: C.grass, display: "flex", alignItems: "center", justifyContent: "center",
                    fontFamily: F.display, fontSize: 96, color: C.night, boxShadow: "0 18px 50px rgba(98,193,59,0.4)" }}>{i + 1}</div>
                  <div style={{ fontFamily: F.display, fontSize: 60, color: C.text, textTransform: "uppercase" }}>{t}</div>
                </div>
              </React.Fragment>
            );
          })}
        </div>
        {[0.1, 0.28, 0.46].map((k) => <Sfx key={k} at={at(beats, 3, k)} name="pop" volume={0.3} />)}
      </AbsoluteFill>
      {f < sec(0.4) && <div style={{ position: "absolute", inset: 0, background: "#000", opacity: 1 - f / sec(0.4) }} />}
    </AbsoluteFill>
  );
};
