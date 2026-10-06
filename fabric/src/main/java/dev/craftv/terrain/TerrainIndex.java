package dev.craftv.terrain;

import dev.craftv.link.Messages;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.concurrent.ConcurrentHashMap;
import net.minecraft.world.level.block.state.BlockState;

/**
 * Every terrain column built in the current world, so the owner's passthrough view can leave GTA's own ground
 * to GTA (brief §8): {@link #isTerrain} says whether a block is exactly what the terrain builder put there.
 * Friends' clients are unaffected; they get the real blocks. Written on the server thread, read by the client's
 * chunk meshing threads, so entries are immutable and the map is concurrent.
 */
public final class TerrainIndex {
	/** One built chunk: the patch's columns and the depths it was built with. Arrays are never changed after creation. */
	public record Columns(short[] groundY, short[] waterY, byte[] material, int depth, int buildingDepth) {
		private static final int GROUND_OFF = 0, WATER_OFF = 512, MATERIAL_OFF = 1024, DEPTH_OFF = 1280, BUILDING_DEPTH_OFF = 1281;
		static final int BYTES = 1282;

		static Columns of(Messages.TerrainPatch patch, int depth, int buildingDepth) {
			byte[] material = new byte[256];
			for (int i = 0; i < 256; i++) {
				material[i] = (byte) patch.materialAt(i);
			}
			return new Columns(patch.groundY().clone(), patch.waterY().clone(), material, depth, buildingDepth);
		}

		/** The saved form (TerrainState): little-endian heights, materials, then the two depths. */
		byte[] encode() {
			ByteBuffer b = ByteBuffer.allocate(BYTES).order(ByteOrder.LITTLE_ENDIAN);
			for (int i = 0; i < 256; i++) {
				b.putShort(GROUND_OFF + 2 * i, groundY[i]);
				b.putShort(WATER_OFF + 2 * i, waterY[i]);
			}
			b.put(MATERIAL_OFF, material);
			b.put(DEPTH_OFF, (byte) depth);
			b.put(BUILDING_DEPTH_OFF, (byte) buildingDepth);
			return b.array();
		}

		/** Null if the bytes aren't a saved column set. */
		static Columns decode(byte[] bytes) {
			if (bytes == null || bytes.length != BYTES) {
				return null;
			}
			ByteBuffer b = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
			short[] ground = new short[256], water = new short[256];
			byte[] material = new byte[256];
			for (int i = 0; i < 256; i++) {
				ground[i] = b.getShort(GROUND_OFF + 2 * i);
				water[i] = b.getShort(WATER_OFF + 2 * i);
			}
			b.get(MATERIAL_OFF, material);
			return new Columns(ground, water, material, Byte.toUnsignedInt(b.get(DEPTH_OFF)), Byte.toUnsignedInt(b.get(BUILDING_DEPTH_OFF)));
		}

		/** What the terrain builder put at (local x, y, local z), or null. */
		public TerrainColumns.Kind kindAt(int localX, int y, int localZ) {
			int column = Messages.TerrainPatch.column(localX, localZ);
			return TerrainColumns.kindAt(groundY[column], waterY[column], material[column] & 0xFF, depth, buildingDepth, y);
		}
	}

	private static final ConcurrentHashMap<Long, Columns> CHUNKS = new ConcurrentHashMap<>();

	private TerrainIndex() {
	}

	static void put(long chunkKey, Columns columns) {
		CHUNKS.put(chunkKey, columns);
	}

	public static Columns get(int chunkX, int chunkZ) {
		return CHUNKS.get(TerrainRequester.key(chunkX, chunkZ));
	}

	/** The world closed (or another opened): nothing is known about the next one until its TerrainState loads. */
	public static void clear() {
		CHUNKS.clear();
	}

	public static int size() {
		return CHUNKS.size();
	}

	/** Whether {@code state} at (x, y, z) is a block the terrain builder placed and nobody has changed since. */
	public static boolean isTerrain(int x, int y, int z, BlockState state) {
		Columns c = CHUNKS.get(TerrainRequester.key(x >> 4, z >> 4));
		if (c == null) {
			return false;
		}
		TerrainColumns.Kind kind = c.kindAt(x & 15, y, z & 15);
		return kind != null && TerrainBuilder.state(kind) == state;
	}
}
