package dev.craftv.link;

import java.lang.foreign.ValueLayout;
import java.lang.invoke.VarHandle;
import java.nio.ByteOrder;

/**
 * Constants and byte offsets of the CraftV shared-memory protocol v1.1.
 *
 * <p>Source of truth: docs/PROTOCOL.md. C++ mirror: protocol/cpp/include/craftv/protocol.h.
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
	/** Big-endian u64: only for the UUID halves in REMOTE_PLAYER_JOIN (§7.8, most significant byte first). */
	public static final ValueLayout.OfLong BE64 = ValueLayout.JAVA_LONG_UNALIGNED.withOrder(ByteOrder.BIG_ENDIAN);
	/** Aligned u32/u64 handles for the acquire/release fields (PROTOCOL.md §6). */
	public static final VarHandle ATOMIC_INT = ValueLayout.JAVA_INT.withOrder(ByteOrder.LITTLE_ENDIAN).varHandle();
	public static final VarHandle ATOMIC_LONG = ValueLayout.JAVA_LONG.withOrder(ByteOrder.LITTLE_ENDIAN).varHandle();

	// ---- identity (§2, §3) ----------------------------------------------------------------------
	public static final int MAGIC = 0x56465243; // bytes 43 52 46 56 = "CRFV"
	public static final int VERSION_MAJOR = 1;
	public static final int VERSION_MINOR = 3;
	public static final String DEFAULT_MAPPING_NAME = "Local\\CraftV_Shared_v1";

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

	/** May a side with this role send this message type? (§7 "Dir" column; unknown types pass and are skipped later.) */
	public static boolean allowedFrom(int type, int senderRole) {
		if (senderRole != ROLE_HOST && senderRole != ROLE_MC) {
			return false;
		}
		return switch (type) {
			case MSG_PLAYER_STATE, MSG_TERRAIN_PATCH, MSG_CAMERA, MSG_VIEW, MSG_INPUT, MSG_BLOCK_REGION_REQUEST -> senderRole == ROLE_HOST; // §7.3: MC -> host is reserved
			case MSG_REMOTE_PLAYER_JOIN, MSG_REMOTE_PLAYER_STATE, MSG_REMOTE_PLAYER_LEAVE, MSG_TERRAIN_REQUEST, MSG_SESSION_INFO, MSG_OWNER_STATE ->
				senderRole == ROLE_MC;
			default -> true;
		};
	}

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
	public static final int MSG_REMOTE_PLAYER_JOIN = 8; // v1.1 (§7.8-7.13)
	public static final int MSG_REMOTE_PLAYER_STATE = 9;
	public static final int MSG_REMOTE_PLAYER_LEAVE = 10;
	public static final int MSG_TERRAIN_REQUEST = 11;
	public static final int MSG_TERRAIN_PATCH = 12;
	public static final int MSG_SESSION_INFO = 13;
	public static final int MSG_CAMERA = 14; // v1.2 (§7.15-7.18)
	public static final int MSG_VIEW = 15;
	public static final int MSG_INPUT = 16;
	public static final int MSG_OWNER_STATE = 17;
	public static final int MSG_BLOCK_REGION_REQUEST = 18; // v1.3 (§7.19)
	public static final int MSG_TEST_PATTERN = 0x7F00;
	public static final int TYPE_VERSION_1 = 1;

	public static final int HELLO_BYTES = 64;
	public static final int HEARTBEAT_BYTES = 32;
	public static final int PLAYER_STATE_BYTES = 64;
	public static final int BLOCK_SET_BYTES = 24;
	public static final int BLOCK_REQUEST_BYTES = 24;
	public static final int LOG_BYTES = 264;
	public static final int REMOTE_PLAYER_JOIN_BYTES = 64;
	public static final int REMOTE_PLAYER_STATE_BYTES = 64;
	public static final int REMOTE_PLAYER_LEAVE_BYTES = 8;
	public static final int TERRAIN_REQUEST_BYTES = 16;
	public static final int TERRAIN_PATCH_BYTES = 1296;
	public static final int SESSION_INFO_BYTES = 96;
	public static final int CAMERA_BYTES = 96;
	public static final int VIEW_BYTES = 16;
	public static final int INPUT_BYTES = 8;
	public static final int OWNER_STATE_BYTES = 16;
	public static final int BLOCK_REGION_REQUEST_BYTES = 16;

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
	public static final int BLOCK_SET_SOLID = 1 << 1, BLOCK_SET_REGION = 1 << 2; // v1.3 (§7.19)
	public static final int BLOCK_SET_KNOWN_FLAGS = BLOCK_SET_ECHO | BLOCK_SET_SOLID | BLOCK_SET_REGION;
	public static final int FACE_MAX = 5;
	public static final int FACE_UNKNOWN = 0xFF;
	public static final int LOG_TRACE = 0, LOG_DEBUG = 1, LOG_INFO = 2, LOG_WARN = 3, LOG_ERROR = 4;
	public static final int LOG_TEXT_MAX_BYTES = 256;

	// §7.8-7.10 friends
	public static final int PLAYER_NAME_MAX_BYTES = 32;
	public static final int UUID_BYTES = 16;
	public static final int REMOTE_ON_GROUND = 1, REMOTE_CROUCHING = 1 << 1, REMOTE_SPRINTING = 1 << 2, REMOTE_SWIMMING = 1 << 3, REMOTE_GLIDING = 1 << 4,
		REMOTE_FLYING = 1 << 5, REMOTE_IN_WATER = 1 << 6, REMOTE_SWING = 1 << 7, REMOTE_SLEEPING = 1 << 8, REMOTE_RIDING = 1 << 9;
	public static final int REMOTE_KNOWN_FLAGS = (1 << 10) - 1;
	public static final int GAME_MODE_MAX = 3; // 0 survival, 1 creative, 2 adventure, 3 spectator
	public static final int LEAVE_LEFT = 0, LEAVE_OTHER_DIMENSION = 1, LEAVE_RESET = 2;
	// §7.11-7.12 terrain
	public static final int MAX_CHUNK_COORD = 1_875_000;
	public static final int TERRAIN_MAX_IN_FLIGHT = 32;
	public static final long TERRAIN_RETRY_MS = 5_000;
	public static final int CHUNK_COLUMNS = 256; // index = localZ * 16 + localX
	public static final short NO_GROUND = Short.MIN_VALUE;
	public static final short NO_WATER = Short.MIN_VALUE;
	public static final int MAT_UNKNOWN = 0, MAT_GRASS = 1, MAT_DIRT = 2, MAT_SAND = 3, MAT_ROCK = 4, MAT_ROAD = 5, MAT_PAVEMENT = 6, MAT_GRAVEL = 7,
		MAT_SNOW = 8, MAT_WOOD = 9, MAT_METAL = 10, MAT_BUILDING = 11, MAT_MUD = 12;
	public static final int MAT_COUNT = 13;
	// §7.13 session
	public static final int SESSION_OPEN = 1, SESSION_AUTH = 1 << 1, SESSION_WHITELIST = 1 << 2;
	public static final int SESSION_KNOWN_FLAGS = SESSION_OPEN | SESSION_AUTH | SESSION_WHITELIST;
	public static final int ADDRESS_MAX_BYTES = 64;
	// §7.15-7.18 passthrough (v1.2)
	public static final int CAMERA_FIRST_PERSON = 1, CAMERA_PASSTHROUGH = 1 << 1, CAMERA_IN_VEHICLE = 1 << 2;
	public static final int CAMERA_KNOWN_FLAGS = CAMERA_FIRST_PERSON | CAMERA_PASSTHROUGH | CAMERA_IN_VEHICLE;
	public static final float MIN_FOV = 1.0F, MAX_FOV = 179.0F;
	public static final int VIEW_MIN_SIDE = 64, VIEW_MAX_WIDTH = 3840, VIEW_MAX_HEIGHT = 2160, HOST_MAX_SIDE = 16384;
	public static final long VIEW_MAX_PIXELS = 2560L * 1440;
	public static final int INPUT_BUTTON = 1, INPUT_SLOT = 2, INPUT_SCROLL = 3;
	public static final int BUTTON_ATTACK = 1, BUTTON_USE = 2, BUTTON_PICK = 3, BUTTON_DROP = 4, BUTTON_INVENTORY = 5, BUTTON_SWAP_HANDS = 6,
		BUTTON_CLOSE_SCREEN = 7;
	public static final int HOTBAR_SLOTS = 9;
	public static final int HELD_EMPTY = 0, HELD_SWORD = 1, HELD_AXE = 2, HELD_PICKAXE = 3, HELD_SHOVEL = 4, HELD_HOE = 5, HELD_BLOCK = 6, HELD_OTHER = 7;
	public static final int OWNER_DEAD = 1, OWNER_SCREEN_OPEN = 1 << 1, OWNER_KNOWN_FLAGS = OWNER_DEAD | OWNER_SCREEN_OPEN;
	public static final int MAX_FOOD = 20;
	public static final float MAX_ATTACK_DAMAGE = 1000.0F;
	// §11 the frame mapping (v1.2)
	public static final String FRAME_MAPPING_NAME = "Local\\CraftV_Frame_v1";
	public static final int FRAME_MAGIC = 0x52465643; // bytes 43 56 46 52 = "CVFR"
	public static final int FRAME_VERSION = 1;
	public static final int FRAME_HEADER_BYTES = 4096;
	public static final int FRAME_SLOT_DESC_OFFSET = 256;
	public static final int FRAME_SLOT_DESC_BYTES = 128;
	public static final int FRAME_SLOTS = 3;
	public static final long FRAME_LAYER_MAX_BYTES = VIEW_MAX_PIXELS * 4;
	public static final long FRAME_SLOT_STRIDE = FRAME_LAYER_MAX_BYTES * 3;
	public static final int FRAME_DEPTH_ZERO_TO_ONE = 1, FRAME_BOTTOM_UP = 1 << 1, FRAME_REVERSED_Z = 1 << 2;

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
