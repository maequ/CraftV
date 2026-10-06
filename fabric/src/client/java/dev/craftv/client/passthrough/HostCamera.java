package dev.craftv.client.passthrough;

import dev.craftv.LinkService;
import dev.craftv.link.Messages;

/** The host's camera (PROTOCOL.md §7.15) for the frame being rendered, taken once per frame so every hook agrees. */
public final class HostCamera {
	private static final long TIMEOUT_NANOS = 2_000_000_000L;
	private static Messages.Camera frame; // render thread only

	private HostCamera() {
	}

	/** The latest camera if the host is still sending it with the passthrough on, else null. */
	public static Messages.Camera live() {
		LinkService.ReceivedCamera r = LinkService.get().latestCamera();
		return r != null && r.camera().passthrough() && System.nanoTime() - r.receivedNanos() < TIMEOUT_NANOS ? r.camera() : null;
	}

	/** Camera.update, before any hook reads the pose. */
	public static void beginFrame() {
		frame = live();
	}

	/** The pose this frame renders with, or null when the host isn't compositing. Render thread. */
	public static Messages.Camera frame() {
		return frame;
	}
}
