package dev.craftv.terrain;

import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.level.chunk.LevelChunk;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.Block;
import net.minecraft.tags.FluidTags;
import net.minecraft.core.BlockPos;
import dev.craftv.link.Proto;
import dev.craftv.BlockSync;
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
	private static final long EMPTY_RETRY_MS = 60_000; // a patch with no ground at all is asked for again after this

	private final CoopConfig config;
	private final TerrainRequester requester = new TerrainRequester();
	private int lastGeneration = -1;
	private int nextRequestId;
	private long built, blocks, requested, empty, nextStatsMs;

	public TerrainService(CoopConfig config) {
		this.config = config;
	}

	/** The mirror world is the overworld of the CraftV save (MirrorWorld's preset). */
	private static final int SPILL_SCAN_HEIGHT = 12; // blocks above a column's ground where spilled water can sit
	private static final int SPILL_SCAN_DEPTH = 8; // and below it, in holes dug into the terrain
	private static int spillScans;

	/**
	 * Removes water that ran out of terrain lakes onto the land before terrain water stopped spreading (Sary's first
	 * passthrough run): flowing (non-source) water above a terrain column's ground that isn't that column's own water.
	 * Water friends place flows from a source and comes back from it. Server thread, on chunk load.
	 */
	public static void removeSpilledWater(ServerLevel level, LevelChunk chunk) {
		TerrainIndex.Columns c = TerrainIndex.get(chunk.getPos().x(), chunk.getPos().z());
		if (c == null) {
			return;
		}
		BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
		int removed = 0;
		for (int lz = 0; lz < 16; lz++) {
			for (int lx = 0; lx < 16; lx++) {
				short ground = c.groundY()[Messages.TerrainPatch.column(lx, lz)];
				if (ground == Proto.NO_GROUND) {
					continue;
				}
				for (int y = Math.max(ground - SPILL_SCAN_DEPTH, level.getMinY()); y <= ground + SPILL_SCAN_HEIGHT && y < level.getMaxY(); y++) {
					pos.set(chunk.getPos().getMinBlockX() + lx, y, chunk.getPos().getMinBlockZ() + lz);
					FluidState f = chunk.getFluidState(pos);
					if (!f.isEmpty() && !f.isSource() && f.is(FluidTags.WATER) && c.kindAt(lx, y, lz) != TerrainColumns.Kind.WATER) {
						BlockPos at = pos.immutable();
						BlockSync.silently(() -> level.setBlock(at, Blocks.AIR.defaultBlockState(), Block.UPDATE_CLIENTS | Block.UPDATE_KNOWN_SHAPE));
						removed++;
					}
				}
			}
		}
		if (++spillScans == 1) {
			CraftLog.info("terrain: checking loaded terrain chunks for spilled water");
		}
		if (removed > 0) {
			CraftLog.info("terrain: removed " + removed + " spilled water blocks in chunk " + chunk.getPos().x() + ", " + chunk.getPos().z());
		}
	}

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
			if (requested > 0 || built > 0 || empty > 0) {
				CraftLog.info("terrain: " + state.size() + " chunks built in this world (" + built + " this session, " + blocks + " blocks), " + requested
					+ " requests sent, " + empty + " came back empty, " + requester.inFlight() + " in flight");
			}
		}
	}

	private void buildArrived(ServerLevel level, TerrainState state, LinkService link) {
		long deadline = System.nanoTime() + BUILD_BUDGET_NS;
		Messages.TerrainPatch patch;
		while ((patch = link.pollTerrainPatch()) != null) {
			requester.answered(patch.chunkX(), patch.chunkZ());
			if (TerrainBuilder.noGround(patch)) {
				// Not remembered as built: the host may have had no collision there yet, or an older plugin's map
				// edge. Ask again later instead of leaving that chunk void for good.
				requester.snooze(patch.chunkX(), patch.chunkZ(), Win32.tickCount(), EMPTY_RETRY_MS);
				empty++;
				continue;
			}
			long key = TerrainRequester.key(patch.chunkX(), patch.chunkZ());
			if (!state.isBuilt(key)) {
				state.markBuilt(key, patch, config.terrainDepth, config.buildingDepth); // first, so the owner's view knows it
				try {
					blocks += TerrainBuilder.build(level, patch, config.terrainDepth, config.buildingDepth);
					built++;
				} catch (RuntimeException e) {
					CraftLog.error("failed to build terrain for chunk " + patch.chunkX() + ", " + patch.chunkZ(), e);
				}
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
