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

	/** Guest to server: their GTA player got hurt (PROTOCOL.md §7.17 INPUT DAMAGE): cause, half hearts. */
	public record Hurt(int cause, int halfHearts) implements CustomPacketPayload {
		public static final Type<Hurt> TYPE = typeOf("hurt");
		public static final StreamCodec<RegistryFriendlyByteBuf, Hurt> CODEC = StreamCodec.composite(ByteBufCodecs.VAR_INT, Hurt::cause, ByteBufCodecs.VAR_INT,
			Hurt::halfHearts, Hurt::new);

		@Override
		public Type<Hurt> type() {
			return TYPE;
		}
	}

	/** Guest to server: what their GTA player is doing (GtaWorld.STATE_* flags), when it changes. */
	public record GtaState(int flags) implements CustomPacketPayload {
		public static final Type<GtaState> TYPE = typeOf("gta_state");
		public static final StreamCodec<RegistryFriendlyByteBuf, GtaState> CODEC = StreamCodec.composite(ByteBufCodecs.VAR_INT, GtaState::flags, GtaState::new);

		@Override
		public Type<GtaState> type() {
			return TYPE;
		}
	}

	/** Server to guest: an explosion or a flying arrow near them (PROTOCOL.md §7.20), for their GTA. */
	public record Event(int kind, float power, double x, double y, double z, int id) implements CustomPacketPayload {
		public static final Type<Event> TYPE = typeOf("world_event");
		public static final StreamCodec<RegistryFriendlyByteBuf, Event> CODEC = StreamCodec.composite(ByteBufCodecs.VAR_INT, Event::kind, ByteBufCodecs.FLOAT,
			Event::power, ByteBufCodecs.DOUBLE, Event::x, ByteBufCodecs.DOUBLE, Event::y, ByteBufCodecs.DOUBLE, Event::z, ByteBufCodecs.INT, Event::id, Event::new);

		public Messages.WorldEvent message() {
			return new Messages.WorldEvent(kind, power, x, y, z, id);
		}

		@Override
		public Type<Event> type() {
			return TYPE;
		}
	}

	/** Both sides: called from the common initializer. */
	public static void register() {
		PayloadTypeRegistry.serverboundPlay().register(GtaHello.TYPE, GtaHello.CODEC);
		PayloadTypeRegistry.serverboundPlay().register(Snap.TYPE, Snap.CODEC);
		PayloadTypeRegistry.serverboundPlay().register(Patch.TYPE, Patch.CODEC);
		PayloadTypeRegistry.serverboundPlay().register(Hurt.TYPE, Hurt.CODEC);
		PayloadTypeRegistry.serverboundPlay().register(GtaState.TYPE, GtaState.CODEC);
		PayloadTypeRegistry.clientboundPlay().register(Columns.TYPE, Columns.CODEC);
		PayloadTypeRegistry.clientboundPlay().register(Event.TYPE, Event.CODEC);
	}
}
