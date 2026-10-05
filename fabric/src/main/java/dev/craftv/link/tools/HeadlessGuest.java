package dev.craftv.link.tools;

import static dev.craftv.link.Proto.*;

import dev.craftv.link.Endpoint;
import dev.craftv.link.Messages;
import dev.craftv.link.Win32;

/**
 * A Minecraft-less guest that runs the real Java link code (role MC). Used by the chaos and
 * cross-process stress tests against the C++ mock host, and handy for poking the link by hand:
 *
 * <pre>
 * java --enable-native-access=ALL-UNNAMED -cp fabric/build/classes/java/main dev.craftv.link.tools.HeadlessGuest [options]
 *   --mapping NAME  --host-timeout-ms N  --stress N  --exit-after-ms N  --status-ms N  --blocks  --coop
 * </pre>
 *
 * Prints the same '@' lines as the mock host (@EVENT, @STATUS, @STRESS_DONE, @RX). {@code --coop} plays the
 * co-op side of v1.1 once connected: asks for a few chunks of terrain (one of them twice, to check the host
 * is deterministic), reports a fake friend joining, moving and leaving, and sends SESSION_INFO.
 */
public final class HeadlessGuest {
	private static final long LOOP_SLEEP_MS = 1;
	private static final long BLOCK_EVENT_PERIOD_MS = 1000;
	private static final int COOP_FRIEND_ID = 42;
	private static final long COOP_FRIEND_STATE_MS = 50, COOP_FRIEND_LEAVE_MS = 3000;
	private static final int[][] COOP_CHUNKS = { { 0, 0 }, { -1, 0 }, { 5, -3 }, { 0, 0 } };

	private HeadlessGuest() {
	}

	public static void main(String[] args) throws InterruptedException {
		Endpoint.Config config = new Endpoint.Config();
		config.role = ROLE_MC;
		config.software = "CraftV-HeadlessGuest 0.1.0";
		long stress = 0, exitAfterMs = 0, statusMs = 1000;
		boolean blocks = false, coop = false;
		for (int i = 0; i < args.length; i++) {
			switch (args[i]) {
				case "--mapping" -> config.mappingName = args[++i];
				case "--host-timeout-ms" -> config.peerTimeoutMs = Long.parseLong(args[++i]);
				case "--stress" -> stress = Long.parseLong(args[++i]);
				case "--exit-after-ms" -> exitAfterMs = Long.parseLong(args[++i]);
				case "--status-ms" -> statusMs = Long.parseLong(args[++i]);
				case "--blocks" -> blocks = true;
				case "--coop" -> coop = true;
				default -> {
					System.err.println("unknown option " + args[i]);
					System.exit(2);
				}
			}
		}
		config.log = (level, text) -> out("[link " + level + "] " + text);
		Endpoint ep = new Endpoint(config);
		Runtime.getRuntime().addShutdownHook(new Thread(ep::detach));

		long start = Win32.tickCount();
		long nextStatus = 0, nextBlock = 0;
		long[] counts = new long[3]; // playerStates, stressReceived, stressErrors
		long stressSent = 0;
		boolean stressDone = false;
		int requestId = 0;
		long coopStartMs = 0, nextFriendState = 0;
		boolean coopLeft = false;
		out("CraftV headless guest, pid " + Win32.currentPid());
		while (true) {
			long nowMs = Win32.tickCount();
			long nowUs = Win32.nowUs();
			ep.tick(nowMs, nowUs);
			int ev = ep.takeEvents();
			reportEvents(ep, ev);
			if ((ev & Endpoint.EV_PEER_RESTARTED) != 0) { // records may arrive before our own CONNECTED event
				counts[1] = 0;
			}
			ep.drain(nowUs, MAX_DRAIN_BYTES_PER_TICK, (type, bytes, s, off) -> {
				switch (type) {
					case MSG_PLAYER_STATE -> {
						Messages.PlayerState ps = Messages.PlayerState.read(s, off);
						if (bytes >= PLAYER_STATE_BYTES && ps.valid()) {
							counts[0]++;
						} else {
							ep.countMalformed();
						}
					}
					case MSG_TEST_PATTERN -> {
						long idx = Messages.TestPattern.check(s, off, bytes);
						if (idx != counts[1]) {
							if (++counts[2] <= 5) {
								out("@STRESS error: got index " + idx + ", expected " + counts[1]);
							}
						}
						counts[1]++;
					}
					case MSG_BLOCK_SET -> out("@RX BLOCK_SET " + Messages.BlockSet.read(s, off));
					case MSG_LOG -> out("@RX LOG [host] " + Messages.Log.read(s, off).text());
					case MSG_TERRAIN_PATCH -> out("@RX TERRAIN_PATCH " + describe(Messages.TerrainPatch.read(s, off)));
					default -> out("@RX type " + type + " (" + bytes + " bytes)");
				}
			});
			if (ep.connected()) {
				if (stress > 0) {
					for (int burst = 0; burst < 4096 && stressSent < stress; burst++) {
						if (!ep.send(new Messages.TestPattern(stressSent))) {
							break;
						}
						stressSent++;
					}
					if (!stressDone && stressSent >= stress && counts[1] >= stress) {
						stressDone = true;
						out("@STRESS_DONE sent=" + stressSent + " received=" + counts[1] + " errors=" + counts[2] + " ok=" + (counts[2] == 0 ? 1 : 0));
					}
				} else if (coop) {
					if (coopStartMs == 0) {
						coopStartMs = nowMs;
						for (int i = 0; i < COOP_CHUNKS.length; i++) {
							ep.send(new Messages.TerrainRequest(COOP_CHUNKS[i][0], COOP_CHUNKS[i][1], i + 1, 0));
						}
						ep.send(new Messages.SessionInfo(SESSION_OPEN | SESSION_AUTH, 25565, 1, 8, 1, "127.0.0.1:25565"));
						ep.send(new Messages.RemotePlayerJoin(COOP_FRIEND_ID, 0, 0x0001020304050607L, 0x08090A0B0C0D0E0FL, "HeadlessFriend"));
					}
					long age = nowMs - coopStartMs;
					if (age < COOP_FRIEND_LEAVE_MS && nowMs >= nextFriendState) {
						nextFriendState = nowMs + COOP_FRIEND_STATE_MS;
						double x = age / 1000.0 * 4.0; // sprinting east along z = 0
						ep.send(new Messages.RemotePlayerState(COOP_FRIEND_ID, REMOTE_ON_GROUND | REMOTE_SPRINTING, x, 70, 0, 4.0F, 0, 0, -90.0F, 0, -90.0F,
							(int) (age / 50), 1, 20));
					} else if (age >= COOP_FRIEND_LEAVE_MS && !coopLeft) {
						coopLeft = true;
						ep.send(new Messages.RemotePlayerLeave(COOP_FRIEND_ID, LEAVE_LEFT));
					}
				} else if (blocks && nowMs >= nextBlock) {
					nextBlock = nowMs + BLOCK_EVENT_PERIOD_MS;
					requestId++;
					ep.send(new Messages.BlockBreakRequest(requestId, requestId, -61, 0, 1, 0));
					ep.send(new Messages.BlockSet(requestId, -61, 0, BLOCK_AIR, 0, 0));
				}
			}
			if (nowMs >= nextStatus) {
				nextStatus = nowMs + statusMs;
				var tx = ep.txStats();
				var rx = ep.rxStats();
				var peer = ep.peer();
				out("@STATUS state=" + ep.state() + " session=" + Integer.toUnsignedString(ep.session()) + " peerSession=" + Integer.toUnsignedString(peer.session)
					+ " peerPid=" + peer.pid + " rttUs=" + (peer.rttValid ? peer.rttUs : 0) + " playerStates=" + counts[0] + " tx=" + tx.produced + " txDropped="
					+ tx.droppedFull + " rx=" + rx.consumed + " stale=" + rx.stale + " unknown=" + rx.unknown + " malformed=" + rx.malformed + " corrupt="
					+ rx.corrupt + " notConnected=" + ep.droppedNotConnected());
			}
			if (exitAfterMs > 0 && nowMs - start >= exitAfterMs) {
				break;
			}
			if (stress > 0 && ep.connected()) {
				Thread.onSpinWait();
			} else {
				Thread.sleep(LOOP_SLEEP_MS);
			}
		}
		ep.detach();
		out("@EVENT EXIT");
	}

	/** One line per patch: chunk, request, validity, how much ground and water, and a hash of the columns. */
	private static String describe(Messages.TerrainPatch p) {
		int ground = 0, water = 0, hash = 0x811C9DC5;
		for (int i = 0; i < CHUNK_COLUMNS; i++) {
			ground += p.groundY()[i] != NO_GROUND ? 1 : 0;
			water += p.waterY()[i] != NO_WATER && p.waterY()[i] > p.groundY()[i] ? 1 : 0;
			for (int v : new int[] { p.groundY()[i], p.waterY()[i], p.material()[i] }) {
				hash = (hash ^ (v & 0xFFFF)) * 0x01000193;
			}
		}
		return "chunk=(" + p.chunkX() + "," + p.chunkZ() + ") req=" + p.requestId() + " valid=" + (p.valid() ? 1 : 0) + " ground=" + ground + " water=" + water
			+ " y0=" + p.groundY()[0] + " hash=" + Integer.toHexString(hash);
	}

	private static void reportEvents(Endpoint ep, int ev) {
		String[] names = { "ATTACHED", "CONNECTED", "RESUMED", "STALE", "PEER_DETACHED", "PEER_RESTARTED", "PEER_HELLO", "VERSION_MISMATCH", "DETACHED", "CORRUPT" };
		for (int bit = 0; bit < names.length; bit++) {
			if ((ev & (1 << bit)) != 0) {
				out("@EVENT " + names[bit] + " session=" + Integer.toUnsignedString(ep.session()) + " peerSession=" + Integer.toUnsignedString(ep.peer().session));
			}
		}
	}

	private static synchronized void out(String line) {
		System.out.println(line);
		System.out.flush();
	}
}
