package dev.craftv.terrain;

import static dev.craftv.link.Proto.*;
import static org.junit.jupiter.api.Assertions.*;

import dev.craftv.terrain.TerrainColumns.Kind;
import java.util.ArrayList;
import java.util.List;
import org.junit.jupiter.api.Test;

class TerrainColumnsTest {
	record Block(int y, Kind kind) {
	}

	static List<Block> column(int ground, int water, int material, int minY, int maxY) {
		List<Block> out = new ArrayList<>();
		int n = TerrainColumns.build((short) ground, (short) water, material, 4, 10, minY, maxY, (y, k) -> out.add(new Block(y, k)));
		assertEquals(out.size(), n);
		return out;
	}

	@Test
	void grassHasDirtThenStoneDownToTheDepth() {
		var c = column(70, NO_WATER, MAT_GRASS, -1024, 1023);
		assertEquals(List.of(new Block(70, Kind.GRASS_BLOCK), new Block(69, Kind.DIRT), new Block(68, Kind.DIRT), new Block(67, Kind.DIRT), new Block(66, Kind.STONE)), c);
	}

	@Test
	void waterFillsAboveTheGroundTopDown() {
		var c = column(58, 62, MAT_SAND, -1024, 1023);
		assertEquals(new Block(62, Kind.WATER), c.get(0));
		assertEquals(new Block(59, Kind.WATER), c.get(3));
		assertEquals(new Block(58, Kind.SAND), c.get(4));
		assertEquals(Kind.SANDSTONE, c.get(c.size() - 1).kind());
		assertEquals(4 + 5, c.size());
	}

	@Test
	void waterAtOrBelowTheGroundIsIgnored() {
		assertEquals(5, column(62, 62, MAT_GRASS, -1024, 1023).size());
		assertEquals(5, column(62, 40, MAT_GRASS, -1024, 1023).size());
	}

	@Test
	void noGroundPlacesNothingEvenWithWater() {
		assertTrue(column(NO_GROUND, 62, MAT_GRASS, -1024, 1023).isEmpty());
	}

	@Test
	void buildingTopsGoDeeperSoBuildingsLookSolid() {
		var c = column(90, NO_WATER, MAT_BUILDING, -1024, 1023);
		assertEquals(11, c.size());
		assertTrue(c.stream().allMatch(b -> b.kind() == Kind.STONE_BRICKS));
	}

	@Test
	void blocksOutsideTheWorldHeightAreSkipped() {
		var c = column(1025, NO_WATER, MAT_ROCK, -1024, 1023);
		assertEquals(1023, c.get(0).y());
		assertEquals(1021, c.get(c.size() - 1).y());
		var low = column(-1022, NO_WATER, MAT_ROCK, -1024, 1023);
		assertEquals(-1024, low.get(low.size() - 1).y());
		assertEquals(3, low.size());
	}

	@Test
	void everyMaterialHasASurfaceAndUnknownOnesAreStone() {
		assertEquals(Kind.GRAY_CONCRETE, TerrainColumns.surface(MAT_ROAD));
		assertEquals(Kind.SMOOTH_STONE, TerrainColumns.surface(MAT_PAVEMENT));
		assertEquals(Kind.MUD, TerrainColumns.surface(MAT_MUD));
		assertEquals(Kind.STONE, TerrainColumns.surface(MAT_UNKNOWN));
		assertEquals(Kind.STONE, TerrainColumns.surface(200));
		for (int m = 0; m < MAT_COUNT; m++) {
			assertNotNull(TerrainColumns.surface(m));
			assertNotNull(TerrainColumns.topsoil(m));
			assertNotNull(TerrainColumns.deep(m));
		}
	}

	@Test
	void kindAtAgreesWithBuildForEveryHeight() {
		int[][] cases = { { 70, NO_WATER, MAT_GRASS }, { 58, 62, MAT_SAND }, { 90, NO_WATER, MAT_BUILDING }, { 0, 3, MAT_MUD }, { 12, NO_WATER, MAT_ROAD } };
		for (int[] c : cases) {
			java.util.Map<Integer, Kind> built = new java.util.HashMap<>();
			TerrainColumns.build((short) c[0], (short) c[1], c[2], 4, 10, -1024, 1023, (y, k) -> built.put(y, k));
			for (int y = c[0] - 20; y <= c[0] + 10; y++) {
				assertEquals(built.get(y), TerrainColumns.kindAt((short) c[0], (short) c[1], c[2], 4, 10, y), "y " + y + " of " + java.util.Arrays.toString(c));
			}
		}
		assertNull(TerrainColumns.kindAt(NO_GROUND, (short) 70, MAT_GRASS, 4, 10, 70));
	}
}

