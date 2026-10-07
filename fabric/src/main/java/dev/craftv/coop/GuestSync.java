package dev.craftv.coop;

import dev.craftv.CraftLog;
import dev.craftv.link.Messages;
import dev.craftv.net.CraftNet;
import dev.craftv.terrain.TerrainIndex;
import dev.craftv.terrain.TerrainRequester;
import it.unimi.dsi.fastutil.longs.LongOpenHashSet;
import java.util.Map;
import java.util.Queue;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentLinkedQueue;
import net.fabricmc.fabric.api.networking.v1.ServerPlayNetworking;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.level.ChunkPos;

/**
 * Guests: friends who play through their own GTA (DECISIONS D-030), whose CraftV Minecraft said so with
 * {@link CraftNet.GtaHello}. The server treats them like the owner (Survival with the kit, no in-wall, fall or
 * drowning damage, never rubber-banded), builds the ground their GTA scans, and sends them every built terrain
 * chunk near them so their view leaves the ground to their GTA, as the owner's does. Server thread.
 */
public final class GuestSync {
	private static final int COLUMNS_PER_TICK = 16;
	/** Chunks around a guest whose terrain columns they get: past their render distance (at most 10). */
	private static final int SEND_RADIUS = 12;
	/** A guest's patch is only accepted this close to them (in chunks): their GTA only scans around its player. */
	private static final int PATCH_RADIUS = 16;
	private static final int PATCH_INBOX_LIMIT = 4_096;
	private static final int SENT_LIMIT = 50_000;
	private static final double MAX_COORD = 30_000_000.0;

	/** Guest -> the terrain chunks already sent to them. */
	private static final Map<UUID, LongOpenHashSet> SENT = new ConcurrentHashMap<>();
	private static final Queue<Messages.TerrainPatch> PATCHES = new ConcurrentLinkedQueue<>();

	private GuestSync() {
	}

	static void init(CoopConfig config) {
		ServerPlayNetworking.registerGlobalReceiver(CraftNet.GtaHello.TYPE, (p, ctx) -> hello(config, ctx.server(), ctx.player(), p.driving()));
		ServerPlayNetworking.registerGlobalReceiver(CraftNet.Snap.TYPE, (p, ctx) -> snap(ctx.player(), p));
		ServerPlayNetworking.registerGlobalReceiver(CraftNet.Patch.TYPE, (p, ctx) -> patch(ctx.player(), p));
		// A rebuilt chunk goes to every guest again (put runs on the server thread, like tick).
		TerrainIndex.addOnPut((key, columns) -> SENT.values().forEach(sent -> sent.remove((long) key)));
	}

	/** The owner, or a guest: a player whose body is a GTA player. */
	public static boolean isGtaPlayer(MinecraftServer server, ServerPlayer player) {
		return isGuest(player) || CoopServer.isOwner(server, player);
	}

	public static boolean isGuest(ServerPlayer player) {
		return SENT.containsKey(player.getUUID());
	}

	private static void hello(CoopConfig config, MinecraftServer server, ServerPlayer player, boolean driving) {
		if (CoopServer.isOwner(server, player)) {
			return; // the owner's GTA talks to this Minecraft directly
		}
		String name = player.getGameProfile().name();
		if (!driving) {
			if (SENT.remove(player.getUUID()) != null) {
				CraftLog.info("guest: " + name + "'s GTA stopped; they're a plain friend again until it's back");
			}
			return;
		}
		if (SENT.putIfAbsent(player.getUUID(), new LongOpenHashSet()) != null) {
			return;
		}
		if (player.gameMode() != config.ownerGameMode) {
			player.setGameMode(config.ownerGameMode);
		}
		if (config.ownerKit && player.getInventory().isEmpty()) {
			CoopServer.giveKit(player);
		} else if (config.ownerKit) {
			CoopServer.giveKitExtras(player);
		}
		CraftLog.info("guest: " + name + " plays through their own GTA (" + config.ownerGameMode.getName() + ")");
	}

	private static void snap(ServerPlayer player, CraftNet.Snap s) {
		if (!isGuest(player) || !Double.isFinite(s.x()) || !Double.isFinite(s.y()) || !Double.isFinite(s.z()) || Math.abs(s.x()) > MAX_COORD
			|| Math.abs(s.z()) > MAX_COORD || !Float.isFinite(s.yaw()) || !Float.isFinite(s.pitch())) {
			return;
		}
		player.teleportTo(player.level(), s.x(), s.y(), s.z(), Set.of(), s.yaw(), s.pitch(), false);
		player.resetFallDistance();
		CraftLog.limited("guest-snap", 2000, String.format("guest: %s jumped to (%.1f, %.1f, %.1f)", player.getGameProfile().name(), s.x(), s.y(), s.z()));
	}

	private static void patch(ServerPlayer player, CraftNet.Patch p) {
		Messages.TerrainPatch patch = isGuest(player) ? p.patch() : null;
		if (patch == null) {
			return;
		}
		ChunkPos at = player.chunkPosition();
		if (Math.abs(patch.chunkX() - at.x()) > PATCH_RADIUS || Math.abs(patch.chunkZ() - at.z()) > PATCH_RADIUS || PATCHES.size() >= PATCH_INBOX_LIMIT) {
			return;
		}
		PATCHES.add(patch);
	}

	/** The next ground patch a guest's GTA scanned, or null (TerrainService builds it like the owner's). */
	public static Messages.TerrainPatch pollPatch() {
		return PATCHES.poll();
	}

	static void left(ServerPlayer player) {
		SENT.remove(player.getUUID());
	}

	static void clear() {
		SENT.clear();
		PATCHES.clear();
	}

	/** Every server tick: the built terrain near each guest that they don't have yet, nearest first. */
	static void tick(MinecraftServer server) {
		for (Map.Entry<UUID, LongOpenHashSet> e : SENT.entrySet()) {
			ServerPlayer player = server.getPlayerList().getPlayer(e.getKey());
			if (player == null || !ServerPlayNetworking.canSend(player, CraftNet.Columns.TYPE)) {
				continue;
			}
			LongOpenHashSet sent = e.getValue();
			if (sent.size() > SENT_LIMIT) {
				sent.clear(); // a long trip: start over (re-sending is harmless)
			}
			ChunkPos c = player.chunkPosition();
			int budget = COLUMNS_PER_TICK;
			for (int r = 0; r <= SEND_RADIUS && budget > 0; r++) {
				for (int dz = -r; dz <= r && budget > 0; dz++) {
					for (int dx = -r; dx <= r && budget > 0; dx++) {
						if (Math.max(Math.abs(dx), Math.abs(dz)) != r) {
							continue; // ring r only
						}
						int cx = c.x() + dx, cz = c.z() + dz;
						long key = TerrainRequester.key(cx, cz);
						if (sent.contains(key)) {
							continue;
						}
						TerrainIndex.Columns columns = TerrainIndex.get(cx, cz);
						if (columns == null) {
							continue;
						}
						ServerPlayNetworking.send(player, new CraftNet.Columns(cx, cz, columns.encode()));
						sent.add(key);
						budget--;
					}
				}
			}
		}
	}
}
