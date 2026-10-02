package dev.redcraft.link;

import static org.junit.jupiter.api.Assertions.*;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.function.Predicate;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.Assumptions;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Tag;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.TestInfo;

/**
 * Brief §5.4 chaos tests and the cross-process stress test: the real C++ mock host
 * (tools/mockhost) against the real Java link code (HeadlessGuest) in separate processes. Each test
 * uses its own mapping name. Run with: gradlew integrationTest -Pmockhost=..\build\tools\mockhost\Release\mockhost.exe
 */
@Tag("integration")
class ChaosIntegrationTest {
	static final long HOST_TIMEOUT_MS = 2000; // guest's view of the host (short for tests)
	static final long MC_TIMEOUT_MS = 1500; // host's view of the guest
	static final long CONNECT_WAIT_MS = 10_000;
	static final String STATUS_MS = "200";
	static final Pattern PLAYER_STATES = Pattern.compile("playerStates=(\\d+)");

	final List<Proc> procs = new ArrayList<>();
	String mapping;
	String testName;

	@BeforeEach
	void setUp(TestInfo info) {
		String mock = System.getProperty("redcraft.mockhost", "");
		Assumptions.assumeTrue(!mock.isBlank() && Files.exists(Path.of(mock)), "set -Pmockhost=<path to mockhost.exe>");
		testName = info.getTestMethod().map(m -> m.getName()).orElse("test");
		mapping = "Local\\RedCraft_Chaos_" + testName + "_" + ProcessHandle.current().pid() + "_" + System.nanoTime();
	}

	@AfterEach
	void tearDown() {
		for (Proc p : procs) {
			p.kill();
		}
	}

	// ---- process helpers ------------------------------------------------------------------------

	static final class Proc {
		final String name;
		final Process process;
		final List<String> lines = new CopyOnWriteArrayList<>();
		final Writer stdin;

		Proc(String name, List<String> command) throws IOException {
			this.name = name;
			this.process = new ProcessBuilder(command).redirectErrorStream(true).start();
			this.stdin = new OutputStreamWriter(process.getOutputStream(), StandardCharsets.UTF_8);
			Thread reader = new Thread(() -> {
				try (BufferedReader r = new BufferedReader(new InputStreamReader(process.getInputStream(), StandardCharsets.UTF_8))) {
					String line;
					while ((line = r.readLine()) != null) {
						lines.add(line);
						System.out.println("[" + name + "] " + line);
					}
				} catch (IOException ignored) {
					// process ended
				}
			}, name + "-reader");
			reader.setDaemon(true);
			reader.start();
		}

		void command(String line) throws IOException {
			stdin.write(line + "\n");
			stdin.flush();
		}

		int mark() {
			return lines.size();
		}

		/** Waits until a line after {@code from} matches. */
		String await(int from, Predicate<String> match, long timeoutMs, String what) throws InterruptedException {
			long end = System.currentTimeMillis() + timeoutMs;
			while (System.currentTimeMillis() < end) {
				for (int i = from; i < lines.size(); i++) {
					if (match.test(lines.get(i))) {
						return lines.get(i);
					}
				}
				Thread.sleep(25);
			}
			fail(name + ": timed out after " + timeoutMs + " ms waiting for " + what);
			return null;
		}

		String awaitContains(int from, String text, long timeoutMs) throws InterruptedException {
			return await(from, l -> l.contains(text), timeoutMs, "'" + text + "'");
		}

		/** The newest playerStates=N on a @STATUS line, or -1. */
		long latestPlayerStates() {
			for (int i = lines.size() - 1; i >= 0; i--) {
				Matcher m = PLAYER_STATES.matcher(lines.get(i));
				if (lines.get(i).startsWith("@STATUS") && m.find()) {
					return Long.parseLong(m.group(1));
				}
			}
			return -1;
		}

		void kill() {
			process.destroyForcibly();
			try {
				process.waitFor();
			} catch (InterruptedException e) {
				Thread.currentThread().interrupt();
			}
		}
	}

	Proc startHost(String... extra) throws IOException {
		List<String> cmd = new ArrayList<>(List.of(System.getProperty("redcraft.mockhost"), "--mapping", mapping, "--mc-timeout-ms",
			Long.toString(MC_TIMEOUT_MS), "--status-ms", STATUS_MS, "--quiet"));
		cmd.addAll(List.of(extra));
		Proc p = new Proc("host", cmd);
		procs.add(p);
		return p;
	}

	Proc startGuest(String... extra) throws IOException {
		String java = Path.of(System.getProperty("java.home"), "bin", "java.exe").toString();
		List<String> cmd = new ArrayList<>(List.of(java, "--enable-native-access=ALL-UNNAMED", "-cp", System.getProperty("redcraft.guestClasspath"),
			"dev.redcraft.link.tools.HeadlessGuest", "--mapping", mapping, "--host-timeout-ms", Long.toString(HOST_TIMEOUT_MS), "--status-ms", STATUS_MS));
		cmd.addAll(List.of(extra));
		Proc p = new Proc("guest", cmd);
		procs.add(p);
		return p;
	}

	/** Player states keep arriving at the guest. */
	void assertPlayerStatesFlow(Proc guest) throws InterruptedException {
		long before = guest.latestPlayerStates();
		Thread.sleep(800);
		long after = guest.latestPlayerStates();
		assertTrue(after > before && after > 0, "PLAYER_STATE should flow to the guest (" + before + " -> " + after + ")");
	}

	// ---- chaos cases ----------------------------------------------------------------------------

	@Test
	void hostStartsFirst() throws Exception {
		Proc host = startHost();
		host.awaitContains(0, "@EVENT ATTACHED", CONNECT_WAIT_MS);
		Thread.sleep(500);
		Proc guest = startGuest();
		host.awaitContains(0, "@EVENT CONNECTED", CONNECT_WAIT_MS);
		guest.awaitContains(0, "@EVENT CONNECTED", CONNECT_WAIT_MS);
		guest.awaitContains(0, "HELLO from host", CONNECT_WAIT_MS);
		assertPlayerStatesFlow(guest);
	}

	@Test
	void minecraftStartsFirst() throws Exception {
		Proc guest = startGuest();
		guest.awaitContains(0, "shared memory created", CONNECT_WAIT_MS);
		Thread.sleep(500);
		Proc host = startHost();
		host.awaitContains(0, "shared memory opened", CONNECT_WAIT_MS);
		host.awaitContains(0, "@EVENT CONNECTED", CONNECT_WAIT_MS);
		guest.awaitContains(0, "@EVENT CONNECTED", CONNECT_WAIT_MS);
		assertPlayerStatesFlow(guest);
	}

	@Test
	void minecraftKilledMidRunThenRestarted() throws Exception {
		Proc host = startHost();
		Proc guest = startGuest();
		host.awaitContains(0, "@EVENT CONNECTED", CONNECT_WAIT_MS);
		guest.awaitContains(0, "@EVENT CONNECTED", CONNECT_WAIT_MS);
		int mark = host.mark();
		guest.kill(); // no clean detach
		host.awaitContains(mark, "@EVENT STALE", MC_TIMEOUT_MS + 3000);
		mark = host.mark();
		Proc guest2 = startGuest();
		host.awaitContains(mark, "@EVENT PEER_RESTARTED", CONNECT_WAIT_MS);
		host.awaitContains(mark, "@EVENT CONNECTED", CONNECT_WAIT_MS);
		guest2.awaitContains(0, "@EVENT CONNECTED", CONNECT_WAIT_MS);
		assertPlayerStatesFlow(guest2);
	}

	@Test
	void hostKilledMidRunThenRestarted() throws Exception {
		Proc host = startHost();
		Proc guest = startGuest();
		guest.awaitContains(0, "@EVENT CONNECTED", CONNECT_WAIT_MS);
		int mark = guest.mark();
		host.kill(); // crash: ATTACHED bit stays set, heartbeat freezes
		guest.awaitContains(mark, "@EVENT STALE", HOST_TIMEOUT_MS + 3000);
		mark = guest.mark();
		Proc host2 = startHost();
		host2.awaitContains(0, "@EVENT CONNECTED", CONNECT_WAIT_MS);
		guest.awaitContains(mark, "@EVENT PEER_RESTARTED", CONNECT_WAIT_MS);
		guest.awaitContains(mark, "@EVENT CONNECTED", CONNECT_WAIT_MS);
		assertPlayerStatesFlow(guest);
	}

	@Test
	void killLinkGoesStaleAndResumeLinkResumesSameSession() throws Exception {
		Proc host = startHost();
		Proc guest = startGuest();
		guest.awaitContains(0, "@EVENT CONNECTED", CONNECT_WAIT_MS);
		int mark = guest.mark();
		host.command("kill-link");
		guest.awaitContains(mark, "@EVENT STALE", HOST_TIMEOUT_MS + 3000);
		mark = guest.mark();
		host.command("resume-link");
		guest.awaitContains(mark, "@EVENT RESUMED", CONNECT_WAIT_MS);
		for (int i = mark; i < guest.lines.size(); i++) {
			assertFalse(guest.lines.get(i).contains("PEER_RESTARTED"), "resume must not look like a restart");
		}
		assertPlayerStatesFlow(guest);
	}

	@Test
	void restartCommandIsANewSession() throws Exception {
		Proc host = startHost();
		Proc guest = startGuest();
		guest.awaitContains(0, "@EVENT CONNECTED", CONNECT_WAIT_MS);
		int mark = guest.mark();
		host.command("restart");
		guest.awaitContains(mark, "@EVENT PEER_RESTARTED", CONNECT_WAIT_MS);
		guest.awaitContains(mark, "@EVENT CONNECTED", CONNECT_WAIT_MS);
		assertPlayerStatesFlow(guest);
	}

	@Test
	void bothKilledThenBothRestarted() throws Exception {
		Proc host = startHost();
		Proc guest = startGuest();
		guest.awaitContains(0, "@EVENT CONNECTED", CONNECT_WAIT_MS);
		host.kill();
		guest.kill();
		Proc guest2 = startGuest();
		Proc host2 = startHost();
		host2.awaitContains(0, "@EVENT CONNECTED", CONNECT_WAIT_MS);
		guest2.awaitContains(0, "@EVENT CONNECTED", CONNECT_WAIT_MS);
		assertPlayerStatesFlow(guest2);
	}

	@Test
	void blockMessagesFromMinecraftReachTheHost() throws Exception {
		Proc host = startHost();
		startGuest("--blocks");
		host.awaitContains(0, "@RX BLOCK_BREAK_REQUEST", CONNECT_WAIT_MS);
		host.awaitContains(0, "@RX BLOCK_SET", CONNECT_WAIT_MS);
	}

	// ---- cross-process stress -------------------------------------------------------------------

	@Test
	void oneMillionRecordsEachWayAcrossProcessesAndLanguages() throws Exception {
		long n = 1_000_000;
		Proc host = startHost("--stress", Long.toString(n));
		Proc guest = startGuest("--stress", Long.toString(n));
		String h = host.awaitContains(0, "@STRESS_DONE", 120_000);
		String g = guest.awaitContains(0, "@STRESS_DONE", 120_000);
		assertTrue(h.contains("received=" + n) && h.contains("errors=0") && h.contains("ok=1"), h);
		assertTrue(g.contains("received=" + n) && g.contains("errors=0") && g.contains("ok=1"), g);
	}
}
