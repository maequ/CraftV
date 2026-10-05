package dev.craftv.link;

import static dev.craftv.link.Proto.*;
import static org.junit.jupiter.api.Assertions.*;

import java.time.Duration;
import java.util.concurrent.atomic.AtomicLong;
import org.junit.jupiter.api.Test;

/**
 * Brief §5.4 stress test, Java side: 1,000,000 TEST_PATTERN records through EACH ring of a real
 * named mapping (through FFM), both rings at once, producer and consumer on separate threads.
 * A timeout fails the test instead of hanging on a deadlock.
 */
class RingStressTest {
	static final long MESSAGES = 1_000_000;
	static final int SESSION = 42;

	@Test
	void oneMillionRecordsThroughEachRing() {
		assertTimeoutPreemptively(Duration.ofSeconds(120), () -> {
			String name = "Local\\CraftV_Test_jstress_" + ProcessHandle.current().pid();
			Win32.MappedView view = Win32.createOrOpenMapping(name, MAPPING_BYTES);
			assertTrue(view.ok(), view.error());
			try {
				var s = view.base();
				Layout.initialize(s, ROLE_HOST, 1);
				Layout.Resolved layout = Layout.validate(s, view.viewBytes());
				// The ring checks records against the producer's session field: point both at host's.
				s.set(I32, OFF_HOST_BLOCK + SB_SESSION, SESSION);
				long sessionOff = OFF_HOST_BLOCK + SB_SESSION;

				AtomicLong[] errors = { new AtomicLong(), new AtomicLong() };
				AtomicLong[] received = { new AtomicLong(), new AtomicLong() };
				Layout.RingLocation[] rings = { layout.hostToMc(), layout.mcToHost() };
				Thread[] threads = new Thread[4];
				for (int r = 0; r < 2; r++) {
					Layout.RingLocation loc = rings[r];
					int ri = r;
					threads[r * 2] = new Thread(() -> {
						Ring.Producer p = new Ring.Producer(s, loc.control(), loc.data(), loc.dataBytes());
						Ring.Stats st = new Ring.Stats();
						for (long i = 0; i < MESSAGES;) {
							if (p.push(MSG_TEST_PATTERN, 1, SESSION, (int) (i + 1), new Messages.TestPattern(i), st) == Ring.PushResult.OK) {
								i++;
							} else {
								Thread.onSpinWait();
							}
						}
					}, "producer-" + r);
					threads[r * 2 + 1] = new Thread(() -> {
						Ring.Consumer c = new Ring.Consumer(s, loc.control(), loc.data(), loc.dataBytes());
						Ring.Stats st = new Ring.Stats();
						long[] expected = { 0 };
						while (expected[0] < MESSAGES) {
							Ring.DrainStatus ds = c.drain(sessionOff, MAX_DRAIN_BYTES_PER_TICK, st, (type, ver, bytes, seq, seg, off) -> {
								if (type != MSG_TEST_PATTERN || Messages.TestPattern.check(seg, off, bytes) != expected[0] || seq != (int) (expected[0] + 1)) {
									errors[ri].incrementAndGet();
								}
								expected[0]++;
							});
							if (ds != Ring.DrainStatus.OK) {
								errors[ri].incrementAndGet();
							}
							received[ri].set(expected[0]);
						}
						errors[ri].addAndGet(st.stale + st.corrupt);
					}, "consumer-" + r);
				}
				long start = System.nanoTime();
				for (Thread t : threads) {
					t.start();
				}
				for (Thread t : threads) {
					t.join();
				}
				System.out.printf("java stress: %d + %d records, errors %d + %d, %d ms%n", received[0].get(), received[1].get(), errors[0].get(), errors[1].get(),
					(System.nanoTime() - start) / 1_000_000);
				assertEquals(MESSAGES, received[0].get());
				assertEquals(MESSAGES, received[1].get());
				assertEquals(0, errors[0].get());
				assertEquals(0, errors[1].get());
			} finally {
				Win32.unmap(view.base());
				Win32.closeHandle(view.handle());
			}
		});
	}
}
