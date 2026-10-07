import React from "react";
import { SceneProps } from "../types";
import { Need, Warning } from "./Basics";
import { Friends, Outro, Trouble } from "./Friends";
import { ReShade, Rename, ScriptHook } from "./Gta";
import { Intro } from "./Intro";
import { Fabric, Launcher } from "./Minecraft";
import { Controls, Menu, Play, Settings } from "./Play";

export const SCENES: Record<string, React.FC<SceneProps>> = {
  intro: Intro,
  warning: Warning,
  need: Need,
  scripthook: ScriptHook,
  reshade: ReShade,
  rename: Rename,
  fabric: Fabric,
  launcher: Launcher,
  settings: Settings,
  play: Play,
  controls: Controls,
  menu: Menu,
  friends: Friends,
  trouble: Trouble,
  outro: Outro,
};
