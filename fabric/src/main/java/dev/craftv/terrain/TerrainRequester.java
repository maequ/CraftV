package dev.craftv.terrain;

import static dev.craftv.link.Proto.*;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.function.LongPredicate;

/**
 * Decides which chunk columns to ask the host for (PROTOCOL.md §7.11). Minecraft-free and
 * single-threaded (the server thread owns it):
 * <ul>
 * <li>every chunk within {@code radius} of any player that isn't built yet is a candidate;</li>
 * <li>nearest first (distance to the closest player), ties broken by position so it's deterministic;</li>
 * <li>at most {@link dev.craftv.link.Proto#TERRAIN_MAX_IN_FLIGHT} unanswered at once;</li>
 * <li>a request unanswered for {@link dev.craftv.link.Proto#TERRAIN_RETRY_MS} is asked again.</li>
 * </ul>
 */
public final class TerrainRequester {
	public record Request(int chunkX, int chunkZ, int distance) {
	}

	/** A player's chunk position. */
	public record Center(int chunkX, int chunkZ) {
	}

	private final Map<Long, Long> inFlight = new HashMap<>(); // chunk key -> time asked (ms)
	private final int maxInFlight;
	private final long retryMs;

	public TerrainRequester() {
		this(TERRAIN_MAX_IN_FLIGHT, TERRAIN_RETRY_MS);
	}

	public TerrainRequester(int maxInFlight, long retryMs) {
		this.maxInFlight = maxInFlight;
		this.retryMs = retryMs;
	}

	public static long key(int chunkX, int chunkZ) {
		return (chunkX & 0xFFFFFFFFL) | ((long) chunkZ << 32); // same packing as Minecraft's ChunkPos.pack
	}

	/** The chunks to request now; they count as in flight from {@code nowMs}. */
	public List<Request> next(Collection<Center> players, int radius, LongPredicate isBuilt, long nowMs) {
		for (Iterator<Map.Entry<Long, Long>> it = inFlight.entrySet().iterator(); it.hasNext();) {
			if (nowMs - it.next().getValue() >= retryMs) {
				it.remove(); // no answer in time: may be asked again below
			}
		}
		int budget = maxInFlight - inFlight.size();
		if (budget <= 0 || players.isEmpty()) {
			return List.of();
		}
		Map<Long, Request> candidates = new HashMap<>();
		for (Center p : players) {
			for (int dz = -radius; dz <= radius; dz++) {
				for (int dx = -radius; dx <= radius; dx++) {
					int cx = p.chunkX() + dx, cz = p.chunkZ() + dz;
					if (Math.abs(cx) > MAX_CHUNK_COORD || Math.abs(cz) > MAX_CHUNK_COORD) {
						continue;
					}
					long k = key(cx, cz);
					if (inFlight.containsKey(k) || isBuilt.test(k)) {
						continue;
					}
					int distance = Math.max(Math.abs(dx), Math.abs(dz));
					Request old = candidates.get(k);
					if (old == null || distance < old.distance()) {
						candidates.put(k, new Request(cx, cz, distance));
					}
				}
			}
		}
		List<Request> sorted = new ArrayList<>(candidates.values());
		sorted.sort((a, b) -> a.distance() != b.distance() ? Integer.compare(a.distance(), b.distance())
			: a.chunkZ() != b.chunkZ() ? Integer.compare(a.chunkZ(), b.chunkZ()) : Integer.compare(a.chunkX(), b.chunkX()));
		List<Request> out = sorted.size() > budget ? new ArrayList<>(sorted.subList(0, budget)) : sorted;
		for (Request r : out) {
			inFlight.put(key(r.chunkX(), r.chunkZ()), nowMs);
		}
		return out;
	}

	/** A patch for this chunk arrived (answered or unsolicited). */
	public void answered(int chunkX, int chunkZ) {
		inFlight.remove(key(chunkX, chunkZ));
	}

	/** Don't ask for this chunk again for {@code delayMs} (it came back with no ground; the host may know more later). */
	public void snooze(int chunkX, int chunkZ, long nowMs, long delayMs) {
		inFlight.put(key(chunkX, chunkZ), nowMs + delayMs - retryMs); // expires like an unanswered request, delayMs from now
	}

	/** The host restarted or the link came back: whatever was in flight is lost. */
	public void reset() {
		inFlight.clear();
	}

	public int inFlight() {
		return inFlight.size();
	}
}
