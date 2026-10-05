package dev.craftv.terrain;

import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import it.unimi.dsi.fastutil.longs.LongOpenHashSet;
import java.util.Arrays;
import java.util.stream.LongStream;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.saveddata.SavedData;
import net.minecraft.world.level.saveddata.SavedDataType;

/**
 * Which chunk columns already hold host terrain, saved with the world (PROTOCOL.md §7.11: a chunk MC has
 * built is never requested again). Keys are {@link TerrainRequester#key}, the same packing as ChunkPos.
 */
public final class TerrainState extends SavedData {
	private static final Codec<TerrainState> CODEC = RecordCodecBuilder.create(i -> i.group(
		Codec.LONG_STREAM.fieldOf("built_chunks").forGetter(s -> Arrays.stream(s.built.toLongArray()))).apply(i, TerrainState::new));
	// ASSUMPTION: a null DataFixTypes is accepted for mod data (no vanilla data fixer applies to it).
	private static final SavedDataType<TerrainState> TYPE = new SavedDataType<>(Identifier.fromNamespaceAndPath("craftv", "terrain"), TerrainState::new, CODEC,
		null);

	private final LongOpenHashSet built;

	public TerrainState() {
		this.built = new LongOpenHashSet();
	}

	private TerrainState(LongStream chunks) {
		this.built = new LongOpenHashSet(chunks.toArray());
	}

	public static TerrainState of(ServerLevel level) {
		return level.getDataStorage().computeIfAbsent(TYPE);
	}

	public boolean isBuilt(long chunkKey) {
		return built.contains(chunkKey);
	}

	public void markBuilt(long chunkKey) {
		if (built.add(chunkKey)) {
			setDirty();
		}
	}

	public int size() {
		return built.size();
	}
}
