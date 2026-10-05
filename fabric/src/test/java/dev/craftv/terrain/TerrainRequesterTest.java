package dev.craftv.terrain;

import static org.junit.jupiter.api.Assertions.*;

import dev.craftv.terrain.TerrainRequester.Center;
import dev.craftv.terrain.TerrainRequester.Request;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import org.junit.jupiter.api.Test;

class TerrainRequesterTest {
	@Test
	void nearestFirstAndCappedInFlight() {
		var r = new TerrainRequester(10, 5000);
		List<Request> got = r.next(List.of(new Center(0, 0)), 3, k -> false, 0);
		assertEquals(10, got.size());
		assertEquals(new Request(0, 0, 0), got.get(0));
		assertTrue(got.subList(1, 9).stream().allMatch(q -> q.distance() == 1), "the 8 neighbours come next");
		assertEquals(2, got.get(9).distance());
		assertEquals(10, r.inFlight());
		assertTrue(r.next(List.of(new Center(0, 0)), 3, k -> false, 100).isEmpty(), "nothing more while 10 are in flight");
	}

	@Test
	void answeredChunksFreeSlotsAndAreNotAskedTwice() {
		var r = new TerrainRequester(4, 5000);
		Set<Long> built = new HashSet<>();
		List<Request> first = r.next(List.of(new Center(5, -5)), 1, built::contains, 0);
		assertEquals(4, first.size());
		for (Request q : first) {
			r.answered(q.chunkX(), q.chunkZ());
			built.add(TerrainRequester.key(q.chunkX(), q.chunkZ()));
		}
		List<Request> second = r.next(List.of(new Center(5, -5)), 1, built::contains, 10);
		assertEquals(4, second.size());
		for (Request q : second) {
			assertFalse(first.contains(q));
		}
	}

	@Test
	void unansweredRequestsAreRetriedAfterTheTimeout() {
		var r = new TerrainRequester(1, 5000);
		assertEquals(1, r.next(List.of(new Center(0, 0)), 0, k -> false, 0).size());
		assertTrue(r.next(List.of(new Center(0, 0)), 0, k -> false, 4999).isEmpty());
		assertEquals(List.of(new Request(0, 0, 0)), r.next(List.of(new Center(0, 0)), 0, k -> false, 5000));
	}

	@Test
	void distanceIsToTheClosestPlayerAndOverlapsAreAskedOnce() {
		var r = new TerrainRequester(100, 5000);
		List<Request> got = r.next(List.of(new Center(0, 0), new Center(2, 0)), 1, k -> false, 0);
		assertEquals(3 * 5, got.size(), "two 3x3 squares overlapping in one column of 3");
		assertEquals(got.size(), new HashSet<>(got).size());
		assertTrue(got.contains(new Request(1, 0, 1)));
		assertEquals(0, got.stream().filter(q -> q.chunkX() == 2 && q.chunkZ() == 0).findFirst().orElseThrow().distance());
	}

	@Test
	void resetForgetsInFlight() {
		var r = new TerrainRequester(2, 5000);
		r.next(List.of(new Center(0, 0)), 1, k -> false, 0);
		assertEquals(2, r.inFlight());
		r.reset();
		assertEquals(0, r.inFlight());
	}

	@Test
	void keyMatchesMinecraftsChunkPacking() {
		assertEquals(0x00000002_00000001L, TerrainRequester.key(1, 2));
		assertEquals(0xFFFFFFFF_FFFFFFFFL, TerrainRequester.key(-1, -1));
		assertEquals(0xFFFFFFFD_00000007L, TerrainRequester.key(7, -3));
	}
}
