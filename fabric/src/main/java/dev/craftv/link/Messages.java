package dev.craftv.link;

import static dev.craftv.link.Proto.*;

import java.lang.foreign.MemorySegment;
import java.nio.charset.StandardCharsets;

/**
 * The v1.1 message payloads (PROTOCOL.md §7) as records that read themselves from and write
 * themselves to a {@link MemorySegment}. Every {@code read} takes each field exactly once into a
 * local, so a misbehaving peer can't change a value between validation and use.
 */
public final class Messages {
	private Messages() {
	}

	/** Something that can be written as a ring payload. */
	public interface Payload {
		int type();

		int payloadBytes();

		void write(MemorySegment s, long off);
	}

	// ---- §7.1 HELLO ------------------------------------------------------------------------------
	public record Hello(int versionMajor, int versionMinor, int role, int pid, int session, int capabilities, String software) implements Payload {
		public static Hello of(int role, int pid, int session, String software) {
			return new Hello(VERSION_MAJOR, VERSION_MINOR, role, pid, session, 0, software);
		}

		@Override
		public int type() {
			return MSG_HELLO;
		}

		@Override
		public int payloadBytes() {
			return HELLO_BYTES;
		}

		@Override
		public void write(MemorySegment s, long off) {
			s.asSlice(off, HELLO_BYTES).fill((byte) 0);
			s.set(I16, off, (short) versionMajor);
			s.set(I16, off + 2, (short) versionMinor);
			s.set(I32, off + 4, role);
			s.set(I32, off + 8, pid);
			s.set(I32, off + 12, session);
			s.set(I32, off + 16, capabilities);
			byte[] text = Text.truncateUtf8(software, SOFTWARE_MAX_BYTES);
			s.set(I16, off + 20, (short) text.length);
			MemorySegment.copy(MemorySegment.ofArray(text), 0, s, off + 24, text.length);
		}

		public static Hello read(MemorySegment s, long off) {
			int textBytes = Short.toUnsignedInt(s.get(I16, off + 20));
			String software = textBytes <= SOFTWARE_MAX_BYTES ? Text.readUtf8(s, off + 24, textBytes) : null;
			return new Hello(Short.toUnsignedInt(s.get(I16, off)), Short.toUnsignedInt(s.get(I16, off + 2)), s.get(I32, off + 4), s.get(I32, off + 8),
				s.get(I32, off + 12), s.get(I32, off + 16), software);
		}

		public boolean valid() {
			return (role == ROLE_HOST || role == ROLE_MC) && software != null && session != 0;
		}
	}

	// ---- §7.2 HEARTBEAT ----------------------------------------------------------------------------
	public record Heartbeat(long sentUs, long echoUs, long echoHoldUs, int counter) implements Payload {
		@Override
		public int type() {
			return MSG_HEARTBEAT;
		}

		@Override
		public int payloadBytes() {
			return HEARTBEAT_BYTES;
		}

		@Override
		public void write(MemorySegment s, long off) {
			s.set(I64, off, sentUs);
			s.set(I64, off + 8, echoUs);
			s.set(I64, off + 16, echoHoldUs);
			s.set(I32, off + 24, counter);
			s.set(I32, off + 28, 0);
		}

		public static Heartbeat read(MemorySegment s, long off) {
			return new Heartbeat(s.get(I64, off), s.get(I64, off + 8), s.get(I64, off + 16), s.get(I32, off + 24));
		}

		public boolean valid() {
			return sentUs != 0;
		}
	}

	// ---- §7.3 PLAYER_STATE -------------------------------------------------------------------------
	public record PlayerState(double x, double y, double z, float vx, float vy, float vz, float yaw, float pitch, int flags, long timeUs, int frame)
		implements Payload {
		@Override
		public int type() {
			return MSG_PLAYER_STATE;
		}

		@Override
		public int payloadBytes() {
			return PLAYER_STATE_BYTES;
		}

		@Override
		public void write(MemorySegment s, long off) {
			s.set(F64, off, x);
			s.set(F64, off + 8, y);
			s.set(F64, off + 16, z);
			s.set(F32, off + 24, vx);
			s.set(F32, off + 28, vy);
			s.set(F32, off + 32, vz);
			s.set(F32, off + 36, yaw);
			s.set(F32, off + 40, pitch);
			s.set(I32, off + 44, flags);
			s.set(I64, off + 48, timeUs);
			s.set(I32, off + 56, frame);
			s.set(I32, off + 60, 0);
		}

		public static PlayerState read(MemorySegment s, long off) {
			return new PlayerState(s.get(F64, off), s.get(F64, off + 8), s.get(F64, off + 16), s.get(F32, off + 24), s.get(F32, off + 28),
				s.get(F32, off + 32), s.get(F32, off + 36), s.get(F32, off + 40), s.get(I32, off + 44), s.get(I64, off + 48), s.get(I32, off + 56));
		}

		public boolean onGround() {
			return (flags & PLAYER_ON_GROUND) != 0;
		}

		public boolean teleport() {
			return (flags & PLAYER_TELEPORT) != 0;
		}

		public boolean valid() {
			if (!Double.isFinite(x) || !Double.isFinite(y) || !Double.isFinite(z) || !Float.isFinite(vx) || !Float.isFinite(vy) || !Float.isFinite(vz)
				|| !Float.isFinite(yaw) || !Float.isFinite(pitch)) {
				return false;
			}
			if (Math.abs(x) > MAX_HORIZONTAL_COORD || Math.abs(z) > MAX_HORIZONTAL_COORD || y < MIN_Y || y > MAX_Y) {
				return false;
			}
			if (pitch < -90.0F || pitch > 90.0F) {
				return false;
			}
			float speed2 = vx * vx + vy * vy + vz * vz;
			return speed2 <= MAX_SPEED * MAX_SPEED && (flags & ~PLAYER_KNOWN_FLAGS) == 0;
		}
	}

	private static boolean blockCoordsOk(int x, int y, int z) {
		return y >= (int) MIN_Y && y <= (int) MAX_Y && x >= -(int) MAX_HORIZONTAL_COORD && x <= (int) MAX_HORIZONTAL_COORD
			&& z >= -(int) MAX_HORIZONTAL_COORD && z <= (int) MAX_HORIZONTAL_COORD;
	}

	private static boolean faceOk(int face) {
		return (face >= 0 && face <= FACE_MAX) || face == FACE_UNKNOWN;
	}

	// ---- §7.4 BLOCK_SET ------------------------------------------------------------------------------
	public record BlockSet(int x, int y, int z, int blockId, int flags, int requestId) implements Payload {
		@Override
		public int type() {
			return MSG_BLOCK_SET;
		}

		@Override
		public int payloadBytes() {
			return BLOCK_SET_BYTES;
		}

		@Override
		public void write(MemorySegment s, long off) {
			s.set(I32, off, x);
			s.set(I32, off + 4, y);
			s.set(I32, off + 8, z);
			s.set(I32, off + 12, blockId);
			s.set(I32, off + 16, flags);
			s.set(I32, off + 20, requestId);
		}

		public static BlockSet read(MemorySegment s, long off) {
			return new BlockSet(s.get(I32, off), s.get(I32, off + 4), s.get(I32, off + 8), s.get(I32, off + 12), s.get(I32, off + 16), s.get(I32, off + 20));
		}

		public boolean echo() {
			return (flags & BLOCK_SET_ECHO) != 0;
		}

		public boolean valid() {
			return blockCoordsOk(x, y, z) && (flags & ~BLOCK_SET_KNOWN_FLAGS) == 0;
		}
	}

	// ---- §7.5 BLOCK_BREAK_REQUEST / BLOCK_PLACE_REQUEST ---------------------------------------
	public record BlockBreakRequest(int requestId, int x, int y, int z, int face, int flags) implements Payload {
		@Override
		public int type() {
			return MSG_BLOCK_BREAK_REQUEST;
		}

		@Override
		public int payloadBytes() {
			return BLOCK_REQUEST_BYTES;
		}

		@Override
		public void write(MemorySegment s, long off) {
			writeRequest(s, off, requestId, x, y, z, face, flags);
		}

		public static BlockBreakRequest read(MemorySegment s, long off) {
			return new BlockBreakRequest(s.get(I32, off), s.get(I32, off + 4), s.get(I32, off + 8), s.get(I32, off + 12), Byte.toUnsignedInt(s.get(U8, off + 16)),
				s.get(I32, off + 20));
		}

		public boolean valid() {
			return requestId != 0 && faceOk(face) && blockCoordsOk(x, y, z);
		}
	}

	public record BlockPlaceRequest(int requestId, int x, int y, int z, int face, int blockId) implements Payload {
		@Override
		public int type() {
			return MSG_BLOCK_PLACE_REQUEST;
		}

		@Override
		public int payloadBytes() {
			return BLOCK_REQUEST_BYTES;
		}

		@Override
		public void write(MemorySegment s, long off) {
			writeRequest(s, off, requestId, x, y, z, face, blockId);
		}

		public static BlockPlaceRequest read(MemorySegment s, long off) {
			return new BlockPlaceRequest(s.get(I32, off), s.get(I32, off + 4), s.get(I32, off + 8), s.get(I32, off + 12), Byte.toUnsignedInt(s.get(U8, off + 16)),
				s.get(I32, off + 20));
		}

		public boolean valid() {
			return requestId != 0 && faceOk(face) && blockCoordsOk(x, y, z);
		}
	}

	private static void writeRequest(MemorySegment s, long off, int requestId, int x, int y, int z, int face, int last) {
		s.set(I32, off, requestId);
		s.set(I32, off + 4, x);
		s.set(I32, off + 8, y);
		s.set(I32, off + 12, z);
		s.set(U8, off + 16, (byte) face);
		s.set(U8, off + 17, (byte) 0);
		s.set(U8, off + 18, (byte) 0);
		s.set(U8, off + 19, (byte) 0);
		s.set(I32, off + 20, last);
	}

	// ---- §7.6 LOG ---------------------------------------------------------------------------------------
	public record Log(int level, String text) implements Payload {
		@Override
		public int type() {
			return MSG_LOG;
		}

		@Override
		public int payloadBytes() {
			return LOG_BYTES;
		}

		@Override
		public void write(MemorySegment s, long off) {
			s.asSlice(off, LOG_BYTES).fill((byte) 0);
			byte[] bytes = Text.truncateUtf8(text, LOG_TEXT_MAX_BYTES);
			s.set(U8, off, (byte) level);
			s.set(I16, off + 2, (short) bytes.length);
			MemorySegment.copy(MemorySegment.ofArray(bytes), 0, s, off + 8, bytes.length);
		}

		public static Log read(MemorySegment s, long off) {
			int level = Byte.toUnsignedInt(s.get(U8, off));
			int textBytes = Short.toUnsignedInt(s.get(I16, off + 2));
			return new Log(level, textBytes <= LOG_TEXT_MAX_BYTES ? Text.readUtf8(s, off + 8, textBytes) : null);
		}

		public boolean valid() {
			return level >= LOG_TRACE && level <= LOG_ERROR && text != null;
		}
	}

	private static boolean positionOk(double x, double y, double z) {
		return Double.isFinite(x) && Double.isFinite(y) && Double.isFinite(z) && Math.abs(x) <= MAX_HORIZONTAL_COORD && Math.abs(z) <= MAX_HORIZONTAL_COORD
			&& y >= MIN_Y && y <= MAX_Y;
	}

	private static boolean chunkOk(int chunkX, int chunkZ) {
		return chunkX >= -MAX_CHUNK_COORD && chunkX <= MAX_CHUNK_COORD && chunkZ >= -MAX_CHUNK_COORD && chunkZ <= MAX_CHUNK_COORD;
	}

	private static boolean heightOk(short y, short sentinel) {
		return y == sentinel || (y >= (int) MIN_Y && y <= (int) MAX_Y);
	}

	// ---- §7.8 REMOTE_PLAYER_JOIN (v1.1) ----------------------------------------------------------
	/** {@code uuidMsb}/{@code uuidLsb} are {@link java.util.UUID}'s two halves, written most significant byte first. */
	public record RemotePlayerJoin(int playerId, int flags, long uuidMsb, long uuidLsb, String name) implements Payload {
		@Override
		public int type() {
			return MSG_REMOTE_PLAYER_JOIN;
		}

		@Override
		public int payloadBytes() {
			return REMOTE_PLAYER_JOIN_BYTES;
		}

		@Override
		public void write(MemorySegment s, long off) {
			s.asSlice(off, REMOTE_PLAYER_JOIN_BYTES).fill((byte) 0);
			s.set(I32, off, playerId);
			s.set(I32, off + 4, flags);
			s.set(BE64, off + 8, uuidMsb);
			s.set(BE64, off + 16, uuidLsb);
			byte[] text = Text.truncateUtf8(name, PLAYER_NAME_MAX_BYTES);
			s.set(I16, off + 24, (short) text.length);
			MemorySegment.copy(MemorySegment.ofArray(text), 0, s, off + 32, text.length);
		}

		public static RemotePlayerJoin read(MemorySegment s, long off) {
			int nameBytes = Short.toUnsignedInt(s.get(I16, off + 24));
			String name = nameBytes >= 1 && nameBytes <= PLAYER_NAME_MAX_BYTES ? Text.readUtf8(s, off + 32, nameBytes) : null;
			return new RemotePlayerJoin(s.get(I32, off), s.get(I32, off + 4), s.get(BE64, off + 8), s.get(BE64, off + 16), name);
		}

		public boolean valid() {
			return playerId != 0 && name != null;
		}
	}

	// ---- §7.9 REMOTE_PLAYER_STATE (v1.1) ---------------------------------------------------------
	public record RemotePlayerState(int playerId, int flags, double x, double y, double z, float vx, float vy, float vz, float yaw, float pitch,
		float bodyYaw, int tick, int gameMode, int health) implements Payload {
		@Override
		public int type() {
			return MSG_REMOTE_PLAYER_STATE;
		}

		@Override
		public int payloadBytes() {
			return REMOTE_PLAYER_STATE_BYTES;
		}

		@Override
		public void write(MemorySegment s, long off) {
			s.set(I32, off, playerId);
			s.set(I32, off + 4, flags);
			s.set(F64, off + 8, x);
			s.set(F64, off + 16, y);
			s.set(F64, off + 24, z);
			s.set(F32, off + 32, vx);
			s.set(F32, off + 36, vy);
			s.set(F32, off + 40, vz);
			s.set(F32, off + 44, yaw);
			s.set(F32, off + 48, pitch);
			s.set(F32, off + 52, bodyYaw);
			s.set(I32, off + 56, tick);
			s.set(U8, off + 60, (byte) gameMode);
			s.set(U8, off + 61, (byte) health);
			s.set(I16, off + 62, (short) 0);
		}

		public static RemotePlayerState read(MemorySegment s, long off) {
			return new RemotePlayerState(s.get(I32, off), s.get(I32, off + 4), s.get(F64, off + 8), s.get(F64, off + 16), s.get(F64, off + 24),
				s.get(F32, off + 32), s.get(F32, off + 36), s.get(F32, off + 40), s.get(F32, off + 44), s.get(F32, off + 48), s.get(F32, off + 52),
				s.get(I32, off + 56), Byte.toUnsignedInt(s.get(U8, off + 60)), Byte.toUnsignedInt(s.get(U8, off + 61)));
		}

		public boolean valid() {
			if (playerId == 0 || !positionOk(x, y, z)) {
				return false;
			}
			if (!Float.isFinite(vx) || !Float.isFinite(vy) || !Float.isFinite(vz) || !Float.isFinite(yaw) || !Float.isFinite(pitch) || !Float.isFinite(bodyYaw)) {
				return false;
			}
			float speed2 = vx * vx + vy * vy + vz * vz;
			return pitch >= -90.0F && pitch <= 90.0F && speed2 <= MAX_SPEED * MAX_SPEED && (flags & ~REMOTE_KNOWN_FLAGS) == 0 && gameMode <= GAME_MODE_MAX;
		}
	}

	// ---- §7.10 REMOTE_PLAYER_LEAVE (v1.1) --------------------------------------------------------
	public record RemotePlayerLeave(int playerId, int reason) implements Payload {
		@Override
		public int type() {
			return MSG_REMOTE_PLAYER_LEAVE;
		}

		@Override
		public int payloadBytes() {
			return REMOTE_PLAYER_LEAVE_BYTES;
		}

		@Override
		public void write(MemorySegment s, long off) {
			s.set(I32, off, playerId);
			s.set(I32, off + 4, reason);
		}

		public static RemotePlayerLeave read(MemorySegment s, long off) {
			return new RemotePlayerLeave(s.get(I32, off), s.get(I32, off + 4));
		}

		public boolean valid() {
			return playerId != 0 && reason >= LEAVE_LEFT && reason <= LEAVE_RESET;
		}
	}

	// ---- §7.11 TERRAIN_REQUEST (v1.1) ------------------------------------------------------------
	public record TerrainRequest(int chunkX, int chunkZ, int requestId, int distance) implements Payload {
		@Override
		public int type() {
			return MSG_TERRAIN_REQUEST;
		}

		@Override
		public int payloadBytes() {
			return TERRAIN_REQUEST_BYTES;
		}

		@Override
		public void write(MemorySegment s, long off) {
			s.set(I32, off, chunkX);
			s.set(I32, off + 4, chunkZ);
			s.set(I32, off + 8, requestId);
			s.set(I16, off + 12, (short) distance);
			s.set(I16, off + 14, (short) 0);
		}

		public static TerrainRequest read(MemorySegment s, long off) {
			return new TerrainRequest(s.get(I32, off), s.get(I32, off + 4), s.get(I32, off + 8), Short.toUnsignedInt(s.get(I16, off + 12)));
		}

		public boolean valid() {
			return requestId != 0 && chunkOk(chunkX, chunkZ);
		}
	}

	// ---- §7.12 TERRAIN_PATCH (v1.1) --------------------------------------------------------------
	/**
	 * One chunk column of host ground. Arrays have {@link Proto#CHUNK_COLUMNS} entries, index
	 * {@code localZ * 16 + localX}. Records compare arrays by reference, so tests compare fields.
	 */
	public record TerrainPatch(int chunkX, int chunkZ, int requestId, int flags, short[] groundY, short[] waterY, byte[] material) implements Payload {
		private static final long GROUND_OFF = 16, WATER_OFF = 528, MATERIAL_OFF = 1040;

		public static int column(int localX, int localZ) {
			return localZ * 16 + localX;
		}

		@Override
		public int type() {
			return MSG_TERRAIN_PATCH;
		}

		@Override
		public int payloadBytes() {
			return TERRAIN_PATCH_BYTES;
		}

		@Override
		public void write(MemorySegment s, long off) {
			s.set(I32, off, chunkX);
			s.set(I32, off + 4, chunkZ);
			s.set(I32, off + 8, requestId);
			s.set(I32, off + 12, flags);
			for (int i = 0; i < CHUNK_COLUMNS; i++) {
				s.set(I16, off + GROUND_OFF + 2L * i, groundY[i]);
				s.set(I16, off + WATER_OFF + 2L * i, waterY[i]);
				s.set(U8, off + MATERIAL_OFF + i, material[i]);
			}
		}

		public static TerrainPatch read(MemorySegment s, long off) {
			short[] ground = new short[CHUNK_COLUMNS];
			short[] water = new short[CHUNK_COLUMNS];
			byte[] material = new byte[CHUNK_COLUMNS];
			MemorySegment.copy(s, I16, off + GROUND_OFF, ground, 0, CHUNK_COLUMNS);
			MemorySegment.copy(s, I16, off + WATER_OFF, water, 0, CHUNK_COLUMNS);
			MemorySegment.copy(s, U8, off + MATERIAL_OFF, material, 0, CHUNK_COLUMNS);
			return new TerrainPatch(s.get(I32, off), s.get(I32, off + 4), s.get(I32, off + 8), s.get(I32, off + 12), ground, water, material);
		}

		/** Surface material of a column; values this version doesn't know read as {@link Proto#MAT_UNKNOWN} (§7.12). */
		public int materialAt(int column) {
			int m = Byte.toUnsignedInt(material[column]);
			return m < MAT_COUNT ? m : MAT_UNKNOWN;
		}

		public boolean valid() {
			if (!chunkOk(chunkX, chunkZ) || groundY.length != CHUNK_COLUMNS || waterY.length != CHUNK_COLUMNS || material.length != CHUNK_COLUMNS) {
				return false;
			}
			for (int i = 0; i < CHUNK_COLUMNS; i++) {
				if (!heightOk(groundY[i], NO_GROUND) || !heightOk(waterY[i], NO_WATER)) {
					return false;
				}
			}
			return true;
		}
	}

	// ---- §7.13 SESSION_INFO (v1.1) ---------------------------------------------------------------
	public record SessionInfo(int flags, int port, int friends, int maxPlayers, int gameMode, String address) implements Payload {
		@Override
		public int type() {
			return MSG_SESSION_INFO;
		}

		@Override
		public int payloadBytes() {
			return SESSION_INFO_BYTES;
		}

		@Override
		public void write(MemorySegment s, long off) {
			s.asSlice(off, SESSION_INFO_BYTES).fill((byte) 0);
			s.set(I32, off, flags);
			s.set(I16, off + 4, (short) port);
			s.set(I16, off + 6, (short) friends);
			s.set(I16, off + 8, (short) maxPlayers);
			s.set(U8, off + 10, (byte) gameMode);
			byte[] text = Text.truncateUtf8(address, ADDRESS_MAX_BYTES);
			s.set(I16, off + 12, (short) text.length);
			MemorySegment.copy(MemorySegment.ofArray(text), 0, s, off + 16, text.length);
		}

		public static SessionInfo read(MemorySegment s, long off) {
			int addressBytes = Short.toUnsignedInt(s.get(I16, off + 12));
			String address = addressBytes <= ADDRESS_MAX_BYTES ? Text.readUtf8(s, off + 16, addressBytes) : null;
			return new SessionInfo(s.get(I32, off), Short.toUnsignedInt(s.get(I16, off + 4)), Short.toUnsignedInt(s.get(I16, off + 6)),
				Short.toUnsignedInt(s.get(I16, off + 8)), Byte.toUnsignedInt(s.get(U8, off + 10)), address);
		}

		public boolean valid() {
			return (flags & ~SESSION_KNOWN_FLAGS) == 0 && gameMode <= GAME_MODE_MAX && address != null;
		}
	}

	// ---- §7.14 TEST_PATTERN (test only) ----------------------------------------------------------
	public record TestPattern(long index) implements Payload {
		private static final int INDEX_MUL = 31;

		@Override
		public int type() {
			return MSG_TEST_PATTERN;
		}

		@Override
		public int payloadBytes() {
			return TEST_PATTERN_FIXED_BYTES + fillBytes(index);
		}

		public static int fillBytes(long index) {
			return (int) Long.remainderUnsigned(index, TEST_PATTERN_FILL_MODULUS);
		}

		private static byte fillByte(long index, int i) {
			return (byte) ((index * INDEX_MUL + i) & 0xFF);
		}

		@Override
		public void write(MemorySegment s, long off) {
			int n = fillBytes(index);
			int hash = Text.FNV_OFFSET_BASIS;
			for (int i = 0; i < n; i++) {
				byte b = fillByte(index, i);
				s.set(U8, off + TEST_PATTERN_FIXED_BYTES + i, b);
				hash = (hash ^ (b & 0xFF)) * Text.FNV_PRIME;
			}
			s.set(I64, off, index);
			s.set(I32, off + 8, n);
			s.set(I32, off + 12, hash);
		}

		/** Checks a received payload; returns its index, or -1 if anything is wrong. */
		public static long check(MemorySegment s, long off, int payloadBytes) {
			if (payloadBytes < TEST_PATTERN_FIXED_BYTES) {
				return -1;
			}
			long index = s.get(I64, off);
			int n = s.get(I32, off + 8);
			int checksum = s.get(I32, off + 12);
			if (n < 0 || n > TEST_PATTERN_MAX_FILL || TEST_PATTERN_FIXED_BYTES + n != payloadBytes) {
				return -1;
			}
			int hash = Text.FNV_OFFSET_BASIS;
			for (int i = 0; i < n; i++) {
				byte b = s.get(U8, off + TEST_PATTERN_FIXED_BYTES + i);
				if (b != fillByte(index, i)) {
					return -1;
				}
				hash = (hash ^ (b & 0xFF)) * Text.FNV_PRIME;
			}
			return hash == checksum && index >= 0 ? index : -1;
		}
	}

	// ---- §7.15 CAMERA (v1.2) ---------------------------------------------------------------------
	public record Camera(long frame, long timeUs, double x, double y, double z, float yaw, float pitch, float roll, float fovY, double feetX, double feetY,
		double feetZ, float bodyYaw, int flags, float nearClip, float farClip) implements Payload {
		@Override
		public int type() {
			return MSG_CAMERA;
		}

		@Override
		public int payloadBytes() {
			return CAMERA_BYTES;
		}

		@Override
		public void write(MemorySegment s, long off) {
			s.set(I64, off, frame);
			s.set(I64, off + 8, timeUs);
			s.set(F64, off + 16, x);
			s.set(F64, off + 24, y);
			s.set(F64, off + 32, z);
			s.set(F32, off + 40, yaw);
			s.set(F32, off + 44, pitch);
			s.set(F32, off + 48, roll);
			s.set(F32, off + 52, fovY);
			s.set(F64, off + 56, feetX);
			s.set(F64, off + 64, feetY);
			s.set(F64, off + 72, feetZ);
			s.set(F32, off + 80, bodyYaw);
			s.set(I32, off + 84, flags);
			s.set(F32, off + 88, nearClip);
			s.set(F32, off + 92, farClip);
		}

		public static Camera read(MemorySegment s, long off) {
			return new Camera(s.get(I64, off), s.get(I64, off + 8), s.get(F64, off + 16), s.get(F64, off + 24), s.get(F64, off + 32), s.get(F32, off + 40),
				s.get(F32, off + 44), s.get(F32, off + 48), s.get(F32, off + 52), s.get(F64, off + 56), s.get(F64, off + 64), s.get(F64, off + 72),
				s.get(F32, off + 80), s.get(I32, off + 84), s.get(F32, off + 88), s.get(F32, off + 92));
		}

		public boolean firstPerson() {
			return (flags & CAMERA_FIRST_PERSON) != 0;
		}

		public boolean passthrough() {
			return (flags & CAMERA_PASSTHROUGH) != 0;
		}

		public boolean valid() {
			if (!positionOk(x, y, z) || !positionOk(feetX, feetY, feetZ)) {
				return false;
			}
			if (!Float.isFinite(yaw) || !Float.isFinite(pitch) || !Float.isFinite(roll) || !Float.isFinite(fovY) || !Float.isFinite(bodyYaw)
				|| !Float.isFinite(nearClip) || !Float.isFinite(farClip)) {
				return false;
			}
			return pitch >= -90.0F && pitch <= 90.0F && fovY >= MIN_FOV && fovY <= MAX_FOV && (flags & ~CAMERA_KNOWN_FLAGS) == 0 && nearClip >= 0.0F
				&& farClip >= 0.0F;
		}
	}

	// ---- §7.16 VIEW (v1.2) -----------------------------------------------------------------------
	public record View(int width, int height, int hostWidth, int hostHeight) implements Payload {
		@Override
		public int type() {
			return MSG_VIEW;
		}

		@Override
		public int payloadBytes() {
			return VIEW_BYTES;
		}

		@Override
		public void write(MemorySegment s, long off) {
			s.set(I32, off, width);
			s.set(I32, off + 4, height);
			s.set(I32, off + 8, hostWidth);
			s.set(I32, off + 12, hostHeight);
		}

		public static View read(MemorySegment s, long off) {
			return new View(s.get(I32, off), s.get(I32, off + 4), s.get(I32, off + 8), s.get(I32, off + 12));
		}

		public boolean valid() {
			return width >= VIEW_MIN_SIDE && width <= VIEW_MAX_WIDTH && height >= VIEW_MIN_SIDE && height <= VIEW_MAX_HEIGHT
				&& (long) width * height <= VIEW_MAX_PIXELS && hostWidth >= 0 && hostWidth <= HOST_MAX_SIDE && hostHeight >= 0 && hostHeight <= HOST_MAX_SIDE;
		}
	}

	// ---- §7.17 INPUT (v1.2) ----------------------------------------------------------------------
	public record Input(int kind, int button, int down, int value) implements Payload {
		@Override
		public int type() {
			return MSG_INPUT;
		}

		@Override
		public int payloadBytes() {
			return INPUT_BYTES;
		}

		@Override
		public void write(MemorySegment s, long off) {
			s.set(U8, off, (byte) kind);
			s.set(U8, off + 1, (byte) button);
			s.set(U8, off + 2, (byte) down);
			s.set(U8, off + 3, (byte) value);
			s.set(I32, off + 4, 0);
		}

		public static Input read(MemorySegment s, long off) {
			return new Input(Byte.toUnsignedInt(s.get(U8, off)), Byte.toUnsignedInt(s.get(U8, off + 1)), Byte.toUnsignedInt(s.get(U8, off + 2)),
				s.get(U8, off + 3));
		}

		public boolean valid() {
			return switch (kind) {
				case INPUT_BUTTON -> button >= BUTTON_ATTACK && button <= BUTTON_CLOSE_SCREEN && (down == 0 || down == 1) && value == 0;
				case INPUT_SLOT -> button == 0 && down == 0 && value >= 0 && value < HOTBAR_SLOTS;
				case INPUT_SCROLL -> button == 0 && down == 0 && value != 0 && value >= -HOTBAR_SLOTS && value <= HOTBAR_SLOTS;
				default -> false;
			};
		}
	}

	// ---- §7.18 OWNER_STATE (v1.2) ----------------------------------------------------------------
	public record OwnerState(int held, int health, int food, int gameMode, float attackDamage, float attackCharge, int flags) implements Payload {
		@Override
		public int type() {
			return MSG_OWNER_STATE;
		}

		@Override
		public int payloadBytes() {
			return OWNER_STATE_BYTES;
		}

		@Override
		public void write(MemorySegment s, long off) {
			s.set(U8, off, (byte) held);
			s.set(U8, off + 1, (byte) health);
			s.set(U8, off + 2, (byte) food);
			s.set(U8, off + 3, (byte) gameMode);
			s.set(F32, off + 4, attackDamage);
			s.set(F32, off + 8, attackCharge);
			s.set(I32, off + 12, flags);
		}

		public static OwnerState read(MemorySegment s, long off) {
			return new OwnerState(Byte.toUnsignedInt(s.get(U8, off)), Byte.toUnsignedInt(s.get(U8, off + 1)), Byte.toUnsignedInt(s.get(U8, off + 2)),
				Byte.toUnsignedInt(s.get(U8, off + 3)), s.get(F32, off + 4), s.get(F32, off + 8), s.get(I32, off + 12));
		}

		public boolean valid() {
			return held >= HELD_EMPTY && held <= HELD_OTHER && health >= 0 && health <= 255 && food >= 0 && food <= MAX_FOOD && gameMode >= 0
				&& gameMode <= GAME_MODE_MAX && Float.isFinite(attackDamage) && attackDamage >= 0.0F && attackDamage <= MAX_ATTACK_DAMAGE
				&& Float.isFinite(attackCharge) && attackCharge >= 0.0F && attackCharge <= 1.0F && (flags & ~OWNER_KNOWN_FLAGS) == 0;
		}
	}

	// ---- §7.19 BLOCK_REGION_REQUEST (v1.3) --------------------------------------------------------
	public record BlockRegionRequest(int chunkX, int chunkZ, int requestId) implements Payload {
		@Override
		public int type() {
			return MSG_BLOCK_REGION_REQUEST;
		}

		@Override
		public int payloadBytes() {
			return BLOCK_REGION_REQUEST_BYTES;
		}

		@Override
		public void write(MemorySegment s, long off) {
			s.set(I32, off, chunkX);
			s.set(I32, off + 4, chunkZ);
			s.set(I32, off + 8, requestId);
			s.set(I32, off + 12, 0);
		}

		public static BlockRegionRequest read(MemorySegment s, long off) {
			return new BlockRegionRequest(s.get(I32, off), s.get(I32, off + 4), s.get(I32, off + 8));
		}

		public boolean valid() {
			return requestId != 0 && chunkOk(chunkX, chunkZ);
		}
	}

	/** UTF-8 helpers shared by HELLO and LOG. */
	public static final class Text {
		static final int FNV_OFFSET_BASIS = 0x811C9DC5;
		static final int FNV_PRIME = 0x01000193;

		private Text() {
		}

		/** UTF-8 bytes of {@code s}, cut to at most {@code max} bytes without splitting a character. */
		public static byte[] truncateUtf8(String s, int max) {
			byte[] all = (s == null ? "" : s).getBytes(StandardCharsets.UTF_8);
			if (all.length <= max) {
				return all;
			}
			int n = max;
			while (n > 0 && (all[n] & 0xC0) == 0x80) {
				n--;
			}
			byte[] out = new byte[n];
			System.arraycopy(all, 0, out, 0, n);
			return out;
		}

		static String readUtf8(MemorySegment s, long off, int bytes) {
			byte[] b = new byte[bytes];
			MemorySegment.copy(s, U8, off, b, 0, bytes);
			return new String(b, StandardCharsets.UTF_8);
		}
	}
}
