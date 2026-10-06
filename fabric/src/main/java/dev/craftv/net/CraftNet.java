package dev.craftv.net;

import dev.craftv.CraftV;
import dev.craftv.link.Messages;
import dev.craftv.link.Proto;
import java.lang.foreign.MemorySegment;
import net.fabricmc.fabric.api.networking.v1.PayloadTypeRegistry;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.common.custom.CustomPacketPayload;
import net.minecraft.resources.Identifier;

/**
 * What a guest's CraftV Minecraft and the owner's server tell each other (DECISIONS D-030, two GTA players). A guest is a
 * friend whose own GTA drives their Minecraft player, exactly like the owner's; plain-Minecraft friends never see these.
 */
public final class CraftNet {
	private static final int COLUMNS_MAX_BYTES = 4096;

	private CraftNet() {
	}

	private static <T extends CustomPacketPayload> CustomPacketPayload.Type<T> typeOf(String path) {
		return new CustomPacketPayload.Type<>(Identifier.fromNamespaceAndPath(CraftV.MOD_ID, path));
	}

	/** Guest to server: this player's body is (or no longer is) driven by their own GTA. */
	public record GtaHello(boolean driving) implements CustomPacketPayload {
		public static final Type<GtaHello> TYPE = typeOf("gta_hello");
		public static final StreamCodec<RegistryFriendlyByteBuf, GtaHello> CODEC = StreamCodec.composite(ByteBufCodecs.BOOL, GtaHello::driving, GtaHello::new);

		@Override
		public Type<GtaHello> type() {
			return TYPE;
		}
	}

	/** Guest to server: their GTA player jumped (a first position, a cutscene, fast travel); put them there. */
	public record Snap(double x, double y, double z, float yaw, float pitch) implements CustomPacketPayload {
		public static final Type<Snap> TYPE = typeOf("snap");
		public static final StreamCodec<RegistryFriendlyByteBuf, Snap> CODEC = StreamCodec.composite(ByteBufCodecs.DOUBLE, Snap::x, ByteBufCodecs.DOUBLE, Snap::y,
			ByteBufCodecs.DOUBLE, Snap::z, ByteBufCodecs.FLOAT, Snap::yaw, ByteBufCodecs.FLOAT, Snap::pitch, Snap::new);

		@Override
		public Type<Snap> type() {
			return TYPE;
		}
	}

	/** Guest to server: ground their GTA scanned (PROTOCOL.md §7.12's TERRAIN_PATCH, as the link encodes it). */
	public record Patch(byte[] bytes) implements CustomPacketPayload {
		public static final Type<Patch> TYPE = typeOf("terrain_patch");
		public static final StreamCodec<RegistryFriendlyByteBuf, Patch> CODEC = StreamCodec.composite(ByteBufCodecs.byteArray(Proto.TERRAIN_PATCH_BYTES), Patch::bytes,
			Patch::new);

		public static Patch of(Messages.TerrainPatch p) {
			byte[] b = new byte[Proto.TERRAIN_PATCH_BYTES];
			p.write(MemorySegment.ofArray(b), 0);
			return new Patch(b);
		}

		/** Null if it isn't a whole, valid patch. */
		public Messages.TerrainPatch patch() {
			if (bytes.length != Proto.TERRAIN_PATCH_BYTES) {
				return null;
			}
			Messages.TerrainPatch p = Messages.TerrainPatch.read(MemorySegment.ofArray(bytes), 0);
			return p.valid() ? p : null;
		}

		@Override
		public Type<Patch> type() {
			return TYPE;
		}
	}

	/** Server to guest: one built terrain chunk (TerrainIndex.Columns), so the guest's view hides it as the owner's does. */
	public record Columns(int chunkX, int chunkZ, byte[] bytes) implements CustomPacketPayload {
		public static final Type<Columns> TYPE = typeOf("terrain_columns");
		public static final StreamCodec<RegistryFriendlyByteBuf, Columns> CODEC = StreamCodec.composite(ByteBufCodecs.INT, Columns::chunkX, ByteBufCodecs.INT,
			Columns::chunkZ, ByteBufCodecs.byteArray(COLUMNS_MAX_BYTES), Columns::bytes, Columns::new);

		@Override
		public Type<Columns> type() {
			return TYPE;
		}
	}

	/** Both sides: called from the common initializer. */
	public static void register() {
		PayloadTypeRegistry.serverboundPlay().register(GtaHello.TYPE, GtaHello.CODEC);
		PayloadTypeRegistry.serverboundPlay().register(Snap.TYPE, Snap.CODEC);
		PayloadTypeRegistry.serverboundPlay().register(Patch.TYPE, Patch.CODEC);
		PayloadTypeRegistry.clientboundPlay().register(Columns.TYPE, Columns.CODEC);
	}
}
