# The setup video's narration, one entry per slide in deck.py. Windows' voice reads it, so it's spelled the way it should sound.
SLIDES = [
    {
        "say": "This is how to set up CraftV. It lets you play Minecraft inside G T A five story mode, with your friends. "
               "By the end of this video you'll be playing. The whole setup takes about fifteen minutes.",
    },
    {
        "say": "First, an important warning. CraftV is for story mode only. Never use mods in G T A Online, "
               "because you can get your Rockstar account banned. CraftV turns itself off if it sees an online session, "
               "and Script Hook V closes the game if you try to go online.",
    },
    {
        "say": "Here's what you need. Your own copy of G T A five Legacy, not Enhanced. Script Hook V, from dev dash c dot com. "
               "ReShade, the version with add-on support, from reshade dot me. Minecraft Java Edition. "
               "Fabric for Minecraft twenty six point three, and Fabric A P I. And CraftV itself, from the Releases page on GitHub. "
               "All the links are in the description. Only download them from those sites.",
    },
    {
        "say": "Part one is G T A. Download Script Hook V and open the zip. Go into its bin folder, "
               "and copy script hook V dot D L L and D input eight dot D L L into your G T A five folder. "
               "That's the folder with G T A five dot exe in it.",
    },
    {
        "say": "Next, download the CraftV zip and unzip it. Copy CraftV dot A S I and CraftV dot I N I into the same G T A five folder.",
    },
    {
        "say": "Now ReShade. On reshade dot me, pick download with full add-on support. The normal version won't work. "
               "Run the installer, pick G T A five dot exe, then pick DirectX ten, eleven, twelve. "
               "When it asks about effect packages, untick all of them, because CraftV brings its own. Then finish.",
    },
    {
        "say": "This step is easy to miss. In your G T A five folder, find D X G I dot D L L, and rename it to ReShade sixty four dot A S I. "
               "G T A doesn't load ReShade the normal way, but Script Hook V loads anything that ends in dot A S I.",
    },
    {
        "say": "Last step for G T A. In the CraftV zip, open the folder called Minecraft view. Copy everything inside it into your G T A five folder. "
               "That's two I N I files and a folder called reshade shaders. If Windows asks, replace the files.",
    },
    {
        "say": "Part two is Minecraft. CraftV runs its own copy of Minecraft next to G T A, with your normal account. "
               "Download the Fabric installer from fabric M C dot net and run it. Pick Minecraft twenty six point three and click install. "
               "A new installation called fabric loader appears in the Minecraft Launcher.",
    },
    {
        "say": "Open the Minecraft Launcher and go to installations. Click edit on the fabric loader installation. "
               "Under game directory, pick a new empty folder, for example dot minecraft dash craftv in your app data folder. Click save. "
               "This keeps your normal Minecraft and its settings exactly as they are.",
    },
    {
        "say": "Download Fabric A P I for twenty six point three from Modrinth. In the game directory you just picked, make a folder called mods. "
               "Put the Fabric A P I file in it, and the CraftV jar from the Minecraft mod folder in the CraftV zip.",
    },
    {
        "say": "Part three. Start G T A once and open settings, then graphics. Set screen type to windowed borderless, "
               "and set pause game on focus loss to off. Turning depth of field off is a good idea too, "
               "because G T A blurs its own picture but not Minecraft's.",
    },
    {
        "say": "That's the install done. Every time you play, start the fabric loader installation in the Minecraft Launcher first. "
               "It opens a world called CraftV by itself. Leave that window open. The first time, Windows may ask if Java can use the network. "
               "Allow it on private networks. Then start G T A and load story mode. When the CraftV box in the top right says M C view, it's working.",
    },
    {
        "say": "Here are the controls. Left mouse breaks blocks and hits people and cars. Right mouse places blocks, eats, uses the bow and lights T N T. "
               "The number keys and the mouse wheel pick your hotbar slot. E opens your inventory. Space is a Minecraft jump. "
               "F seven turns the Minecraft view on and off, and F eight opens CraftV's settings. Walking, driving and the camera are normal G T A.",
    },
    {
        "say": "Press F eight in G T A for the settings menu. You can turn the crosshair, the hand and the hotbar on or off, "
               "change the Minecraft frame rate, choose whether Steve sits in cars, switch between the Minecraft jump and the G T A jump, "
               "and turn G T A damage, T N T, arrows and torch light on or off. If your F P S is low, lower the Minecraft quality and frame rate.",
    },
    {
        "say": "To play with friends, find your address in F eight, under friends join at. "
               "A friend with plain Minecraft goes to multiplayer, direct connect, and types your address. "
               "A friend with their own G T A does all three parts on their PC, then copies the config folder from the CraftV zip "
               "and puts your address after guest join in the craftv properties file. "
               "If they're not on your Wi-Fi, forward port two five five six five on your router and give them your public I P address. "
               "Only give it to people you trust.",
    },
    {
        "say": "If something's wrong, here are the usual fixes. If there's no CraftV box, Script Hook V or CraftV dot A S I isn't next to G T A five dot exe. "
               "If you see the box but no Minecraft, check that you renamed ReShade, that you have the add-on version, and that Minecraft started first. "
               "If the game freezes when you click another window, turn off pause game on focus loss. "
               "If you're still stuck, open an issue on the GitHub page with your CraftV log and ReShade log. Both are in your G T A folder.",
    },
    {
        "say": "That's it, you're set up. All the links and the full guide are on the GitHub page. Remember, story mode only. Have fun.",
    },
]
