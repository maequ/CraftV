package dev.redcraft.link;

import static dev.redcraft.link.Proto.*;
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
		Path file = Path.of(System.getProperty("redcraft.goldenFile", "../protocol/golden/golden_vectors.txt"));
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

	static Map<String, Messages.Payload> documented() {
		Map<String, Messages.Payload> m = new LinkedHashMap<>();
		m.put("HELLO", Messages.Hello.of(ROLE_HOST, 4242, SESSION, "RedCraft-Golden"));
		m.put("HEARTBEAT", new Messages.Heartbeat(1_000_000L, 999_000L, 250L, 77));
		m.put("PLAYER_STATE", new Messages.PlayerState(12.5, -60.0, -3.25, 1.5F, 0.0F, -4.25F, 90.0F, -15.5F, PLAYER_ON_GROUND, 123456789L, 4321));
		m.put("BLOCK_SET", new Messages.BlockSet(-5, 64, 1024, 1, BLOCK_SET_ECHO, 9));
		m.put("BLOCK_BREAK_REQUEST", new Messages.BlockBreakRequest(3, 10, -61, -20, 1, 0));
		m.put("BLOCK_PLACE_REQUEST", new Messages.BlockPlaceRequest(4, 10, -61, -20, 1, 1));
		m.put("LOG", new Messages.Log(LOG_INFO, "hello from golden ✓"));
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
					assertEquals("RedCraft-Golden", m.software());
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
		assertFalse(new Messages.BlockSet(0, 0, 0, 1, 2, 0).valid());
	}

	@Test
	void textTruncatesOnUtf8Boundaries() {
		String s = "a".repeat(LOG_TEXT_MAX_BYTES - 1) + "✓";
		assertEquals(LOG_TEXT_MAX_BYTES - 1, Messages.Text.truncateUtf8(s, LOG_TEXT_MAX_BYTES).length);
		assertEquals(SOFTWARE_MAX_BYTES, Messages.Text.truncateUtf8("x".repeat(60), SOFTWARE_MAX_BYTES).length);
	}
}
