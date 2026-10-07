// The narration, scene by scene. Each beat is one voice clip; the scene's animation is timed to its beats.
// Spelled the way it should sound ("G T A", "dot A S I"). The on-screen captions use `caption` when given.
export const VOICE = process.env.CRAFTV_VOICE || "am_michael";

export const SCENES = [
  {
    id: "intro",
    beats: [
      { id: "intro1", say: "This is CraftV. It puts real Minecraft inside G T A five.", caption: "This is CraftV. It puts real Minecraft inside GTA V." },
      { id: "intro2", say: "You play as Steve, with your hotbar, your hearts and your hunger, right on the streets of Los Santos." },
      { id: "intro3", say: "You can build anywhere, blow up cars with T N T, and bring your friends along.", caption: "You can build anywhere, blow up cars with TNT, and bring your friends along." },
      { id: "intro4", say: "Setting it up takes about fifteen minutes, and I'll walk you through every step." },
    ],
  },
  {
    id: "warning",
    beats: [
      { id: "warn1", say: "Quick, but important. This is for story mode only." },
      { id: "warn2", say: "Never take mods into G T A Online. You can get your account banned, and CraftV switches itself off if it even sees an online session.",
        caption: "Never take mods into GTA Online. You can get your account banned, and CraftV switches itself off if it even sees an online session." },
    ],
  },
  {
    id: "need",
    beats: [
      { id: "need1", say: "Here's what you'll need. Your own copy of G T A five Legacy, and Minecraft Java Edition.", caption: "Here's what you'll need. Your own copy of GTA V Legacy, and Minecraft Java Edition." },
      { id: "need2", say: "Then a few free tools. Script Hook V, ReShade, and Fabric for Minecraft." },
      { id: "need3", say: "And CraftV itself. Every link is in the description. Only grab them from those official sites." },
    ],
  },
  {
    id: "scripthook",
    part: 1,
    beats: [
      { id: "gta1", say: "Part one is G T A. Start with Script Hook V. Open the zip, and go into the bin folder.", caption: "Part one is GTA. Start with Script Hook V. Open the zip, and go into the bin folder." },
      { id: "gta2", say: "Drag these two files into your G T A folder. That's the one with G T A five dot exe in it.", caption: "Drag these two files into your GTA folder. That's the one with GTA5.exe in it." },
      { id: "gta3", say: "Now unzip CraftV, and drop CraftV dot A S I, and the I N I file, right next to them.", caption: "Now unzip CraftV, and drop CraftV.asi and the .ini file right next to them." },
    ],
  },
  {
    id: "reshade",
    part: 1,
    beats: [
      { id: "rs1", say: "Next up, ReShade. This is what draws Minecraft into G T A's picture.", caption: "Next up, ReShade. This is what draws Minecraft into GTA's picture." },
      { id: "rs2", say: "On reshade dot me, pick the version with full add-on support. The normal one won't work.", caption: "On reshade.me, pick the version with full add-on support. The normal one won't work." },
      { id: "rs3", say: "In the installer, choose G T A five, then DirectX ten, eleven, twelve.", caption: "In the installer, choose GTA5.exe, then DirectX 10/11/12." },
      { id: "rs4", say: "When it offers effect packs, untick all of them. CraftV brings its own." },
    ],
  },
  {
    id: "rename",
    part: 1,
    beats: [
      { id: "rn1", say: "Now, this is the step everyone misses. Find D X G I dot D L L in your G T A folder,", caption: "Now, this is the step everyone misses. Find dxgi.dll in your GTA folder," },
      { id: "rn2", say: "and rename it to ReShade sixty-four dot A S I. That's how Script Hook V knows to load it.", caption: "and rename it to ReShade64.asi. That's how Script Hook V knows to load it." },
      { id: "rn3", say: "Last thing for G T A. Copy everything from the Minecraft view folder in the CraftV zip, into your game folder.", caption: "Last thing for GTA. Copy everything from the \"Minecraft view\" folder in the CraftV zip into your game folder." },
    ],
  },
  {
    id: "fabric",
    part: 2,
    beats: [
      { id: "fb1", say: "Part two, Minecraft. CraftV runs its own copy of Minecraft next to G T A, using your normal account.", caption: "Part two, Minecraft. CraftV runs its own copy of Minecraft next to GTA, using your normal account." },
      { id: "fb2", say: "Run the Fabric installer, pick version twenty six point three, and hit install.", caption: "Run the Fabric installer, pick version 26.3, and hit Install." },
    ],
  },
  {
    id: "launcher",
    part: 2,
    beats: [
      { id: "ln1", say: "In the Minecraft Launcher, open installations, and edit the new Fabric one." },
      { id: "ln2", say: "Give it its own game folder. That way your normal Minecraft stays exactly how it is." },
      { id: "ln3", say: "Inside that folder, make a mods folder, and drop in Fabric A P I and the CraftV mod.", caption: "Inside that folder, make a mods folder, and drop in Fabric API and the CraftV mod." },
    ],
  },
  {
    id: "settings",
    part: 3,
    beats: [
      { id: "st1", say: "Part three is quick. In G T A's graphics settings, set the screen type to windowed borderless.", caption: "Part three is quick. In GTA's graphics settings, set the screen type to Windowed Borderless." },
      { id: "st2", say: "And turn off pause game on focus loss, so G T A keeps running next to Minecraft.", caption: "And turn off \"Pause Game On Focus Loss\", so GTA keeps running next to Minecraft." },
    ],
  },
  {
    id: "play",
    beats: [
      { id: "pl1", say: "And that's the setup. Every time you play, start the Fabric version of Minecraft first. It opens a world called CraftV by itself." },
      { id: "pl2", say: "Leave it running, start G T A, and load story mode.", caption: "Leave it running, start GTA, and load story mode." },
      { id: "pl3", say: "When the little CraftV box in the corner says M C view, you're in.", caption: "When the little CraftV box in the corner says \"MC view\", you're in." },
    ],
  },
  {
    id: "controls",
    beats: [
      { id: "ct1", say: "Left click breaks blocks and hits things. People react, and cars dent." },
      { id: "ct2", say: "Right click places blocks, eats food, and pulls back your bow." },
      { id: "ct3", say: "E opens your inventory, space does a Minecraft jump, and F eight opens the settings.", caption: "E opens your inventory, Space does a Minecraft jump, and F8 opens the settings." },
    ],
  },
  {
    id: "menu",
    beats: [
      { id: "f8a", say: "The settings menu looks just like G T A's. Turn the crosshair on or off, hide your hand, change the frame rate,",
        caption: "The settings menu looks just like GTA's. Turn the crosshair on or off, hide your hand, change the frame rate," },
      { id: "f8b", say: "pick whether Steve sits in cars, and switch damage, T N T and torch light on or off. If your F P S drops, lower the quality here.",
        caption: "pick whether Steve sits in cars, and switch damage, TNT and torch light on or off. If your FPS drops, lower the quality here." },
    ],
  },
  {
    id: "friends",
    beats: [
      { id: "fr1", say: "Want to play with friends? Your address is in that same menu, under friends join at.", caption: "Want to play with friends? Your address is in that same menu, under \"Friends join at\"." },
      { id: "fr2", say: "Friends with normal Minecraft just join that address from multiplayer." },
      { id: "fr3", say: "Friends with their own G T A install everything too, and put your address in one config file. Then you see each other in Los Santos.",
        caption: "Friends with their own GTA install everything too, and put your address in one config file. Then you see each other in Los Santos." },
      { id: "fr4", say: "If they're not on your Wi-Fi, forward port two five five six five on your router.", caption: "If they're not on your Wi-Fi, forward port 25565 on your router." },
    ],
  },
  {
    id: "trouble",
    beats: [
      { id: "tb1", say: "If you don't see the CraftV box at all, Script Hook V isn't loading. Check its files are next to G T A five dot exe.", caption: "If you don't see the CraftV box at all, Script Hook V isn't loading. Check its files are next to GTA5.exe." },
      { id: "tb2", say: "If the box shows up but Minecraft doesn't, check the ReShade rename, and make sure Minecraft started first." },
      { id: "tb3", say: "Still stuck? Open an issue on the GitHub page with your log files." },
    ],
  },
  {
    id: "outro",
    beats: [
      { id: "out1", say: "That's it. Go build something ridiculous in Los Santos. And remember, story mode only. Have fun." },
    ],
  },
];
