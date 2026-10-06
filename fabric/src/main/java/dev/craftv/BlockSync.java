package dev.craftv;

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

	/** From LevelChunkMixin, after a chunk's block actually changed (server side only). */
	public static void onBlockChanged(BlockPos pos, BlockState newState) {
		LinkService link = LinkService.get();
		if (!link.connected() || SILENT.get() != null) {
			return; // Phase 4 will resync on (re)connect; see KNOWN_LIMITATIONS.md
		}
		int[] applying = APPLYING.get();
		int flags = applying != null ? BLOCK_SET_ECHO : 0;
		int requestId = applying != null ? applying[0] : 0;
		link.send(new Messages.BlockSet(pos.getX(), pos.getY(), pos.getZ(), Block.getId(newState), flags, requestId));
		if (applying == null) {
			// KNOWN_LIMITATIONS: block changes arrived with no friends online; this names them (water? falling blocks?)
			CraftLog.limited("worldchange", 2000, "block change sent to the host: " + pos.toShortString() + " -> " + newState);
		}
	}

	/** Server tick: apply what the host asked for. */
	public static void applyHostOps(MinecraftServer server) {
		LinkService link = LinkService.get();
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
