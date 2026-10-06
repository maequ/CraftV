package dev.craftv;

import dev.craftv.terrain.TerrainIndex;
import net.minecraft.world.level.chunk.LevelChunkSection;
import net.minecraft.world.level.chunk.LevelChunk;
import net.minecraft.world.level.Level;
import static dev.craftv.link.Proto.*;

import dev.craftv.link.Messages;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.item.BlockItem;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;

/**
 * Minecraft is the authority for blocks (PROTOCOL.md §7.4, §7.5):
 * <ul>
 * <li>every block change in a server world goes to the host as an authoritative BLOCK_SET
 * ({@code LevelChunkMixin} calls {@link #onBlockChanged});</li>
 * <li>block messages from the host are applied on the server thread ({@link #applyHostOps}),
 * and the resulting change is echoed back flagged ECHO with the request id.</li>
 * </ul>
 */
public final class BlockSync {
	private static final int MAX_HOST_OPS_PER_TICK = 256;
	private static final int UPDATE_FLAGS = Block.UPDATE_ALL;

	// While applying a host op on the server thread: the flags/request id for the echo.
	private static final ThreadLocal<int[]> APPLYING = new ThreadLocal<>();
	// While building host terrain: those blocks came from the host, so they are not reported back (§7.12).
	private static final ThreadLocal<Boolean> SILENT = new ThreadLocal<>();

	private BlockSync() {
	}

	/**
	 * Whether a block should be solid in the host game (PROTOCOL.md §7.19 SOLID): a full collision cube that isn't
	 * CraftV's terrain and stands above the terrain's surface (inside the ground the host's own ground is solid).
	 */
	public static boolean solidForHost(Level level, BlockPos pos, BlockState state) {
		if (state.isAir() || !state.isCollisionShapeFullBlock(level, pos)) {
			return false;
		}
		if (TerrainIndex.isTerrain(pos.getX(), pos.getY(), pos.getZ(), state)) {
			return false;
		}
		TerrainIndex.Columns c = TerrainIndex.get(pos.getX() >> 4, pos.getZ() >> 4);
		if (c == null) {
			return true;
		}
		short ground = c.groundY()[Messages.TerrainPatch.column(pos.getX() & 15, pos.getZ() & 15)];
		return ground == NO_GROUND || pos.getY() > ground;
	}

	/** From LevelChunkMixin, after a chunk's block actually changed (server side only). */
	public static void onBlockChanged(Level level, BlockPos pos, BlockState newState) {
		LinkService link = LinkService.get();
		if (!link.connected() || SILENT.get() != null) {
			return; // the host asks for what it missed with BLOCK_REGION_REQUEST (§7.19)
		}
		int[] applying = APPLYING.get();
		int flags = (applying != null ? BLOCK_SET_ECHO : 0) | (solidForHost(level, pos, newState) ? BLOCK_SET_SOLID : 0);
		int requestId = applying != null ? applying[0] : 0;
		link.send(new Messages.BlockSet(pos.getX(), pos.getY(), pos.getZ(), Block.getId(newState), flags, requestId));
		if (applying == null) {
			// KNOWN_LIMITATIONS: block changes arrived with no friends online; this names them (water? falling blocks?)
			CraftLog.limited("worldchange", 2000, "block change sent to the host: " + pos.toShortString() + " -> " + newState);
		}
	}

	private static final int MAX_REGIONS_PER_TICK = 2;

	/**
	 * Answers the host's BLOCK_REGION_REQUESTs (PROTOCOL.md §7.19): every block of that chunk column that should be
	 * solid in the host game, as BLOCK_SET SOLID|REGION. Server thread, a couple of chunks per tick.
	 */
	private static void answerRegions(MinecraftServer server, LinkService link) {
		ServerLevel level = dev.craftv.terrain.TerrainService.mirror(server);
		for (int n = 0; n < MAX_REGIONS_PER_TICK; n++) {
			Messages.BlockRegionRequest r = link.pollRegionRequest();
			if (r == null) {
				return;
			}
			LevelChunk chunk = level.getChunk(r.chunkX(), r.chunkZ());
			LevelChunkSection[] sections = chunk.getSections();
			BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
			int sent = 0;
			for (int si = 0; si < sections.length; si++) {
				LevelChunkSection section = sections[si];
				if (section.hasOnlyAir()) {
					continue;
				}
				int baseY = level.getSectionYFromSectionIndex(si) << 4;
				for (int y = 0; y < 16; y++) {
					for (int z = 0; z < 16; z++) {
						for (int x = 0; x < 16; x++) {
							BlockState state = section.getBlockState(x, y, z);
							if (state.isAir()) {
								continue;
							}
							pos.set(r.chunkX() * 16 + x, baseY + y, r.chunkZ() * 16 + z);
							if (solidForHost(level, pos, state)) {
								link.send(new Messages.BlockSet(pos.getX(), pos.getY(), pos.getZ(), Block.getId(state), BLOCK_SET_SOLID | BLOCK_SET_REGION, r.requestId()));
								sent++;
							}
						}
					}
				}
			}
			CraftLog.limited("region", 2000, "blocks: chunk " + r.chunkX() + ", " + r.chunkZ() + " has " + sent + " solid blocks for the host");
		}
	}

	/** Server tick: apply what the host asked for. */
	public static void applyHostOps(MinecraftServer server) {
		LinkService link = LinkService.get();
		answerRegions(server, link);
		for (int i = 0; i < MAX_HOST_OPS_PER_TICK; i++) {
			Messages.Payload op = link.pollHostBlockOp();
			if (op == null) {
				return;
			}
			try {
				apply(server, op);
			} catch (RuntimeException e) {
				CraftLog.error("failed to apply host block op " + op, e);
			}
		}
	}

	private static void apply(MinecraftServer server, Messages.Payload op) {
		ServerPlayer player = server.getPlayerList().getPlayers().isEmpty() ? null : server.getPlayerList().getPlayers().get(0);
		ServerLevel level = player != null ? (ServerLevel) player.level() : server.overworld();
		switch (op) {
			case Messages.BlockSet m -> {
				BlockPos pos = new BlockPos(m.x(), m.y(), m.z());
				BlockState state = Block.BLOCK_STATE_REGISTRY.byId(m.blockId());
				if (state == null || !level.isInWorldBounds(pos)) {
					CraftLog.limited("hostset", 2000, "host BLOCK_SET rejected: " + m + (state == null ? " (unknown block id)" : " (outside the world)"));
					return;
				}
				withEcho(m.requestId(), () -> level.setBlock(pos, state, UPDATE_FLAGS));
				CraftLog.limited("hostsetok", 1000, "host BLOCK_SET applied: " + pos.toShortString() + " = " + state);
			}
			case Messages.BlockBreakRequest m -> {
				BlockPos pos = new BlockPos(m.x(), m.y(), m.z());
				if (player == null || !level.isInWorldBounds(pos)) {
					return;
				}
				// The player's own break path: game mode, tools, drops (Phase 4 refines this).
				withEcho(m.requestId(), () -> player.gameMode.destroyBlock(pos));
				CraftLog.limited("hostbreak", 1000, "host BLOCK_BREAK_REQUEST #" + m.requestId() + " at " + pos.toShortString());
			}
			case Messages.BlockPlaceRequest m -> {
				BlockPos clicked = new BlockPos(m.x(), m.y(), m.z());
				if (!level.isInWorldBounds(clicked)) {
					return;
				}
				BlockPos target = level.getBlockState(clicked).canBeReplaced() || m.face() > FACE_MAX ? clicked
					: clicked.relative(Direction.from3DDataValue(m.face()));
				BlockState state = placeState(player, m.blockId());
				// ASSUMPTION (Phase 1 simplification): places without consuming inventory or
				// running the item's placement logic. Phase 4 routes this through the real use-item path.
				if (state == null || !level.isInWorldBounds(target) || !level.getBlockState(target).canBeReplaced()) {
					CraftLog.limited("hostplace", 2000, "host BLOCK_PLACE_REQUEST #" + m.requestId() + " not placeable at " + target.toShortString());
					return;
				}
				withEcho(m.requestId(), () -> level.setBlock(target, state, UPDATE_FLAGS));
				CraftLog.limited("hostplaceok", 1000, "host BLOCK_PLACE_REQUEST #" + m.requestId() + " placed " + state + " at " + target.toShortString());
			}
			default -> {
			}
		}
	}

	private static BlockState placeState(ServerPlayer player, int blockId) {
		if (blockId != 0) {
			return Block.BLOCK_STATE_REGISTRY.byId(blockId);
		}
		if (player != null && player.getMainHandItem().getItem() instanceof BlockItem item) {
			return item.getBlock().defaultBlockState();
		}
		return null;
	}

	/** Runs {@code action} (on the server thread) without reporting the block changes it makes. */
	public static void silently(Runnable action) {
		SILENT.set(Boolean.TRUE);
		try {
			action.run();
		} finally {
			SILENT.remove();
		}
	}

	private static void withEcho(int requestId, Runnable action) {
		APPLYING.set(new int[] { requestId });
		try {
			action.run();
		} finally {
			APPLYING.remove();
		}
	}
}
