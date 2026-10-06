package dev.craftv.client.passthrough;

/**
 * Whether the hidden Minecraft is rendering the owner's view for the host to composite (brief §8, PROTOCOL.md
 * §7.15 PASSTHROUGH). {@link #hideTerrain} changes only between client ticks, together with a re-mesh of every
 * chunk, so all chunk meshing threads see one value per mesh.
 */
public final class Passthrough {
	private static volatile boolean hideTerrain;

	private Passthrough() {
	}

	/** True while the owner's view leaves CraftV's terrain to the host's own ground. Any thread. */
	public static boolean hideTerrain() {
		return hideTerrain;
	}

	static void setHideTerrain(boolean hide) {
		hideTerrain = hide;
	}
}
