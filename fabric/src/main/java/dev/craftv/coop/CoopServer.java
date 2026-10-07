package dev.craftv.coop;

import static dev.craftv.link.Proto.*;

import dev.craftv.CraftLog;
import dev.craftv.LinkService;
import dev.craftv.link.Messages;
import dev.craftv.terrain.TerrainService;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import net.fabricmc.fabric.api.event.lifecycle.v1.ServerChunkEvents;
import net.fabricmc.fabric.api.event.lifecycle.v1.ServerLifecycleEvents;
import net.fabricmc.fabric.api.event.lifecycle.v1.ServerTickEvents;
import net.fabricmc.fabric.api.networking.v1.ServerPlayConnectionEvents;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.util.HttpUtil;
import net.minecraft.world.entity.player.Abilities;
import net.minecraft.world.entity.player.Inventory;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.Items;
import net.minecraft.world.level.levelgen.Heightmap;

/**
 * Co-op on the Minecraft side (brief §6.2). The hidden Minecraft's integrated server is the server friends
 * join in plain Minecraft. Everyone except its owner (the host's own player, the puppet) is a friend:
 * <ul>
 * <li>the world is opened to friends once the owner is in it ({@link #publishIfNeeded}, client thread);</li>
 * <li>friends are reported to the host every tick ({@link FriendTracker}, PROTOCOL.md §7.8-7.10);</li>
 * <li>friends spawn next to the owner and hover until the ground under them has been built;</li>
 * <li>how to join goes to the host as SESSION_INFO (§7.13).</li>
 * </ul>
 */
public final class CoopServer {
	private static final int SESSION_INFO_EVERY_TICKS = 20;
	private static final double SPAWN_OFFSET = 2.0; // friends appear this many blocks east of the owner
	private static final float FACE_WEST = 90.0F;
	private static final int UNDERGROUND_MARGIN = 2; // blocks below the built ground before a friend is lifted

	private static CoopConfig config;
	private static TerrainService terrain;
	private static FriendTracker tracker;
	private static final Map<UUID, Boolean> hovering = new HashMap<>(); // friend -> whether we lifted them
	private static int lastGeneration = -1;
	private static boolean wasConnected;
	private static Messages.SessionInfo lastInfo;
	private static volatile String address = "";
	private static volatile int friendsOnline;
	private static boolean publishTried;

	private CoopServer() {
	}

	public static void init(Path gameDir) {
		config = CoopConfig.load(gameDir);
		terrain = new TerrainService(config);
		tracker = new FriendTracker(LinkService.get()::send);
		GuestSync.init(config);
		GtaWorld.init();
		ServerLifecycleEvents.SERVER_STARTED.register(server -> publishTried = false);
		ServerLifecycleEvents.SERVER_STOPPED.register(server -> dev.craftv.terrain.TerrainIndex.clear()); // the next world has its own
		ServerLifecycleEvents.SERVER_STOPPING.register(server -> {
			tracker.resetAll(); // §7.10: everyone is gone when the world closes
			hovering.clear();
			GuestSync.clear();
			GtaWorld.clear();
			address = "";
			friendsOnline = 0;
		});
		ServerTickEvents.END_SERVER_TICK.register(CoopServer::tick);
		ServerChunkEvents.CHUNK_LOAD.register((level, chunk, generated) -> {
			if (level == TerrainService.mirror(level.getServer())) {
				// Load the terrain index before the first chunk is meshed: the owner's view hides terrain by it.
				dev.craftv.terrain.TerrainState.of(level);
				TerrainService.removeSpilledWater(level, chunk);
			}
		});
		ServerPlayConnectionEvents.JOIN.register((handler, sender, server) -> server.execute(() -> onJoin(server, handler.player)));
		ServerPlayConnectionEvents.DISCONNECT.register((handler, server) -> {
			hovering.remove(handler.player.getUUID());
			GuestSync.left(handler.player);
			GtaWorld.left(handler.player);
		});
	}

	/**
	 * Opens the world to friends (LAN, authentication on unless the dev switch is set). Called on the client
	 * thread once the owner is in the world, as vanilla's "Open to LAN" does.
	 */
	public static void publishIfNeeded(MinecraftServer server) {
		if (server == null || publishTried || !config.open || server.isPublished()) {
			return;
		}
		publishTried = true;
		int port = config.port;
		if (!HttpUtil.isPortAvailable(port)) {
			int other = HttpUtil.getAvailablePort();
			CraftLog.warn("port " + port + " is busy; friends will use port " + other + " instead");
			port = other;
		}
		server.setDefaultGameType(config.friendsGameMode);
		if (config.devNoAuth) {
			server.setUsesAuthentication(false);
			CraftLog.warn("DEV: Minecraft account authentication is OFF (-Dcraftv.devNoAuth). Never use this outside testing.");
		}
		if (server.publishServer(MinecraftServer.MultiplayerScope.LAN, false, port)) {
			address = localAddress() + ":" + port;
			CraftLog.info("friends can join: " + address + " (or pick the world under Multiplayer on the same network)");
		} else {
			CraftLog.warn("couldn't open the world to friends on port " + port);
		}
	}

	public static CoopConfig config() {
		return config;
	}

	public static boolean isOwner(MinecraftServer server, ServerPlayer player) {
		return server.isSingleplayerOwner(player.nameAndId());
	}

	private static void onJoin(MinecraftServer server, ServerPlayer player) {
		if (isOwner(server, player)) {
			if (player.gameMode() != config.ownerGameMode) {
				player.setGameMode(config.ownerGameMode);
			}
			if (config.ownerKit && player.getInventory().isEmpty()) {
				giveKit(player);
			} else if (config.ownerKit) {
				giveKitExtras(player); // the survival extras added after their kit
			}
			// The puppet hovers while the host isn't moving it, instead of falling through unbuilt ground (a
			// Survival owner can't fly; PlayerPuppet holds them in place instead).
			Abilities a = player.getAbilities();
			if (a.mayfly) {
				a.flying = true;
				player.onUpdateAbilities();
			}
			return;
		}
		player.setGameMode(config.friendsGameMode);
		ServerPlayer owner = owner(server);
		ServerLevel mirror = TerrainService.mirror(server);
		if (owner != null && owner.level() == mirror) {
			int x = (int) Math.floor(owner.getX() + SPAWN_OFFSET), z = (int) Math.floor(owner.getZ());
			int top = mirror.getHeight(Heightmap.Types.MOTION_BLOCKING, x, z);
			double y = top > mirror.getMinY() ? top : owner.getY();
			player.teleportTo(mirror, x + 0.5, y, z + 0.5, Set.of(), FACE_WEST, 0.0F, false);
		}
		CraftLog.info("friend joined: " + player.getGameProfile().name() + " (" + player.getUUID() + ")");
	}

	/** The owner's starter hotbar (brief §8: Survival with a kit). */
	static void giveKit(ServerPlayer player) {
		Inventory inv = player.getInventory();
		inv.setItem(0, new ItemStack(Items.DIAMOND_SWORD));
		inv.setItem(1, new ItemStack(Items.DIAMOND_PICKAXE));
		inv.setItem(2, new ItemStack(Items.BOW));
		inv.setItem(3, new ItemStack(Items.TNT, 16));
		inv.setItem(4, new ItemStack(Items.FLINT_AND_STEEL));
		inv.setItem(5, new ItemStack(Items.OAK_PLANKS, 64));
		inv.setItem(6, new ItemStack(Items.STONE_BRICKS, 64));
		inv.setItem(7, new ItemStack(Items.TORCH, 64));
		inv.setItem(8, new ItemStack(Items.COOKED_BEEF, 32));
		player.addTag(KIT_EXTRAS_TAG);
		addRest(player);
		CraftLog.info(player.getGameProfile().name() + ": gave the starter kit (sword, pickaxe, bow, TNT, flint and steel, planks, stone bricks, torches, steak, and more in the inventory)");
	}

	private static final String KIT_EXTRAS_TAG = "craftv_kit_v2";

	/** Players who got the first kit (2026-10-06) get what it lacked once: a bow, arrows, TNT and redstone. */
	static void giveKitExtras(ServerPlayer player) {
		if (player.entityTags().contains(KIT_EXTRAS_TAG)) {
			return;
		}
		player.addTag(KIT_EXTRAS_TAG);
		Inventory inv = player.getInventory();
		inv.add(new ItemStack(Items.BOW));
		inv.add(new ItemStack(Items.TNT, 16));
		inv.add(new ItemStack(Items.FLINT_AND_STEEL));
		addRest(player);
		CraftLog.info(player.getGameProfile().name() + ": gave the new kit extras (bow, arrows, TNT, flint and steel, redstone, crafting table)");
	}

	/** The rest of the kit, in the inventory: things to craft with, redstone that does something, arrows. */
	private static void addRest(ServerPlayer player) {
		Inventory inv = player.getInventory();
		inv.add(new ItemStack(Items.ARROW, 64));
		inv.add(new ItemStack(Items.DIAMOND_AXE));
		inv.add(new ItemStack(Items.DIAMOND_SHOVEL));
		inv.add(new ItemStack(Items.CRAFTING_TABLE));
		inv.add(new ItemStack(Items.FURNACE));
		inv.add(new ItemStack(Items.GLASS, 64));
		inv.add(new ItemStack(Items.REDSTONE, 64));
		inv.add(new ItemStack(Items.REDSTONE_TORCH, 16));
		inv.add(new ItemStack(Items.REDSTONE_LAMP, 16));
		inv.add(new ItemStack(Items.LEVER, 8));
		inv.add(new ItemStack(Items.STONE_BUTTON, 8));
		inv.add(new ItemStack(Items.LANTERN, 8));
		inv.add(new ItemStack(Items.SHIELD));
		inv.add(new ItemStack(Items.WATER_BUCKET));
		inv.add(new ItemStack(Items.COAL, 32));
		inv.add(new ItemStack(Items.OAK_LOG, 32));
	}

	private static ServerPlayer owner(MinecraftServer server) {
		for (ServerPlayer p : server.getPlayerList().getPlayers()) {
			if (isOwner(server, p)) {
				return p;
			}
		}
		return null;
	}

	private static void tick(MinecraftServer server) {
		try {
			terrain.tick(server);
			GuestSync.tick(server);
			GtaWorld.tick(server);
			LinkService link = LinkService.get();
			// A new host, or the link back from STALE (GTA pauses its scripts when it loses focus, and while the
			// link is stale every message for it is dropped, JOINs included): announce every friend again (§7.10).
			boolean connected = link.connected();
			if (link.peerGeneration() != lastGeneration || (connected && !wasConnected)) {
				lastGeneration = link.peerGeneration();
				tracker.resync();
				lastInfo = null;
			}
			wasConnected = connected;
			ServerLevel mirror = TerrainService.mirror(server);
			List<FriendTracker.Sample> present = new ArrayList<>();
			Map<Integer, Integer> gone = new HashMap<>();
			for (ServerPlayer p : server.getPlayerList().getPlayers()) {
				if (isOwner(server, p)) {
					continue;
				}
				if (p.level() != mirror) {
					gone.put(p.getId(), LEAVE_OTHER_DIMENSION);
					continue;
				}
				present.add(sample(server, p));
				if (!GuestSync.isGuest(p)) {
					keepSafe(server, p); // a guest's GTA holds them up, like the owner's
				}
			}
			tracker.tick(present, gone.isEmpty() ? Collections.emptyMap() : gone);
			friendsOnline = present.size();
			if (server.getTickCount() % SESSION_INFO_EVERY_TICKS == 0) {
				sendSessionInfo(server, present.size());
			}
		} catch (RuntimeException e) {
			CraftLog.error("co-op tick failed", e);
		}
	}

	private static FriendTracker.Sample sample(MinecraftServer server, ServerPlayer p) {
		int flags = 0;
		flags |= p.onGround() ? REMOTE_ON_GROUND : 0;
		flags |= p.isCrouching() ? REMOTE_CROUCHING : 0;
		flags |= p.isSprinting() ? REMOTE_SPRINTING : 0;
		flags |= p.isSwimming() ? REMOTE_SWIMMING : 0;
		flags |= p.isFallFlying() ? REMOTE_GLIDING : 0;
		flags |= p.getAbilities().flying ? REMOTE_FLYING : 0;
		flags |= p.isInWater() ? REMOTE_IN_WATER : 0;
		flags |= p.isSleeping() ? REMOTE_SLEEPING : 0;
		flags |= p.isPassenger() ? REMOTE_RIDING : 0;
		return new FriendTracker.Sample(p.getId(), p.getUUID(), p.getGameProfile().name(), p.getX(), p.getY(), p.getZ(), p.getYHeadRot(), p.getXRot(), p.yBodyRot,
			flags, p.isSwinging(), p.gameMode.getGameModeForPlayer().getId(), p.getHealth(), server.getTickCount());
	}

	/** A friend standing where the ground hasn't been built yet hovers instead of falling into the void. */
	private static void keepSafe(MinecraftServer server, ServerPlayer p) {
		boolean built = terrain.isBuilt(server, p.chunkPosition());
		UUID id = p.getUUID();
		Abilities a = p.getAbilities();
		if (!built && !hovering.containsKey(id)) {
			boolean forced = !a.flying;
			hovering.put(id, forced);
			if (forced) {
				a.mayfly = true;
				a.flying = true;
				p.onUpdateAbilities();
			}
			CraftLog.limited("hover", 2000, p.getGameProfile().name() + " hovers until the ground at chunk " + p.chunkPosition() + " arrives");
		} else if (built && hovering.containsKey(id)) {
			boolean forced = hovering.remove(id);
			p.gameMode.getGameModeForPlayer().updatePlayerAbilities(a); // back to what the game mode allows
			if (forced) {
				a.flying = false; // we lifted them: put them down on the new ground (creative keeps mayfly)
			}
			p.onUpdateAbilities();
			CraftLog.limited("hover-end", 2000, p.getGameProfile().name() + " has ground under them again");
		}
		if (built) {
			liftIfUnderground(p);
		}
	}

	/**
	 * Host ground is only a few blocks thick, with void under it. A friend below it (the ground arrived above
	 * them, or they dug through the bottom) is put back on the surface instead of falling forever.
	 */
	private static void liftIfUnderground(ServerPlayer p) {
		ServerLevel level = p.level();
		int x = (int) Math.floor(p.getX()), z = (int) Math.floor(p.getZ());
		int top = level.getHeight(Heightmap.Types.MOTION_BLOCKING, x, z); // the first free block above the ground
		if (top > level.getMinY() && p.getY() < top - config.terrainDepth - UNDERGROUND_MARGIN) {
			p.teleportTo(level, p.getX(), top, p.getZ(), Set.of(), p.getYRot(), p.getXRot(), false);
			p.resetFallDistance();
			CraftLog.info(p.getGameProfile().name() + " was below the ground at " + x + ", " + z + "; lifted to y " + top);
		}
	}

	private static void sendSessionInfo(MinecraftServer server, int friends) {
		boolean open = server.isPublished();
		int flags = (open ? SESSION_OPEN : 0) | (server.usesAuthentication() ? SESSION_AUTH : 0) | (server.getPlayerList().isUsingWhitelist() ? SESSION_WHITELIST : 0);
		Messages.SessionInfo info = new Messages.SessionInfo(flags, open ? server.getPort() : 0, friends, server.getMaxPlayers(), config.friendsGameMode.getId(),
			open ? address : "");
		if (!info.equals(lastInfo)) {
			lastInfo = info;
			LinkService.get().send(info);
		}
	}

	/** For the HUD: "2 friends @ 192.168.1.23:25565", or why nobody can join. */
	public static String hudLine() {
		if (config == null || !config.open) {
			return "friends: closed";
		}
		String a = address;
		return a.isEmpty() ? "friends: opening..." : "friends " + friendsOnline + " @ " + a;
	}

	/** The PC's LAN address friends on the same network type in: 192.168.x first, then 10.x, then 172.16-31.x. */
	static String localAddress() {
		String best = null;
		int bestRank = Integer.MAX_VALUE;
		try {
			for (NetworkInterface nif : Collections.list(NetworkInterface.getNetworkInterfaces())) {
				if (!nif.isUp() || nif.isLoopback() || nif.isVirtual()) {
					continue;
				}
				for (InetAddress a : Collections.list(nif.getInetAddresses())) {
					if (!(a instanceof Inet4Address) || !a.isSiteLocalAddress()) {
						continue;
					}
					String s = a.getHostAddress();
					int rank = s.startsWith("192.168.") ? 0 : s.startsWith("10.") ? 1 : 2;
					if (rank < bestRank) {
						best = s;
						bestRank = rank;
					}
				}
			}
		} catch (SocketException e) {
			CraftLog.limited("lanaddr", 60000, "can't list network interfaces: " + e);
		}
		return best != null ? best : "localhost";
	}
}
