package dev.redcraft.link;

import java.lang.foreign.ValueLayout;
import java.lang.invoke.VarHandle;
import java.nio.ByteOrder;

/**
 * Constants and byte offsets of the RedCraft shared-memory protocol v1.0.
 *
 * <p>Source of truth: docs/PROTOCOL.md. C++ mirror: protocol/cpp/include/redcraft/protocol.h.
 * {@code ProtoLayoutTest} pins these against the documented offsets; the golden vectors pin the
 * encoding against the C++ side. Change all of them together (PROTOCOL.md §9).
 */
public final class Proto {
	private Proto() {
	}

	// ---- byte access (PROTOCOL.md §1: little-endian) -----------------------------------------
	public static final ValueLayout.OfByte U8 = ValueLayout.JAVA_BYTE;
	public static final ValueLayout.OfShort I16 = ValueLayout.JAVA_SHORT_UNALIGNED.withOrder(ByteOrder.LITTLE_ENDIAN);
	public static final ValueLayout.OfInt I32 = ValueLayout.JAVA_INT_UNALIGNED.withOrder(ByteOrder.LITTLE_ENDIAN);
	public static final ValueLayout.OfLong I64 = ValueLayout.JAVA_LONG_UNALIGNED.withOrder(ByteOrder.LITTLE_ENDIAN);
	public static final ValueLayout.OfFloat F32 = ValueLayout.JAVA_FLOAT_UNALIGNED.withOrder(ByteOrder.LITTLE_ENDIAN);
	public static final ValueLayout.OfDouble F64 = ValueLayout.JAVA_DOUBLE_UNALIGNED.withOrder(ByteOrder.LITTLE_ENDIAN);
	/** Aligned u32/u64 handles for the acquire/release fields (PROTOCOL.md §6). */
	public static final VarHandle ATOMIC_INT = ValueLayout.JAVA_INT.withOrder(ByteOrder.LITTLE_ENDIAN).varHandle();
	public static final VarHandle ATOMIC_LONG = ValueLayout.JAVA_LONG.withOrder(ByteOrder.LITTLE_ENDIAN).varHandle();

	// ---- identity (§2, §3) ----------------------------------------------------------------------
	public static final int MAGIC = 0x52434452; // bytes 52 44 43 52 = "RDCR"
	public static final int VERSION_MAJOR = 1;
	public static final int VERSION_MINOR = 0;
	public static final String DEFAULT_MAPPING_NAME = "Local\\RedCraft_Shared_v1";

	public static final int ROLE_NONE = 0;
	public static final int ROLE_HOST = 1;
	public static final int ROLE_MC = 2;

	// ---- region map (§2.1) ----------------------------------------------------------------------
	public static final long HEADER_BYTES = 0x1000;
	public static final long SECTION_ALIGN = 0x1000;
	public static final long RING_CONTROL_BYTES = 0x1000;
	public static final long RING_DATA_BYTES = 0x100000;
	public static final long RING_SECTION_BYTES = RING_CONTROL_BYTES + RING_DATA_BYTES;
	public static final long OFF_RING_HOST_TO_MC = 0x1000;
	public static final long OFF_RING_MC_TO_HOST = OFF_RING_HOST_TO_MC + RING_SECTION_BYTES;
	public static final long MAPPING_BYTES = OFF_RING_MC_TO_HOST + RING_SECTION_BYTES;
	public static final int SECTION_COUNT = 2;
	public static final int MAX_SECTIONS = 16;
	public static final long MIN_RING_DATA_BYTES = 64L * 1024;
	public static final long MAX_RING_DATA_BYTES = 64L * 1024 * 1024;
	public static final int SECTION_RING_HOST_TO_MC = 1;
	public static final int SECTION_RING_MC_TO_HOST = 2;

	// ---- header (§3) ------------------------------------------------------------------------------
	public static final long H_MAGIC = 0x000;
	public static final long H_VERSION_MAJOR = 0x004;
	public static final long H_VERSION_MINOR = 0x006;
	public static final long H_HEADER_BYTES = 0x008;
	public static final long H_SECTION_COUNT = 0x00C;
	public static final long H_MAPPING_BYTES = 0x010;
	public static final long H_CREATOR_ROLE = 0x018;
	public static final long H_CREATOR_PID = 0x01C;
	public static final long OFF_HOST_BLOCK = 0x040;
	public static final long OFF_MC_BLOCK = 0x080;
	public static final long OFF_SECTION_TABLE = 0x100;

	// side block (§3.1), relative to OFF_HOST_BLOCK / OFF_MC_BLOCK
	public static final long SB_PID = 0x00;
	public static final long SB_SESSION = 0x04;
	public static final long SB_HEARTBEAT = 0x08;
	public static final long SB_HEARTBEAT_US = 0x10;
	public static final long SB_STATE = 0x18;
	public static final long SIDE_BLOCK_BYTES = 64;
	public static final int SIDE_ATTACHED = 1;
	public static final int SIDE_IN_GAME = 1 << 1;

	// section entry (§3.2)
	public static final long SECTION_ENTRY_BYTES = 32;
	public static final long SE_ID = 0x00;
	public static final long SE_FLAGS = 0x04;
	public static final long SE_OFFSET = 0x08;
	public static final long SE_BYTES = 0x10;

	// ---- ring control (§4.1) -------------------------------------------------------------------
	public static final long RC_HEAD = 0x00;
	public static final long RC_TAIL = 0x40;
	public static final long RC_DATA_BYTES = 0x80;
	public static final long RC_PRODUCER_ROLE = 0x88;

	// ---- records (§4.2) ----------------------------------------------------------------------------
	public static final int RECORD_ALIGN = 16;
	public static final int RECORD_HEADER_BYTES = 16;
	public static final int MAX_PAYLOAD = 4096;
	public static final long RH_TYPE = 0;
	public static final long RH_TYPE_VERSION = 2;
	public static final long RH_PAYLOAD_BYTES = 4;
	public static final long RH_SESSION = 8;
	public static final long RH_SEQ = 12;

	public static long recordBytes(long payloadBytes) {
		return (RECORD_HEADER_BYTES + payloadBytes + (RECORD_ALIGN - 1)) & ~(long) (RECORD_ALIGN - 1);
	}

	// ---- message types (§7) ----------------------------------------------------------------------
	public static final int MSG_PAD = 0;
	public static final int MSG_HELLO = 1;
	public static final int MSG_HEARTBEAT = 2;
	public static final int MSG_PLAYER_STATE = 3;
	public static final int MSG_BLOCK_SET = 4;
	public static final int MSG_BLOCK_BREAK_REQUEST = 5;
	public static final int MSG_BLOCK_PLACE_REQUEST = 6;
	public static final int MSG_LOG = 7;
	public static final int MSG_TEST_PATTERN = 0x7F00;
	public static final int TYPE_VERSION_1 = 1;

	public static final int HELLO_BYTES = 64;
	public static final int HEARTBEAT_BYTES = 32;
	public static final int PLAYER_STATE_BYTES = 64;
	public static final int BLOCK_SET_BYTES = 24;
	public static final int BLOCK_REQUEST_BYTES = 24;
	public static final int LOG_BYTES = 264;

	public static final int SOFTWARE_MAX_BYTES = 40;
	public static final int PLAYER_ON_GROUND = 1;
	public static final int PLAYER_TELEPORT = 1 << 1;
	public static final int PLAYER_KNOWN_FLAGS = PLAYER_ON_GROUND | PLAYER_TELEPORT;
	public static final double MAX_HORIZONTAL_COORD = 30_000_000.0;
	public static final double MIN_Y = -2048.0;
	public static final double MAX_Y = 4096.0;
	public static final float MAX_SPEED = 1000.0F;
	public static final int BLOCK_AIR = 0;
	public static final int BLOCK_SET_ECHO = 1;
	public static final int FACE_MAX = 5;
	public static final int FACE_UNKNOWN = 0xFF;
	public static final int LOG_TRACE = 0, LOG_DEBUG = 1, LOG_INFO = 2, LOG_WARN = 3, LOG_ERROR = 4;
	public static final int LOG_TEXT_MAX_BYTES = 256;

	public static final int TEST_PATTERN_FIXED_BYTES = 16;
	public static final int TEST_PATTERN_MAX_FILL = 256;
	public static final int TEST_PATTERN_FILL_MODULUS = TEST_PATTERN_MAX_FILL + 1;

	// ---- timing (§5.4) ---------------------------------------------------------------------------
	public static final long HEARTBEAT_PERIOD_MS = 100;
	public static final long HOST_TIMEOUT_MS = 10_000;
	public static final long MC_TIMEOUT_MS = 3_000;
	public static final long HEARTBEAT_MSG_PERIOD_MS = 500;
	public static final long INIT_TIMEOUT_MS = 2_000;
	public static final long RETRY_MS = 1_000;
	public static final long INVALID_RETRY_MS = 5_000;
	public static final long MAX_DRAIN_BYTES_PER_TICK = 256L * 1024;
}
