import React from "react";
import { Composition } from "remotion";
import { TOTAL_FRAMES, Tutorial } from "./Tutorial";

export const RemotionRoot: React.FC = () => (
  <Composition id="CraftVTutorial" component={Tutorial} durationInFrames={TOTAL_FRAMES} fps={30} width={1920} height={1080} />
);
