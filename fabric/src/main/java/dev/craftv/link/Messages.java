package dev.craftv.link;

import static dev.craftv.link.Proto.*;

import java.lang.foreign.MemorySegment;
import java.nio.charset.StandardCharsets;

/**
 * The v1.0 message payloads (PROTOCOL.md §7) as records that read themselves from and write
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
			return blockCoordsOk(x, y, z) && (flags & ~BLOCK_SET_ECHO) == 0;
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

	// ---- §7.7 TEST_PATTERN (test only) ----------------------------------------------------------
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
