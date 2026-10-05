package dev.craftv.terrain;

import dev.craftv.BlockSync;
import dev.craftv.link.Messages;
import java.util.EnumMap;
import java.util.Map;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;

/**
 * Builds one TERRAIN_PATCH into the mirror world (PROTOCOL.md §7.12) on the server thread. The blocks
 * are sent to players but cause no neighbour updates, no placement side effects (water doesn't start
 * flowing, sand doesn't fall) and are never echoed back to the host as BLOCK_SET.
 */
public final class TerrainBuilder {
	private static final int FLAGS = Block.UPDATE_CLIENTS | Block.UPDATE_KNOWN_SHAPE | Block.UPDATE_SKIP_ON_PLACE;
	private static final Map<TerrainColumns.Kind, BlockState> STATES = new EnumMap<>(TerrainColumns.Kind.class);

	static {
		STATES.put(TerrainColumns.Kind.GRASS_BLOCK, Blocks.GRASS_BLOCK.defaultBlockState());
		STATES.put(TerrainColumns.Kind.DIRT, Blocks.DIRT.defaultBlockState());
		STATES.put(TerrainColumns.Kind.SAND, Blocks.SAND.defaultBlockState());
		STATES.put(TerrainColumns.Kind.SANDSTONE, Blocks.SANDSTONE.defaultBlockState());
		STATES.put(TerrainColumns.Kind.STONE, Blocks.STONE.defaultBlockState());
		STATES.put(TerrainColumns.Kind.GRAY_CONCRETE, Blocks.CONCRETE.gray().defaultBlockState());
		STATES.put(TerrainColumns.Kind.SMOOTH_STONE, Blocks.SMOOTH_STONE.defaultBlockState());
		STATES.put(TerrainColumns.Kind.GRAVEL, Blocks.GRAVEL.defaultBlockState());
		STATES.put(TerrainColumns.Kind.SNOW_BLOCK, Blocks.SNOW_BLOCK.defaultBlockState());
		STATES.put(TerrainColumns.Kind.OAK_PLANKS, Blocks.OAK_PLANKS.defaultBlockState());
		STATES.put(TerrainColumns.Kind.LIGHT_GRAY_CONCRETE, Blocks.CONCRETE.lightGray().defaultBlockState());
		STATES.put(TerrainColumns.Kind.STONE_BRICKS, Blocks.STONE_BRICKS.defaultBlockState());
		STATES.put(TerrainColumns.Kind.MUD, Blocks.MUD.defaultBlockState());
		STATES.put(TerrainColumns.Kind.WATER, Blocks.WATER.defaultBlockState());
	}

	private TerrainBuilder() {
	}

	/** Places every column of the patch. Returns the number of blocks placed. */
	public static int build(ServerLevel level, Messages.TerrainPatch patch, int depth, int buildingDepth) {
		BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
		int minY = level.getMinY(), maxY = level.getMaxY();
		int[] placed = { 0 };
		BlockSync.silently(() -> {
			for (int lz = 0; lz < 16; lz++) {
				for (int lx = 0; lx < 16; lx++) {
					int column = Messages.TerrainPatch.column(lx, lz);
					int x = patch.chunkX() * 16 + lx, z = patch.chunkZ() * 16 + lz;
					placed[0] += TerrainColumns.build(patch.groundY()[column], patch.waterY()[column], patch.materialAt(column), depth, buildingDepth, minY, maxY,
						(y, kind) -> level.setBlock(pos.set(x, y, z), STATES.get(kind), FLAGS));
				}
			}
		});
		return placed[0];
	}
}
