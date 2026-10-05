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
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import net.fabricmc.fabric.api.event.lifecycle.v1.ServerLifecycleEvents;
import net.fabricmc.fabric.api.event.lifecycle.v1.ServerTickEvents;
import net.fabricmc.fabric.api.networking.v1.ServerPlayConnectionEvents;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.util.HttpUtil;
import net.minecraft.world.entity.player.Abilities;
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

	private static CoopConfig config;
	private static TerrainService terrain;
	private static FriendTracker tracker;
	private static final Set<UUID> hovering = new HashSet<>();
	private static int lastGeneration = -1;
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
		ServerLifecycleEvents.SERVER_STARTED.register(server -> publishTried = false);
		ServerLifecycleEvents.SERVER_STOPPING.register(server -> {
			tracker.resetAll(); // §7.10: everyone is gone when the world closes
			hovering.clear();
			address = "";
			friendsOnline = 0;
		});
		ServerTickEvents.END_SERVER_TICK.register(CoopServer::tick);
		ServerPlayConnectionEvents.JOIN.register((handler, sender, server) -> server.execute(() -> onJoin(server, handler.player)));
		ServerPlayConnectionEvents.DISCONNECT.register((handler, server) -> hovering.remove(handler.player.getUUID()));
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

	public static boolean isOwner(MinecraftServer server, ServerPlayer player) {
		return server.isSingleplayerOwner(player.nameAndId());
	}

	private static void onJoin(MinecraftServer server, ServerPlayer player) {
		if (isOwner(server, player)) {
			// The puppet hovers while the host isn't moving it, instead of falling through unbuilt ground.
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
			LinkService link = LinkService.get();
			if (link.peerGeneration() != lastGeneration) {
				lastGeneration = link.peerGeneration();
				tracker.resync(); // a new host: announce every friend again (§7.10)
				lastInfo = null;
			}
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
				keepSafe(server, p);
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
		if (!built && hovering.add(id)) {
			if (!a.flying) {
				a.mayfly = true;
				a.flying = true;
				p.onUpdateAbilities();
			}
			CraftLog.limited("hover", 2000, p.getGameProfile().name() + " hovers until the ground at chunk " + p.chunkPosition() + " arrives");
		} else if (built && hovering.remove(id)) {
			p.gameMode.getGameModeForPlayer().updatePlayerAbilities(a); // back to what the game mode allows
			p.onUpdateAbilities();
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
