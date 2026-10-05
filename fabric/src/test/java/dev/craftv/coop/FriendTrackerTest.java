package dev.craftv.coop;

import static dev.craftv.link.Proto.*;
import static org.junit.jupiter.api.Assertions.*;

import dev.craftv.coop.FriendTracker.Sample;
import dev.craftv.link.Messages;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import org.junit.jupiter.api.Test;

class FriendTrackerTest {
	static final UUID ALEX = UUID.fromString("00010203-0405-0607-0809-0a0b0c0d0e0f");

	static Sample alex(double x, double z, boolean swinging, int tick) {
		return new Sample(7, ALEX, "Alex", x, 70, z, 370.0F, 95.0F, -190.0F, REMOTE_ON_GROUND, swinging, 1, 19.2F, tick);
	}

	final List<Messages.Payload> sent = new ArrayList<>();
	final FriendTracker tracker = new FriendTracker(sent::add);

	@Test
	void joinComesBeforeTheFirstStateAndVelocityIsPerSecond() {
		tracker.tick(List.of(alex(0, 0, false, 1)), Map.of());
		assertInstanceOf(Messages.RemotePlayerJoin.class, sent.get(0));
		var join = (Messages.RemotePlayerJoin) sent.get(0);
		assertEquals("Alex", join.name());
		assertEquals(ALEX, new UUID(join.uuidMsb(), join.uuidLsb()));
		var first = (Messages.RemotePlayerState) sent.get(1);
		assertEquals(0.0F, first.vx(), "no velocity on the first sample");
		sent.clear();
		tracker.tick(List.of(alex(0.25, -0.1, false, 2)), Map.of());
		assertEquals(1, sent.size(), "no second JOIN");
		var s = (Messages.RemotePlayerState) sent.get(0);
		assertEquals(5.0F, s.vx(), 1e-4);
		assertEquals(-2.0F, s.vz(), 1e-4);
		assertTrue(s.valid());
	}

	@Test
	void anglesAreWrappedAndClampedAndHealthRoundsUp() {
		tracker.tick(List.of(alex(0, 0, false, 1)), Map.of());
		var s = (Messages.RemotePlayerState) sent.get(1);
		assertEquals(10.0F, s.yaw(), 1e-4);
		assertEquals(90.0F, s.pitch(), 1e-4);
		assertEquals(170.0F, s.bodyYaw(), 1e-4);
		assertEquals(20, s.health());
		assertTrue(s.valid());
	}

	@Test
	void swingIsFlaggedOnlyOnTheTickItStarts() {
		tracker.tick(List.of(alex(0, 0, false, 1)), Map.of());
		tracker.tick(List.of(alex(0, 0, true, 2)), Map.of());
		tracker.tick(List.of(alex(0, 0, true, 3)), Map.of());
		var states = sent.stream().filter(p -> p instanceof Messages.RemotePlayerState).map(p -> (Messages.RemotePlayerState) p).toList();
		assertEquals(0, states.get(0).flags() & REMOTE_SWING);
		assertEquals(REMOTE_SWING, states.get(1).flags() & REMOTE_SWING);
		assertEquals(0, states.get(2).flags() & REMOTE_SWING);
	}

	@Test
	void leavingSendsLeaveWithTheReason() {
		tracker.tick(List.of(alex(0, 0, false, 1)), Map.of());
		sent.clear();
		tracker.tick(List.of(), Map.of(7, LEAVE_OTHER_DIMENSION));
		assertEquals(List.of(new Messages.RemotePlayerLeave(7, LEAVE_OTHER_DIMENSION)), sent);
		assertEquals(0, tracker.count());
	}

	@Test
	void resyncAnnouncesEveryoneAgain() {
		tracker.tick(List.of(alex(0, 0, false, 1)), Map.of());
		sent.clear();
		tracker.resync();
		tracker.tick(List.of(alex(0, 0, false, 2)), Map.of());
		assertInstanceOf(Messages.RemotePlayerJoin.class, sent.get(0));
		assertInstanceOf(Messages.RemotePlayerState.class, sent.get(1));
	}

	@Test
	void resetAllSaysEveryoneIsGone() {
		tracker.tick(List.of(alex(0, 0, false, 1)), Map.of());
		sent.clear();
		tracker.resetAll();
		assertEquals(List.of(new Messages.RemotePlayerLeave(7, LEAVE_RESET)), sent);
	}

	@Test
	void aTeleportIsNotReportedAsASpeed() {
		tracker.tick(List.of(alex(0, 0, false, 1)), Map.of());
		tracker.tick(List.of(alex(5000, 0, false, 2)), Map.of());
		var s = (Messages.RemotePlayerState) sent.get(sent.size() - 1);
		assertEquals(0.0F, s.vx());
		assertTrue(s.valid());
	}
}
