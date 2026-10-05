package dev.craftv.link;

import static dev.craftv.link.Proto.*;
import static org.junit.jupiter.api.Assertions.*;

import java.util.concurrent.atomic.AtomicLong;
import org.junit.jupiter.api.Test;

/** Endpoint behaviour with a fake clock (PROTOCOL.md §5); mirrors protocol/cpp/tests/test_endpoint.cpp. */
class EndpointTest {
	static final AtomicLong COUNTER = new AtomicLong();
	long ms = 100_000;

	static String uniqueName(String tag) {
		return "Local\\CraftV_Test_j" + tag + "_" + ProcessHandle.current().pid() + "_" + COUNTER.incrementAndGet() + "_" + System.nanoTime();
	}

	static Endpoint make(int role, String name) {
		Endpoint.Config c = new Endpoint.Config();
		c.role = role;
		c.mappingName = name;
		c.peerTimeoutMs = 1000;
		c.software = role == ROLE_HOST ? "test-host" : "test-mc";
		return new Endpoint(c);
	}

	void tickBoth(Endpoint a, Endpoint b, int ticks) {
		for (int i = 0; i < ticks; i++) {
			ms += 16;
			a.tick(ms, ms * 1000);
			b.tick(ms, ms * 1000);
			a.drain(ms * 1000, MAX_DRAIN_BYTES_PER_TICK, (t, n, s, o) -> {
			});
			b.drain(ms * 1000, MAX_DRAIN_BYTES_PER_TICK, (t, n, s, o) -> {
			});
		}
	}

	@Test
	void connectHelloAndRtt() {
		String name = uniqueName("ep");
		Endpoint host = make(ROLE_HOST, name), mc = make(ROLE_MC, name);
		try {
			tickBoth(mc, host, 5);
			assertEquals(Endpoint.State.CONNECTED, host.state());
			assertEquals(Endpoint.State.CONNECTED, mc.state());
			assertTrue(mc.wasCreator());
			tickBoth(mc, host, 60);
			assertTrue(host.peer().helloSeen);
			assertTrue(mc.peer().helloSeen);
			assertEquals(ROLE_HOST, mc.peer().hello.role());
			assertTrue(mc.peer().rttValid);
			assertEquals(0, host.rxStats().malformed + mc.rxStats().malformed + host.rxStats().corrupt + mc.rxStats().corrupt);
		} finally {
			host.detach();
			mc.detach();
		}
	}

	@Test
	void messagesFlowAndAreRefusedBeforeConnected() {
		String name = uniqueName("msgs");
		Endpoint mc = make(ROLE_MC, name);
		Endpoint host = make(ROLE_HOST, name);
		try {
			mc.tick(ms, ms * 1000);
			assertFalse(mc.send(new Messages.BlockSet(1, 2, 3, 4, 0, 0)));
			tickBoth(mc, host, 5);
			assertTrue(mc.connected());
			assertTrue(host.send(new Messages.PlayerState(3.5, 64, 0, 0, 0, 0, 0, 0, 0, 1, 1)));
			assertTrue(mc.send(new Messages.BlockSet(1, 2, 3, 4, 0, 0)));
			int[] got = new int[2];
			mc.drain(ms * 1000, MAX_DRAIN_BYTES_PER_TICK, (t, n, s, o) -> {
				if (t == MSG_PLAYER_STATE && Messages.PlayerState.read(s, o).x() == 3.5) {
					got[0]++;
				}
			});
			host.drain(ms * 1000, MAX_DRAIN_BYTES_PER_TICK, (t, n, s, o) -> {
				if (t == MSG_BLOCK_SET && Messages.BlockSet.read(s, o).z() == 3) {
					got[1]++;
				}
			});
			assertArrayEquals(new int[] { 1, 1 }, got);
			// MC -> host PLAYER_STATE is reserved: counted as malformed, not delivered.
			assertTrue(mc.send(new Messages.PlayerState(3.5, 64, 0, 0, 0, 0, 0, 0, 0, 1, 1)));
			host.drain(ms * 1000, MAX_DRAIN_BYTES_PER_TICK, (t, n, s, o) -> fail("should not be delivered"));
			assertEquals(1, host.rxStats().malformed);
		} finally {
			host.detach();
			mc.detach();
		}
	}

	@Test
	void frozenHeartbeatGoesStaleThenResumes() {
		String name = uniqueName("stale");
		Endpoint host = make(ROLE_HOST, name), mc = make(ROLE_MC, name);
		try {
			tickBoth(host, mc, 5);
			mc.takeEvents();
			int session = host.session();
			host.setHeartbeatSuspended(true);
			tickBoth(host, mc, 80);
			assertEquals(Endpoint.State.STALE, mc.state());
			assertNotEquals(0, mc.takeEvents() & Endpoint.EV_STALE);
			host.setHeartbeatSuspended(false);
			tickBoth(host, mc, 3);
			assertEquals(Endpoint.State.CONNECTED, mc.state());
			int ev = mc.takeEvents();
			assertNotEquals(0, ev & Endpoint.EV_RESUMED);
			assertEquals(0, ev & Endpoint.EV_PEER_RESTARTED);
			assertEquals(session, host.session());
		} finally {
			host.detach();
			mc.detach();
		}
	}

	@Test
	void restartedHostIsANewSessionAndOldRecordsAreStale() {
		String name = uniqueName("restart");
		Endpoint host = make(ROLE_HOST, name), mc = make(ROLE_MC, name);
		try {
			tickBoth(host, mc, 5);
			int old = host.session();
			for (int i = 0; i < 5; i++) {
				host.send(new Messages.PlayerState(0, 64, 0, 0, 0, 0, 0, 0, 0, 1, 1));
			}
			host.detach();
			Endpoint host2 = make(ROLE_HOST, name);
			int[] received = { 0 };
			for (int i = 0; i < 10; i++) {
				ms += 16;
				host2.tick(ms, ms * 1000);
				mc.tick(ms, ms * 1000);
				mc.drain(ms * 1000, MAX_DRAIN_BYTES_PER_TICK, (t, n, s, o) -> received[0] += t == MSG_PLAYER_STATE ? 1 : 0);
				host2.drain(ms * 1000, MAX_DRAIN_BYTES_PER_TICK, (t, n, s, o) -> {
				});
			}
			assertEquals(old + 1, host2.session());
			assertEquals(Endpoint.State.CONNECTED, mc.state());
			assertNotEquals(0, mc.takeEvents() & Endpoint.EV_PEER_RESTARTED);
			assertEquals(0, received[0]);
			assertEquals(5, mc.rxStats().stale);
			host2.detach();
		} finally {
			mc.detach();
		}
	}
}
