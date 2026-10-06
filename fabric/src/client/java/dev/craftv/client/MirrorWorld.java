package dev.craftv.client;

import dev.craftv.CraftLog;
import dev.craftv.CraftV;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.screens.ConnectScreen;
import net.minecraft.client.gui.screens.TitleScreen;
import net.minecraft.client.multiplayer.ServerData;
import net.minecraft.client.multiplayer.resolver.ServerAddress;
import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.Difficulty;
import net.minecraft.world.level.GameType;
import net.minecraft.world.level.LevelSettings;
import net.minecraft.world.level.WorldDataConfiguration;
import net.minecraft.world.level.levelgen.WorldOptions;
import net.minecraft.world.level.levelgen.presets.WorldPreset;

/**
 * Gets this Minecraft into the right world without clicks (brief §6.2). Adapted from SkyCraft's
 * MirrorWorld (MIT, chasmlol), including its {@code mirror} preset: a void world in a tall dimension
 * (min_y -1024, height 2048), so the host game's heights fit at 1 metre = 1 block. Its ground is built
 * from the host's TERRAIN_PATCHes.
 * <ul>
 * <li>Host ({@code -Dcraftv.autoWorld=true}, set by runClient): opens, or creates, the world "CraftV".</li>
 * <li>Stand-in friend ({@code -Dcraftv.role=friend}, test only): joins {@code -Dcraftv.join} like a
 * player using Multiplayer would, retrying every few seconds until the host is up.</li>
 * </ul>
 */
public final class MirrorWorld {
	public static final String WORLD_NAME = "CraftV";
	public static final boolean FRIEND = "friend".equals(System.getProperty("craftv.role"));
	private static final boolean AUTO_WORLD = Boolean.getBoolean("craftv.autoWorld");
	private static final String JOIN = System.getProperty("craftv.join", "localhost:25565");
	private static final long JOIN_RETRY_MS = 5000;
	private static final ResourceKey<WorldPreset> PRESET = ResourceKey.create(Registries.WORLD_PRESET, Identifier.fromNamespaceAndPath(CraftV.MOD_ID, "mirror"));

	private static boolean worldAttempted;
	private static long nextJoinMs;

	private MirrorWorld() {
	}

	/** Where this Minecraft joins by itself: the stand-in friend's -Dcraftv.join, or a guest's guest.join. Empty: nowhere. */
	private static String joinAddress() {
		if (FRIEND) {
			return JOIN;
		}
		var config = dev.craftv.coop.CoopServer.config();
		return AUTO_WORLD || config == null ? "" : config.guestJoin;
	}

	public static void tick(Minecraft minecraft) {
		String join = joinAddress();
		boolean joining = !join.isEmpty();
		if ((!AUTO_WORLD && !joining) || minecraft.level != null || minecraft.gui.overlay() != null) {
			return;
		}
		if (!(minecraft.gui.screen() instanceof TitleScreen title)) {
			// First launch shows an onboarding/accessibility screen before the title screen.
			var screen = minecraft.gui.screen();
			if (screen == null || screen.getClass().getName().contains("Onboarding") || (joining && screen.getClass().getName().contains("Disconnected"))) {
				CraftLog.info("mirror world: skipping " + (screen == null ? "empty screen" : screen.getClass().getSimpleName()) + " to reach the title screen");
				minecraft.gui.setScreen(new TitleScreen());
			} else {
				CraftLog.limited("mirror-wait", 5000, "mirror world: waiting on screen " + screen.getClass().getName());
			}
			return;
		}
		if (joining) {
			join(minecraft, title, join);
			return;
		}
		if (worldAttempted) {
			return;
		}
		worldAttempted = true;
		if (minecraft.getLevelSource().levelExists(WORLD_NAME)) {
			CraftLog.info("mirror world: opening '" + WORLD_NAME + "'");
			minecraft.createWorldOpenFlows().openWorld(WORLD_NAME, () -> minecraft.gui.setScreen(title));
			return;
		}
		CraftLog.info("mirror world: creating '" + WORLD_NAME + "' (void, tall, Creative; the ground comes from the host)");
		LevelSettings settings = new LevelSettings(WORLD_NAME, GameType.CREATIVE, new LevelSettings.DifficultySettings(Difficulty.PEACEFUL, false, false), true,
			WorldDataConfiguration.DEFAULT);
		minecraft.createWorldOpenFlows().createFreshLevel(WORLD_NAME, settings, new WorldOptions(0L, false, false),
			registries -> registries.lookupOrThrow(Registries.WORLD_PRESET).getOrThrow(PRESET).value().createWorldDimensions(), title);
	}

	private static void join(Minecraft minecraft, TitleScreen title, String address) {
		long now = System.currentTimeMillis();
		if (now < nextJoinMs) {
			return;
		}
		nextJoinMs = now + JOIN_RETRY_MS;
		CraftLog.info((FRIEND ? "friend" : "guest") + ": joining " + address);
		ConnectScreen.startConnecting(title, minecraft, ServerAddress.parseString(address), new ServerData("CraftV", address, ServerData.Type.OTHER), false, null);
	}
}
