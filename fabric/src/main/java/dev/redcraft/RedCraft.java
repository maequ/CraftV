package dev.redcraft;

import net.fabricmc.api.ModInitializer;
import net.fabricmc.fabric.api.event.lifecycle.v1.ServerLifecycleEvents;
import net.fabricmc.fabric.api.event.lifecycle.v1.ServerTickEvents;
import net.fabricmc.loader.api.FabricLoader;
import net.minecraft.server.MinecraftServer;
import net.minecraft.world.level.gamerules.GameRules;

/**
 * RedCraft: Minecraft inside Red Dead Redemption 2 (story mode only). This is the Minecraft half;
 * the RDR2 half is an ASI plugin (Phase 2). Architecture adapted from SkyCraft by chasmlol (MIT).
 */
public final class RedCraft implements ModInitializer {
	public static final String MOD_ID = "redcraft";

	public static String version() {
		return FabricLoader.getInstance().getModContainer(MOD_ID).map(c -> c.getMetadata().getVersion().getFriendlyString()).orElse("dev");
	}

	@Override
	public void onInitialize() {
		RedLog.open(FabricLoader.getInstance().getGameDir());
		RedLog.info("RedCraft " + version() + " loading (protocol 1.0)");
		ServerLifecycleEvents.SERVER_STARTED.register(RedCraft::configureServer);
		ServerTickEvents.END_SERVER_TICK.register(BlockSync::applyHostOps);
	}

	/**
	 * The host drives the player and owns the world around it, so the integrated server must not
	 * fight it (rules from SkyCraft's mirror world).
	 */
	private static void configureServer(MinecraftServer server) {
		GameRules rules = server.getGameRules();
		// Host-driven moves can be large; don't let the server rubber-band them.
		rules.set(GameRules.PLAYER_MOVEMENT_CHECK, false, server);
		rules.set(GameRules.SPAWN_MOBS, false, server);
		rules.set(GameRules.SPAWN_MONSTERS, false, server);
		rules.set(GameRules.ADVANCE_WEATHER, false, server);
		rules.set(GameRules.KEEP_INVENTORY, true, server);
		RedLog.info("server configured (movement check off, no mob spawning)");
	}
}
