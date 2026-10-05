package dev.craftv.terrain;

import static dev.craftv.link.Proto.*;

/**
 * Which blocks one column of host ground becomes (PROTOCOL.md §7.12, brief §6.2). Minecraft-free so it
 * can be unit-tested: {@link TerrainBuilder} maps each {@link Kind} to a block state.
 *
 * <p>A column is the surface block at {@code groundY}, the layers under it down to {@code depth} blocks
 * below the surface (building tops go deeper, so buildings look solid), and water from
 * {@code groundY + 1} up to {@code waterY}. Anything outside {@code [minY, maxY]} is skipped.
 */
public final class TerrainColumns {
	public static final int DEFAULT_DEPTH = 8;
	public static final int DEFAULT_BUILDING_DEPTH = 40;
	/** Top-soil layers (dirt under grass, sand under sand) before the deep block starts. */
	private static final int TOPSOIL = 3;

	public enum Kind {
		GRASS_BLOCK, DIRT, SAND, SANDSTONE, STONE, GRAY_CONCRETE, SMOOTH_STONE, GRAVEL, SNOW_BLOCK, OAK_PLANKS, LIGHT_GRAY_CONCRETE, STONE_BRICKS, MUD, WATER
	}

	@FunctionalInterface
	public interface Sink {
		void place(int y, Kind kind);
	}

	private TerrainColumns() {
	}

	/**
	 * Calls {@code sink} for every block of the column, top to bottom (water first). Returns how many
	 * blocks it placed. A column without ground places nothing.
	 */
	public static int build(short groundY, short waterY, int material, int depth, int buildingDepth, int minY, int maxY, Sink sink) {
		if (groundY == NO_GROUND) {
			return 0;
		}
		int placed = 0;
		if (waterY != NO_WATER && waterY > groundY) {
			for (int y = Math.min(waterY, maxY); y > groundY; y--) {
				if (y >= minY) {
					sink.place(y, Kind.WATER);
					placed++;
				}
			}
		}
		int layers = material == MAT_BUILDING ? buildingDepth : depth;
		int bottom = Math.max(groundY - layers, minY);
		for (int y = Math.min(groundY, maxY); y >= bottom; y--) {
			int below = groundY - y;
			sink.place(y, below == 0 ? surface(material) : below <= TOPSOIL ? topsoil(material) : deep(material));
			placed++;
		}
		return placed;
	}

	static Kind surface(int material) {
		return switch (material) {
			case MAT_GRASS -> Kind.GRASS_BLOCK;
			case MAT_DIRT -> Kind.DIRT;
			case MAT_SAND -> Kind.SAND;
			case MAT_ROAD -> Kind.GRAY_CONCRETE;
			case MAT_PAVEMENT -> Kind.SMOOTH_STONE;
			case MAT_GRAVEL -> Kind.GRAVEL;
			case MAT_SNOW -> Kind.SNOW_BLOCK;
			case MAT_WOOD -> Kind.OAK_PLANKS;
			case MAT_METAL -> Kind.LIGHT_GRAY_CONCRETE;
			case MAT_BUILDING -> Kind.STONE_BRICKS;
			case MAT_MUD -> Kind.MUD;
			default -> Kind.STONE; // ROCK, UNKNOWN and anything newer (§7.12)
		};
	}

	static Kind topsoil(int material) {
		return switch (material) {
			case MAT_GRASS, MAT_DIRT -> Kind.DIRT;
			case MAT_SAND -> Kind.SAND;
			case MAT_GRAVEL -> Kind.GRAVEL;
			case MAT_MUD -> Kind.MUD;
			case MAT_WOOD -> Kind.OAK_PLANKS;
			case MAT_BUILDING -> Kind.STONE_BRICKS;
			default -> Kind.STONE;
		};
	}

	static Kind deep(int material) {
		return switch (material) {
			case MAT_SAND -> Kind.SANDSTONE;
			case MAT_MUD -> Kind.DIRT;
			case MAT_BUILDING -> Kind.STONE_BRICKS;
			default -> Kind.STONE;
		};
	}
}
