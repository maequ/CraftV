package dev.redcraft.link.tools;

import static dev.redcraft.link.Proto.*;

import dev.redcraft.link.Endpoint;
import dev.redcraft.link.Messages;
import dev.redcraft.link.Win32;

/**
 * A Minecraft-less guest that runs the real Java link code (role MC). Used by the chaos and
 * cross-process stress tests against the C++ mock host, and handy for poking the link by hand:
 *
 * <pre>
 * java --enable-native-access=ALL-UNNAMED -cp fabric/build/classes/java/main dev.redcraft.link.tools.HeadlessGuest [options]
 *   --mapping NAME  --host-timeout-ms N  --stress N  --exit-after-ms N  --status-ms N  --blocks
 * </pre>
 *
 * Prints the same '@' lines as the mock host (@EVENT, @STATUS, @STRESS_DONE, @RX).
 */
public final class HeadlessGuest {
	private static final long LOOP_SLEEP_MS = 1;
	private static final long BLOCK_EVENT_PERIOD_MS = 1000;

	private HeadlessGuest() {
	}

	public static void main(String[] args) throws InterruptedException {
		Endpoint.Config config = new Endpoint.Config();
		config.role = ROLE_MC;
		config.software = "RedCraft-HeadlessGuest 0.1.0";
		long stress = 0, exitAfterMs = 0, statusMs = 1000;
		boolean blocks = false;
		for (int i = 0; i < args.length; i++) {
			switch (args[i]) {
				case "--mapping" -> config.mappingName = args[++i];
				case "--host-timeout-ms" -> config.peerTimeoutMs = Long.parseLong(args[++i]);
				case "--stress" -> stress = Long.parseLong(args[++i]);
				case "--exit-after-ms" -> exitAfterMs = Long.parseLong(args[++i]);
				case "--status-ms" -> statusMs = Long.parseLong(args[++i]);
				case "--blocks" -> blocks = true;
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
		out("RedCraft headless guest, pid " + Win32.currentPid());
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
