package dev.craftv.terrain;

import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import dev.craftv.CraftLog;
import dev.craftv.link.Messages;
import it.unimi.dsi.fastutil.longs.Long2ObjectOpenHashMap;
import it.unimi.dsi.fastutil.longs.LongOpenHashSet;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import java.util.stream.LongStream;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.saveddata.SavedData;
import net.minecraft.world.level.saveddata.SavedDataType;

/**
 * Which chunk columns already hold host terrain, and the columns they were built from, saved with the world
 * (PROTOCOL.md §7.11: a chunk MC has built is never requested again). The columns feed {@link TerrainIndex}, which
 * the owner's passthrough view uses to hide terrain. Keys are {@link TerrainRequester#key}, the same packing as
 * ChunkPos.
 *
 * <p>Worlds saved before v1.2 only listed the built chunks ({@code built_chunks}). Those chunks are forgotten on
 * load, so they are requested and built again, this time with their columns.
 */
public final class TerrainState extends SavedData {
	private record Entry(long chunk, ByteBuffer columns) {
		static final Codec<Entry> CODEC = RecordCodecBuilder.create(i -> i.group(Codec.LONG.fieldOf("chunk").forGetter(Entry::chunk),
			Codec.BYTE_BUFFER.fieldOf("columns").forGetter(Entry::columns)).apply(i, Entry::new));
	}

	private static final Codec<TerrainState> CODEC = RecordCodecBuilder.create(i -> i.group(
		Codec.LONG_STREAM.optionalFieldOf("built_chunks").forGetter(s -> Optional.empty()),
		Entry.CODEC.listOf().optionalFieldOf("chunks", List.of()).forGetter(TerrainState::entries)).apply(i, TerrainState::new));
	// ASSUMPTION: a null DataFixTypes is accepted for mod data (no vanilla data fixer applies to it).
	private static final SavedDataType<TerrainState> TYPE = new SavedDataType<>(Identifier.fromNamespaceAndPath("craftv", "terrain"), TerrainState::new, CODEC,
		null);

	private final LongOpenHashSet built = new LongOpenHashSet();
	private final Long2ObjectOpenHashMap<byte[]> columns = new Long2ObjectOpenHashMap<>();

	public TerrainState() {
	}

	private TerrainState(Optional<LongStream> legacy, List<Entry> entries) {
		for (Entry e : entries) {
			byte[] bytes = new byte[e.columns().remaining()];
			e.columns().duplicate().get(bytes);
			TerrainIndex.Columns c = TerrainIndex.Columns.decode(bytes);
			if (c != null) {
				built.add(e.chunk());
				columns.put(e.chunk(), bytes);
				TerrainIndex.put(e.chunk(), c);
			}
		}
		long forgotten = legacy.map(s -> s.filter(k -> !built.contains(k)).count()).orElse(0L);
		if (forgotten > 0) {
			CraftLog.info("terrain: " + forgotten + " chunks were built before v1.2 without their columns; they'll be built again as you get near");
			setDirty();
		}
	}

	private List<Entry> entries() {
		List<Entry> out = new ArrayList<>(columns.size());
		for (var e : columns.long2ObjectEntrySet()) {
			out.add(new Entry(e.getLongKey(), ByteBuffer.wrap(e.getValue())));
		}
		return out;
	}

	public static TerrainState of(ServerLevel level) {
		return level.getDataStorage().computeIfAbsent(TYPE);
	}

	public boolean isBuilt(long chunkKey) {
		return built.contains(chunkKey);
	}

	/** Remembers the chunk and its columns. Call before placing its blocks, so the owner's view already knows them. */
	public void markBuilt(long chunkKey, Messages.TerrainPatch patch, int depth, int buildingDepth) {
		TerrainIndex.Columns c = TerrainIndex.Columns.of(patch, depth, buildingDepth);
		TerrainIndex.put(chunkKey, c);
		columns.put(chunkKey, c.encode());
		built.add(chunkKey);
		setDirty();
	}

	public int size() {
		return built.size();
	}
}
