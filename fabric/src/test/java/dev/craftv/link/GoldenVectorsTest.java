package dev.craftv.link;

import static dev.craftv.link.Proto.*;
import static org.junit.jupiter.api.Assertions.*;

import java.io.IOException;
import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.HexFormat;
import java.util.LinkedHashMap;
import java.util.Map;
import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.api.Test;

/**
 * Cross-language agreement (PROTOCOL.md §10): Java encodes the documented values and must produce
 * exactly the bytes in protocol/golden/golden_vectors.txt, which the C++ encoder wrote. Field values
 * are identical to protocol/cpp/tests/golden_values.cpp.
 */
class GoldenVectorsTest {
	static final int SESSION = 0x11223344;
	static final int SEQ = 7;
	static Map<String, String> golden;

	@BeforeAll
	static void load() throws IOException {
		Path file = Path.of(System.getProperty("craftv.goldenFile", "../protocol/golden/golden_vectors.txt"));
		golden = new LinkedHashMap<>();
		for (String line : Files.readAllLines(file, StandardCharsets.UTF_8)) {
			line = line.strip();
			if (line.isEmpty() || line.startsWith("#")) {
				continue;
			}
			String[] parts = line.split("\\s+");
			golden.put(parts[0], parts[1]);
		}
	}

	// v1.1 values, identical to golden_values.cpp (UUID 00010203-0405-0607-0809-0a0b0c0d0e0f).
	static final Messages.RemotePlayerJoin JOIN = new Messages.RemotePlayerJoin(117, 0, 0x0001020304050607L, 0x08090A0B0C0D0E0FL, "Steve_Friend");
	static final Messages.RemotePlayerState STATE = new Messages.RemotePlayerState(117, REMOTE_ON_GROUND | REMOTE_SPRINTING | REMOTE_SWING, 100.5, 71.0,
		-250.25, 5.5F, 0.0F, -1.25F, -45.5F, 12.25F, -40.0F, 9001, 1, 20);

	static Messages.TerrainPatch terrainPatch() {
		short[] ground = new short[CHUNK_COLUMNS];
		short[] water = new short[CHUNK_COLUMNS];
		byte[] material = new byte[CHUNK_COLUMNS];
		for (int i = 0; i < CHUNK_COLUMNS; i++) {
			ground[i] = (short) (60 + (i & 15) - (i >> 4));
			water[i] = i < 16 ? 75 : NO_WATER;
			material[i] = (byte) (i % 13);
		}
		ground[255] = NO_GROUND;
		return new Messages.TerrainPatch(-3, 7, 42, 0, ground, water, material);
	}

	static final Messages.Camera CAMERA = new Messages.Camera(123456L, 987654321L, -16.5, 31.625, 1447.25, 135.5F, -12.25F, 1.5F, 50.0F, -16.0, 29.625,
		1446.0, 130.0F, CAMERA_FIRST_PERSON | CAMERA_PASSTHROUGH, 0.25F, 10000.0F);

	static Map<String, Messages.Payload> documented() {
		Map<String, Messages.Payload> m = new LinkedHashMap<>();
		m.put("HELLO", Messages.Hello.of(ROLE_HOST, 4242, SESSION, "CraftV-Golden"));
		m.put("HEARTBEAT", new Messages.Heartbeat(1_000_000L, 999_000L, 250L, 77));
		m.put("PLAYER_STATE", new Messages.PlayerState(12.5, -60.0, -3.25, 1.5F, 0.0F, -4.25F, 90.0F, -15.5F, PLAYER_ON_GROUND, 123456789L, 4321));
		m.put("BLOCK_SET", new Messages.BlockSet(-5, 64, 1024, 1, BLOCK_SET_ECHO, 9));
		m.put("BLOCK_BREAK_REQUEST", new Messages.BlockBreakRequest(3, 10, -61, -20, 1, 0));
		m.put("BLOCK_PLACE_REQUEST", new Messages.BlockPlaceRequest(4, 10, -61, -20, 1, 1));
		m.put("LOG", new Messages.Log(LOG_INFO, "hello from golden ✓"));
		m.put("REMOTE_PLAYER_JOIN", JOIN);
		m.put("REMOTE_PLAYER_STATE", STATE);
		m.put("REMOTE_PLAYER_LEAVE", new Messages.RemotePlayerLeave(117, LEAVE_LEFT));
		m.put("TERRAIN_REQUEST", new Messages.TerrainRequest(-3, 7, 42, 2));
		m.put("TERRAIN_PATCH", terrainPatch());
		m.put("SESSION_INFO", new Messages.SessionInfo(SESSION_OPEN | SESSION_AUTH, 25565, 2, 8, 1, "192.168.1.23:25565"));
		m.put("CAMERA", CAMERA);
		m.put("VIEW", new Messages.View(1920, 1080, 2560, 1440));
		m.put("INPUT", new Messages.Input(INPUT_BUTTON, BUTTON_ATTACK, 1, 0));
		m.put("OWNER_STATE", new Messages.OwnerState(HELD_PICKAXE, 17, 18, 0, 5.0F, 0.75F, 0));
		m.put("BLOCK_REGION_REQUEST", new Messages.BlockRegionRequest(-3, 92, 77));
		m.put("WORLD_EVENT", new Messages.WorldEvent(EVENT_EXPLOSION, 4.0F, -14.5, 30.25, 1438.75, 12345));
		m.put("TEST_PATTERN", new Messages.TestPattern(5));
		return m;
	}

	/** Encodes one complete record exactly as a ring producer would. */
	static byte[] encodeRecord(Messages.Payload p) {
		try (Arena arena = Arena.ofConfined()) {
			int size = (int) recordBytes(p.payloadBytes());
			MemorySegment s = arena.allocate(size, 16);
			s.fill((byte) 0);
			s.set(I16, RH_TYPE, (short) p.type());
			s.set(I16, RH_TYPE_VERSION, (short) TYPE_VERSION_1);
			s.set(I32, RH_PAYLOAD_BYTES, p.payloadBytes());
			s.set(I32, RH_SESSION, SESSION);
			s.set(I32, RH_SEQ, SEQ);
			p.write(s, RECORD_HEADER_BYTES);
			return s.toArray(U8);
		}
	}

	@Test
	void everyGoldenVectorMatchesTheJavaEncoder() {
		Map<String, Messages.Payload> doc = documented();
		assertEquals(doc.keySet(), golden.keySet(), "same message set as the C++ goldens");
		for (var e : doc.entrySet()) {
			assertEquals(golden.get(e.getKey()), HexFormat.of().formatHex(encodeRecord(e.getValue())), e.getKey() + " differs from the C++ encoding");
		}
	}

	@Test
	void goldenBytesDecodeToTheDocumentedValues() {
		for (var e : golden.entrySet()) {
			MemorySegment s = MemorySegment.ofArray(HexFormat.of().parseHex(e.getValue()));
			assertEquals(SESSION, s.get(I32, RH_SESSION));
			assertEquals(SEQ, s.get(I32, RH_SEQ));
			assertEquals(0, s.byteSize() % RECORD_ALIGN);
			int bytes = s.get(I32, RH_PAYLOAD_BYTES);
			long p = RECORD_HEADER_BYTES;
			switch (e.getKey()) {
				case "HELLO" -> {
					var m = Messages.Hello.read(s, p);
					assertTrue(m.valid());
					assertEquals("CraftV-Golden", m.software());
					assertEquals(4242, m.pid());
				}
				case "PLAYER_STATE" -> {
					var m = Messages.PlayerState.read(s, p);
					assertTrue(m.valid());
					assertEquals(new Messages.PlayerState(12.5, -60.0, -3.25, 1.5F, 0.0F, -4.25F, 90.0F, -15.5F, PLAYER_ON_GROUND, 123456789L, 4321), m);
				}
				case "BLOCK_SET" -> assertEquals(new Messages.BlockSet(-5, 64, 1024, 1, BLOCK_SET_ECHO, 9), Messages.BlockSet.read(s, p));
				case "BLOCK_BREAK_REQUEST" -> assertEquals(new Messages.BlockBreakRequest(3, 10, -61, -20, 1, 0), Messages.BlockBreakRequest.read(s, p));
				case "BLOCK_PLACE_REQUEST" -> assertEquals(new Messages.BlockPlaceRequest(4, 10, -61, -20, 1, 1), Messages.BlockPlaceRequest.read(s, p));
				case "LOG" -> assertEquals("hello from golden ✓", Messages.Log.read(s, p).text());
				case "HEARTBEAT" -> assertEquals(new Messages.Heartbeat(1_000_000L, 999_000L, 250L, 77), Messages.Heartbeat.read(s, p));
				case "TEST_PATTERN" -> assertEquals(5, Messages.TestPattern.check(s, p, bytes));
				case "REMOTE_PLAYER_JOIN" -> {
					var m = Messages.RemotePlayerJoin.read(s, p);
					assertTrue(m.valid());
					assertEquals(JOIN, m);
					assertEquals("00010203-0405-0607-0809-0a0b0c0d0e0f", new java.util.UUID(m.uuidMsb(), m.uuidLsb()).toString());
				}
				case "REMOTE_PLAYER_STATE" -> {
					var m = Messages.RemotePlayerState.read(s, p);
					assertTrue(m.valid());
					assertEquals(STATE, m);
				}
				case "REMOTE_PLAYER_LEAVE" -> assertEquals(new Messages.RemotePlayerLeave(117, LEAVE_LEFT), Messages.RemotePlayerLeave.read(s, p));
				case "TERRAIN_REQUEST" -> assertEquals(new Messages.TerrainRequest(-3, 7, 42, 2), Messages.TerrainRequest.read(s, p));
				case "TERRAIN_PATCH" -> {
					var m = Messages.TerrainPatch.read(s, p);
					assertTrue(m.valid());
					var doc = terrainPatch();
					assertEquals(-3, m.chunkX());
					assertEquals(42, m.requestId());
					assertArrayEquals(doc.groundY(), m.groundY());
					assertArrayEquals(doc.waterY(), m.waterY());
					assertArrayEquals(doc.material(), m.material());
					assertEquals(75, m.groundY()[Messages.TerrainPatch.column(15, 0)]);
				}
				case "SESSION_INFO" -> {
					var m = Messages.SessionInfo.read(s, p);
					assertTrue(m.valid());
					assertEquals("192.168.1.23:25565", m.address());
					assertEquals(25565, m.port());
				}
				case "CAMERA" -> {
					var m = Messages.Camera.read(s, p);
					assertTrue(m.valid());
					assertEquals(CAMERA, m);
				}
				case "VIEW" -> assertEquals(new Messages.View(1920, 1080, 2560, 1440), Messages.View.read(s, p));
				case "INPUT" -> assertEquals(new Messages.Input(INPUT_BUTTON, BUTTON_ATTACK, 1, 0), Messages.Input.read(s, p));
				case "OWNER_STATE" -> assertEquals(new Messages.OwnerState(HELD_PICKAXE, 17, 18, 0, 5.0F, 0.75F, 0), Messages.OwnerState.read(s, p));
				case "BLOCK_REGION_REQUEST" -> assertEquals(new Messages.BlockRegionRequest(-3, 92, 77), Messages.BlockRegionRequest.read(s, p));
				case "WORLD_EVENT" -> {
					var m = Messages.WorldEvent.read(s, p);
					assertTrue(m.valid());
					assertEquals(new Messages.WorldEvent(EVENT_EXPLOSION, 4.0F, -14.5, 30.25, 1438.75, 12345), m);
				}
				default -> fail("unexpected golden " + e.getKey());
			}
		}
	}

	@Test
	void validationRejectsBadValues() {
		var ok = new Messages.PlayerState(1, 64, 1, 0, 0, 0, 0, 0, 0, 1, 1);
		assertTrue(ok.valid());
		assertFalse(new Messages.PlayerState(Double.NaN, 64, 1, 0, 0, 0, 0, 0, 0, 1, 1).valid());
		assertFalse(new Messages.PlayerState(1, 64, 1, 0, 0, 0, 0, 91, 0, 1, 1).valid());
		assertFalse(new Messages.PlayerState(1, 5000, 1, 0, 0, 0, 0, 0, 0, 1, 1).valid());
		assertFalse(new Messages.PlayerState(1, 64, 1, 2000, 0, 0, 0, 0, 0, 1, 1).valid());
		assertFalse(new Messages.PlayerState(1, 64, 1, 0, 0, 0, 0, 0, 0x80, 1, 1).valid());
		assertFalse(new Messages.BlockBreakRequest(1, 0, 0, 0, 6, 0).valid());
		assertTrue(new Messages.BlockBreakRequest(1, 0, 0, 0, FACE_UNKNOWN, 0).valid());
		assertFalse(new Messages.BlockBreakRequest(0, 0, 0, 0, 1, 0).valid());
		assertFalse(new Messages.BlockSet(0, 0, 0, 1, 1 << 4, 0).valid());
		assertTrue(new Messages.BlockSet(0, 0, 0, 1, BLOCK_SET_SOLID | BLOCK_SET_REGION, 7).valid());
	}

	@Test
	void v11ValidationAndDirections() {
		assertFalse(new Messages.RemotePlayerState(0, 0, 0, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20).valid());
		assertFalse(new Messages.RemotePlayerState(1, 1 << 10, 0, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20).valid());
		assertFalse(new Messages.RemotePlayerState(1, 0, 0, 64, 0, 0, 0, 0, 0, 0, Float.NaN, 0, 0, 20).valid());
		assertFalse(new Messages.RemotePlayerState(1, 0, 0, 64, 0, 0, 0, 0, 0, 0, 0, 0, 4, 20).valid());
		assertFalse(new Messages.RemotePlayerLeave(117, 3).valid());
		assertFalse(new Messages.TerrainRequest(MAX_CHUNK_COORD + 1, 0, 1, 0).valid());
		assertFalse(new Messages.TerrainRequest(0, 0, 0, 0).valid());
		var patch = terrainPatch();
		patch.material()[3] = (byte) 200;
		assertTrue(patch.valid(), "unknown materials are not malformed");
		assertEquals(MAT_UNKNOWN, patch.materialAt(3));
		patch.groundY()[100] = 5000;
		assertFalse(patch.valid());
		assertFalse(new Messages.SessionInfo(8, 0, 0, 0, 0, "").valid());
		assertTrue(allowedFrom(MSG_TERRAIN_PATCH, ROLE_HOST));
		assertFalse(allowedFrom(MSG_TERRAIN_PATCH, ROLE_MC));
		assertTrue(allowedFrom(MSG_REMOTE_PLAYER_STATE, ROLE_MC));
		assertFalse(allowedFrom(MSG_REMOTE_PLAYER_STATE, ROLE_HOST));
		assertFalse(allowedFrom(MSG_PLAYER_STATE, ROLE_MC));
	}

	@Test
	void textTruncatesOnUtf8Boundaries() {
		String s = "a".repeat(LOG_TEXT_MAX_BYTES - 1) + "✓";
		assertEquals(LOG_TEXT_MAX_BYTES - 1, Messages.Text.truncateUtf8(s, LOG_TEXT_MAX_BYTES).length);
		assertEquals(SOFTWARE_MAX_BYTES, Messages.Text.truncateUtf8("x".repeat(60), SOFTWARE_MAX_BYTES).length);
	}

	@Test
	void v12ValidationAndDirections() {
		assertTrue(CAMERA.valid());
		assertFalse(new Messages.Camera(1, 1, 0, 64, 0, 0, 0, 0, 0.5F, 0, 64, 0, 0, 0, 0, 0).valid(), "fov below 1");
		assertFalse(new Messages.Camera(1, 1, 0, 64, 0, 0, 91, 0, 70, 0, 64, 0, 0, 0, 0, 0).valid(), "pitch above 90");
		assertFalse(new Messages.Camera(1, 1, 0, 64, 0, 0, 0, 0, 70, 0, 64, 0, 0, 1 << 5, 0, 0).valid(), "unknown flag");
		assertFalse(new Messages.Camera(1, 1, 0, 64, 0, 0, 0, Float.NaN, 70, 0, 64, 0, 0, 0, 0, 0).valid());
		assertFalse(new Messages.View(3840, 2160, 3840, 2160).valid(), "more pixels than VIEW_MAX_PIXELS");
		assertTrue(new Messages.View(2560, 1080, 5120, 2160).valid());
		assertTrue(new Messages.Input(INPUT_SLOT, 0, 0, 8).valid());
		assertFalse(new Messages.Input(INPUT_SLOT, 0, 0, 9).valid());
		assertTrue(new Messages.Input(INPUT_SCROLL, 0, 0, -1).valid());
		assertFalse(new Messages.Input(INPUT_SCROLL, 0, 0, 0).valid());
		assertTrue(new Messages.Input(INPUT_BUTTON, BUTTON_SNEAK, 1, 0).valid()); // v1.5
		assertFalse(new Messages.Input(INPUT_BUTTON, 9, 1, 0).valid());
		assertFalse(new Messages.OwnerState(9, 20, 20, 0, 1, 1, 0).valid());
		assertTrue(new Messages.OwnerState(HELD_LIGHT, 20, 20, 0, 1, 1, 0).valid());
		assertFalse(new Messages.OwnerState(HELD_SWORD, 20, 20, 0, 1, 1.5F, 0).valid());
		assertTrue(allowedFrom(MSG_CAMERA, ROLE_HOST));
		assertFalse(allowedFrom(MSG_CAMERA, ROLE_MC));
		assertTrue(allowedFrom(MSG_OWNER_STATE, ROLE_MC));
		assertFalse(allowedFrom(MSG_OWNER_STATE, ROLE_HOST));
	}

	@Test
	void v14ValidationAndDirections() {
		assertTrue(allowedFrom(MSG_WORLD_EVENT, ROLE_MC));
		assertFalse(allowedFrom(MSG_WORLD_EVENT, ROLE_HOST));
		assertTrue(new Messages.WorldEvent(EVENT_SHOT, 1, 0, 64, 0, 0).valid()); // v1.5
		assertFalse(new Messages.WorldEvent(4, 1, 0, 64, 0, 0).valid());
		assertFalse(new Messages.WorldEvent(EVENT_EXPLOSION, -1, 0, 64, 0, 0).valid());
		assertTrue(new Messages.Input(INPUT_DAMAGE, DAMAGE_BULLET, 0, 6).valid());
		assertFalse(new Messages.Input(INPUT_DAMAGE, DAMAGE_BULLET, 0, 0).valid());
		assertTrue(new Messages.Input(INPUT_OPTION, OPTION_CROSSHAIR, 0, 1).valid());
		assertFalse(new Messages.Input(INPUT_OPTION, OPTION_MAX + 1, 0, 1).valid());
		assertTrue(new Messages.BlockSet(0, 0, 0, 1, BLOCK_SET_LIGHT | BLOCK_SET_REGION, 3).valid());
	}
}
