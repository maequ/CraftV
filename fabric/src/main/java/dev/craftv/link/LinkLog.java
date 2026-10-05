package dev.craftv.link;

/** Where the link core writes its log lines. Kept free of Minecraft classes so the core runs headless. */
@FunctionalInterface
public interface LinkLog {
	LinkLog NONE = (level, text) -> {
	};

	/** {@code level} is one of Proto.LOG_*. */
	void log(int level, String text);
}
