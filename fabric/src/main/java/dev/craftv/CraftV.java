package dev.craftv;

import dev.craftv.coop.CoopServer;
import dev.craftv.link.Proto;
import net.fabricmc.api.ModInitializer;
import net.fabricmc.fabric.api.event.lifecycle.v1.ServerLifecycleEvents;
import net.fabricmc.fabric.api.event.lifecycle.v1.ServerTickEvents;
import net.fabricmc.loader.api.FabricLoader;
import net.minecraft.core.Holder;
import net.minecraft.core.registries.Registries;
import net.minecraft.server.MinecraftServer;
import net.minecraft.world.clock.ClockTimeMarkers;
import net.minecraft.world.clock.WorldClock;
import net.minecraft.world.clock.WorldClocks;
import net.minecraft.world.level.gamerules.GameRules;

/**
 * CraftV: your friends in plain Minecraft, inside GTA V story mode (docs/BRIEF.md). This is the Minecraft
 * half: the hidden Minecraft hosts the world friends join and talks to the GTA V plugin over shared memory.
 * Architecture adapted from SkyCraft by chasmlol (MIT).
 */
public final class CraftV implements ModInitializer {
	public static final String MOD_ID = "craftv";

	public static String version() {
		return FabricLoader.getInstance().getModContainer(MOD_ID).map(c -> c.getMetadata().getVersion().getFriendlyString()).orElse("dev");
	}

	@Override
	public void onInitialize() {
		CraftLog.open(FabricLoader.getInstance().getGameDir());
		CraftLog.info("CraftV " + version() + " loading (protocol " + Proto.VERSION_MAJOR + "." + Proto.VERSION_MINOR + ")");
		ServerLifecycleEvents.SERVER_STARTED.register(CraftV::configureServer);
		ServerTickEvents.END_SERVER_TICK.register(BlockSync::applyHostOps);
		dev.craftv.net.CraftNet.register();
		CoopServer.init(FabricLoader.getInstance().getGameDir());
	}

	/**
	 * The host drives the owner's player and owns the world around it, so the integrated server must not
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
		// Always midday, so the host game's ground is easy to see (following GTA's clock is for later).
		rules.set(GameRules.ADVANCE_TIME, false, server);
		try {
			Holder<WorldClock> clock = server.registryAccess().lookupOrThrow(Registries.WORLD_CLOCK).getOrThrow(WorldClocks.OVERWORLD);
			server.clockManager().moveToTimeMarker(clock, ClockTimeMarkers.NOON);
		} catch (RuntimeException e) {
			CraftLog.error("couldn't set the time to noon", e);
		}
		CraftLog.info("server configured (movement check off, no mob spawning, always noon)");
	}
}
