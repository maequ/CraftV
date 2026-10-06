package dev.craftv.client;

import dev.craftv.BlockSync;
import dev.craftv.CraftLog;
import dev.craftv.LinkService;
import dev.craftv.link.Messages;
import dev.craftv.link.Win32;
import dev.craftv.net.CraftNet;
import dev.craftv.terrain.TerrainBuilder;
import dev.craftv.terrain.TerrainIndex;
import dev.craftv.terrain.TerrainRequester;
import java.util.List;
import net.fabricmc.fabric.api.client.networking.v1.ClientPlayConnectionEvents;
import net.fabricmc.fabric.api.client.networking.v1.ClientPlayNetworking;
import net.minecraft.client.Minecraft;
import net.minecraft.world.level.ChunkPos;

/**
 * A guest (DECISIONS D-030): a friend playing through their own GTA, whose CraftV Minecraft joined the owner's world
 * instead of hosting one. Their GTA drives their player exactly like the owner's (PlayerPuppet, the passthrough); this
 * adds what the owner's integrated server would otherwise do for them: tell the server this player is GTA-driven,
 * send it the ground their GTA scans, and answer their GTA's solid-block questions from the world as this client sees it.
 */
public final class GuestClient {
	/** Chunks around the guest whose ground their GTA is asked for (the server's default terrain radius). */
	private static final int TERRAIN_RADIUS = 6;
	private static final int REQUEST_EVERY_TICKS = 10;
	private static final int PATCHES_PER_TICK = 32;
	private static final long EMPTY_RETRY_MS = 60_000;
	private static final long BUILD_WAIT_MS = 10_000;

	private static final TerrainRequester requester = new TerrainRequester();
	private static boolean guest;
	private static boolean announced;
	private static boolean warnedNoCraftV;
	private static int lastGeneration = -1;
	private static int nextRequestId;
	private static int ticks;

	private GuestClient() {
	}

	public static void init() {
		ClientPlayNetworking.registerGlobalReceiver(CraftNet.Columns.TYPE, (p, ctx) -> TerrainIndex.putFromServer(p.chunkX(), p.chunkZ(), p.bytes()));
		ClientPlayConnectionEvents.DISCONNECT.register((handler, minecraft) -> minecraft.execute(GuestClient::left));
	}

	/** In someone else's world (no integrated server here). */
	public static boolean isGuest(Minecraft minecraft) {
		return minecraft.player != null && minecraft.level != null && minecraft.getSingleplayerServer() == null;
	}

	private static void left() {
		if (guest) {
			guest = false;
			BlockSync.setClientAuthority(false);
			TerrainIndex.clear(); // the next world has its own ground
			requester.reset();
			CraftLog.info("guest: left the owner's world");
		}
		announced = false;
		warnedNoCraftV = false;
	}

	/** End of every client tick. */
	public static void tick(Minecraft minecraft) {
		if (!isGuest(minecraft)) {
			if (guest) {
				left();
			}
			return;
		}
		if (!guest) {
			guest = true;
			CraftLog.info("guest: in someone else's world; this Minecraft follows its own GTA there");
		}
		BlockSync.setClientAuthority(true);
		LinkService link = LinkService.get();
		boolean driving = link.connected();
		boolean serverHasCraftV = ClientPlayNetworking.canSend(CraftNet.GtaHello.TYPE);
		if (!serverHasCraftV) {
			if (!warnedNoCraftV) {
				warnedNoCraftV = true;
				CraftLog.warn("guest: this server doesn't run CraftV; the GTA view works, but nobody builds the ground here");
			}
		} else if (driving != announced) {
			announced = driving;
			ClientPlayNetworking.send(new CraftNet.GtaHello(driving));
			CraftLog.info("guest: told the server " + (driving ? "our GTA drives this player" : "our GTA stopped"));
		}
		while (link.pollHostBlockOp() != null) {
			// the owner's server is the authority here; this player's own clicks reach it through Minecraft
		}
		forwardPatches(link, serverHasCraftV);
		if (driving && serverHasCraftV && ++ticks % REQUEST_EVERY_TICKS == 0) {
			requestGround(minecraft, link);
		}
		BlockSync.answerRegionsAsGuest(minecraft.level);
	}

	private static void forwardPatches(LinkService link, boolean serverHasCraftV) {
		Messages.TerrainPatch patch;
		for (int n = 0; n < PATCHES_PER_TICK && (patch = link.pollTerrainPatch()) != null; n++) {
			// Not asked again while the server builds it and sends the columns back (or ever, unless it had no ground).
			boolean empty = TerrainBuilder.noGround(patch);
			requester.snooze(patch.chunkX(), patch.chunkZ(), Win32.tickCount(), empty ? EMPTY_RETRY_MS : BUILD_WAIT_MS);
			if (!empty && serverHasCraftV) {
				ClientPlayNetworking.send(CraftNet.Patch.of(patch));
			}
		}
	}

	/** Asks this guest's GTA for the unbuilt chunks around them; the server builds them and sends the columns back. */
	private static void requestGround(Minecraft minecraft, LinkService link) {
		if (link.peerGeneration() != lastGeneration) {
			lastGeneration = link.peerGeneration();
			requester.reset(); // a new host forgot what we asked
		}
		ChunkPos c = minecraft.player.chunkPosition();
		for (TerrainRequester.Request r : requester.next(List.of(new TerrainRequester.Center(c.x(), c.z())), TERRAIN_RADIUS, TerrainIndex::has, Win32.tickCount())) {
			link.send(new Messages.TerrainRequest(r.chunkX(), r.chunkZ(), ++nextRequestId == 0 ? ++nextRequestId : nextRequestId, r.distance()));
		}
	}
}
