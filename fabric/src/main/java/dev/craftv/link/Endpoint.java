package dev.craftv.link;

import static dev.craftv.link.Proto.*;

import java.lang.foreign.MemorySegment;

/**
 * One side of the CraftV link: mapping lifecycle, sessions, heartbeats, link state, HELLO and
 * HEARTBEAT handling, typed sends (PROTOCOL.md §5, §7). The Java twin of
 * protocol/cpp/src/endpoint.cpp; behaviour must match it line for line.
 *
 * <p>Threading: one thread owns an Endpoint. In Minecraft that is {@code LinkService}'s thread.
 */
public final class Endpoint {
	public enum State { DETACHED, WAITING, ATTACHED, CONNECTED, STALE }

	// Bits returned by takeEvents(); same values as the C++ LinkEvent.
	public static final int EV_ATTACHED = 1;
	public static final int EV_CONNECTED = 1 << 1;
	public static final int EV_RESUMED = 1 << 2;
	public static final int EV_STALE = 1 << 3;
	public static final int EV_PEER_DETACHED = 1 << 4;
	public static final int EV_PEER_RESTARTED = 1 << 5;
	public static final int EV_PEER_HELLO = 1 << 6;
	public static final int EV_VERSION_MISMATCH = 1 << 7;
	public static final int EV_DETACHED = 1 << 8;
	public static final int EV_CORRUPT = 1 << 9;

	private static final long CORRUPT_LOG_INTERVAL_MS = 5000;

	/** Receives gameplay records (HELLO and HEARTBEAT are handled inside). */
	@FunctionalInterface
	public interface MessageSink {
		void accept(int type, int payloadBytes, MemorySegment s, long payloadOff);
	}

	public static final class Config {
		public int role = ROLE_MC;
		public String mappingName = DEFAULT_MAPPING_NAME;
		public long peerTimeoutMs; // 0: role default
		public String software = "CraftV";
		public LinkLog log = LinkLog.NONE;
	}

	/** What we know about the other side. */
	public static final class Peer {
		public int session;
		public int pid;
		public boolean helloSeen;
		public Messages.Hello hello;
		public long rttUs;
		public boolean rttValid;
	}

	private final Config config;
	private final int peerRole;
	private final long peerTimeoutMs;
	private final int pid;
	private final long myBlock;
	private final long theirBlock;

	private Win32.MappedView view;
	private MemorySegment seg;
	private boolean creator;
	private Layout.Resolved layout;
	private Ring.Producer tx;
	private Ring.Consumer rx;
	private final Ring.Stats txStats = new Ring.Stats();
	private final Ring.Stats rxStats = new Ring.Stats();

	private State state = State.DETACHED;
	private boolean attached;
	private int session;
	private int txSeq;
	private int events;
	private volatile boolean heartbeatSuspended;
	private boolean inGame;
	private boolean helloPending;
	private boolean versionMismatch;
	private long nextOpenMs;
	private long waitingSinceMs;
	private long nextHeartbeatMsgMs;
	private long droppedNotConnected;
	private long lastCorruptLogMs;

	private Peer peer = new Peer();
	private long peerBeat;
	private long peerBeatChangedMs;
	private boolean peerBeatObserved;
	private boolean peerEverConnected;
	private long peerHbSentUs;
	private long peerHbRecvUs;

	private String lastOpenError = "";
	private String lastInvalidError = "";

	public Endpoint(Config config) {
		this.config = config;
		this.peerRole = config.role == ROLE_HOST ? ROLE_MC : ROLE_HOST;
		this.peerTimeoutMs = config.peerTimeoutMs != 0 ? config.peerTimeoutMs : (config.role == ROLE_HOST ? MC_TIMEOUT_MS : HOST_TIMEOUT_MS);
		this.pid = Win32.currentPid();
		this.myBlock = config.role == ROLE_HOST ? OFF_HOST_BLOCK : OFF_MC_BLOCK;
		this.theirBlock = config.role == ROLE_HOST ? OFF_MC_BLOCK : OFF_HOST_BLOCK;
	}

	private static String roleName(int role) {
		return role == ROLE_HOST ? "host" : role == ROLE_MC ? "minecraft" : "?";
	}

	private void logOnce(boolean openError, int level, String text) {
		if (openError ? !text.equals(lastOpenError) : !text.equals(lastInvalidError)) {
			if (openError) {
				lastOpenError = text;
			} else {
				lastInvalidError = text;
			}
			config.log.log(level, text);
		}
	}

	/** Call at least every HEARTBEAT_PERIOD_MS. */
	public void tick(long nowMs, long nowUs) {
		if (state == State.DETACHED && nowMs >= nextOpenMs) {
			tryOpen(nowMs, nowUs);
		} else if (state == State.WAITING) {
			pollWaiting(nowMs, nowUs);
		}
		if (!attached) {
			return;
		}
		if (!heartbeatSuspended) {
			ATOMIC_LONG.setOpaque(seg, myBlock + SB_HEARTBEAT_US, nowUs);
			ATOMIC_LONG.getAndAddRelease(seg, myBlock + SB_HEARTBEAT, 1L);
		}
		updatePeer(nowMs);
		sendControl(nowMs, nowUs);
	}

	private void tryOpen(long nowMs, long nowUs) {
		Win32.MappedView v = Win32.createOrOpenMapping(config.mappingName, MAPPING_BYTES);
		if (!v.ok()) {
			logOnce(true, LOG_WARN, v.error());
			nextOpenMs = nowMs + RETRY_MS;
			return;
		}
		lastOpenError = "";
		view = v;
		seg = v.base();
		creator = v.created();
		if (creator) {
			if (v.viewBytes() < MAPPING_BYTES) {
				config.log.log(LOG_ERROR, "new mapping smaller than requested (" + v.viewBytes() + " bytes)");
				closeMapping(nowMs + INVALID_RETRY_MS);
				return;
			}
			Layout.initialize(seg, config.role, pid);
		}
		config.log.log(LOG_INFO, "shared memory " + (creator ? "created" : "opened") + " (" + (v.viewBytes() / 1024) + " KiB)");
		state = State.WAITING;
		waitingSinceMs = nowMs;
		pollWaiting(nowMs, nowUs);
	}

	private void pollWaiting(long nowMs, long nowUs) {
		int magic = (int) ATOMIC_INT.getAcquire(seg, H_MAGIC);
		if (magic == 0) {
			if (nowMs - waitingSinceMs > INIT_TIMEOUT_MS && view.viewBytes() >= MAPPING_BYTES) {
				config.log.log(LOG_WARN, "shared memory never initialised by its creator; initialising it");
				Layout.initialize(seg, config.role, pid);
			}
			return;
		}
		try {
			layout = Layout.validate(seg, view.viewBytes());
		} catch (Layout.Invalid e) {
			logOnce(false, LOG_ERROR, e.getMessage());
			closeMapping(nowMs + INVALID_RETRY_MS);
			return;
		}
		lastInvalidError = "";
		attach(nowMs, nowUs);
	}

	private static boolean processAlive(int pid) {
		return pid != 0 && ProcessHandle.of(pid).map(ProcessHandle::isAlive).orElse(false);
	}

	private void attach(long nowMs, long nowUs) {
		int mineState = (int) ATOMIC_INT.getAcquire(seg, myBlock + SB_STATE);
		int minePid = (int) ATOMIC_INT.getAcquire(seg, myBlock + SB_PID);
		if ((mineState & SIDE_ATTACHED) != 0 && minePid != pid && processAlive(minePid)) {
			logOnce(false, LOG_ERROR, "another " + roleName(config.role) + " (pid " + minePid + ") is already attached; waiting");
			closeMapping(nowMs + INVALID_RETRY_MS);
			return;
		}
		Layout.RingLocation txLoc = config.role == ROLE_HOST ? layout.hostToMc() : layout.mcToHost();
		Layout.RingLocation rxLoc = config.role == ROLE_HOST ? layout.mcToHost() : layout.hostToMc();
		// §5.2: drop the stale backlog of the ring we consume, continue the ring we produce.
		rx = new Ring.Consumer(seg, rxLoc.control(), rxLoc.data(), rxLoc.dataBytes());
		rx.discardBacklog();
		tx = new Ring.Producer(seg, txLoc.control(), txLoc.data(), txLoc.dataBytes());

		int previous = (int) ATOMIC_INT.getAcquire(seg, myBlock + SB_SESSION);
		session = previous + 1;
		if (session == 0) {
			session = 1;
		}
		ATOMIC_INT.setOpaque(seg, myBlock + SB_PID, pid);
		ATOMIC_LONG.setOpaque(seg, myBlock + SB_HEARTBEAT_US, nowUs);
		ATOMIC_LONG.getAndAdd(seg, myBlock + SB_HEARTBEAT, 1L);
		ATOMIC_INT.setRelease(seg, myBlock + SB_STATE, SIDE_ATTACHED | (inGame ? SIDE_IN_GAME : 0));
		ATOMIC_INT.setRelease(seg, myBlock + SB_SESSION, session);

		txSeq = 0;
		attached = true;
		state = State.ATTACHED;
		helloPending = true;
		versionMismatch = false;
		peer = new Peer();
		peerBeatObserved = false;
		peerEverConnected = false;
		peerHbSentUs = 0;
		events |= EV_ATTACHED;
		config.log.log(LOG_INFO, "attached as " + roleName(config.role) + ", session " + Integer.toUnsignedString(session));
	}

	private void updatePeer(long nowMs) {
		int peerSession = (int) ATOMIC_INT.getAcquire(seg, theirBlock + SB_SESSION);
		int peerState = (int) ATOMIC_INT.getAcquire(seg, theirBlock + SB_STATE);
		long beat = (long) ATOMIC_LONG.getAcquire(seg, theirBlock + SB_HEARTBEAT);

		if (peerSession != peer.session) {
			boolean wasKnown = peer.session != 0;
			// The new peer's HELLO may already have been drained (see the C++ twin). Keep it.
			boolean keepHello = peer.helloSeen && peer.hello != null && peer.hello.session() == peerSession;
			Messages.Hello hello = peer.hello;
			peer = new Peer();
			peer.session = peerSession;
			if (keepHello) {
				peer.helloSeen = true;
				peer.hello = hello;
			}
			peer.pid = (int) ATOMIC_INT.getAcquire(seg, theirBlock + SB_PID);
			peerBeat = beat;
			peerBeatChangedMs = nowMs;
			peerBeatObserved = false;
			peerEverConnected = false;
			peerHbSentUs = 0;
			versionMismatch = false;
			if (peerSession != 0) {
				helloPending = true;
				if (wasKnown) {
					events |= EV_PEER_RESTARTED;
					config.log.log(LOG_INFO, roleName(peerRole) + " restarted (new session " + Integer.toUnsignedString(peerSession) + ", pid " + peer.pid + ")");
				}
			}
		} else if (beat != peerBeat) {
			peerBeat = beat;
			peerBeatChangedMs = nowMs;
			peerBeatObserved = true;
		}

		State next;
		if (peerSession == 0 || (peerState & SIDE_ATTACHED) == 0) {
			next = State.ATTACHED;
		} else if (nowMs - peerBeatChangedMs > peerTimeoutMs) {
			next = State.STALE;
		} else if (peerBeatObserved) {
			next = State.CONNECTED;
		} else {
			next = State.ATTACHED;
		}
		if (next == state) {
			return;
		}
		State previous = state;
		state = next;
		switch (next) {
			case CONNECTED -> {
				events |= peerEverConnected ? EV_RESUMED : EV_CONNECTED;
				config.log.log(LOG_INFO, "link " + (peerEverConnected ? "resumed" : "up") + " with " + roleName(peerRole) + " (session "
					+ Integer.toUnsignedString(peer.session) + ", pid " + peer.pid + ")");
				peerEverConnected = true;
				nextHeartbeatMsgMs = nowMs;
			}
			case STALE -> {
				events |= EV_STALE;
				config.log.log(LOG_WARN, "link stale: no heartbeat from " + roleName(peerRole) + " for " + (nowMs - peerBeatChangedMs) + " ms");
			}
			case ATTACHED -> {
				// Not when a new session just appeared (that's a restart, reported above).
				if ((previous == State.CONNECTED || previous == State.STALE) && (peerSession == 0 || (peerState & SIDE_ATTACHED) == 0)) {
					events |= EV_PEER_DETACHED;
					config.log.log(LOG_INFO, roleName(peerRole) + " detached");
				}
			}
			default -> {
			}
		}
	}

	private void sendControl(long nowMs, long nowUs) {
		if (helloPending && peer.session != 0) {
			if (send(Messages.Hello.of(config.role, pid, session, config.software))) {
				helloPending = false;
			}
		}
		if (state == State.CONNECTED && !heartbeatSuspended && nowMs >= nextHeartbeatMsgMs) {
			long hold = peerHbSentUs != 0 ? nowUs - peerHbRecvUs : 0;
			int counter = (int) (long) ATOMIC_LONG.getOpaque(seg, myBlock + SB_HEARTBEAT);
			send(new Messages.Heartbeat(nowUs, peerHbSentUs, hold, counter));
			nextHeartbeatMsgMs = nowMs + HEARTBEAT_MSG_PERIOD_MS;
		}
	}

	/** HELLO/HEARTBEAT/LOG need only an attachment; everything else needs CONNECTED (§5.3). */
	public boolean send(Messages.Payload payload) {
		if (!attached) {
			droppedNotConnected++;
			return false;
		}
		int type = payload.type();
		boolean control = type == MSG_HELLO || type == MSG_HEARTBEAT || type == MSG_LOG;
		if (!control && (state != State.CONNECTED || versionMismatch)) {
			droppedNotConnected++;
			return false;
		}
		if (tx.push(type, TYPE_VERSION_1, session, txSeq + 1, payload, txStats) != Ring.PushResult.OK) {
			return false;
		}
		txSeq++;
		return true;
	}

	/** Reads up to about {@code maxBytes}; gameplay records go to {@code sink}. */
	public void drain(long nowUs, long maxBytes, MessageSink sink) {
		if (!attached) {
			return;
		}
		Ring.DrainStatus status = rx.drain(theirBlock + SB_SESSION, maxBytes, rxStats, (type, typeVersion, payloadBytes, seq, s, off) -> {
			if (type == MSG_PLAYER_STATE && peerRole != ROLE_HOST) {
				rxStats.malformed++; // MC -> host PLAYER_STATE is reserved (§7.3)
				return;
			}
			switch (type) {
				case MSG_HELLO -> handleHello(payloadBytes, s, off);
				case MSG_HEARTBEAT -> handleHeartbeat(payloadBytes, s, off, nowUs);
				case MSG_PLAYER_STATE, MSG_BLOCK_SET, MSG_BLOCK_BREAK_REQUEST, MSG_BLOCK_PLACE_REQUEST, MSG_LOG, MSG_TEST_PATTERN -> {
					if (!versionMismatch) {
						sink.accept(type, payloadBytes, s, off);
					}
				}
				default -> rxStats.unknown++;
			}
		});
		if (status == Ring.DrainStatus.CORRUPT) {
			events |= EV_CORRUPT;
			long now = Win32.tickCount();
			if (now - lastCorruptLogMs > CORRUPT_LOG_INTERVAL_MS) {
				lastCorruptLogMs = now;
				config.log.log(LOG_ERROR, "incoming ring from " + roleName(peerRole) + " was corrupt; dropped its backlog (" + rxStats.corrupt + " so far)");
			}
		}
	}

	private void handleHello(int payloadBytes, MemorySegment s, long off) {
		if (payloadBytes < HELLO_BYTES) {
			rxStats.malformed++;
			return;
		}
		Messages.Hello hello = Messages.Hello.read(s, off);
		if (!hello.valid() || hello.role() != peerRole) {
			rxStats.malformed++;
			return;
		}
		peer.helloSeen = true;
		peer.hello = hello;
		events |= EV_PEER_HELLO;
		if (hello.versionMajor() != VERSION_MAJOR) {
			versionMismatch = true;
			events |= EV_VERSION_MISMATCH;
			config.log.log(LOG_ERROR, "HELLO from " + roleName(peerRole) + " '" + hello.software() + "' speaks protocol " + hello.versionMajor() + "."
				+ hello.versionMinor() + "; we speak " + VERSION_MAJOR + "." + VERSION_MINOR + ": ignoring its messages");
			return;
		}
		config.log.log(LOG_INFO, "HELLO from " + roleName(peerRole) + " '" + hello.software() + "' (protocol " + hello.versionMajor() + "." + hello.versionMinor()
			+ ", pid " + hello.pid() + ", session " + Integer.toUnsignedString(hello.session()) + ")");
	}

	private void handleHeartbeat(int payloadBytes, MemorySegment s, long off, long nowUs) {
		if (payloadBytes < HEARTBEAT_BYTES) {
			rxStats.malformed++;
			return;
		}
		Messages.Heartbeat hb = Messages.Heartbeat.read(s, off);
		if (!hb.valid()) {
			rxStats.malformed++;
			return;
		}
		peerHbSentUs = hb.sentUs();
		peerHbRecvUs = nowUs;
		if (hb.echoUs() != 0 && nowUs >= hb.echoUs() + hb.echoHoldUs()) {
			peer.rttUs = nowUs - hb.echoUs() - hb.echoHoldUs();
			peer.rttValid = true;
		}
	}

	public void setInGame(boolean value) {
		inGame = value;
		if (attached) {
			ATOMIC_INT.setRelease(seg, myBlock + SB_STATE, SIDE_ATTACHED | (inGame ? SIDE_IN_GAME : 0));
		}
	}

	/** Test hook ("kill-link"). */
	public void setHeartbeatSuspended(boolean suspended) {
		heartbeatSuspended = suspended;
	}

	public void countMalformed() {
		rxStats.malformed++;
	}

	public int takeEvents() {
		int e = events;
		events = 0;
		return e;
	}

	private void closeMapping(long retryAtMs) {
		tx = null;
		rx = null;
		if (view != null) {
			Win32.unmap(view.base());
			Win32.closeHandle(view.handle());
			view = null;
		}
		seg = null;
		layout = null;
		attached = false;
		state = State.DETACHED;
		nextOpenMs = retryAtMs;
	}

	/** Clean shutdown (§5.3): clear ATTACHED, keep the session id, unmap. */
	public void detach() {
		if (attached) {
			int st = (int) ATOMIC_INT.getOpaque(seg, myBlock + SB_STATE);
			ATOMIC_INT.setRelease(seg, myBlock + SB_STATE, st & ~SIDE_ATTACHED);
			events |= EV_DETACHED;
			config.log.log(LOG_INFO, "detached (session " + Integer.toUnsignedString(session) + ")");
		}
		closeMapping(0);
	}

	public State state() {
		return state;
	}

	public boolean connected() {
		return state == State.CONNECTED;
	}

	public int session() {
		return session;
	}

	public Peer peer() {
		return peer;
	}

	public Ring.Stats txStats() {
		return txStats;
	}

	public Ring.Stats rxStats() {
		return rxStats;
	}

	public long droppedNotConnected() {
		return droppedNotConnected;
	}

	public boolean wasCreator() {
		return creator;
	}

	public long txBacklog() {
		return tx != null ? tx.backlog() : 0;
	}
}
