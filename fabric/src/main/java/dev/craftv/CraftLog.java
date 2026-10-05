package dev.craftv;

import java.io.IOException;
import java.io.Writer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * CraftV's log: timestamped lines in {@code <logDir>/craftv-fabric.log} (brief §5.2), mirrored to
 * Minecraft's own log. {@link #limited} rate-limits repetitive lines by key.
 */
public final class CraftLog {
	private static final Logger SLF4J = LoggerFactory.getLogger("craftv");
	private static final DateTimeFormatter TIME = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss.SSS");
	private static final String FILE_NAME = "craftv-fabric.log";
	private static final Map<String, Long> LAST_BY_KEY = new ConcurrentHashMap<>();
	private static final Map<String, Long> SUPPRESSED_BY_KEY = new ConcurrentHashMap<>();
	private static Writer file;

	private CraftLog() {
	}

	/** Opens the log file. {@code -Dcraftv.logDir} wins; otherwise {@code <game dir>/logs}. */
	public static synchronized void open(Path gameDir) {
		if (file != null) {
			return;
		}
		String configured = System.getProperty("craftv.logDir");
		Path dir = configured != null && !configured.isBlank() ? Path.of(configured) : gameDir.resolve("logs");
		try {
			Files.createDirectories(dir);
			file = Files.newBufferedWriter(dir.resolve(FILE_NAME), StandardCharsets.UTF_8, StandardOpenOption.CREATE, StandardOpenOption.APPEND);
			SLF4J.info("CraftV log: {}", dir.resolve(FILE_NAME).toAbsolutePath());
		} catch (IOException e) {
			SLF4J.warn("CraftV: can't open {} ({}); logging to latest.log only", dir.resolve(FILE_NAME), e.toString());
		}
	}

	public static void info(String text) {
		write("INFO", text);
		SLF4J.info(text);
	}

	public static void warn(String text) {
		write("WARN", text);
		SLF4J.warn(text);
	}

	public static void error(String text, Throwable t) {
		write("ERROR", t == null ? text : text + ": " + t);
		SLF4J.error(text, t);
	}

	public static void debug(String text) {
		write("DEBUG", text);
	}

	/** Logs at most once per {@code intervalMs} for this key, then reports how many were suppressed. */
	public static void limited(String key, long intervalMs, String text) {
		long now = System.currentTimeMillis();
		Long last = LAST_BY_KEY.get(key);
		if (last != null && now - last < intervalMs) {
			SUPPRESSED_BY_KEY.merge(key, 1L, Long::sum);
			return;
		}
		LAST_BY_KEY.put(key, now);
		Long suppressed = SUPPRESSED_BY_KEY.remove(key);
		info(suppressed == null ? text : text + " (+" + suppressed + " similar suppressed)");
	}

	/** Link-core levels (Proto.LOG_*) to our levels. */
	public static void link(int level, String text) {
		String line = "link: " + text;
		if (level >= 4) {
			error(line, null);
		} else if (level == 3) {
			warn(line);
		} else if (level == 2) {
			info(line);
		} else {
			debug(line);
		}
	}

	private static synchronized void write(String level, String text) {
		if (file == null) {
			return;
		}
		try {
			file.write(LocalDateTime.now().format(TIME) + " [" + Thread.currentThread().getName() + "/" + level + "] " + text + System.lineSeparator());
			file.flush();
		} catch (IOException ignored) {
			// keep running without the file
		}
	}
}
