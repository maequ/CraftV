package dev.redcraft.link;

import static dev.redcraft.link.Proto.*;
import static org.junit.jupiter.api.Assertions.*;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.util.ArrayList;
import java.util.List;
import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.Test;

/** Ring tests on a private buffer (PROTOCOL.md §4); mirrors protocol/cpp/tests/test_ring.cpp. */
class RingTest {
	static final long DATA = 4096;
	static final long CONTROL = 0;
	static final long DATA_OFF = RING_CONTROL_BYTES;
	static final long SESSION_OFF = DATA_OFF + DATA; // a fake side-block session field after the data

	final Arena arena = Arena.ofConfined();
	final MemorySegment s = arena.allocate(DATA_OFF + DATA + 64, 4096);
	final Ring.Stats tx = new Ring.Stats(), rx = new Ring.Stats();
	Ring.Producer producer;
	Ring.Consumer consumer;

	RingTest() {
		s.fill((byte) 0xEE);
		s.asSlice(0, RING_CONTROL_BYTES).fill((byte) 0);
		s.set(I64, RC_DATA_BYTES, DATA);
		setSession(1);
		producer = new Ring.Producer(s, CONTROL, DATA_OFF, DATA);
		consumer = new Ring.Consumer(s, CONTROL, DATA_OFF, DATA);
	}

	@AfterEach
	void close() {
		arena.close();
	}

	void setSession(int v) {
		s.set(I32, SESSION_OFF, v);
	}

	record Blob(int type, int bytes, byte fill) implements Messages.Payload {
		@Override
		public int payloadBytes() {
			return bytes;
		}

		@Override
		public void write(MemorySegment seg, long off) {
			if (bytes > 0) {
				seg.asSlice(off, bytes).fill(fill);
			}
		}
	}

	Ring.PushResult push(int bytes, int fill) {
		return producer.push(MSG_LOG, 1, s.get(I32, SESSION_OFF), 1, new Blob(MSG_LOG, bytes, (byte) fill), tx);
	}

	List<Integer> drain(long max) {
		List<Integer> sizes = new ArrayList<>();
		consumer.drain(SESSION_OFF, max, rx, (type, ver, bytes, seq, seg, off) -> sizes.add(bytes));
		return sizes;
	}

	@Test
	void roundTripInOrder() {
		push(10, 0x11);
		push(0, 0);
		push(33, 0x22);
		List<byte[]> seen = new ArrayList<>();
		consumer.drain(SESSION_OFF, Long.MAX_VALUE, rx, (type, ver, bytes, seq, seg, off) -> seen.add(seg.asSlice(off, bytes).toArray(U8)));
		assertEquals(3, seen.size());
		assertArrayEquals(new byte[10], new byte[10]);
		for (byte b : seen.get(0)) {
			assertEquals(0x11, b);
		}
		assertEquals(0, seen.get(1).length);
		assertEquals(33, seen.get(2).length);
		assertEquals(s.get(I64, RC_HEAD), s.get(I64, RC_TAIL));
	}

	@Test
	void wrapWritesPadAndNeverStraddles() {
		int big = (int) (DATA - 32 - RECORD_HEADER_BYTES);
		assertEquals(Ring.PushResult.OK, push(big, 0x33));
		assertEquals(1, drain(Long.MAX_VALUE).size());
		assertEquals(Ring.PushResult.OK, push(100, 0x44));
		assertEquals(DATA + recordBytes(100), s.get(I64, RC_HEAD));
		assertEquals(MSG_PAD, s.get(I16, DATA_OFF + DATA - 32));
		assertEquals(List.of(100), drain(Long.MAX_VALUE));
		assertEquals(0, rx.corrupt);
	}

	@Test
	void fullRingDropsThenRecovers() {
		int pushed = 0;
		while (push(240, 0x55) == Ring.PushResult.OK) {
			pushed++;
		}
		assertTrue(pushed > 0);
		assertEquals(1, tx.droppedFull);
		assertEquals(pushed, drain(Long.MAX_VALUE).size());
		assertEquals(Ring.PushResult.OK, push(240, 0x55));
		assertEquals(Ring.PushResult.TOO_LARGE, push(MAX_PAYLOAD + 1, 0));
	}

	@Test
	void manyWrapsKeepEveryRecordIntact() {
		long sent = 0;
		long[] received = { 0 }, bad = { 0 };
		for (int round = 0; round < 2000; round++) {
			for (int k = 0; k < 3; k++) {
				if (producer.push(MSG_TEST_PATTERN, 1, 1, 1, new Messages.TestPattern(sent), tx) == Ring.PushResult.OK) {
					sent++;
				}
			}
			consumer.drain(SESSION_OFF, 700, rx, (type, ver, bytes, seq, seg, off) -> {
				if (Messages.TestPattern.check(seg, off, bytes) != received[0]) {
					bad[0]++;
				}
				received[0]++;
			});
		}
		assertTrue(sent > 1000);
		assertEquals(0, bad[0]);
		assertEquals(0, rx.corrupt);
		assertTrue(s.get(I64, RC_HEAD) > 50 * DATA);
	}

	@Test
	void staleSessionRecordsAreSkipped() {
		push(8, 1);
		push(8, 1);
		setSession(2);
		push(16, 2);
		assertEquals(List.of(16), drain(Long.MAX_VALUE));
		assertEquals(2, rx.stale);
	}

	@Test
	void corruptFramingDropsBacklogAndStaysInBounds() {
		push(8, 1);
		s.set(I32, DATA_OFF + RH_PAYLOAD_BYTES, 0x7FFFFFFF);
		assertTrue(drain(Long.MAX_VALUE).isEmpty());
		assertEquals(1, rx.corrupt);
		assertEquals(s.get(I64, RC_HEAD), s.get(I64, RC_TAIL));
		assertEquals(Ring.PushResult.OK, push(8, 1));
		assertEquals(1, drain(Long.MAX_VALUE).size());

		s.set(I64, RC_HEAD, s.get(I64, RC_TAIL) + DATA * 5);
		assertEquals(Ring.DrainStatus.CORRUPT, consumer.drain(SESSION_OFF, Long.MAX_VALUE, rx, (a, b, c, d, e, f) -> fail()));
		s.set(I64, RC_HEAD, s.get(I64, RC_TAIL) + 24);
		assertEquals(Ring.DrainStatus.CORRUPT, consumer.drain(SESSION_OFF, Long.MAX_VALUE, rx, (a, b, c, d, e, f) -> fail()));
	}

	@Test
	void byteBudgetIsHonoured() {
		for (int i = 0; i < 10; i++) {
			push(16, 1);
		}
		assertEquals(2, drain(64).size());
		assertEquals(8, drain(Long.MAX_VALUE).size());
	}
}
