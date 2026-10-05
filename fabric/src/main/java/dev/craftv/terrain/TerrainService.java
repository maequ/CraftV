package dev.craftv.terrain;

import dev.craftv.CraftLog;
import dev.craftv.LinkService;
import dev.craftv.coop.CoopConfig;
import dev.craftv.link.Messages;
import dev.craftv.link.Win32;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.level.ChunkPos;

/**
 * The mirror world's ground comes from the host (brief §6.2): every half second this asks for the chunks
 * around every player that aren't built yet (TERRAIN_REQUEST), and every tick it builds the patches that
 * arrived (TERRAIN_PATCH), within a time budget so the server keeps its 20 ticks a second.
 */
public final class TerrainService {
	private static final int REQUEST_EVERY_TICKS = 10;
	private static final long BUILD_BUDGET_NS = 10_000_000L; // per tick; at least one patch is always built
	private static final long STATS_LOG_MS = 30_000;

	private final CoopConfig config;
	private final TerrainRequester requester = new TerrainRequester();
	private int lastGeneration = -1;
	private int nextRequestId;
	private long built, blocks, requested, nextStatsMs;

	public TerrainService(CoopConfig config) {
		this.config = config;
	}

	/** The mirror world is the overworld of the CraftV save (MirrorWorld's preset). */
	public static ServerLevel mirror(MinecraftServer server) {
		return server.overworld();
	}

	public boolean isBuilt(MinecraftServer server, ChunkPos pos) {
		return TerrainState.of(mirror(server)).isBuilt(TerrainRequester.key(pos.x(), pos.z()));
	}

	public void tick(MinecraftServer server) {
		LinkService link = LinkService.get();
		ServerLevel level = mirror(server);
		TerrainState state = TerrainState.of(level);
		if (link.peerGeneration() != lastGeneration) {
			lastGeneration = link.peerGeneration();
			requester.reset(); // a new host forgot what we asked
		}
		buildArrived(level, state, link);
		if (link.connected() && server.getTickCount() % REQUEST_EVERY_TICKS == 0) {
			request(level, state, link);
		}
		long now = Win32.tickCount();
		if (now >= nextStatsMs) {
			nextStatsMs = now + STATS_LOG_MS;
			if (requested > 0 || built > 0) {
				CraftLog.info("terrain: " + state.size() + " chunks built in this world (" + built + " this session, " + blocks + " blocks), " + requested
					+ " requests sent, " + requester.inFlight() + " in flight");
			}
		}
	}

	private void buildArrived(ServerLevel level, TerrainState state, LinkService link) {
		long deadline = System.nanoTime() + BUILD_BUDGET_NS;
		Messages.TerrainPatch patch;
		while ((patch = link.pollTerrainPatch()) != null) {
			requester.answered(patch.chunkX(), patch.chunkZ());
			long key = TerrainRequester.key(patch.chunkX(), patch.chunkZ());
			if (!state.isBuilt(key)) {
				try {
					blocks += TerrainBuilder.build(level, patch, config.terrainDepth, config.buildingDepth);
					built++;
				} catch (RuntimeException e) {
					CraftLog.error("failed to build terrain for chunk " + patch.chunkX() + ", " + patch.chunkZ(), e);
				}
				state.markBuilt(key);
			}
			if (System.nanoTime() >= deadline) {
				return; // the rest waits for the next tick
			}
		}
	}

	private void request(ServerLevel level, TerrainState state, LinkService link) {
		List<TerrainRequester.Center> centers = new ArrayList<>();
		for (ServerPlayer player : level.players()) {
			ChunkPos c = player.chunkPosition();
			centers.add(new TerrainRequester.Center(c.x(), c.z()));
		}
		for (TerrainRequester.Request r : requester.next(centers, config.terrainRadius, state::isBuilt, Win32.tickCount())) {
			link.send(new Messages.TerrainRequest(r.chunkX(), r.chunkZ(), ++nextRequestId == 0 ? ++nextRequestId : nextRequestId, r.distance()));
			requested++;
		}
	}
}
