package dev.craftv.client;

import dev.craftv.CraftLog;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.screens.TitleScreen;
import net.minecraft.core.registries.Registries;
import net.minecraft.world.Difficulty;
import net.minecraft.world.level.GameType;
import net.minecraft.world.level.LevelSettings;
import net.minecraft.world.level.WorldDataConfiguration;
import net.minecraft.world.level.levelgen.WorldOptions;
import net.minecraft.world.level.levelgen.presets.WorldPresets;

/**
 * With {@code -Dcraftv.autoWorld=true} (set by runClient): opens, or creates, a superflat
 * Creative world called "CraftV Dev" as soon as the title screen is up, so Phase 1 testing is
 * one command. Flat ground makes the host-driven circle easy to see (DECISIONS.md D-006). Adapted
 * from SkyCraft's MirrorWorld (MIT, chasmlol).
 */
public final class DevWorld {
	public static final String WORLD_NAME = "CraftV Dev";
	private static final boolean ENABLED = Boolean.getBoolean("craftv.autoWorld");
	private static boolean attempted;

	private DevWorld() {
	}

	public static void tick(Minecraft minecraft) {
		if (!ENABLED || attempted || minecraft.level != null || minecraft.gui.overlay() != null) {
			return;
		}
		if (!(minecraft.gui.screen() instanceof TitleScreen title)) {
			// First launch shows an onboarding/accessibility screen before the title screen.
			var screen = minecraft.gui.screen();
			if (screen == null || screen.getClass().getName().contains("Onboarding")) {
				CraftLog.info("dev world: skipping " + (screen == null ? "empty screen" : screen.getClass().getSimpleName()) + " to reach the title screen");
				minecraft.gui.setScreen(new TitleScreen());
			} else {
				CraftLog.limited("devworld-wait", 5000, "dev world: waiting on screen " + screen.getClass().getName());
			}
			return;
		}
		attempted = true;
		if (minecraft.getLevelSource().levelExists(WORLD_NAME)) {
			CraftLog.info("dev world: opening '" + WORLD_NAME + "'");
			minecraft.createWorldOpenFlows().openWorld(WORLD_NAME, () -> minecraft.gui.setScreen(title));
			return;
		}
		CraftLog.info("dev world: creating '" + WORLD_NAME + "' (superflat, Creative)");
		LevelSettings settings = new LevelSettings(WORLD_NAME, GameType.CREATIVE, new LevelSettings.DifficultySettings(Difficulty.PEACEFUL, false, false), true,
			WorldDataConfiguration.DEFAULT);
		minecraft.createWorldOpenFlows().createFreshLevel(WORLD_NAME, settings, new WorldOptions(0L, false, false),
			registries -> registries.lookupOrThrow(Registries.WORLD_PRESET).getOrThrow(WorldPresets.FLAT).value().createWorldDimensions(), title);
	}
}
