package dev.craftv;

import static dev.craftv.link.Proto.*;

import dev.craftv.link.Endpoint;
import dev.craftv.link.Messages;
import dev.craftv.link.Win32;
import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Owns the link on its own daemon thread (DECISIONS.md D-005): attaches, heartbeats, drains the
 * host's ring and sends what the game queued. The game threads never touch shared memory; they
 * read the latest {@link Messages.PlayerState}, take host block ops and terrain patches, and {@link #send} payloads.
 * Because this thread keeps beating while Minecraft loads a world, the host never sees a stall.
 */
public final class LinkService {
	private static final long LOOP_SLEEP_MS = 2;
	private static final long STATUS_LOG_PERIOD_MS = 30_000;
	private static final int OUTBOX_LIMIT = 50_000;
	private static final int TERRAIN_INBOX_LIMIT = 4_096; // ~5 MiB of patches; beyond that MC asks again later
	private static final int INPUT_LIMIT = 256; // forwarded presses; more than this unprocessed means the game is stuck

	/** The host's latest CAMERA (PROTOCOL.md §7.15) and when it arrived ({@link System#nanoTime}). */
	public record ReceivedCamera(Messages.Camera camera, long receivedNanos) {
	}

	private static final LinkService INSTANCE = new LinkService();

	private final AtomicReference<Messages.PlayerState> latestPlayerState = new AtomicReference<>();
	private final Queue<Messages.Payload> hostBlockOps = new ConcurrentLinkedQueue<>();
	private final Queue<Messages.Payload> outbox = new ConcurrentLinkedQueue<>();
	private final Queue<Messages.TerrainPatch> terrainInbox = new ConcurrentLinkedQueue<>();
	private final AtomicReference<ReceivedCamera> latestCamera = new AtomicReference<>();
	private final AtomicReference<Messages.View> latestView = new AtomicReference<>();
	private final Queue<Messages.Input> inputs = new ConcurrentLinkedQueue<>();
	private final Queue<Messages.BlockRegionRequest> regionRequests = new ConcurrentLinkedQueue<>();
	private volatile Thread thread;
	private volatile boolean running;
	private volatile boolean inGame;

	// Snapshot for the HUD and other threads (written by the link thread).
	private volatile Endpoint.State state = Endpoint.State.DETACHED;
	private volatile long rttUs;
	private volatile long received, sent, dropped, malformed;
	private volatile int session, peerSession;
	private volatile String peerSoftware = "";
	/** Bumps whenever a new host session connects: the game should re-snap the player. */
	private volatile int peerGeneration;

	private LinkService() {
	}

	public static LinkService get() {
		return INSTANCE;
	}

	public synchronized void start() {
		if (thread != null) {
			return;
		}
		Win32.holdNamedMutex(mappingName() + "_minecraft");
		running = true;
		thread = new Thread(this::run, "CraftV-Link");
		thread.setDaemon(true);
		thread.start();
		Runtime.getRuntime().addShutdownHook(new Thread(this::stop, "CraftV-Link-Stop"));
	}

	public void stop() {
		running = false;
		Thread t = thread;
		if (t != null) {
			try {
				t.join(1000);
			} catch (InterruptedException e) {
				Thread.currentThread().interrupt();
			}
		}
	}

	static String mappingName() {
		return System.getProperty("craftv.link", DEFAULT_MAPPING_NAME);
	}

	private void run() {
		Endpoint.Config config = new Endpoint.Config();
		config.role = ROLE_MC;
		config.mappingName = mappingName();
		config.software = "CraftV-Fabric " + CraftV.version();
		config.peerTimeoutMs = Long.getLong("craftv.hostTimeoutMs", 0L);
		config.log = CraftLog::link;
		Endpoint ep = new Endpoint(config);
		CraftLog.info("link thread started (mapping " + config.mappingName + ")");
		long nextStatusLog = 0;
		boolean lastInGame = false;
		try {
			while (running) {
				long nowMs = Win32.tickCount();
				long nowUs = Win32.nowUs();
				if (inGame != lastInGame) {
					lastInGame = inGame;
					ep.setInGame(lastInGame);
				}
				ep.tick(nowMs, nowUs);
				handleEvents(ep);
				ep.drain(nowUs, MAX_DRAIN_BYTES_PER_TICK, (type, bytes, s, off) -> {
					switch (type) {
						case MSG_PLAYER_STATE -> accept(ep, bytes >= PLAYER_STATE_BYTES ? Messages.PlayerState.read(s, off) : null);
						case MSG_BLOCK_SET -> queueBlockOp(ep, bytes >= BLOCK_SET_BYTES ? Messages.BlockSet.read(s, off) : null);
						case MSG_BLOCK_BREAK_REQUEST -> queueBlockOp(ep, bytes >= BLOCK_REQUEST_BYTES ? Messages.BlockBreakRequest.read(s, off) : null);
						case MSG_BLOCK_PLACE_REQUEST -> queueBlockOp(ep, bytes >= BLOCK_REQUEST_BYTES ? Messages.BlockPlaceRequest.read(s, off) : null);
						case MSG_TERRAIN_PATCH -> acceptTerrain(ep, bytes >= TERRAIN_PATCH_BYTES ? Messages.TerrainPatch.read(s, off) : null);
						case MSG_CAMERA -> acceptCamera(ep, bytes >= CAMERA_BYTES ? Messages.Camera.read(s, off) : null);
						case MSG_VIEW -> acceptView(ep, bytes >= VIEW_BYTES ? Messages.View.read(s, off) : null);
						case MSG_INPUT -> acceptInput(ep, bytes >= INPUT_BYTES ? Messages.Input.read(s, off) : null);
						case MSG_BLOCK_REGION_REQUEST -> acceptRegion(ep, bytes >= BLOCK_REGION_REQUEST_BYTES ? Messages.BlockRegionRequest.read(s, off) : null);
						case MSG_LOG -> {
							Messages.Log log = bytes >= LOG_BYTES ? Messages.Log.read(s, off) : null;
							if (log != null && log.valid()) {
								CraftLog.limited("hostlog", 200, "[host] " + log.text());
							} else {
								ep.countMalformed();
							}
						}
						default -> {
						} // TEST_PATTERN etc.: ignored in the game
					}
				});
				flushOutbox(ep);
				publishSnapshot(ep);
				if (nowMs >= nextStatusLog) {
					nextStatusLog = nowMs + STATUS_LOG_PERIOD_MS;
					CraftLog.info("link status: " + statusLine());
				}
				Thread.sleep(LOOP_SLEEP_MS);
			}
		} catch (InterruptedException e) {
			Thread.currentThread().interrupt();
		} catch (Throwable t) {
			CraftLog.error("link thread crashed; CraftV link is off until restart", t);
		} finally {
			ep.detach();
			state = Endpoint.State.DETACHED;
		}
	}

	private void handleEvents(Endpoint ep) {
		int ev = ep.takeEvents();
		if ((ev & (Endpoint.EV_CONNECTED | Endpoint.EV_PEER_RESTARTED)) != 0) {
			peerGeneration++;
		}
		if ((ev & (Endpoint.EV_STALE | Endpoint.EV_PEER_DETACHED)) != 0) {
			latestPlayerState.set(null); // stop puppeting; the player stays where it is
			latestCamera.set(null); // and the passthrough view ends
			inputs.clear();
		}
		if ((ev & Endpoint.EV_CONNECTED) != 0) {
			send(new Messages.Log(LOG_INFO, "CraftV-Fabric " + CraftV.version() + " connected"));
		}
	}

	private void accept(Endpoint ep, Messages.PlayerState ps) {
		if (ps == null || !ps.valid()) {
			ep.countMalformed();
			CraftLog.limited("badps", 5000, "ignored an invalid PLAYER_STATE from the host");
			return;
		}
		latestPlayerState.set(ps);
	}

	private void acceptCamera(Endpoint ep, Messages.Camera camera) {
		if (camera == null || !camera.valid()) {
			ep.countMalformed();
			CraftLog.limited("badcamera", 5000, "ignored an invalid CAMERA from the host");
			return;
		}
		latestCamera.set(new ReceivedCamera(camera, System.nanoTime()));
	}

	private void acceptView(Endpoint ep, Messages.View view) {
		if (view == null || !view.valid()) {
			ep.countMalformed();
			CraftLog.limited("badview", 5000, "ignored an invalid VIEW from the host");
			return;
		}
		latestView.set(view);
	}

	private void acceptInput(Endpoint ep, Messages.Input input) {
		if (input == null || !input.valid()) {
			ep.countMalformed();
			CraftLog.limited("badinput", 5000, "ignored an invalid INPUT from the host");
			return;
		}
		if (inputs.size() < INPUT_LIMIT) {
			inputs.add(input);
		}
	}

	private void acceptRegion(Endpoint ep, Messages.BlockRegionRequest r) {
		if (r == null || !r.valid()) {
			ep.countMalformed();
			CraftLog.limited("badregion", 5000, "ignored an invalid BLOCK_REGION_REQUEST from the host");
			return;
		}
		if (regionRequests.size() < TERRAIN_INBOX_LIMIT) {
			regionRequests.add(r);
		}
	}

	private void acceptTerrain(Endpoint ep, Messages.TerrainPatch patch) {
		if (patch == null || !patch.valid()) {
			ep.countMalformed();
			CraftLog.limited("badterrain", 5000, "ignored an invalid TERRAIN_PATCH from the host");
			return;
		}
		if (terrainInbox.size() >= TERRAIN_INBOX_LIMIT) {
			CraftLog.limited("terraininbox", 5000, "terrain inbox full; dropping patches (they'll be requested again)");
			return;
		}
		terrainInbox.add(patch);
	}

	private void queueBlockOp(Endpoint ep, Messages.Payload op) {
		boolean ok = switch (op) {
			case Messages.BlockSet b -> b.valid();
			case Messages.BlockBreakRequest b -> b.valid();
			case Messages.BlockPlaceRequest b -> b.valid();
			case null, default -> false;
		};
		if (!ok) {
			ep.countMalformed();
			CraftLog.limited("badblock", 5000, "ignored an invalid block message from the host");
			return;
		}
		hostBlockOps.add(op);
	}

	private void flushOutbox(Endpoint ep) {
		Messages.Payload p;
		while ((p = outbox.peek()) != null) {
			if (!ep.connected() && p.type() != MSG_LOG) {
				outbox.poll(); // not connected: gameplay messages are dropped (MC stays the authority)
				dropped++;
				continue;
			}
			if (!ep.send(p)) {
				if (ep.connected()) {
					break; // ring full: retry next loop
				}
				outbox.poll();
				dropped++;
				continue;
			}
			outbox.poll();
		}
	}

	private void publishSnapshot(Endpoint ep) {
		state = ep.state();
		var peer = ep.peer();
		rttUs = peer.rttValid ? peer.rttUs : 0;
		received = ep.rxStats().consumed;
		sent = ep.txStats().produced;
		malformed = ep.rxStats().malformed + ep.rxStats().corrupt;
		session = ep.session();
		peerSession = peer.session;
		peerSoftware = peer.helloSeen && peer.hello != null ? peer.hello.software() : "";
	}

	// ---- game-facing API (any thread) -----------------------------------------------------------

	/** Queues a message for the host. Dropped if the link isn't connected (see flushOutbox). */
	public void send(Messages.Payload payload) {
		if (outbox.size() >= OUTBOX_LIMIT) {
			dropped++;
			CraftLog.limited("outbox", 5000, "outbox full; dropping messages for the host");
			return;
		}
		outbox.add(payload);
	}

	/** The host's software name from its HELLO ("CraftV-GTA5 0.1.0", "CraftV-MockHost 0.1.0"), or empty. */
	public String peerSoftware() {
		return peerSoftware;
	}

	public boolean connected() {
		return state == Endpoint.State.CONNECTED;
	}

	/** The host's latest camera, or null (link down or never sent). */
	public ReceivedCamera latestCamera() {
		return latestCamera.get();
	}

	/** The window size the host last asked for (§7.16), or null. */
	public Messages.View latestView() {
		return latestView.get();
	}

	/** The next BLOCK_REGION_REQUEST (§7.19), or null. Server thread. */
	public Messages.BlockRegionRequest pollRegionRequest() {
		return regionRequests.poll();
	}

	/** The next forwarded input (§7.17), or null. Client thread. */
	public Messages.Input pollInput() {
		return inputs.poll();
	}

	public Messages.PlayerState latestPlayerState() {
		return latestPlayerState.get();
	}

	public Messages.Payload pollHostBlockOp() {
		return hostBlockOps.poll();
	}

	/** The next ground patch from the host (PROTOCOL.md §7.12), or null. Server thread. */
	public Messages.TerrainPatch pollTerrainPatch() {
		return terrainInbox.poll();
	}

	public int peerGeneration() {
		return peerGeneration;
	}

	public void setInGame(boolean value) {
		inGame = value;
	}

	public String statusLine() {
		return "state=" + state + " session=" + Integer.toUnsignedString(session) + " host=" + Integer.toUnsignedString(peerSession)
			+ (peerSoftware.isEmpty() ? "" : " '" + peerSoftware + "'") + " ping=" + String.format("%.2f", rttUs / 1000.0) + "ms rx=" + received + " tx=" + sent
			+ " dropped=" + dropped + " bad=" + malformed;
	}

	public Endpoint.State state() {
		return state;
	}

	public long rttUs() {
		return rttUs;
	}

	public long receivedCount() {
		return received;
	}

	public long sentCount() {
		return sent;
	}
}
