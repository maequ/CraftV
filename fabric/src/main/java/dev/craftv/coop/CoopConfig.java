package dev.craftv.coop;

import dev.craftv.CraftLog;
import dev.craftv.terrain.TerrainColumns;
import java.io.IOException;
import java.io.Reader;
import java.io.Writer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Locale;
import java.util.Properties;
import net.minecraft.world.level.GameType;

/**
 * Minecraft-side settings in {@code config/craftv.properties} (brief §13), written with defaults on the
 * first run. Account authentication is not a setting: it's always on, except with the dev-only system
 * property {@code -Dcraftv.devNoAuth=true}, which no normal script sets (brief §2.4).
 */
public final class CoopConfig {
	public static final int DEFAULT_PORT = 25565;

	public final boolean open;
	public final int port;
	public final GameType friendsGameMode;
	/** The host's own player (Sary): Survival shows the hearts and hunger bar in the passthrough (brief §8). */
	public final GameType ownerGameMode;
	/** Give the owner a starter hotbar when they join with an empty inventory. */
	public final boolean ownerKit;
	public final int terrainRadius;
	public final int terrainDepth;
	public final int buildingDepth;
	public final boolean devNoAuth;

	private CoopConfig(Properties p) {
		open = Boolean.parseBoolean(p.getProperty("friends.open", "true"));
		port = clamp(intValue(p, "friends.port", DEFAULT_PORT), 1024, 65535);
		friendsGameMode = gameMode(p.getProperty("friends.gameMode", "creative"));
		ownerGameMode = gameMode(p.getProperty("owner.gameMode", "survival"));
		ownerKit = Boolean.parseBoolean(p.getProperty("owner.kit", "true"));
		terrainRadius = clamp(intValue(p, "terrain.radius", 6), 1, 16);
		terrainDepth = clamp(intValue(p, "terrain.depth", TerrainColumns.DEFAULT_DEPTH), 1, 64);
		buildingDepth = clamp(intValue(p, "terrain.buildingDepth", TerrainColumns.DEFAULT_BUILDING_DEPTH), 1, 256);
		devNoAuth = Boolean.getBoolean("craftv.devNoAuth");
	}

	public static CoopConfig load(Path gameDir) {
		Path file = gameDir.resolve("config").resolve("craftv.properties");
		Properties p = new Properties();
		if (Files.exists(file)) {
			try (Reader r = Files.newBufferedReader(file, StandardCharsets.UTF_8)) {
				p.load(r);
			} catch (IOException e) {
				CraftLog.warn("can't read " + file + " (" + e + "); using defaults");
			}
		} else {
			writeDefaults(file);
		}
		CoopConfig c = new CoopConfig(p);
		CraftLog.info("config: friends " + (c.open ? "open on port " + c.port : "closed") + ", game mode " + c.friendsGameMode.getName() + "; you play "
			+ c.ownerGameMode.getName() + (c.ownerKit ? " with the starter kit" : "") + "; terrain radius "
			+ c.terrainRadius + " chunks, depth " + c.terrainDepth + (c.devNoAuth ? ", DEV: account authentication OFF" : ""));
		return c;
	}

	private static void writeDefaults(Path file) {
		try {
			Files.createDirectories(file.getParent());
			try (Writer w = Files.newBufferedWriter(file, StandardCharsets.UTF_8)) {
				w.write("""
					# CraftV (Minecraft side). Restart Minecraft after changing this file.
					# Let friends join this world (they need their own Minecraft Java 26.3 account).
					friends.open=true
					# The TCP port friends connect to. If it's busy, CraftV picks a free one and shows it.
					friends.port=25565
					# survival, creative or adventure
					friends.gameMode=creative
					# Your own game mode (survival shows hearts and hunger), and a starter hotbar when your inventory is empty.
					owner.gameMode=survival
					owner.kit=true
					# How far around every player (in 16-block chunks) to build the host game's ground.
					terrain.radius=6
					# How many blocks deep the ground is built under the surface, and under building tops.
					terrain.depth=8
					terrain.buildingDepth=40
					""");
			}
		} catch (IOException e) {
			CraftLog.warn("can't write " + file + " (" + e + ")");
		}
	}

	private static int intValue(Properties p, String key, int fallback) {
		try {
			return Integer.parseInt(p.getProperty(key, Integer.toString(fallback)).trim());
		} catch (NumberFormatException e) {
			CraftLog.warn("config: " + key + " isn't a number; using " + fallback);
			return fallback;
		}
	}

	private static int clamp(int v, int lo, int hi) {
		return Math.max(lo, Math.min(hi, v));
	}

	private static GameType gameMode(String name) {
		return switch (name.trim().toLowerCase(Locale.ROOT)) {
			case "survival" -> GameType.SURVIVAL;
			case "adventure" -> GameType.ADVENTURE;
			default -> GameType.CREATIVE;
		};
	}
}
